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
#include "PowerManager.h"
#include "CoolingManager.h"
#include "PSMEspNowManager.h"
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
static PowerManager*      gPwr   = nullptr;
static CoolingManager*    gCool  = nullptr;
static PSMEspNowManager*  gNow   = nullptr;
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

  // ---- Power domain manager ----
  gPwr = new PowerManager(gCfg);
  gPwr->begin();

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

  // --- Sample power & compose PSM status (every 500 ms) ---
  if (gPwr && (nowMs - gLastStatusMs >= 500)) {
    gLastStatusMs = nowMs;

    // Measurements
    const uint16_t v48mV  = gPwr->measure48V_mV();
    const uint16_t i48mA  = gPwr->measure48V_mA();
    const uint16_t vbatmV = gPwr->measureBat_mV();
    const uint16_t ibatmA = gPwr->measureBat_mA();
    const uint8_t  faults = gPwr->readFaultBits();
    const bool     mains  = gPwr->mainsPresent();

    // Compose buzzer/LED status (tune thresholds to your system)
    BuzzerManager::Status st{};
    st.linkUp      = (gNow != nullptr);           // refine with actual link state if available
    st.mains       = mains;
    st.onBattery   = (!mains && vbatmV > 9000);
    st.charging    = false;                       // set via charger status if available
    st.batFull     = false;
    st.lowBat      = (vbatmV > 0 && vbatmV < 11400);  // ~11.4 V for 12 V pack (example)
    st.rail48V     = (v48mV > 10000);            // treat >10 V as "on" (adjust to UVP)
    st.overTemp    = false;                       // fill from CoolingManager if you expose it
    st.overCurrent = (faults & 0x02) != 0;       // adjust to your fault bit map
    st.commError   = false;
    st.fault       = (faults != 0);

    // LED policy
    if (gLED) {
      if (st.fault || st.overTemp || st.overCurrent) {
        gLED->startBlink(0xFF0000, 250);   // red fast
      } else if (!st.mains) {
        gLED->startBlink(0xFFFF00, 600);   // yellow slow (battery)
      } else if (st.rail48V) {
        gLED->startBlink(0x00FF00, 800);   // green slow (48 V enabled)
      } else {
        gLED->setColorHex(0x0033FF);       // blue idle
      }
    }

    // Buzzer on edges/critical changes
    if (gBuzz) {
      gBuzz->playFromStatus(st, &gPrevStatus);
    }

    // Notify master on edges or every 5 s
    const bool edge =
      (st.mains     != gPrevStatus.mains)     ||
      (st.onBattery != gPrevStatus.onBattery) ||
      (st.lowBat    != gPrevStatus.lowBat)    ||
      (st.rail48V   != gPrevStatus.rail48V)   ||
      (st.fault     != gPrevStatus.fault);

    if (gNow && (edge || (nowMs - gLastSendMs >= 5000))) {
      gNow->sendPowerStatus(0);  // let class format payload/counter
      gLastSendMs = nowMs;
    }

    // Debug heartbeat
    Serial.printf(
      "[HB] V48=%u mV I48=%u mA | VBAT=%u mV IBAT=%u mA | mains=%d faults=0x%02X\n",
      v48mV, i48mA, vbatmV, ibatmA, mains ? 1 : 0, faults
    );

    gPrevStatus = st;
  }
}
