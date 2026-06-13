/**
 * @file LightSensor.h
 * @brief Driver for the BH1750 ambient light sensor.
 *
 * The BH1750 is a 16-bit I²C lux meter. It reports illuminance in lux,
 * which is useful for tracking light levels inside the greenhouse (e.g. to
 * detect shading or determine if supplemental lighting is needed).
 *
 * Wiring:
 *   VCC  → 3.3 V
 *   GND  → GND
 *   SDA  → PIN_I2C_SDA (GPIO 21)
 *   SCL  → PIN_I2C_SCL (GPIO 22)
 *   ADDR → GND for address 0x23 (default), VCC for 0x5C
 *
 * Depends on: claws/BH1750 @ ^1.3.0
 */

#pragma once

#include <BH1750.h>
#include <cstdint>
#include "config.h"

/**
 * @brief Thin wrapper around the claws/BH1750 library.
 *
 * Only data collection is performed; the reading is published to SharedData
 * by SensorTask and there is no actuator driven from this value.
 */
class LightSensor {
public:
    /**
     * @brief Construct the driver.
     * @param i2cAddress I²C address – BH1750_I2C_ADDRESS from config.h.
     */
    explicit LightSensor(uint8_t i2cAddress = BH1750_I2C_ADDRESS);

    /**
     * @brief Initialise the BH1750 in continuous high-resolution mode.
     *
     * Resolution: 1 lux, measurement time ~120 ms.
     *
     * @return true if the chip is detected and configured successfully.
     */
    bool init();

    /**
     * @brief Read the current illuminance.
     * @param[out] lux  Illuminance in lux (0–65535).
     * @return true on success; false if the sensor is unavailable or returns
     *         an invalid reading.
     */
    bool read(float& lux);

private:
    BH1750  m_bh;
    uint8_t m_address;
    bool    m_initialised = false;
};
