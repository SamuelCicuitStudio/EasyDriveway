/**************************************************************
 *  Project     : EasyDriveway
 *  File        : RTCManager.h
 *  Purpose     : Unified RTC manager with role-based implementation.
 *  Author      : Tshibangu Samuel
 *  Role        : Freelance Embedded Systems Engineer
 *  Expertise   : Secure IoT Systems, Embedded C++, RTOS, Control Logic
 *  Contact     : tshibsamuel47@gmail.com
 *  Phone       : +216 54 429 793
 *  Created     : 2025-10-05
 *  Version     : 1.0.0
 **************************************************************/
#ifndef RTCMANAGER_H
#define RTCMANAGER_H

// ===== Local Test Mode toggle =====
// Uncomment to force stubbed behavior for this module only:
// #define RTC_TESTMODE 1

#include <Arduino.h>
#include <time.h>
#include <sys/time.h>
#include <RTClib.h>
#include "Config/RTCConfig.h"
#include "NVS/NVSManager.h"
#include "I2CBusHub.h"
#include "LogFS.h"
#include <stdarg.h>

// Forward declarations (avoid heavy deps in non-ICM builds)
class LogFS;
class TwoWire;
class RTC_DS3231;

/**
 * @class RTCManager
 * @brief Unified RTC abstraction for DS3231 (ICM role) or ESP32 system time (other roles).
 *        TESTMODE provides a stubbed, controllable RTC for local testing.
 *        Public API remains consistent across roles.
 */
class RTCManager {
public:
  /**
   * @brief Construct the RTC manager with optional injected dependencies.
   * @param rtc   Pointer to RTC_DS3231 instance (ICM role); ignored otherwise.
   * @param bus   Optional I2CBusHub to obtain a TwoWire reference if none injected.
   * @param wire  Optional TwoWire bus; used if provided.
   */
  explicit RTCManager(RTC_DS3231* rtc = nullptr, I2CBusHub* bus = nullptr, TwoWire* wire = nullptr);

  /**
   * @brief Set/replace the I2C wire instance (ICM role).
   * @param wire TwoWire to use for RTC communications.
   */
  void setWire(TwoWire* wire);

  /**
   * @brief Set/replace the DS3231 instance (ICM role).
   * @param rtc Pointer to RTC_DS3231.
   */
  void setRTC(RTC_DS3231* rtc);

  /**
   * @brief Set the logger used for RTC events.
   * @param logger Pointer to LogFS logger.
   */
  void setLogger(LogFS* logger);

  /**
   * @brief Initialize the RTC according to role.
   * @return true on success, false otherwise.
   */
  bool begin();

  /**
   * @brief Get configured model name.
   * @return Model string.
   */
  const String& model() const { return _model; }

  /**
   * @brief Get SCL pin (sentinel -1 when unused).
   * @return Pin number or -1.
   */
  int pinSCL() const { return _pinSCL; }

  /**
   * @brief Get SDA pin (sentinel -1 when unused).
   * @return Pin number or -1.
   */
  int pinSDA() const { return _pinSDA; }

  /**
   * @brief Get INT pin (sentinel -1 when unused).
   * @return Pin number or -1.
   */
  int pinINT() const { return _pinINT; }

  /**
   * @brief Get 32k pin (sentinel -1 when unused).
   * @return Pin number or -1.
   */
  int pin32K() const { return _pin32K; }

  /**
   * @brief Get RST pin (sentinel -1 when unused).
   * @return Pin number or -1.
   */
  int pinRST() const { return _pinRST; }

  // -------- High-level Time I/O --------

  /**
   * @brief Set Unix time on the active clock source.
   * @param ts Unix timestamp (seconds).
   * @return true on success.
   */
  bool setUnixTime(unsigned long ts);

  /**
   * @brief Get Unix time from the active clock source.
   * @return Unix timestamp (seconds).
   */
  unsigned long getUnixTime();

  /**
   * @brief Set Y-M-D H:M:S on the active clock source.
   * @param year   Full year (e.g., 2025)
   * @param month  1..12
   * @param day    1..31
   * @param hour   0..23
   * @param minute 0..59
   * @param second 0..59
   * @return true on success.
   */
  bool setRTCTime(int year, int month, int day, int hour, int minute, int second);

  /**
   * @brief Get current time as RTClib DateTime.
   * @return DateTime instance.
   */
  DateTime now() const;

  /**
   * @brief Adjust RTC to a DateTime value.
   * @param dt DateTime to apply.
   */
  void adjust(const DateTime& dt);

  // -------- System <-> RTC sync (ESP32) --------

  /**
   * @brief Copy RTC time to ESP32 system time.
   * @return true on success.
   */
  bool syncSystemFromRTC();

  /**
   * @brief Copy ESP32 system time to RTC.
   * @return true on success.
   */
  bool syncRTCFromSystem();

