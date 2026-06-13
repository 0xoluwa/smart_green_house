/**
 * @file SensorTask.cpp
 * @brief SensorTask implementation.
 */

#include "SensorTask.h"
#include "../data/SharedData.h"
#include "../sensors/DHT22Sensor.h"
#include "../sensors/LightSensor.h"
#include "../sensors/SoilMoistureSensor.h"
#include "../config.h"
#include <Arduino.h>
#include <Preferences.h>
#include <esp_log.h>

static const char* TAG = "SensorTask";

void SensorTask::start() {
    xTaskCreatePinnedToCore(
        taskFunc,
        "SensorTask",
        SENSOR_TASK_STACK,
        nullptr,
        SENSOR_TASK_PRIORITY,
        nullptr,
        0   // Core 0 – leave Core 1 for WiFi/web
    );
}

void SensorTask::taskFunc(void* /*param*/) {
    SharedData& sd = SharedData::instance();

    // ── Initialise DHT22 ─────────────────────────────────────────────────────
    DHT22Sensor envSensor;
    envSensor.init();
    ESP_LOGI(TAG, "DHT22 ready on GPIO %d", PIN_DHT22);

    // ── Initialise BH1750 light sensor ────────────────────────────────────────
    LightSensor lightSensor;
    if (!lightSensor.init()) {
        ESP_LOGW(TAG, "BH1750 not found – light readings will be invalid");
    }

    // ── Initialise capacitive soil moisture sensor ────────────────────────────
    SoilMoistureSensor soilSensor;
    soilSensor.init();  // Loads dry/wet calibration from NVS inside init()

    // Publish initial calibration into SharedData so the calibration page can
    // read the current values immediately without waiting for a WS command
    {
        SoilCalibration initCal{};
        initCal.dryAdc  = soilSensor.getDry();
        initCal.wetAdc  = soilSensor.getWet();
        initCal.updated = false;
        sd.setSoilCalibration(initCal);
    }

    // Signal that sensors are ready
    xEventGroupSetBits(sd.events, EVT_SENSORS_INIT);

    // ── Periodic read loop ────────────────────────────────────────────────────
    TickType_t lastWake = xTaskGetTickCount();
    for (;;) {
        // ── Check for calibration update from the web calibration page ────────
        {
            SoilCalibration cal = sd.getSoilCalibration();
            if (cal.updated) {
                soilSensor.setCalibration(cal.dryAdc, cal.wetAdc);

                // Persist to NVS so calibration survives a power cycle
                Preferences prefs;
                if (prefs.begin(NVS_NAMESPACE, false)) {
                    prefs.putInt(NVS_KEY_SOIL_DRY, cal.dryAdc);
                    prefs.putInt(NVS_KEY_SOIL_WET, cal.wetAdc);
                    prefs.end();
                }
                cal.updated = false;
                sd.setSoilCalibration(cal);
                ESP_LOGI(TAG, "Calibration applied and saved: dry=%d  wet=%d",
                         cal.dryAdc, cal.wetAdc);
            }
        }

        // ── DHT22 – temperature and humidity ─────────────────────────────────
        EnvironmentData envData{};
        bool envValid = envSensor.read(envData);

        EnvironmentReading envReading{};
        envReading.temperature = envData.temperature;
        envReading.humidity    = envData.humidity;
        envReading.valid       = envValid;
        envReading.timestampMs = millis();
        sd.setEnvironment(envReading);

        if (envValid) {
            ESP_LOGI(TAG, "T=%.1f°C  H=%.1f%%", envData.temperature, envData.humidity);
        } else {
            ESP_LOGW(TAG, "DHT22 read failed – check wiring and 4.7 kΩ pull-up");
        }

        // ── BH1750 – ambient light ─────────────────────────────────────────────
        float lux = NAN;
        bool luxValid = lightSensor.read(lux);

        LightReading lightReading{};
        lightReading.lux         = lux;
        lightReading.valid       = luxValid;
        lightReading.timestampMs = millis();
        sd.setLight(lightReading);

        if (luxValid) {
            ESP_LOGI(TAG, "Light=%.1f lux", lux);
        }

        // ── Capacitive soil moisture ───────────────────────────────────────────
        SoilData soilData{};
        bool soilValid = soilSensor.read(soilData);

        SoilReading soilReading{};
        soilReading.rawAdc      = soilData.rawAdc;
        soilReading.moisturePct = soilData.moisturePct;
        soilReading.valid       = soilValid;
        soilReading.timestampMs = millis();
        sd.setSoil(soilReading);

        if (soilValid) {
            ESP_LOGI(TAG, "Soil=%d raw  %.1f%%", soilData.rawAdc, soilData.moisturePct);
        }

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(SENSOR_TASK_PERIOD_MS));
    }
}
