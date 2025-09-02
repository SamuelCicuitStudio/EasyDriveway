#pragma once
/**
 * @file main.h
 * @brief Single-include bring-up for the Sensor board. Creates and starts:
 *        - NVS (Preferences) + ConfigManager
 *        - Dual I2C buses (I2C1 for TF‑Luna A/B, I2C2 for BME/ALS)
 *        - RTC, LogFS (SD), RGB LED, Buzzer, Cooling
 *        - ESP‑NOW manager + SwitchManager
 *        - High-level Device (RTOS controller)
 *
 * Call:
 *   #include "main.h"
 *   void setup() { App_BringUpAll(); }
 *   void loop()  { App_MainLoop();   }
 */
#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>

#include "Config.h"
#include "ConfigManager.h"
#include "ICMLogFS.h"
#include "RTCManager.h"
#include "RGBLed.h"
#include "BuzzerManager.h"
#include "CoolingManager.h"
#include "SensEspNow.h"
#include "SwitchManager.h"
#include "TFLunaManager.h"
#include "BME280Manager.h"
#include "VEML7700Manager.h"
#include "Device.h"

// ========== Globals (defined in your main.cpp) ==========
extern Preferences        gPrefs;

extern TwoWire*           gI2C1;     // I2C1 → TF‑Luna A/B
extern TwoWire*           gI2C2;     // I2C2 → BME280 (+ optional ALS)

extern ConfigManager*     gCfg;
extern ICMLogFS*          gLog;
extern RTCManager*        gRTC;
extern RGBLed*            gLED;
extern BuzzerManager*     gBuzzer;
extern BME280Manager*     gBME;
extern VEML7700Manager*  gALS;
extern TFLunaManager*     gTF;
extern CoolingManager*    gCooling;
extern SensorEspNowManager* gNow;
extern SwitchManager*     gSwitch;
extern Device*            gDevice;

// ===== Helpers =====
static inline void App_InitSerial(uint32_t baud = SERIAL_BAUD_RATE) {
  Serial.begin(baud);
  while (!Serial && millis() < 1500) { /* wait for USB CDC if present */ }
}

static inline void App_BeginNVS() {
  if (!gPrefs.begin(CONFIG_PARTITION, /*readOnly=*/false)) {
    Serial.println(F("[NVS] begin() failed"));
  }
}

// Create or reconfigure both I2C buses from NVS pins
static inline void App_InitI2C(uint32_t hz = 400000UL) {
#if defined(ESP32)
  if (!gCfg) return;
  int sda1 = gCfg->GetInt(I2C1_SDA_PIN_KEY, I2C1_SDA_PIN_DEFAULT);
  int scl1 = gCfg->GetInt(I2C1_SCL_PIN_KEY, I2C1_SCL_PIN_DEFAULT);
  int sda2 = gCfg->GetInt(I2C2_SDA_PIN_KEY, I2C2_SDA_PIN_DEFAULT);
  int scl2 = gCfg->GetInt(I2C2_SCL_PIN_KEY, I2C2_SCL_PIN_DEFAULT);

  if (!gI2C1) gI2C1 = new TwoWire(0);
  if (!gI2C2) gI2C2 = new TwoWire(1);

  gI2C1->begin(sda1, scl1, hz);
  gI2C2->begin(sda2, scl2, hz);
#else
  // Single bus fallback
  if (!gI2C1) gI2C1 = &Wire;
  if (!gI2C2) gI2C2 = &Wire;
  gI2C1->begin();
  gI2C2->begin();
#endif
}

// Initialize SD LogFS, attach RTC for timestamps
static inline void App_InitLogFS() {
  if (!gLog) gLog = new ICMLogFS(Serial, gCfg);
  gLog->attachConfig(gCfg);
  gLog->attachRTC(gRTC);
  if (gLog->beginFromConfig()) {
    gLog->cardInfo();
  } else {
    Serial.println(F("[LogFS] SD init failed — running without external storage."));
  }
}

// Instantiate and start all components (idempotent)
static inline void App_BringUpAll() {
  App_InitSerial();
  App_BeginNVS();

  // ---- Config ----
  if (!gCfg) gCfg = new ConfigManager(&gPrefs);
  gCfg->begin();
  // gCfg->initializeVariables(); // (call once on factory reset)

  // ---- RTC ----
  if (!gRTC) gRTC = new RTCManager(gCfg);
  gRTC->begin();

  // ---- LogFS ----
  App_InitLogFS();

  // ---- LED & Buzzer ----
  if (!gLED)    gLED    = new RGBLed(gCfg, gLog);
  if (!gBuzzer) gBuzzer = new BuzzerManager(gCfg);
  gLED->begin();
  gBuzzer->begin();

  // ---- I2C buses ----
  App_InitI2C();

  // ---- Sensors ----
  if (!gBME) gBME = new BME280Manager(gCfg, gI2C2);
  gBME->begin();  // reads pins/addr from NVS, uses gI2C2

  if (!gTF)  gTF  = new TFLunaManager(gCfg, gI2C1);
  gTF->begin(/*fps_hz*/100, /*continuous*/true);

  // ---- Cooling (uses BME for temperature) ----
  if (!gCooling) gCooling = new CoolingManager(gCfg, gBME, gLog);
  gCooling->begin();

  // ---- ESP‑NOW manager ----
  if (!gNow) gNow = new SensorEspNowManager(gCfg);
  gNow->attachBME(gBME);
  // If you have ALS manager, attach here: gNow->attachALS(gALS);
  gNow->attachTF(gTF);
  gNow->begin(gCfg->GetInt(SSM_ESPNOW_CH_KEY, gCfg->GetInt(ESPNOW_CH_KEY, (int)ESPNOW_CH_DEFAULT)), /*pmk16*/nullptr);

  // ---- Switch manager ----
  if (!gSwitch) gSwitch = new SwitchManager(gCfg,gLog);
  // Expose optional deps (public members in your SwitchManager)
  gSwitch->Conf = gCfg;
  gSwitch->LogFS= gLog;
  gSwitch->Tf   = gTF;
  gSwitch->Bz   = gBuzzer;
  gSwitch->TapDetect();   // start gesture task

  // ---- High-level Device ----
  if (!gDevice) gDevice = new Device(gCfg, gNow, gTF, gBME, gSwitch, gLED, gBuzzer);
  gDevice->begin();
}

// One-stop loop integration
static inline void App_MainLoop() {
  if (gNow) gNow->poll();
  if (gLog) gLog->serveOnce(2);
  if (gDevice) gDevice->loopOnce();
}
