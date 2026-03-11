#include "mqtt_manager.h"
#include "config.h"
#include "dac_controller.h"
#include "opto_controller.h"
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

static Preferences* _prefs = nullptr;
static WiFiClient   _wifi_client;
static PubSubClient _mqtt(_wifi_client);

static String _host;
static int    _port = MQTT_DEFAULT_PORT;
static String _user;
static String _pass;

static unsigned long _last_reconnect = 0;

// ── Topic helper ────────────────────────────────────────────────
static String topic(const char* suffix) {
    // 以 MAC 後 6 碼作為裝置唯一識別
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    mac = mac.substring(6);  // 取後 6 碼
    return String(MQTT_TOPIC_PREFIX) + "/" + mac + "/" + suffix;
}

// ── 發布目前狀態 ─────────────────────────────────────────────────
static void publish_status() {
    String payload = "{\"run\":"   + String(opto_get_run() ? "true" : "false") +
                     ",\"speed\":" + String(dac_get_percent()) + "}";
    _mqtt.publish(topic("status").c_str(), payload.c_str(), true);
}

// ── 收到 MQTT 訊息 ───────────────────────────────────────────────
static void on_message(char* top, byte* payload, unsigned int len) {
    String t(top);
    String msg((char*)payload, len);
    msg.trim();
    Serial.printf("[MQTT] ← %s : %s\n", top, msg.c_str());

    if (t == topic("cmd/run")) {
        bool run = (msg == "1" || msg == "true" || msg == "on");
        opto_set_run(run);
        publish_status();
    } else if (t == topic("cmd/speed")) {
        uint8_t spd = (uint8_t)constrain(msg.toInt(), 0, 100);
        dac_set_percent(spd);
        publish_status();
    }
}

// ── 連線 / 重連 ──────────────────────────────────────────────────
static bool do_connect() {
    String clientId = "VibMachine-" + WiFi.macAddress();
    bool ok = _user.isEmpty()
        ? _mqtt.connect(clientId.c_str())
        : _mqtt.connect(clientId.c_str(), _user.c_str(), _pass.c_str());

    if (ok) {
        Serial.printf("[MQTT] 已連線  broker=%s:%d\n", _host.c_str(), _port);
        _mqtt.subscribe(topic("cmd/run").c_str());
        _mqtt.subscribe(topic("cmd/speed").c_str());
        publish_status();
    } else {
        Serial.printf("[MQTT] 連線失敗 rc=%d\n", _mqtt.state());
    }
    return ok;
}

// ── 公開 API ─────────────────────────────────────────────────────
void mqtt_manager_init(Preferences* prefs) {
    _prefs = prefs;
    _prefs->begin(NVS_NAMESPACE, true);
    _host = _prefs->getString(NVS_KEY_MQTT_HOST, "");
    _port = _prefs->getInt   (NVS_KEY_MQTT_PORT, MQTT_DEFAULT_PORT);
    _user = _prefs->getString(NVS_KEY_MQTT_USER, "");
    _pass = _prefs->getString(NVS_KEY_MQTT_PASS, "");
    _prefs->end();

    if (_host.isEmpty()) {
        Serial.println("[MQTT] 未設定 Broker，跳過");
        return;
    }

    _mqtt.setServer(_host.c_str(), _port);
    _mqtt.setCallback(on_message);
    do_connect();
}

void mqtt_manager_loop() {
    if (_host.isEmpty()) return;
    if (!_mqtt.connected()) {
        unsigned long now = millis();
        if (now - _last_reconnect > MQTT_RECONNECT_MS) {
            _last_reconnect = now;
            do_connect();
        }
    } else {
        _mqtt.loop();
    }
}

bool mqtt_manager_is_connected() {
    return _mqtt.connected();
}
