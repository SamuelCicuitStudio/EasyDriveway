
#ifndef BUZZER_MANAGER_H
#define BUZZER_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "Config.h"
#include "ConfigManager.h"

// ----------------------
// Defaults (if not in Config.h)
// ----------------------
#ifndef BUZZER_PIN_KEY
  #define BUZZER_PIN_KEY "BZPIN"
#endif
#ifndef BUZZER_PIN_DEFAULT
  #define BUZZER_PIN_DEFAULT 4
#endif
#ifndef BUZZER_ACTIVE_HIGH_KEY
  #define BUZZER_ACTIVE_HIGH_KEY "BZAH"
#endif
#ifndef BUZZER_ACTIVE_HIGH_DEFAULT
  #define BUZZER_ACTIVE_HIGH_DEFAULT 1
#endif
#ifndef BUZZER_ENABLED_KEY
  #define BUZZER_ENABLED_KEY "BZEN"
#endif
#ifndef BUZZER_ENABLED_DEFAULT
  #define BUZZER_ENABLED_DEFAULT 1
#endif

// ----------------------
// BuzzerManager (PSM profile)
// ----------------------
// A small non-blocking beeper with patterns that map to
// Power Supply Module (PSM) states and faults.
// It is driven by high-level signals typically coming from
// PSMEspNowManager / Power telemetry.
class BuzzerManager {
public:
  // PSM-centric events
  enum Event : uint8_t {
    EV_STARTUP = 0,          // boot complete, config valid
    EV_CONFIG_MODE,          // factory/AP/config mode entered
    EV_PAIR_REQUEST,        // prompt: pair with ICM (user attention)
    EV_CONFIG_PROMPT,       // prompt: configure TF-Luna (user attention)
    EV_CONFIG_SAVED,         // settings stored OK
    EV_LINK_UP,              // ICM link established (ESP-NOW peer ready)
    EV_LINK_DOWN,            // ICM link lost
    EV_MAINS_PRESENT,        // AC/DC mains detected
    EV_MAINS_LOST,           // mains lost
    EV_ON_BATTERY,           // running from standby battery
    EV_BAT_CHARGING,         // charger active
    EV_BAT_FULL,             // battery full
    EV_LOW_BAT,              // battery low threshold
    EV_48V_ON,               // 48V rail enabled
    EV_48V_OFF,              // 48V rail disabled
    EV_OVERCURRENT,          // rail OC / OCP
    EV_OVERTEMP,             // over-temperature
    EV_COMM_ERROR,           // I2C/Peripheral comm failure
    EV_BITE_PASS,            // Built-In Test passed
    EV_BITE_FAIL,            // Built-In Test failed
    EV_SHUTDOWN,             // orderly power-down
    EV_FAULT                 // generic critical fault
  };

  // Condensed status snapshot that buzzer cares about;
  // derive this from your telemetry (PSM status over ESP-NOW).
  struct Status {
    bool linkUp        = false;
    bool mains         = false;
    bool onBattery     = false;
    bool charging      = false;
    bool batFull       = false;
    bool lowBat        = false;
    bool rail48V       = false;
    bool overTemp      = false;
    bool overCurrent   = false;
    bool commError     = false;
    bool fault         = false;   // any critical latched fault
  };

  explicit BuzzerManager(ConfigManager* cfg) : _cfg(cfg) {}

  bool begin();

  // Core APIs
  void play(Event ev);
  void playFromStatus(const Status& now, const Status* prev = nullptr);

  // Convenience hooks for higher-level callbacks
  void onSetRail48VResult(bool requestedOn, bool ok);
  void onClearFaultResult(bool ok);
  void onEnterConfigMode();
  void onSaveConfig(bool ok);
  void onLinkChange(bool up);
  void onShutdownRequested();

  // Manual "bip"
  void bip(uint16_t freq = 1200, uint16_t ms = 50);

  // Controls
  void setEnabled(bool en);
  bool enabled() const { return _enabled; }
  void toggle() { setEnabled(!_enabled); }

  // Emergency stop any pattern
  void stop();

  int pin() const { return _pin; }

private:
  struct Step { uint16_t freq; uint16_t durMs; uint16_t pauseMs; };

  static void taskThunk(void* arg);
  void runPattern(const Step* steps, size_t count);
  void toneOn(uint16_t freq);
  void toneOff();
  void idleLevel();

private:
  ConfigManager* _cfg = nullptr;
  TaskHandle_t   _task = nullptr;

  int   _pin        = BUZZER_PIN_DEFAULT;
  bool  _activeHigh = (BUZZER_ACTIVE_HIGH_DEFAULT != 0);
  bool  _enabled    = (BUZZER_ENABLED_DEFAULT != 0);
};

#endif // BUZZER_MANAGER_H
