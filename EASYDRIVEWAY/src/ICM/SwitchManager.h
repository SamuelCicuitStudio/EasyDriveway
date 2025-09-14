#ifndef SWITCH_MANAGER_H
#define SWITCH_MANAGER_H

#include "Arduino.h"
#include "ConfigManager.h"
#include "Config.h"
#include "Utils.h"
#include "Peripheral/ICMLogFS.h"
#include "Peripheral/BuzzerManager.h"

// ===============================
// Tap/Hold timing & task knobs
// ===============================
#define SWITCH_TASK_LOOP_DELAY_MS   20      // poll period
#define SWITCH_TASK_CALL_DELAY_MS   500     // (unused with our loop, kept for compat)
#define TAP_TIMEOUT_MS              1200    // end-of-sequence timeout
#define TAP_WINDOW_MS               900     // tight window for fast triple
#define HOLD_THRESHOLD_MS           3000    // long-hold threshold
#define POWER_ON_SWITCH_PIN         BOOT_SW_PIN
#define SWITCH_TASK_STACK_SIZE      3072
#define SWITCH_TASK_PRIORITY        1
#define SWITCH_TASK_CORE            1

class WiFiManager; // fwd decl to avoid heavy include

class SwitchManager {
public:
  // ctor
  SwitchManager(ConfigManager* Conf, ICMLogFS* LogFS = nullptr);

  // bring-up
  void TapDetect();                        // spawns RTOS task

  // deps
  void attachBuzzer(BuzzerManager* b) { Bz = b; }
  void attachLog(ICMLogFS* Log) { LogFS = Log; }
  void attachWiFi  (WiFiManager*   w) { WiFi = w; }

  // task entry
  static void SwitchTask(void* pvParameters);
  static SwitchManager* instance;

  // exposed (optional)
  ConfigManager*  Conf  = nullptr;
  ICMLogFS*       LogFS = nullptr;
  BuzzerManager*  Bz    = nullptr;
  WiFiManager*    WiFi  = nullptr;

private:
  // detection loop
  void detectTapOrHold();

  // gesture actions
  void actionLongHold_FactoryReset();     // long hold
  void actionSingleTap_ToggleAP();        // single tap
  void actionSerialOnlyMode();            // double tap (also used by triple fallback)
};

#endif // SWITCH_MANAGER_H
