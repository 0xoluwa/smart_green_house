/**
 * @file SoilMoistureSensor.cpp
 * @brief Capacitive soil moisture sensor implementation.
 */

#include "SoilMoistureSensor.h"
#include <Arduino.h>
#include <Preferences.h>
#include <esp_log.h>
#include <algorithm>

static const char* TAG = "SoilSensor";

static constexpr int RAW_INVALID_LOW  = 10;
static constexpr int RAW_INVALID_HIGH = 4085;

SoilMoistureSensor::SoilMoistureSensor(int pin, int dry, int wet)
    : m_pin(pin), m_dry(dry), m_wet(wet) {}

bool SoilMoistureSensor::init() {
    // ── 1. Load saved calibration from NVS ───────────────────────────────────
    {
        Preferences prefs;
        if (prefs.begin(NVS_NAMESPACE, true)) {
            m_dry = prefs.getInt(NVS_KEY_SOIL_DRY, m_dry);
            m_wet = prefs.getInt(NVS_KEY_SOIL_WET, m_wet);
            prefs.end();
        }
    }
    ESP_LOGI(TAG, "GPIO%d  dry=%d  wet=%d", m_pin, m_dry, m_wet);

    // ── 2. Re-apply ADC attenuation ───────────────────────────────────────────
    // The Preferences/NVS call above accesses the internal flash SPI bus.
    // On ESP-IDF v5.x (Arduino-ESP32 v3.x) this can reset the ADC channel
    // configuration that was set in setup() – so we must re-apply it here,
    // AFTER the flash operation, not before.
    analogSetPinAttenuation(m_pin, ADC_11db);  // 0–3.6 V on this pin
    vTaskDelay(pdMS_TO_TICKS(100));            // Let ADC settle

    // Discard the first few reads – the ADC sample-hold needs a moment to
    // charge to the real input voltage after the attenuation was re-applied.
    for (int i = 0; i < 5; i++) {
        (void)analogRead(m_pin);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // ── 3. Startup probe ──────────────────────────────────────────────────────
    int samples[10];
    long sum = 0;
    int minV = 4095, maxV = 0;
    for (int i = 0; i < 10; i++) {
        samples[i] = analogRead(m_pin);
        sum  += samples[i];
        if (samples[i] < minV) minV = samples[i];
        if (samples[i] > maxV) maxV = samples[i];
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    int avg    = static_cast<int>(sum / 10);
    int spread = maxV - minV;

    ESP_LOGI(TAG, "Probe GPIO%d: avg=%d  min=%d  max=%d  spread=%d",
             m_pin, avg, minV, maxV, spread);

    if (avg <= RAW_INVALID_LOW && spread <= 8) {
        ESP_LOGW(TAG, "AOUT stuck at 0 V – sensor not powered or output shorted to GND on GPIO%d", m_pin);
    } else if (avg <= RAW_INVALID_LOW && spread > 8) {
        ESP_LOGW(TAG, "GPIO%d floating – AOUT wire not reaching the pin", m_pin);
    } else if (avg >= RAW_INVALID_HIGH) {
        ESP_LOGW(TAG, "AOUT saturated (avg=%d) on GPIO%d", avg, m_pin);
    } else {
        ESP_LOGI(TAG, "Sensor OK – avg=%d (%.1f%%)", avg, toPercent(avg));
    }

    return true;
}

bool SoilMoistureSensor::read(SoilData& data) {
    long sum = 0;
    for (int i = 0; i < 8; i++) {
        sum += analogRead(m_pin);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    data.rawAdc = static_cast<int>(sum / 8);

    if (data.rawAdc <= RAW_INVALID_LOW || data.rawAdc >= RAW_INVALID_HIGH) {
        static uint32_t lastWarnMs = 0;
        if (millis() - lastWarnMs >= 30000) {
            lastWarnMs = millis();
            ESP_LOGW(TAG, "raw=%d invalid on GPIO%d (valid range %d–%d)",
                     data.rawAdc, m_pin, RAW_INVALID_LOW, RAW_INVALID_HIGH);
        }
        data.moisturePct = 0.0f;
        return false;
    }

    data.moisturePct = toPercent(data.rawAdc);
    return true;
}

float SoilMoistureSensor::toPercent(int raw) const {
    float pct = static_cast<float>(m_dry - raw) /
                static_cast<float>(m_dry - m_wet) * 100.0f;
    return std::clamp(pct, 0.0f, 100.0f);
}
