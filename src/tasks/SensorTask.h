/**
 * @file SensorTask.h
 * @brief FreeRTOS task that periodically reads the environment and soil sensors.
 *
 * Runs on Core 0. Reads the active environment sensor (BME280/AHT20/DHT22)
 * and the capacitive soil moisture sensor every SENSOR_TASK_PERIOD_MS
 * milliseconds, then publishes the results to SharedData.
 */

#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/**
 * @brief Sensor polling task.
 *
 * Instantiates the correct environment driver via SensorFactory, initialises
 * both sensors, then enters a periodic read loop. On init failure the task
 * retries indefinitely every 5 s rather than blocking the rest of the system.
 */
class SensorTask {
public:
    /**
     * @brief Create and pin the FreeRTOS task to Core 0.
     *
     * Call once from setup() after I²C has been initialised.
     */
    static void start();

private:
    /** @brief FreeRTOS task function (static trampoline into the instance). */
    static void taskFunc(void* param);
};
