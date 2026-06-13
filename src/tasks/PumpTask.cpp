/**
 * @file PumpTask.cpp
 * @brief PumpTask implementation.
 */

#include "PumpTask.h"
#include "../data/SharedData.h"
#include "../actuators/PumpController.h"
#include "../config.h"
#include <Arduino.h>
#include <Preferences.h>
#include <esp_log.h>

static const char* TAG = "PumpTask";

void PumpTask::start() {
    xTaskCreatePinnedToCore(
        taskFunc,
        "PumpTask",
        PUMP_TASK_STACK,
        nullptr,
        PUMP_TASK_PRIORITY,
        nullptr,
        0   // Core 0
    );
}

void PumpTask::loadConfigFromNvs(PumpConfig& cfg) {
    Preferences prefs;
    if (prefs.begin(NVS_NAMESPACE, true)) {  // read-only
        cfg.moistureThreshPct = prefs.getFloat(NVS_KEY_THRESHOLD, PUMP_DEFAULT_THRESHOLD_PCT);
        cfg.maxRunTimeS       = prefs.getUInt(NVS_KEY_TIMEOUT,    PUMP_DEFAULT_TIMEOUT_S);
        prefs.end();
        ESP_LOGI(TAG, "Loaded config: threshold=%.1f%%  timeout=%lus",
                 cfg.moistureThreshPct, cfg.maxRunTimeS);
    } else {
        ESP_LOGW(TAG, "NVS open failed – using defaults");
    }
}

void PumpTask::saveConfigToNvs(const PumpConfig& cfg) {
    Preferences prefs;
    if (prefs.begin(NVS_NAMESPACE, false)) {  // read-write
        prefs.putFloat(NVS_KEY_THRESHOLD, cfg.moistureThreshPct);
        prefs.putUInt(NVS_KEY_TIMEOUT,    cfg.maxRunTimeS);
        prefs.end();
        ESP_LOGI(TAG, "Saved config to NVS");
    }
}

void PumpTask::taskFunc(void* /*param*/) {
    SharedData& sd = SharedData::instance();

    // Load persisted pump configuration
    PumpConfig cfg{};
    loadConfigFromNvs(cfg);
    sd.setPumpConfig(cfg);

    PumpController pump;
    pump.init();

    TickType_t lastWake = xTaskGetTickCount();

    for (;;) {
        // ── Process incoming commands from the WebSocket handler ──────────────
        PumpCommandMsg msg{};
        while (sd.receivePumpCommand(msg)) {
            cfg = sd.getPumpConfig();  // Get the latest before modifying

            switch (msg.cmd) {
                case PumpCommand::SET_AUTO:
                    cfg.autoMode  = true;
                    cfg.manualOn  = false;
                    ESP_LOGI(TAG, "Mode → AUTO");
                    break;

                case PumpCommand::SET_MANUAL:
                    cfg.autoMode = false;
                    pump.turnOff();  // Stop any auto-cycle in progress
                    ESP_LOGI(TAG, "Mode → MANUAL");
                    break;

                case PumpCommand::MANUAL_ON:
                    cfg.manualOn = true;
                    ESP_LOGI(TAG, "Manual ON requested");
                    break;

                case PumpCommand::MANUAL_OFF:
                    cfg.manualOn = false;
                    pump.turnOff();
                    ESP_LOGI(TAG, "Manual OFF requested");
                    break;

                case PumpCommand::SET_THRESHOLD:
                    cfg.moistureThreshPct = msg.param;
                    ESP_LOGI(TAG, "Threshold → %.1f%%", msg.param);
                    saveConfigToNvs(cfg);
                    break;

                case PumpCommand::SET_TIMEOUT:
                    cfg.maxRunTimeS = static_cast<uint32_t>(msg.param);
                    ESP_LOGI(TAG, "Timeout → %lu s", cfg.maxRunTimeS);
                    saveConfigToNvs(cfg);
                    break;
            }
            sd.setPumpConfig(cfg);
        }

        // ── Refresh config snapshot ───────────────────────────────────────────
        cfg = sd.getPumpConfig();

        // ── Control logic ─────────────────────────────────────────────────────
        if (cfg.autoMode) {
            SoilReading soil = sd.getSoil();
            if (soil.valid) {
                if (!pump.isRunning() && soil.moisturePct < cfg.moistureThreshPct) {
                    pump.turnOn(cfg.maxRunTimeS);
                } else if (pump.isRunning() && soil.moisturePct >= cfg.moistureThreshPct) {
                    pump.turnOff();
                }
            }
        } else {
            // Manual mode: follow the manualOn flag
            if (cfg.manualOn && !pump.isRunning()) {
                pump.turnOn(cfg.maxRunTimeS);
            } else if (!cfg.manualOn && pump.isRunning()) {
                pump.turnOff();
            }
        }

        // ── Enforce safety timeout ────────────────────────────────────────────
        pump.tick();

        // ── Publish pump state to SharedData ─────────────────────────────────
        PumpState state{};
        state.isRunning   = pump.isRunning();
        state.startedAtMs = pump.isRunning() ? (millis() - pump.runningForMs()) : 0;
        state.lastOffAtMs = 0;  // Not exposed by PumpController directly
        sd.setPumpState(state);

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(PUMP_TASK_PERIOD_MS));
    }
}
