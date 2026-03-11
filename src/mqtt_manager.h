#pragma once
#include <Preferences.h>

void mqtt_manager_init(Preferences* prefs);
void mqtt_manager_loop();
bool mqtt_manager_is_connected();
