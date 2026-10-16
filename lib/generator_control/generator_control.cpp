#include <Arduino.h>
#include <ESP32Servo.h>
#include "config.h"
#include "generator_control.h"
#include "fuel_monitor.h"

static GenState genState = GEN_OFF;
static Servo chokeServo;
static unsigned long chokeStartTime = 0;
static unsigned long genVoltageStableStart = 0;
static unsigned long genStopTime = 0;
static unsigned long genStartTime = 0;
static float lastFuelLiters = 0;
static bool isChoked = false;

extern void sendSms(String phone, String msg);
extern String getDateTimeStr();
extern float getFuelLiters();
extern float getFuelPercent();

void initGeneratorControl() {
  pinMode(PIN_RELAY_GEN, OUTPUT);
  digitalWrite(PIN_RELAY_GEN, RELAY_OFF);
  chokeServo.attach(PIN_SERVO);
  chokeServo.write(SERVO_UNCHOKE_POS);
}

void updateGeneratorState(GenState newState) {
  genState = newState;
  if (genState == GEN_OFF) {
    chokeServo.write(SERVO_UNCHOKE_POS);
    isChoked = false;
  }
}

void updateGeneratorControl(GenState* state, bool mainsPresent, bool genVoltagePresent, String registeredPhone, float mainsVoltage, float genVoltage) {
  *state = genState;

  switch (genState) {
    case GEN_OFF:
      digitalWrite(PIN_RELAY_GEN, RELAY_OFF);
      if (isChoked) {
        chokeServo.write(SERVO_UNCHOKE_POS);
        isChoked = false;
      }
      break;

    case GEN_STARTING:
      // if (mainsPresent) return;
      digitalWrite(PIN_RELAY_GEN, RELAY_ON);

      if (millis() - chokeStartTime < CHOKE_HOLD_MS) {
        isChoked = true;
        chokeServo.write(SERVO_CHOKE_POS);
      } else {
        isChoked = false;
        chokeServo.write(SERVO_UNCHOKE_POS);
      }

      if (genVoltagePresent) {
        if (genVoltageStableStart == 0) {
          genVoltageStableStart = millis();
        } else if (millis() - genVoltageStableStart >= GEN_VOLTAGE_STABLE_MS) {
          genState = GEN_RUNNING;
          digitalWrite(PIN_RELAY_LOAD, RELAY_ON);
          genStartTime = millis();
          lastFuelLiters = getFuelLiters();
          sendSms(registeredPhone, "Gen: " + String(genVoltage, 0) + "V Mains: " + String(mainsVoltage, 0) + "V Date/Time: " + getDateTimeStr() + " Fuel level: " + String((int)getFuelPercent()) + "%");
        }
      } else {
        genVoltageStableStart = 0;
      }

      if (genVoltagePresent && genState == GEN_STARTING) {
        genState = GEN_RUNNING;
        digitalWrite(PIN_RELAY_LOAD, RELAY_ON);
        genStartTime = millis();
        lastFuelLiters = getFuelLiters();
      } else if (!genVoltagePresent && genState == GEN_STARTING && millis() - chokeStartTime > GEN_STARTUP_TIMEOUT_MS) {
        genState = GEN_STOPPING;
        genStopTime = millis();
      }

      // if (millis() - chokeStartTime > GEN_STARTUP_TIMEOUT_MS) {
      //   genState = GEN_OFF;
      //   sendSms(registeredPhone, "Generator failed to start");
      // }
      break;

    case GEN_RUNNING:
      // digitalWrite(PIN_RELAY_GEN, RELAY_OFF);
      if(isChoked) {
        chokeServo.write(SERVO_UNCHOKE_POS);
        isChoked = false;
      }

      if (!genVoltagePresent) {
        genState = GEN_STOPPING;
        genStopTime = millis();
      }
      break;

    case GEN_STOPPING:
      digitalWrite(PIN_RELAY_GEN, RELAY_OFF);
      digitalWrite(PIN_RELAY_LOAD, RELAY_OFF);
      if(isChoked) {
        chokeServo.write(SERVO_UNCHOKE_POS);
        isChoked = false;
      }

      if (millis() - genStopTime > GEN_STOP_DELAY_MS) {
        if (!genVoltagePresent) {
          genState = GEN_OFF;
          genStartTime = 0;
          sendSms(registeredPhone, "Gen: " + String(genVoltage, 0) + "V Mains: " + String(mainsVoltage, 0) + "V Date/Time: " + getDateTimeStr() + " Fuel level: " + String((int)getFuelPercent()) + "% \r\n Generator was stopped because there is no power from the generator.");
        } else {
          genStopTime = millis();
        }
      }
      break;
  }
}

GenState getGenState() { return genState; }

void startGenerator() {
  if (genState == GEN_OFF) {
    genState = GEN_STARTING;
    chokeStartTime = millis();
    genVoltageStableStart = 0;
  }
}

void stopGenerator() {
  if (genState == GEN_RUNNING || genState == GEN_STARTING) {
    genState = GEN_STOPPING;
    genStopTime = millis();
  }
}

unsigned long getGenRuntime() {
  if (genState == GEN_RUNNING && genStartTime > 0) {
    return (millis() - genStartTime) / 1000;
  }
  return 0;
}

float getLastFuelLiters() { return lastFuelLiters; }
void setLastFuelLiters(float v) { lastFuelLiters = v; }