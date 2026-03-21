#include "dac_controller.h"
#include "config.h"
#include <Wire.h>
#include <MCP4725.h> // robtillaart/MCP4725

static MCP4725 _dac(DAC_I2C_ADDR);
static uint8_t _percent = 0;
static bool _dac_ok = false; // begin() 成功才為 true

void dac_init()
{
    Wire.begin(DAC_SDA_PIN, DAC_SCL_PIN);

    // 掃描 I2C 匯流排，印出所有找到的裝置位址（幫助確認 MCP4725 實際位址）
    Serial.println("[DAC] 掃描 I2C 裝置...");
    for (byte addr = 1; addr < 127; addr++)
    {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0)
        {
            Serial.printf("[DAC] 找到 I2C 裝置 @ 0x%02X\n", addr);
        }
    }

    bool ok = _dac.begin();
    if (!ok)
    {
        Serial.printf("[DAC] MCP4725 0x%02X 未回應！\n"
                      "      A0=GND → 0x60 ; A0=VCC → 0x61\n"
                      "      請修改 config.h 的 DAC_I2C_ADDR\n",
                      DAC_I2C_ADDR);
        _dac_ok = false;
        return;
    }
    _dac_ok = true;
    _dac.setValue(0);
    Serial.printf("[DAC] MCP4725 初始化成功  addr=0x%02X  SDA=GPIO%d  SCL=GPIO%d\n",
                  DAC_I2C_ADDR, DAC_SDA_PIN, DAC_SCL_PIN);
}

void dac_set_percent(uint8_t percent)
{
    _percent = constrain(percent, 0, 100);
    uint16_t raw = (uint16_t)(_percent * DAC_MAX_VALUE / 100);
    if (_dac_ok)
    {
        _dac.setValue(raw);
        Serial.printf("[DAC] 轉速 %d%% → raw=%d (約 %.2fV) 寫入成功\n",
                      _percent, raw, raw * 5.0f / DAC_MAX_VALUE);
    }
    else
    {
        Serial.printf("[DAC] 警告：MCP4725 未初始化，忽略指令 %d%%\n", _percent);
    }
}

uint8_t dac_get_percent()
{
    return _percent;
}

uint16_t dac_get_raw()
{
    return (uint16_t)(_percent * DAC_MAX_VALUE / 100);
}

float dac_get_voltage()
{
    return _percent * 5.0f / 100.0f;
}

bool dac_is_ok()
{
    return _dac_ok;
}
