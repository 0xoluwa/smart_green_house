/**
 * @file WebTask.h
 * @brief FreeRTOS task that hosts the async web server and WebSocket broadcaster.
 *
 * Waits for EVT_WIFI_READY before starting. Once the network is available it:
 *  - Serves the embedded dashboard HTML at GET /
 *  - Manages a WebSocket endpoint at /ws
 *  - Pushes a JSON sensor snapshot to all connected clients every
 *    WEB_BROADCAST_PERIOD_MS milliseconds
 *  - Parses incoming WebSocket commands and forwards them to PumpTask via
 *    SharedData::postPumpCommand()
 *  - Handles a GET /history request that returns the full 24-hour ring buffer
 *    as a JSON array (useful on initial page load)
 *
 * ESPAsyncWebServer runs its own internal tasks for I/O; this FreeRTOS task
 * only drives the periodic broadcast loop.
 */

#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <WString.h>   // Arduino String type

/**
 * @brief Async web server and WebSocket broadcast task.
 */
class WebTask {
public:
    /**
     * @brief Create and pin the FreeRTOS task to Core 1.
     *
     * Must be called after WiFiTask::start(), because it blocks on
     * EVT_WIFI_READY before initialising the server.
     */
    static void start();

    /**
     * @brief Serialise the full history ring buffer as a JSON array.
     *
     * Public so the onWsEvent free function can call it on client connect.
     * @return JSON string, or an empty string on allocation failure.
     */
    static String buildHistoryJson();

    /**
     * @brief Push the current ring buffer to all connected WebSocket clients.
     *
     * Called by HistoryTask after it pre-populates the ring buffer from
     * persistent flash logs so clients already open get the historical data.
     */
    static void broadcastHistory();

private:
    static void taskFunc(void* param);

    /**
     * @brief Serialise current SharedData readings into a compact JSON string.
     * @param[out] buf    Destination buffer.
     * @param      bufLen Capacity of @p buf in bytes.
     */
    static void buildLiveJson(char* buf, size_t bufLen);
};
