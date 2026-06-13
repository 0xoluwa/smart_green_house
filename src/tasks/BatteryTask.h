/**
 * @file BatteryTask.h
 * @brief FreeRTOS task that periodically reads the 3S LiPo battery voltage.
 *
 * Runs on Core 0 at low priority. Reads the ADC every BATTERY_TASK_PERIOD_MS
 * and publishes the result (voltage, SoC %, low-battery flag) to SharedData.
 */

#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/**
 * @brief Battery monitoring task.
 */
class BatteryTask {
public:
    /**
     * @brief Create and pin the FreeRTOS task to Core 0.
     *
     * Call once from setup() after the ADC attenuation has been set.
     */
    static void start();

private:
    static void taskFunc(void* param);
};
