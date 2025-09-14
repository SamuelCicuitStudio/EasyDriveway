/**************************************************************
 *  Project : ICM (Interface Control Module)
 *  File    : RTCManager.h
 *  Purpose : Unified RTC manager with role-based implementation.
 *            - ICM builds (NVS_ROLE_ICM) use DS3231 via RTClib.
 *            - Other roles use ESP32 system time (internal RTC).
 *            - Local test mode (RTC_TESTMODE) returns fixed data.
 *            Public API remains consistent across roles.
 **************************************************************/
#ifndef RTCMANAGER_H
#define RTCMANAGER_H

// ===== Local Test Mode toggle =====
// Uncomment to force stubbed behavior for this module only:
 #define RTC_TESTMODE 1

#include <Arduino.h>
#include <time.h>
#include <sys/time.h>

// Keep RTClib for the DateTime type used in signatures (all roles)
#include <RTClib.h>

#include "Config.h"
#include "ConfigManager.h"

// Forward declarations to keep signatures without heavy deps on non-ICM builds.
class ICMLogFS;
class TwoWire;
class RTC_DS3231;

class RTCManager {
public:
    // Inject dependencies; on non-ICM builds these are ignored.
    explicit RTCManager(ConfigManager* cfg,
                        RTC_DS3231* rtc = nullptr,
                        TwoWire*     wire = nullptr);

    // Replace dependencies at runtime (ICM only; otherwise no-ops)
    void setWire(TwoWire* wire);
    void setRTC (RTC_DS3231* rtc);
    void setLogger(ICMLogFS* logger);

    // Initialize underlying RTC:
    //  - ICM: init I2C and DS3231 using pins from ConfigManager
    //  - Other roles: nothing to do (system time)
    //  - TESTMODE: succeeds immediately (no I2C)
    bool begin(uint32_t i2cHz = 400000UL);

    // Pins/model (ICM: from config; non-ICM/TESTMODE: sentinel values)
    const String& model() const { return _model; }
    int  pinSCL() const { return _pinSCL; }
    int  pinSDA() const { return _pinSDA; }
    int  pinINT() const { return _pinINT; }
    int  pin32K() const { return _pin32K; }
    int  pinRST() const { return _pinRST; }

    // -------- High-level Time I/O --------
    bool          setUnixTime(unsigned long ts);
    unsigned long getUnixTime();
    bool setRTCTime(int year, int month, int day, int hour, int minute, int second);

    // RTClib-style primitives
    DateTime now() const;
    void     adjust(const DateTime& dt);

    // -------- System <-> RTC sync (ESP32) --------
    bool syncSystemFromRTC();   // copy RTC -> system
    bool syncRTCFromSystem();   // copy system -> RTC

    // -------- Status / utilities --------
    bool  lostPower();                              // ICM: real; others: false
    float readTemperatureC(bool* ok = nullptr);     // ICM: DS3231 temp; others: NAN/25.0 (test)

    // 32 kHz output helper
    bool enable32k(bool en);
    bool isEnabled32k() const;

    // -------- Alarm / SQW helpers (ICM), stubs (others) --------
    bool setAlarm1(const DateTime& dt, Ds3231Alarm1Mode m);
    bool setAlarm2(const DateTime& dt, Ds3231Alarm2Mode m);
    DateTime getAlarm1();
    DateTime getAlarm2();
    Ds3231Alarm1Mode getAlarm1Mode();
    Ds3231Alarm2Mode getAlarm2Mode();
    void disableAlarm(uint8_t n);
    void clearAlarm(uint8_t n);
    bool alarmFired(uint8_t n);
    Ds3231SqwPinMode readSqwPinMode();
    void writeSqwPinMode(Ds3231SqwPinMode m);

    // -------- Pretty strings --------
    String timeString();            // "HH:MM"
    String dateString();            // "YYYY-MM-DD"
    String iso8601String();         // "YYYY-MM-DDTHH:MM:SS"

private:
    void  loadPinsFromConfig();     // ICM: read pins/model; others: set sentinels

private:
    ConfigManager* _cfg  = nullptr;
    ICMLogFS*      _log  = nullptr;
    TwoWire*       _wire = nullptr;
    RTC_DS3231*    _rtc  = nullptr;

    // From config (defaults from Config.h). Non-ICM/TESTMODE: sentinel values.
    String _model = String("ESP32-RTC");
    int    _pinSCL = -1;
    int    _pinSDA = -1;
    int    _pinINT = -1;
    int    _pin32K = -1;
    int    _pinRST = -1;

    // Cached strings
    String _cachedTime, _cachedDate, _cachedIso;

    // ===== Simulation state for TESTMODE =====
#if defined(RTC_TESTMODE)
    unsigned long     _simUnix     = 1735689600UL; // 2025-01-01T00:00:00Z
    bool              _sim32k      = false;
    DateTime          _simA1       = DateTime(2025, 1, 1, 0, 0, 0);
    DateTime          _simA2       = DateTime(2025, 1, 1, 0, 0, 0);
    Ds3231Alarm1Mode  _simA1Mode   = DS3231_A1_PerSecond;
    Ds3231Alarm2Mode  _simA2Mode   = DS3231_A2_PerMinute;
    bool              _simA1Fired  = false;
    bool              _simA2Fired  = false;
#endif
};

#endif // RTCMANAGER_H
