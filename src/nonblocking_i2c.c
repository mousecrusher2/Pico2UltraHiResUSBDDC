/*
 * Copyright (c) 2025 ArqAlice
 *
 * Released under the MIT license
 * https://opensource.org/licenses/mit-license.php
 */

#include "nonblocking_i2c.h"
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <hardware/dma.h>
#include <hardware/i2c.h>
#include <hardware/sync.h>
#include <pico/stdlib.h>

static _Atomic bool is_transferring_data;
static _Atomic(i2c_hw_t *) i2c_hw;
static int dma_ch;
static dma_channel_config c_dma;
static uint32_t i2c_tx_cmd_data[16];

static void i2c_irq_handler(void);

void i2c_dma_initialize(i2c_inst_t *const i2c) {
    dma_ch = dma_claim_unused_channel(true);

    c_dma = dma_channel_get_default_config(dma_ch);
    channel_config_set_transfer_data_size(&c_dma, DMA_SIZE_32);
    channel_config_set_read_increment(&c_dma, true);
    channel_config_set_write_increment(&c_dma, false);
    channel_config_set_dreq(&c_dma, i2c_get_dreq(i2c, true));
}

bool i2c_dma_is_busy(void) {
    return atomic_load_explicit(&is_transferring_data, memory_order_acquire);
}

void i2c_write_dma(
    i2c_inst_t *const i2c,
    uint8_t addr_7bit,
    const uint8_t *const data,
    size_t len,
    bool nostop
) {
    if (len == 0 || data == nullptr) {
        return;
    }

    // dma動作中は終わるまで待機
    while (dma_channel_is_busy(dma_ch)) {
        tight_loop_contents();
    }

    i2c_hw_t *selected_hw;

    // I2Cのアドレスを格納し、割り込み処理をセット
    if (i2c == i2c0) {
        atomic_store_explicit(&is_transferring_data, true, memory_order_release);
        selected_hw = i2c0_hw;
        atomic_store_explicit(&i2c_hw, selected_hw, memory_order_release);
        selected_hw->intr_mask |= I2C_IC_INTR_MASK_M_STOP_DET_BITS; // STOP_DET割り込みを有効化
        selected_hw->intr_mask |= I2C_IC_INTR_MASK_M_TX_EMPTY_BITS; // TX_EMPTY割り込みを有効化
        selected_hw->intr_mask |= I2C_IC_INTR_MASK_M_TX_ABRT_BITS; // TX_ABRT割り込みを有効化
        irq_set_exclusive_handler(I2C0_IRQ, i2c_irq_handler);
        irq_set_enabled(I2C0_IRQ, true);
        irq_set_priority(I2C0_IRQ, 0);
    } else if (i2c == i2c1) {
        atomic_store_explicit(&is_transferring_data, true, memory_order_release);
        selected_hw = i2c1_hw;
        atomic_store_explicit(&i2c_hw, selected_hw, memory_order_release);
        selected_hw->intr_mask |= I2C_IC_INTR_MASK_M_STOP_DET_BITS; // STOP_DET割り込みを有効化
        selected_hw->intr_mask |= I2C_IC_INTR_MASK_M_TX_EMPTY_BITS; // TX_EMPTY割り込みを有効化
        selected_hw->intr_mask |= I2C_IC_INTR_MASK_M_TX_ABRT_BITS; // TX_ABRT割り込みを有効化
        irq_set_exclusive_handler(I2C1_IRQ, i2c_irq_handler);
        irq_set_enabled(I2C1_IRQ, true);
        irq_set_priority(I2C1_IRQ, 0);
    } else {
        return; // 不正なI2Cを指定したらreturnする
    }

    // コマンドデータを作成
    for (size_t byte_ctr = 0; byte_ctr < len; byte_ctr++) {
        const bool first = (byte_ctr == 0);
        const bool last = (byte_ctr == len - 1);

        i2c_tx_cmd_data[byte_ctr] = bool_to_bit(first && i2c->restart_on_next)
                << I2C_IC_DATA_CMD_RESTART_LSB
            | bool_to_bit(last && !nostop) << I2C_IC_DATA_CMD_STOP_LSB | data[byte_ctr];
    }

    // I2C無効化 → アドレス設定 → 有効化
    selected_hw->enable = 0;
    selected_hw->tar = addr_7bit; // 7bitアドレス（左にシフトしない）
    selected_hw->enable = 1;

    // DMAで先頭〜(n-2)バイト送信
    dma_channel_configure(
        dma_ch,
        &c_dma,
        &selected_hw->data_cmd, // 書き込み先
        (void *)i2c_tx_cmd_data, // 読み込み元
        len,
        true // 即開始
    );

    // nostop means we are now at the end of a *message* but not the end of a *transfer*
    i2c->restart_on_next = nostop;
}

