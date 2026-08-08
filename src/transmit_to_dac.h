/*
* Copyright (c) 2025 ArqAlice 
*
* Released under the MIT license
* https://opensource.org/licenses/mit-license.php
*/

#ifndef PICO2_TRANSMIT_TO_DAC_H
#define PICO2_TRANSMIT_TO_DAC_H

#include <stdint.h>

#include "common.h"
#include "pico/stdlib.h"
#include "upsampling.h"

#define SIZE_DMA_TX_BUF \
    (49 * 2 * CORE0_UP_RATIO_MAX * CORE1_UP_RATIO_MAX * CORE1_PROCESS_US / 1000 + 256)

// DMA転送バッファはダブルバッファとして使うので2で十分
#define SIZE_DMA_TX_BUF_STACK (DEPTH_DMA_TX_BUFFER)

typedef struct {
    uint32_t tx_buf[SIZE_DMA_TX_BUF];
    uint32_t tx_size;
} DMA_TX_DATA;

typedef struct {
    DMA_TX_DATA data[DEPTH_DMA_TX_BUFFER];
    volatile uint32_t wp;
    volatile uint32_t rp;
    volatile uint32_t using;
    volatile uint32_t prev_write_length;
} DMA_TX_STRUCTURE;

extern void init_i2s_interface(void);
extern void reset_i2s_freq(void);
extern void __not_in_flash_func(dma_tx_start)(void);
extern void dma_stop_and_clear(void);

#endif /* PICO2_TRANSMIT_TO_DAC_H */
