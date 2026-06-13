/**
 * @file HistoryTask.cpp
 * @brief HistoryTask implementation.
 */

#include "HistoryTask.h"
#include "../data/SharedData.h"
#include "../config.h"
#include <Arduino.h>
#include <esp_log.h>

static const char* TAG = "HistoryTask";

void HistoryTask::start() {
    xTaskCreatePinnedToCore(
        taskFunc,
        "HistoryTask",
        HISTORY_TASK_STACK,
        nullptr,
        HISTORY_TASK_PRIORITY,
        nullptr,
        0   // Core 0
    );
}

void HistoryTask::taskFunc(void* /*param*/) {
    SharedData& sd = SharedData::instance();

    // Wait until SensorTask has completed at least one read cycle so that
    // the first snapshot is not filled entirely with NAN values.
    xEventGroupWaitBits(sd.events, EVT_SENSORS_INIT, pdFALSE, pdTRUE, portMAX_DELAY);
    // Give SensorTask one period to finish its first read before we snapshot
    vTaskDelay(pdMS_TO_TICKS(SENSOR_TASK_PERIOD_MS + 500));

    TickType_t lastWake = xTaskGetTickCount();
    for (;;) {
        EnvironmentReading env   = sd.getEnvironment();
        LightReading       light = sd.getLight();
        SoilReading        soil  = sd.getSoil();

        HistorySample sample{};
        sample.timestampMs     = millis();
        sample.temperature     = env.valid   ? env.temperature   : NAN;
        sample.humidity        = env.valid   ? env.humidity      : NAN;
        sample.lux             = light.valid ? light.lux         : NAN;
        sample.soilMoisturePct = soil.valid  ? soil.moisturePct  : NAN;

        sd.appendHistory(sample);
        ESP_LOGD(TAG, "Snapshot saved (T=%.1f  H=%.1f  Lux=%.1f  Soil=%.1f)",
                 sample.temperature, sample.humidity,
                 sample.lux, sample.soilMoisturePct);

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(HISTORY_TASK_PERIOD_MS));
    }
}