static void i2c_irq_handler(void) {
    i2c_hw_t *const selected_hw = atomic_load_explicit(&i2c_hw, memory_order_acquire);
    selected_hw->clr_stop_det; // STOP割り込みフラグをクリア
    selected_hw->clr_tx_abrt; // Abort割り込みフラグをクリア

    // I2Cの割り込み処理の割付を解除
    selected_hw->intr_mask &= ~I2C_IC_INTR_MASK_M_STOP_DET_BITS; // STOP_DET割り込みを無効化
    selected_hw->intr_mask &= ~I2C_IC_INTR_MASK_M_TX_EMPTY_BITS; // TX_EMPTY割り込みを無効化
    selected_hw->intr_mask &= ~I2C_IC_INTR_MASK_M_TX_ABRT_BITS; // TX_ABRT割り込みを無効化

    if (selected_hw == i2c0_hw) {
        irq_remove_handler(I2C0_IRQ, i2c_irq_handler);
        irq_set_enabled(I2C0_IRQ, false);
    } else if (selected_hw == i2c1_hw) {
        irq_remove_handler(I2C1_IRQ, i2c_irq_handler);
        irq_set_enabled(I2C1_IRQ, false);
    }
    atomic_store_explicit(&is_transferring_data, false, memory_order_release);
}

// ---------------------- ring buffer ----------------------
int16_t initialize_i2c_ringbuffer(uint16_t size, I2C_RINGBUFFER *const ringbuffer) {
    ringbuffer->size_buffer = size;
    atomic_init(&ringbuffer->write_point, 0);
    atomic_init(&ringbuffer->read_point, 0);
    atomic_init(&ringbuffer->size_using, 0);
    ringbuffer->buffer = (I2C_RB_DATA *)malloc(sizeof(I2C_RB_DATA) * size);
    return 0;
}

int64_t i2c_ringbuf_get_size_using(const I2C_RINGBUFFER *const ringbuffer) {
    return atomic_load_explicit(&ringbuffer->size_using, memory_order_acquire);
}

int16_t i2c_ringbuf_write(const I2C_RB_DATA *const input, I2C_RINGBUFFER *const ringbuffer) {
    const uint32_t interrupt_state = save_and_disable_interrupts();
    const uint16_t size_using =
        (uint16_t)atomic_load_explicit(&ringbuffer->size_using, memory_order_relaxed);
    if (size_using >= ringbuffer->size_buffer) {
        restore_interrupts(interrupt_state);
        return -1; // buffer is full
    }

    uint16_t write_point = atomic_load_explicit(&ringbuffer->write_point, memory_order_relaxed);
    memcpy(ringbuffer->buffer + write_point, input, sizeof(I2C_RB_DATA));
    write_point++;
    if (write_point >= ringbuffer->size_buffer) {
        write_point = 0;
    }
    atomic_store_explicit(&ringbuffer->write_point, write_point, memory_order_relaxed);
    atomic_store_explicit(&ringbuffer->size_using, (int16_t)(size_using + 1), memory_order_release);
    restore_interrupts(interrupt_state);
    return 1;
}

int16_t i2c_ringbuf_read(I2C_RB_DATA *const output, I2C_RINGBUFFER *const ringbuffer) {
    const uint32_t interrupt_state = save_and_disable_interrupts();
    const int16_t size_using = atomic_load_explicit(&ringbuffer->size_using, memory_order_relaxed);
    if (size_using == 0) {
        restore_interrupts(interrupt_state);
        return -1; // buffer is empty
    }

    uint16_t read_point = atomic_load_explicit(&ringbuffer->read_point, memory_order_relaxed);
    memcpy(output, ringbuffer->buffer + read_point, sizeof(I2C_RB_DATA));
    read_point++;
    if (read_point >= ringbuffer->size_buffer) {
        read_point = 0;
    }
    atomic_store_explicit(&ringbuffer->read_point, read_point, memory_order_relaxed);
    atomic_store_explicit(&ringbuffer->size_using, size_using - 1, memory_order_release);
    restore_interrupts(interrupt_state);
    return 1;
}

void i2c_ringbuf_set_data(
    i2c_inst_t *const i2c,
    uint8_t addr_7bit,
    const uint8_t *const data,
    size_t len,
    bool nostop,
    I2C_RB_DATA *const output
) {
    output->i2c = i2c;
    output->addr_7bit = addr_7bit;
    memcpy(output->data, data, len * sizeof(uint8_t));
    output->len = (uint8_t)len;
    output->nostop = nostop;
}

void i2c_dma_stop_and_clear(void) {
    // DMAを強制停止
    dma_channel_abort(dma_ch);

    // DMAステータスをリセット
    dma_hw->ints1 = UINT32_C(1) << (uint32_t)dma_ch;

    atomic_store_explicit(&is_transferring_data, false, memory_order_release);
}
