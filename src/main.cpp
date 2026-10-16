#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Preferences.h>
#include <RTClib.h>
#include "config.h"
#include "fuel_monitor.h"
#include "battery_monitor.h"
#include "generator_control.h"
#include "load_transfer.h"
#include "gsm_handler.h"
#include "display.h"
#include "storage.h"
#include "ZMPT101B.h"
#include "esp_task_wdt.h"

#define CLEAR_NVS false
#define SENSITIVITY 491.7f
#define CALIBRATION_FACTOR 1.49

RTC_DS3231 rtc;
ZMPT101B mainsVoltageSensor(PIN_VSENSE_MAINS, 50.0);
ZMPT101B genVoltageSensor(PIN_VSENSE_GEN, 50.0);

GenState genState = GEN_OFF;
bool mainsPresent = false;
bool genVoltagePresent = false;
float mainsVoltage = 0.0f;
float genVoltage = 0.0f;
float batteryVoltage = 0.0f;
unsigned long genRuntimeDaily = 0;
unsigned long genRuntimeTotal = 0;
float fuelConsumedDaily = 0.0f;
float fuelConsumedTotal = 0.0f;
int lastDay = -1;
unsigned long lastSensorRead = 0;
unsigned long lastLcdRefresh = 0;
unsigned long lastFuelAlert = 0;
unsigned long lastNvsSave = 0;
unsigned long genStartTime = 0;
String registeredPhone = "";
static void gsm_task(void* pvParameters);
static bool isGsmReady = false;

String getDateTimeStr() {
  DateTime now = rtc.now();
  char buf[20];
  sprintf(buf, "%02d/%02d/%04d %02d:%02d", now.day(), now.month(), now.year(), now.hour(), now.minute());
  return String(buf);
}

String getTimeStr() {
  DateTime now = rtc.now();
  char buf[20];
  sprintf(buf, "%02d:%02d", now.hour(), now.minute());
  return String(buf);
}

String getDateStr() {
  DateTime now = rtc.now();
  char buf[20];
  sprintf(buf, "%02d/%02d/%04d", now.day(), now.month(), now.year());
  return String(buf);
}

void beepBuzzer(int count, int duration) {
  for (int i = 0; i < count; i++) {
    digitalWrite(PIN_BUZZER, HIGH);
    delay(duration);
    digitalWrite(PIN_BUZZER, LOW);
    if (i < count - 1) delay(duration);
  }
}

