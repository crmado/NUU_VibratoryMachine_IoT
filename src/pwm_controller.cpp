#include "pwm_controller.h"
#include "config.h"

static uint8_t _current_duty = 0;

void pwm_init() {
    // ESP32 LEDC PWM 設定
    ledcSetup(VIBRATOR_CHANNEL, VIBRATOR_FREQ_HZ, VIBRATOR_RESOLUTION);
    ledcAttachPin(VIBRATOR_PIN, VIBRATOR_CHANNEL);
    ledcWrite(VIBRATOR_CHANNEL, 0);   // 初始關閉
    Serial.printf("[PWM] Init on GPIO %d, freq %dHz, %d-bit\n",
                  VIBRATOR_PIN, VIBRATOR_FREQ_HZ, VIBRATOR_RESOLUTION);
}

void pwm_set_duty(uint8_t duty) {
    _current_duty = duty;
    ledcWrite(VIBRATOR_CHANNEL, duty);
    Serial.printf("[PWM] Duty set to %d/255 (%d%%)\n", duty, (int)(duty * 100 / 255));
}

uint8_t pwm_get_duty() {
    return _current_duty;
}
