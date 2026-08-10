/*
* Copyright (c) 2025 ArqAlice
*
* Released under the MIT license
* https://opensource.org/licenses/mit-license.php
*/

#ifndef PICO2_COMMON_H
#define PICO2_COMMON_H

#include <stdint.h>
#include <hardware/i2c.h>
#include <hardware/vreg.h>

#include "ess_specific.h"
#include "ringbuffer.h"

#define CORE1_FIR_MODE_HALF_BAND UINT8_C(0)
#define CORE1_FIR_MODE_POLYPHASE UINT8_C(1)
#define CORE1_FIR_MODE_FFT UINT8_C(2)

// User Configurable ------------------------------------------------------------------

// Test mode
static constexpr bool TEST_MODE = true;
static constexpr uint32_t TEST_PIN1 = 1;
static constexpr uint32_t TEST_PIN2 = 2;

// String Desc.
static constexpr char MFG_NAME[] = "ArqAlice";
static constexpr char DEVICE_NAME[] = "Pico2 UltraHiRes USB-DDC";
static constexpr char WEBSITE_ADDR[] = "y.tomi0131@gmail.com:";

// Faster I2S slew rate
static constexpr bool I2S_SLEWRATE_FAST_ENABLE = false;

// Enhancement I2S signal output current
static constexpr bool I2S_STRENGTH_REINFORCE_ENABLE = true;

// Power Mode Switch Pin
// The Hi-Power Mode, Core0 uses 384KHz FIR Filter
static constexpr uint32_t POWER_MODE_SWITCH_PIN = 0;
static constexpr bool ALWAYS_HIGH_POWER = true;

// I2C
static i2c_inst_t *const I2C_PORT = i2c1;
static constexpr uint32_t I2C_SDA = 6;
static constexpr uint32_t I2C_SCL = 7;

// I2S Pin : sideset0:BCLK, sideset1:LRCK (if No Changed)
static constexpr bool I2S_SIDESET_CHANGE = false;
static constexpr uint32_t I2S_DATA_PIN = 26;
static constexpr uint32_t I2S_SIDESET_BASE = 27;

// Upsampler control
// Core0 ratio is for 48k family; 96k uses /2, 192k uses /4 (min 1).
static constexpr uint16_t CORE0_UP_RATIO_HP = 8;
static constexpr uint16_t CORE0_UP_RATIO_LP = 4;
// Core1 ratio is applied directly (1/2/4 are supported).
static constexpr uint16_t CORE1_UP_RATIO_HP = 1;
static constexpr uint16_t CORE1_UP_RATIO_LP = 1;
// Adjust this according to your filter to avoid clipping.
static constexpr float DEFAULT_GAIN_RATIO = 0.75f;

// ESS DAC Specific
static constexpr bool USE_ESS_DAC = false;
static constexpr uint8_t KIND_ESS_DAC = ESS_DAC_NONE;
// 8-bit address byte as shown in the datasheet.
static constexpr uint8_t I2C_ESS_DAC_ADDR = ADDR0_NONE;
static constexpr bool ENABLE_ES9038Q2M_DEPOP = false;
static constexpr bool ENABLE_ESS_DAC_VOLUME = false;
static constexpr bool ENABLE_ESS_DAC_THD_COMPEN = false;
static constexpr bool ENABLE_ESS_THD_COMPEN_VOL_CORR = false;
static constexpr int16_t ESS_THD_COMPEN_C2 = 0;
static constexpr int16_t ESS_THD_COMPEN_C3 = 0;
static constexpr uint8_t ESS_DPLL_BANDWIDTH = 0xA0; // 0~255, 0 is DPLL off
static constexpr uint8_t ESS_DPLL_LOCKSPEED = 8; // 0~16
static constexpr int64_t TIME_ES9038Q2M_DEPOP_USEC = INT64_C(40000);
static constexpr uint32_t DAC_ENABLE_PIN = 5;

// Other Function
static constexpr bool USE_EXT_POWER_ENABLE = false;
static constexpr uint32_t EXT_POWER_ENABLE_PIN = 3;
static constexpr uint32_t BOOT_WAIT_TIME_US = 50000;

// User Configurable end ------------------------------------------------------------

// システムクロック
//#define SYS_CLOCK_KHZ_44K (412000)
//#define SYS_CLOCK_KHZ_48K (430000)
static constexpr uint32_t SYS_CLOCK_KHZ_44K = 336000;
static constexpr uint32_t SYS_CLOCK_KHZ_48K = 336000;
static constexpr uint32_t SYS_CLOCK_KHZ_LP_44K = 208000;
static constexpr uint32_t SYS_CLOCK_KHZ_LP_48K = 208000;
// 208M8/48k/64 = 67.968->68, 208M8/44k1/64 = 73.979->74

// Core Voltage
//#define V_CORE_HI VREG_VOLTAGE_1_50
static constexpr enum vreg_voltage V_CORE_HI = VREG_VOLTAGE_1_25;
static constexpr enum vreg_voltage V_CORE_LO = VREG_VOLTAGE_1_05;

