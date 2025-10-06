/**************************************************************
 *  Project     : EasyDriveway
 *  File        : BuzzerManager.h
 *  Purpose     : Event-driven buzzer driver with NVS-controlled policy.
 *  Author      : Tshibangu Samuel
 *  Role        : Freelance Embedded Systems Engineer
 *  Expertise   : Secure IoT Systems, Embedded C++, RTOS, Control Logic
 *  Contact     : tshibsamuel47@gmail.com
 *  Phone       : +216 54 429 793
 *  Created     : 2025-10-05
 *  Version     : 1.0.0
 **************************************************************/
#ifndef BUZZER_MANAGER_H
#define BUZZER_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "NVS/NVSManager.h"
#include "NVS/NVSConfig.h"

/**
 * @file BuzzerManager.h
 * @brief Event-driven buzzer driver with NVS-controlled policy (enable/disable + polarity).
 * @details Maps device events to short tone patterns and strictly obeys indicator policy persisted in NVS.
 *
 * - Effective enable: enabled = (BUZFBK == 1) && (BUZDIS == 0)
 * - Polarity: activeHigh = (BUZAHI == 1) → idle is LOW; otherwise HIGH
 *
 * ### NVS Keys (namespace "IND")
 * - BUZAHI : Buzzer active-high (bool) — default NVS_DEF_BUZAHI
 * - BUZFBK : Buzzer feedback enabled (bool) — default NVS_DEF_BUZFBK
 * - BUZDIS : Legacy buzzer disable (bool) — default NVS_DEF_BUZDIS (mirrored)
 */
class BuzzerManager {
public:
  /**
   * @brief Enumerates high-level system events mapped to tone patterns.
   */
  enum Event : uint8_t {
    EV_STARTUP = 0, EV_CONFIG_MODE, EV_PAIR_REQUEST, EV_CONFIG_PROMPT, EV_CONFIG_SAVED,
    EV_LINK_UP, EV_LINK_DOWN, EV_MAINS_PRESENT, EV_MAINS_LOST, EV_ON_BATTERY,
    EV_BAT_CHARGING, EV_BAT_FULL, EV_LOW_BAT, EV_48V_ON, EV_48V_OFF,
    EV_OVERCURRENT, EV_OVERTEMP, EV_COMM_ERROR, EV_BITE_PASS, EV_BITE_FAIL,
    EV_SHUTDOWN, EV_FAULT
  };

  /**
   * @brief Snapshot of system status flags used to derive edge-driven sounds.
   */
  struct Status {
    bool linkUp=false, mains=false, onBattery=false, charging=false, batFull=false, lowBat=false;
    bool rail48V=false, overTemp=false, overCurrent=false, commError=false, fault=false;
  };

  /**
   * @brief Construct the buzzer manager and bind the NVS manager.
   * @param cfg Pointer to NvsManager used for reading/writing indicator policy.
   */
  explicit BuzzerManager(NvsManager* cfg) : _cfg(cfg) {}

  /**
   * @brief Initialize the buzzer: read GPIO, load policy, configure pin, set idle.
   * @return true if initialized; false if NVS manager is missing.
   */
  bool begin();

  /**
   * @brief Play the tone pattern bound to a specific event.
   * @param ev Event selector (see ::Event).
   * @note No-op if disabled by policy; runs non-blocking in a FreeRTOS task.
   */
  void play(Event ev);

  /**
   * @brief Derive and play sounds from status edges (prev → now).
   * @param now Current status snapshot.
   * @param prev Optional previous snapshot (edge detection); if nullptr, plays initial-state cues.
   * @note No-op if disabled by policy.
   */
  void playFromStatus(const Status& now, const Status* prev = nullptr);

  /**
   * @brief Notify result of a 48V rail request to emit success/failure tones.
   * @param requestedOn Desired state requested by the system.
   * @param ok True if action succeeded; false triggers a fault pattern.
   */
  void onSetRail48VResult(bool requestedOn, bool ok);

  /**
   * @brief Notify result of clearing a fault (BITE PASS/FAIL tones).
   * @param ok True -> BITE_PASS; False -> BITE_FAIL.
   */
  void onClearFaultResult(bool ok);

