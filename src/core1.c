/*
* Copyright (c) 2025 ArqAlice 
*
* Released under the MIT license
* https://opensource.org/licenses/mit-license.php
 */

#include <pico/stdlib.h>
#include "common.h"
#include "transmit_to_dac.h"
#include "upsampling.h"

void core1_main(void) { // NOLINT(misc-use-internal-linkage): launched from main.c.
    // I2S初期化
    init_i2s_interface();

    while (true) {
        service_core1_upsampling_reset();
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
