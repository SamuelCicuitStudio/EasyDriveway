#pragma once
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "Config.h"
#include "ConfigManager.h"
#include "ICMLogFS.h"   // <-- for logging

// ==== RTOS task config ====
#ifndef COOLING_TASK_CORE
#define COOLING_TASK_CORE      0
#endif
#ifndef COOLING_TASK_PRIORITY
#define COOLING_TASK_PRIORITY  3
#endif
#ifndef COOLING_TASK_STACK
#define COOLING_TASK_STACK     4096
#endif
#ifndef COOLING_TASK_PERIOD_MS
#define COOLING_TASK_PERIOD_MS 10000   // 10s
#endif

// ==== PWM config (ESP32 LEDC) ====
#ifndef COOLING_LEDC_CHANNEL
#define COOLING_LEDC_CHANNEL   2
#endif
#ifndef COOLING_LEDC_FREQUENCY
#define COOLING_LEDC_FREQUENCY 25000   // 25 kHz typical for fans
#endif
#ifndef COOLING_LEDC_RES_BITS
#define COOLING_LEDC_RES_BITS  10      // 0..1023
#endif

// ==== Thermostat thresholds & behavior ====
#ifndef COOL_TEMP_ECO_ON_C
#define COOL_TEMP_ECO_ON_C     40.0f
#endif
#ifndef COOL_TEMP_NORM_ON_C
#define COOL_TEMP_NORM_ON_C    55.0f
#endif
#ifndef COOL_TEMP_FORCE_ON_C
#define COOL_TEMP_FORCE_ON_C   70.0f
#endif
#ifndef COOL_TEMP_HYST_C
#define COOL_TEMP_HYST_C       3.0f
#endif

// ==== Default preset speeds ====
#ifndef COOL_SPEED_ECO_PCT
#define COOL_SPEED_ECO_PCT     30
#endif
#ifndef COOL_SPEED_NORM_PCT
#define COOL_SPEED_NORM_PCT    60
#endif
#ifndef COOL_SPEED_FORCE_PCT
#define COOL_SPEED_FORCE_PCT   100
#endif
#ifndef COOL_SPEED_STOP_PCT
#define COOL_SPEED_STOP_PCT    0
#endif

// ==== Logging throttles ====
#ifndef COOL_LOG_DELTA_C
#define COOL_LOG_DELTA_C       0.5f     // log temp if changed ≥0.5°C since last log
#endif
#ifndef COOL_LOG_MIN_PERIODS
#define COOL_LOG_MIN_PERIODS   6        // or at least every 6 cycles (~1 minute)
#endif

class CoolingManager {
public:
    enum Mode : uint8_t { COOL_STOPPED=0, COOL_ECO, COOL_NORMAL, COOL_FORCED, COOL_AUTO };

    CoolingManager(ConfigManager* cfg, ICMLogFS* log = nullptr)
        : _cfg(cfg), _log(log) {}

    bool begin();
    void end();

    // Mode / speed control
    void setMode(Mode m);
    void setManualSpeedPct(uint8_t pct);   // also exits AUTO (manual override)
    void stopFan();

    // Config
    void setThresholds(float ecoOn, float normOn, float forceOn, float hyst = COOL_TEMP_HYST_C);
    void setPresetSpeeds(uint8_t ecoPct, uint8_t normPct, uint8_t forcePct);

    // Runtime info
    float lastTempC()     const { return _lastTempC; }
    uint8_t lastSpeedPct()const { return _lastSpeedPct; }
    Mode modeApplied()    const { return _modeApplied; }
    Mode modeRequested()  const { return _modeUser; }

    // Dependency injection after construction
    void setLogger(ICMLogFS* log) { _log = log; }

private:
    // Task plumbing
    static void taskThunk(void* arg);
    void taskLoop();

    // Hardware
    void setupPWM();
    void setupSensor();
    void writeFanPercent(uint8_t pct);
    uint32_t pctToDuty(uint8_t pct) const { return (uint32_t)((_dutyMax * (uint32_t)pct) / 100U); }

    // Control logic
    void periodicUpdate();
    void applyModeCommand(Mode m, float tC);
    void applyAutoLogic(float tC);

    // Logging helpers
    void logInit();
    void logTempIfNeeded(float tC);
    void logModeChange(Mode from, Mode to, float tC, uint8_t pct);
    void logSensorFault(const char* what);

private:
    ConfigManager*  _cfg = nullptr;
    ICMLogFS*       _log = nullptr;

    // Pins / HW
    int _pinFanPwm = FAN_PWM_PIN_DEFAULT;
    int _pinTemp   = TEMP_SENSOR_PIN_DEFAULT;
    bool _tempPullup = TEMP_SENSOR_PULLUP_DEFAULT;

    // Sensor
    OneWire*           _oneWire = nullptr;
    DallasTemperature* _sensors = nullptr;

    // Task
    TaskHandle_t _task = nullptr;

    // Thresholds / presets
    float   _ecoOnC   = COOL_TEMP_ECO_ON_C;
    float   _normOnC  = COOL_TEMP_NORM_ON_C;
    float   _forceOnC = COOL_TEMP_FORCE_ON_C;
    float   _hystC    = COOL_TEMP_HYST_C;
    uint8_t _ecoPct   = COOL_SPEED_ECO_PCT;
    uint8_t _normPct  = COOL_SPEED_NORM_PCT;
    uint8_t _forcePct = COOL_SPEED_FORCE_PCT;

    // State
    Mode    _modeUser    = COOL_AUTO;
    Mode    _modeApplied = COOL_STOPPED;
    float   _lastTempC   = NAN;
    float   _lastLoggedTempC = NAN;
    uint8_t _lastSpeedPct= 0;
    uint32_t _dutyMax    = (1u << COOLING_LEDC_RES_BITS) - 1u;

    // Log throttling
    uint32_t _periodCounter = 0;
};
