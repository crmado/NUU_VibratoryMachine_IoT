#include "web_handler.h"
#include "dac_controller.h"
#include "opto_controller.h"
#include "mqtt_manager.h"
#include "wifi_manager.h"
#include "config_store.h"
#include "config.h"
#include <Arduino.h>
#include <ESP8266WiFi.h>

static ESP8266WebServer *_server = nullptr;

static const char CONTROL_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="zh-TW">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>振動機控制</title>
<style>
  *{box-sizing:border-box}
  body{font-family:sans-serif;background:#f0f4f8;margin:0;padding:20px}
  .card{background:#fff;border-radius:12px;padding:22px;max-width:440px;margin:auto;box-shadow:0 2px 12px rgba(0,0,0,.1)}
  h2{margin-top:0;color:#222}
  .row-status{display:flex;gap:8px;flex-wrap:wrap;margin-bottom:16px}
  .badge{display:inline-block;padding:4px 12px;border-radius:12px;font-size:13px;font-weight:bold}
  .running{background:#d4edda;color:#155724}
  .stopped{background:#f8d7da;color:#721c24}
  .mqtt-ok{background:#cce5ff;color:#004085}
  .mqtt-no{background:#fff3cd;color:#856404}
  label{display:block;margin:16px 0 6px;font-weight:bold}
  input[type=range]{width:100%;accent-color:#0078d4}
  .speed-val{text-align:center;font-size:32px;font-weight:bold;color:#0078d4;margin:4px 0}
  .btn-row{display:flex;gap:10px;margin-top:16px}
  button{flex:1;padding:12px;font-size:16px;border:none;border-radius:8px;cursor:pointer;font-weight:bold}
  .btn-run{background:#28a745;color:#fff}
  .btn-stop{background:#dc3545;color:#fff}
  .btn-run:hover{background:#218838}
  .btn-stop:hover{background:#b02a37}
  .btn-sm{font-size:13px;background:#6c757d;color:#fff;flex:0 0 auto;padding:8px 14px}
  .info{margin-top:16px;font-size:12px;color:#888;border-top:1px solid #eee;padding-top:12px;line-height:1.8}
</style>
</head>
<body>
<div class="card">
  <h2>&#9654; 振動機控制台</h2>
  <div class="row-status">
    <span id="runBadge" class="badge stopped">■ 停止</span>
    <span id="mqttBadge" class="badge mqtt-no">MQTT ...</span>
  </div>

  <label>轉速：<span id="pctDisplay">0</span>%
    <small style="color:#999;font-weight:normal">（0 = 最低 / 100 = 最高）</small>
  </label>
  <input type="range" id="slider" min="0" max="100" value="0"
         oninput="onSlide(this.value)">
  <div class="speed-val"><span id="rawVal">0</span>%</div>

  <div class="btn-row">
    <button class="btn-run"  onclick="sendRun(1)">&#9654; 啟動</button>
    <button class="btn-stop" onclick="sendRun(0)">&#9646;&#9646; 停止</button>
    <button class="btn-sm"   onclick="if(confirm('確定重設所有設定？'))location='/reset'">重設</button>
  </div>

  <div class="info" id="infoBar">載入中...</div>
</div>

<script>
let pendingSpeed = null, timer = null;

function onSlide(v) {
  document.getElementById('pctDisplay').textContent = v;
  document.getElementById('rawVal').textContent = v;
  clearTimeout(timer);
  timer = setTimeout(() => sendSpeed(v), 300);
}

function sendSpeed(v) {
  fetch('/api/speed?v=' + v).then(r => r.json()).then(updateUI);
}

function sendRun(v) {
  fetch('/api/run?v=' + v).then(r => r.json()).then(d => {
    updateUI(d);
    if (!v) {
      document.getElementById('slider').value = 0;
      onSlide(0);
    }
  });
}

function updateUI(d) {
  const run = d.run;
  const b = document.getElementById('runBadge');
  b.textContent = run ? '▶ 運行中' : '■ 停止';
  b.className = 'badge ' + (run ? 'running' : 'stopped');
}

function refreshStatus() {
  fetch('/api/status').then(r => r.json()).then(d => {
    document.getElementById('slider').value = d.speed;
    document.getElementById('pctDisplay').textContent = d.speed;
    document.getElementById('rawVal').textContent = d.speed;
    updateUI(d);
    const mb = document.getElementById('mqttBadge');
    mb.textContent = d.mqtt ? 'MQTT ✓' : 'MQTT 未連線';
    mb.className = 'badge ' + (d.mqtt ? 'mqtt-ok' : 'mqtt-no');
    document.getElementById('infoBar').innerHTML =
      'IP：' + d.ip + '&nbsp;&nbsp;WiFi：' + d.wifi +
      '<br>MQTT 訂閱主題：<code>vibratory/' + d.mac + '/cmd/run</code>' +
      '&nbsp;&nbsp;<code>vibratory/' + d.mac + '/cmd/speed</code>';
  });
}

refreshStatus();
setInterval(refreshStatus, 5000);
</script>
</body>
</html>
)rawhtml";

void web_handler_init(ESP8266WebServer *server)
{
  _server = server;

  _server->on("/", HTTP_GET, []()
              { _server->send_P(200, "text/html; charset=utf-8", CONTROL_PAGE); });

  // GET /api/run?v=1|0
  _server->on("/api/run", HTTP_GET, []()
              {
        bool run = _server->arg("v") == "1";
        opto_set_run(run);
        String json = "{\"run\":" + String(run ? "true" : "false") +
                      ",\"speed\":" + String(dac_get_percent()) + "}";
        _server->send(200, "application/json", json); });

  // GET /api/speed?v=0-100
  _server->on("/api/speed", HTTP_GET, []()
              {
        uint8_t spd = (uint8_t)constrain(_server->arg("v").toInt(), 0, 100);
        dac_set_percent(spd);
        String json = "{\"run\":" + String(opto_get_run() ? "true" : "false") +
                      ",\"speed\":" + String(spd) + "}";
        _server->send(200, "application/json", json); });

  // GET /api/status
  _server->on("/api/status", HTTP_GET, []()
              {
        String mac = WiFi.macAddress();
        mac.replace(":", "");
        mac = mac.substring(6);

        String json = "{\"run\":"    + String(opto_get_run() ? "true" : "false") +
                      ",\"speed\":"  + String(dac_get_percent()) +
                      ",\"mqtt\":"   + String(mqtt_manager_is_connected() ? "true" : "false") +
                      ",\"ip\":\""   + wifi_manager_get_ip() + "\"" +
                      ",\"wifi\":\"" + WiFi.SSID() + "\"" +
                      ",\"mac\":\""  + mac + "\"}";
        _server->send(200, "application/json", json); });

  // GET /reset — 清除所有設定，重啟進 AP 模式
  _server->on("/reset", HTTP_GET, []()
              {
        _server->send(200, "text/html; charset=utf-8",
            "<html><head><meta charset='UTF-8'></head>"
            "<body style='font-family:sans-serif;padding:24px'>"
            "<h2>重設中...</h2><p>請重新連線熱點 <b>" AP_SSID "</b></p>"
            "</body></html>");
        delay(1000);
        config_store_clear();
        ESP.restart(); });
}
