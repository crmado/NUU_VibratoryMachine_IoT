#pragma once
#include <Arduino.h>

void dac_init();
void dac_set_percent(uint8_t percent); // 0=停止 100=全速 → SDVC31 A2 0~5V
uint8_t dac_get_percent();
uint16_t dac_get_raw();  // 目前寫入 MCP4725 的 12-bit 原始值（0–4095）
float dac_get_voltage(); // 目前輸出電壓（V）
bool dac_is_ok();        // MCP4725 是否初始化成功
