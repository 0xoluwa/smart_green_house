/**
 * @file PumpTask.h
 * @brief FreeRTOS task that implements the watering control loop.
 *
 * Runs on Core 0 at elevated priority. On each iteration it:
 *  1. Drains the command queue (commands from the WebSocket handler).
 *  2. In auto mode: compares soil moisture to the configured threshold and
 *     starts/stops the pump accordingly.
 *  3. In manual mode: honours the manualOn flag from PumpConfig.
 *  4. Calls PumpController::tick() to enforce the safety timeout.
 *  5. Writes the current pump state back to SharedData.
 *
 * Loading PumpConfig and saving pump state are done atomically through
 * SharedData, so the WebTask can safely update settings mid-cycle.
 */

#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "../data/SharedData.h"

/**
 * @brief Watering control task.
 */
class PumpTask {
public:
    /**
     * @brief Create and pin the FreeRTOS task to Core 0.
     * @param nvs_namespace  NVS namespace for loading/saving PumpConfig.
     *                       Pass nullptr to skip NVS persistence.
     */
    static void start();

private:
    static void taskFunc(void* param);

    /**
     * @brief Load PumpConfig from NVS (or fall back to defaults if not found).
     */
    static void loadConfigFromNvs(PumpConfig& cfg);

    /**
     * @brief Save the relevant PumpConfig fields to NVS.
     * @param cfg  Config to persist.
     */
    static void saveConfigToNvs(const PumpConfig& cfg);
};
