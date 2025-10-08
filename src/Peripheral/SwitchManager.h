/**************************************************************
 *  Project     : EasyDriveway
 *  File        : SwitchManager.h
 *  Purpose     : Button-press detection (tap/hold) with role-aware actions.
 *  Author      : Tshibangu Samuel
 *  Role        : Freelance Embedded Systems Engineer
 *  Expertise   : Secure IoT Systems, Embedded C++, RTOS, Control Logic
 *  Contact     : tshibsamuel47@gmail.com
 *  Portfolio   : https://www.freelancer.com/u/tshibsamuel477
 *  Phone       : +216 54 429 793
 *  Created     : 2025-10-05
 *  Version     : 1.0.0
 **************************************************************/
#ifndef SWITCH_MANAGER_H
#define SWITCH_MANAGER_H

#include "NVS/NvsManager.h"
#include "NVS/NVSConfig.h"
#include "Utils.h"
#include "LogFS.h"
#include "BuzzerManager.h"
#include <driver/gpio.h>

/* Tap/Hold timing & task knobs */
#define SWITCH_TASK_LOOP_DELAY_MS   20
#define SWITCH_TASK_CALL_DELAY_MS   500
#define TAP_TIMEOUT_MS              1200
#define TAP_WINDOW_MS               900
#define HOLD_THRESHOLD_MS           3000
#define POWER_ON_SWITCH_PIN         0
#define SWITCH_TASK_STACK_SIZE      3072
#define SWITCH_TASK_PRIORITY        1
#define SWITCH_TASK_CORE            1

class WiFiManager; /**< fwd decl */

class SwitchManager {
public:
  /**
   * @brief Construct with configuration and optional LogFS.
   * @param Conf Pointer to NvsManager.
   * @param LogFS Optional LogFS pointer for structured events.
   */
  SwitchManager(NvsManager* Conf, LogFS* log = nullptr);

  /**
   * @brief Start the RTOS task that detects taps/holds.
   */
  void TapDetect();

  /**
   * @brief Attach optional buzzer for feedback.
   * @param b Buzzer manager.
   */
  void attachBuzzer(BuzzerManager* b) { Bz = b; }

  /**
   * @brief Attach LogFS for logging.
   * @param Log LogFS instance.
   */
  void attachLog(LogFS* Log) { log = Log; }

  /**
   * @brief Attach WiFi manager (ICM role only).
   * @param w WiFi manager.
   */
  void attachWiFi(WiFiManager* w) { WiFi = w; }

  /**
   * @brief RTOS task entry.
   * @param pvParameters Unused.
   */
  static void SwitchTask(void* pvParameters);

  /** @brief Global instance (task shim). */
  static SwitchManager* instance;

  /** @brief Exposed deps (optional). */
  NvsManager*   Conf  = nullptr;
  LogFS*        log = nullptr;
  BuzzerManager* Bz   = nullptr;
  WiFiManager*   WiFi = nullptr;

private:
  /**
   * @brief Polling loop to detect tap sequences and long-hold.
   */
  void detectTapOrHold();

  /**
   * @brief Long-hold action: set factory reset flag and reboot (role-agnostic).
   */
  void actionLongHold_FactoryReset();

  /**
   * @brief Single-tap action when ICM role: toggle AP state (beeps + logs).
   */
  void actionSingleTap_ToggleAP();

  /**
   * @brief Single-tap action when non-ICM role: print MAC on Serial (beeps + logs).
   */
  void actionSingleTap_PrintMac();

  /**
   * @brief Multi-tap action: enter Serial-only mode (double/triple).
   */
  void actionSerialOnlyMode();
};

#endif /* SWITCH_MANAGER_H */
