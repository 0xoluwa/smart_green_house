/**
 * @file DHT22Sensor.cpp
 * @brief DHT22 driver implementation.
 */

#include "DHT22Sensor.h"
#include <esp_log.h>
#include <cmath>

static const char* TAG = "DHT22";

DHT22Sensor::DHT22Sensor(uint8_t pin)
    : m_dht(pin, DHT22) {}

bool DHT22Sensor::init() {
    m_dht.begin();
    m_initialised = true;
    ESP_LOGI(TAG, "Initialised on GPIO %d", PIN_DHT22);
    return true;
}

bool DHT22Sensor::read(EnvironmentData& data) {
    if (!m_initialised) {
        return false;
    }
    data.temperature = m_dht.readTemperature();
    data.humidity    = m_dht.readHumidity();
    data.pressure    = NAN;

    if (isnan(data.temperature) || isnan(data.humidity)) {
        ESP_LOGW(TAG, "Read failed – check wiring and 4.7k pull-up on data pin");
        return false;
    }
    return true;
}
