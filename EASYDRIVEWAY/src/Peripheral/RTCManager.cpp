/**************************************************************
 *  Project : ICM (Interface Control Module)
 *  File    : RTCManager.cpp
 **************************************************************/
#include "RTCManager.h"
#include "ICMLogFS.h"
#include <stdarg.h>

// ---------------- Tiny logging helpers ----------------
static void _rtclog_info(ICMLogFS* log, int code, const char* fmt, ...) {
    if (!log) return;
    char buf[192];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    log->event(ICMLogFS::DOM_RTC, ICMLogFS::EV_INFO, code, String(buf), "RTC");
}
static void _rtclog_warn(ICMLogFS* log, int code, const char* fmt, ...) {
    if (!log) return;
    char buf[192];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    log->event(ICMLogFS::DOM_RTC, ICMLogFS::EV_WARN, code, String(buf), "RTC");
}
static void _rtclog_err (ICMLogFS* log, int code, const char* fmt, ...) {
    if (!log) return;
    char buf[192];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    log->event(ICMLogFS::DOM_RTC, ICMLogFS::EV_ERROR, code, String(buf), "RTC");
}

// =====================================================================
// Constructor / dependency setters (role-independent)
// =====================================================================
RTCManager::RTCManager(ConfigManager* cfg, RTC_DS3231* rtc, TwoWire* wire)
: _cfg(cfg), _log(nullptr), _wire(wire), _rtc(rtc) {
    loadPinsFromConfig();
}
void RTCManager::setWire(TwoWire* wire)      { _wire = wire; }
void RTCManager::setRTC (RTC_DS3231* rtc)    { _rtc  = rtc;  }
void RTCManager::setLogger(ICMLogFS* logger) { _log  = logger; }

// =====================================================================
// Local TEST MODE (overrides both roles)
// =====================================================================
#if defined(RTC_TESTMODE)

void RTCManager::loadPinsFromConfig() {
    _model  = String("TEST-RTC");
    _pinSCL = _pinSDA = _pinINT = _pin32K = _pinRST = -1;
}

bool RTCManager::begin(uint32_t /*i2cHz*/) {
    // No hardware access; always succeed
    _rtclog_info(_log, 3003, "TESTMODE: RTC ready (stubbed)");
    return true;
}

bool RTCManager::setUnixTime(unsigned long ts) {
    _simUnix = ts;
    _rtclog_info(_log, 3010, "TESTMODE: setUnixTime=%lu", ts);
    return true;
}

unsigned long RTCManager::getUnixTime() {
    return _simUnix;
}

bool RTCManager::setRTCTime(int y, int m, int d, int hh, int mm, int ss) {
    DateTime dt(y, m, d, hh, mm, ss);
    _simUnix = (unsigned long)dt.unixtime();
    _rtclog_info(_log, 3011, "TESTMODE: setRTCTime=%04d-%02d-%02d %02d:%02d:%02d",
                 y, m, d, hh, mm, ss);
    return true;
}

DateTime RTCManager::now() const {
    return DateTime((uint32_t)_simUnix);
}

void RTCManager::adjust(const DateTime& dt) {
    // Mirror RTClib adjust
    const_cast<RTCManager*>(this)->_simUnix = (unsigned long)dt.unixtime();
}

bool RTCManager::syncSystemFromRTC() {
    // Copy simulated RTC -> system
    struct timeval tv{ (time_t)_simUnix, 0 };
    settimeofday(&tv, nullptr);
    _rtclog_info(_log, 3021, "TESTMODE: System time set from simulated RTC");
    return true;
}

bool RTCManager::syncRTCFromSystem() {
    // Copy system -> simulated RTC
    _simUnix = (unsigned long)time(nullptr);
    _rtclog_info(_log, 3022, "TESTMODE: Simulated RTC set from system");
    return true;
}

bool RTCManager::lostPower() { return false; }

bool RTCManager::enable32k(bool en) { _sim32k = en; return true; }
bool RTCManager::isEnabled32k() const { return _sim32k; }

float RTCManager::readTemperatureC(bool* ok) {
    if (ok) *ok = true;
    return 25.0f; // fixed, safe value
}

