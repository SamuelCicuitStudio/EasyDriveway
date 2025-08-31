/**************************************************************
 *  Project : ICM (Interface Control Module)
 *  File    : RTCManager.h  (ESP32 internal RTC only)
 *  Purpose : Drop external I2C RTC (DS3231) usage while preserving
 *            the *same* public API so dependencies don't break.
 *            All timekeeping uses ESP32 system time (time.h).
 *
 *  Notes:
 *  - setWire()/setRTC() become no-ops.
 *  - Alarm/32k/lostPower/temperature features are stubbed.
 *  - DateTime is still available via RTClib for signatures only.
 **************************************************************/
#ifndef RTCMANAGER_H
#define RTCMANAGER_H

#include <Arduino.h>
#include <time.h>
#include <sys/time.h>

// Keep RTClib for the DateTime type used in signatures.
#include <RTClib.h>

#include "Config.h"
#include "ConfigManager.h"

// Forward declarations to keep signatures without depending on hardware libs.
class TwoWire;
class RTC_DS3231;
class ICMLogFS;

class RTCManager {
public:
    explicit RTCManager(ConfigManager* cfg,
                        RTC_DS3231* rtc = nullptr,
                        TwoWire*     wire = nullptr);

    // No-ops (kept for compatibility)
    inline void setWire(TwoWire* /*wire*/)     {}
    inline void setRTC (RTC_DS3231* /*rtc*/)   {}
    void setLogger(ICMLogFS* logger) { _log = logger; }

    // Initialize internal RTC (no I2C). Returns true always.
    bool begin(uint32_t i2cHz = 400000UL);

    // Pins/model kept only for compatibility
    const String& model() const { return _model; }
    int  pinSCL() const { return -1; }
    int  pinSDA() const { return -1; }
    int  pinINT() const { return -1; }
    int  pin32K() const { return -1; }
    int  pinRST() const { return -1; }

    // -------- High-level Time I/O --------
    bool          setUnixTime(unsigned long ts);
    unsigned long getUnixTime();
    bool setRTCTime(int year, int month, int day, int hour, int minute, int second);

    // RTClib-style primitives backed by system time
    DateTime now() const;
    void     adjust(const DateTime& dt);

    // -------- System <-> RTC sync (ESP32) --------
    bool syncSystemFromRTC();   // system already *is* the RTC; returns true
    bool syncRTCFromSystem();   // same

    // -------- Status / utilities (stubs) --------
    bool  lostPower();                              // always false
    float readTemperatureC(bool* ok = nullptr);     // NAN; ok=false

    bool enable32k(bool en);                        // false (unsupported)
    inline bool isEnabled32k() const { return false; }

    // -------- Alarm / SQW stubs --------
    inline bool setAlarm1(const DateTime&, Ds3231Alarm1Mode) { return false; }
    inline bool setAlarm2(const DateTime&, Ds3231Alarm2Mode) { return false; }
    inline DateTime getAlarm1() { return DateTime((uint32_t)0); }
    inline DateTime getAlarm2() { return DateTime((uint32_t)0); }
    inline Ds3231Alarm1Mode getAlarm1Mode() { return DS3231_A1_PerSecond; }
    inline Ds3231Alarm2Mode getAlarm2Mode() { return DS3231_A2_PerMinute; }
    inline void disableAlarm(uint8_t) {}
    inline void clearAlarm(uint8_t) {}
    inline bool alarmFired(uint8_t) { return false; }

    inline Ds3231SqwPinMode readSqwPinMode() { return DS3231_SquareWave1Hz; }
    inline void writeSqwPinMode(Ds3231SqwPinMode) {}

    // -------- Pretty strings --------
    String timeString();            // "HH:MM"
    String dateString();            // "YYYY-MM-DD"
    String iso8601String();         // "YYYY-MM-DDTHH:MM:SS"

private:
    // no pins to load; but keep model string for compatibility
    void  loadPinsFromConfig() {}

private:
    ConfigManager* _cfg  = nullptr;
    ICMLogFS*      _log  = nullptr;

    // Compatibility: keep a model string so UI code showing "RTC model" still compiles.
    String _model = String("ESP32-RTC");

    // Cached strings
    String _cachedTime, _cachedDate, _cachedIso;
};

#endif // RTCMANAGER_H
