# NUU VibratoryMachine IoT

ESP8266 (NodeMCU V3) 透過 MQTT 遠端控制 SDVC31 振動送料機控制器。

---

## 硬體清單

| 零件         | 型號 / 規格                | 用途               |
| ------------ | -------------------------- | ------------------ |
| 主控板       | NodeMCU V3 (ESP8266 LoLin) | WiFi + MQTT 主控   |
| DAC          | MCP4725 模組 (I2C, 12-bit) | 輸出 0~5V 調速訊號 |
| 光耦模組     | PC817 模組                 | 隔離啟停控制訊號   |
| 振動機控制器 | SDVC31 系列 (南京創優)     | 振動送料機主機     |

---

## NodeMCU V3 腳位圖

![NodeMCU V3 Pinout](docs\img\nodemcu-v3-esp8266-development-board-pinout.jpeg)

```
                      ┌──────────────┐
               ADC0 ──┤ A0       D0  ├── GPIO16
               GND  ──┤ G        D1  ├── GPIO5  (SCL → MCP4725 SCL)
               VUSB ──┤ VU       D2  ├── GPIO4  (SDA → MCP4725 SDA)
              GPIO10 ──┤ S3       D3  ├── GPIO0  (FLASH 重置按鈕)
              GPIO9  ──┤ S2       D4  ├── GPIO2
              MOSI  ──┤ S1       3V  ├── 3.3V
               CS   ──┤ SC       G   ├── GND
               MISO ──┤ SO       D5  ├── GPIO14 (→ PC817 IN+)
               SCLK ──┤ SK       D6  ├── GPIO12
               GND  ──┤ G        D7  ├── GPIO13
               3.3V ──┤ 3V       D8  ├── GPIO15
               EN   ──┤ EN       RX  ├── GPIO3
               RST  ──┤ RST      TX  ├── GPIO1
               GND  ──┤ G        G   ├── GND ◄─── 接 SDVC31 A1 (RGND)
               VIN  ──┤ VIN      3V  ├── 3.3V
                      └──────────────┘
                           [FLASH]  ← 長按 3 秒重置 WiFi 設定
```

**本專案使用的腳位：**

| NodeMCU 標籤 |  GPIO  | 接到                  | 功能           |
| :----------: | :----: | --------------------- | -------------- |
|      D1      | GPIO5  | MCP4725 SCL           | I2C 時脈       |
|      D2      | GPIO4  | MCP4725 SDA           | I2C 資料       |
|      D5      | GPIO14 | PC817 IN+             | 啟停控制       |
|   G (GND)    |  GND   | PC817 IN- / SDVC31 A1 | 共地（必接）   |
|     VIN      | 5V in  | USB 供電              | ESP8266 主電源 |

---

## SDVC31 接線端口說明

```
A 口（調速控制）          C 口（啟停控制）
┌─────────────────┐      ┌─────────────────┐
│ A1  RGND        │      │ C1  GND         │
│ A2  Input 0~5V  │      │ C2  Input (停機)│
│ A3  5V out      │      │ C3  24V out     │
└─────────────────┘      └─────────────────┘
```

---

## 完整接線圖

### 調速控制（A 口 + MCP4725）

```
SDVC31                    MCP4725 模組              NodeMCU V3
──────                    ────────────              ──────────
A3 (5V out) ─────────────► VCC
A1 (RGND)   ─────────────► GND
                           SDA ◄──────────────────── D2 (GPIO4)
                           SCL ◄──────────────────── D1 (GPIO5)
A2 (Input)  ◄────────────  VOUT
A1 (RGND)   ◄──────────────────────────────────────  G (GND) ← 共地！
```

> ⚠️ **MCP4725 必須接 5V**（A3 或外部 5V），接 3.3V 輸出最高只有 3.3V，無法達到全速 5V。

**調速電壓對應：**

| MQTT `cmd/speed` | DAC raw | 輸出電壓 | 振動強度 |
| :--------------: | :-----: | :------: | :------: |
|        0         |    0    |  0.00 V  |   停止   |
|        25        |  1023   |  1.25 V  |   25%    |
|        50        |  2047   |  2.50 V  |   50%    |
|        75        |  3071   |  3.75 V  |   75%    |
|       100        |  4095   |  5.00 V  |   全速   |

