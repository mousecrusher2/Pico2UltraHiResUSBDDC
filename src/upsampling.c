/*
 * Copyright (c) 2025 ArqAlice
 *
 * Released under the MIT license
 * https://opensource.org/licenses/mit-license.php
 */

#include "upsampling.h"

#include <arm_math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "fft_fir_core0.h"
#include "fft_fir_core1.h"
#include "ringbuffer.h"

// 双二次フィルタ構造体
static arm_biquad_casd_df1_inst_f32 biquad_filter3L;
static arm_biquad_casd_df1_inst_f32 biquad_filter3R;
static arm_biquad_casd_df1_inst_f32 biquad_filter4L;
static arm_biquad_casd_df1_inst_f32 biquad_filter4R;

// 双二次フィルタ状態バッファ
static float biquad3L_state[SIZE_BQ_FILTER_3 * 4];
static float biquad3R_state[SIZE_BQ_FILTER_3 * 4];
static float biquad4L_state[SIZE_BQ_FILTER_4 * 4];
static float biquad4R_state[SIZE_BQ_FILTER_4 * 4];

// 双二次フィルタ係数
static float biquad3_coeffs[SIZE_BQ_FILTER_3 * 5];
static float biquad4_coeffs[SIZE_BQ_FILTER_4 * 5];

// 2x Half-band FIR (time domain)
static float hb_state_L_44[FFT_FIR_HALF_BAND_EVEN_TAPS_44];
static float hb_state_R_44[FFT_FIR_HALF_BAND_EVEN_TAPS_44];
static float hb_state_L_48[FFT_FIR_HALF_BAND_EVEN_TAPS_48];
static float hb_state_R_48[FFT_FIR_HALF_BAND_EVEN_TAPS_48];
static float hb_state_core1_L_44_hi[FFT_FIR_HALF_BAND_EVEN_TAPS_44_HI];
static float hb_state_core1_R_44_hi[FFT_FIR_HALF_BAND_EVEN_TAPS_44_HI];
static float hb_state_core1_L_48_hi[FFT_FIR_HALF_BAND_EVEN_TAPS_48_HI];
static float hb_state_core1_R_48_hi[FFT_FIR_HALF_BAND_EVEN_TAPS_48_HI];

typedef struct {
    uint32_t index;
    float hist[CORE1_POLY_PHASE_LEN_MAX];
} CORE1_POLY_STATE;

static CORE1_POLY_STATE core1_poly_state_L;
static CORE1_POLY_STATE core1_poly_state_R;
static const CORE1_POLY_PROFILE *core1_poly_profile_active;

static void __not_in_flash_func(halfband_interp2)(
    const float *const input,
    float *const output,
    uint32_t length,
    const float *const even_taps,
    uint32_t even_taps_len,
    float center,
    uint32_t center_index,
    float *const state
) {
    for (uint32_t n = 0; n < length; n++) {
        for (uint32_t i = even_taps_len - 1; i > 0; i--) {
            state[i] = state[i - 1];
        }
        state[0] = input[n];

        float acc = 0.0f;
        for (uint32_t i = 0; i < even_taps_len; i++) {
            acc += even_taps[i] * state[i];
        }

        output[(n * 2u)] = acc;
        output[(n * 2u) + 1u] = center * state[center_index];
    }
}

// ARM FIR構造体

// ARM FIR 状態保存バッファ

