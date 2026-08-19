/*
 * Copyright (c) 2025 ArqAlice
 *
 * Released under the MIT license
 * https://opensource.org/licenses/mit-license.php
 */

#include <math.h>
#include <stdatomic.h>
#include <stdint.h>
#include <hardware/clocks.h>
#include <hardware/gpio.h>
#include <hardware/i2c.h>
#include <hardware/vreg.h>
#include "common.h"
#include "nonblocking_i2c.h"
#include "transmit_to_dac.h"

static void renew_cpu_clock(bool is_high_power) {
    // CPUクロックを再設定
    const uint32_t freq = atomic_load_explicit(&audio_state.freq, memory_order_relaxed);
    switch (freq) {
        case 192000:
        case 96000:
        case 48000:
            if (is_high_power) {
                vreg_set_voltage(V_CORE_HI);
                busy_wait_us(100);
                set_sys_clock_khz(SYS_CLOCK_KHZ_48K, true);
                busy_wait_us(50);
            } else {
                set_sys_clock_khz(SYS_CLOCK_KHZ_LP_48K, true);
                busy_wait_us(100);
                vreg_set_voltage(V_CORE_LO);
                busy_wait_us(50);
            }
            break;
        case 176400:
        case 88200:
        case 44100:
        default:
            if (is_high_power) {
                vreg_set_voltage(V_CORE_HI);
                busy_wait_us(100);
                set_sys_clock_khz(SYS_CLOCK_KHZ_44K, true);
                busy_wait_us(50);
            } else {
                set_sys_clock_khz(SYS_CLOCK_KHZ_LP_44K, true);
                busy_wait_us(100);
                vreg_set_voltage(V_CORE_LO);
                busy_wait_us(50);
            }
            break;
    }
}

void renew_clock(bool is_high_power) {
    // Core0の割り込みタイマを停止
    cancel_timer0();

    // CPUクロックを再設定
    renew_cpu_clock(is_high_power);

    // Core0の割り込みタイマを再開
    restart_timer0();

    // DMAをクリア
    dma_stop_and_clear();

    // PIO分周率を再設定
    reset_i2s_freq();

    // I2C周波数を再設定
    // i2c_set_baudrate(I2C_PORT, 104 * 100000);
}

// DACを設定するためのI2C
void setup_I2C(void) {
    // run at 100kHz.
    i2c_init(I2C_PORT, 104 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    i2c_dma_initialize(I2C_PORT);
}

void volume_control(void) {
    const bool muted = atomic_load_explicit(&audio_state.mute, memory_order_relaxed);
    const int16_t volume = atomic_load_explicit(&audio_state.acq_volume, memory_order_relaxed);
    if (!muted) {
        if (ENABLE_ESS_DAC_VOLUME) {
            atomic_store_explicit(&audio_state.vol_float, 1.0f, memory_order_relaxed);
            ess_dac_volume();
        } else {
            atomic_store_explicit(
                &audio_state.vol_float,
                saturation_f32(
                    powf(10.0f, (float)volume / (float)VOLUME_RESOLUTION / 20.0f),
                    1.0f,
                    0.0f
                ),
                memory_order_relaxed
            );
        }
    } else {
        atomic_store_explicit(&audio_state.vol_float, 0.0f, memory_order_relaxed);
    }
}
