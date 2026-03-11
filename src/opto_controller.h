#pragma once
#include <Arduino.h>

void opto_init();
void opto_set_run(bool run);   // true=運行, false=停止
bool opto_get_run();