// BiQuad-IIRフィルタの係数を初期化する
static void initialize_bq_filter_coef(void) {
    for (uint16_t i = 0; i < SIZE_BQ_FILTER_3; i++) {
        biquad3_coeffs[i * 5 + 0] = coef_bq_filter_2x_3[i][0];
        biquad3_coeffs[i * 5 + 1] = coef_bq_filter_2x_3[i][1];
        biquad3_coeffs[i * 5 + 2] = coef_bq_filter_2x_3[i][2];
        biquad3_coeffs[i * 5 + 3] = -coef_bq_filter_2x_3[i][4];
        biquad3_coeffs[i * 5 + 4] = -coef_bq_filter_2x_3[i][5];
    }
    arm_biquad_cascade_df1_init_f32(
        &biquad_filter3L,
        SIZE_BQ_FILTER_3,
        biquad3_coeffs,
        biquad3L_state
    );
    arm_biquad_cascade_df1_init_f32(
        &biquad_filter3R,
        SIZE_BQ_FILTER_3,
        biquad3_coeffs,
        biquad3R_state
    );

    for (uint16_t i = 0; i < SIZE_BQ_FILTER_4; i++) {
        biquad4_coeffs[i * 5 + 0] = coef_bq_filter_4x_0[i][0];
        biquad4_coeffs[i * 5 + 1] = coef_bq_filter_4x_0[i][1];
        biquad4_coeffs[i * 5 + 2] = coef_bq_filter_4x_0[i][2];
        biquad4_coeffs[i * 5 + 3] = -coef_bq_filter_4x_0[i][4];
        biquad4_coeffs[i * 5 + 4] = -coef_bq_filter_4x_0[i][5];
    }
    arm_biquad_cascade_df1_init_f32(
        &biquad_filter4L,
        SIZE_BQ_FILTER_4,
        biquad4_coeffs,
        biquad4L_state
    );
    arm_biquad_cascade_df1_init_f32(
        &biquad_filter4R,
        SIZE_BQ_FILTER_4,
        biquad4_coeffs,
        biquad4R_state
    );
}

// アップサンプリングフィルタの初期化処理
extern void init_upsampling_filter(void) {
    initialize_bq_filter_coef();
    clear_bq_filter_delay();
    fft_fir_core0_init();
    fft_fir_core1_init();
}

// BiQuad-IIRフィルタの遅延バッファをクリアする
extern void clear_bq_filter_delay(void) {
    for (uint16_t i = 0; i < SIZE_BQ_DELAY_3 * 4; i++) {
        biquad3L_state[i] = 0;
        biquad3R_state[i] = 0;
    }
    for (uint16_t i = 0; i < SIZE_BQ_DELAY_4 * 4; i++) {
        biquad4L_state[i] = 0;
        biquad4R_state[i] = 0;
    }
    for (uint16_t i = 0; i < FFT_FIR_HALF_BAND_EVEN_TAPS_44; i++) {
        hb_state_L_44[i] = 0;
        hb_state_R_44[i] = 0;
    }
    for (uint16_t i = 0; i < FFT_FIR_HALF_BAND_EVEN_TAPS_48; i++) {
        hb_state_L_48[i] = 0;
        hb_state_R_48[i] = 0;
    }
    clear_core1_halfband_state();
    clear_core1_polyphase_state();
    fft_fir_core0_reset();
    fft_fir_core1_reset();
}

static void __not_in_flash_func(polyphase_interp2)(
    const CORE1_POLY_PROFILE *const profile,
    const float *const input,
    float *const output,
    uint32_t length,
    CORE1_POLY_STATE *const state
) {
    const uint32_t phase_len = profile->phase_len;
    const float *const even = profile->even_taps;
    const float *const odd = profile->odd_taps;
    uint32_t idx = state->index;

    for (uint32_t n = 0; n < length; n++) {
        idx++;
        if (idx >= phase_len) {
            idx = 0;
        }
        state->hist[idx] = input[n];

        float acc_even = 0.0f;
        float acc_odd = 0.0f;
        uint32_t h = idx;
        for (uint32_t k = 0; k < phase_len; k++) {
            const float sample = state->hist[h];
            acc_even += even[k] * sample;
            acc_odd += odd[k] * sample;
            if (h == 0) {
                h = phase_len - 1;
            } else {
                h--;
            }
        }

        output[(n * 2u)] = acc_even;
        output[(n * 2u) + 1u] = acc_odd;
    }
    state->index = idx;
}