  // -------- Status / utilities --------

  /**
   * @brief Check if RTC lost power (ICM role).
   * @return true if lost power; false otherwise.
   */
  bool lostPower();

  /**
   * @brief Read DS3231 temperature (ICM) or stub (others).
   * @param ok Optional out flag set true if reading valid.
   * @return Temperature in °C or NAN when unsupported.
   */
  float readTemperatureC(bool* ok = nullptr);

  /**
   * @brief Enable/disable 32 kHz output (ICM role).
   * @param en true to enable, false to disable.
   * @return true on success; false when unsupported.
   */
  bool enable32k(bool en);

  /**
   * @brief Query 32 kHz output state (ICM role).
   * @return true if enabled; false otherwise.
   */
  bool isEnabled32k() const;

  // -------- Alarm / SQW helpers (ICM), stubs (others) --------

  /**
   * @brief Configure Alarm1.
   * @param dt Date/time to set.
   * @param m  Alarm1 mode.
   * @return true on success.
   */
  bool setAlarm1(const DateTime& dt, Ds3231Alarm1Mode m);

  /**
   * @brief Configure Alarm2.
   * @param dt Date/time to set.
   * @param m  Alarm2 mode.
   * @return true on success.
   */
  bool setAlarm2(const DateTime& dt, Ds3231Alarm2Mode m);

  /**
   * @brief Get Alarm1 configured time.
   * @return DateTime representing Alarm1.
   */
  DateTime getAlarm1();

  /**
   * @brief Get Alarm2 configured time.
   * @return DateTime representing Alarm2.
   */
  DateTime getAlarm2();

  /**
   * @brief Get Alarm1 mode.
   * @return Ds3231Alarm1Mode value.
   */
  Ds3231Alarm1Mode getAlarm1Mode();

  /**
   * @brief Get Alarm2 mode.
   * @return Ds3231Alarm2Mode value.
   */
  Ds3231Alarm2Mode getAlarm2Mode();

  /**
   * @brief Disable a given alarm.
   * @param n Alarm index (1 or 2).
   */
  void disableAlarm(uint8_t n);

  /**
   * @brief Clear a given alarm’s fired flag.
   * @param n Alarm index (1 or 2).
   */
  void clearAlarm(uint8_t n);

  /**
   * @brief Check if an alarm fired.
   * @param n Alarm index (1 or 2).
   * @return true if fired; false otherwise.
   */
  bool alarmFired(uint8_t n);

  /**
   * @brief Read SQW pin mode.
   * @return Ds3231SqwPinMode.
   */
  Ds3231SqwPinMode readSqwPinMode();

  /**
   * @brief Write SQW pin mode.
   * @param m Desired mode.
   */
  void writeSqwPinMode(Ds3231SqwPinMode m);

  // -------- Pretty strings --------

  /**
   * @brief Get "HH:MM" formatted time string.
   * @return String with time.
   */
  String timeString();

  /**
   * @brief Get "YYYY-MM-DD" formatted date string.
   * @return String with date.
   */
  String dateString();

  /**
   * @brief Get ISO8601 "YYYY-MM-DDTHH:MM:SS" formatted string.
   * @return String with date-time.
   */
  String iso8601String();

private:
  /**
   * @brief Populate pins/model from config (ICM) or set sentinels (others/TESTMODE).
   */
  void loadPinsFromConfig();

private:
  LogFS*      _log  = nullptr;   //!< Optional logger
  TwoWire*    _wire = nullptr;   //!< I2C bus reference
  RTC_DS3231* _rtc  = nullptr;   //!< DS3231 instance (ICM)
  I2CBusHub*  _bus  = nullptr;   //!< Optional I2C hub

  // From config (defaults in Config). Non-ICM/TESTMODE: sentinel values.
  String _model = String("ESP32-RTC");
  int    _pinSCL = -1;
  int    _pinSDA = -1;
  int    _pinINT = -1;
  int    _pin32K = -1;
  int    _pinRST = -1;

  // Cached strings
  String _cachedTime, _cachedDate, _cachedIso;

#if defined(RTC_TESTMODE)
  // ===== Simulation state for TESTMODE =====
  unsigned long     _simUnix    = 1735689600UL; // 2025-01-01T00:00:00Z
  bool              _sim32k     = false;
  DateTime          _simA1      = DateTime(2025, 1, 1, 0, 0, 0);
  DateTime          _simA2      = DateTime(2025, 1, 1, 0, 0, 0);
  Ds3231Alarm1Mode  _simA1Mode  = DS3231_A1_PerSecond;
  Ds3231Alarm2Mode  _simA2Mode  = DS3231_A2_PerMinute;
  bool              _simA1Fired = false;
  bool              _simA2Fired = false;
#endif
};

#endif // RTCMANAGER_H
