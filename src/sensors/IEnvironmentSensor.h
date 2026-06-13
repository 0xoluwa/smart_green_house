/**
 * @file IEnvironmentSensor.h
 * @brief Abstract interface that all environment sensor drivers must implement.
 *
 * Decouples the rest of the system from any specific sensor chip. To add a
 * new sensor, subclass this interface, implement init() and read(), then
 * register it in SensorFactory.cpp.
 */

#pragma once

#include <cmath>

/** @brief Aggregated reading from an environment sensor. */
struct EnvironmentData {
    float temperature = NAN;  ///< Degrees Celsius; NAN if unavailable
    float humidity    = NAN;  ///< Relative humidity %; NAN if unavailable
    float pressure    = NAN;  ///< hPa; NAN if the sensor has no pressure channel
};

/**
 * @brief Pure-virtual interface for environment sensor drivers.
 *
 * All drivers are initialised once via init() and then polled periodically
 * via read(). The driver owns its hardware handle; the system owns the driver.
 */
class IEnvironmentSensor {
public:
    virtual ~IEnvironmentSensor() = default;

    /**
     * @brief Initialise the sensor hardware.
     *
     * Called once after power-up (or after a reset). The implementation should
     * configure the chip, verify its identity (e.g. read a WHO_AM_I register),
     * and return false if the hardware is not detected.
     *
     * @return true on success, false if the sensor is absent or failed to init.
     */
    virtual bool init() = 0;

    /**
     * @brief Perform a single measurement and populate @p data.
     *
     * On failure the implementation should leave NAN in the affected fields
     * and return false. The caller checks the return value and the individual
     * field validity (isnan()) as needed.
     *
     * @param[out] data  Filled with the latest measurements.
     * @return true if at least temperature and humidity were read successfully.
     */
    virtual bool read(EnvironmentData& data) = 0;

    /**
     * @brief Human-readable name of the sensor (for log messages).
     * @return Null-terminated string, e.g. "BME280".
     */
    virtual const char* name() const = 0;
};