extern void clear_core1_halfband_state(void) {
    for (uint16_t i = 0; i < FFT_FIR_HALF_BAND_EVEN_TAPS_44_HI; i++) {
        hb_state_core1_L_44_hi[i] = 0;
        hb_state_core1_R_44_hi[i] = 0;
    }
    for (uint16_t i = 0; i < FFT_FIR_HALF_BAND_EVEN_TAPS_48_HI; i++) {
        hb_state_core1_L_48_hi[i] = 0;
        hb_state_core1_R_48_hi[i] = 0;
    }
}

extern void clear_core1_polyphase_state(void) {
    memset(&core1_poly_state_L, 0, sizeof(core1_poly_state_L));
    memset(&core1_poly_state_R, 0, sizeof(core1_poly_state_R));
    core1_poly_profile_active = NULL;
}

uint32_t upsampling_core1_get_block_len(void) {
    const uint16_t ratio = get_ratio_upsampling_core1();
    if (ratio <= 1) {
        return 0;
    }
    if (ratio == 4) {
        return 0;
    }

    const uint32_t freq_in = audio_state.freq * get_ratio_upsampling_core0(audio_state.freq);
    const uint32_t freq_out = freq_in * ratio;
    if (freq_out == 1536000u) {
        return 0;
    }
#if CORE1_FIR_MODE == CORE1_FIR_MODE_POLYPHASE
    if (ratio == 2) {
        return 0;
    }
#elif CORE1_FIR_MODE == CORE1_FIR_MODE_HALF_BAND
    if (ratio == 2 && freq_in >= 352800) {
        return 0;
    }
#endif
    const FFT_FIR_PROFILE *const profile =
        fft_fir_core1_select_profile(freq_in, ratio, is_high_power_mode);
    if (profile == NULL) {
        return 0;
    }

    return profile->input_len;
}

// upsampling biquad IIR filter NOS統合版 (RAM上で実行する)
// upsampling biquad IIR filter NOS統合版 (RAM上で実行する)
static uint32_t __not_in_flash_func(fast_BQ_filter_2x_3)(
    uint32_t length,
    const float *p_in,
    float *const p_out,
    arm_biquad_casd_df1_inst_f32 *const S
) {
    uint32_t length_buffer = length;
    float *const NOS_buffer = (float *)malloc(sizeof(float) * (length * 2u));
    float *p_NOS_buffer = NOS_buffer;

    // サンプル数を2倍にする NOS方式
    while (length_buffer--) {
        *(p_NOS_buffer++) = *(p_in);
        *(p_NOS_buffer++) = *(p_in++);
    }

    // BiQuad-IIRフィルタを実行する
    arm_biquad_cascade_df1_f32(S, NOS_buffer, p_out, length * 2u);

    free(NOS_buffer);
    return length * 2u;
}

// upsampling biquad IIR filter NOS統合版 (RAM上で実行する)
static uint32_t __not_in_flash_func(fast_BQ_filter_4x_0)(
    uint32_t length,
    const float *p_in,
    float *const p_out,
    arm_biquad_casd_df1_inst_f32 *const S
) {
    uint32_t length_buffer = length;
    float *const NOS_buffer = (float *)malloc(sizeof(float) * (length * 4u));
    float *p_NOS_buffer = NOS_buffer;

    // サンプル数を4倍にする NOS方式
    while (length_buffer--) {
        *(p_NOS_buffer++) = *(p_in);
        *(p_NOS_buffer++) = *(p_in);
        *(p_NOS_buffer++) = *(p_in);
        *(p_NOS_buffer++) = *(p_in++);
    }

    // BiQuad-IIRフィルタを実行する
    arm_biquad_cascade_df1_f32(S, NOS_buffer, p_out, length * 4u);

    free(NOS_buffer);
    return length * 4u;
}

// アップサンプリングに使用するメモリを静的確保
static int32_t buffer_copy_from_ep_left_ch[FFT_FIR_MAX_INPUT];
static int32_t buffer_copy_from_ep_right_ch[FFT_FIR_MAX_INPUT];
static float fft_stage1_L[FFT_FIR_MAX_OUTPUT];
static float fft_stage1_R[FFT_FIR_MAX_OUTPUT];
static float fft_output_L[FFT_FIR_MAX_OUTPUT];
static float fft_output_R[FFT_FIR_MAX_OUTPUT];

