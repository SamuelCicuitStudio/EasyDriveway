#include <Arduino.h>
#include "main.h"   // App_BringUpAll(), App_MainLoop()

// ========== Global singletons (definitions) ==========
// Preferences (NVS)
Preferences        gPrefs;

// Dual I2C buses
TwoWire*           gI2C1     = nullptr;  // I2C1 → TF‑Luna A/B
TwoWire*           gI2C2     = nullptr;  // I2C2 → BME/ALS

// Managers
ConfigManager*       gCfg     = nullptr;
ICMLogFS*            gLog     = nullptr;
RTCManager*          gRTC     = nullptr;
RGBLed*              gLED     = nullptr;
BuzzerManager*       gBuzzer  = nullptr;
BME280Manager*       gBME     = nullptr;
VEML7700Manager*      gALS     = nullptr;
TFLunaManager*       gTF      = nullptr;
CoolingManager*      gCooling = nullptr;
SensorEspNowManager* gNow     = nullptr;
SwitchManager*       gSwitch  = nullptr;

// High‑level controller
Device*              gDevice  = nullptr;

void setup() {
  App_BringUpAll();   // bring up everything and start Device tasks
}

void loop() {
  App_MainLoop();     // housekeeping + state machine
}
