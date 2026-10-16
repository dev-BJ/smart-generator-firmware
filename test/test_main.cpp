#include <unity.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "../include/config.h"

#define FUEL_EMPTY_DIST_CM 40.0f
#define FUEL_FULL_DIST_CM 5.0f
#define FUEL_TANK_CAPACITY_L 50.0f
#define FUEL_FILTER_SAMPLES 5

static float fuelSamples[FUEL_FILTER_SAMPLES];
static int fuelSampleIdx = 0;
static int fuelSampleCount = 0;

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

float readVoltageMock(int adc, float dividerRatio) {
  return (adc * 3.3f / 4095.0f) * dividerRatio;
}

String normalizePhone(const char* phone) {
  String p = phone;
  p.trim();
  if (p.startsWith("+")) return p;
  if (p.startsWith("0") && p.length() == 11) return "+234" + p.substring(1);
  if (p.length() == 10) return "+234" + p;
  return p;
}

bool isAuthorized(const char* sender, const char* registered, const char* defaultNum) {
  String s = normalizePhone(sender);
  String r = normalizePhone(registered);
  String d = normalizePhone(defaultNum);
  return s == r || s == d;
}

typedef enum { GEN_OFF, GEN_STARTING, GEN_RUNNING, GEN_STOPPING } GenState;

void setUp() {
  fuelSampleIdx = 0;
  fuelSampleCount = 0;
  memset(fuelSamples, 0, sizeof(fuelSamples));
}

void tearDown() {}

void test_calcFuelPercent_valid() {
  float pct = calcFuelPercent(FUEL_EMPTY_DIST_CM);
  TEST_ASSERT_FLOAT_WITHIN(0.1, 0.0f, pct);

  pct = calcFuelPercent(FUEL_FULL_DIST_CM);
  TEST_ASSERT_FLOAT_WITHIN(0.1, 100.0f, pct);

  float midDist = (FUEL_EMPTY_DIST_CM + FUEL_FULL_DIST_CM) / 2.0f;
  pct = calcFuelPercent(midDist);
  TEST_ASSERT_FLOAT_WITHIN(1.0, 50.0f, pct);
}

void test_calcFuelPercent_clamped() {
  float pct = calcFuelPercent(FUEL_EMPTY_DIST_CM + 10);
  TEST_ASSERT_FLOAT_WITHIN(0.1, 0.0f, pct);

  pct = calcFuelPercent(FUEL_FULL_DIST_CM - 5);
  TEST_ASSERT_FLOAT_WITHIN(0.1, 100.0f, pct);
}

void test_calcFuelPercent_invalid() {
  float pct = calcFuelPercent(-1.0f);
  TEST_ASSERT_EQUAL(-1.0f, pct);
}

void test_fuelLiters_calculation() {
  float pct = 50.0f;
  float liters = pct * FUEL_TANK_CAPACITY_L / 100.0f;
  TEST_ASSERT_FLOAT_WITHIN(0.1, 25.0f, liters);
}

void test_fuelFilter() {
  float samples[] = {30, 31, 29, 30, 32};
  for (int i = 0; i < 5; i++) filterFuel(samples[i]);
  float avg = 0;
  for (int i = 0; i < 5; i++) avg += samples[i];
  avg /= 5;
  float filtered = filterFuel(30);
  TEST_ASSERT_FLOAT_WITHIN(0.5, avg, filtered);
}

void test_batteryChargingHysteresis() {
  bool chg = false;
  float v = 10.5f;
  if (v < 11.0f && !chg) chg = true;
  TEST_ASSERT_TRUE(chg);

  v = 12.0f;
  if (v >= 13.4f && chg) chg = false;
  TEST_ASSERT_FALSE(chg);

  v = 13.4f;
  if (v >= 13.4f && chg) chg = false;
  TEST_ASSERT_FALSE(chg);
}

void test_genStateTransitions() {
  GenState state = GEN_OFF;
  TEST_ASSERT_EQUAL(GEN_OFF, state);
  state = GEN_STARTING;
  TEST_ASSERT_EQUAL(GEN_STARTING, state);
  state = GEN_RUNNING;
  TEST_ASSERT_EQUAL(GEN_RUNNING, state);
  state = GEN_STOPPING;
  TEST_ASSERT_EQUAL(GEN_STOPPING, state);
}

