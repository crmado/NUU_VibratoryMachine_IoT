#include "config_store.h"
#include <EEPROM.h>

#define EEPROM_SIZE 512 // 大於 sizeof(AppConfig) = ~262 bytes

void config_store_init()
{
    EEPROM.begin(EEPROM_SIZE);
}

AppConfig config_store_load()
{
    AppConfig c;
    EEPROM.get(0, c);
    return c;
}

void config_store_save(const AppConfig &c)
{
    EEPROM.put(0, c);
    EEPROM.commit();
}

void config_store_clear()
{
    AppConfig c;
    memset(&c, 0, sizeof(c));
    EEPROM.put(0, c);
    EEPROM.commit();
    Serial.println("[CFG] 所有設定已清除");
}

bool config_store_valid(const AppConfig &c)
{
    return c.magic == CONFIG_MAGIC;
}
