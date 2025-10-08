/**************************************************************
 *  Project     : EasyDriveway
 *  File        : SleepTimer.h
 *  Purpose     : Inactivity-based sleep with role-aware wake using RTCManager only.
 *  Author      : Tshibangu Samuel
 *  Role        : Freelance Embedded Systems Engineer
 *  Expertise   : Secure IoT Systems, Embedded C++, RTOS, Control Logic
 *  Contact     : tshibsamuel47@gmail.com
 *  Portfolio   : https://www.freelancer.com/u/tshibsamuel477
 *  Phone       : +216 54 429 793
 *  Created     : 2025-10-06
 *  Version     : 1.0.0
 **************************************************************/
#ifndef SLEEPTIMER_H
#define SLEEPTIMER_H

#include <Arduino.h>
#include "driver/gpio.h"
#include "RTCManager.h"
#include "esp_sleep.h"
#include "NVS/NvsManager.h"
#include "NVS/NVSConfig.h"
#include "LogFS.h"
#include <RTClib.h>

#ifndef SLEEP_TASK_CORE
#define SLEEP_TASK_CORE        1
#endif
#ifndef SLEEP_TASK_PRIORITY
#define SLEEP_TASK_PRIORITY    1
#endif
#ifndef SLEEP_TASK_STACK
#define SLEEP_TASK_STACK       4096
#endif
#ifndef SLEEP_CHECK_PERIOD_MS
#define SLEEP_CHECK_PERIOD_MS  1000
#endif
#ifndef SLEEP_TIMEOUT_SEC_DEFAULT
#define SLEEP_TIMEOUT_SEC_DEFAULT  600
#endif

/**
 * @class SleepTimer
 * @brief Periodically checks inactivity and arms sleep; uses RTCManager for time/alarm.
 */
class SleepTimer {
public:
  /** Callback type for power hooks (before/after sleep). */
  typedef void (*CallbackFn)();

  /**
   * @brief Construct with managers.
   * @param rtcMan  RTCManager (required; wraps DS3231 for ICM, system time otherwise).
   * @param cfg     NVS manager (required).
   * @param log     Optional LogFS logger.
   */
  SleepTimer(RTCManager* rtcMan, NvsManager* cfg, LogFS* log = nullptr)
  : _rtc(rtcMan), _cfg(cfg), _log(log) {}

  /** @brief Attach / replace logger. */
  void setLogger(LogFS* log) { _log = log; }

  /**
   * @brief Initialize module (loads pins for ICM; seeds inactivity window).
   * @param inactivityTimeoutSec Seconds before arming sleep on inactivity.
   * @return true on success.
   */
  bool begin(uint32_t inactivityTimeoutSec = SLEEP_TIMEOUT_SEC_DEFAULT);

  /** @brief Reset inactivity timer; call on any user/hardware activity. */
  void resetActivity();

  /** @brief Set inactivity window (seconds). */
  void setInactivityTimeoutSec(uint32_t sec) { _inactTimeoutSec = sec ? sec : 1; }

  /** @brief Start RTOS task that checks inactivity. */
  bool startTask();

  /** @brief Schedule sleep after delta seconds. */
  bool sleepAfterSeconds(uint32_t deltaSec);

  /** @brief Schedule sleep to a Unix epoch. */
  bool sleepUntilEpoch(uint32_t wakeEpoch);

  /** @brief Schedule sleep to a DateTime. */
  bool sleepUntil(const DateTime& when);

  /** @brief Current epoch via RTCManager. */
  uint32_t nowEpoch() const;

  /** @brief Last activity epoch. */
  uint32_t lastActivityEpoch() const { return _lastActivityEpoch; }

  /** @brief Next wake epoch. */
  uint32_t nextWakeEpoch()  const { return _nextWakeEpoch; }

  /** @brief Whether sleep is armed. */
  bool     isArmed()        const { return _sleepArmed; }

  /** @brief Seconds remaining before sleep (negative => due/armed). */
  long     secondsUntilSleep() const;

  /** @brief Set power-down hook. */
  void setPowerDownHook(CallbackFn fn) { _powerDownHook = fn; }

  /** @brief Set power-up hook. */
  void setPowerUpHook  (CallbackFn fn) { _powerUpHook   = fn; }

private:
  /** @brief Task entry thunk. */
  static void taskThunk(void* arg);
  /** @brief Task loop. */
  void        taskLoop();

  /** @brief Arm sleep at a DateTime target. */
  bool armSleepAt(const DateTime& when);
  /** @brief Arm sleep at a Unix epoch target. */
  bool armSleepAt(uint32_t wakeEpoch);
  /** @brief Configure wake sources (EXT1 on ICM, timer otherwise). */
  bool configureWakeSources(bool& deepCapable);
  /** @brief Enter sleep (deep if possible). */
  void goToSleep(bool deepCapable);

#ifdef NVS_ROLE_ICM
  /** @brief Program DS3231 Alarm1 via RTCManager (exact date/time). */
  bool programAlarm1Exact(const DateTime& when);
  /** @brief Clear and disable Alarm1 via RTCManager. */
  void clearAndDisableAlarm1();
#endif

  inline void logInfo (int code, const String& msg) { if (_log) _log->event(LogFS::DOM_POWER, LogFS::EV_INFO,  code, msg,  "SleepTimer"); }
  inline void logWarn (int code, const String& msg) { if (_log) _log->event(LogFS::DOM_POWER, LogFS::EV_WARN,  code, msg,  "SleepTimer"); }
  inline void logError(int code, const String& msg) { if (_log) _log->event(LogFS::DOM_POWER, LogFS::EV_ERROR, code, msg,  "SleepTimer"); }

private:
  RTCManager*   _rtc = nullptr;
  NvsManager*   _cfg = nullptr;
  LogFS*        _log = nullptr;

  TaskHandle_t  _task = nullptr;

  uint32_t      _inactTimeoutSec   = SLEEP_TIMEOUT_SEC_DEFAULT;
  uint32_t      _lastActivityEpoch = 0;
  uint32_t      _nextWakeEpoch     = 0;
  bool          _sleepArmed        = false;

#ifdef NVS_ROLE_ICM
  int           _pinRTCInt         = DS3231_INT_PIN;
#endif

  CallbackFn    _powerDownHook     = nullptr;
  CallbackFn    _powerUpHook       = nullptr;
};

#endif // SLEEPTIMER_H
