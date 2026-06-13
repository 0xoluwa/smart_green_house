/**
 * @file SharedData.h
 * @brief Thread-safe shared data store for all inter-task communication.
 *
 * Every sensor reading, actuator state, and configuration value lives here.
 * A FreeRTOS mutex guards each structure, so tasks on both cores can safely
 * call getters and setters without data races.
 *
 * Usage:
 * @code
 *   SharedData& sd = SharedData::instance();
 *   EnvironmentReading r = sd.getEnvironment();
 *   if (r.valid) Serial.printf("Temp: %.1f C\n", r.temperature);
 * @endcode
 */

#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <cmath>
#include <cstdint>
#include "config.h"

// ─────────────────────────────────────────────────────────────────────────────
// System event bits  (used with SharedData::events)
// ─────────────────────────────────────────────────────────────────────────────

/** @brief Set by WiFiTask when the network is ready (station connected or AP up). */
constexpr EventBits_t EVT_WIFI_READY   = BIT0;

/** @brief Set by SensorTask after the environment sensor initialises successfully. */
constexpr EventBits_t EVT_SENSORS_INIT = BIT1;

// ─────────────────────────────────────────────────────────────────────────────
// Data structures
// ─────────────────────────────────────────────────────────────────────────────

/** @brief Snapshot from the DHT22 environment sensor. */
struct EnvironmentReading {
    float    temperature  = NAN;  ///< Degrees Celsius; NAN if read failed
    float    humidity     = NAN;  ///< Relative humidity %; NAN if read failed
    bool     valid        = false;
    uint32_t timestampMs  = 0;    ///< millis() at time of read
};

/** @brief Snapshot from the BH1750 ambient light sensor. */
struct LightReading {
    float    lux         = NAN;  ///< Illuminance in lux; NAN if read failed
    bool     valid       = false;
    uint32_t timestampMs = 0;
};

/** @brief Snapshot from the capacitive soil moisture sensor. */
struct SoilReading {
    int      rawAdc       = 0;
    float    moisturePct  = 0.0f;  ///< Calibrated 0–100 %
    bool     valid        = false;
    uint32_t timestampMs  = 0;
};

/**
 * @brief Soil moisture sensor calibration values.
 *
 * Loaded from NVS by SoilMoistureSensor::init(). Updated at runtime from the
 * calibration web page without restarting — SensorTask checks the `updated`
 * flag on every cycle and applies the new values to the sensor driver.
 */
struct SoilCalibration {
    int  dryAdc  = SOIL_ADC_DRY;  ///< Raw ADC reading in completely dry air
    int  wetAdc  = SOIL_ADC_WET;  ///< Raw ADC reading fully submerged in water
    bool updated = false;          ///< SensorTask: apply + clear when true
};

/** @brief Battery state derived from the ADC voltage divider. */
struct BatteryReading {
    float    voltageV    = 0.0f;
    uint8_t  percent     = 0;     ///< State-of-charge 0–100 %
    bool     isLow       = false; ///< True when voltage ≤ BATTERY_VOLTAGE_WARN
    bool     valid       = false;
    uint32_t timestampMs = 0;
};

/**
 * @brief Persistent pump configuration.
 *
 * Loaded from NVS on boot and saved back whenever the dashboard changes a
 * setting. The PumpTask reads this on every control cycle.
 */
struct PumpConfig {
    bool     autoMode          = true;
    float    moistureThreshPct = PUMP_DEFAULT_THRESHOLD_PCT;
    uint32_t maxRunTimeS       = PUMP_DEFAULT_TIMEOUT_S;
    bool     manualOn          = false;  ///< Force-on in manual mode
};

/** @brief Transient pump run-state (not persisted). */
struct PumpState {
    bool     isRunning   = false;
    uint32_t startedAtMs = 0;
    uint32_t lastOffAtMs = 0;
};

/** @brief One history snapshot for the 24-hour trend charts. */
struct HistorySample {
    uint32_t timestampMs     = 0;
    float    temperature     = NAN;
    float    humidity        = NAN;
    float    lux             = NAN;  ///< Ambient light in lux (BH1750)
    float    soilMoisturePct = NAN;
};

// ─────────────────────────────────────────────────────────────────────────────
// Command messages  (WebSocket handler → PumpTask via queue)
// ─────────────────────────────────────────────────────────────────────────────

/** @brief Identifies the action to perform on the pump or its configuration. */
enum class PumpCommand : uint8_t {
    SET_AUTO,       ///< Switch to fully-automatic watering mode
    SET_MANUAL,     ///< Switch to manual mode (operator controls the pump)
    MANUAL_ON,      ///< Force pump on (only honoured in manual mode)
    MANUAL_OFF,     ///< Force pump off  (only honoured in manual mode)
    SET_THRESHOLD,  ///< Update moisture threshold; param = new value (%)
    SET_TIMEOUT,    ///< Update max run time; param = new value (seconds)
};

/** @brief Item placed on the command queue by the WebSocket message handler. */
struct PumpCommandMsg {
    PumpCommand cmd;
    float       param = 0.0f;  ///< Optional numeric payload (threshold / timeout)
};

// ─────────────────────────────────────────────────────────────────────────────
// SharedData singleton
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Process-wide, thread-safe data store.
 *
 * Getters return a private copy of the protected structure; setters atomically
 * replace it. Never pass a raw pointer or reference out of this class.
 */
