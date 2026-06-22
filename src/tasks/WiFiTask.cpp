/**
 * @file WiFiTask.cpp
 * @brief WiFiTask implementation – station mode + captive-portal provisioning.
 */

#include "WiFiTask.h"
#include "../data/SharedData.h"
#include "../config.h"
#include <WiFi.h>
#include <ESPmDNS.h>     // mDNS: dashboard reachable at http://greenhouse.local
#include <DNSServer.h>
#include <WebServer.h>   // Lightweight synchronous server for the provision page only
#include <Preferences.h>
#include <esp_log.h>
#include <sys/time.h>

static const char* TAG = "WiFiTask";

// ─────────────────────────────────────────────────────────────────────────────
// Provisioning page HTML (tiny – just a WiFi form)
// ─────────────────────────────────────────────────────────────────────────────
static const char PROVISION_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Greenhouse Setup</title>
  <style>
    *{box-sizing:border-box;margin:0;padding:0}
    body{font-family:sans-serif;background:#0f172a;color:#e2e8f0;
         display:flex;align-items:center;justify-content:center;min-height:100vh}
    .card{background:#1e293b;border-radius:12px;padding:2rem;width:90%;max-width:380px}
    h1{color:#22c55e;margin-bottom:1.5rem;font-size:1.4rem}
    label{display:block;margin-bottom:.3rem;font-size:.85rem;color:#94a3b8}
    input{width:100%;padding:.7rem;border:1px solid #334155;border-radius:8px;
          background:#0f172a;color:#e2e8f0;font-size:1rem;margin-bottom:1rem}
    button{width:100%;padding:.8rem;background:#22c55e;color:#0f172a;
           border:none;border-radius:8px;font-size:1rem;font-weight:700;cursor:pointer}
    .note{margin-top:1rem;font-size:.78rem;color:#64748b;text-align:center}
  </style>
</head>
<body>
<div class="card">
  <h1>🌿 Greenhouse Setup</h1>
  <form method="POST" action="/save">
    <label>WiFi Network (SSID)</label>
    <input type="text" name="ssid" placeholder="Your WiFi name" required>
    <label>Password</label>
    <input type="password" name="password" placeholder="WiFi password">
    <button type="submit">Save &amp; Connect</button>
  </form>
  <p class="note">The device will restart and connect to your network.</p>
</div>
</body>
</html>
)rawhtml";

static const char SAVED_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html>
<head><meta charset="utf-8"><title>Saved</title>
<style>body{font-family:sans-serif;background:#0f172a;color:#e2e8f0;
  display:flex;align-items:center;justify-content:center;min-height:100vh}
.card{background:#1e293b;border-radius:12px;padding:2rem;text-align:center}
h1{color:#22c55e;margin-bottom:1rem}</style></head>
<body><div class="card"><h1>✓ Credentials Saved</h1>
<p>Restarting now…</p></div></body></html>
)rawhtml";

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

void WiFiTask::start() {
    xTaskCreatePinnedToCore(
        taskFunc,
        "WiFiTask",
        WIFI_TASK_STACK,
        nullptr,
        WIFI_TASK_PRIORITY,
        nullptr,
        1   // Core 1 – shared with AsyncTCP
    );
}

// ─────────────────────────────────────────────────────────────────────────────
// Private helpers
// ─────────────────────────────────────────────────────────────────────────────

bool WiFiTask::hasStoredCredentials(String& ssid, String& password) {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) return false;
    ssid     = prefs.getString(NVS_KEY_SSID, "");
    password = prefs.getString(NVS_KEY_PASS, "");
    prefs.end();
    return ssid.length() > 0;
}

bool WiFiTask::connectStation(const String& ssid, const String& password) {
    ESP_LOGI(TAG, "Connecting to \"%s\" …", ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    uint32_t deadline = millis() + WIFI_CONNECT_TIMEOUT_MS;
    while (WiFi.status() != WL_CONNECTED && millis() < deadline) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    if (WiFi.status() == WL_CONNECTED) {
        ESP_LOGI(TAG, "Connected – IP: %s", WiFi.localIP().toString().c_str());
        return true;
    }
    ESP_LOGW(TAG, "Connection timed out");
    return false;
}

void WiFiTask::runProvisioningAP() {
    String apSsid = WIFI_AP_SSID;
    String apPass = WIFI_AP_PASSWORD;
    {
        Preferences prefs;
        if (prefs.begin(NVS_NAMESPACE, true)) {
            apSsid = prefs.getString(NVS_KEY_AP_SSID, apSsid);
            apPass = prefs.getString(NVS_KEY_AP_PASS,  apPass);
            prefs.end();
        }
    }
    ESP_LOGI(TAG, "Starting provisioning AP: %s", apSsid.c_str());
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSsid.c_str(), apPass.length() ? apPass.c_str() : nullptr);

    // DNS server: redirect all queries to our IP (captive portal)
    static DNSServer dns;
    dns.start(53, "*", WiFi.softAPIP());

    // Lightweight synchronous web server for the provisioning form only
    static WebServer server(80);

    server.on("/", HTTP_GET, [&]() {
        server.send_P(200, "text/html", PROVISION_HTML);
    });

    // Captive portal redirect for common OS probes
    auto captiveRedirect = [&]() {
        server.sendHeader("Location", "http://192.168.4.1/", true);
        server.send(302, "text/plain", "");
    };
    server.on("/generate_204",        HTTP_GET, captiveRedirect);
    server.on("/hotspot-detect.html", HTTP_GET, captiveRedirect);
    server.on("/fwlink",              HTTP_GET, captiveRedirect);
    server.onNotFound([&]() { captiveRedirect(); });

    server.on("/save", HTTP_POST, [&]() {
        String ssid     = server.arg("ssid");
        String password = server.arg("password");

        if (ssid.isEmpty()) {
            server.send(400, "text/plain", "SSID required");
            return;
        }
        Preferences prefs;
        if (prefs.begin(NVS_NAMESPACE, false)) {
            prefs.putString(NVS_KEY_SSID, ssid);
            prefs.putString(NVS_KEY_PASS, password);
            prefs.end();
        }
        server.send_P(200, "text/html", SAVED_HTML);
        vTaskDelay(pdMS_TO_TICKS(2000));
        ESP_LOGI(TAG, "Credentials saved – restarting");
        esp_restart();
    });

    server.begin();
    ESP_LOGI(TAG, "Provisioning server ready at http://192.168.4.1");

    // Serve requests forever (esp_restart() exits this loop)
    for (;;) {
        dns.processNextRequest();
        server.handleClient();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Task entry point
// ─────────────────────────────────────────────────────────────────────────────

void WiFiTask::taskFunc(void* /*param*/) {
    SharedData& sd = SharedData::instance();

    String ssid, password;

    if (hasStoredCredentials(ssid, password) && connectStation(ssid, password)) {
        // ── Station mode: sync NTP, then start mDNS ──────────────────────────
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");
        ESP_LOGI(TAG, "NTP sync started");
        for (int i = 0; i < 30; i++) {  // wait up to 15 s
            struct timeval tv;
            gettimeofday(&tv, nullptr);
            if (tv.tv_sec > 1577836800L) {
                struct tm ti;
                gmtime_r(&tv.tv_sec, &ti);
                ESP_LOGI(TAG, "NTP synced: %04d-%02d-%02d %02d:%02d:%02d UTC",
                         ti.tm_year+1900, ti.tm_mon+1, ti.tm_mday,
                         ti.tm_hour, ti.tm_min, ti.tm_sec);
                xEventGroupSetBits(sd.events, EVT_TIME_SYNCED);
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        if (MDNS.begin("greenhouse")) {
            MDNS.addService("http", "tcp", 80);
            ESP_LOGI(TAG, "mDNS started – dashboard at http://greenhouse.local/");
        } else {
            ESP_LOGW(TAG, "mDNS failed to start");
        }

        // Flag must be set BEFORE the event bit so WebTask reads it correctly
        sd.provisioningMode = false;
        xEventGroupSetBits(sd.events, EVT_WIFI_READY);

        Serial.printf("\n*** Dashboard: http://%s/  or  http://greenhouse.local/ ***\n\n",
                      WiFi.localIP().toString().c_str());

        // Monitor for disconnection and reconnect automatically
        for (;;) {
            if (WiFi.status() != WL_CONNECTED) {
                ESP_LOGW(TAG, "Lost connection – reconnecting…");
                xEventGroupClearBits(sd.events, EVT_WIFI_READY);
                if (connectStation(ssid, password)) {
                    xEventGroupSetBits(sd.events, EVT_WIFI_READY);
                    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
                }
            }
            vTaskDelay(pdMS_TO_TICKS(10000));
        }
    } else {
        // ── Provisioning AP mode ─────────────────────────────────────────────
        // Set the flag BEFORE the event bit so WebTask sees the correct state.
        sd.provisioningMode = true;
        xEventGroupSetBits(sd.events, EVT_WIFI_READY);
        runProvisioningAP();  // Blocks until credentials saved → esp_restart()
    }
}
