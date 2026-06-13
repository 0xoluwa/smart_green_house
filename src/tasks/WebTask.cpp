/**
 * @file WebTask.cpp
 * @brief WebTask implementation – async HTTP server + WebSocket broadcaster.
 */

#include "WebTask.h"
#include "../data/SharedData.h"
#include "../config.h"
#include "../web/dashboard_html.h"
#include "../web/calibrate_html.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <cmath>
#include <esp_log.h>

static const char* TAG = "WebTask";

static AsyncWebServer g_server(80);
static AsyncWebSocket g_ws("/ws");

// ─────────────────────────────────────────────────────────────────────────────
// Helper: serialize a float that might be NAN as JSON null or a number
// ─────────────────────────────────────────────────────────────────────────────
static void fmtF(char* out, size_t len, float v, int decimals = 2) {
    if (std::isnan(v)) { strncpy(out, "null", len); out[len - 1] = '\0'; }
    else               { snprintf(out, len, decimals == 1 ? "%.1f" : "%.2f", v); }
}

// ─────────────────────────────────────────────────────────────────────────────
// WebSocket event handler (runs in AsyncTCP context on Core 1)
// ─────────────────────────────────────────────────────────────────────────────
static void onWsEvent(AsyncWebSocket*       /*server*/,
                      AsyncWebSocketClient* client,
                      AwsEventType          type,
                      void*                 arg,
                      uint8_t*              data,
                      size_t                len)
{
    SharedData& sd = SharedData::instance();

    switch (type) {
        case WS_EVT_CONNECT:
            ESP_LOGI(TAG, "WS client #%u connected", client->id());
            {
                String hist = WebTask::buildHistoryJson();
                if (hist.length()) client->text(hist);
            }
            break;

        case WS_EVT_DISCONNECT:
            ESP_LOGI(TAG, "WS client #%u disconnected", client->id());
            break;

        case WS_EVT_DATA: {
            AwsFrameInfo* info = static_cast<AwsFrameInfo*>(arg);
            if (!info->final || info->index != 0 || info->len != len) break;
            if (info->opcode != WS_TEXT) break;

            JsonDocument doc;
            if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
                ESP_LOGW(TAG, "Bad JSON from client #%u", client->id());
                break;
            }
            const char* typeStr = doc["type"];
            if (!typeStr) break;

            // ── Pump commands ─────────────────────────────────────────────────
            PumpCommandMsg msg{};
            bool isPumpCmd = true;
            if      (strcmp(typeStr, "set_auto")      == 0) msg.cmd = PumpCommand::SET_AUTO;
            else if (strcmp(typeStr, "set_manual")    == 0) msg.cmd = PumpCommand::SET_MANUAL;
            else if (strcmp(typeStr, "pump_on")       == 0) msg.cmd = PumpCommand::MANUAL_ON;
            else if (strcmp(typeStr, "pump_off")      == 0) msg.cmd = PumpCommand::MANUAL_OFF;
            else if (strcmp(typeStr, "set_threshold") == 0) {
                msg.cmd = PumpCommand::SET_THRESHOLD;
                msg.param = doc["value"] | PUMP_DEFAULT_THRESHOLD_PCT;
            }
            else if (strcmp(typeStr, "set_timeout")   == 0) {
                msg.cmd = PumpCommand::SET_TIMEOUT;
                msg.param = doc["value"] | static_cast<float>(PUMP_DEFAULT_TIMEOUT_S);
            }
            else { isPumpCmd = false; }

            if (isPumpCmd) { sd.postPumpCommand(msg); break; }

            // ── Soil calibration commands ─────────────────────────────────────
            if (strcmp(typeStr, "capture_dry") == 0) {
                SoilReading soil = sd.getSoil();
                if (soil.valid && soil.rawAdc > 0) {
                    SoilCalibration cal = sd.getSoilCalibration();
                    cal.dryAdc  = soil.rawAdc;
                    cal.updated = true;
                    sd.setSoilCalibration(cal);
                    ESP_LOGI(TAG, "Calibration: dry set to %d", soil.rawAdc);
                }
            }
            else if (strcmp(typeStr, "capture_wet") == 0) {
                SoilReading soil = sd.getSoil();
                if (soil.valid && soil.rawAdc > 0) {
                    SoilCalibration cal = sd.getSoilCalibration();
                    cal.wetAdc  = soil.rawAdc;
                    cal.updated = true;
                    sd.setSoilCalibration(cal);
                    ESP_LOGI(TAG, "Calibration: wet set to %d", soil.rawAdc);
                }
            }
            else if (strcmp(typeStr, "set_soil_cal") == 0) {
                int dry = doc["dry"] | SOIL_ADC_DRY;
                int wet = doc["wet"] | SOIL_ADC_WET;
                SoilCalibration cal{dry, wet, true};
                sd.setSoilCalibration(cal);
                ESP_LOGI(TAG, "Calibration: manual set dry=%d wet=%d", dry, wet);
            }
            else {
                ESP_LOGW(TAG, "Unknown WS command: %s", typeStr);
            }
            break;
        }

        case WS_EVT_ERROR:
            ESP_LOGW(TAG, "WS error on client #%u", client->id());
            break;

        default: break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// JSON builders
// ─────────────────────────────────────────────────────────────────────────────

void WebTask::buildLiveJson(char* buf, size_t bufLen) {
    SharedData& sd = SharedData::instance();

    EnvironmentReading env   = sd.getEnvironment();
    LightReading       light = sd.getLight();
    SoilReading        soil  = sd.getSoil();
    BatteryReading     batt  = sd.getBattery();
    PumpConfig         cfg   = sd.getPumpConfig();
    PumpState          state = sd.getPumpState();
    SoilCalibration    cal   = sd.getSoilCalibration();

    snprintf(buf, bufLen,
        "{"
        "\"type\":\"live\","
        "\"temperature\":%.2f,"
        "\"humidity\":%.2f,"
        "\"lux\":%.1f,"
        "\"soilMoisture\":%.2f,"
        "\"rawSoilAdc\":%d,"
        "\"calDry\":%d,"
        "\"calWet\":%d,"
        "\"batteryV\":%.2f,"
        "\"batteryPct\":%d,"
        "\"batteryLow\":%s,"
        "\"pumpRunning\":%s,"
        "\"pumpAuto\":%s,"
        "\"manualOn\":%s,"
        "\"threshold\":%.1f,"
        "\"timeoutS\":%lu,"
        "\"uptimeMs\":%lu,"
        "\"rssi\":%d,"
        "\"ip\":\"%s\""
        "}",
        env.valid   ? env.temperature  : -999.0f,
        env.valid   ? env.humidity     : -999.0f,
        light.valid ? light.lux        : -999.0f,
        soil.valid  ? soil.moisturePct : -999.0f,
        soil.rawAdc,
        cal.dryAdc,
        cal.wetAdc,
        batt.valid  ? batt.voltageV    :    0.0f,
        batt.valid  ? batt.percent     :      0,
        batt.isLow          ? "true" : "false",
        state.isRunning     ? "true" : "false",
        cfg.autoMode        ? "true" : "false",
        cfg.manualOn        ? "true" : "false",
        cfg.moistureThreshPct,
        cfg.maxRunTimeS,
        millis(),
        WiFi.RSSI(),
        WiFi.localIP().toString().c_str()
    );
}

String WebTask::buildHistoryJson() {
    SharedData& sd = SharedData::instance();

    static HistorySample buf[HISTORY_SIZE];
    int count = 0;
    sd.getHistory(buf, count);

    if (count == 0) {
        return R"({"type":"history","data":[]})";
    }

    // ~72 chars per entry (null fields are 4 chars each, floats are 4-6 chars)
    String json;
    json.reserve(static_cast<size_t>(count) * 72 + 32);
    json = R"({"type":"history","data":[)";

    for (int i = 0; i < count; i++) {
        char te[12], hu[12], lx[12], so[12];
        fmtF(te, sizeof(te), buf[i].temperature);
        fmtF(hu, sizeof(hu), buf[i].humidity);
        fmtF(lx, sizeof(lx), buf[i].lux, 1);
        fmtF(so, sizeof(so), buf[i].soilMoisturePct);

        char entry[96];
        snprintf(entry, sizeof(entry),
            "%s{\"ts\":%lu,\"te\":%s,\"hu\":%s,\"lx\":%s,\"so\":%s}",
            i > 0 ? "," : "",
            buf[i].timestampMs,
            te, hu, lx, so);
        json += entry;
    }
    json += "]}";
    return json;
}

// ─────────────────────────────────────────────────────────────────────────────
// Task entry point
// ─────────────────────────────────────────────────────────────────────────────

void WebTask::start() {
    xTaskCreatePinnedToCore(
        taskFunc, "WebTask", WEB_TASK_STACK,
        nullptr, WEB_TASK_PRIORITY, nullptr,
        1   // Core 1 – same core as AsyncTCP
    );
}

void WebTask::taskFunc(void* /*param*/) {
    SharedData& sd = SharedData::instance();

    xEventGroupWaitBits(sd.events, EVT_WIFI_READY, pdFALSE, pdTRUE, portMAX_DELAY);

    if (sd.provisioningMode) {
        ESP_LOGI(TAG, "Provisioning mode – async server not started");
        vTaskDelete(nullptr);
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(500));  // Let TCP/IP stack settle

    g_ws.onEvent(onWsEvent);
    g_server.addHandler(&g_ws);

    g_server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send_P(200, "text/html", DASHBOARD_HTML);
    });

    g_server.on("/calibrate", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send_P(200, "text/html", CALIBRATE_HTML);
    });

    g_server.on("/history", HTTP_GET, [](AsyncWebServerRequest* req) {
        String json = WebTask::buildHistoryJson();
        req->send(200, "application/json", json);
    });

    g_server.onNotFound([](AsyncWebServerRequest* req) {
        req->send(404, "text/plain", "Not found");
    });

    g_server.begin();
    ESP_LOGI(TAG, "Server ready – http://%s/", WiFi.localIP().toString().c_str());

    static char liveBuf[640];
    TickType_t lastWake = xTaskGetTickCount();

    for (;;) {
        g_ws.cleanupClients();
        if (g_ws.count() > 0) {
            buildLiveJson(liveBuf, sizeof(liveBuf));
            g_ws.textAll(liveBuf);
        }
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(WEB_BROADCAST_PERIOD_MS));
    }
}
