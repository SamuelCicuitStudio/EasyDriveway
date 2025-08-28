/**************************************************************
 *  Project : ICM (Interface Control Module)
 *  File    : RTCManager.h
 *  Purpose : DS3231 RTC manager using Adafruit RTClib (RTC_DS3231)
 *            - Injected RTC_DS3231* and TwoWire* (e.g., &Wire or &Wire1)
 *            - SDA/SCL/INT/32K/RST pins pulled from ConfigManager
 **************************************************************/
#ifndef RTCMANAGER_H
#define RTCMANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>          // Adafruit RTClib
#include <time.h>
#include <sys/time.h>

#include "Config.h"
#include "ConfigManager.h"

// Forward-declare to avoid circular include
class ICMLogFS;

class RTCManager {
public:
    // Inject dependencies; you can replace later via setters
    explicit RTCManager(ConfigManager* cfg,
                        RTC_DS3231* rtc = nullptr,
                        TwoWire*     wire = &Wire);

    // Replace dependencies at runtime (optional)
    void setWire(TwoWire* wire)       { _wire = wire ? wire : &Wire; }
    void setRTC(RTC_DS3231* rtc)      { _rtc  = rtc; }
    void setLogger(ICMLogFS* logger)  { _log  = logger; }

    // Initialize I2C on pins from ConfigManager and start the DS3231
    bool begin(uint32_t i2cHz = 400000UL);

    // Pins/model (as read from config)
    const String& model() const { return _model; }
    int  pinSCL() const { return _pinSCL; }
    int  pinSDA() const { return _pinSDA; }
    int  pinINT() const { return _pinINT; }
    int  pin32K() const { return _pin32K; }
    int  pinRST() const { return _pinRST; }

    // -------- High-level Time I/O --------
    bool          setUnixTime(unsigned long ts);                  // rtc->adjust(DateTime(ts))
    unsigned long getUnixTime();                                  // rtc->now().unixtime()
    bool setRTCTime(int year, int month, int day, int hour, int minute, int second);

    // Expose RTClib-style primitives if you need direct access
    inline DateTime now() const          { return _rtc ? _rtc->now() : DateTime((uint32_t)0); }
    inline void     adjust(const DateTime& dt) { if (_rtc) _rtc->adjust(dt); }

    // -------- System <-> RTC sync (ESP32) --------
    bool syncSystemFromRTC();                                     // settimeofday() from rtc->now()
    bool syncRTCFromSystem();                                     // rtc->adjust() from getLocalTime()

    // -------- Status / utilities --------
    bool  lostPower();                                            // rtc->lostPower()
    float readTemperatureC(bool* ok = nullptr);                   // rtc->getTemperature()

    // 32 kHz output helper (RTClib already handles this)
    bool enable32k(bool en);                                      // enable/disable 32 kHz via RTClib
    inline bool isEnabled32k() const                              { return _rtc ? _rtc->isEnabled32K() : false; }

    // -------- Alarm / SQW helpers (pass-through to RTClib) --------
    inline bool setAlarm1(const DateTime& dt, Ds3231Alarm1Mode m) { return _rtc ? _rtc->setAlarm1(dt, m) : false; }
    inline bool setAlarm2(const DateTime& dt, Ds3231Alarm2Mode m) { return _rtc ? _rtc->setAlarm2(dt, m) : false; }
    inline DateTime getAlarm1()                                   { return _rtc ? _rtc->getAlarm1() : DateTime((uint32_t)0); }
    inline DateTime getAlarm2()                                   { return _rtc ? _rtc->getAlarm2() : DateTime((uint32_t)0); }
    inline Ds3231Alarm1Mode getAlarm1Mode()                       { return _rtc ? _rtc->getAlarm1Mode() : DS3231_A1_PerSecond; }
    inline Ds3231Alarm2Mode getAlarm2Mode()                       { return _rtc ? _rtc->getAlarm2Mode() : DS3231_A2_PerMinute; }
    inline void disableAlarm(uint8_t n)                           { if (_rtc) _rtc->disableAlarm(n); }
    inline void clearAlarm(uint8_t n)                             { if (_rtc) _rtc->clearAlarm(n); }
    inline bool alarmFired(uint8_t n)                             { return _rtc ? _rtc->alarmFired(n) : false; }

    inline Ds3231SqwPinMode readSqwPinMode()                      { return _rtc ? _rtc->readSqwPinMode() : DS3231_SquareWave1Hz; }
    inline void writeSqwPinMode(Ds3231SqwPinMode m)               { if (_rtc) _rtc->writeSqwPinMode(m); }

    // -------- Pretty strings --------
    String timeString();            // "HH:MM"
    String dateString();            // "YYYY-MM-DD"
    String iso8601String();         // "YYYY-MM-DDTHH:MM:SS"

private:
    void  loadPinsFromConfig();

private:
    ConfigManager* _cfg  = nullptr;
    ICMLogFS*      _log  = nullptr;
    TwoWire*       _wire = &Wire;
    RTC_DS3231*    _rtc  = nullptr;

    // From config (defaults from Config.h)
    String _model = RTC_MODEL_DEFAULT;
    int    _pinSCL = I2C_SCL_PIN_DEFAULT;
    int    _pinSDA = I2C_SDA_PIN_DEFAULT;
    int    _pinINT = RTC_INT_PIN_DEFAULT;
    int    _pin32K = RTC_32K_PIN_DEFAULT;
    int    _pinRST = RTC_RST_PIN_DEFAULT;

    // Cached strings
    String _cachedTime, _cachedDate, _cachedIso;
};

#endif // RTCMANAGER_H
