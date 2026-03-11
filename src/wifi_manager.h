#pragma once
#include <WebServer.h>
#include <Preferences.h>

void wifi_manager_init(Preferences* prefs, WebServer* server);
void wifi_manager_loop();
bool wifi_manager_is_connected();
String wifi_manager_get_ip();
