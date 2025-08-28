#ifndef BUZZER_MANAGER_H
#define BUZZER_MANAGER_H

#include <Arduino.h>
#include "Config.h"
#include "ConfigManager.h"
#include <vector>

// ---- NVS key for enabling buzzer feedback (≤ 6 chars) ----
#ifndef BUZZER_FEEDBACK_KEY
  #define BUZZER_FEEDBACK_KEY     "BZFEED"   // 6
#endif
#ifndef BUZZER_FEEDBACK_DEFAULT
  #define BUZZER_FEEDBACK_DEFAULT 1          // enabled by default
#endif

class BuzzerManager {
public:
  // All events we support
  enum Event : uint8_t {
    EV_STARTUP = 0,
    EV_READY,
    EV_WIFI_CONNECTED,
    EV_WIFI_OFF,
    EV_BLE_PAIRING,
    EV_BLE_PAIRED,
    EV_BLE_UNPAIRED,
    EV_CLIENT_CONNECTED,
    EV_CLIENT_DISCONNECTED,
    EV_POWER_GONE,
    EV_ON_BATTERY,
    EV_LOW_POWER,
    EV_OVER_TEMP,
    EV_FAULT,
    EV_SHUTDOWN,
    EV_SUCCESS,
    EV_FAILED
  };

  explicit BuzzerManager(ConfigManager* cfg) : _cfg(cfg) {}

  // must be called once (after ConfigManager is ready)
  bool begin();

  // fire-and-forget event beep
  void play(Event ev);

  // one short "bip" utility
  void bip(uint16_t freq = 1000, uint16_t ms = 60);

  // control
  void setEnabled(bool en);
  bool enabled() const { return _enabled; }
  void toggle()        { setEnabled(!_enabled); }

  // stop any active sequence immediately
  void stop();

  // expose pin for debugging
  int pin() const { return _pin; }

private:
  // pattern runner
  struct Step { uint16_t freq; uint16_t durMs; uint16_t pauseMs; };
  static void taskThunk(void* arg);
  void runPattern(const Step* steps, size_t count);

  // helpers
  void applyIdle();
  void toneOn(uint16_t freq);
  void toneOff();

private:
  ConfigManager*  _cfg     = nullptr;
  TaskHandle_t    _task    = nullptr;

  int   _pin              = BUZZER_PIN_DEFAULT;
  bool  _activeHigh       = BUZZER_ACTIVE_HIGH_DEFAULT; // HIGH=on when true
  bool  _enabled          = (BUZZER_FEEDBACK_DEFAULT != 0);
};

#endif // BUZZER_MANAGER_H
