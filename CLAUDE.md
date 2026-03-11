# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Flash (PlatformIO)

```bash
# 編譯
pio run

# 編譯 + 燒入
pio run --target upload

# Serial Monitor (115200 baud)
pio device monitor

# 一次完成燒入並開啟 Monitor
pio run --target upload && pio device monitor
```

VSCode 快捷鍵：`Ctrl+Alt+B` 編譯、`Ctrl+Alt+U` 燒入。

## Hardware Target

**SDVC31 Series Vibratory Feeder Controller** (南京創優科技)
- 調速介面：A2 口 0–5V 類比輸入（遠端調速）
- 啟停介面：C2 口（料滿停機輸入，收到訊號 = 停止）、C3 口 24V 輸出

## Architecture

所有原始碼位於 `src/`，模組職責如下：

| 檔案 | 職責 |
|------|------|
| `main.cpp` | setup/loop、硬體重置按鈕偵測 |
| `config.h` | **所有腳位與常數的唯一定義處**（改腳位只需改這裡）|
| `wifi_manager` | AP 模式熱點建立 + STA 模式連線 + NVS 讀寫（WiFi & MQTT 設定） |
| `dac_controller` | MCP4725 I2C DAC，`dac_set_percent(0–100)` 輸出 0–5V |
| `opto_controller` | PC817 光耦，`opto_set_run(true/false)` 控制啟停 |
| `mqtt_manager` | PubSubClient 封裝，訂閱 cmd/run、cmd/speed，發布 status |
| `web_handler` | HTTP Server 路由（控制頁 + REST API） |

### 資料流

```
MQTT broker  ──→  mqtt_manager  ──→  dac_controller / opto_controller
瀏覽器 Web UI ──→  web_handler   ──→  dac_controller / opto_controller
```

### 啟動流程

1. `dac_init()` + `opto_init()`（預設停止）
2. `wifi_manager_init()` — NVS 有設定 → STA 連線；無設定或逾時 → AP 模式
3. AP 模式：Web 頁面同時設定 WiFi + MQTT，儲存後重啟
4. STA 連線成功後 → `mqtt_manager_init()`

### WiFi 重設

- Web：`GET /reset`
- 硬體：長按 BOOT 鍵（GPIO 0）3 秒（定義於 `RESET_HOLD_MS`）

## Key Constants (src/config.h)

| 常數 | 預設值 | 說明 |
|------|--------|------|
| `DAC_SDA_PIN` / `DAC_SCL_PIN` | 21 / 22 | MCP4725 I2C |
| `DAC_I2C_ADDR` | `0x60` | A0=GND；A0=VCC 時改 `0x61` |
| `OPTO_RUN_PIN` | 26 | PC817 IN1（HIGH=停止，LOW=運行）|
| `AP_SSID` | `VibMachine-Setup` | 初次設定熱點名稱 |
| `NVS_NAMESPACE` | `cfg` | ESP32 NVS 命名空間 |

## MQTT Topics

主題格式：`vibratory/{MAC後6碼}/…`

| 方向 | 主題 | Payload |
|------|------|---------|
| 訂閱 | `cmd/run` | `"1"` / `"0"` |
| 訂閱 | `cmd/speed` | `0`–`100`（%）|
| 發布 | `status` | `{"run":bool,"speed":int}` |

## REST API

| Endpoint | 說明 |
|----------|------|
| `GET /api/run?v=1\|0` | 啟動 / 停止 |
| `GET /api/speed?v=0-100` | 設定轉速 % |
| `GET /api/status` | 回傳 JSON 狀態 |
| `GET /reset` | 清除 NVS 並重啟進 AP 模式 |

## Libraries

- `robtillaart/MCP4725 @ ^0.4.3` — DAC（無額外依賴，API：`begin()` / `setValue(0–4095)`）
- `knolleary/PubSubClient @ ^2.8` — MQTT 客戶端
