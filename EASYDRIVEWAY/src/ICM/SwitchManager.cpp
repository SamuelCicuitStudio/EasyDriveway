#include "SwitchManager.h"
#include "WiFiManager.h"            // we only include in the .cpp
#include <driver/gpio.h>

SwitchManager* SwitchManager::instance = nullptr;

// ---- ctor ----
SwitchManager::SwitchManager(ConfigManager* Conf, ICMLogFS* LogFS)
: Conf(Conf), LogFS(LogFS) {
  DEBUG_PRINTLN("###########################################################");
  DEBUG_PRINTLN("#                  Starting Switch Manager                #");
  DEBUG_PRINTLN("###########################################################");
  DEBUG_PRINTLN("================ Switch Pin Map ==================");
  DEBUG_PRINTF("POWER_ON_SWITCH_PIN = GPIO %d (Boot / Mode button)\n", POWER_ON_SWITCH_PIN);
  DEBUG_PRINTLN("==================================================");

  pinMode(POWER_ON_SWITCH_PIN, INPUT_PULLUP);
  instance = this;
}

// ---- actions ----
void SwitchManager::actionLongHold_FactoryReset() {
  BlinkStatusLED(PIN_LED_G_DEFAULT, 100);
  DEBUG_PRINTLN("Long hold ⏱️ → mark factory-reset & reboot");
  if (LogFS) ICM_PWR_INFO((*LogFS), 101, "LONG_HOLD reset flag set, reboot soon");
  if (Conf) {
    Conf->PutBool(RESET_FLAG_KEY, true);
    Conf->RestartSysDelayDown(3000);
  }
}

void SwitchManager::actionSingleTap_ToggleAP() {
  BlinkStatusLED(PIN_LED_B_DEFAULT, 80);

  if (!WiFi) {
    DEBUG_PRINTLN("Single tap → WiFiManager not attached");
    if (Bz) { Bz->bip(400, 140); vTaskDelay(pdMS_TO_TICKS(80)); Bz->bip(400, 140); }
    return;
  }

  // If AP is on, turn it off; else force start it
  bool apOn = WiFi->isAPOn();     // new tiny helper
  if (apOn) {
    DEBUG_PRINTLN("Single tap → disable Wi-Fi AP");
    WiFi->disableWiFiAP();
    if (Bz) Bz->bip(900, 60);
  } else {
    DEBUG_PRINTLN("Single tap → start Wi-Fi AP (hotspot)");
    WiFi->forceAP();              // wraps private startAccessPoint()
    if (Bz) { Bz->bip(1200, 60); vTaskDelay(pdMS_TO_TICKS(50)); Bz->bip(1500, 80); }
  }
}

void SwitchManager::actionSerialOnlyMode() {
  BlinkStatusLED(PIN_LED_R_DEFAULT, 120);
  DEBUG_PRINTLN("Serial-only mode: enabling USB CLI");
  if (LogFS) ICM_PWR_INFO((*LogFS), 103, "Entering SERIAL-ONLY mode; USB CLI enabled");
  if (Conf)  Conf->PutBool(SERIAL_ONLY_FLAG_KEY, true);

  // Block into the CLI loop (other RTOS tasks continue); exit via reset
  if (LogFS) {
    Serial.println("\n=== SERIAL-ONLY MODE ===");
    Serial.println("USB CLI active. Type commands (e.g. FS.LS / LOG.LS / LOG.EVENT).");
    LogFS->serveLoop();
  }
}

// ---- detection loop ----
void SwitchManager::detectTapOrHold() {
  uint8_t       tapCount    = 0;
  unsigned long pressStart  = 0;
  unsigned long lastTapTime = 0;

  while (true) {
    // active-low
    if (digitalRead(POWER_ON_SWITCH_PIN) == LOW) {
      pressStart = millis();

      // wait while held
      while (digitalRead(POWER_ON_SWITCH_PIN) == LOW) {
        vTaskDelay(pdMS_TO_TICKS(10));
      }

      const unsigned long pressDuration = millis() - pressStart;

      if (pressDuration >= HOLD_THRESHOLD_MS) {
        // LONG HOLD
        actionLongHold_FactoryReset();
        tapCount = 0;
        lastTapTime = 0;
      } else {
        // SHORT PRESS → accumulate taps
        tapCount++;
        lastTapTime = millis();
        DEBUG_PRINTF("Tap #%u\n", tapCount);

        // optional: immediate triple path if you want it
        if (tapCount == 3 && (millis() - lastTapTime) <= TAP_WINDOW_MS) {
          actionSerialOnlyMode();
          tapCount = 0;
          lastTapTime = 0;
        }
      }
    }

    // resolve sequence on timeout
    if (tapCount > 0 && (millis() - lastTapTime) > TAP_TIMEOUT_MS) {
      if (tapCount == 1) {
        actionSingleTap_ToggleAP();
      } else {
        // double (or more) → serial-only
        actionSerialOnlyMode();
      }
      tapCount = 0;
      lastTapTime = 0;
    }

    vTaskDelay(pdMS_TO_TICKS(SWITCH_TASK_LOOP_DELAY_MS));
  }
}

// ---- RTOS task shim ----
void SwitchManager::SwitchTask(void* pvParameters) {
  if (SwitchManager::instance) {
    SwitchManager::instance->detectTapOrHold(); // never returns
  }
  vTaskDelete(nullptr);
}

// ---- spawn task ----
void SwitchManager::TapDetect() {
  xTaskCreatePinnedToCore(
    SwitchTask,
    "SwitchTask",
    SWITCH_TASK_STACK_SIZE,
    nullptr,
    SWITCH_TASK_PRIORITY,
    nullptr,
    SWITCH_TASK_CORE
  );
}
