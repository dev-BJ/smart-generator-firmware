#include <Arduino.h>
#include "battery_monitor.h"

static bool charging = false;

float readBatteryVoltage() {
  int adc = analogRead(PIN_VSENSE_BATT);
  return (adc * ADC_MAX_VOLTAGE / ADC_MAX_COUNT) * BATT_VOLTAGE_DIVIDER_RATIO;
}

void updateBatteryCharging() {
  float v = readBatteryVoltage();
  if (v < BATT_CHARGE_START_V && !charging) {
    charging = true;
    digitalWrite(PIN_RELAY_BATT, RELAY_ON);
  } else if (v >= BATT_CHARGE_STOP_V && charging) {
    charging = false;
    digitalWrite(PIN_RELAY_BATT, RELAY_OFF);
  }
}

bool isCharging() { return charging; }