#ifndef CONFIG_H
#define CONFIG_H

#pragma once

// ============================================================================
// PIN ASSIGNMENTS - Centralized for easy modification
// ============================================================================

// SD/MMC SPI pins
#define PIN_SD_MISO     19
#define PIN_SD_MOSI     23
#define PIN_SD_SCK      18
#define PIN_SD_CS       5

// Relays (active level defined below)
#define PIN_RELAY_GEN       32  // Relay 2: Generator start/stop
#define PIN_RELAY_BATT      13  // Relay 3: Battery charging
#define PIN_RELAY_LOAD      14  // Relay 4: Load transfer

// Voltage sensors (input-only pins)
#define PIN_VSENSE_MAINS    34  // Mains voltage sensor
#define PIN_VSENSE_GEN      35  // Generator voltage sensor
#define PIN_VSENSE_BATT     36  // Battery voltage divider

// LEDs
#define PIN_LED_SMS         4   // SMS activity LED
#define PIN_LED_LOW_FUEL    2   // Low fuel warning LED

// Ultrasonic sensor
#define PIN_ULTRA_TRIG      25
#define PIN_ULTRA_ECHO      26

// Servo
#define PIN_SERVO           27

// Buzzer
#define PIN_BUZZER          33

// SIM800L UART2
#define PIN_SIM_TX          16  // ESP32 UART2 RX <- SIM800L TX
#define PIN_SIM_RX          17  // ESP32 UART2 TX -> SIM800L RX

// I2C (LCD + DS3231)
#define PIN_I2C_SDA         21
#define PIN_I2C_SCL         22

// ============================================================================
// RELAY CONFIGURATION
// ============================================================================
#define RELAY_ACTIVE_LOW    true   // Most relay modules are active-low
#define RELAY_ON            (RELAY_ACTIVE_LOW ? LOW : HIGH)
#define RELAY_OFF           (RELAY_ACTIVE_LOW ? HIGH : LOW)

// ============================================================================
// SERVO CONFIGURATION
// ============================================================================
#define SERVO_CHOKE_POS     0     // Choke engaged position (degrees)
#define SERVO_UNCHOKE_POS   90    // Choke released position (degrees)

// ============================================================================
// GENERATOR CONTROL TIMING (milliseconds)
// ============================================================================
#define GEN_STARTUP_TIMEOUT_MS     30000   // Max time to wait for gen startup
#define CHOKE_HOLD_MS              5000    // Hold choke for 5 seconds
#define GEN_VOLTAGE_STABLE_MS      3000    // Gen voltage must be stable this long
#define MAINS_RETURN_DELAY_MS      60000   // Wait 60s after mains return before stop
#define GEN_STOP_DELAY_MS          5000    // Delay after gen stop before load transfer

// ============================================================================
// VOLTAGE DETECTION THRESHOLDS
// ============================================================================
#define MAINS_VOLTAGE_THRESHOLD    150     // ADC threshold for mains present
#define GEN_VOLTAGE_THRESHOLD      150     // ADC threshold for gen voltage present
#define R1 47000
#define R2 10000
#define BATT_VOLTAGE_DIVIDER_RATIO (R1 + R2) / R2   // Voltage divider ratio (R1+R2)/R2
#define ADC_MAX_VOLTAGE            2.53f    // ESP32 ADC reference voltage
#define ADC_MAX_COUNT              2880    // 12-bit ADC

// Battery charging thresholds (actual battery voltage, not divided)
#define BATT_CHARGE_START_V        11.0f   // Start charging below this
#define BATT_CHARGE_STOP_V         13.4f   // Stop charging above this

// ============================================================================
// FUEL TANK CONFIGURATION
// ============================================================================
#define FUEL_TANK_CAPACITY_L       6.0f   // Tank capacity in liters
#define FUEL_EMPTY_DIST_CM         5.50f   // Sensor distance when empty
#define FUEL_FULL_DIST_CM          1.0f    // Sensor distance when full
#define FUEL_LOW_THRESHOLD_PCT     20      // Low fuel warning (%)
#define FUEL_CRITICAL_THRESHOLD_PCT 10     // Critical fuel warning (%)
#define FUEL_SHUTDOWN_THRESHOLD_PCT 7      // Auto shutdown (%)

// ============================================================================
// GSM CONFIGURATION
// ============================================================================
#define GSM_BAUD_RATE              115200
#define GSM_BUFFER_SIZE            256
#define GSM_CMD_TIMEOUT_MS         5000
#define GSM_INIT_TIMEOUT_MS        10000
#define GSM_APN                    "web.gprs.mtnnigeria.net"

// Default admin phone number (CHANGE THIS!)
#define DEFAULT_ADMIN_NUMBER       "+2349166983631"

// ============================================================================
// LCD CONFIGURATION
// ============================================================================
#define LCD_I2C_ADDR               0x27
#define LCD_COLS                   20
#define LCD_ROWS                   4
#define LCD_REFRESH_MS             500

// ============================================================================
// TIMING INTERVALS
// ============================================================================
#define SENSOR_READ_MS             500
#define FUEL_FILTER_SAMPLES        5
#define TELEMETRY_INTERVAL_MS      30000   // Send telemetry every 30s
#define NVS_SAVE_INTERVAL_MS       120000   // Save persistent data every 1230s
#define SMS_INTERVAL_MS             90000   // Check for SMS every 90s

// ============================================================================
// SMS COMMANDS
// ============================================================================
#define CMD_ONGEN       "ONGEN"
#define CMD_OFFGEN      "OFFGEN"
#define CMD_MSTATUS     "MSTATUS"
#define CMD_DSTATUS     "DSTATUS"
#define CMD_RSTALL      "RSTALL"
#define CMD_RSTNUMBER   "RSTNUMBER"
#define CMD_CHP_PREFIX  "CHP"

// ============================================================================
// NVS STORAGE KEYS
// ============================================================================
#define NVS_KEY_PHONE_NUM        "phone_num"
#define NVS_KEY_GEN_RUNTIME_DAILY "gen_rt_daily"
#define NVS_KEY_GEN_RUNTIME_TOTAL "gen_rt_total"
#define NVS_KEY_FUEL_DAILY       "fuel_daily"
#define NVS_KEY_FUEL_TOTAL       "fuel_total"
#define NVS_KEY_LAST_DATE        "last_date"
#define FILE_GEN_STAT            "/gen_stat.csv"

// ============================================================================
// TELEMETRY (placeholder - configure for your backend)
// ============================================================================
#define TELEMETRY_ENDPOINT         "https://smart-generator-alpha.vercel.app/api/devices/telemetry"
#define DEVICE_ID                  "GEN-CTRL-001"

#endif // CONFIG_H