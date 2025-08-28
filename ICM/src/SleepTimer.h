#ifndef SLEEPTIMER_H
#define SLEEPTIMER_H

#include <Arduino.h>
#include <RTClib.h>          // RTC_DS3231, DateTime, Ds3231* enums
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp_sleep.h"

#include "ConfigManager.h"
#include "Config.h"          // RTC_INT_PIN_KEY / defaults, etc.
#include "ICMLogFS.h"        // event logging

// ================== Task config (override at build if needed) ==================
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
#define SLEEP_CHECK_PERIOD_MS  1000   // check inactivity every 1s
#endif

// Default inactivity timeout (seconds)
#ifndef SLEEP_TIMEOUT_SEC_DEFAULT
#define SLEEP_TIMEOUT_SEC_DEFAULT  600  // 10 minutes
#endif

class SleepTimer {
public:
    // Optional callbacks to power down / up your peripherals just before/after sleep
    typedef void (*CallbackFn)();

    // Construct with managers (required)
    SleepTimer(RTC_DS3231* rtc, ConfigManager* cfg, ICMLogFS* log = nullptr)
    : _rtc(rtc), _cfg(cfg), _log(log) {}

    // Attach / replace logger if needed
    void setLogger(ICMLogFS* log) { _log = log; }

    // Initialize: reads RTC INT pin from ConfigManager; stamps inactivity start
    bool begin(uint32_t inactivityTimeoutSec = SLEEP_TIMEOUT_SEC_DEFAULT);

    // Inactivity bookkeeping
    void resetActivity();                            // call on any user/hw activity
    void setInactivityTimeoutSec(uint32_t sec) { _inactTimeoutSec = sec ? sec : 1; }

    // Start RTOS loop that checks inactivity periodically
    bool startTask();

    // Manual sleep scheduling
    bool sleepAfterSeconds(uint32_t deltaSec);
    bool sleepUntilEpoch(uint32_t wakeEpoch);
    bool sleepUntil(const DateTime& when);

    // Query
    uint32_t nowEpoch() const;                       // current RTC epoch
    uint32_t lastActivityEpoch() const { return _lastActivityEpoch; }
    uint32_t nextWakeEpoch()  const { return _nextWakeEpoch; }
    bool     isArmed()        const { return _sleepArmed; }
    long     secondsUntilSleep() const;              // negative => due/armed

    // Power hooks
    void setPowerDownHook(CallbackFn fn) { _powerDownHook = fn; }
    void setPowerUpHook  (CallbackFn fn) { _powerUpHook   = fn; }

private:
    // Task plumbing
    static void taskThunk(void* arg);
    void        taskLoop();

    // Core operations
    bool armSleepAt(const DateTime& when);
    bool armSleepAt(uint32_t wakeEpoch);
    bool configureWakeSources(bool& deepCapable);
    void goToSleep(bool deepCapable);

    // DS3231 alarm helpers via RTClib
    bool programAlarm1Exact(const DateTime& when);
    void clearAndDisableAlarm1();

    // Logging helpers
    inline void logInfo (int code, const String& msg) {
        if (_log) _log->event(ICMLogFS::DOM_POWER, ICMLogFS::EV_INFO,  code, msg,  "SleepTimer");
    }
    inline void logWarn (int code, const String& msg) {
        if (_log) _log->event(ICMLogFS::DOM_POWER, ICMLogFS::EV_WARN,  code, msg,  "SleepTimer");
    }
    inline void logError(int code, const String& msg) {
        if (_log) _log->event(ICMLogFS::DOM_POWER, ICMLogFS::EV_ERROR, code, msg,  "SleepTimer");
    }

private:
    RTC_DS3231*     _rtc  = nullptr;
    ConfigManager*  _cfg  = nullptr;
    ICMLogFS*       _log  = nullptr;

    TaskHandle_t    _task = nullptr;

    // Policy/state
    uint32_t        _inactTimeoutSec   = SLEEP_TIMEOUT_SEC_DEFAULT;
    uint32_t        _lastActivityEpoch = 0;
    uint32_t        _nextWakeEpoch     = 0;
    bool            _sleepArmed        = false;

    // Pins (from ConfigManager)
    int             _pinRTCInt         = RTC_INT_PIN_DEFAULT;

    // Hooks
    CallbackFn      _powerDownHook     = nullptr;
    CallbackFn      _powerUpHook       = nullptr;
};

#endif // SLEEPTIMER_H