  /**
   * @brief Emit the "enter config mode" notification pattern.
   */
  void onEnterConfigMode();

  /**
   * @brief Emit the "save config" result pattern.
   * @param ok True -> CONFIG_SAVED; False -> BITE_FAIL.
   */
  void onSaveConfig(bool ok);

  /**
   * @brief Emit link up/down notification tones.
   * @param up True -> LINK_UP; False -> LINK_DOWN.
   */
  void onLinkChange(bool up);

  /**
   * @brief Emit shutdown notification tone sequence.
   */
  void onShutdownRequested();

  /**
   * @brief Generate a single bip at the requested frequency and duration.
   * @param freq Frequency in Hz (default 1200 Hz).
   * @param ms   Duration in milliseconds (default 50 ms).
   * @note No-op if disabled by policy; blocks only for the given duration.
   */
  void bip(uint16_t freq = 1200, uint16_t ms = 50);

  /**
   * @brief Enable/disable buzzer feedback and persist to NVS.
   * @param en True to enable; false to disable.
   * @param persist If true, writes BUZFBK and mirrors legacy BUZDIS.
   */
  void setEnabled(bool en, bool persist = true);

  /**
   * @brief Query the effective enable state: (BUZFBK && !BUZDIS).
   * @return true if buzzer feedback is enabled; otherwise false.
   */
  bool enabled() const { return _enabled; }

  /**
   * @brief Toggle enable state (persisted).
   */
  void toggle() { setEnabled(!_enabled); }

  /**
   * @brief Set active-high polarity and persist to NVS.
   * @param ah True -> active-high (idle LOW); false -> active-low (idle HIGH).
   * @param persist If true, writes BUZAHI to NVS and reapplies idle level.
   */
  void setActiveHigh(bool ah, bool persist = true);

  /**
   * @brief Query the current electrical polarity.
   * @return true if active-high; false if active-low.
   */
  bool activeHigh() const { return _activeHigh; }

  /**
   * @brief Stop any running pattern/task and enforce idle level.
   */
  void stop();

  /**
   * @brief Get the configured buzzer GPIO pin.
   * @return Pin number currently in use.
   */
  int pin() const { return _pin; }

private:
  /**
   * @brief Single pattern step: tone for durMs, then pauseMs silence.
   */
  struct Step { uint16_t freq; uint16_t durMs; uint16_t pauseMs; };

  /**
   * @brief FreeRTOS task entry point (dispatch for pattern playback).
   * @param arg Packed instance pointer + steps vector (allocated by runPattern()).
   */
  static void taskThunk(void* arg);

  /**
   * @brief Start (or restart) an asynchronous pattern.
   * @param steps Pointer to array of Step.
   * @param count Number of steps in the array.
   */
  void runPattern(const Step* steps, size_t count);

  /**
   * @brief Turn tone output ON at the requested frequency (if enabled).
   * @param freq Frequency in Hz.
   */
  void toneOn(uint16_t freq);

  /**
   * @brief Turn tone output OFF and reassert idle level.
   */
  void toneOff();

  /**
   * @brief Drive the pin to the correct idle (silent) level per polarity.
   */
  void idleLevel();

  /**
   * @brief Load indicator policy (BUZAHI, BUZFBK, BUZDIS) from NVS.
   */
  void loadPolicy();

  /**
   * @brief Read/choose the buzzer GPIO and prepare the pin for output.
   */
  void setupPin();

private:
  NvsManager* _cfg = nullptr;      ///< Bound NVS manager (not owned)
  TaskHandle_t _task = nullptr;    ///< Playback task handle (nullable)
  int  _pin = BUZZER_PIN;          ///< Buzzer GPIO (default from your config)
  bool _activeHigh = NVS_DEF_BUZAHI != 0;                 ///< Polarity default
  bool _enabled    = (NVS_DEF_BUZFBK != 0) && (NVS_DEF_BUZDIS == 0); ///< Effective enable default
};
#endif // BUZZER_MANAGER_H
