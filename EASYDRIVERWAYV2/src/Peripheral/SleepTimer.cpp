/**************************************************************
 *  Project : EasyDriveway
 *  File    : SleepTimer.cpp
 **************************************************************/
#include "SleepTimer.h"
bool SleepTimer::begin(uint32_t inactivityTimeoutSec) {
  if (!_cfg) { logError(5000, "begin(): missing cfg"); return false; }
  if (!_rtc) { logError(5000, "begin(): missing rtc manager"); return false; }
#ifdef NVS_ROLE_ICM
  _pinRTCInt = DS3231_INT_PIN;
#endif
  _inactTimeoutSec = inactivityTimeoutSec ? inactivityTimeoutSec : SLEEP_TIMEOUT_SEC_DEFAULT;
  _lastActivityEpoch = nowEpoch();
  logInfo(5001,
#ifdef NVS_ROLE_ICM
    String("Init OK (ICM). RTC-INT=") + String(_pinRTCInt) +
#else
    String("Init OK (NODE). ") +
#endif
    " timeoutSec=" + String(_inactTimeoutSec));
  return true;
}
void SleepTimer::resetActivity() { _lastActivityEpoch = nowEpoch(); }
uint32_t SleepTimer::nowEpoch() const {
  unsigned long ts = _rtc ? _rtc->getUnixTime() : 0UL;
  return (uint32_t)ts;
}
long SleepTimer::secondsUntilSleep() const {
  uint32_t now = nowEpoch(); if (!now) return -1; return (long)_inactTimeoutSec - (long)(now - _lastActivityEpoch);
}
void SleepTimer::taskThunk(void* arg) { static_cast<SleepTimer*>(arg)->taskLoop(); }
void SleepTimer::taskLoop() {
  (void)secondsUntilSleep();
  const TickType_t period = pdMS_TO_TICKS(SLEEP_CHECK_PERIOD_MS);
  TickType_t last = xTaskGetTickCount();
  while (true) {
    vTaskDelayUntil(&last, period);
    if (_sleepArmed) continue;
    long secsLeft = secondsUntilSleep();
    if (secsLeft <= 0) {
      uint32_t wake = nowEpoch() + 1;
      if (armSleepAt(wake)) {
        bool deepCapable = false;
        if (configureWakeSources(deepCapable)) { goToSleep(deepCapable); }
        else { logError(5010, "configureWakeSources() failed"); }
      }
    }
  }
}
bool SleepTimer::startTask() {
  if (_task) return true;
  BaseType_t ok = xTaskCreatePinnedToCore(&SleepTimer::taskThunk, "SleepTimer", SLEEP_TASK_STACK, this, SLEEP_TASK_PRIORITY, &_task, SLEEP_TASK_CORE);
  return (ok == pdPASS);
}
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
  return true;
}
bool SleepTimer::armSleepAt(const DateTime& when) {
  if (!when.isValid()) { logWarn(5020, "armSleepAt: invalid DateTime"); return false; }
#ifdef NVS_ROLE_ICM
  if (!programAlarm1Exact(when)) { logWarn(5021, "RTC Alarm1 program failed"); return false; }
  _rtc->writeSqwPinMode(DS3231_OFF);
#endif
  _nextWakeEpoch = (uint32_t)when.unixtime();
  _sleepArmed = true;
  logInfo(5022, String("Sleep armed. Wake @ epoch ") + String(_nextWakeEpoch));
  return true;
}
bool SleepTimer::armSleepAt(uint32_t wakeEpoch) { return armSleepAt(DateTime((time_t)wakeEpoch)); }
#ifdef NVS_ROLE_ICM
bool SleepTimer::programAlarm1Exact(const DateTime& when) {
  return _rtc ? _rtc->setAlarm1(when, DS3231_A1_Date) : false;
}
void SleepTimer::clearAndDisableAlarm1() {
  if (_rtc) { _rtc->clearAlarm(1); _rtc->disableAlarm(1); }
}
#endif
bool SleepTimer::configureWakeSources(bool& deepCapable) {
#ifdef NVS_ROLE_ICM
  deepCapable = false;
  pinMode(_pinRTCInt, INPUT_PULLUP);
  const gpio_num_t pin = (gpio_num_t)_pinRTCInt;
  if (rtc_gpio_is_valid_gpio(pin)) {
    const uint64_t mask = 1ULL << (uint32_t)pin;
    esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ALL_LOW);
    deepCapable = true;
    logInfo(5030, "EXT1 deep-sleep wake enabled on RTC INT (active LOW)");
  } else {
    gpio_wakeup_enable(pin, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();
    logWarn(5031, "RTC INT not RTC-capable; using light sleep GPIO wake");
  }
  esp_sleep_enable_timer_wakeup(2ULL * 1000000ULL);
#else
  deepCapable = true;
  uint32_t now = nowEpoch();
  uint64_t delta_us = 0;
  if (_sleepArmed && _nextWakeEpoch > now) { delta_us = (uint64_t)(_nextWakeEpoch - now) * 1000000ULL; }
  else { delta_us = 1000000ULL; }
  esp_sleep_enable_timer_wakeup(delta_us);
  logInfo(5032, String("Timer wake in us=") + String((unsigned long)(delta_us & 0xFFFFFFFFUL)));
#endif
  return true;
}
void SleepTimer::goToSleep(bool deepCapable) {
  if (_powerDownHook) _powerDownHook();
  logInfo(5040, deepCapable ? "Entering DEEP SLEEP..." : "Entering LIGHT SLEEP...");
#ifdef NVS_ROLE_ICM
  if (deepCapable) { esp_deep_sleep_start(); }
  else { esp_light_sleep_start(); }
#else
  esp_deep_sleep_start();
#endif
#ifdef NVS_ROLE_ICM
  _sleepArmed = false;
  clearAndDisableAlarm1();
  if (_powerUpHook) _powerUpHook();
  resetActivity();
  logInfo(5041, "Woke from sleep");
#endif
}
