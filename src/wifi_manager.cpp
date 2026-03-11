#include "wifi_manager.h"
#include "config.h"
#include <Arduino.h>

static Preferences* _prefs     = nullptr;
static WebServer*   _server    = nullptr;
static bool         _is_ap_mode = false;

// ── NVS 讀寫 ──────────────────────────────────────────────────
static String load_str(const char* key, const char* def = "") {
    _prefs->begin(NVS_NAMESPACE, true);
    String v = _prefs->getString(key, def);
    _prefs->end();
    return v;
}

static void save_str(const char* key, const String& val) {
    _prefs->begin(NVS_NAMESPACE, false);
    _prefs->putString(key, val);
    _prefs->end();
}

static void save_int(const char* key, int val) {
    _prefs->begin(NVS_NAMESPACE, false);
    _prefs->putInt(key, val);
    _prefs->end();
}

static void clear_all() {
    _prefs->begin(NVS_NAMESPACE, false);
    _prefs->clear();
    _prefs->end();
    Serial.println("[WiFi] 所有設定已清除");
}

// ── AP 模式設定頁面 ────────────────────────────────────────────
static void start_ap_mode() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);

    IPAddress apIP(192, 168, 4, 1);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(AP_SSID, AP_PASSWORD);

    _is_ap_mode = true;
    Serial.println("[WiFi] AP 模式啟動");
    Serial.printf("  SSID    : %s\n", AP_SSID);
    Serial.printf("  Password: %s\n", AP_PASSWORD);
    Serial.printf("  URL     : http://%s\n", AP_IP);

    // ── 設定頁面（WiFi + MQTT）──────────────────────────────────
    _server->on("/", HTTP_GET, []() {
        String html = R"rawhtml(
<!DOCTYPE html>
<html lang="zh-TW">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>裝置設定</title>
<style>
  *{box-sizing:border-box}
  body{font-family:sans-serif;background:#f0f4f8;margin:0;padding:16px}
  .card{background:#fff;border-radius:12px;padding:20px;max-width:420px;margin:auto;box-shadow:0 2px 12px rgba(0,0,0,.1)}
  h2{margin-top:0;color:#222}
  h3{color:#555;margin:20px 0 8px;border-top:1px solid #eee;padding-top:14px}
  label{display:block;margin:10px 0 3px;font-size:14px;font-weight:bold}
  input{width:100%;padding:9px;font-size:15px;border:1px solid #bbb;border-radius:6px}
  input[type=number]{width:100px}
  .row{display:flex;gap:8px;align-items:flex-end}
  .row input{flex:1}
  button{width:100%;padding:12px;margin-top:18px;font-size:16px;background:#0078d4;color:#fff;border:none;border-radius:8px;cursor:pointer}
  button:hover{background:#005fa3}
  .note{color:#888;font-size:12px;margin-top:10px}
  .opt{color:#999;font-size:12px}
</style>
</head>
<body>
<div class="card">
  <h2>&#9881; 裝置初始設定</h2>

  <form method="POST" action="/save">
    <h3>&#128246; WiFi 設定</h3>
    <label>WiFi 名稱 (SSID) <span style="color:red">*</span></label>
    <input type="text" name="ssid" placeholder="您的 WiFi 名稱" required>
    <label>WiFi 密碼</label>
    <input type="password" name="pass" placeholder="WiFi 密碼">

    <h3>&#128200; MQTT 設定 <span class="opt">（選填）</span></h3>
    <label>Broker 位址</label>
    <input type="text" name="mqtt_host" placeholder="192.168.1.100 或 broker.hivemq.com">
    <label>Port</label>
    <input type="number" name="mqtt_port" value="1883" min="1" max="65535">
    <label>帳號 <span class="opt">（選填）</span></label>
    <input type="text" name="mqtt_user" placeholder="MQTT 帳號">
    <label>密碼 <span class="opt">（選填）</span></label>
    <input type="password" name="mqtt_pass" placeholder="MQTT 密碼">

    <button type="submit">儲存並重啟</button>
  </form>
  <p class="note">儲存後裝置重啟，連上您的 WiFi 後即可透過 MQTT 或 Web 控制振動機。</p>
</div>
</body>
</html>
)rawhtml";
        _server->send(200, "text/html; charset=utf-8", html);
    });

    // ── 接收並儲存所有設定 ──────────────────────────────────────
    _server->on("/save", HTTP_POST, []() {
        String ssid = _server->arg("ssid");
        if (ssid.isEmpty()) {
            _server->send(400, "text/plain", "WiFi SSID 不能為空");
            return;
        }

        save_str(NVS_KEY_SSID, ssid);
        save_str(NVS_KEY_PASS, _server->arg("pass"));

        String mqttHost = _server->arg("mqtt_host");
        if (!mqttHost.isEmpty()) {
            save_str(NVS_KEY_MQTT_HOST, mqttHost);
            save_int(NVS_KEY_MQTT_PORT, _server->arg("mqtt_port").toInt());
            save_str(NVS_KEY_MQTT_USER, _server->arg("mqtt_user"));
            save_str(NVS_KEY_MQTT_PASS, _server->arg("mqtt_pass"));
        }

        Serial.printf("[WiFi] 已儲存 SSID=%s, MQTT=%s\n",
                      ssid.c_str(), mqttHost.isEmpty() ? "未設定" : mqttHost.c_str());

        _server->send(200, "text/html; charset=utf-8",
            "<html><head><meta charset='UTF-8'></head><body style='font-family:sans-serif;max-width:400px;margin:40px auto;padding:16px'>"
            "<h2>&#10003; 設定完成</h2>"
            "<p>裝置 3 秒後重啟並連上 WiFi。</p>"
            "<p>連上後請在路由器查詢裝置 IP，或查看 Serial Monitor。</p>"
            "</body></html>");
        delay(3000);
        ESP.restart();
    });
}

// ── STA 連線 ──────────────────────────────────────────────────
static bool connect_sta(const String& ssid, const String& pass) {
    Serial.printf("[WiFi] 連線中: %s\n", ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > WIFI_CONNECT_TIMEOUT_MS) {
            Serial.println("\n[WiFi] 連線逾時");
            return false;
        }
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.printf("[WiFi] 連線成功  IP: %s\n", WiFi.localIP().toString().c_str());
    _is_ap_mode = false;
    return true;
}

// ── 公開 API ─────────────────────────────────────────────────
void wifi_manager_init(Preferences* prefs, WebServer* server) {
    _prefs  = prefs;
    _server = server;

    String ssid = load_str(NVS_KEY_SSID);
    String pass = load_str(NVS_KEY_PASS);

    if (ssid.isEmpty()) {
        Serial.println("[WiFi] 無儲存設定 → AP 模式");
        start_ap_mode();
        return;
    }

    if (!connect_sta(ssid, pass)) {
        Serial.println("[WiFi] 連線失敗 → 清除設定 → AP 模式");
        clear_all();
        start_ap_mode();
    }
}

void wifi_manager_loop() {
    if (!_is_ap_mode && WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] 斷線，重試中...");
        connect_sta(load_str(NVS_KEY_SSID), load_str(NVS_KEY_PASS));
    }
}

bool wifi_manager_is_connected() {
    return !_is_ap_mode && WiFi.status() == WL_CONNECTED;
}

String wifi_manager_get_ip() {
    return _is_ap_mode ? String(AP_IP) : WiFi.localIP().toString();
}
