#include "mqtt_manager.h"
#include "config_store.h"
#include "config.h"
#include "dac_controller.h"
#include "opto_controller.h"
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

static BearSSL::WiFiClientSecure *_wifi_client = nullptr;
static PubSubClient *_mqtt = nullptr;

static String _host;
static int _port = MQTT_DEFAULT_PORT;
static String _user;
static String _pass;

static unsigned long _last_reconnect = 0;
static unsigned long _last_heartbeat = 0;
#define HEARTBEAT_MS 30000 // 每 30 秒發送一次心跳

// ── Topic helper ────────────────────────────────────────────────
static String topic(const char *suffix)
{
    // 以 MAC 後 6 碼作為裝置唯一識別
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    mac = mac.substring(6); // 取後 6 碼
    return String(MQTT_TOPIC_PREFIX) + "/" + mac + "/" + suffix;
}

// ── 發布目前狀態 ─────────────────────────────────────────────────
static void publish_status()
{
    if (!_mqtt)
        return;
    float voltage = dac_get_voltage();
    char vbuf[8];
    dtostrf(voltage, 4, 2, vbuf); // ESP8266 printf %f 不穩，用 dtostrf
    String payload = "{\"run\":" + String(opto_get_run() ? "true" : "false") +
                     ",\"speed\":" + String(dac_get_percent()) +
                     ",\"voltage\":" + String(vbuf) +
                     ",\"raw\":" + String(dac_get_raw()) +
                     ",\"dac_ok\":" + String(dac_is_ok() ? "true" : "false") + "}";
    _mqtt->publish(topic("status").c_str(), payload.c_str(), true);
}

// ── 收到 MQTT 訊息 ───────────────────────────────────────────────
static void on_message(char *top, byte *payload, unsigned int len)
{
    String t(top);
    // ESP8266 WString 不支援 String(char*, len)，手動加結尾字元
    char buf[len + 1];
    memcpy(buf, payload, len);
    buf[len] = '\0';
    String msg(buf);
    msg.trim();
    Serial.printf("[MQTT] ← %s : %s\n", top, msg.c_str());

    if (t == topic("cmd/run"))
    {
        bool run = (msg == "1" || msg == "true" || msg == "on");
        opto_set_run(run);
        publish_status();
    }
    else if (t == topic("cmd/speed"))
    {
        uint8_t spd = (uint8_t)constrain(msg.toInt(), 0, 100);
        dac_set_percent(spd);
        publish_status();
    }
}

// ── 連線 / 重連 ──────────────────────────────────────────────────
static bool do_connect()
{
    if (!_mqtt)
        return false;
    String clientId = "VibMachine-" + WiFi.macAddress();
    bool ok = _user.isEmpty()
                  ? _mqtt->connect(clientId.c_str())
                  : _mqtt->connect(clientId.c_str(), _user.c_str(), _pass.c_str());

    if (ok)
    {
        Serial.printf("[MQTT] 已連線  broker=%s:%d\n", _host.c_str(), _port);
        _mqtt->subscribe(topic("cmd/run").c_str());
        _mqtt->subscribe(topic("cmd/speed").c_str());
        // 連線成功立刻發送 online 訊息
        String onlineMsg = "{\"status\":\"online\",\"ip\":\"" + WiFi.localIP().toString() + "\",\"rssi\":" + String(WiFi.RSSI()) + "}";
        _mqtt->publish(topic("online").c_str(), onlineMsg.c_str(), true);
        _last_heartbeat = millis();
        publish_status();
    }
    else
    {
        Serial.printf("[MQTT] 連線失敗 rc=%d\n", _mqtt->state());
    }
    return ok;
}

// ── 公開 API ─────────────────────────────────────────────────────
void mqtt_manager_init()
{
    AppConfig c = config_store_load();
    if (config_store_valid(c) && strlen(c.mqtt_host) > 0)
    {
        _host = String(c.mqtt_host);
        _port = c.mqtt_port ? c.mqtt_port : MQTT_DEFAULT_PORT;
        _user = String(c.mqtt_user);
        _pass = String(c.mqtt_pass);
    }

    if (_host.isEmpty())
        _host = MQTT_DEFAULT_HOST;
    if (_port == 0)
        _port = MQTT_DEFAULT_PORT;
    if (_user.isEmpty())
        _user = MQTT_DEFAULT_USER;
    if (_pass.isEmpty())
        _pass = MQTT_DEFAULT_PASS;

    // 惰性初始化：在 setup() 完成後才建立物件
    if (!_wifi_client)
        _wifi_client = new BearSSL::WiFiClientSecure();
    if (!_mqtt)
        _mqtt = new PubSubClient(*_wifi_client);

    _wifi_client->setInsecure(); // 跳過憑證驗證（TLS 加密但不驗 CA）
    _mqtt->setBufferSize(512);   // 預設 128 bytes 不夠，含長 topic 需要更大緩衝
    _mqtt->setServer(_host.c_str(), _port);
    _mqtt->setCallback(on_message);
    Serial.printf("[MQTT] Broker=%s:%d  user=%s\n", _host.c_str(), _port, _user.c_str());
    do_connect();
}

void mqtt_manager_loop()
{
    if (_host.isEmpty() || !_mqtt)
        return;
    if (!_mqtt->connected())
    {
        unsigned long now = millis();
        if (now - _last_reconnect > MQTT_RECONNECT_MS)
        {
            _last_reconnect = now;
            do_connect();
        }
    }
    else
    {
        _mqtt->loop();
        // 每 30 秒發送一次心跳（含電壓 + 狀態）
        unsigned long now = millis();
        if (now - _last_heartbeat > HEARTBEAT_MS)
        {
            _last_heartbeat = now;
            publish_status();
        }
    }
}

bool mqtt_manager_is_connected()
{
    return _mqtt && _mqtt->connected();
}
