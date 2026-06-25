#include "PersistentLog.h"
#include "../data/SharedData.h"
#include "../config.h"
#include <LittleFS.h>
#include <esp_log.h>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <vector>

static const char* TAG      = "PersistentLog";
static const char* LOGS_DIR = "/logs";

PersistentLog& PersistentLog::instance() {
    static PersistentLog inst;
    return inst;
}

bool PersistentLog::begin() {
    m_mutex = xSemaphoreCreateMutex();
    if (!LittleFS.begin(true)) {
        ESP_LOGE(TAG, "LittleFS mount failed");
        return false;
    }
    if (!LittleFS.exists(LOGS_DIR)) LittleFS.mkdir(LOGS_DIR);
    m_mounted = true;
    ESP_LOGI(TAG, "LittleFS mounted: %u / %u bytes used",
             (unsigned)LittleFS.usedBytes(), (unsigned)LittleFS.totalBytes());
    return true;
}

String PersistentLog::dateFromEpoch(time_t epoch) {
    struct tm t;
    gmtime_r(&epoch, &t);
    char buf[11];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", t.tm_year+1900, t.tm_mon+1, t.tm_mday);
    return String(buf);
}

String PersistentLog::logPath(const char* dateStr) {
    String p = LOGS_DIR;
    p += '/';
    p += dateStr;
    p += ".csv";
    return p;
}

bool PersistentLog::isValidDateStr(const char* s) {
    if (!s || strlen(s) != 10) return false;
    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) { if (s[i] != '-') return false; }
        else                   { if (s[i] < '0' || s[i] > '9') return false; }
    }
    return true;
}

time_t PersistentLog::parseIsoUtc(const char* ts) {
    int y, mo, d, h, mi, s;
    if (sscanf(ts, "%4d-%2d-%2dT%2d:%2d:%2d", &y, &mo, &d, &h, &mi, &s) != 6) return 0;
    struct tm t = {};
    t.tm_year  = y - 1900;
    t.tm_mon   = mo - 1;
    t.tm_mday  = d;
    t.tm_hour  = h;
    t.tm_min   = mi;
    t.tm_sec   = s;
    t.tm_isdst = 0;
    return mktime(&t);  // ESP32 default TZ is UTC; mktime gives correct UTC epoch
}

void PersistentLog::evictOldestIfNeeded() {
    size_t used  = LittleFS.usedBytes();
    size_t total = LittleFS.totalBytes();
    if (total == 0) return;
    uint8_t pct = (uint8_t)((used * 100UL) / total);
    if (pct < LOG_EVICT_PCT) return;

    File dir = LittleFS.open(LOGS_DIR);
    if (!dir) return;
    String oldest;
    File entry;
    while ((entry = dir.openNextFile())) {
        if (!entry.isDirectory()) {
            String name = String(entry.name());
            if (name.endsWith(".csv") && (oldest.isEmpty() || name < oldest))
                oldest = name;
        }
        entry.close();
    }
    dir.close();

    if (!oldest.isEmpty()) {
        String full = String(LOGS_DIR) + "/" + oldest;
        LittleFS.remove(full);
        ESP_LOGW(TAG, "Storage %u%% full – evicted %s", (unsigned)pct, oldest.c_str());
    }
}

void PersistentLog::appendSample(time_t epoch, float temp, float hum, float lux, float soil) {
    if (!m_mounted) return;
    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(200)) != pdTRUE) return;

    evictOldestIfNeeded();

    String date = dateFromEpoch(epoch);
    String path = logPath(date.c_str());

    struct tm t;
    gmtime_r(&epoch, &t);
    char tsBuf[20];
    snprintf(tsBuf, sizeof(tsBuf), "%04d-%02d-%02dT%02d:%02d:%02d",
             t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);

    bool needHeader = !LittleFS.exists(path);
    File f = LittleFS.open(path, FILE_APPEND);
    if (!f) {
        ESP_LOGE(TAG, "Cannot open %s", path.c_str());
        xSemaphoreGive(m_mutex);
        return;
    }
    if (needHeader) f.println("timestamp,temperature,humidity,lux,soilMoisture");

    char line[80];
    auto fmtF = [](char* b, size_t n, float v, int dp) {
        if (std::isnan(v)) b[0] = '\0';
        else snprintf(b, n, dp == 1 ? "%.1f" : "%.2f", v);
    };
    char te[10], hu[10], lx[10], so[10];
    fmtF(te, sizeof(te), temp, 2);
    fmtF(hu, sizeof(hu), hum,  2);
    fmtF(lx, sizeof(lx), lux,  1);
    fmtF(so, sizeof(so), soil, 2);
    snprintf(line, sizeof(line), "%s,%s,%s,%s,%s\r\n", tsBuf, te, hu, lx, so);
    f.print(line);
    f.close();

    xSemaphoreGive(m_mutex);
}