// ---- Alarms (pure stubs with local mirrors) ----
bool RTCManager::setAlarm1(const DateTime& dt, Ds3231Alarm1Mode m) { _simA1 = dt; _simA1Mode = m; _simA1Fired = false; return true; }
bool RTCManager::setAlarm2(const DateTime& dt, Ds3231Alarm2Mode m) { _simA2 = dt; _simA2Mode = m; _simA2Fired = false; return true; }
DateTime RTCManager::getAlarm1() { return _simA1; }
DateTime RTCManager::getAlarm2() { return _simA2; }
Ds3231Alarm1Mode RTCManager::getAlarm1Mode() { return _simA1Mode; }
Ds3231Alarm2Mode RTCManager::getAlarm2Mode() { return _simA2Mode; }
void RTCManager::disableAlarm(uint8_t n) { if (n==1) {_simA1Mode = DS3231_A1_PerSecond;} else {_simA2Mode = DS3231_A2_PerMinute;} }
void RTCManager::clearAlarm(uint8_t n) { if (n==1) _simA1Fired = false; else _simA2Fired = false; }
bool RTCManager::alarmFired(uint8_t n) { return (n==1) ? _simA1Fired : _simA2Fired; }
Ds3231SqwPinMode RTCManager::readSqwPinMode() { return DS3231_SquareWave1Hz; }
void RTCManager::writeSqwPinMode(Ds3231SqwPinMode /*m*/) {}

String RTCManager::timeString() {
    DateTime n((uint32_t)_simUnix);
    char buf[8]; snprintf(buf, sizeof(buf), "%02d:%02d", n.hour(), n.minute());
    _cachedTime = buf; return _cachedTime;
}
String RTCManager::dateString() {
    DateTime n((uint32_t)_simUnix);
    char buf[16]; snprintf(buf, sizeof(buf), "%04d-%02d-%02d", n.year(), n.month(), n.day());
    _cachedDate = buf; return _cachedDate;
}
String RTCManager::iso8601String() {
    DateTime n((uint32_t)_simUnix);
    char buf[24];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d",
             n.year(), n.month(), n.day(), n.hour(), n.minute(), n.second());
    _cachedIso = buf; return _cachedIso;
}

#else // ====================== !RTC_TESTMODE =============================

// =====================================================================
// Role: ICM (external DS3231 via I2C)
// =====================================================================
#ifdef NVS_ROLE_ICM
#include <Wire.h>
#include <RTClib.h>

void RTCManager::loadPinsFromConfig() {
    if (_cfg) {
        _model  = _cfg->GetString(NVS_RTC_MODEL_KEY, RTC_MODEL_DEFAULT);
        _pinSCL = _cfg->GetInt(NVS_PIN_I2C_SCL, PIN_I2C_SCL_DEFAULT);
        _pinSDA = _cfg->GetInt(NVS_PIN_I2C_SDA, PIN_I2C_SDA_DEFAULT);
        _pinINT = _cfg->GetInt(NVS_PIN_RTC_INT, PIN_RTC_INT_DEFAULT);
        _pin32K = _cfg->GetInt(NVS_PIN_RTC_32K, PIN_RTC_32K_DEFAULT);
        _pinRST = _cfg->GetInt(NVS_PIN_RTC_RST, PIN_RTC_RST_DEFAULT);
    }
}

bool RTCManager::begin(uint32_t i2cHz) {
    if (!_wire) _wire = &Wire;
    _wire->begin(_pinSDA, _pinSCL, i2cHz);

    if (_pin32K >= 0) pinMode(_pin32K, INPUT);
    if (_pinINT >= 0) pinMode(_pinINT, INPUT_PULLUP);
    if (_pinRST >= 0) pinMode(_pinRST, INPUT);

    if (!_rtc) {
        _rtclog_err(_log, 3000, "RTC pointer is null; inject RTC_DS3231* first");
        return false;
    }
    if (!_rtc->begin(_wire)) {
        _rtclog_err(_log, 3001, "RTC_DS3231::begin() failed (SDA=%d SCL=%d hz=%u)",
                    _pinSDA, _pinSCL, (unsigned)i2cHz);
        return false;
    }

    if (lostPower()) {
        _rtclog_warn(_log, 3002, "DS3231 lost power (OSF=1). Time may be invalid.");
    }

    _rtclog_info(_log, 3003, "RTC init OK model=%s SDA=%d SCL=%d INT=%d 32K=%d RST=%d hz=%u",
                 _model.c_str(), _pinSDA, _pinSCL, _pinINT, _pin32K, _pinRST, (unsigned)i2cHz);
    return true;
}

bool RTCManager::setUnixTime(unsigned long ts) {
    if (!_rtc) return false;
    _rtc->adjust(DateTime((uint32_t)ts));
    _rtclog_info(_log, 3010, "RTC set to %lu", ts);
    return true;
}

unsigned long RTCManager::getUnixTime() {
    if (!_rtc) return 0;
    return (unsigned long)_rtc->now().unixtime();
}

bool RTCManager::setRTCTime(int y, int m, int d, int hh, int mm, int ss) {
    if (!_rtc) return false;
    _rtc->adjust(DateTime(y, m, d, hh, mm, ss));
    _rtclog_info(_log, 3011, "RTC set to %04d-%02d-%02d %02d:%02d:%02d", y, m, d, hh, mm, ss);
    return true;
}

