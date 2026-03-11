#include "web_handler.h"
#include "pwm_controller.h"
#include "wifi_manager.h"
#include "config.h"
#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>

static WebServer* _server = nullptr;

// ── 控制頁面 HTML ─────────────────────────────────────────────
static const char CONTROL_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="zh-TW">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>振動機控制</title>
<style>
  * { box-sizing: border-box; }
  body { font-family: sans-serif; background: #f0f4f8; margin: 0; padding: 20px; }
  .card { background: #fff; border-radius: 12px; padding: 24px; max-width: 440px; margin: auto; box-shadow: 0 2px 12px rgba(0,0,0,.1); }
  h2 { margin-top: 0; color: #222; }
  .badge { display: inline-block; padding: 3px 10px; border-radius: 12px; font-size: 13px; font-weight: bold; }
  .on  { background: #d4edda; color: #155724; }
  .off { background: #f8d7da; color: #721c24; }
  label { display: block; margin: 20px 0 6px; font-weight: bold; }
  input[type=range] { width: 100%; accent-color: #0078d4; }
  .duty-val { text-align: center; font-size: 28px; font-weight: bold; color: #0078d4; }
  .btn-row { display: flex; gap: 10px; margin-top: 20px; }
  button { flex: 1; padding: 12px; font-size: 16px; border: none; border-radius: 8px; cursor: pointer; }
  .btn-start { background: #0078d4; color: #fff; }
  .btn-stop  { background: #dc3545; color: #fff; }
  .btn-reset { background: #6c757d; color: #fff; font-size: 13px; }
  .btn-start:hover { background: #005fa3; }
  .btn-stop:hover  { background: #b02a37; }
  .info { margin-top: 16px; font-size: 13px; color: #666; border-top: 1px solid #eee; padding-top: 12px; }
</style>
</head>
<body>
<div class="card">
  <h2>&#128246; 振動機控制</h2>
  <p>狀態：<span id="status" class="badge off">停止</span></p>

  <label>PWM 強度：<span id="dutyDisplay">0</span>%</label>
  <input type="range" id="dutySlider" min="0" max="255" value="0"
         oninput="updateDisplay(this.value)">
  <div class="duty-val"><span id="dutyRaw">0</span> / 255</div>

  <div class="btn-row">
    <button class="btn-start" onclick="setDuty(document.getElementById('dutySlider').value)">啟動</button>
    <button class="btn-stop"  onclick="stopVib()">停止</button>
  </div>

  <div class="btn-row">
    <button class="btn-reset" onclick="if(confirm('確定重新設定 WiFi？'))location='/reset'">重設 WiFi</button>
  </div>

  <div class="info" id="info">IP：載入中...</div>
</div>

<script>
function updateDisplay(v) {
  document.getElementById('dutyDisplay').textContent = Math.round(v/255*100);
  document.getElementById('dutyRaw').textContent = v;
}

function setDuty(v) {
  fetch('/pwm?duty=' + v)
    .then(r => r.json())
    .then(d => {
      document.getElementById('status').textContent = d.duty > 0 ? '運行中' : '停止';
      document.getElementById('status').className = 'badge ' + (d.duty > 0 ? 'on' : 'off');
    });
}

function stopVib() {
  document.getElementById('dutySlider').value = 0;
  updateDisplay(0);
  setDuty(0);
}

// 載入目前狀態
fetch('/status').then(r=>r.json()).then(d=>{
  document.getElementById('dutySlider').value = d.duty;
  updateDisplay(d.duty);
  document.getElementById('status').textContent = d.duty > 0 ? '運行中' : '停止';
  document.getElementById('status').className = 'badge ' + (d.duty > 0 ? 'on' : 'off');
  document.getElementById('info').textContent = 'IP：' + d.ip + '  |  WiFi：' + d.wifi;
});
</script>
</body>
</html>
)rawhtml";

void web_handler_init(WebServer* server) {
    _server = server;

    // ── 主控制頁 ────────────────────────────────────────────
    _server->on("/", HTTP_GET, []() {
        _server->send_P(200, "text/html; charset=utf-8", CONTROL_PAGE);
    });

    // ── PWM 設定 API ─────────────────────────────────────────
    // GET /pwm?duty=128
    _server->on("/pwm", HTTP_GET, []() {
        if (!_server->hasArg("duty")) {
            _server->send(400, "application/json", "{\"error\":\"missing duty\"}");
            return;
        }
        int duty = constrain(_server->arg("duty").toInt(), 0, 255);
        pwm_set_duty((uint8_t)duty);

        String json = "{\"duty\":" + String(duty) + "}";
        _server->send(200, "application/json", json);
    });

    // ── 狀態 API ─────────────────────────────────────────────
    // GET /status
    _server->on("/status", HTTP_GET, []() {
        uint8_t duty = pwm_get_duty();
        String ip    = wifi_manager_get_ip();
        String ssid  = WiFi.SSID();
        if (ssid.isEmpty()) ssid = "AP Mode";

        String json = "{\"duty\":" + String(duty) +
                      ",\"ip\":\"" + ip + "\"" +
                      ",\"wifi\":\"" + ssid + "\"}";
        _server->send(200, "application/json", json);
    });

    // ── 重設 WiFi ─────────────────────────────────────────────
    // GET /reset
    _server->on("/reset", HTTP_GET, []() {
        _server->send(200, "text/html; charset=utf-8",
            "<html><body><h2>重設中，請重新連線熱點 " AP_SSID "。</h2></body></html>");
        delay(1000);
        Preferences prefs;
        prefs.begin("wifi_cfg", false);
        prefs.clear();
        prefs.end();
        ESP.restart();
    });
}
