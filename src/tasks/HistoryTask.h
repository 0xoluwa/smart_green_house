/**
 * @file HistoryTask.h
 * @brief FreeRTOS task that snapshots sensor data into the 24-hour ring buffer.
 *
 * Runs on Core 0 at low priority. Every HISTORY_TASK_PERIOD_MS (default 5 min)
 * it reads the latest values from SharedData and appends a HistorySample to the
 * circular buffer. The WebTask drains this buffer when a client connects or
 * requests a chart update.
 */

#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/**
 * @brief History recording task.
 */
class HistoryTask {
public:
    /**
     * @brief Create and pin the FreeRTOS task to Core 0.
     */
    static void start();

private:
    static void taskFunc(void* param);
};