void PersistentLog::reloadIntoSharedData(SharedData& sd, int windowHours) {
    if (!m_mounted) return;
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    if (tv.tv_sec < 1577836800L) return;

    time_t cutoff    = tv.tv_sec - (time_t)windowHours * 3600L;
    String dateToday  = dateFromEpoch(tv.tv_sec);
    String dateCutoff = dateFromEpoch(cutoff);

    auto readAndLoad = [&](const String& date) {
        String path = logPath(date.c_str());
        File f = LittleFS.open(path);
        if (!f) return;
        f.readStringUntil('\n'); // skip header
        while (f.available()) {
            String line = f.readStringUntil('\n');
            line.trim();
            if (line.isEmpty()) continue;
            int c1 = line.indexOf(',');  if (c1 < 0) continue;
            int c2 = line.indexOf(',', c1+1); if (c2 < 0) continue;
            int c3 = line.indexOf(',', c2+1); if (c3 < 0) continue;
            int c4 = line.indexOf(',', c3+1); if (c4 < 0) continue;
            time_t epoch = parseIsoUtc(line.substring(0, c1).c_str());
            if (epoch < cutoff || epoch > tv.tv_sec) continue;
            HistorySample s{};
            s.epochMs         = (int64_t)epoch * 1000LL;
            String teS = line.substring(c1+1, c2);
            String huS = line.substring(c2+1, c3);
            String lxS = line.substring(c3+1, c4);
            String soS = line.substring(c4+1);
            s.temperature     = teS.length() ? teS.toFloat() : NAN;
            s.humidity        = huS.length() ? huS.toFloat() : NAN;
            s.lux             = lxS.length() ? lxS.toFloat() : NAN;
            s.soilMoisturePct = soS.length() ? soS.toFloat() : NAN;
            sd.appendHistory(s);
        }
        f.close();
    };

    if (dateCutoff != dateToday) readAndLoad(dateCutoff);
    readAndLoad(dateToday);
    ESP_LOGI(TAG, "Ring buffer pre-populated from persistent logs");
}

LogStorageInfo PersistentLog::getStorageInfo() const {
    LogStorageInfo info{};
    if (!m_mounted) return info;
    info.usedBytes  = LittleFS.usedBytes();
    info.totalBytes = LittleFS.totalBytes();
    uint8_t pct = info.totalBytes > 0
        ? (uint8_t)((info.usedBytes * 100UL) / info.totalBytes) : 0;
    info.nearFull = (pct >= LOG_WARN_PCT);
    return info;
}

bool PersistentLog::hasDate(const char* dateStr) const {
    if (!m_mounted || !isValidDateStr(dateStr)) return false;
    return LittleFS.exists(logPath(dateStr));
}

bool PersistentLog::buildLogListJson(char* buf, size_t bufLen) const {
    if (!m_mounted) {
        snprintf(buf, bufLen,
            "{\"usedBytes\":0,\"totalBytes\":0,\"nearFull\":false,\"logs\":[]}");
        return true;
    }
    LogStorageInfo info = getStorageInfo();

    struct LogEntry { String date; size_t size; };
    std::vector<LogEntry> entries;
    File dir = LittleFS.open(LOGS_DIR);
    if (dir) {
        File f;
        while ((f = dir.openNextFile())) {
            if (!f.isDirectory()) {
                String name = String(f.name());
                if (name.endsWith(".csv")) {
                    String date = name.substring(0, name.length() - 4);
                    entries.push_back({date, (size_t)f.size()});
                }
            }
            f.close();
        }
        dir.close();
    }
    std::sort(entries.begin(), entries.end(),
        [](const LogEntry& a, const LogEntry& b) { return a.date > b.date; });

    int w = snprintf(buf, bufLen,
        "{\"usedBytes\":%u,\"totalBytes\":%u,\"nearFull\":%s,\"logs\":[",
        (unsigned)info.usedBytes, (unsigned)info.totalBytes,
        info.nearFull ? "true" : "false");
    for (size_t i = 0; i < entries.size() && w < (int)bufLen - 64; i++) {
        char e[80];
        snprintf(e, sizeof(e), "%s{\"date\":\"%s\",\"size\":%u}",
                 i > 0 ? "," : "", entries[i].date.c_str(), (unsigned)entries[i].size);
        w += snprintf(buf + w, bufLen - w, "%s", e);
    }
    if (w < (int)bufLen - 2) snprintf(buf + w, bufLen - w, "]}");
    return true;
}
