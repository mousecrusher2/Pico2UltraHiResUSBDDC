/*
* Copyright (c) 2025 ArqAlice 
*
* Released under the MIT license
* https://opensource.org/licenses/mit-license.php
*/

#ifndef PICO2_DEBUG_WITH_GPIO_H
#define PICO2_DEBUG_WITH_GPIO_H

#include <stdint.h>

static constexpr uint8_t GPIO_FOR_DEBUG_0 = UINT8_C(2);
static constexpr uint8_t GPIO_FOR_DEBUG_1 = UINT8_C(3);
static constexpr uint8_t GPIO_FOR_DEBUG_2 = UINT8_C(4);
static constexpr uint8_t GPIO_FOR_DEBUG_3 = UINT8_C(5);

typedef union value2gpio {
    int16_t vINT16_T;
    uint16_t vUINT16_T;
    struct gpio {
        bool bit0 : 1;
        bool bit1 : 1;
        bool bit2 : 1;
        bool bit3 : 1;
        bool bit4 : 1;
        bool bit5 : 1;
        bool bit6 : 1;
        bool bit7 : 1;
        bool bit8 : 1;
        bool bit9 : 1;
        bool bit10 : 1;
        bool bit11 : 1;
        bool bit12 : 1;
        bool bit13 : 1;
        bool bit14 : 1;
        bool bit15 : 1;
    } gpio;
} value2gpio;

void initialize_gpio_debugging(uint8_t gpio0, uint8_t gpio1, uint8_t gpio2, uint8_t gpio3);
void gpio_toggle(uint8_t gpio);
void uint8_to_single_gpio(uint8_t gpio, uint8_t in_value);
void uint16_to_gpio(int16_t in_value);
void uint8_to_gpio(uint8_t in_value);

#endif
