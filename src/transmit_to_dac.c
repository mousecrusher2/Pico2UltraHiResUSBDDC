/*
* Copyright (c) 2025 ArqAlice 
*
* Released under the MIT license
* https://opensource.org/licenses/mit-license.php
*/

#include "transmit_to_dac.h"
#include <stdatomic.h>
#include <stdint.h>
#include <string.h>
#include <hardware/dma.h>
#include "i2s_pio_interface.h"
#include "upsampling.h"

static pio_hw_t *const pio = pio0;
static constexpr uint sm = 0;
static pio_sm_config sm_config;
static uint offset;

static constexpr uint8_t DMA_TX_CHAIN_CHANNELS = 2;

typedef enum {
    DMA_CH_STATE_IDLE = 0,
    DMA_CH_STATE_ARMED,
    DMA_CH_STATE_RUNNING
} dma_ch_state_t;

static int dma_ch[DMA_TX_CHAIN_CHANNELS];
static dma_channel_config c_dma[DMA_TX_CHAIN_CHANNELS];
static uint8_t dma_ch_state[DMA_TX_CHAIN_CHANNELS];
static DMA_TX_STRUCTURE dma_tx;
// Core1 owns the DMA/PIO state. Core0 communicates with it only through these atomics.
static _Atomic bool dma_stop_requested;
static _Atomic bool dma_stop_completed;
static _Atomic bool i2s_reset_requested;
static _Atomic bool i2s_reset_completed;

_Atomic bool enable_output = false; // NOLINT(misc-use-internal-linkage): read by main.c.

void __not_in_flash_func(dma_tx_start)(void);
static inline uint32_t dma_tx_active_count(void);
static inline bool dma_tx_any_running(void);
static void __not_in_flash_func(dma_tx_chain_service)(void);
static void reset_i2s_freq_core1(void);
static void dma_stop_and_clear_core1(void);
static bool __not_in_flash_func(service_control_requests)(void);

static inline uint32_t dma_tx_active_count(void) {
    uint32_t count = 0;
    for (int i = 0; i < DMA_TX_CHAIN_CHANNELS; i++) {
        if (dma_ch_state[i] != DMA_CH_STATE_IDLE) {
            count++;
        }
    }
    return count;
}

static inline bool dma_tx_any_running(void) {
    for (int i = 0; i < DMA_TX_CHAIN_CHANNELS; i++) {
        if (dma_ch_state[i] == DMA_CH_STATE_RUNNING) {
            return true;
        }
    }
    return false;
}

static void __not_in_flash_func(dma_tx_chain_service)(void) {
    for (int i = 0; i < DMA_TX_CHAIN_CHANNELS; i++) {
        const bool busy = dma_channel_is_busy(dma_ch[i]);
        if (dma_ch_state[i] == DMA_CH_STATE_ARMED) {
            if (busy) {
                dma_ch_state[i] = DMA_CH_STATE_RUNNING;
            }
        } else if (dma_ch_state[i] == DMA_CH_STATE_RUNNING) {
            if (!busy) {
                dma_ch_state[i] = DMA_CH_STATE_IDLE;
                if (dma_tx.using > 0) {
                    dma_tx.using --;
                }
            }
        }
    }

    uint32_t active = dma_tx_active_count();
    int32_t unassigned = (int32_t)dma_tx.using - (int32_t)active;
    if (unassigned < 0) {
        unassigned = 0;
    }

    while (unassigned > 0) {
        int idle_idx = -1;
        for (int i = 0; i < DMA_TX_CHAIN_CHANNELS; i++) {
            if (dma_ch_state[i] == DMA_CH_STATE_IDLE) {
                idle_idx = i;
                break;
            }
        }
        if (idle_idx < 0) {
            break;
        }

        const uint32_t rp = dma_tx.rp;
        dma_channel_set_read_addr(dma_ch[idle_idx], dma_tx.data[rp].tx_buf, false);
        dma_channel_set_trans_count(dma_ch[idle_idx], dma_tx.data[rp].tx_size, false);
        dma_tx.rp++;
        if (dma_tx.rp >= DEPTH_DMA_TX_BUFFER) {
            dma_tx.rp = 0;
        }
        dma_ch_state[idle_idx] = DMA_CH_STATE_ARMED;
        active++;
        unassigned--;
    }

    if (!dma_tx_any_running()) {
        int armed_count = 0;
        int start_idx = -1;
        for (int i = 0; i < DMA_TX_CHAIN_CHANNELS; i++) {
            if (dma_ch_state[i] == DMA_CH_STATE_ARMED) {
                armed_count++;
                if (start_idx < 0) {
                    start_idx = i;
                }
            }
        }
        if (armed_count >= 2 && start_idx >= 0) {
            dma_channel_start(dma_ch[start_idx]);
            dma_ch_state[start_idx] = DMA_CH_STATE_RUNNING;
        }
    }
}