void readSensors() {
  float mains_voltage = mainsVoltageSensor.getRmsVoltage() * CALIBRATION_FACTOR;
  float gen_voltage = genVoltageSensor.getRmsVoltage() * CALIBRATION_FACTOR;
  mainsVoltage = 0;
  genVoltage = 0;
  if (mains_voltage >= MAINS_VOLTAGE_THRESHOLD) mainsVoltage = mains_voltage;
  if (gen_voltage >= GEN_VOLTAGE_THRESHOLD) genVoltage = gen_voltage;
  mainsPresent = mainsVoltage > MAINS_VOLTAGE_THRESHOLD;
  genVoltagePresent = genVoltage > GEN_VOLTAGE_THRESHOLD;
  batteryVoltage = readBatteryVoltage();
  updateFuelPercent();
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Smart Generator Controller ===");

  pinMode(PIN_RELAY_GEN, OUTPUT);
  pinMode(PIN_RELAY_BATT, OUTPUT);
  pinMode(PIN_RELAY_LOAD, OUTPUT);
  pinMode(PIN_LED_SMS, OUTPUT);
  pinMode(PIN_LED_LOW_FUEL, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_ULTRA_TRIG, OUTPUT);
  pinMode(PIN_ULTRA_ECHO, INPUT);
  pinMode(PIN_VSENSE_MAINS, INPUT);
  pinMode(PIN_VSENSE_GEN, INPUT);
  pinMode(PIN_VSENSE_BATT, INPUT);

  digitalWrite(PIN_RELAY_GEN, RELAY_OFF);
  digitalWrite(PIN_RELAY_BATT, RELAY_OFF);
  digitalWrite(PIN_RELAY_LOAD, RELAY_OFF);
  digitalWrite(PIN_LED_SMS, LOW);
  digitalWrite(PIN_LED_LOW_FUEL, LOW);
  digitalWrite(PIN_BUZZER, LOW);

  mainsVoltageSensor.setSensitivity(SENSITIVITY);
  genVoltageSensor.setSensitivity(SENSITIVITY);

  initDisplay();

  if (!rtc.begin()) {
    Serial.println("RTC not found!");
    while (1) delay(1000);
  }
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting compile time");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  initStorage();
  if(CLEAR_NVS) clearStorage();
  loadPersistentData(registeredPhone, genRuntimeDaily, genRuntimeTotal, fuelConsumedDaily, fuelConsumedTotal, lastDay);

  initGeneratorControl();
  initLoadTransfer();

  esp_task_wdt_config_t config = {
      .timeout_ms = 240000,  // 4 mins
      .idle_core_mask = (1 << 0) | (1 << 1),
      .trigger_panic = true
  };

  esp_err_t err = esp_task_wdt_reconfigure(&config);
  Serial.printf("WDT init result: %s (0x%X)\n",
                  esp_err_to_name(err),
                  err);

  if (err != ESP_OK) {
      Serial.println("ERROR: Task Watchdog initialization failed!");
  } else {
      Serial.println("Task Watchdog initialized successfully.");
  }
  initSmsWorker();

  // GSM switch handler task (high priority for GSM tasks)
  xTaskCreatePinnedToCore(
      gsm_task,
      "GsmTask",
      80000,
      NULL,
      1,
      NULL,
      0
  );

  // sendSms(DEFAULT_ADMIN_NUMBER, "Setup complete");

  lastDay = rtc.now().day();
  beepBuzzer(1, 200);
  Serial.println("Setup complete");
}

void loop() {
  unsigned long now = millis();

  if (now - lastSensorRead >= SENSOR_READ_MS) {
    readSensors();
    lastSensorRead = now;
  }

  checkDayRollover(lastDay, genRuntimeDaily, fuelConsumedDaily);

  updateGeneratorControl(&genState, mainsPresent, genVoltagePresent, registeredPhone, mainsVoltage, genVoltage);
  updateLoadTransfer(mainsPresent, genState);
  updateBatteryCharging();
  
  updateRuntimeStats(genRuntimeDaily, genRuntimeTotal, fuelConsumedDaily, fuelConsumedTotal, getLastFuelLiters(), genState);

  if (now - lastLcdRefresh >= LCD_REFRESH_MS) {
   updateDisplay(mainsPresent, genState, getFuelPercent(), getFuelLiters(), batteryVoltage, mainsVoltage, genVoltage);
   lastLcdRefresh = now;
  }

  updateSmsLed();

  delay(10);
}

static void flush_sms_task (void* pvParameters) {
    (void)pvParameters;

    for (;;) {
      // flushQueuedSms();
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void gsm_task(void* pvParameters) {
    (void)pvParameters;

    initGsm();
    configureGPRS();

    // xTaskCreatePinnedToCore(
    //   flush_sms_task,
    //   "FlushSmsTask",
    //   40000,
    //   NULL,
    //   2,
    //   NULL,
    //   0
    // );
    
    for (;;) {
      unsigned long now = millis();

      processGsm(registeredPhone);

      if (now - lastFuelAlert >= SMS_INTERVAL_MS) {
        Serial.println("### Checking fuel level for alerts ###");
        updateFuelAlerts(registeredPhone);
        lastFuelAlert = now;
      }

      if (now - lastNvsSave >= NVS_SAVE_INTERVAL_MS) {
        savePersistentData(registeredPhone, genRuntimeDaily, genRuntimeTotal, fuelConsumedDaily, fuelConsumedTotal, lastDay, genVoltage, mainsVoltage);
        lastNvsSave = now;
      }

      // processGsm() normally returns immediately between SMS polling windows.
      // Yield CPU 0 so IDLE0 can service the task watchdog.
      vTaskDelay(pdMS_TO_TICKS(10));
    }
}