// 初期オーディオサンプル周波数
static constexpr uint32_t AUDIO_INITIAL_FREQ = 44100;

// アップサンプリング倍率(Core0)
static constexpr uint16_t CORE0_UP_RATIO_MAX =
    CORE0_UP_RATIO_HP > CORE0_UP_RATIO_LP ? CORE0_UP_RATIO_HP : CORE0_UP_RATIO_LP;
static constexpr uint16_t CORE1_UP_RATIO_MAX =
    CORE1_UP_RATIO_HP > CORE1_UP_RATIO_LP ? CORE1_UP_RATIO_HP : CORE1_UP_RATIO_LP;

// Core1 FIR Filter Mode
#define CORE1_FIR_MODE CORE1_FIR_MODE_POLYPHASE

// DCDC Control
static constexpr uint32_t DCDC_MODE_PIN = 23;

// LED
static constexpr uint32_t ONBOARD_LED_PIN = 25;

// Core0 Timer0 period (us) 雑多な処理用
static constexpr int64_t TIMER0_US = INT64_C(250);

// Core1 DMA/processing chunk size (us). Larger values give more upsampling time.
static constexpr uint32_t CORE1_PROCESS_US = 2000;

// エンドポイントバッファサイズ((96+1)kHz*1ms=97以上あればよい)
static constexpr uint32_t SIZE_EP_BUFFER = 512;

// アップサンプリングバッファサイズ
// Core0 Upsampling Buffer Size (samples per channel)
static constexpr uint32_t SIZE_UPSAMPLE_CORE0 = 8192;

// DMA転送バッファサイズ 3以上あればよい
static constexpr uint32_t DEPTH_DMA_TX_BUFFER = 3;

// FB水位(50%が望ましい)
static constexpr int32_t SIZE_BUFFER_FB_THRESHOLD = (int32_t)(SIZE_UPSAMPLE_CORE0 / UINT32_C(2));

// Feedback(±1サンプルになる値を返す 基準は1000)
static constexpr int32_t FB_ADJ_LIMIT = INT32_C(1000);

// ボリューム管理
static constexpr int16_t VOLUME_RESOLUTION = INT16_C(256);
static constexpr int16_t MIN_VOLUME = INT16_C(-64) * VOLUME_RESOLUTION;
static constexpr int16_t MAX_VOLUME = INT16_C(0);
static constexpr int16_t DEFAULT_VOLUME = INT16_C(0);

// ノンブロッキングI2C
static constexpr uint16_t SIZE_I2C_RINGBUFFER = 8;
static constexpr uint8_t SIZE_I2C_TRANSFER_MAX = 32;
static constexpr int64_t I2C_ESS_DAC_TRANSFER_INTERVAL_USEC = INT64_C(90);

typedef struct {
    uint32_t freq; // 周波数系列軸・倍率軸用(既存)
    uint32_t bit_depth; // ビット深度軸用(追加)
    int16_t now_volume;
    int16_t acq_volume;
    float vol_float;
    int16_t vol_mul;
    uint32_t vol_shift;
    bool mute;
} AUDIO_STATE;

extern RINGBUFFER buffer_ep_Lch;
extern RINGBUFFER buffer_ep_Rch;
extern RINGBUFFER buffer_upsr_data_Lch_0;
extern RINGBUFFER buffer_upsr_data_Rch_0;

extern AUDIO_STATE audio_state;
extern volatile bool is_high_power_mode;
extern uint32_t now_playing;
extern uint16_t length_remain_to_I2S_FIFO;

uint32_t calc_pwm_period_us(float period_us, uint16_t prescale);
void setup_I2C(void);
void volume_control(void);

void renew_clock(bool is_high_power);
void cancel_timer0(void);
void restart_timer0(void);
void core1_main(void);

static inline int32_t saturation_i32(int32_t in, int32_t max, int32_t min) {
    if (in > max) {
        return max;
    } else if (in < min) {
        return min;
    }
    return in;
}

static inline float saturation_f32(float in, float max, float min) {
    if (in > max) {
        return max;
    } else if (in < min) {
        return min;
    }
    return in;
}

// アップサンプリング倍率取得関数(Core0)
static inline uint16_t get_ratio_upsampling_core0(uint32_t freq) {
    const uint16_t base = is_high_power_mode ? CORE0_UP_RATIO_HP : CORE0_UP_RATIO_LP;
    uint16_t ratio;
    switch (freq) {
        case 192000:
        case 176400:
            ratio = base / 4u;
            break;
        case 96000:
        case 88200:
            ratio = base / 2u;
            break;
        case 48000:
        case 44100:
        default:
            ratio = base;
            break;
    }

    if (ratio == 0) {
        return 1;
    }
    return ratio;
}

static inline uint16_t get_ratio_upsampling_core1(void) {
    const uint16_t ratio = is_high_power_mode ? CORE1_UP_RATIO_HP : CORE1_UP_RATIO_LP;
    if (ratio == 0) {
        return 1;
    }
    return ratio;
}

#endif