DateTime RTCManager::now() const {
    return _rtc ? _rtc->now() : DateTime((uint32_t)0);
}
void RTCManager::adjust(const DateTime& dt) {
    if (_rtc) _rtc->adjust(dt);
}

bool RTCManager::syncSystemFromRTC() {
    unsigned long ts = getUnixTime();
    if (!ts) {
        _rtclog_warn(_log, 3020, "syncSystemFromRTC: RTC returned 0 (unset?)");
        return false;
    }
    struct timeval tv{ (time_t)ts, 0 };
    settimeofday(&tv, nullptr);
    _rtclog_info(_log, 3021, "System time set from RTC: %lu", ts);
    return true;
}

bool RTCManager::syncRTCFromSystem() {
    struct tm tmv{};
    if (getLocalTime(&tmv)) {
        time_t now = mktime(&tmv);
        setUnixTime((unsigned long)now);
        _rtclog_info(_log, 3022, "RTC set from system: %lu", (unsigned long)now);
        return true;
    }
    _rtclog_warn(_log, 3023, "syncRTCFromSystem: getLocalTime() failed");
    return false;
}

bool RTCManager::lostPower() {
    return _rtc ? _rtc->lostPower() : false;
}

bool RTCManager::enable32k(bool en) {
    if (!_rtc) return false;
    if (en) _rtc->enable32K(); else _rtc->disable32K();
    _rtclog_info(_log, 3030, "32kHz output %s", en ? "ENABLED" : "DISABLED");
    return true;
}

bool RTCManager::isEnabled32k() const {
    return _rtc ? _rtc->isEnabled32K() : false;
}

float RTCManager::readTemperatureC(bool* ok) {
    if (!_rtc) { if (ok) *ok = false; return NAN; }
    float t = _rtc->getTemperature();
    if (ok) *ok = true;
    return t;
}

bool RTCManager::setAlarm1(const DateTime& dt, Ds3231Alarm1Mode m) { return _rtc ? _rtc->setAlarm1(dt, m) : false; }
bool RTCManager::setAlarm2(const DateTime& dt, Ds3231Alarm2Mode m) { return _rtc ? _rtc->setAlarm2(dt, m) : false; }
DateTime RTCManager::getAlarm1() { return _rtc ? _rtc->getAlarm1() : DateTime((uint32_t)0); }
DateTime RTCManager::getAlarm2() { return _rtc ? _rtc->getAlarm2() : DateTime((uint32_t)0); }
Ds3231Alarm1Mode RTCManager::getAlarm1Mode() { return _rtc ? _rtc->getAlarm1Mode() : DS3231_A1_PerSecond; }
Ds3231Alarm2Mode RTCManager::getAlarm2Mode() { return _rtc ? _rtc->getAlarm2Mode() : DS3231_A2_PerMinute; }
void RTCManager::disableAlarm(uint8_t n) { if (_rtc) _rtc->disableAlarm(n); }
void RTCManager::clearAlarm(uint8_t n) { if (_rtc) _rtc->clearAlarm(n); }
bool RTCManager::alarmFired(uint8_t n) { return _rtc ? _rtc->alarmFired(n) : false; }

Ds3231SqwPinMode RTCManager::readSqwPinMode() { return _rtc ? _rtc->readSqwPinMode() : DS3231_SquareWave1Hz; }
void RTCManager::writeSqwPinMode(Ds3231SqwPinMode m) { if (_rtc) _rtc->writeSqwPinMode(m); }

String RTCManager::timeString() {
    if (!_rtc) return "UNSET";
    DateTime n = _rtc->now();
    char buf[8]; snprintf(buf, sizeof(buf), "%02d:%02d", n.hour(), n.minute());
    _cachedTime = buf; return _cachedTime;
}
String RTCManager::dateString() {
    if (!_rtc) return "1970-01-01";
    DateTime n = _rtc->now();
    char buf[16]; snprintf(buf, sizeof(buf), "%04d-%02d-%02d", n.year(), n.month(), n.day());
    _cachedDate = buf; return _cachedDate;
}
String RTCManager::iso8601String() {
    if (!_rtc) return "1970-01-01T00:00:00";
    DateTime n = _rtc->now();
    char buf[24];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d",
             n.year(), n.month(), n.day(), n.hour(), n.minute(), n.second());
    _cachedIso = buf; return _cachedIso;
}

#endif // NVS_ROLE_ICM

// =====================================================================
// Role: Non-ICM (SENSOR/RELAY/PMS) — use ESP32 system time
// =====================================================================
#ifndef NVS_ROLE_ICM

