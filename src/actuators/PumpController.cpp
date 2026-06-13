/**
 * @file PumpController.cpp
 * @brief PumpController implementation.
 */

#include "PumpController.h"
#include <Arduino.h>
#include <esp_log.h>

static const char* TAG = "Pump";

PumpController::PumpController(int relayPin, bool activeLow)
    : m_pin(relayPin), m_activeLow(activeLow) {}

void PumpController::init() {
    pinMode(m_pin, OUTPUT);
    writeRelay(false);  // Ensure pump is off at startup
    ESP_LOGI(TAG, "Relay on GPIO %d  active-%s", m_pin, m_activeLow ? "LOW" : "HIGH");
}

bool PumpController::turnOn(uint32_t maxRunTimeS) {
    if (m_running) {
        return true;  // Already running
    }
    // Enforce minimum rest period between cycles
    uint32_t now = millis();
    uint32_t minOffMs = PUMP_MIN_OFF_INTERVAL_S * 1000;
    if (m_lastOffAtMs > 0 && (now - m_lastOffAtMs) < minOffMs) {
        ESP_LOGD(TAG, "Blocked – minimum off-interval not yet elapsed (%lu ms remaining)",
                 minOffMs - (now - m_lastOffAtMs));
        return false;
    }

    m_maxRunTimeMs = maxRunTimeS * 1000;
    m_startedAtMs  = now;
    m_running      = true;
    writeRelay(true);
    ESP_LOGI(TAG, "Pump ON  (timeout %lu s)", maxRunTimeS);
    return true;
}

void PumpController::turnOff() {
    if (!m_running) {
        return;
    }
    writeRelay(false);
    m_lastOffAtMs = millis();
    m_running     = false;
    ESP_LOGI(TAG, "Pump OFF  (ran for %lu ms)", m_lastOffAtMs - m_startedAtMs);
}

void PumpController::tick() {
    if (!m_running) {
        return;
    }
    if (m_maxRunTimeMs > 0 && runningForMs() >= m_maxRunTimeMs) {
        ESP_LOGW(TAG, "Safety timeout reached – forcing pump OFF");
        turnOff();
    }
}

uint32_t PumpController::runningForMs() const {
    return m_running ? (millis() - m_startedAtMs) : 0;
}

void PumpController::writeRelay(bool on) {
    // Active-LOW: coil fires on LOW; active-HIGH: coil fires on HIGH
    digitalWrite(m_pin, (on ^ m_activeLow) ? HIGH : LOW);
}
