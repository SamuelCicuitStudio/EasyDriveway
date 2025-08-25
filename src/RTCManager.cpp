/**************************************************************
 *  Project : ICM (Interface Control Module)
 *  File    : RTCManager.cpp
 **************************************************************/
#include "RTCManager.h"
#include "ICMLogFS.h"

// ---- Tiny logging helpers (optional) ----
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

// -----------------------------
// ctor
// -----------------------------
RTCManager::RTCManager(ConfigManager* cfg, RTC_DS3231* rtc, TwoWire* wire)
: _cfg(cfg), _log(nullptr), _wire(wire ? wire : &Wire), _rtc(rtc) {
    loadPinsFromConfig();
}

// -----------------------------
// config loading
// -----------------------------
void RTCManager::loadPinsFromConfig() {
    if (_cfg) {
        _model  = _cfg->GetString(RTC_MODEL_KEY, String(RTC_MODEL_DEFAULT));
        _pinSCL = _cfg->GetInt(I2C_SCL_PIN_KEY, I2C_SCL_PIN_DEFAULT);
        _pinSDA = _cfg->GetInt(I2C_SDA_PIN_KEY, I2C_SDA_PIN_DEFAULT);
        _pinINT = _cfg->GetInt(RTC_INT_PIN_KEY, RTC_INT_PIN_DEFAULT);
        _pin32K = _cfg->GetInt(RTC_32K_PIN_KEY, RTC_32K_PIN_DEFAULT);
        _pinRST = _cfg->GetInt(RTC_RST_PIN_KEY, RTC_RST_PIN_DEFAULT);
    }
}

// -----------------------------
// begin
// -----------------------------
bool RTCManager::begin(uint32_t i2cHz) {
    if (!_wire) _wire = &Wire;
    _wire->begin(_pinSDA, _pinSCL, i2cHz);

    if (_pin32K >= 0) pinMode(_pin32K, INPUT);
    if (_pinINT >= 0) pinMode(_pinINT, INPUT_PULLUP);
    if (_pinRST >= 0) pinMode(_pinRST, INPUT); // typically not driven by MCU

    if (!_rtc) {
        _rtclog_err(_log, 3000, "RTC pointer is null; inject RTC_DS3231* first");
        return false;
    }
    if (!_rtc->begin(_wire)) {
        _rtclog_err(_log, 3001, "RTC_DS3231::begin() failed (SDA=%d SCL=%d hz=%u)",
                    _pinSDA, _pinSCL, (unsigned)i2cHz);
        return false;
    }

    // If lost power, note it (caller may set time after begin)
    if (lostPower()) {
        _rtclog_warn(_log, 3002, "DS3231 lost power (OSF=1). Time may be invalid.");
    }

    _rtclog_info(_log, 3003, "RTC init OK model=%s SDA=%d SCL=%d INT=%d 32K=%d RST=%d hz=%u",
                 _model.c_str(), _pinSDA, _pinSCL, _pinINT, _pin32K, _pinRST, (unsigned)i2cHz);
    return true;
}

// -----------------------------
// Time I/O
// -----------------------------
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

// -----------------------------
// Sync helpers
// -----------------------------
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

// -----------------------------
// Status / utilities
// -----------------------------
bool RTCManager::lostPower() {
    return _rtc ? _rtc->lostPower() : false;
}

bool RTCManager::enable32k(bool en) {
    if (!_rtc) return false;
    if (en) _rtc->enable32K(); else _rtc->disable32K();
    _rtclog_info(_log, 3030, "32kHz output %s", en ? "ENABLED" : "DISABLED");
    return true;
}

float RTCManager::readTemperatureC(bool* ok) {
    if (!_rtc) { if (ok) *ok = false; return NAN; }
    float t = _rtc->getTemperature();
    if (ok) *ok = true;
    return t;
}

// -----------------------------
// Pretty strings
// -----------------------------
String RTCManager::timeString() {
    if (!_rtc) return "UNSET";
    DateTime n = _rtc->now();
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", n.hour(), n.minute());
    _cachedTime = buf;
    return _cachedTime;
}
String RTCManager::dateString() {
    if (!_rtc) return "1970-01-01";
    DateTime n = _rtc->now();
    char buf[16];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", n.year(), n.month(), n.day());
    _cachedDate = buf;
    return _cachedDate;
}
String RTCManager::iso8601String() {
    if (!_rtc) return "1970-01-01T00:00:00";
    DateTime n = _rtc->now();
    char buf[24];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d",
             n.year(), n.month(), n.day(), n.hour(), n.minute(), n.second());
    _cachedIso = buf;
    return _cachedIso;
}
