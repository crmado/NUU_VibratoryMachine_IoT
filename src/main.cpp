/**
 * NUU Vibratory Machine IoT Controller
 * ESP32 NodeMCU + MCP4725 DAC + PC817 Optocoupler
 * Target: SDVC31 Series Vibratory Feeder Controller
 *
 * 硬體：
 *   MCP4725 DAC → SDVC31 A2 口（0~5V 遠端調速）
 *   PC817 光耦   → SDVC31 C2 口（啟停控制）
 *
 * WiFi 重設：長按 BOOT 鍵（GPIO0）3 秒
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "config.h"
#include "wifi_manager.h"
#include "dac_controller.h"
#include "opto_controller.h"
#include "web_handler.h"
#include "mqtt_manager.h"

WebServer   server(80);
Preferences preferences;

// ── 硬體重置按鈕（BOOT 鍵長按 3 秒）──────────────────────────
static unsigned long _btn_press_time = 0;
static bool          _btn_armed      = false;

static void check_reset_button() {
    bool pressed = (digitalRead(RESET_BUTTON_PIN) == LOW);
    if (pressed && !_btn_armed) {
        _btn_armed      = true;
        _btn_press_time = millis();
    } else if (!pressed) {
        _btn_armed = false;
    } else if (_btn_armed && (millis() - _btn_press_time > RESET_HOLD_MS)) {
        Serial.println("[SYS] BOOT 按鈕長按 → 清除所有設定 → 重啟");
        preferences.begin(NVS_NAMESPACE, false);
        preferences.clear();
        preferences.end();
        ESP.restart();
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== NUU Vibratory Machine IoT ===");

    pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

    dac_init();
    opto_init();

    wifi_manager_init(&preferences, &server);
    web_handler_init(&server);
    server.begin();
    Serial.println("[Web] HTTP Server 啟動");

    if (wifi_manager_is_connected()) {
        mqtt_manager_init(&preferences);
    }
}

void loop() {
    check_reset_button();
    server.handleClient();
    wifi_manager_loop();
    mqtt_manager_loop();
}
