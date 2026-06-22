/**
 * @file LightSensor.cpp
 * @brief BH1750 light sensor driver implementation.
 */

#include "LightSensor.h"
#include <Wire.h>
#include <esp_log.h>

static const char* TAG = "LightSensor";

LightSensor::LightSensor(uint8_t i2cAddress)
    : m_address(i2cAddress) {}

bool LightSensor::init() {
    // CONTINUOUS_HIGH_RES_MODE: 1 lux resolution, ~120 ms per measurement
    if (!m_bh.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, m_address, &Wire)) {
        ESP_LOGE(TAG, "BH1750 not found at I2C address 0x%02X – check wiring", m_address);
        m_initialised = false;
        return false;
    }
    m_initialised = true;
    ESP_LOGI(TAG, "Initialised at 0x%02X", m_address);
    return true;
}

bool LightSensor::read(float& lux) {
    if (!m_initialised) {
        return false;
    }
    lux = m_bh.readLightLevel();
    if (lux < 0.0f) {
        ESP_LOGW(TAG, "readLightLevel() returned invalid value");
        return false;
    }
    ESP_LOGD(TAG, "%.1f lux", lux);
    return true;
}
