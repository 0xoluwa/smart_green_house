/**
 * @file SoilMoistureSensor.h
 * @brief Driver for a single capacitive soil moisture sensor via ADC.
 *
 * The sensor outputs an analog voltage proportional to dielectric permittivity
 * of the surrounding soil. The raw ADC value is mapped to a 0–100 % scale
 * using the dry-air and fully-submerged calibration constants defined in
 * config.h (SOIL_ADC_DRY, SOIL_ADC_WET).
 *
 * Wiring: sensor VCC → 3.3 V (or 5 V, see note), GND → GND, AOUT → PIN_SOIL_ADC (ADC1 only).
 *
 * @note Some capacitive sensor modules (common blue v1.2) require 5 V to oscillate
 *       properly. At 3.3 V the NE555/CD4093 inside can stall and drive AOUT to 0 V.
 *       If init() reports "AOUT stuck at 0 V", try powering from VIN (5 V) and add a
 *       voltage divider on AOUT: 22 kΩ in series → GPIO pin → 33 kΩ → GND. That
 *       scales 5 V full-scale to ~3.05 V, safely inside the ADC's 3.6 V window.
 */

#pragma once

#include <cstdint>
#include "config.h"

/** @brief Aggregated soil moisture reading. */
struct SoilData {
    int   rawAdc;       ///< Raw 12-bit ADC value (0–4095)
    float moisturePct;  ///< Calibrated moisture percentage (0–100 %)
};

/**
 * @brief Driver for a capacitive soil moisture sensor on a single ADC pin.
 *
 * Uses esp32 analogRead() internally. The ADC is configured for 11 dB
 * attenuation (0–3.6 V range) to cover the full sensor output swing.
 */
class SoilMoistureSensor {
public:
    /**
     * @brief Construct the sensor driver.
     * @param pin  ADC1 GPIO pin the sensor output is connected to.
     * @param dry  Raw ADC value measured in completely dry air.
     * @param wet  Raw ADC value measured with the probe fully in water.
     */
    explicit SoilMoistureSensor(int pin = PIN_SOIL_ADC,
                                int dry = SOIL_ADC_DRY,
                                int wet = SOIL_ADC_WET);

    /**
     * @brief Load NVS calibration and run a startup ADC probe.
     *
     * Prints a diagnostic classification to the Serial log so hardware wiring
     * or power-supply problems are immediately visible on first boot.
     *
     * @return Always true (sensor absence is logged, not treated as fatal).
     */
    bool init();

    /**
     * @brief Read and calibrate the soil moisture.
     *
     * Takes four samples and averages them to reduce ADC noise before mapping
     * to the calibrated percentage.
     *
     * @param[out] data  Filled with raw ADC and calibrated percentage.
     * @return true on success.
     */
    bool read(SoilData& data);

    /**
     * @brief Update calibration at runtime without restarting.
     *
     * Called by SensorTask when the calibration page posts new dry/wet values.
     * The new values are also saved to NVS by SensorTask immediately after this
     * call so they survive a power cycle.
     *
     * @param dry  New raw ADC reference for completely dry air.
     * @param wet  New raw ADC reference for fully submerged in water.
     */
    void setCalibration(int dry, int wet) { m_dry = dry; m_wet = wet; }

    /** @brief Current dry-air ADC reference. */
    int getDry() const { return m_dry; }

    /** @brief Current fully-wet ADC reference. */
    int getWet() const { return m_wet; }

private:
    /**
     * @brief Map a raw ADC count to a moisture percentage.
     *
     * Clamps the result to [0, 100]. Higher ADC → drier (lower moisture %),
     * which is the inverse of what you might expect.
     *
     * @param raw  Raw 12-bit ADC value.
     * @return Moisture percentage in the range [0.0, 100.0].
     */
    float toPercent(int raw) const;

    int m_pin;
    int m_dry;
    int m_wet;
};
