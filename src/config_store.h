#pragma once
#include <Arduino.h>

// ESP8266 沒有 NVS，改用 EEPROM 儲存固定結構體
// 取代 ESP32 的 Preferences 功能

#define CONFIG_MAGIC 0xCAFEBABEUL

struct AppConfig
{
    uint32_t magic; // CONFIG_MAGIC 代表資料有效
    char ssid[64];
    char pass[64];
    char mqtt_host[64];
    uint16_t mqtt_port;
    char mqtt_user[32];
    char mqtt_pass[32];
};

void config_store_init();
AppConfig config_store_load();
void config_store_save(const AppConfig &c);
void config_store_clear();
bool config_store_valid(const AppConfig &c);
