#include <Arduino.h>
#include "fuel_monitor.h"

static float fuelSamples[FUEL_FILTER_SAMPLES];
static int fuelSampleIdx = 0;
static int fuelSampleCount = 0;
static float currentFuelPercent = 0.0f;
static float currentFuelLiters = 0.0f;
static bool fuelLowAlertSent = false;
static bool fuelCriticalAlertSent = false;

float measureDistance() {
  digitalWrite(PIN_ULTRA_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_ULTRA_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_ULTRA_TRIG, LOW);
  long duration = pulseIn(PIN_ULTRA_ECHO, HIGH);
  if (duration == 0) return -1.0f;
  return duration * 0.034f / 2.0f;
}

float calcFuelPercent(float distCm) {
  if (distCm < 0) return -1.0f;
  float range = FUEL_EMPTY_DIST_CM - FUEL_FULL_DIST_CM;
  if (range <= 0) return -1.0f;
  float pct = (FUEL_EMPTY_DIST_CM - distCm) * 100.0f / range;
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  return pct;
}

float filterFuel(float newVal) {
  fuelSamples[fuelSampleIdx] = newVal;
  fuelSampleIdx = (fuelSampleIdx + 1) % FUEL_FILTER_SAMPLES;
  if (fuelSampleCount < FUEL_FILTER_SAMPLES) fuelSampleCount++;
  float sum = 0;
  for (int i = 0; i < fuelSampleCount; i++) sum += fuelSamples[i];
  return sum / fuelSampleCount;
}

void updateFuelPercent() {
  float dist = measureDistance();
  Serial.print("Measured distance: ");
  Serial.print(dist);
  Serial.println(" cm");
  if (dist >= 0) {
    float rawPct = calcFuelPercent(dist);
    if (rawPct >= 0) {
      currentFuelPercent = filterFuel(rawPct);
      currentFuelLiters = currentFuelPercent * FUEL_TANK_CAPACITY_L / 100.0f;
    }
  }
}

void updateFuelAlerts(String registeredPhone) {
  extern GenState genState;
  extern bool mainsPresent;
  extern bool genVoltagePresent;
  extern float mainsVoltage;
  extern float genVoltage;
  extern void sendSms(String phone, String msg);
  extern void beepBuzzer(int count, int duration);
  extern String getDateTimeStr();

  if (currentFuelPercent <= FUEL_SHUTDOWN_THRESHOLD_PCT && genState == GEN_RUNNING) {
    genState = GEN_STOPPING;
    sendSms(registeredPhone, "Gen: " + String(genVoltage, 0) + "V Mains: " + String(mainsVoltage, 0) + "V Date/Time: " + getDateTimeStr() + " Fuel level: " + String((int)currentFuelPercent) + "% Generator stopped due to low fuel.");
    return;
  }

  if (currentFuelPercent <= FUEL_CRITICAL_THRESHOLD_PCT && !fuelCriticalAlertSent) {
    sendSms(registeredPhone, "Gen: " + String(genVoltage, 0) + "V Mains: " + String(mainsVoltage, 0) + "V Date/Time: " + getDateTimeStr() + " Fuel level: " + String((int)currentFuelPercent) + "% Critical fuel level! Please refill the tank.");
    fuelCriticalAlertSent = true;
    digitalWrite(PIN_LED_LOW_FUEL, HIGH);
    beepBuzzer(3, 200);
  } else if (currentFuelPercent > FUEL_CRITICAL_THRESHOLD_PCT) {
    fuelCriticalAlertSent = false;
    digitalWrite(PIN_LED_LOW_FUEL, LOW);
  }

  if (currentFuelPercent <= FUEL_LOW_THRESHOLD_PCT && !fuelLowAlertSent) {
    sendSms(registeredPhone, "Gen: " + String(genVoltage, 0) + "V Mains: " + String(mainsVoltage, 0) + "V Date/Time: " + getDateTimeStr() + " Fuel level: " + String((int)currentFuelPercent) + "% Low fuel level! Please refill the tank.");
    fuelLowAlertSent = true;
    digitalWrite(PIN_LED_LOW_FUEL, HIGH);
    beepBuzzer(2, 500);
  } else if (currentFuelPercent > FUEL_LOW_THRESHOLD_PCT) {
    fuelLowAlertSent = false;
    if (!fuelCriticalAlertSent) digitalWrite(PIN_LED_LOW_FUEL, LOW);
  }
}

float getFuelPercent() { return currentFuelPercent; }
float getFuelLiters() { return currentFuelLiters; }

void resetFuelAlerts() {
  fuelLowAlertSent = false;
  fuelCriticalAlertSent = false;
  digitalWrite(PIN_LED_LOW_FUEL, LOW);
}