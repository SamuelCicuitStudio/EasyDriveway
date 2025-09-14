#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>              // RTC_DS3231

#include "ConfigManager.h"
#include "Utils.h"
#include "WiFiManager.h"
#include "BleICM.h"
#include "EspNow/ICM_Nw.h"
#include "Pin/ICM_PinsNVS.h"
#include "Peripheral/BuzzerManager.h"
#include "SwitchManager.h"
#include "Peripheral/SleepTimer.h"
#include "Peripheral/ICMLogFS.h"
#include "Peripheral/RTCManager.h"
#include "Peripheral/RGBLed.h"
#include "Peripheral/CoolingManager.h"

// -------------------- NVS --------------------
Preferences gPrefs;

// -------------------- I2C / RTC --------------------
TwoWire*   gI2C1 = &Wire;   // classes do Wire.begin() internally per your design
RTC_DS3231 gRtc;            // passed to RTCManager

// -------------------- Managers (all pointers) --------------------
ConfigManager*  gCfg     = nullptr;
ICMLogFS*       gLog     = nullptr;
RTCManager*     gRTC     = nullptr;
RGBLed*         gLED     = nullptr;
BuzzerManager*  gBuzzer  = nullptr;
CoolingManager* gCooling = nullptr;
NwCore::Core*   gNow     = nullptr;
WiFiManager*    gWifi    = nullptr;
SleepTimer*     gSleep   = nullptr;
SwitchManager*  gSwitch  = nullptr;

// -------------------- Safety helper --------------------
#define REQUIRE(P) do{ if(!(P)){ Serial.printf("[FATAL] %s is null @ %s:%d\n", #P, __FILE__, __LINE__); for(;;){ delay(500);} } }while(0)

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  delay(30);
  Serial.println("\n=== BOOT: bring-up start ===");

  // ======================================================
  // A) CONSTRUCT — allocate, no cross-calls or begins here
  // ======================================================
  gPrefs.begin(CONFIG_PARTITION, /*readWrite=*/false);

  WiFi.mode(WIFI_AP_STA);

  gCfg     = new ConfigManager(&gPrefs);                      REQUIRE(gCfg);
  gLED     = new RGBLed(gCfg);                                REQUIRE(gLED);
  gLog     = new ICMLogFS(Serial, gCfg, gLED);                REQUIRE(gLog);
  gRTC     = new RTCManager(gCfg, &gRtc, gI2C1);              REQUIRE(gRTC);
  gBuzzer  = new BuzzerManager(gCfg);                         REQUIRE(gBuzzer);
  gCooling = new CoolingManager(gCfg);                        REQUIRE(gCooling);
  gNow     = new NwCore::Core();                              REQUIRE(gNow);
  gSleep   = new SleepTimer(&gRtc, gCfg, gLog);               REQUIRE(gSleep);
  gWifi    = new WiFiManager(gCfg, &WiFi, gNow, gLog, gSleep);REQUIRE(gWifi);
  gSwitch  = new SwitchManager(gCfg);                         REQUIRE(gSwitch);

  // ======================================================
  // B) ATTACH — wire all cross references, still no begin()
  // ======================================================
  gLED->attachLog(gLog);

  gLog->attachRTC(gRTC);
  gRTC->setLogger(gLog);

  gCooling->attachLogger(gLog);
  gSleep->setLogger(gLog);

  gNow->attachCfg(gCfg);
  gNow->attachLog(gLog);
  gNow->attachRtc(gRTC);

  gWifi->attachLog(gLog);
  gWifi->attachNow(gNow);
  gWifi->attachSlp(gSleep);

  gSwitch->attachBuzzer(gBuzzer);
  gSwitch->attachLog(gLog);
  gSwitch->attachWiFi(gWifi);

  // ======================================================
  // C) START — explicit begin() in dependency order
  // ======================================================
  Serial.println("-- BEGIN: ConfigManager");
  gCfg->begin();                       // load pins, network, etc.

  Serial.println("-- BEGIN: ICMLogFS");
  gLog->beginFromConfig();             // mount FS early (needs cfg)

  Serial.println("-- BEGIN: RTCManager");
  gRTC->begin();                       // sets up I2C/RTC

  Serial.println("-- BEGIN: SleepTimer");
  gSleep->begin();                     // uses RTC + cfg

  Serial.println("-- BEGIN: RGBLed");
  gLED->begin();                       // LED pins from cfg; logger attached

  Serial.println("-- BEGIN: BuzzerManager");
  gBuzzer->begin();                    // buzzer GPIOs

  Serial.println("-- BEGIN: CoolingManager");
  gCooling->begin();                   // fan/PWM pins, thresholds

  Serial.println("-- BEGIN: ESP-NOW Core");
  gNow->begin(1);                       // uses cfg/log/rtc; sets radio mode

  Serial.println("-- BEGIN: WiFiManager");
  gWifi->begin();                      // needs now/log/sleep/cfg

  Serial.println("-- BEGIN: SwitchManager");
  gSwitch->TapDetect();                // start tap/gesture worker last

  Serial.println("=== BOOT: bring-up complete ===");
}

void loop() {
  // App loop; managers run their own tasks/processing.
  delay(50);
}
