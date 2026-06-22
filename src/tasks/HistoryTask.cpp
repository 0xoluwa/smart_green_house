/**
 * @file HistoryTask.cpp
 * @brief HistoryTask implementation – ring-buffer snapshots + persistent CSV logging.
 */

#include "HistoryTask.h"
#include "WebTask.h"
#include "../data/SharedData.h"
#include "../logging/PersistentLog.h"
#include "../config.h"
#include <Arduino.h>
#include <esp_log.h>
#include <sys/time.h>

static const char* TAG = "HistoryTask";

void HistoryTask::start() {
    xTaskCreatePinnedToCore(taskFunc, "HistoryTask",
        HISTORY_TASK_STACK, nullptr, HISTORY_TASK_PRIORITY, nullptr, 0);
}

void HistoryTask::taskFunc(void* /*param*/) {
    SharedData& sd = SharedData::instance();

    xEventGroupWaitBits(sd.events, EVT_SENSORS_INIT, pdFALSE, pdTRUE, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(SENSOR_TASK_PERIOD_MS + 500));

    bool historyPreloaded = false;
    TickType_t lastWake = xTaskGetTickCount();
    for (;;) {
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        bool timeSynced = (tv.tv_sec > 1577836800L);
        int64_t epochMs = timeSynced
            ? ((int64_t)tv.tv_sec * 1000LL + tv.tv_usec / 1000LL)
            : 0LL;

        // First tick with a valid wall clock: pre-load ring buffer from LittleFS,
        // then re-broadcast to any browser tabs already open (they got empty history).
        if (timeSynced && !historyPreloaded) {
            historyPreloaded = true;
            PersistentLog::instance().reloadIntoSharedData(sd);
            xEventGroupWaitBits(sd.events, EVT_WIFI_READY, pdFALSE, pdTRUE, pdMS_TO_TICKS(5000));
            vTaskDelay(pdMS_TO_TICKS(1500));
            WebTask::broadcastHistory();
        }

        EnvironmentReading env   = sd.getEnvironment();
        LightReading       light = sd.getLight();
        SoilReading        soil  = sd.getSoil();

        HistorySample sample{};
        sample.epochMs         = epochMs;
        sample.temperature     = env.valid   ? env.temperature  : NAN;
        sample.humidity        = env.valid   ? env.humidity     : NAN;
        sample.lux             = light.valid ? light.lux        : NAN;
        sample.soilMoisturePct = soil.valid  ? soil.moisturePct : NAN;

        sd.appendHistory(sample);

        if (timeSynced) {
            PersistentLog::instance().appendSample(
                tv.tv_sec,
                sample.temperature, sample.humidity,
                sample.lux, sample.soilMoisturePct);
        }

        ESP_LOGD(TAG, "Snapshot: T=%.1f H=%.1f Lux=%.1f Soil=%.1f",
                 sample.temperature, sample.humidity, sample.lux, sample.soilMoisturePct);

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(HISTORY_TASK_PERIOD_MS));
    }
}
