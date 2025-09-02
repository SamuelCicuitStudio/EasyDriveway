#include <Arduino.h>
#include <Preferences.h>

// Core config
#include "Config.h"
#include "ConfigManager.h"

// Subsystems
#include "ICMLogFS.h"
#include "RTCManager.h"
#include "RGBLed.h"
#include "BuzzerManager.h"
#include "CoolingManager.h"
#include "SensorEspNowManager.h"
#include "SwitchManager.h"

// =============================
// Globals (owned singletons)
// =============================
static Preferences        gPrefs;
static ConfigManager*     gCfg   = nullptr;
static ICMLogFS*          gLog   = nullptr;
static RTCManager*        gRTC   = nullptr;
static RGBLed*            gLED   = nullptr;
static BuzzerManager*     gBuzz  = nullptr;
static CoolingManager*    gCool  = nullptr;
static SensorEspNowManager*gNow   = nullptr;
static SwitchManager*     gSw    = nullptr;

// Status shadow for buzzer/LED/ESP-NOW notifications
static BuzzerManager::Status gPrevStatus{};
static uint32_t              gLastStatusMs = 0;
static uint32_t              gLastSendMs   = 0;

// =============================
// Arduino setup/loop
// =============================
void setup()
{
  Serial.begin(921600);
  delay(50);
  Serial.println();
  Serial.println(F("###########################################################"));
  Serial.println(F("#          Starting System Setup @ 921600 baud ⚙️           #"));
  Serial.println(F("###########################################################"));

  // ---- NVS ----
  Serial.println(F("[Setup] Initializing NVS (Preferences)..."));
  gPrefs.begin(CONFIG_PARTITION, /*readOnly=*/false);
  Serial.println(F("[Setup] NVS Initialized. ✅"));

  // ---- Config ----
  gCfg = new ConfigManager(&gPrefs);
  gCfg->begin();

  // ---- LogFS (SD) ----
  gLog = new ICMLogFS(Serial, gCfg);
  gLog->attachConfig(gCfg);

  // ---- RTC (ESP32 system clock) ----
  gRTC = new RTCManager(gCfg);
  gRTC->setLogger(gLog);
  gRTC->begin();

  // Connect RTC to LogFS (timestamps), then init SD
  gLog->attachRTC(gRTC);
  if (gLog->beginFromConfig()) {
    gLog->cardInfo();
  } else {
    Serial.println(F("[Setup] SD init failed — continuing without log storage."));
  }

  // ---- RGB LED ----
  gLED = new RGBLed(gCfg, gLog);
  if (gLED->begin()) {
    gLED->startBlink(/*white*/0xFFFFFF, /*ms*/400);
  }

  // ---- Buzzer ----
  gBuzz = new BuzzerManager(gCfg);
  if (gBuzz->begin()) {
    gBuzz->play(BuzzerManager::EV_STARTUP);
  }


  // ---- Cooling manager (fan + DS18B20) ----
  gCool = new CoolingManager(gCfg, gLog);
  gCool->begin();

  // ---- ESP-NOW manager (PSM slave) ----
  gNow = new PSMEspNowManager(gCfg, gLog, gRTC, gPwr, gCool);
  gNow->begin(gCfg->GetInt(ESPNOW_CH_KEY, (int)ESPNOW_CH_DEFAULT), /*pmk16*/nullptr);

  // ---- Switch manager (tap/hold gestures) ----
  gSw = new SwitchManager(gCfg, gPwr, gLog);
  gSw->TapDetect();

  Serial.println(F("[Setup] ✅ All subsystems initialized."));
}

void loop()
{
  // --- Serial-only mode (blocks here in CLI) ---
  if (gCfg && gCfg->GetBool(SERIAL_ONLY_FLAG_KEY, false)) {
    if (gLog) {
      Serial.println(F("\n=== SERIAL-ONLY MODE ACTIVE ==="));
      Serial.println(F("Type FS.LS / LOG.LS / LOG.EVENT ..."));
      gLog->serveLoop();           // infinite CLI loop: serveOnce(10) + delay(1)
    }
    // If no LogFS, just idle
    while (true) { delay(1000); }
  }

  // --- Normal PSM logic ---
  // ESP-NOW housekeeping
  if (gNow) gNow->poll();

  // UART command server for LogFS (non-blocking)
  if (gLog) gLog->serveOnce(2);

  const uint32_t nowMs = millis();

 }
