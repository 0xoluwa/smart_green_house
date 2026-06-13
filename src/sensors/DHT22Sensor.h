/**
 * @file DHT22Sensor.h
 * @brief IEnvironmentSensor driver for the ASAIR DHT22 (AM2302).
 *
 * Provides temperature and relative humidity via single-wire protocol.
 * Pressure is not available; data.pressure will always be NAN.
 * Minimum polling interval is 2 s per the datasheet; SensorTask respects this
 * since it runs every SENSOR_TASK_PERIOD_MS (default 5 s).
 * Depends on the Adafruit DHT sensor library (adafruit/DHT sensor library).
 */

#pragma once

#include "IEnvironmentSensor.h"
#include <DHT.h>
#include "config.h"

/**
 * @brief Concrete driver for the DHT22 (AM2302) sensor.
 *
 * Switch to this driver by setting ACTIVE_SENSOR = SensorType::DHT22 in
 * config.h. The data pin is set by PIN_DHT22 in config.h (default GPIO 4).
 */
class DHT22Sensor : public IEnvironmentSensor {
public:
    /**
     * @brief Construct a DHT22Sensor.
     * @param pin GPIO data pin connected to the DHT22 output (default PIN_DHT22).
     */
    explicit DHT22Sensor(uint8_t pin = PIN_DHT22);

    /**
     * @brief Call DHT::begin() to set up the 1-wire timing.
     * @return Always true (the DHT library has no I²C presence check).
     */
    bool init() override;

    /**
     * @brief Read temperature and humidity.
     *        data.pressure is set to NAN (no pressure channel).
     * @param[out] data  Populated with fresh sensor values.
     * @return true if both fields are non-NAN.
     */
    bool read(EnvironmentData& data) override;

    /** @brief Returns "DHT22". */
    const char* name() const override { return "DHT22"; }

private:
    DHT  m_dht;
    bool m_initialised = false;
};