static void reset_i2s_freq_core1(void) {
    I2S_freq_init(pio, sm, &sm_config, offset);
}

void reset_i2s_freq(void) {
    if (get_core_num() == 1) {
        reset_i2s_freq_core1();
        return;
    }

    atomic_store_explicit(&i2s_reset_completed, false, memory_order_relaxed);
    atomic_store_explicit(&i2s_reset_requested, true, memory_order_release);
    while (!atomic_load_explicit(&i2s_reset_completed, memory_order_acquire)) {
        tight_loop_contents();
    }
}

void init_i2s_interface(void) {
    if (I2S_SLEWRATE_FAST_ENABLE) {
        gpio_set_slew_rate(I2S_DATA_PIN, GPIO_SLEW_RATE_FAST);
        gpio_set_slew_rate(I2S_SIDESET_BASE, GPIO_SLEW_RATE_FAST);
        gpio_set_slew_rate(I2S_SIDESET_BASE + 1, GPIO_SLEW_RATE_FAST);
    }

    if (I2S_STRENGTH_REINFORCE_ENABLE) {
        gpio_set_drive_strength(I2S_DATA_PIN, GPIO_DRIVE_STRENGTH_12MA);
        gpio_set_drive_strength(I2S_SIDESET_BASE, GPIO_DRIVE_STRENGTH_12MA);
        gpio_set_drive_strength(I2S_SIDESET_BASE + 1, GPIO_DRIVE_STRENGTH_12MA);
    }

    // PIO I2Sの初期化
    if (I2S_SIDESET_CHANGE) {
        I2S_32bit_inv_program_init(
            pio,
            sm,
            I2S_DATA_PIN,
            I2S_SIDESET_BASE,
            AUDIO_INITIAL_FREQ,
            &sm_config,
            &offset
        );
    } else {
        I2S_32bit_program_init(
            pio,
            sm,
            I2S_DATA_PIN,
            I2S_SIDESET_BASE,
            AUDIO_INITIAL_FREQ,
            &sm_config,
            &offset
        );
    }

    reset_i2s_freq_core1();

    // DMA setup (chained, no IRQ)
    dma_ch[0] = dma_claim_unused_channel(true);
    dma_ch[1] = dma_claim_unused_channel(true);

    c_dma[0] = dma_channel_get_default_config(dma_ch[0]);
    channel_config_set_transfer_data_size(&c_dma[0], DMA_SIZE_32);
    channel_config_set_dreq(&c_dma[0], pio_get_dreq(pio, sm, true));
    channel_config_set_chain_to(&c_dma[0], dma_ch[1]);

    c_dma[1] = dma_channel_get_default_config(dma_ch[1]);
    channel_config_set_transfer_data_size(&c_dma[1], DMA_SIZE_32);
    channel_config_set_dreq(&c_dma[1], pio_get_dreq(pio, sm, true));
    channel_config_set_chain_to(&c_dma[1], dma_ch[0]);

    memset(&dma_tx, 0, sizeof(DMA_TX_STRUCTURE));
    memset(dma_ch_state, 0, sizeof(dma_ch_state));

    dma_channel_set_irq0_enabled(dma_ch[0], false);
    dma_channel_set_irq0_enabled(dma_ch[1], false);

    dma_channel_configure(dma_ch[0], &c_dma[0], &pio->txf[sm], dma_tx.data[0].tx_buf, 0, false);
    dma_channel_configure(dma_ch[1], &c_dma[1], &pio->txf[sm], dma_tx.data[0].tx_buf, 0, false);
}

