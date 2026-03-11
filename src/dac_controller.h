#pragma once
#include <Arduino.h>

void    dac_init();
void    dac_set_percent(uint8_t percent);  // 0=停止 100=全速 → SDVC31 A2 0~5V
uint8_t dac_get_percent();
