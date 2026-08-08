/*
* Copyright (c) 2025 ArqAlice 
*
* Released under the MIT license
* https://opensource.org/licenses/mit-license.php
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "debug_with_gpio.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "hardware/uart.h"
#include "hardware/vreg.h"
#include "hardware/watchdog.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "ringbuffer.h"
#include "transmit_to_dac.h"
#include "upsampling.h"

void core1_main() {
    // I2S初期化
    init_i2s_interface();

    while (true) {
        if (TEST_MODE) {
            gpio_put(TEST_PIN2, true);
        }
        dma_tx_start();
        if (TEST_MODE) {
            gpio_put(TEST_PIN2, false);
        }
        sleep_us(1);
    }
}