void __not_in_flash_func(upsampling_process_core0)(void) {
    const int32_t size_buf = (int32_t)get_size_using(&buffer_ep_Lch);
    const uint16_t ratio = get_ratio_upsampling_core0(audio_state.freq);
    uint32_t save;
    const bool is_44k1_family =
        (audio_state.freq == 44100 || audio_state.freq == 88200 || audio_state.freq == 176400);

    if (size_buf <= 0) {
        return;
    }

    if (ratio <= 1) {
        const uint32_t length = saturation_i32(size_buf, FFT_FIR_MAX_INPUT, 0);
        if (get_size_remain(&buffer_upsr_data_Lch_0) < (int32_t)length) {
            return;
        }

        save = save_and_disable_interrupts();
        ringbuf_read_array_no_spinlock(buffer_copy_from_ep_left_ch, length, &buffer_ep_Lch);
        ringbuf_read_array_no_spinlock(buffer_copy_from_ep_right_ch, length, &buffer_ep_Rch);
        restore_interrupts(save);

        save = save_and_disable_interrupts();
        ringbuf_write_array_spinlock(buffer_copy_from_ep_left_ch, length, &buffer_upsr_data_Lch_0);
        ringbuf_write_array_spinlock(buffer_copy_from_ep_right_ch, length, &buffer_upsr_data_Rch_0);
        restore_interrupts(save);
        return;
    }

    const uint16_t stage1_ratio = (ratio == 8 && !is_high_power_mode) ? 4 : 0;

    if (stage1_ratio > 0) {
        const FFT_FIR_PROFILE *const profile_stage1 =
            fft_fir_core0_select_profile(audio_state.freq, stage1_ratio, is_high_power_mode);
        if (profile_stage1 == NULL) {
            goto single_stage;
        }

        const uint32_t input_len = profile_stage1->input_len;
        const uint32_t stage1_out_len = input_len * stage1_ratio;
        const uint32_t output_len = stage1_out_len * 2u;
        if (output_len != (input_len * ratio)) {
            goto single_stage;
        }

        if (size_buf < (int32_t)input_len) {
            return;
        }
        if (get_size_remain(&buffer_upsr_data_Lch_0) < (int32_t)output_len) {
            return;
        }

        save = save_and_disable_interrupts();
        ringbuf_read_array_no_spinlock(buffer_copy_from_ep_left_ch, input_len, &buffer_ep_Lch);
        ringbuf_read_array_no_spinlock(buffer_copy_from_ep_right_ch, input_len, &buffer_ep_Rch);
        restore_interrupts(save);

        const uint32_t produced_stage1 = fft_fir_core0_process_block_stage(
            profile_stage1,
            (float *)buffer_copy_from_ep_left_ch,
            (float *)buffer_copy_from_ep_right_ch,
            fft_stage1_L,
            fft_stage1_R,
            0
        );
        if (produced_stage1 == 0 || produced_stage1 != stage1_out_len) {
            return;
        }

        const float *const hb_even =
            is_44k1_family ? fft_fir_halfband_even_44 : fft_fir_halfband_even_48;
        const float hb_center =
            is_44k1_family ? fft_fir_halfband_center_44 : fft_fir_halfband_center_48;
        const uint32_t hb_len =
            is_44k1_family ? FFT_FIR_HALF_BAND_EVEN_TAPS_44 : FFT_FIR_HALF_BAND_EVEN_TAPS_48;
        const uint32_t hb_center_index =
            is_44k1_family ? FFT_FIR_HALF_BAND_CENTER_INDEX_44 : FFT_FIR_HALF_BAND_CENTER_INDEX_48;
        float *const hb_state_L = is_44k1_family ? hb_state_L_44 : hb_state_L_48;
        float *const hb_state_R = is_44k1_family ? hb_state_R_44 : hb_state_R_48;

        halfband_interp2(
            fft_stage1_L,
            fft_output_L,
            produced_stage1,
            hb_even,
            hb_len,
            hb_center,
            hb_center_index,
            hb_state_L
        );
        halfband_interp2(
            fft_stage1_R,
            fft_output_R,
            produced_stage1,
            hb_even,
            hb_len,
            hb_center,
            hb_center_index,
            hb_state_R
        );

        save = save_and_disable_interrupts();
        ringbuf_write_array_spinlock((int32_t *)fft_output_L, output_len, &buffer_upsr_data_Lch_0);
        ringbuf_write_array_spinlock((int32_t *)fft_output_R, output_len, &buffer_upsr_data_Rch_0);
        restore_interrupts(save);
        return;
    }

single_stage: {
    const FFT_FIR_PROFILE *const profile =
        fft_fir_core0_select_profile(audio_state.freq, ratio, is_high_power_mode);
    if (profile == NULL) {
        return;
    }

    const uint32_t input_len = profile->input_len;
    const uint32_t output_len = input_len * profile->up_ratio;

    if (size_buf < (int32_t)input_len) {
        return;
    }
    if (get_size_remain(&buffer_upsr_data_Lch_0) < (int32_t)output_len) {
        return;
    }

    save = save_and_disable_interrupts();
    ringbuf_read_array_no_spinlock(buffer_copy_from_ep_left_ch, input_len, &buffer_ep_Lch);
    ringbuf_read_array_no_spinlock(buffer_copy_from_ep_right_ch, input_len, &buffer_ep_Rch);
    restore_interrupts(save);

    const uint32_t produced = fft_fir_core0_process_block(
        profile,
        (float *)buffer_copy_from_ep_left_ch,
        (float *)buffer_copy_from_ep_right_ch,
        fft_output_L,
        fft_output_R
    );
    if (produced == 0) {
        return;
    }

    save = save_and_disable_interrupts();
    ringbuf_write_array_spinlock((int32_t *)fft_output_L, produced, &buffer_upsr_data_Lch_0);
    ringbuf_write_array_spinlock((int32_t *)fft_output_R, produced, &buffer_upsr_data_Rch_0);
    restore_interrupts(save);
}
}

