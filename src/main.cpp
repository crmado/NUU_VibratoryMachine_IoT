/**
 * NUU Vibratory Machine IoT Controller
 * ESP32 NodeMCU
 *
 * 功能：
 *  - 初次使用：ESP32 建立 WiFi 熱點 (AP Mode)，手機連上去設定 WiFi
 *  - 設定完成：ESP32 連上家用 WiFi (STA Mode)
 *  - 透過 Web 介面控制 PWM 振動機
 *
 * PWM 腳位預設：GPIO 25 (可在 config.h 修改)
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "config.h"
#include "wifi_manager.h"
#include "pwm_controller.h"
#include "web_handler.h"

WebServer server(80);
Preferences preferences;

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== NUU Vibratory Machine IoT ===");

    // 初始化 PWM 控制器
    pwm_init();

    // 初始化 WiFi 管理器
    wifi_manager_init(&preferences, &server);

    // 啟動 Web Server
    web_handler_init(&server);
    server.begin();
    Serial.println("[Web] Server started");
}

void loop() {
    server.handleClient();
    wifi_manager_loop();
}