// アップサンプリングに使用するメモリを静的確保
static float from_core0_Lch[SIZE_DMA_TX_BUF / CORE1_UP_RATIO_MAX / 2];
static float from_core0_Rch[SIZE_DMA_TX_BUF / CORE1_UP_RATIO_MAX / 2];
static float upsr_core1_Lch[SIZE_DMA_TX_BUF / 2];
static float upsr_core1_Rch[SIZE_DMA_TX_BUF / 2];

// 転送用バッファを静的確保

void __not_in_flash_func(dma_tx_start)(void) {
    if (service_control_requests()) {
        return;
    }

    const int32_t length = (int32_t)get_size_using(&buffer_upsr_data_Lch_0);
    bool output_enabled = atomic_load_explicit(&enable_output, memory_order_relaxed);

    // バッファに規定量以上のデータが溜まってから出力開始
    if (!output_enabled && length > SIZE_BUFFER_FB_THRESHOLD) {
        atomic_store_explicit(&time_start_output, get_absolute_time(), memory_order_relaxed);
        // Resync I2S frame phase on playback start to prevent L/R swap.
        pio_sm_set_enabled(pio, sm, false);
        pio_sm_clear_fifos(pio, sm);
        pio_sm_restart(pio, sm);
        reset_i2s_freq_core1();
        atomic_store_explicit(&enable_output, true, memory_order_release);
        output_enabled = true;
    }

    // 入ってくるデータが枯渇したうえで、DMA送信バッファ内のすべてのデータを送信完了したら出力停止
    else if (output_enabled && length <= 0 && dma_tx.using == 0) {
        atomic_store_explicit(&enable_output, false, memory_order_release);
        output_enabled = false;
    }

    if (output_enabled) {
        // 入ってくるデータが枯渇した、またはDMA送信バッファに規定量以上データが蓄積されているときはデータ送信処理をしない
        if ((length > 0) && (dma_tx.using < SIZE_DMA_TX_BUF_STACK)) {
            const uint32_t freq = atomic_load_explicit(&audio_state.freq, memory_order_relaxed);
            const bool high_power = atomic_load_explicit(&is_high_power_mode, memory_order_relaxed);
            const uint32_t core1_block_len = upsampling_core1_get_block_len();
            constexpr int32_t max_in_len =
                (int32_t)(sizeof(from_core0_Lch) / sizeof(from_core0_Lch[0]));
            int32_t transmit_ref_size =
                (int32_t)((float)(freq
                              * get_ratio_upsampling_core0_for_power_mode(freq, high_power))
                    / 1000.0f * (CORE1_PROCESS_US / 1000.0f));
            if (transmit_ref_size > max_in_len) {
                transmit_ref_size = max_in_len;
            }
            if (core1_block_len > 0 && transmit_ref_size < (int32_t)core1_block_len) {
                transmit_ref_size = (int32_t)core1_block_len;
            }

            while (dma_tx.using < SIZE_DMA_TX_BUF_STACK) {
                const int32_t avail_len_L = (int32_t)get_size_using(&buffer_upsr_data_Lch_0);
                const int32_t avail_len_R = (int32_t)get_size_using(&buffer_upsr_data_Rch_0);
                const int32_t avail_len = (avail_len_L < avail_len_R) ? avail_len_L : avail_len_R;
                if (avail_len <= 0) {
                    break;
                }

                int32_t chunk_len = saturation_i32(avail_len, transmit_ref_size, 0);
                if (core1_block_len > 0) {
                    chunk_len = (chunk_len / (int32_t)core1_block_len) * (int32_t)core1_block_len;
                    if (chunk_len <= 0) {
                        break;
                    }
                }

                if (ringbuf_read_array_spinlock(
                        (int32_t *)from_core0_Lch,
                        chunk_len,
                        &buffer_upsr_data_Lch_0
                    )
                    < 0) {
                    break;
                }
                if (ringbuf_read_array_spinlock(
                        (int32_t *)from_core0_Rch,
                        chunk_len,
                        &buffer_upsr_data_Rch_0
                    )
                    < 0) {
                    break;
                }

                // Core1?????????????????????????????????250us????????????????)
                const int32_t out_len = (int32_t)upsampling_process_core1(
                    from_core0_Lch,
                    from_core0_Rch,
                    upsr_core1_Lch,
                    upsr_core1_Rch,
                    (uint32_t)chunk_len
                );
                if (out_len <= 0) {
                    break;
                }

                const uint32_t wp = dma_tx.wp;
                int32_t *const tx_buf = (int32_t *)dma_tx.data[wp].tx_buf;
                int count = 0;
                for (int32_t i = 0; i < out_len; i++) {
                    tx_buf[count++] = (int32_t)upsr_core1_Lch[i];
                    tx_buf[count++] = (int32_t)upsr_core1_Rch[i];
                }

                const uint32_t save = save_and_disable_interrupts();
                dma_tx.data[wp].tx_size = count;
                dma_tx.wp++;
                if (dma_tx.wp >= DEPTH_DMA_TX_BUFFER) {
                    dma_tx.wp = 0;
                }
                dma_tx.using ++;
                restore_interrupts(save);
            }

            // ???????????DMA?????????2????????DMA?????????????????
        }
        dma_tx_chain_service();
    } else {
        dma_stop_and_clear_core1();
    }
}

