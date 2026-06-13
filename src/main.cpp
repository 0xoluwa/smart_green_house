/**
 * @file main.cpp
 * @brief Smart Greenhouse – system entry point.
 *
 * Performs one-time hardware setup (Serial, I²C, ADC) then spawns all
 * FreeRTOS tasks. The Arduino loop() is intentionally empty; all work is
 * done inside tasks.
 *
 * Task map:
 *   Core 0 ── SensorTask   (priority 2)  reads BME280 + capacitive soil sensor
 *   Core 0 ── BatteryTask  (priority 1)  reads 3S LiPo voltage via ADC divider
 *   Core 0 ── PumpTask     (priority 3)  watering control loop + safety timeout
 *   Core 0 ── HistoryTask  (priority 1)  5-min ring-buffer snapshots (24 h)
 *   Core 1 ── WiFiTask     (priority 5)  WiFi station / AP provisioning
 *   Core 1 ── WebTask      (priority 2)  async HTTP + WebSocket broadcaster
 */

#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "tasks/SensorTask.h"
#include "tasks/BatteryTask.h"
#include "tasks/PumpTask.h"
#include "tasks/HistoryTask.h"
#include "tasks/WiFiTask.h"
#include "tasks/WebTask.h"

void setup() {
    Serial.begin(115200);
    Serial.println("\n\n=== Smart Greenhouse booting ===");

    // ── I²C bus (BME280 / AHT20) ──────────────────────────────────────────────
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(400000);  // Fast-mode 400 kHz

    // ── ADC – configure both channels here, before any task starts.
    // In Arduino-ESP32 v3.x the attenuation must be set before the FreeRTOS
    // scheduler hands control to a task, otherwise the first reads return 0.
    analogReadResolution(BATTERY_ADC_BITS);                  // 12-bit, 0-4095
    analogSetPinAttenuation(PIN_SOIL_ADC,    ADC_11db);      // 0-3.6 V on GPIO34
    analogSetPinAttenuation(PIN_BATTERY_ADC, ADC_11db);      // 0-3.6 V on GPIO35
    // Dummy reads to flush the ADC channel initialisation
    (void)analogRead(PIN_SOIL_ADC);
    (void)analogRead(PIN_BATTERY_ADC);

    // ── Spawn tasks ────────────────────────────────────────────────────────────
    // WiFi must start first; WebTask blocks on EVT_WIFI_READY.
    WiFiTask::start();
    WebTask::start();

    SensorTask::start();
    BatteryTask::start();
    PumpTask::start();
    HistoryTask::start();

    Serial.println("All tasks started.");
}

void loop() {
    // Intentionally empty – FreeRTOS tasks handle all work.
    vTaskDelete(nullptr);
}
