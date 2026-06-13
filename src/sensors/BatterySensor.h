/**
 * @file BatterySensor.h
 * @brief Driver for monitoring a 3S LiPo battery via a resistive voltage divider.
 *
 * The battery positive terminal connects through a voltage divider
 * (R1 = 33 kΩ, R2 = 10 kΩ) to an ADC1 pin. The mid-point voltage is read,
 * scaled back to the real battery voltage, then mapped to a state-of-charge
 * percentage using a piecewise-linear approximation of the 3S LiPo discharge
 * curve.
 *
 * Wiring:
 *   V_bat ── R1(33 kΩ) ── PIN_BATTERY_ADC ── R2(10 kΩ) ── GND
 */

#pragma once

#include <cstdint>
#include "config.h"

/** @brief Battery voltage and state-of-charge snapshot. */
struct BatteryData {
    float   voltageV;   ///< Reconstructed battery terminal voltage (V)
    uint8_t percent;    ///< State-of-charge 0–100 %
    bool    isLow;      ///< True when voltage ≤ BATTERY_VOLTAGE_WARN
};

/**
 * @brief Measures battery voltage through an external resistive divider.
 *
 * Uses esp32 analogRead() with 11 dB attenuation. Multiple samples are
 * averaged per read to reduce ADC non-linearity effects.
 */
class BatterySensor {
public:
    /**
     * @brief Construct the battery sensor driver.
     * @param pin          ADC1 GPIO connected to the divider mid-point.
     * @param dividerRatio R2 / (R1 + R2) — see BATTERY_DIVIDER_RATIO in config.h.
     */
    explicit BatterySensor(int   pin          = PIN_BATTERY_ADC,
                           float dividerRatio = BATTERY_DIVIDER_RATIO);

    /**
     * @brief Configure the ADC channel.
     * @return Always true.
     */
    bool init();

    /**
     * @brief Read and interpret the battery voltage.
     * @param[out] data  Filled with voltage, percentage, and low-battery flag.
     * @return true on success.
     */
    bool read(BatteryData& data);

private:
    /**
     * @brief Convert a battery voltage to a state-of-charge percentage.
     *
     * Uses a piecewise-linear lookup table that approximates the typical
     * 3S LiPo discharge curve. Returns 100 % above BATTERY_VOLTAGE_FULL
     * and 0 % below BATTERY_VOLTAGE_DEAD.
     *
     * @param voltage  Battery terminal voltage in volts.
     * @return SoC in the range [0, 100].
     */
    static uint8_t voltageToPercent(float voltage);

    int   m_pin;
    float m_dividerRatio;
};
