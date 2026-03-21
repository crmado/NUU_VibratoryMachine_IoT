#pragma once
#include <ESP8266WebServer.h>

void wifi_manager_init(ESP8266WebServer *server);
void wifi_manager_loop();
bool wifi_manager_is_connected();
String wifi_manager_get_ip();
