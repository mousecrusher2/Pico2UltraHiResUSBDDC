/*
* Copyright (c) 2025 ArqAlice 
*
* Released under the MIT license
* https://opensource.org/licenses/mit-license.php
*/

#ifndef PICO2_ESS_SPECIFIC_H
#define PICO2_ESS_SPECIFIC_H

#include <stdint.h>

// ESS DAC Kind
static constexpr uint8_t ESS_DAC_NONE = 0;
static constexpr uint8_t ES9010K2M = 1;
static constexpr uint8_t ES9038Q2M = 2;
static constexpr uint8_t ES9039Q2M = 3;
static constexpr uint8_t ES9038PRO = 4;
static constexpr uint8_t ES9039PRO = 5;

// ESS DAC Default ADDR
static constexpr uint8_t ADDR0_NONE = 0x00;
static constexpr uint8_t ADDR0_ES9010K2M = 0x90;
static constexpr uint8_t ADDR0_ES9038Q2M = 0x90;
static constexpr uint8_t ADDR0_ES9039Q2M = 0x90;
static constexpr uint8_t ADDR0_ES9039PRO = 0x90;
static constexpr uint8_t ADDR1_ES9039PRO = 0x92;

void ess_dac_i2c_setup(void);
void ess_dac_initialize(void);
void ess_dac_activate(void);

void ess_dac_volume(void);
bool get_ess_dac_mute(void);
void ess_dac_mute(void);
void ess_dac_unmute(void);

#endif