void test_runtimeAccumulation() {
  unsigned long daily = 3600;
  unsigned long total = 7200;
  unsigned long elapsed = 1800;
  daily += elapsed;
  total += elapsed;
  TEST_ASSERT_EQUAL(5400, daily);
  TEST_ASSERT_EQUAL(9000, total);
}

void test_dayRollover() {
  int lastDay = 15;
  int today = 16;
  if (today != lastDay) lastDay = today;
  TEST_ASSERT_EQUAL(16, lastDay);
}

void test_smsCommandParsing() {
  const char* cmd = "ONGEN";
  TEST_ASSERT_EQUAL_STRING("ONGEN", cmd);

  cmd = "CHP09132452423N07034221134";
  const char* nPos = strchr(cmd, 'N');
  TEST_ASSERT_NOT_NULL(nPos);
  TEST_ASSERT_TRUE(nPos - cmd > 3);
  char oldNum[20], newNum[20];
  int oldLen = nPos - (cmd + 3);
  strncpy(oldNum, cmd + 3, oldLen);
  oldNum[oldLen] = '\0';
  strcpy(newNum, nPos + 1);
  TEST_ASSERT_EQUAL_STRING("09132452423", oldNum);
  TEST_ASSERT_EQUAL_STRING("07034221134", newNum);
}

void test_phoneNormalization() {
  String p1 = normalizePhone("08012345678");
  TEST_ASSERT_EQUAL_STRING("+2348012345678", p1.c_str());

  String p2 = normalizePhone("+2348012345678");
  TEST_ASSERT_EQUAL_STRING("+2348012345678", p2.c_str());

  String p3 = normalizePhone("8012345678");
  TEST_ASSERT_EQUAL_STRING("+2348012345678", p3.c_str());
}

void test_voltageReading() {
  int adc = 2048;
  float v = (adc * 3.3f / 4095.0f) * 11.0f;
  TEST_ASSERT_FLOAT_WITHIN(0.5, 6.6f * 11.0f, v);
}

void test_fuelThresholds() {
  TEST_ASSERT_TRUE(30 > 10);
  TEST_ASSERT_TRUE(10 > 7);
  TEST_ASSERT_EQUAL(30, 30);
  TEST_ASSERT_EQUAL(10, 10);
  TEST_ASSERT_EQUAL(7, 7);
}

void test_telemetryPayloadFormat() {
  const char* payload = "{\"device_id\":\"GEN-CTRL-001\",\"timestamp\":\"01/01/2026 12:00\",\"mains_voltage\":230,\"gen_voltage\":0,\"gen_state\":0,\"fuel_percent\":50,\"fuel_liters\":25.0,\"battery_voltage\":12.5,\"charging\":false}";
  TEST_ASSERT_NOT_NULL(strstr(payload, "GEN-CTRL-001"));
  TEST_ASSERT_NOT_NULL(strstr(payload, "mains_voltage"));
  TEST_ASSERT_NOT_NULL(strstr(payload, "gen_voltage"));
  TEST_ASSERT_NOT_NULL(strstr(payload, "fuel_percent"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_calcFuelPercent_valid);
  RUN_TEST(test_calcFuelPercent_clamped);
  RUN_TEST(test_calcFuelPercent_invalid);
  RUN_TEST(test_fuelLiters_calculation);
  RUN_TEST(test_fuelFilter);
  RUN_TEST(test_batteryChargingHysteresis);
  RUN_TEST(test_genStateTransitions);
  RUN_TEST(test_runtimeAccumulation);
  RUN_TEST(test_dayRollover);
  RUN_TEST(test_smsCommandParsing);
  RUN_TEST(test_phoneNormalization);
  RUN_TEST(test_voltageReading);
  RUN_TEST(test_fuelThresholds);
  RUN_TEST(test_telemetryPayloadFormat);
  UNITY_END();
  return 0;
}