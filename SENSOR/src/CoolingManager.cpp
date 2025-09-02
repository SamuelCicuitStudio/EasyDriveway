#include "CoolingManager.h"

// ======================= Public API ===========================
bool CoolingManager::begin() {
    if (!_cfg) return false;
    if (!_bme) { logSensorFault("bme_ptr_null"); return false; }

    // Load pins from config
    _pinFanPwm  = _cfg->GetInt(FAN_PWM_PIN_KEY, FAN_PWM_PIN_DEFAULT);

    // Setup HW
    setupPWM();

    // Log init snapshot
    logInit();

    // Create RTOS task
    if (_task == nullptr) {
        BaseType_t ok = xTaskCreatePinnedToCore(
            &CoolingManager::taskThunk, "CoolingTask",
            COOLING_TASK_STACK, this,
            COOLING_TASK_PRIORITY,
            &_task, COOLING_TASK_CORE
        );
        if (ok != pdPASS) {
            logSensorFault("task_create_failed");
            return false;
        }
    }
    return true;
}

void CoolingManager::end() {
    if (_task) {
        TaskHandle_t t = _task;
        _task = nullptr;
        vTaskDelete(t);
    }
    writeFanPercent(0);
    _modeApplied = COOL_STOPPED;
}

void CoolingManager::setMode(Mode m) {
    _modeUser = m; // applied on next periodic update
    // If user forces a non-AUTO mode, log the request immediately
    if (m != COOL_AUTO) {
        logModeChange(_modeApplied, m, _lastTempC, (m==COOL_ECO)?_ecoPct:(m==COOL_NORMAL)?_normPct:(m==COOL_FORCED)?_forcePct:0);
    }
}

void CoolingManager::setManualSpeedPct(uint8_t pct) {
    _modeUser = COOL_NORMAL; // semantic: manual override uses NORMAL bucket
    writeFanPercent(pct);
    logModeChange(_modeApplied, COOL_NORMAL, _lastTempC, pct);
}

void CoolingManager::stopFan() {
    _modeUser = COOL_STOPPED;
    writeFanPercent(0);
    logModeChange(_modeApplied, COOL_STOPPED, _lastTempC, 0);
}

void CoolingManager::setThresholds(float ecoOn, float normOn, float forceOn, float hyst) {
    _ecoOnC   = ecoOn;
    _normOnC  = normOn;
    _forceOnC = forceOn;
    _hystC    = (hyst > 0.f ? hyst : COOL_TEMP_HYST_C);
    if (_log) ICM_PWR_INFO((*_log), 1201, "Cooling thresholds set eco=%.1f norm=%.1f force=%.1f hyst=%.1f",
                           _ecoOnC, _normOnC, _forceOnC, _hystC);
}

void CoolingManager::setPresetSpeeds(uint8_t ecoPct, uint8_t normPct, uint8_t forcePct) {
    _ecoPct   = constrain(ecoPct,   0, 100);
    _normPct  = constrain(normPct,  0, 100);
    _forcePct = constrain(forcePct, 0, 100);
    if (_log) ICM_PWR_INFO((*_log), 1202, "Cooling presets eco=%u%% norm=%u%% force=%u%%",
                           _ecoPct, _normPct, _forcePct);
}

// ===================== Task plumbing ==========================
void CoolingManager::taskThunk(void* arg) {
    static_cast<CoolingManager*>(arg)->taskLoop();
}

void CoolingManager::taskLoop() {
    // First immediate run
    periodicUpdate();

    // Then periodic
    const TickType_t period = pdMS_TO_TICKS(COOLING_TASK_PERIOD_MS);
    TickType_t last = xTaskGetTickCount();
    while (_task) {
        vTaskDelayUntil(&last, period);
        if (!_task) break; // allow end() to stop us
        periodicUpdate();
    }
    vTaskDelete(nullptr);
}

// ======================= Private HW ===========================
void CoolingManager::setupPWM() {
#if CONFIG_IDF_TARGET_ESP32 || defined(ESP32)
    ledcSetup(COOLING_LEDC_CHANNEL, COOLING_LEDC_FREQUENCY, COOLING_LEDC_RES_BITS);
    ledcAttachPin(_pinFanPwm, COOLING_LEDC_CHANNEL);
    writeFanPercent(0);
#else
#warning "LEDC not available on this target; add your PWM backend."
    pinMode(_pinFanPwm, OUTPUT);
    digitalWrite(_pinFanPwm, LOW);
#endif
}

// ===================== Control / Logic ========================
void CoolingManager::periodicUpdate() {
    _periodCounter++;

    // Read temperature from BME
    float tC = NAN, rh = NAN, pPa = NAN;
    if (!_bme->read(tC, rh, pPa)) {
        // If no new data, try to use cached values (if any)
        if (!isnan(_lastTempC)) {
            tC = _lastTempC;
            rh = _lastRH;
            pPa = _lastP;
        } else {
            logSensorFault("bme_read_fail");
        }
    }

    // Apply mode logic (may receive NaN -> conservative behavior)
    applyModeCommand(_modeUser, tC);

    // Log temp periodically or when it changes significantly
    logTempIfNeeded(tC);

    _lastTempC = tC;
    _lastRH    = rh;
    _lastP     = pPa;
}