class SharedData {
public:
    /**
     * @brief Return the process-wide singleton.
     * @return Reference to the single SharedData instance.
     */
    static SharedData& instance();

    // ── Environment ──────────────────────────────────────────────────────────

    /** @brief Retrieve the latest environment reading. */
    EnvironmentReading getEnvironment() const;

    /** @brief Store a new environment reading (called from SensorTask). */
    void setEnvironment(const EnvironmentReading& r);

    // ── Light (BH1750) ────────────────────────────────────────────────────────

    /** @brief Retrieve the latest ambient light reading. */
    LightReading getLight() const;

    /** @brief Store a new light reading (called from SensorTask). */
    void setLight(const LightReading& r);

    // ── Soil calibration ──────────────────────────────────────────────────────

    /** @brief Retrieve the current soil moisture calibration. */
    SoilCalibration getSoilCalibration() const;

    /**
     * @brief Update the soil calibration.
     *
     * Set `cal.updated = true` to signal SensorTask to apply the values on its
     * next cycle and persist them to NVS. SensorTask clears the flag afterward.
     */
    void setSoilCalibration(const SoilCalibration& cal);

    // ── Soil moisture ─────────────────────────────────────────────────────────

    /** @brief Retrieve the latest soil moisture reading. */
    SoilReading getSoil() const;

    /** @brief Store a new soil reading (called from SensorTask). */
    void setSoil(const SoilReading& r);

    // ── Battery ───────────────────────────────────────────────────────────────

    /** @brief Retrieve the latest battery reading. */
    BatteryReading getBattery() const;

    /** @brief Store a new battery reading (called from BatteryTask). */
    void setBattery(const BatteryReading& r);

    // ── Pump configuration ────────────────────────────────────────────────────

    /** @brief Retrieve the current pump configuration. */
    PumpConfig getPumpConfig() const;

    /** @brief Atomically replace the pump configuration. */
    void setPumpConfig(const PumpConfig& cfg);

    // ── Pump state ────────────────────────────────────────────────────────────

    /** @brief Retrieve the current transient pump state. */
    PumpState getPumpState() const;

    /** @brief Update the pump state (called from PumpTask only). */
    void setPumpState(const PumpState& s);

    // ── 24-hour history ring buffer ───────────────────────────────────────────

    /**
     * @brief Append one sample to the circular history buffer.
     *        Overwrites the oldest entry when the buffer is full.
     * @param s Sample to append.
     */
    void appendHistory(const HistorySample& s);

    /**
     * @brief Copy the history buffer (oldest → newest) into a caller array.
     * @param[out] out   Destination array; must hold at least HISTORY_SIZE elements.
     * @param[out] count Number of valid entries written (0–HISTORY_SIZE).
     */
    void getHistory(HistorySample out[], int& count) const;

    // ── Command queue (WebSocket → PumpTask) ──────────────────────────────────

    /**
     * @brief Enqueue a command from any context (e.g. AsyncWebSocket callback).
     * @param msg Command to enqueue.
     * @return true if the queue accepted the item before timing out.
     */
    bool postPumpCommand(const PumpCommandMsg& msg);

    /**
     * @brief Dequeue a command (called from PumpTask).
     * @param[out] msg Filled on success.
     * @param waitMs   Maximum time to block in milliseconds (0 = non-blocking).
     * @return true if a command was dequeued.
     */
    bool receivePumpCommand(PumpCommandMsg& msg, uint32_t waitMs = 0);

    // ── System event group ────────────────────────────────────────────────────

    /**
     * @brief FreeRTOS event group shared by all tasks.
     *
     * Tasks use xEventGroupSetBits() / xEventGroupWaitBits() with the EVT_*
     * constants defined at the top of this header.
     */
    EventGroupHandle_t events;

    /**
     * @brief True when the device is in captive-portal provisioning mode.
     *
     * Set by WiFiTask BEFORE it signals EVT_WIFI_READY so that WebTask can
     * make a deterministic decision without racing on WiFi.getMode().
     * Written once at startup; no mutex needed.
     */
    volatile bool provisioningMode = false;

private:
    SharedData();
    ~SharedData() = default;
    SharedData(const SharedData&)            = delete;
    SharedData& operator=(const SharedData&) = delete;

    // One mutex per structure to minimise contention between tasks
    mutable SemaphoreHandle_t m_envMutex;
    mutable SemaphoreHandle_t m_lightMutex;
    mutable SemaphoreHandle_t m_soilCalMutex;
    mutable SemaphoreHandle_t m_soilMutex;
    mutable SemaphoreHandle_t m_battMutex;
    mutable SemaphoreHandle_t m_pumpCfgMutex;
    mutable SemaphoreHandle_t m_pumpStateMutex;
    mutable SemaphoreHandle_t m_histMutex;

    EnvironmentReading m_env{};
    LightReading       m_light{};
    SoilCalibration    m_soilCal{};
    SoilReading        m_soil{};
    BatteryReading     m_batt{};
    PumpConfig         m_pumpCfg{};
    PumpState          m_pumpState{};

    HistorySample m_history[HISTORY_SIZE]{};
    int           m_histHead  = 0;  ///< Next write index in ring buffer
    int           m_histCount = 0;  ///< Valid entries accumulated (caps at HISTORY_SIZE)

    QueueHandle_t m_cmdQueue;
};
