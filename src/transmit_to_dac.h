/*
* Copyright (c) 2025 ArqAlice
*
* Released under the MIT license
* https://opensource.org/licenses/mit-license.php
*/

#ifndef PICO2_TRANSMIT_TO_DAC_H
#define PICO2_TRANSMIT_TO_DAC_H

#include <stdint.h>
#include <pico/time.h>
#include "common.h"

static constexpr uint32_t SIZE_DMA_TX_BUF = UINT32_C(49) * UINT32_C(2) * CORE0_UP_RATIO_MAX
        * CORE1_UP_RATIO_MAX * CORE1_PROCESS_US / UINT32_C(1000)
    + UINT32_C(256);

// DMA転送バッファはダブルバッファとして使うので2で十分
static constexpr uint32_t SIZE_DMA_TX_BUF_STACK = DEPTH_DMA_TX_BUFFER;

typedef struct {
    uint32_t tx_buf[SIZE_DMA_TX_BUF];
    uint32_t tx_size;
} DMA_TX_DATA;

typedef struct {
    DMA_TX_DATA data[DEPTH_DMA_TX_BUFFER];
    uint32_t wp;
    uint32_t rp;
    uint32_t using;
} DMA_TX_STRUCTURE;

extern _Atomic bool enable_output;
extern _Atomic absolute_time_t time_start_output;

void init_i2s_interface(void);
void reset_i2s_freq(void);
void __not_in_flash_func(dma_tx_start)(void);
void dma_stop_and_clear(void);

#endif /* PICO2_TRANSMIT_TO_DAC_H */