void CoolingManager::applyModeCommand(Mode m, float tC) {
    if (m == COOL_AUTO) {
        applyAutoLogic(tC);
        return;
    }

    // Manual modes ignore temperature
    Mode prev = _modeApplied;
    switch (m) {
        case COOL_STOPPED: writeFanPercent(COOL_SPEED_STOP_PCT); _modeApplied = COOL_STOPPED; break;
        case COOL_ECO:     writeFanPercent(_ecoPct);             _modeApplied = COOL_ECO;     break;
        case COOL_NORMAL:  writeFanPercent(_normPct);            _modeApplied = COOL_NORMAL;  break;
        case COOL_FORCED:  writeFanPercent(_forcePct);           _modeApplied = COOL_FORCED;  break;
        default:           writeFanPercent(0);                   _modeApplied = COOL_STOPPED; break;
    }
    if (_modeApplied != prev) {
        logModeChange(prev, _modeApplied, tC, _lastSpeedPct);
    }
}

void CoolingManager::applyAutoLogic(float tC) {
    Mode prev = _modeApplied;

    // If temperature invalid, be conservative: keep current speed, or stop if we had nothing
    if (isnan(tC)) {
        if (_modeApplied == COOL_STOPPED) writeFanPercent(0);
        return;
    }

    // Hysteresis thresholds for stepping *down*
    float ecoOff   = _ecoOnC   - _hystC;
    float normOff  = _normOnC  - _hystC;
    float forceOff = _forceOnC - _hystC;

    Mode newMode;
    // Decide upward transitions (no hysteresis needed)
    if      (tC >= _forceOnC) newMode = COOL_FORCED;
    else if (tC >= _normOnC)  newMode = COOL_NORMAL;
    else if (tC >= _ecoOnC)   newMode = COOL_ECO;
    else                      newMode = COOL_STOPPED;

    // For downward transitions, require stepping below the *off* threshold
    switch (_modeApplied) {
        case COOL_FORCED: if (tC <= forceOff) newMode = (tC >= _normOnC) ? COOL_NORMAL :
                                                       (tC >= _ecoOnC)  ? COOL_ECO    : COOL_STOPPED; break;
        case COOL_NORMAL: if (tC <= normOff)  newMode = (tC >= _ecoOnC)  ? COOL_ECO    : COOL_STOPPED; break;
        case COOL_ECO:    if (tC <= ecoOff)   newMode = COOL_STOPPED; break;
        default: break;
    }

    // Apply preset speed for selected mode
    switch (newMode) {
        case COOL_STOPPED: writeFanPercent(COOL_SPEED_STOP_PCT); break;
        case COOL_ECO:     writeFanPercent(_ecoPct);             break;
        case COOL_NORMAL:  writeFanPercent(_normPct);            break;
        case COOL_FORCED:  writeFanPercent(_forcePct);           break;
        default:           writeFanPercent(0);                   break;
    }
    _modeApplied = newMode;

    if (_modeApplied != prev) {
        logModeChange(prev, _modeApplied, tC, _lastSpeedPct);
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

// ========================= Logging ============================
void CoolingManager::logInit() {
    if (!_log) return;
    ICM_PWR_INFO((*_log), 1200,
        "Cooling init pwmPin=%d eco=%.1f norm=%.1f force=%.1f hyst=%.1f eco%%=%u norm%%=%u force%%=%u",
        _pinFanPwm,
        _ecoOnC, _normOnC, _forceOnC, _hystC,
        _ecoPct, _normPct, _forcePct);
}

void CoolingManager::logTempIfNeeded(float tC) {
    if (!_log) return;
    bool dueByDelta = (!isnan(_lastLoggedTempC) && !isnan(tC) && fabsf(tC - _lastLoggedTempC) >= COOL_LOG_DELTA_C);
    bool dueByTime  = (_periodCounter % COOL_LOG_MIN_PERIODS) == 0; // e.g. every ~1 minute

    if (isnan(tC)) {
        ICM_EVT((*_log), ICMLogFS::DOM_POWER, ICMLogFS::EV_WARN, 1203, "Temp=NaN mode=%d pct=%u", (int)_modeApplied, _lastSpeedPct);
        _lastLoggedTempC = tC;
        return;
    }

    if (dueByDelta || dueByTime || isnan(_lastLoggedTempC)) {
        ICM_PWR_INFO((*_log), 1204, "Temp=%.2fC RH=%.1f%% P=%.0fPa mode=%d pct=%u",
                     tC, _lastRH, _lastP, (int)_modeApplied, _lastSpeedPct);
        _lastLoggedTempC = tC;
    }
}

void CoolingManager::logModeChange(Mode from, Mode to, float tC, uint8_t pct) {
    if (!_log) return;
    ICM_EVT((*_log), ICMLogFS::DOM_POWER, ICMLogFS::EV_INFO, 1205,
            "ModeChange from=%d to=%d temp=%.2fC pct=%u", (int)from, (int)to, isnan(tC)?-999.0f:tC, pct);
}

void CoolingManager::logSensorFault(const char* what) {
    if (!_log) return;
    ICM_EVT((*_log), ICMLogFS::DOM_POWER, ICMLogFS::EV_WARN, 1206, "Cooling fault: %s", what ? what : "unknown");
}
