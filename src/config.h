#pragma once

// ─── WiFi AP 設定 ──────────────────────────────────────────────
#define AP_SSID "VibMachine-Setup"
#define AP_PASSWORD "YOUR_AP_PASSWORD"
#define AP_IP "192.168.4.1"
#define WIFI_CONNECT_TIMEOUT_MS 15000

// ─── MQTT 預設設定 ──────────────────────────────────────────────
#define MQTT_DEFAULT_HOST "YOUR_MQTT_BROKER"
#define MQTT_DEFAULT_PORT 8883 // TLS (8883) or WS+TLS (8084)
#define MQTT_DEFAULT_USER "YOUR_MQTT_USERNAME"
#define MQTT_DEFAULT_PASS "YOUR_MQTT_PASSWORD"
#define MQTT_RECONNECT_MS 5000
#define MQTT_TOPIC_PREFIX "vibratory"
// 訂閱：vibratory/{MAC}/cmd/run    payload: "1"/"0"
// 訂閱：vibratory/{MAC}/cmd/speed  payload: 0-100 (%)
// 發布：vibratory/{MAC}/status     {"run":true,"speed":50}

// ─── MCP4725 DAC (I2C 調速 → SDVC31 A2 口) ─────────────────────
//  接線（NodeMCU v2 / ESP8266）：
//    D2 GPIO4  (SDA) ── MCP4725 SDA
//    D1 GPIO5  (SCL) ── MCP4725 SCL
//    MCP4725 VCC     ── 5V  ← 必須接 5V，輸出才能達到 5V 全範圍
//    MCP4725 GND     ── GND
//    MCP4725 VOUT    ── SDVC31 A2  (0~5V 遠端調速輸入)
//    GND             ── SDVC31 A1  (RGND 類比參考地)
#define DAC_SDA_PIN 4      // D2
#define DAC_SCL_PIN 5      // D1
#define DAC_I2C_ADDR 0x60  // MCP4725 預設地址 (A0=GND)
#define DAC_MAX_VALUE 4095 // 12-bit 最大值

// ─── PC817 光耦隔離 (啟停控制 → SDVC31 C2 口) ──────────────────
//  接線（NodeMCU v2 / ESP8266）：
//    D5 GPIO14 ── PC817 模組 IN1 (模組內有限流電阻)
//    GND       ── PC817 模組 GND1
//    PC817 OUT1 Collector ── SDVC31 C3 (24V 輸出)
//    PC817 OUT1 Emitter   ── SDVC31 C2 (料滿停機輸入)
//
//  邏輯（SDVC31 C 口預設：收到訊號 = 停止）：
//    GPIO14 = LOW  → PC817 不導通 → C2 無 24V → 振動機【運行】
//    GPIO14 = HIGH → PC817 導通   → C2 得到 24V → 振動機【停止】
#define OPTO_RUN_PIN 14 // D5

// ─── 硬體重置按鈕 ──────────────────────────────────────────────
//  使用 NodeMCU 內建的 FLASH 按鈕 (GPIO0 / D3)，無需外接
//  長按 3 秒 → 清除所有設定 → 重啟進 AP 設定模式
#define RESET_BUTTON_PIN 0 // D3 / FLASH button
#define RESET_HOLD_MS 3000
