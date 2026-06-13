/**
 * @file WiFiTask.h
 * @brief FreeRTOS task that manages WiFi connection and AP-mode provisioning.
 *
 * On first boot (no NVS credentials) or after repeated connection failures,
 * the ESP32 opens a SoftAP named "GreenHouse_Setup". A captive DNS server
 * redirects all queries to 192.168.4.1 where a simple HTML form lets the
 * user enter their home WiFi SSID and password. On submit the credentials are
 * stored in NVS and the device restarts in station mode.
 *
 * Once in station mode the task monitors the connection and automatically
 * reconnects on drop. When the link is stable it sets EVT_WIFI_READY in the
 * system event group so the WebTask can start serving clients.
 */

#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <WString.h>   // Arduino String type

/**
 * @brief WiFi lifecycle management task.
 *
 * @note This task must be started before WebTask because WebTask blocks on
 *       EVT_WIFI_READY.
 */
class WiFiTask {
public:
    /**
     * @brief Create and pin the FreeRTOS task to Core 1.
     *
     * Runs on Core 1 alongside the AsyncTCP stack (configured at build time
     * via -DCONFIG_ASYNC_TCP_RUNNING_CORE=1).
     */
    static void start();

private:
    static void taskFunc(void* param);

    /** @brief True if SSID and password are stored in NVS. */
    static bool hasStoredCredentials(String& ssid, String& password);

    /** @brief Attempt station connection; returns true when IP is assigned. */
    static bool connectStation(const String& ssid, const String& password);

    /** @brief Start SoftAP provisioning mode and block until credentials received. */
    static void runProvisioningAP();
};
