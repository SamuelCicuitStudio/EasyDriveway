/**************************************************************
 *  Project : ICM (Interface Control Module)
 *  File    : RTCManager.cpp  (ESP32 internal RTC only)
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
RTCManager::RTCManager(ConfigManager* cfg, RTC_DS3231* /*rtc*/, TwoWire* /*wire*/)
: _cfg(cfg), _log(nullptr) {
    loadPinsFromConfig();
}

// -----------------------------
// begin
// -----------------------------
bool RTCManager::begin(uint32_t /*i2cHz*/) {
    // Nothing to do for internal RTC.
    _rtclog_info(_log, 3003, "Internal ESP32 RTC ready");
    return true;
}

// -----------------------------
// Time I/O
// -----------------------------
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
    struct tm tmv;
    memset(&tmv, 0, sizeof(tmv));
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

// -----------------------------
// Sync helpers
// -----------------------------
bool RTCManager::syncSystemFromRTC() {
    // System time is the single source; nothing to copy from.
    _rtclog_info(_log, 3021, "syncSystemFromRTC: no external RTC; nothing to do");
    return true;
}

bool RTCManager::syncRTCFromSystem() {
    // Same as above; ensure time() is sane (caller may have set via NTP).
    struct tm tmv{};
    if (getLocalTime(&tmv)) {
        _rtclog_info(_log, 3022, "syncRTCFromSystem: OK");
        return true;
    }
    _rtclog_warn(_log, 3023, "syncRTCFromSystem: getLocalTime() failed");
    return false;
}

// -----------------------------
// Status / utilities
// -----------------------------
bool RTCManager::lostPower() {
    // No concept of lost power in internal RTC context
    return false;
}

bool RTCManager::enable32k(bool /*en*/) {
    // Not supported on internal RTC; return false
    return false;
}

float RTCManager::readTemperatureC(bool* ok) {
    if (ok) *ok = false;
    return NAN;
}

// -----------------------------
// Pretty strings
// -----------------------------
String RTCManager::timeString() {
    time_t t = time(nullptr);
    struct tm tmv;
    localtime_r(&t, &tmv);
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", tmv.tm_hour, tmv.tm_min);
    _cachedTime = buf;
    return _cachedTime;
}
String RTCManager::dateString() {
    time_t t = time(nullptr);
    struct tm tmv;
    localtime_r(&t, &tmv);
    char buf[16];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday);
    _cachedDate = buf;
    return _cachedDate;
}
String RTCManager::iso8601String() {
    time_t t = time(nullptr);
    struct tm tmv;
    localtime_r(&t, &tmv);
    char buf[24];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d",
             tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
             tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
    _cachedIso = buf;
    return _cachedIso;
}
