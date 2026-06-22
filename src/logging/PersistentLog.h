#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <ctime>

struct LogStorageInfo {
    size_t usedBytes  = 0;
    size_t totalBytes = 0;
    bool   nearFull   = false;
};

class SharedData; // forward-declare to avoid circular include

class PersistentLog {
public:
    static PersistentLog& instance();

    // Call once from setup() before tasks start
    bool begin();

    // Called by HistoryTask every HISTORY_INTERVAL_S when time is synced
    void appendSample(time_t epoch, float temp, float hum, float lux, float soil);

    // Pre-populate SharedData ring buffer from the last `windowHours` of CSV logs
    void reloadIntoSharedData(SharedData& sd, int windowHours = 8);

    // Storage info (thread-safe via LittleFS internal locking)
    LogStorageInfo getStorageInfo() const;

    // Returns true if a .csv log for dateStr "YYYY-MM-DD" exists
    bool hasDate(const char* dateStr) const;

    // Full LittleFS path for a date string: "/logs/YYYY-MM-DD.csv"
    static String logPath(const char* dateStr);

    // Build JSON for /api/logs endpoint into caller-provided buffer
    // Returns false if buffer too small
    bool buildLogListJson(char* buf, size_t bufLen) const;

private:
    PersistentLog() : m_mutex(nullptr), m_mounted(false) {}

    bool   m_mounted;
    mutable SemaphoreHandle_t m_mutex;

    void        evictOldestIfNeeded();           // call with m_mutex held
    static String  dateFromEpoch(time_t epoch); // "YYYY-MM-DD"
    static time_t  parseIsoUtc(const char* ts); // "YYYY-MM-DDTHH:MM:SS" → epoch
    static bool    isValidDateStr(const char* s);
};
