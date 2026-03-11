#include "opto_controller.h"
#include "config.h"

static bool _running = false;

void opto_init() {
    pinMode(OPTO_RUN_PIN, OUTPUT);
    // 初始化為停止狀態（GPIO HIGH → PC817 導通 → SDVC31 C2 收到訊號 → 停止）
    digitalWrite(OPTO_RUN_PIN, HIGH);
    Serial.printf("[OPTO] PC817 初始化  GPIO%d  預設: 停止\n", OPTO_RUN_PIN);
}

void opto_set_run(bool run) {
    _running = run;
    // run=true  → GPIO LOW  → PC817 不導通 → C2 無訊號 → SDVC31 運行
    // run=false → GPIO HIGH → PC817 導通   → C2 有訊號 → SDVC31 停止
    digitalWrite(OPTO_RUN_PIN, run ? LOW : HIGH);
    Serial.printf("[OPTO] 振動機 %s\n", run ? "▶ 運行" : "■ 停止");
}

bool opto_get_run() {
    return _running;
}
