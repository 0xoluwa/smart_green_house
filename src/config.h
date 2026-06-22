/**
 * @file config.h
 * @brief Central compile-time configuration for the Smart Greenhouse system.
 *
 * This is the single file to edit when changing hardware layout, calibration
 * values, or task timing. Everything else in the codebase reads from here.
 */

#pragma once

#include <freertos/FreeRTOS.h>   // UBaseType_t
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
// I²C bus pins  (BH1750 light sensor)
// ─────────────────────────────────────────────────────────────────────────────
constexpr int PIN_I2C_SDA = 22;  
constexpr int PIN_I2C_SCL = 21;  

/**
 * @brief BH1750 I²C address.
 *  0x23 when ADDR pin is tied to GND (default).
 *  0x5C when ADDR pin is tied to VCC.
 */
constexpr uint8_t BH1750_I2C_ADDRESS = 0x23;

// ─────────────────────────────────────────────────────────────────────────────
// DHT22 data pin
// ─────────────────────────────────────────────────────────────────────────────
constexpr int PIN_DHT22 = 4;

// ─────────────────────────────────────────────────────────────────────────────
// ADC input pins
// ADC1 pins only – ADC2 conflicts with the WiFi radio when WiFi is active.
// ─────────────────────────────────────────────────────────────────────────────
constexpr int PIN_SOIL_ADC    = 34;  ///< ADC1_CH6 – capacitive soil moisture output
constexpr int PIN_BATTERY_ADC = 35;  ///< ADC1_CH7 – voltage-divider mid-point

// ─────────────────────────────────────────────────────────────────────────────
// Relay / pump
// ─────────────────────────────────────────────────────────────────────────────
constexpr int PIN_RELAY = 25;

/**
 * @brief Set true for the common blue/red relay modules where a LOW signal
 *        energises the coil (pump ON), and HIGH de-energises it (pump OFF).
 *        Set false for active-HIGH relay modules.
 */
constexpr bool RELAY_ACTIVE_LOW = false;

// ─────────────────────────────────────────────────────────────────────────────
// Battery – 3S LiPo voltage divider
//
// Recommended resistors: R1 = 33 kΩ, R2 = 10 kΩ
//   Ratio = R2 / (R1 + R2) = 10 / 43 ≈ 0.2326
//   At V_bat = 12.6 V → V_adc = 2.93 V  (safely below 3.3 V)
//   At V_bat =  9.0 V → V_adc = 2.09 V
// ─────────────────────────────────────────────────────────────────────────────
constexpr float BATTERY_DIVIDER_RATIO = 10.0f / 43.0f;
constexpr float BATTERY_ADC_REF_V     = 3.3f;    ///< ADC full-scale voltage
constexpr int   BATTERY_ADC_BITS      = 12;       ///< 12-bit → 0–4095
constexpr float BATTERY_VOLTAGE_FULL  = 12.60f;   ///< 4.20 V × 3 cells (100 %)
constexpr float BATTERY_VOLTAGE_WARN  = 10.80f;   ///< 3.60 V × 3 cells (~20 %)
constexpr float BATTERY_VOLTAGE_DEAD  =  9.00f;   ///< 3.00 V × 3 cells (0 %)

// ─────────────────────────────────────────────────────────────────────────────
// Capacitive soil moisture sensor calibration
//
// Measure the raw ADC count with the probe in dry air and fully submerged in
// water, then update the two constants below.
// ─────────────────────────────────────────────────────────────────────────────
constexpr int SOIL_ADC_DRY = 100;  ///< Raw ADC in completely dry air
constexpr int SOIL_ADC_WET = 0;  ///< Raw ADC fully submerged in water

// ─────────────────────────────────────────────────────────────────────────────
// WiFi provisioning AP
// ─────────────────────────────────────────────────────────────────────────────
constexpr const char* WIFI_AP_SSID          = "GreenHouse_Setup";
constexpr const char* WIFI_AP_PASSWORD      = "";        ///< Open network
constexpr uint32_t    WIFI_CONNECT_TIMEOUT_MS = 30000; ///< Station mode timeout

