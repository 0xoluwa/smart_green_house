/**
 * @file BatteryTask.cpp
 * @brief BatteryTask implementation.
 */

#include "BatteryTask.h"
#include "../data/SharedData.h"
#include "../sensors/BatterySensor.h"
#include "../config.h"
#include <Arduino.h>
#include <esp_log.h>

static const char* TAG = "BatteryTask";

void BatteryTask::start() {
    xTaskCreatePinnedToCore(
        taskFunc,
        "BatteryTask",
        BATTERY_TASK_STACK,
        nullptr,
        BATTERY_TASK_PRIORITY,
        nullptr,
        0   // Core 0
    );
}

void BatteryTask::taskFunc(void* /*param*/) {
    SharedData& sd = SharedData::instance();
    BatterySensor sensor;
    sensor.init();

    TickType_t lastWake = xTaskGetTickCount();
    for (;;) {
        BatteryData data{};
        bool ok = sensor.read(data);

        BatteryReading reading{};
        reading.voltageV    = data.voltageV;
        reading.percent     = data.percent;
        reading.isLow       = data.isLow;
        reading.valid       = ok;
        reading.timestampMs = millis();
        sd.setBattery(reading);

        if (ok) {
            ESP_LOGI(TAG, "%.2f V  %d%%  %s",
                     data.voltageV, data.percent,
                     data.isLow ? "LOW" : "OK");
        }

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(BATTERY_TASK_PERIOD_MS));
    }
}
