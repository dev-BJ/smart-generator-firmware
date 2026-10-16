#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include "display.h"

static LiquidCrystal_I2C lcd(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS);
static bool firstUpdate = true;

void initDisplay() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Smart Generator");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  delay(1000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Smart Generator");
  lcd.setCursor(0, 1);
  lcd.print("System Ready");
  delay(1000);
  lcd.clear();
}

// void updateDisplay(bool mainsPresent, GenState genState, float fuelPercent, float fuelLiters, float batteryVoltage, float mainsVoltage, float genVoltage) {
//   if (firstUpdate) { lcd.clear(); firstUpdate = false; }

//   lcd.setCursor(0, 0);
//   lcd.print("Mains: ");
//   lcd.print(mainsPresent ? "ON " : "OFF");
//   lcd.print(" Volt: ");
//   lcd.print(mainsVoltage, 1);

//   lcd.setCursor(0, 1);
//   lcd.print("Gen: ");
//   switch (genState) {
//     case GEN_OFF: lcd.print("OFF "); break;
//     case GEN_STARTING: lcd.print("START"); break;
//     case GEN_RUNNING: lcd.print("RUN "); break;
//     case GEN_STOPPING: lcd.print("STOP"); break;
//   }
//   // lcd.print("       ");
//   lcd.print(" Volt: ");
//   lcd.print(genVoltage, 1);

//   extern RTC_DS3231 rtc;
//   extern String getDateTimeStr();
//   lcd.setCursor(0, 2);
//   lcd.print(getDateTimeStr());
//   lcd.print("       ");

//   lcd.setCursor(0, 3);
//   lcd.print("Fuel: ");
//   lcd.print((int)fuelPercent);
//   lcd.print("% ");
//   lcd.print(fuelLiters, 1);
//   lcd.print("L Bat:");
//   lcd.print(batteryVoltage, 1);
//   lcd.print("V   ");
// }

void updateDisplay(bool mainsPresent, GenState genState, float fuelPercent, float fuelLiters, float batteryVoltage, float mainsVoltage, float genVoltage) {
  // if (firstUpdate) { lcd.clear(); firstUpdate = false; }
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("MAINS");
  lcd.setCursor(7, 0);
  extern String getTimeStr();
  lcd.print(getTimeStr());
  lcd.setCursor(14, 0);
  lcd.print("GEN");

  lcd.setCursor(0, 1);
  lcd.print(mainsPresent ? "ON " : "OFF ");
  lcd.print(mainsVoltage, 0);
  lcd.print("V ");
  lcd.setCursor(10, 1);
  switch (genState) {
    case GEN_OFF: lcd.print("OFF "); break;
    case GEN_STARTING: lcd.print("START "); break;
    case GEN_RUNNING: lcd.print("RUN "); break;
    case GEN_STOPPING: lcd.print("STOP "); break;
  }
  // lcd.print("       ");
  lcd.print(genVoltage, 0);
  lcd.print("V");

  extern RTC_DS3231 rtc;
  extern String getDateStr();
  lcd.setCursor(0, 2);
  lcd.print(getDateStr());
  // lcd.print("       ");
  lcd.print(" Bat:");
  lcd.print(batteryVoltage, 1);
  lcd.print("V");

  lcd.setCursor(0, 3);
  lcd.print("Fuel: ");
  lcd.print((int)fuelPercent);
  lcd.print("% ");
  lcd.print(fuelLiters, 1);
  lcd.print("L");
}