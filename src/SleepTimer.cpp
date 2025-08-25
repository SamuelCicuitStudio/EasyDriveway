#include "SleepTimer.h"

// -------------------------------- Begin ----------------------------------------
bool SleepTimer::begin(uint32_t inactivityTimeoutSec) {
    if (!_rtc || !_cfg) {
        logError(5000, "begin(): missing rtc/cfg");
        return false;
    }

    _inactTimeoutSec = inactivityTimeoutSec ? inactivityTimeoutSec : SLEEP_TIMEOUT_SEC_DEFAULT;
    _pinRTCInt       = _cfg->GetInt(RTC_INT_PIN_KEY, RTC_INT_PIN_DEFAULT);

    // Seed inactivity window from RTC time
    _lastActivityEpoch = nowEpoch();
    logInfo(5001, String("Init OK. RTC-INT=") + String(_pinRTCInt) +
                   " timeoutSec=" + String(_inactTimeoutSec));
    return true;
}

// ---------------------------- Inactivity / Query --------------------------------
void SleepTimer::resetActivity() {
    _lastActivityEpoch = nowEpoch();
}

uint32_t SleepTimer::nowEpoch() const {
    if (!_rtc) return 0;
    DateTime now = _rtc->now();
    return now.isValid() ? (uint32_t)now.unixtime() : 0;
}

long SleepTimer::secondsUntilSleep() const {
    uint32_t now = nowEpoch();
    if (!now) return -1;
    return (long)_inactTimeoutSec - (long)(now - _lastActivityEpoch);
}

// -------------------------------- RTOS Task ------------------------------------
void SleepTimer::taskThunk(void* arg) {
    static_cast<SleepTimer*>(arg)->taskLoop();
}

void SleepTimer::taskLoop() {
    // Initial evaluation
    (void)secondsUntilSleep();

    const TickType_t period = pdMS_TO_TICKS(SLEEP_CHECK_PERIOD_MS);
    TickType_t last = xTaskGetTickCount();

    while (true) {
        vTaskDelayUntil(&last, period);

        if (_sleepArmed) continue;

        long secsLeft = secondsUntilSleep();
        if (secsLeft <= 0) {
            // Arm sleep to wake 1s later as guard
            uint32_t wake = nowEpoch() + 1;
            if (armSleepAt(wake)) {
                bool deepCapable = false;
                if (configureWakeSources(deepCapable)) {
                    goToSleep(deepCapable);
                } else {
                    logError(5010, "configureWakeSources() failed");
                }
            }
        }
    }
}

bool SleepTimer::startTask() {
    if (_task) return true;
    BaseType_t ok = xTaskCreatePinnedToCore(
        &SleepTimer::taskThunk, "SleepTimer",
        SLEEP_TASK_STACK, this,
        SLEEP_TASK_PRIORITY, &_task, SLEEP_TASK_CORE
    );
    return (ok == pdPASS);
}

// ----------------------------- Manual Scheduling --------------------------------
bool SleepTimer::sleepAfterSeconds(uint32_t deltaSec) {
    uint32_t wake = nowEpoch() + (deltaSec ? deltaSec : 1);
    return sleepUntilEpoch(wake);
}

bool SleepTimer::sleepUntilEpoch(uint32_t wakeEpoch) {
    DateTime when((time_t)wakeEpoch);
    return sleepUntil(when);
}

bool SleepTimer::sleepUntil(const DateTime& when) {
    if (!armSleepAt(when)) return false;

    bool deepCapable = false;
    if (!configureWakeSources(deepCapable)) return false;

    goToSleep(deepCapable);
    return true; // for light sleep; deep sleep reboots the chip
}

// ------------------------- Arm / Program DS3231 alarm ---------------------------
bool SleepTimer::armSleepAt(const DateTime& when) {
    if (!when.isValid()) { logWarn(5020, "armSleepAt: invalid DateTime"); return false; }
    if (!programAlarm1Exact(when)) { logWarn(5021, "DS3231 Alarm1 program failed"); return false; }

    // Ensure INT/SQW outputs alarm (not square wave)
    _rtc->writeSqwPinMode(DS3231_OFF);

    // Remember + mark armed
    _nextWakeEpoch = (uint32_t)when.unixtime();
    _sleepArmed = true;

    logInfo(5022, String("Sleep armed. Wake @ epoch ") + String(_nextWakeEpoch));
    return true;
}

bool SleepTimer::armSleepAt(uint32_t wakeEpoch) {
    return armSleepAt(DateTime((time_t)wakeEpoch));
}

bool SleepTimer::programAlarm1Exact(const DateTime& when) {
    // Exact date + hh:mm:ss (A1 date match)
    if (!_rtc->setAlarm1(when, DS3231_A1_Date)) return false;

    // Clear stale flag; leave alarm enabled so INT line will latch LOW on fire
    _rtc->clearAlarm(1);
    return true;
}

void SleepTimer::clearAndDisableAlarm1() {
    _rtc->clearAlarm(1);
    _rtc->disableAlarm(1);
}

// ---------------------------- Wake source config --------------------------------
bool SleepTimer::configureWakeSources(bool& deepCapable) {
    deepCapable = false;

    // DS3231 INT is open-drain active-LOW, latched until alarm cleared.
    pinMode(_pinRTCInt, INPUT_PULLUP);

    const gpio_num_t pin = (gpio_num_t)_pinRTCInt;
    if (rtc_gpio_is_valid_gpio(pin)) {
        // Deep sleep (EXT1) when the line is LOW
        const uint64_t mask = 1ULL << (uint32_t)pin;
        esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ALL_LOW);
        deepCapable = true;
        logInfo(5030, "EXT1 deep-sleep wake enabled on RTC INT (active LOW)");
    } else {
        // Fallback: light sleep GPIO wake
        gpio_wakeup_enable(pin, GPIO_INTR_LOW_LEVEL);
        esp_sleep_enable_gpio_wakeup();
        logWarn(5031, "RTC INT not RTC-capable; using light sleep GPIO wake");
    }

    // Safety timer wake guard in case of alarm misconfig (2s)
    esp_sleep_enable_timer_wakeup(2ULL * 1000000ULL);

    return true;
}

// --------------------------------- Sleep ---------------------------------------
void SleepTimer::goToSleep(bool deepCapable) {
    if (_powerDownHook) _powerDownHook();

    logInfo(5040, deepCapable ? "Entering DEEP SLEEP..." : "Entering LIGHT SLEEP...");

    if (deepCapable) {
        esp_deep_sleep_start();   // no return
    } else {
        esp_light_sleep_start();  // returns after wake
    }

    // ---- on wake (light sleep) continues here; deep-sleep reboots app ----
    _sleepArmed = false;

    // Clear and disable Alarm1 to release the INT line (latched LOW)
    clearAndDisableAlarm1();

    if (_powerUpHook) _powerUpHook();

    resetActivity();
    logInfo(5041, "Woke from sleep");
}
