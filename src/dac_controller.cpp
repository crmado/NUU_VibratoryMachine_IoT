#include "dac_controller.h"
#include "config.h"
#include <Wire.h>
#include <MCP4725.h> // robtillaart/MCP4725

static MCP4725 _dac(DAC_I2C_ADDR);
static uint8_t _percent = 0;

void dac_init()
{
    Wire.begin(DAC_SDA_PIN, DAC_SCL_PIN);
    bool ok = _dac.begin();
    if (!ok)
    {
        Serial.println("[DAC] MCP4725 未找到！請確認接線與 I2C 地址");
        return;
    }
    _dac.setValue(0);
    Serial.printf("[DAC] MCP4725 初始化成功  SDA=GPIO%d  SCL=GPIO%d\n",
                  DAC_SDA_PIN, DAC_SCL_PIN);
}

void dac_set_percent(uint8_t percent)
{
    _percent = constrain(percent, 0, 100);
    uint16_t raw = (uint16_t)(_percent * DAC_MAX_VALUE / 100);
    _dac.setValue(raw);
    Serial.printf("[DAC] 轉速 %d%% → raw=%d (約 %.2fV)\n",
                  _percent, raw, raw * 5.0f / DAC_MAX_VALUE);
}

uint8_t dac_get_percent()
{
    return _percent;
}

float dac_get_voltage()
{
    return _percent * 5.0f / 100.0f;
}
