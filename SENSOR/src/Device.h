
#pragma once
/**
 * @file Device.h
 * @brief High-level RTOS controller for a Sensor Node in the "smart runway light" system.
 *
 * Responsibilities:
 *  - Own a simple state machine: IDLE → PAIRING → CONFIG_TF → WAIT_TOPO → RUNNING
 *  - Prompt user to pair with ICM (LED blink + buzzer) on first boot
 *  - Prompt user to configure both TF‑Luna sensors via SwitchManager gestures
 *  - Only mark paired/configured in NVS once a valid topology has been received
 *  - Run two RTOS tasks:
 *      * BME task (every 5 minutes): read environmental data and optionally report to ICM
 *      * TF watcher task (~10–20 ms): detect car direction & speed, fan out wave to relays
 *
 * Integration points:
 *  - ConfigManager  : persistent flags and thresholds
 *  - SensorEspNowManager : topology & ESP‑NOW sends to ICM/relays
 *  - TFLunaManager  : dual presence sensing and A/B timing
 *  - BME280Manager  : periodic telemetry
 *  - SwitchManager  : user gestures to configure TF‑Luna roles/addresses
 *  - RGBLed / BuzzerManager : user prompts & status
 */

#include <Arduino.h>
#include "Config.h"
#include "ConfigManager.h"
#include "SensEspNow.h"
#include "TFLunaManager.h"
#include "BME280Manager.h"
#include "SwitchManager.h"
#include "RGBLed.h"
#include "CommandAPI.h"
// Forward-declare buzzer (header available in your project)
class BuzzerManager;

#ifndef DEVICE_TASK_CORE
#define DEVICE_TASK_CORE   1
#endif
#ifndef DEVICE_TASK_STACK
#define DEVICE_TASK_STACK  4096
#endif
#ifndef DEVICE_TASK_PRIO
#define DEVICE_TASK_PRIO   2
#endif

#ifndef DEVICE_TF_TASK_CORE
#define DEVICE_TF_TASK_CORE 1
#endif
#ifndef DEVICE_TF_TASK_STACK
#define DEVICE_TF_TASK_STACK 4096
#endif
#ifndef DEVICE_TF_TASK_PRIO
#define DEVICE_TF_TASK_PRIO  2
#endif

#ifndef DEVICE_BME_PERIOD_MS
#define DEVICE_BME_PERIOD_MS  (5UL * 60UL * 1000UL) // 5 minutes
#endif

#ifndef DEVICE_TF_POLL_MS
#define DEVICE_TF_POLL_MS     15UL                  // TF watcher loop period
#endif

class Device {
public:
  enum Mode : uint8_t { IDLE=0, PAIRING, CONFIG_TF, WAIT_TOPO, RUNNING, ERROR };

  Device(ConfigManager* cfg,
         SensorEspNowManager* now,
         TFLunaManager* tf,
         BME280Manager* bme,
         SwitchManager* sw = nullptr,
         RGBLed* led = nullptr,
         BuzzerManager* buz = nullptr);

  bool begin();       // create tasks, seed state machine
  void loopOnce();    // call from Arduino loop() if desired (lightweight)

  // Accessors
  Mode mode() const { return mode_; }

  // Optional hooks
  void setPromptColors(uint32_t pairingColor, uint32_t configColor) {
    colorPairing_ = pairingColor; colorConfig_ = configColor;
  }

private:
  // ===== Tasks =====
  static void taskBMEThunk(void* arg);
  static void taskTFThunk(void* arg);
  void taskBMELoop();
  void taskTFLoop();

  // ===== State machine =====
  void stepStateMachine();
  bool checkPaired_() const;
  bool checkTfConfigured_() const;
  bool hasTopology_() const;
  void markPairedAndConfigured_();

  // ===== TF detection helpers =====
  struct PresenceState {
    bool presentA=false, presentB=false;
    uint32_t tA_ms=0, tB_ms=0;
    void clear(){ presentA=false; presentB=false; tA_ms=tB_ms=0; }
  };
  bool detectVehicle_(uint16_t& speed_mmps_out, int8_t& dir_out);

  // Fan out wave to relays on both lanes using playWave()
  void fanoutWave_(uint16_t speed_mmps, int8_t dir);

  // Prompts
  void promptPairing_();
  void promptConfigTF_();
  void clearPrompts_();

private:
  // Dependencies
  ConfigManager*        cfg_  = nullptr;
  SensorEspNowManager*  now_  = nullptr;
  TFLunaManager*        tf_   = nullptr;
  BME280Manager*        bme_  = nullptr;
  SwitchManager*        sw_   = nullptr;
  RGBLed*               led_  = nullptr;
  BuzzerManager*        buz_  = nullptr;

  // Tasks
  TaskHandle_t hBME_ = nullptr;
  TaskHandle_t hTF_  = nullptr;

  // State
  Mode      mode_ = IDLE;
  PresenceState prs_;
  uint32_t  lastBmeSentMs_ = 0;

  // Visuals
  uint32_t colorPairing_ = 0x0000FF; // blue
  uint32_t colorConfig_  = 0x00FF00; // green
};
