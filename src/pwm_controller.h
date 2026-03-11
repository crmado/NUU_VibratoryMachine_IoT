#pragma once
#include <Arduino.h>

void pwm_init();
void pwm_set_duty(uint8_t duty);   // 0 = 停止, 255 = 全速
uint8_t pwm_get_duty();
