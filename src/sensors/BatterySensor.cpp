/**
 * @file BatterySensor.cpp
 * @brief Battery voltage sensor implementation.
 */

#include "BatterySensor.h"
#include <Arduino.h>
#include <esp_log.h>
#include <algorithm>

static const char* TAG = "Battery";

// Piecewise-linear 3S LiPo discharge curve
// { voltage (V), SoC (%) } pairs, from full to empty
static constexpr struct { float v; uint8_t pct; } kCurve[] = {
    { 12.60f, 100 },
    { 12.00f,  90 },
    { 11.70f,  75 },
    { 11.40f,  60 },
    { 11.10f,  40 },
    { 10.80f,  20 },
    { 10.50f,  10 },
    {  9.00f,   0 },
};
static constexpr int kCurveLen = sizeof(kCurve) / sizeof(kCurve[0]);

BatterySensor::BatterySensor(int pin, float dividerRatio)
    : m_pin(pin), m_dividerRatio(dividerRatio) {}

bool BatterySensor::init() {
    // ADC attenuation and resolution are configured in setup() before tasks start.
    ESP_LOGI(TAG, "Battery ADC on GPIO %d (divider ratio = %.4f)", m_pin, m_dividerRatio);
    return true;
}

bool BatterySensor::read(BatteryData& data) {
    // Average 8 samples to reduce ADC noise
    long sum = 0;
    for (int i = 0; i < 8; i++) {
        sum += analogRead(m_pin);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    float adcRaw = static_cast<float>(sum) / 8.0f;

    // Reconstruct real battery voltage from divider mid-point
    float vAdc     = adcRaw / static_cast<float>((1 << BATTERY_ADC_BITS) - 1) * BATTERY_ADC_REF_V;
    data.voltageV  = vAdc / m_dividerRatio;
    data.percent   = voltageToPercent(data.voltageV);
    data.isLow     = data.voltageV <= BATTERY_VOLTAGE_WARN;

    ESP_LOGD(TAG, "raw=%.0f  Vadc=%.3fV  Vbat=%.2fV  SoC=%d%%",
             adcRaw, vAdc, data.voltageV, data.percent);
    return true;
}

uint8_t BatterySensor::voltageToPercent(float voltage) {
    if (voltage >= kCurve[0].v) return 100;
    if (voltage <= kCurve[kCurveLen - 1].v) return 0;

    for (int i = 0; i < kCurveLen - 1; i++) {
        if (voltage <= kCurve[i].v && voltage > kCurve[i + 1].v) {
            float t = (kCurve[i].v - voltage) / (kCurve[i].v - kCurve[i + 1].v);
            return static_cast<uint8_t>(kCurve[i].pct - t * (kCurve[i].pct - kCurve[i + 1].pct));
        }
    }
    return 0;
}
