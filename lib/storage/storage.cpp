#include <Arduino.h>
#include <Preferences.h>
#include <SPI.h>
#include <SD.h>
#include <RTClib.h>
#include <ArduinoJson.h>
#include "config.h"
#include "storage.h"
#include "types.h"
#include "fuel_monitor.h"
#include "sdcard.h"
#include "gsm_handler.h"

static Preferences prefs;

void initStorage() {
  prefs.begin("genctrl", false);
  prefs.end();
  SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
  if (!sdcard_init(PIN_SD_CS)) {
    // SD init failed, continue without logging
  }

  if (!sdcard_exists(FILE_GEN_STAT))
  {
    File file = sdcard_open(FILE_GEN_STAT, FILE_WRITE);
    if (file)
      {
        file.println("phone_number,gen_runtime_daily,gen_runtime_total,fuel_consumed_daily,fuel_consumed_total,last_day,gen_voltage,mains_voltage,date_time");
        file.close();
        Serial.println("Created gen_stat.csv");
      }
  }
}

void clearStorage() {
  prefs.begin("genctrl", false);
  prefs.clear();
  prefs.end();

  sdcard_remove(FILE_GEN_STAT);
}

void loadPersistentData(String& registeredPhone, unsigned long& genRuntimeDaily, unsigned long& genRuntimeTotal, float& fuelConsumedDaily, float& fuelConsumedTotal, int& lastDay) {
  prefs.begin("genctrl", true);
  registeredPhone = prefs.getString(NVS_KEY_PHONE_NUM, DEFAULT_ADMIN_NUMBER);
  genRuntimeDaily = prefs.getULong(NVS_KEY_GEN_RUNTIME_DAILY, 0);
  genRuntimeTotal = prefs.getULong(NVS_KEY_GEN_RUNTIME_TOTAL, 0);
  fuelConsumedDaily = prefs.getFloat(NVS_KEY_FUEL_DAILY, 0.0f);
  fuelConsumedTotal = prefs.getFloat(NVS_KEY_FUEL_TOTAL, 0.0f);
  lastDay = prefs.getInt(NVS_KEY_LAST_DATE, -1);
  prefs.end();
}

void savePersistentData(String registeredPhone, unsigned long genRuntimeDaily, unsigned long genRuntimeTotal, float fuelConsumedDaily, float fuelConsumedTotal, int lastDay, float genVoltage, float mainsVoltage) {
  prefs.begin("genctrl", false);
  prefs.putString(NVS_KEY_PHONE_NUM, registeredPhone);
  prefs.putULong(NVS_KEY_GEN_RUNTIME_DAILY, genRuntimeDaily);
  prefs.putULong(NVS_KEY_GEN_RUNTIME_TOTAL, genRuntimeTotal);
  prefs.putFloat(NVS_KEY_FUEL_DAILY, fuelConsumedDaily);
  prefs.putFloat(NVS_KEY_FUEL_TOTAL, fuelConsumedTotal);
  prefs.putInt(NVS_KEY_LAST_DATE, lastDay);
  prefs.end();

  extern String getDateTimeStr();
  File file = sdcard_open(FILE_GEN_STAT, FILE_APPEND);
  if (file)
  {
    file.print(registeredPhone);
    file.print(",");
    file.print(genRuntimeDaily);
    file.print(",");
    file.print(genRuntimeTotal);
    file.print(",");
    file.println(fuelConsumedDaily);
    file.print(",");
    file.print(fuelConsumedTotal);
    file.print(",");
    file.print(lastDay);
    file.print(",");
    file.println(String(genVoltage, 2));
    file.print(",");
    file.println(String(mainsVoltage, 2));
    file.print(",");
    file.println(getDateTimeStr());
    file.close();

    Serial.println("Gen Stat recorded");
    // return true;
  }

  extern float getFuelPercent();
  extern float getFuelLiters();
  extern unsigned long genRuntimeDaily;
  JsonDocument doc;
  doc["device_id"] = DEVICE_ID;
  doc["generator_voltage"] = genVoltage;
  doc["mains_voltage"] = mainsVoltage;
  doc["fuel_percent"] = getFuelPercent();
  doc["fuel_used_liters"] = (FUEL_TANK_CAPACITY_L - getFuelLiters());
  doc["generator_on_seconds"] = genRuntimeDaily;
  doc["record_at"] = getDateTimeStr();

  String output;
  serializeJson(doc, output);
  sendPostRequest(output);
}

void checkDayRollover(int& lastDay, unsigned long& genRuntimeDaily, float& fuelConsumedDaily) {
  extern RTC_DS3231 rtc;
  DateTime now = rtc.now();
  int today = now.day();
  if (lastDay != -1 && today != lastDay) {
    genRuntimeDaily = 0;
    fuelConsumedDaily = 0.0f;
    lastDay = today;
  }
  if (lastDay == -1) lastDay = today;
}

void updateRuntimeStats(unsigned long& genRuntimeDaily, unsigned long& genRuntimeTotal, float& fuelConsumedDaily, float& fuelConsumedTotal, float lastFuelLiters, GenState genState) {
  extern unsigned long genStartTime;
  extern float getFuelLiters();

  if (genState == GEN_RUNNING && genStartTime > 0) {
    unsigned long elapsed = (millis() - genStartTime) / 1000;
    genRuntimeDaily += elapsed;
    genRuntimeTotal += elapsed;
    float fuelDiff = lastFuelLiters - getFuelLiters();
    if (fuelDiff > 0.1f && fuelDiff < 5.0f) {
      fuelConsumedDaily += fuelDiff;
      fuelConsumedTotal += fuelDiff;
    }
  }
}