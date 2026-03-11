#include "wifi_manager.h"
#include "config.h"
#include <Arduino.h>

static Preferences* _prefs = nullptr;
static WebServer*   _server = nullptr;
static bool         _is_ap_mode = false;

// ── 讀取儲存的 WiFi 憑證 ────────────────────────────────────
static String load_ssid() {
    _prefs->begin(NVS_NAMESPACE, true);
    String s = _prefs->getString(NVS_KEY_SSID, "");
    _prefs->end();
    return s;
}

static String load_pass() {
    _prefs->begin(NVS_NAMESPACE, true);
    String p = _prefs->getString(NVS_KEY_PASS, "");
    _prefs->end();
    return p;
}

// ── 儲存 WiFi 憑證到 NVS ─────────────────────────────────────
static void save_credentials(const String& ssid, const String& pass) {
    _prefs->begin(NVS_NAMESPACE, false);
    _prefs->putString(NVS_KEY_SSID, ssid);
    _prefs->putString(NVS_KEY_PASS, pass);
    _prefs->end();
    Serial.printf("[WiFi] Saved SSID: %s\n", ssid.c_str());
}

// ── 清除 WiFi 憑證 ────────────────────────────────────────────
static void clear_credentials() {
    _prefs->begin(NVS_NAMESPACE, false);
    _prefs->clear();
    _prefs->end();
    Serial.println("[WiFi] Credentials cleared");
}

// ── 啟動 AP 模式 ──────────────────────────────────────────────
static void start_ap_mode() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);

    IPAddress apIP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(apIP, gateway, subnet);
    WiFi.softAP(AP_SSID, AP_PASSWORD);

    _is_ap_mode = true;
    Serial.println("[WiFi] AP Mode started");
    Serial.printf("  SSID    : %s\n", AP_SSID);
    Serial.printf("  Password: %s\n", AP_PASSWORD);
    Serial.printf("  URL     : http://%s\n", AP_IP);

    // ── AP 模式下的 WiFi 設定頁面 ─────────────────────────────
    _server->on("/", HTTP_GET, []() {
        String html = R"rawhtml(
<!DOCTYPE html>
<html lang="zh-TW">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>WiFi 設定</title>
<style>
  body { font-family: sans-serif; max-width: 400px; margin: 40px auto; padding: 16px; }
  h2   { color: #333; }
  input, button { width: 100%; padding: 10px; margin: 6px 0; box-sizing: border-box; font-size: 16px; border-radius: 6px; border: 1px solid #aaa; }
  button { background: #0078d4; color: #fff; border: none; cursor: pointer; }
  button:hover { background: #005fa3; }
  .note { color: #666; font-size: 13px; margin-top: 12px; }
</style>
</head>
<body>
<h2>&#128246; WiFi 設定</h2>
<p>請輸入要讓振動機連線的 WiFi 名稱與密碼：</p>
<form method="POST" action="/save">
  <label>WiFi 名稱 (SSID)</label>
  <input type="text" name="ssid" placeholder="Your WiFi Name" required>
  <label>WiFi 密碼</label>
  <input type="password" name="pass" placeholder="Your WiFi Password">
  <button type="submit">儲存並連線</button>
</form>
<p class="note">儲存後裝置會重新啟動並連上指定的 WiFi。</p>
</body>
</html>
)rawhtml";
        _server->send(200, "text/html; charset=utf-8", html);
    });

    // ── 接收表單，儲存後重啟 ──────────────────────────────────
    _server->on("/save", HTTP_POST, []() {
        String ssid = _server->arg("ssid");
        String pass = _server->arg("pass");

        if (ssid.isEmpty()) {
            _server->send(400, "text/plain", "SSID 不能為空");
            return;
        }

        save_credentials(ssid, pass);

        String html = R"(
<!DOCTYPE html><html lang="zh-TW"><head><meta charset="UTF-8">
<style>body{font-family:sans-serif;max-width:400px;margin:40px auto;padding:16px;}</style>
</head><body>
<h2>✅ 設定完成</h2>
<p>裝置將在 3 秒後重新啟動並連上 WiFi。</p>
<p>連線成功後，請到您家用 WiFi 下查詢裝置 IP 即可使用。</p>
</body></html>
)";
        _server->send(200, "text/html; charset=utf-8", html);
        delay(3000);
        ESP.restart();
    });
}

// ── 嘗試連線到已儲存的 WiFi ──────────────────────────────────
static bool connect_sta(const String& ssid, const String& pass) {
    Serial.printf("[WiFi] Connecting to: %s\n", ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > WIFI_CONNECT_TIMEOUT_MS) {
            Serial.println("[WiFi] Connection timeout");
            return false;
        }
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.printf("[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    _is_ap_mode = false;
    return true;
}

// ── 公開 API ─────────────────────────────────────────────────
void wifi_manager_init(Preferences* prefs, WebServer* server) {
    _prefs  = prefs;
    _server = server;

    String ssid = load_ssid();
    String pass = load_pass();

    if (ssid.isEmpty()) {
        Serial.println("[WiFi] No credentials found -> AP Mode");
        start_ap_mode();
        return;
    }

    if (!connect_sta(ssid, pass)) {
        Serial.println("[WiFi] Failed to connect -> AP Mode");
        clear_credentials();
        start_ap_mode();
    }
}

void wifi_manager_loop() {
    // STA 模式下偵測斷線並嘗試重連
    if (!_is_ap_mode && WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Disconnected, retrying...");
        String ssid = load_ssid();
        String pass = load_pass();
        if (!ssid.isEmpty()) {
            connect_sta(ssid, pass);
        }
    }
}

bool wifi_manager_is_connected() {
    return !_is_ap_mode && WiFi.status() == WL_CONNECTED;
}

String wifi_manager_get_ip() {
    if (_is_ap_mode) return String(AP_IP);
    return WiFi.localIP().toString();
}