static void dma_stop_and_clear_core1(void) {
    // DMAを強制停止
    for (int i = 0; i < DMA_TX_CHAIN_CHANNELS; i++) {
        dma_channel_abort(dma_ch[i]);
    }

    // DMAステータスをリセット
    dma_hw->ints0 = (UINT32_C(1) << (uint32_t)dma_ch[0]) | (UINT32_C(1) << (uint32_t)dma_ch[1]);

    // FIFOバッファをクリア
    pio_sm_clear_fifos(pio0, 0);

    // 使用バッファをゼロクリア
    memset(&dma_tx, 0, sizeof(DMA_TX_STRUCTURE));
    memset(dma_ch_state, 0, sizeof(dma_ch_state));
}

void dma_stop_and_clear(void) {
    if (get_core_num() == 1) {
        dma_stop_and_clear_core1();
        return;
    }

    atomic_store_explicit(&dma_stop_completed, false, memory_order_relaxed);
    atomic_store_explicit(&dma_stop_requested, true, memory_order_release);
    while (!atomic_load_explicit(&dma_stop_completed, memory_order_acquire)) {
        tight_loop_contents();
    }
}

static bool __not_in_flash_func(service_control_requests)(void) {
    bool serviced = false;
    if (atomic_exchange_explicit(&dma_stop_requested, false, memory_order_acq_rel)) {
        dma_stop_and_clear_core1();
        atomic_store_explicit(&dma_stop_completed, true, memory_order_release);
        serviced = true;
    }
    if (atomic_exchange_explicit(&i2s_reset_requested, false, memory_order_acq_rel)) {
        reset_i2s_freq_core1();
        atomic_store_explicit(&i2s_reset_completed, true, memory_order_release);
        serviced = true;
    }
    return serviced;
}
