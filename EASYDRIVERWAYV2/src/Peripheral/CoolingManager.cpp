/**************************************************************
 *  Project : EasyDriveway
 *  File    : CoolingManager.cpp
 **************************************************************/

#include "CoolingManager.h"
#include "LogFS.h"
bool CoolingManager::begin() {
    if (!_cfg) return false;
    _pinFanPwm = FAN_PWM_PIN;
#if defined(NVS_ROLE_SENS) || defined(NVS_ROLE_SEMU)
    if (!_bme) { logSensorFault("bme_ptr_null"); return false; }
#else
    if (!_ds)  { logSensorFault("ds18_ptr_null"); return false; }
#endif
    setupPWM();
    if (!setupSensor()) { logSensorFault("sensor_setup_failed"); }
    logInit();
    if (_task == nullptr) {
        BaseType_t ok = xTaskCreatePinnedToCore(
            &CoolingManager::taskThunk, "CoolingTask",
            COOLING_TASK_STACK, this,
            COOLING_TASK_PRIORITY,
            &_task, COOLING_TASK_CORE
        );
        if (ok != pdPASS) { logSensorFault("task_create_failed"); return false; }
    }
    return true;
}
void CoolingManager::end() {
    if (_task) { TaskHandle_t t = _task; _task = nullptr; vTaskDelete(t); }
    writeFanPercent(0);
    _modeApplied = COOL_STOPPED;
}
void CoolingManager::setMode(Mode m) {
    _modeUser = m;
    if (m != COOL_AUTO && _log) {
        _log->eventf(
            LogFS::DOM_POWER, LogFS::EV_INFO, 1205,
            "ModeChange from=%d to=%d temp=%.2fC pct=%u",
            (int)_modeApplied, (int)m,
            isnan(_lastTempC) ? -999.0f : _lastTempC,
            (m==COOL_ECO)?_ecoPct:(m==COOL_NORMAL)?_normPct:(m==COOL_FORCED)?_forcePct:0
        );
    }
}
void CoolingManager::setManualSpeedPct(uint8_t pct) {
    _modeUser = COOL_NORMAL;
    writeFanPercent(pct);
    if (_log) {
        _log->eventf(
            LogFS::DOM_POWER, LogFS::EV_INFO, 1205,
            "ModeChange from=%d to=%d temp=%.2fC pct=%u",
            (int)_modeApplied, (int)COOL_NORMAL,
            isnan(_lastTempC) ? -999.0f : _lastTempC,
            pct
        );
    }
}
void CoolingManager::stopFan() {
    _modeUser = COOL_STOPPED;
    writeFanPercent(0);
    if (_log) {
        _log->eventf(
            LogFS::DOM_POWER, LogFS::EV_INFO, 1205,
            "ModeChange from=%d to=%d temp=%.2fC pct=%u",
            (int)_modeApplied, (int)COOL_STOPPED,
            isnan(_lastTempC) ? -999.0f : _lastTempC,
            0
        );
    }
}
void CoolingManager::setThresholds(float ecoOn, float normOn, float forceOn, float hyst) {
    _ecoOnC   = ecoOn;
    _normOnC  = normOn;
    _forceOnC = forceOn;
    _hystC    = (hyst > 0.f ? hyst : COOL_TEMP_HYST_C);
    if (_log) {
        _log->eventf(
            LogFS::DOM_POWER, LogFS::EV_INFO, 1201,
            "Cooling thresholds set eco=%.1f norm=%.1f force=%.1f hyst=%.1f",
            _ecoOnC, _normOnC, _forceOnC, _hystC
        );
    }
}
void CoolingManager::setPresetSpeeds(uint8_t ecoPct, uint8_t normPct, uint8_t forcePct) {
    _ecoPct   = constrain(ecoPct,   0, 100);
    _normPct  = constrain(normPct,  0, 100);
    _forcePct = constrain(forcePct, 0, 100);
    if (_log) {
        _log->eventf(
            LogFS::DOM_POWER, LogFS::EV_INFO, 1202,
            "Cooling presets eco=%u%% norm=%u%% force=%u%%",
            _ecoPct, _normPct, _forcePct
        );
    }
}
void CoolingManager::taskThunk(void* arg) {
    static_cast<CoolingManager*>(arg)->taskLoop();
}
void CoolingManager::taskLoop() {
    periodicUpdate();
    const TickType_t period = pdMS_TO_TICKS(COOLING_TASK_PERIOD_MS);
    TickType_t last = xTaskGetTickCount();
    while (_task) {
        vTaskDelayUntil(&last, period);
        if (!_task) break;
        periodicUpdate();
    }
    vTaskDelete(nullptr);
}
void CoolingManager::setupPWM() {
#if CONFIG_IDF_TARGET_ESP32 || defined(ESP32)
    ledcSetup(COOLING_LEDC_CHANNEL, COOLING_LEDC_FREQUENCY, COOLING_LEDC_RES_BITS);
    ledcAttachPin(_pinFanPwm, COOLING_LEDC_CHANNEL);
    writeFanPercent(0);
#else
#  warning "LEDC not available on this target; add your PWM backend."
    pinMode(_pinFanPwm, OUTPUT);
    digitalWrite(_pinFanPwm, LOW);
#endif
}
bool CoolingManager::setupSensor() {
#if defined(NVS_ROLE_SENS) || defined(NVS_ROLE_SEMU)
    if (!_bme) return false;
    return true;
#else
    if (!_ds) return false;
    if (!_ds->isReady()) { if (!_ds->begin()) { return false; } }
    return _ds->isReady();
#endif
}
bool CoolingManager::readSensor(float& tC) {
#if defined(NVS_ROLE_SENS) || defined(NVS_ROLE_SEMU)
    float rh = NAN, pPa = NAN;
    if (_bme && _bme->read(tC, rh, pPa)) { _lastRH = rh; _lastP = pPa; return true; }
    return false;
#else
    if (_ds && _ds->isReady()) {
        if (_ds->readTemperature(tC)) {
            if (tC < -55.0f || tC > 125.0f || fabsf(tC - 85.0f) < 0.01f) return false;
            return true;
        }
    }
    return false;
#endif
}
void CoolingManager::periodicUpdate() {
    _periodCounter++;
    float tC = NAN;
    if (!readSensor(tC)) {
        if (!isnan(_lastTempC)) { tC = _lastTempC; }
        else {
#if defined(NVS_ROLE_SENS) || defined(NVS_ROLE_SEMU)
            logSensorFault("bme_read_fail");
#else
            logSensorFault("ds18_read_fail");
#endif
        }
    }
    applyModeCommand(_modeUser, tC);
    logTempIfNeeded(tC);
    _lastTempC = tC;
}
void CoolingManager::applyModeCommand(Mode m, float tC) {
    if (m == COOL_AUTO) { applyAutoLogic(tC); return; }
    Mode prev = _modeApplied;
    switch (m) {
        case COOL_STOPPED: writeFanPercent(COOL_SPEED_STOP_PCT); _modeApplied = COOL_STOPPED; break;
        case COOL_ECO:     writeFanPercent(_ecoPct);             _modeApplied = COOL_ECO;     break;
        case COOL_NORMAL:  writeFanPercent(_normPct);            _modeApplied = COOL_NORMAL;  break;
        case COOL_FORCED:  writeFanPercent(_forcePct);           _modeApplied = COOL_FORCED;  break;
        default:           writeFanPercent(0);                   _modeApplied = COOL_STOPPED; break;
    }
    if (_modeApplied != prev && _log) {
        _log->eventf(
            LogFS::DOM_POWER, LogFS::EV_INFO, 1205,
            "ModeChange from=%d to=%d temp=%.2fC pct=%u",
            (int)prev, (int)_modeApplied,
            isnan(tC) ? -999.0f : tC,
            _lastSpeedPct
        );
    }
}
void CoolingManager::applyAutoLogic(float tC) {
    Mode prev = _modeApplied;
    if (isnan(tC)) { if (_modeApplied == COOL_STOPPED) writeFanPercent(0); return; }
    float ecoOff   = _ecoOnC   - _hystC;
    float normOff  = _normOnC  - _hystC;
    float forceOff = _forceOnC - _hystC;
    Mode newMode;
    if      (tC >= _forceOnC) newMode = COOL_FORCED;
    else if (tC >= _normOnC)  newMode = COOL_NORMAL;
    else if (tC >= _ecoOnC)   newMode = COOL_ECO;
    else                      newMode = COOL_STOPPED;
    switch (_modeApplied) {
        case COOL_FORCED:
            if (tC <= forceOff)
                newMode = (tC >= _normOnC) ? COOL_NORMAL :
                          (tC >= _ecoOnC)  ? COOL_ECO    : COOL_STOPPED;
            break;
        case COOL_NORMAL:
            if (tC <= normOff)
                newMode = (tC >= _ecoOnC)  ? COOL_ECO : COOL_STOPPED;
            break;
        case COOL_ECO:
            if (tC <= ecoOff) newMode = COOL_STOPPED;
            break;
        default: break;
    }
    switch (newMode) {
        case COOL_STOPPED: writeFanPercent(COOL_SPEED_STOP_PCT); break;
        case COOL_ECO:     writeFanPercent(_ecoPct);             break;
        case COOL_NORMAL:  writeFanPercent(_normPct);            break;
        case COOL_FORCED:  writeFanPercent(_forcePct);           break;
        default:           writeFanPercent(0);                   break;
    }
    _modeApplied = newMode;
    if (_modeApplied != prev && _log) {
        _log->eventf(
            LogFS::DOM_POWER, LogFS::EV_INFO, 1205,
            "ModeChange from=%d to=%d temp=%.2fC pct=%u",
            (int)prev, (int)_modeApplied, tC, _lastSpeedPct
        );
    }
}
void CoolingManager::writeFanPercent(uint8_t pct) {
    pct = constrain(pct, 0, 100);
#if CONFIG_IDF_TARGET_ESP32 || defined(ESP32)
    ledcWrite(COOLING_LEDC_CHANNEL, pctToDuty(pct));
#else
    analogWrite(_pinFanPwm, map(pct, 0, 100, 0, 255));
#endif
    _lastSpeedPct = pct;
}
void CoolingManager::logInit() {
    if (!_log) return;
#if defined(NVS_ROLE_SENS) || defined(NVS_ROLE_SEMU)
    _log->eventf(
        LogFS::DOM_POWER, LogFS::EV_INFO, 1200,
        "Cooling init (SENS-like) pwmPin=%d eco=%.1f norm=%.1f force=%.1f hyst=%.1f eco%%=%u norm%%=%u force%%=%u",
        _pinFanPwm,
        _ecoOnC, _normOnC, _forceOnC, _hystC,
        _ecoPct, _normPct, _forcePct
    );
#else
    _log->eventf(
        LogFS::DOM_POWER, LogFS::EV_INFO, 1200,
        "Cooling init (DS18) pwmPin=%d eco=%.1f norm=%.1f force=%.1f hyst=%.1f eco%%=%u norm%%=%u force%%=%u",
        _pinFanPwm,
        _ecoOnC, _normOnC, _forceOnC, _hystC,
        _ecoPct, _normPct, _forcePct
    );
#endif
}
void CoolingManager::logTempIfNeeded(float tC) {
    if (!_log) return;
    bool dueByDelta = (!isnan(_lastLoggedTempC) && !isnan(tC) && fabsf(tC - _lastLoggedTempC) >= COOL_LOG_DELTA_C);
    bool dueByTime  = (_periodCounter % COOL_LOG_MIN_PERIODS) == 0;
    if (isnan(tC)) {
        _log->eventf(LogFS::DOM_POWER, LogFS::EV_WARN, 1203, "Temp=NaN mode=%d pct=%u", (int)_modeApplied, _lastSpeedPct);
        _lastLoggedTempC = tC;
        return;
    }
#if defined(NVS_ROLE_SENS) || defined(NVS_ROLE_SEMU)
    if (dueByDelta || dueByTime || isnan(_lastLoggedTempC)) {
        _log->eventf(LogFS::DOM_POWER, LogFS::EV_INFO, 1204, "Temp=%.2fC RH=%.1f%% P=%.0fPa mode=%d pct=%u", tC, _lastRH, _lastP, (int)_modeApplied, _lastSpeedPct);
        _lastLoggedTempC = tC;
    }
#else
    if (dueByDelta || dueByTime || isnan(_lastLoggedTempC)) {
        _log->eventf(LogFS::DOM_POWER, LogFS::EV_INFO, 1204, "Temp=%.2fC mode=%d pct=%u", tC, (int)_modeApplied, _lastSpeedPct);
        _lastLoggedTempC = tC;
    }
#endif
}
void CoolingManager::logModeChange(Mode from, Mode to, float tC, uint8_t pct) {
    if (!_log) return;
    _log->eventf(LogFS::DOM_POWER, LogFS::EV_INFO, 1205, "ModeChange from=%d to=%d temp=%.2fC pct=%u", (int)from, (int)to, isnan(tC) ? -999.0f : tC, pct);
}
void CoolingManager::logSensorFault(const char* what) {
    if (!_log) return;
    _log->eventf(LogFS::DOM_POWER, LogFS::EV_WARN, 1206, "Cooling fault: %s", what ? what : "unknown");
}
  