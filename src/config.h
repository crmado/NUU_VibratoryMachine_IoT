#pragma once

// ─── WiFi AP 設定（初次設定用的熱點）───────────────────────
#define AP_SSID     "VibMachine-Setup"   // 手機搜尋這個 SSID
#define AP_PASSWORD "YOUR_AP_PASSWORD"           // 至少 8 字元
#define AP_IP       "192.168.4.1"        // AP 模式下的 IP

// ─── WiFi 連線逾時設定 ────────────────────────────────────
#define WIFI_CONNECT_TIMEOUT_MS  15000   // 15 秒連不上就進入 AP 模式

// ─── PWM 振動機設定 ───────────────────────────────────────
#define VIBRATOR_PIN        25           // GPIO 25 輸出 PWM
#define VIBRATOR_CHANNEL    0            // LEDC channel (0~15)
#define VIBRATOR_FREQ_HZ    1000         // PWM 頻率 1kHz
#define VIBRATOR_RESOLUTION 8            // 8-bit = 0~255

// ─── NVS 存儲 key 名稱 ────────────────────────────────────
#define NVS_NAMESPACE   "wifi_cfg"
#define NVS_KEY_SSID    "ssid"
#define NVS_KEY_PASS    "pass"
