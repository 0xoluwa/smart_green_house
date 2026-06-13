/**
 * @file SharedData.cpp
 * @brief Implementation of the thread-safe SharedData singleton.
 */

#include "SharedData.h"
#include <esp_log.h>
#include <cstring>

static const char* TAG = "SharedData";

// ─────────────────────────────────────────────────────────────────────────────
// Singleton
// ─────────────────────────────────────────────────────────────────────────────

SharedData& SharedData::instance() {
    static SharedData inst;
    return inst;
}

SharedData::SharedData() {
    m_envMutex       = xSemaphoreCreateMutex();
    m_lightMutex     = xSemaphoreCreateMutex();
    m_soilCalMutex   = xSemaphoreCreateMutex();
    m_soilMutex      = xSemaphoreCreateMutex();
    m_battMutex      = xSemaphoreCreateMutex();
    m_pumpCfgMutex   = xSemaphoreCreateMutex();
    m_pumpStateMutex = xSemaphoreCreateMutex();
    m_histMutex      = xSemaphoreCreateMutex();
    events           = xEventGroupCreate();
    m_cmdQueue       = xQueueCreate(8, sizeof(PumpCommandMsg));

    if (!m_envMutex || !m_lightMutex || !m_soilCalMutex || !m_soilMutex || !m_battMutex ||
        !m_pumpCfgMutex || !m_pumpStateMutex || !m_histMutex ||
        !events || !m_cmdQueue) {
        ESP_LOGE(TAG, "Failed to allocate FreeRTOS primitives – heap too small?");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: acquire mutex, copy structure, release mutex
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Generic mutex-guarded getter.
 * @tparam T       Type of the stored structure.
 * @param mtx      Mutex that guards @p stored.
 * @param stored   The protected data member.
 * @return A private copy of @p stored.
 */
template <typename T>
static T lockedGet(SemaphoreHandle_t mtx, const T& stored) {
    T copy{};
    if (xSemaphoreTake(mtx, pdMS_TO_TICKS(100)) == pdTRUE) {
        copy = stored;
        xSemaphoreGive(mtx);
    }
    return copy;
}

/**
 * @brief Generic mutex-guarded setter.
 * @tparam T       Type of the stored structure.
 * @param mtx      Mutex that guards @p stored.
 * @param stored   The protected data member.
 * @param value    New value to write.
 */
template <typename T>
static void lockedSet(SemaphoreHandle_t mtx, T& stored, const T& value) {
    if (xSemaphoreTake(mtx, pdMS_TO_TICKS(100)) == pdTRUE) {
        stored = value;
        xSemaphoreGive(mtx);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Environment
// ─────────────────────────────────────────────────────────────────────────────

EnvironmentReading SharedData::getEnvironment() const {
    return lockedGet(m_envMutex, m_env);
}

void SharedData::setEnvironment(const EnvironmentReading& r) {
    lockedSet(m_envMutex, m_env, r);
}

// ─────────────────────────────────────────────────────────────────────────────
// Light
// ─────────────────────────────────────────────────────────────────────────────

LightReading SharedData::getLight() const {
    return lockedGet(m_lightMutex, m_light);
}

void SharedData::setLight(const LightReading& r) {
    lockedSet(m_lightMutex, m_light, r);
}

// ─────────────────────────────────────────────────────────────────────────────
// Soil calibration
// ─────────────────────────────────────────────────────────────────────────────

SoilCalibration SharedData::getSoilCalibration() const {
    return lockedGet(m_soilCalMutex, m_soilCal);
}

void SharedData::setSoilCalibration(const SoilCalibration& cal) {
    lockedSet(m_soilCalMutex, m_soilCal, cal);
}

// ─────────────────────────────────────────────────────────────────────────────
// Soil moisture
// ─────────────────────────────────────────────────────────────────────────────

SoilReading SharedData::getSoil() const {
    return lockedGet(m_soilMutex, m_soil);
}

void SharedData::setSoil(const SoilReading& r) {
    lockedSet(m_soilMutex, m_soil, r);
}

// ─────────────────────────────────────────────────────────────────────────────
// Battery
// ─────────────────────────────────────────────────────────────────────────────

BatteryReading SharedData::getBattery() const {
    return lockedGet(m_battMutex, m_batt);
}

void SharedData::setBattery(const BatteryReading& r) {
    lockedSet(m_battMutex, m_batt, r);
}

// ─────────────────────────────────────────────────────────────────────────────
// Pump configuration
// ─────────────────────────────────────────────────────────────────────────────

PumpConfig SharedData::getPumpConfig() const {
    return lockedGet(m_pumpCfgMutex, m_pumpCfg);
}

void SharedData::setPumpConfig(const PumpConfig& cfg) {
    lockedSet(m_pumpCfgMutex, m_pumpCfg, cfg);
}

// ─────────────────────────────────────────────────────────────────────────────
// Pump state
// ─────────────────────────────────────────────────────────────────────────────

PumpState SharedData::getPumpState() const {
    return lockedGet(m_pumpStateMutex, m_pumpState);
}

void SharedData::setPumpState(const PumpState& s) {
    lockedSet(m_pumpStateMutex, m_pumpState, s);
}

// ─────────────────────────────────────────────────────────────────────────────
// History ring buffer
// ─────────────────────────────────────────────────────────────────────────────

void SharedData::appendHistory(const HistorySample& s) {
    if (xSemaphoreTake(m_histMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        m_history[m_histHead] = s;
        m_histHead = (m_histHead + 1) % HISTORY_SIZE;
        if (m_histCount < HISTORY_SIZE) {
            m_histCount++;
        }
        xSemaphoreGive(m_histMutex);
    }
}

void SharedData::getHistory(HistorySample out[], int& count) const {
    if (xSemaphoreTake(m_histMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        count = m_histCount;
        if (m_histCount == 0) {
            xSemaphoreGive(m_histMutex);
            return;
        }
        // Oldest entry is at (head - count + HISTORY_SIZE) % HISTORY_SIZE
        int start = (m_histHead - m_histCount + HISTORY_SIZE) % HISTORY_SIZE;
        for (int i = 0; i < m_histCount; i++) {
            out[i] = m_history[(start + i) % HISTORY_SIZE];
        }
        xSemaphoreGive(m_histMutex);
    } else {
        count = 0;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Command queue
// ─────────────────────────────────────────────────────────────────────────────

bool SharedData::postPumpCommand(const PumpCommandMsg& msg) {
    return xQueueSend(m_cmdQueue, &msg, pdMS_TO_TICKS(50)) == pdTRUE;
}

bool SharedData::receivePumpCommand(PumpCommandMsg& msg, uint32_t waitMs) {
    return xQueueReceive(m_cmdQueue, &msg, pdMS_TO_TICKS(waitMs)) == pdTRUE;
}