void RTCManager::loadPinsFromConfig() {
    // No external RTC; keep sentinel pins and a human-friendly model
    _model  = String("ESP32-RTC");
    _pinSCL = _pinSDA = _pinINT = _pin32K = _pinRST = -1;
}

bool RTCManager::begin(uint32_t /*i2cHz*/) {
    // Nothing to init for internal RTC.
    _rtclog_info(_log, 3003, "Internal ESP32 RTC ready");
    return true;
}

bool RTCManager::setUnixTime(unsigned long ts) {
    struct timeval tv{ (time_t)ts, 0 };
    settimeofday(&tv, nullptr);
    _rtclog_info(_log, 3010, "System time set to %lu", ts);
    return true;
}

unsigned long RTCManager::getUnixTime() {
    return (unsigned long)time(nullptr);
}

bool RTCManager::setRTCTime(int y, int m, int d, int hh, int mm, int ss) {
    struct tm tmv{};
    tmv.tm_year = y - 1900;
    tmv.tm_mon  = m - 1;
    tmv.tm_mday = d;
    tmv.tm_hour = hh;
    tmv.tm_min  = mm;
    tmv.tm_sec  = ss;
    time_t t = mktime(&tmv);
    if (t == (time_t)-1) {
        _rtclog_warn(_log, 3011, "Invalid date/time %04d-%02d-%02d %02d:%02d:%02d", y,m,d,hh,mm,ss);
        return false;
    }
    return setUnixTime((unsigned long)t);
}

DateTime RTCManager::now() const {
    time_t t = time(nullptr);
    return DateTime((uint32_t)t);
}
void RTCManager::adjust(const DateTime& dt) {
    setUnixTime((unsigned long)dt.unixtime());
}

bool RTCManager::syncSystemFromRTC() {
    // System time is the source; nothing special to do.
    _rtclog_info(_log, 3021, "syncSystemFromRTC: internal RTC");
    return true;
}

bool RTCManager::syncRTCFromSystem() {
    struct tm tmv{};
    if (getLocalTime(&tmv)) {
        _rtclog_info(_log, 3022, "syncRTCFromSystem: OK");
        return true;
    }
    _rtclog_warn(_log, 3023, "syncRTCFromSystem: getLocalTime() failed");
    return false;
}

bool RTCManager::lostPower() { return false; }
bool RTCManager::enable32k(bool /*en*/) { return false; }
bool RTCManager::isEnabled32k() const { return false; }

float RTCManager::readTemperatureC(bool* ok) { if (ok) *ok = false; return NAN; }

bool RTCManager::setAlarm1(const DateTime&, Ds3231Alarm1Mode) { return false; }
bool RTCManager::setAlarm2(const DateTime&, Ds3231Alarm2Mode) { return false; }
DateTime RTCManager::getAlarm1() { return DateTime((uint32_t)0); }
DateTime RTCManager::getAlarm2() { return DateTime((uint32_t)0); }
Ds3231Alarm1Mode RTCManager::getAlarm1Mode() { return DS3231_A1_PerSecond; }
Ds3231Alarm2Mode RTCManager::getAlarm2Mode() { return DS3231_A2_PerMinute; }
void RTCManager::disableAlarm(uint8_t) {}
void RTCManager::clearAlarm(uint8_t) {}
bool RTCManager::alarmFired(uint8_t) { return false; }

Ds3231SqwPinMode RTCManager::readSqwPinMode() { return DS3231_SquareWave1Hz; }
void RTCManager::writeSqwPinMode(Ds3231SqwPinMode) {}

String RTCManager::timeString() {
    time_t t = time(nullptr);
    struct tm tmv{}; localtime_r(&t, &tmv);
    char buf[8]; snprintf(buf, sizeof(buf), "%02d:%02d", tmv.tm_hour, tmv.tm_min);
    _cachedTime = buf; return _cachedTime;
}
String RTCManager::dateString() {
    time_t t = time(nullptr);
    struct tm tmv{}; localtime_r(&t, &tmv);
    char buf[16]; snprintf(buf, sizeof(buf), "%04d-%02d-%02d", tmv.tm_year+1900, tmv.tm_mon+1, tmv.tm_mday);
    _cachedDate = buf; return _cachedDate;
}
String RTCManager::iso8601String() {
    time_t t = time(nullptr);
    struct tm tmv{}; localtime_r(&t, &tmv);
    char buf[24];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d",
             tmv.tm_year+1900, tmv.tm_mon+1, tmv.tm_mday,
             tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
    _cachedIso = buf; return _cachedIso;
}

#endif // !NVS_ROLE_ICM

#endif // RTC_TESTMODE
