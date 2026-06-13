/**
 * @file PumpController.h
 * @brief Controls the 12 V pump relay and enforces all safety constraints.
 *
 * The PumpController owns the relay GPIO. It exposes turn-on/off methods that
 * are aware of:
 *  - Relay polarity (active-LOW or active-HIGH, configured in config.h)
 *  - Maximum single-run duration (configurable safety timeout)
 *  - Minimum off-time between cycles (prevents rapid cycling)
 *
 * PumpTask is the only caller; no other code should drive the relay directly.
 */

#pragma once

#include <cstdint>
#include "config.h"

/**
 * @brief Low-level relay driver with built-in safety interlocks.
 *
 * Call init() once from setup, then use turnOn() / turnOff() / tick() from
 * the PumpTask loop. tick() must be called regularly so the controller can
 * enforce the configurable safety timeout without blocking.
 */
class PumpController {
public:
    /**
     * @brief Construct the controller.
     * @param relayPin    GPIO pin driving the relay coil.
     * @param activeLow   true if a LOW signal energises the relay (common for
     *                    blue/red relay modules); false for active-HIGH modules.
     */
    explicit PumpController(int relayPin = PIN_RELAY, bool activeLow = RELAY_ACTIVE_LOW);

    /**
     * @brief Configure the relay GPIO as an output and ensure the pump is off.
     */
    void init();

    /**
     * @brief Attempt to energise the relay (turn the pump on).
     *
     * The request is silently ignored when:
     *  - The minimum off-interval has not elapsed since the last stop.
     *
     * @param maxRunTimeS  Hard-stop timer value in seconds (0 = disabled, use
     *                     with caution). The PumpTask passes the value from
     *                     PumpConfig::maxRunTimeS.
     * @return true if the pump was started; false if blocked by an interlock.
     */
    bool turnOn(uint32_t maxRunTimeS);

    /**
     * @brief De-energise the relay (turn the pump off).
     *        Safe to call even when the pump is already off.
     */
    void turnOff();

    /**
     * @brief Enforce the safety timeout.
     *
     * Must be called on every PumpTask iteration (every PUMP_TASK_PERIOD_MS).
     * Automatically calls turnOff() if the pump has been running longer than
     * the configured maximum duration.
     */
    void tick();

    /**
     * @brief Return true if the relay is currently energised.
     */
    bool isRunning() const { return m_running; }

    /**
     * @brief Elapsed run time in milliseconds (0 when pump is off).
     */
    uint32_t runningForMs() const;

private:
    /**
     * @brief Write the relay output pin, accounting for active-LOW polarity.
     * @param on true to energise the coil; false to de-energise it.
     */
    void writeRelay(bool on);

    int      m_pin;
    bool     m_activeLow;
    bool     m_running       = false;
    uint32_t m_startedAtMs   = 0;
    uint32_t m_lastOffAtMs   = 0;
    uint32_t m_maxRunTimeMs  = PUMP_DEFAULT_TIMEOUT_S * 1000;
};
