/*
* Copyright (c) 2025 ArqAlice 
*
* Released under the MIT license
* https://opensource.org/licenses/mit-license.php
*/

#ifndef PICO2_UPSAMPLING_H
#define PICO2_UPSAMPLING_H

#include <stdint.h>
#include <pico/stdlib.h>

static constexpr uint32_t NUM_OF_CH = UINT32_C(2);
static constexpr uint32_t NUM_OF_BQ_SUB_PARAMS = UINT32_C(6);
static constexpr uint32_t SIZE_BQ_FILTER_0 = UINT32_C(17);
static constexpr uint32_t SIZE_BQ_DELAY_0 = SIZE_BQ_FILTER_0;

static constexpr uint32_t SIZE_BQ_FILTER_1 = UINT32_C(5);
static constexpr uint32_t SIZE_BQ_DELAY_1 = SIZE_BQ_FILTER_1;

static constexpr uint32_t SIZE_BQ_FILTER_2 = UINT32_C(5);
static constexpr uint32_t SIZE_BQ_DELAY_2 = SIZE_BQ_FILTER_2;

static constexpr uint32_t SIZE_BQ_FILTER_3 = UINT32_C(5);
static constexpr uint32_t SIZE_BQ_DELAY_3 = SIZE_BQ_FILTER_3;

static constexpr uint32_t SIZE_BQ_FILTER_4 = UINT32_C(3);
static constexpr uint32_t SIZE_BQ_DELAY_4 = SIZE_BQ_FILTER_4;

static constexpr uint32_t SIZE_FIR_FILTER_0 = UINT32_C(128);
static constexpr uint32_t SIZE_FIR_FILTER_384K = UINT32_C(256);
static constexpr uint32_t SIZE_FIR_FILTER_1 = UINT32_C(48);
static constexpr uint32_t SIZE_FIR_FILTER_2 = UINT32_C(56);

// 双二次フィルタの係数と遅延を定義する
typedef struct {
    float a0, a1, a2;
    float b1, b2;
} BIQUAD_FILTER;

typedef struct {
    float z1, z2;
} BQ_DELAY;

typedef struct {
    BQ_DELAY delay0[SIZE_BQ_DELAY_0];
    BQ_DELAY delay1[SIZE_BQ_DELAY_1];
    BQ_DELAY delay2[SIZE_BQ_DELAY_2];
    BQ_DELAY delay3[SIZE_BQ_DELAY_3];
    BQ_DELAY delay4[SIZE_BQ_DELAY_4];
} DELAY_DATA;

// フィルタ係数
extern const float coef_bq_filter_2x_0[SIZE_BQ_FILTER_0][NUM_OF_BQ_SUB_PARAMS];
extern const float coef_bq_filter_2x_1[SIZE_BQ_FILTER_1][NUM_OF_BQ_SUB_PARAMS];
extern const float coef_bq_filter_2x_2[SIZE_BQ_FILTER_2][NUM_OF_BQ_SUB_PARAMS];
extern const float coef_bq_filter_2x_3[SIZE_BQ_FILTER_3][NUM_OF_BQ_SUB_PARAMS];
extern const float coef_bq_filter_4x_0[SIZE_BQ_FILTER_4][NUM_OF_BQ_SUB_PARAMS];

extern const float coef_fir_filter_4x_0[SIZE_FIR_FILTER_0];
extern const float coef_fir_filter_4x_0_linear[SIZE_FIR_FILTER_0];
extern const uint32_t size_coef_fir_filter_4x_0;
extern const float coef_fir_filter_2x_1[SIZE_FIR_FILTER_1];
extern const float coef_fir_filter_2x_1_linear[SIZE_FIR_FILTER_1];
extern const uint32_t size_coef_fir_filter_2x_1;
extern const float coef_fir_filter_2x_2[SIZE_FIR_FILTER_2];
extern const uint32_t size_coef_fir_filter_2x_2;
extern const float coef_fir_filter_384k_linear[SIZE_FIR_FILTER_384K];
extern const float coef_fir_filter_384k[SIZE_FIR_FILTER_384K];
extern const uint32_t size_coef_fir_filter_384k;

void init_upsampling_filter(void);
void clear_bq_filter_delay(void);
void clear_core1_halfband_state(void);
void clear_core1_polyphase_state(void);
void __not_in_flash_func(upsampling_process_core0)(void);
uint32_t __not_in_flash_func(upsampling_process_core1)(
    const float *in_L,
    const float *in_R,
    float *out_L,
    float *out_R,
    uint32_t length
);
uint32_t upsampling_core1_get_block_len(void);

#endif /* PICO2_UPSAMPLING_H */