---

### 啟停控制（C 口 + PC817 模組）

```
NodeMCU V3              PC817 模組                  SDVC31
──────────              ──────────                  ──────
D5 (GPIO14) ───────────► IN+
G (GND)     ───────────► IN-
                         OUT+ (Collector) ──────────► C3 (24V out)
                         OUT- (Emitter)  ──────────── C2 (Input 停機)
```

**控制邏輯（SDVC31 C 口：收到訊號 = 停止）：**

| MQTT `cmd/run` |   D5 電位   | PC817 狀態 | C2 電位 |   振動機    |
| :------------: | :---------: | :--------: | :-----: | :---------: |
|     `"1"`      |  LOW (0V)   |   不導通   | 無 24V  | ✅ **運行** |
|     `"0"`      | HIGH (3.3V) |    導通    |   24V   | ⛔ **停止** |

> PC817 光耦完全隔離 ESP8266 的 3.3V 側與 SDVC31 的 24V 側，安全無虞。

---

### 供電建議

```
方案 A（推薦）：ESP8266 USB 獨立供電
┌─────────────────────────────────────────────────┐
│  USB 充電頭 5V ──► NodeMCU VIN                   │
│  SDVC31 A3 (5V) ──► MCP4725 VCC                 │
│  SDVC31 A1 (RGND) ──► NodeMCU GND + MCP4725 GND │
└─────────────────────────────────────────────────┘

方案 B：全部從 SDVC31 A3 供電（需確認 A3 電流 > 350mA）
┌─────────────────────────────────────────────────┐
│  SDVC31 A3 (5V) ──► NodeMCU VIN                 │
│  SDVC31 A3 (5V) ──► MCP4725 VCC                 │
│  SDVC31 A1 (RGND) ──► NodeMCU GND + MCP4725 GND │
└─────────────────────────────────────────────────┘
```

---

## MQTT 主題

主題前綴：`vibratory/{MAC後6碼}/`

| 方向          | 主題        | Payload                                                            | 說明                  |
| ------------- | ----------- | ------------------------------------------------------------------ | --------------------- |
| 你發 → 裝置收 | `cmd/run`   | `"1"` / `"0"`                                                      | 啟動 / 停止           |
| 你發 → 裝置收 | `cmd/speed` | `0`–`100`                                                          | 轉速百分比            |
| 裝置發 → 你收 | `status`    | `{"run":bool,"speed":int,"voltage":float,"raw":int,"dac_ok":bool}` | 每 30 秒 + 收到指令後 |
| 裝置發 → 你收 | `online`    | `{"status":"online","ip":"...","rssi":-xx}`                        | 連線成功時            |

---

## REST API（區域網路）

裝置 IP 可從 `online` MQTT 訊息取得，或從路由器查詢。

| Endpoint                 | 說明                           |
| ------------------------ | ------------------------------ |
| `GET /api/run?v=1`       | 啟動振動機                     |
| `GET /api/run?v=0`       | 停止振動機                     |
| `GET /api/speed?v=0-100` | 設定轉速 %                     |
| `GET /api/status`        | 回傳 JSON 狀態                 |
| `GET /reset`             | 清除 WiFi 設定，重啟進 AP 模式 |

---

## 初次設定 WiFi

1. 裝置上電，若無 WiFi 記錄會自動進入 AP 模式
2. 手機 / 電腦連上熱點 `VibMachine-Setup`（密碼：`YOUR_AP_PASSWORD`）
3. 開瀏覽器進入 `192.168.4.1`
4. 填入 WiFi SSID、密碼與 MQTT Broker 資訊，送出後裝置自動重啟連線

**重置方式：**

- 長按板子上 **FLASH 按鈕（D3 / GPIO0）3 秒** → 清除所有設定 → 重啟進 AP 模式
- 或 HTTP：`GET /reset`

---

## 編譯與燒入

```bash
# 編譯
pio run

# 編譯 + 燒入
pio run --target upload

# Serial Monitor (115200 baud)
pio device monitor
```

VSCode 快捷鍵：`Ctrl+Alt+B` 編譯、`Ctrl+Alt+U` 燒入。