uint32_t __not_in_flash_func(upsampling_process_core1)(
    const float *const in_L,
    const float *const in_R,
    float *const out_L,
    float *const out_R,
    uint32_t length
) {
    const uint16_t ratio = get_ratio_upsampling_core1();
    if (ratio <= 1) {
        memcpy(out_L, in_L, sizeof(float) * length);
        memcpy(out_R, in_R, sizeof(float) * length);
        return length;
    }
    if (ratio == 4) {
        goto fallback_iir;
    }

    const uint32_t freq_in = audio_state.freq * get_ratio_upsampling_core0(audio_state.freq);
#if CORE1_FIR_MODE == CORE1_FIR_MODE_POLYPHASE
    if (ratio == 2) {
        const CORE1_POLY_PROFILE *profile = NULL;
        switch (freq_in) {
            case 88200:
                profile = is_high_power_mode ? &core1_poly_in88200_out176400_pb20000_sb79380_u2_hp
                                             : &core1_poly_in88200_out176400_pb20000_sb79380_u2_lp;
                break;
            case 96000:
                profile = is_high_power_mode ? &core1_poly_in96000_out192000_pb20000_sb86400_u2_hp
                                             : &core1_poly_in96000_out192000_pb20000_sb86400_u2_lp;
                break;
            case 176400:
                profile = is_high_power_mode
                    ? &core1_poly_in176400_out352800_pb20000_sb158760_u2_hp
                    : &core1_poly_in176400_out352800_pb20000_sb158760_u2_lp;
                break;
            case 192000:
                profile = is_high_power_mode
                    ? &core1_poly_in192000_out384000_pb20000_sb172800_u2_hp
                    : &core1_poly_in192000_out384000_pb20000_sb172800_u2_lp;
                break;
            case 352800:
                profile = is_high_power_mode
                    ? &core1_poly_in352800_out705600_pb20000_sb317520_u2_hp
                    : &core1_poly_in352800_out705600_pb20000_sb317520_u2_lp;
                break;
            case 384000:
                profile = is_high_power_mode
                    ? &core1_poly_in384000_out768000_pb20000_sb345600_u2_hp
                    : &core1_poly_in384000_out768000_pb20000_sb345600_u2_lp;
                break;
            default:
                break;
        }
        if (profile != NULL) {
            if (core1_poly_profile_active != profile) {
                clear_core1_polyphase_state();
                core1_poly_profile_active = profile;
            }
            polyphase_interp2(profile, in_L, out_L, length, &core1_poly_state_L);
            polyphase_interp2(profile, in_R, out_R, length, &core1_poly_state_R);
            return length * 2u;
        }
    }
#elif CORE1_FIR_MODE == CORE1_FIR_MODE_HALF_BAND
    const bool is_44k1_family =
        (audio_state.freq == 44100 || audio_state.freq == 88200 || audio_state.freq == 176400);
    if (ratio == 2 && freq_in >= 352800) {
        const float *const hb_even =
            is_44k1_family ? fft_fir_halfband_even_44_hi : fft_fir_halfband_even_48_hi;
        const float hb_center =
            is_44k1_family ? fft_fir_halfband_center_44_hi : fft_fir_halfband_center_48_hi;
        const uint32_t hb_len =
            is_44k1_family ? FFT_FIR_HALF_BAND_EVEN_TAPS_44_HI : FFT_FIR_HALF_BAND_EVEN_TAPS_48_HI;
        const uint32_t hb_center_index = is_44k1_family ? FFT_FIR_HALF_BAND_CENTER_INDEX_44_HI
                                                        : FFT_FIR_HALF_BAND_CENTER_INDEX_48_HI;
        float *const hb_state_L = is_44k1_family ? hb_state_core1_L_44_hi : hb_state_core1_L_48_hi;
        float *const hb_state_R = is_44k1_family ? hb_state_core1_R_44_hi : hb_state_core1_R_48_hi;
        halfband_interp2(
            in_L,
            out_L,
            length,
            hb_even,
            hb_len,
            hb_center,
            hb_center_index,
            hb_state_L
        );
        halfband_interp2(
            in_R,
            out_R,
            length,
            hb_even,
            hb_len,
            hb_center,
            hb_center_index,
            hb_state_R
        );
        return length * 2u;
    }
#endif
    const FFT_FIR_PROFILE *const profile =
        fft_fir_core1_select_profile(freq_in, ratio, is_high_power_mode);
    if (profile == NULL) {
        goto fallback_iir;
    }

    const uint32_t block_len = profile->input_len;
    if (block_len == 0 || length < block_len) {
        return 0;
    }

    const uint32_t blocks = length / block_len;
    uint32_t out_count = 0;
    for (uint32_t b = 0; b < blocks; b++) {
        const uint32_t offset = b * block_len;
        const uint32_t produced = fft_fir_core1_process_block(
            profile,
            in_L + offset,
            in_R + offset,
            out_L + out_count,
            out_R + out_count
        );
        if (produced == 0) {
            return 0;
        }
        out_count += produced;
    }
    return out_count;

fallback_iir:
    if (ratio == 4) {
        const uint32_t len_L = fast_BQ_filter_4x_0(length, in_L, out_L, &biquad_filter4L);
        fast_BQ_filter_4x_0(length, in_R, out_R, &biquad_filter4R);
        return len_L;
    }
    if (ratio == 2) {
        const uint32_t len_L = fast_BQ_filter_2x_3(length, in_L, out_L, &biquad_filter3L);
        fast_BQ_filter_2x_3(length, in_R, out_R, &biquad_filter3R);
        return len_L;
    }
    return length;
}