// ─────────────────────────────────────────────────────────────────────────────
// NVS (non-volatile storage) namespace and keys
// ─────────────────────────────────────────────────────────────────────────────
constexpr const char* NVS_NAMESPACE     = "greenhouse";
constexpr const char* NVS_KEY_SSID      = "ssid";
constexpr const char* NVS_KEY_PASS      = "password";
constexpr const char* NVS_KEY_AP_SSID   = "ap_ssid";   ///< Custom AP hotspot name
constexpr const char* NVS_KEY_AP_PASS   = "ap_pass";   ///< Custom AP hotspot password
constexpr const char* NVS_KEY_THRESHOLD = "threshold";
constexpr const char* NVS_KEY_TIMEOUT   = "timeout";
constexpr const char* NVS_KEY_SOIL_DRY  = "soil_dry";  ///< Calibration: raw ADC in dry air
constexpr const char* NVS_KEY_SOIL_WET  = "soil_wet";  ///< Calibration: raw ADC in water

// ─────────────────────────────────────────────────────────────────────────────
// Pump defaults  (overridden by values stored in NVS after first configuration)
// ─────────────────────────────────────────────────────────────────────────────
constexpr float    PUMP_DEFAULT_THRESHOLD_PCT = 30.0f;  ///< Water when soil < 30 %
constexpr uint32_t PUMP_DEFAULT_TIMEOUT_S     = 60;     ///< Hard-off after 60 s
constexpr uint32_t PUMP_MIN_OFF_INTERVAL_S    = 30;     ///< Minimum rest between cycles

// ─────────────────────────────────────────────────────────────────────────────
// History ring buffer  (5-min resolution → 288 samples = 24 h)
// ─────────────────────────────────────────────────────────────────────────────
constexpr int HISTORY_INTERVAL_S = 60;    ///< One sample every 1 minute
constexpr int HISTORY_SIZE       = 480;   ///< 480 × 1 min = 8 h ring buffer (~9.6 KB RAM, ~26 KB JSON)

// ─────────────────────────────────────────────────────────────────────────────
// FreeRTOS task periods
// ─────────────────────────────────────────────────────────────────────────────
constexpr uint32_t SENSOR_TASK_PERIOD_MS   =  1000;
constexpr uint32_t BATTERY_TASK_PERIOD_MS  = 15000;
constexpr uint32_t PUMP_TASK_PERIOD_MS     =   500;
constexpr uint32_t WEB_BROADCAST_PERIOD_MS =  2000;
constexpr uint32_t HISTORY_TASK_PERIOD_MS  = static_cast<uint32_t>(HISTORY_INTERVAL_S) * 1000;

// ─────────────────────────────────────────────────────────────────────────────
// FreeRTOS task stack sizes (words)
// ─────────────────────────────────────────────────────────────────────────────
constexpr uint32_t SENSOR_TASK_STACK  = 4096;
constexpr uint32_t BATTERY_TASK_STACK = 4096;  // needs room for float log formatting
constexpr uint32_t PUMP_TASK_STACK    = 4096;  // Preferences + float logs → was overflowing at 2048
constexpr uint32_t WEB_TASK_STACK     = 8192;
constexpr uint32_t WIFI_TASK_STACK    = 8192;
constexpr uint32_t HISTORY_TASK_STACK = 8192;  ///< Increased for LittleFS ops + vector in PersistentLog

// ─────────────────────────────────────────────────────────────────────────────
// FreeRTOS task priorities  (higher number = higher priority)
// ─────────────────────────────────────────────────────────────────────────────
constexpr UBaseType_t WIFI_TASK_PRIORITY    = 5;
constexpr UBaseType_t PUMP_TASK_PRIORITY    = 3;
constexpr UBaseType_t SENSOR_TASK_PRIORITY  = 2;
constexpr UBaseType_t WEB_TASK_PRIORITY     = 2;
constexpr UBaseType_t BATTERY_TASK_PRIORITY = 1;
constexpr UBaseType_t HISTORY_TASK_PRIORITY = 1;

// ─────────────────────────────────────────────────────────────────────────────
// Persistent logging (LittleFS)
// ─────────────────────────────────────────────────────────────────────────────
constexpr uint8_t LOG_WARN_PCT   = 75;  ///< Warn user when LittleFS > 75 % full
constexpr uint8_t LOG_EVICT_PCT  = 90;  ///< Evict oldest day when > 90 % full
