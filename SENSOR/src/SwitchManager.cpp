
#include "SwitchManager.h"

SwitchManager* SwitchManager::instance = nullptr;
extern DeviceState currentState; // if used elsewhere, keep extern

// Constructor
SwitchManager::SwitchManager(ConfigManager* Conf, ICMLogFS* LogFS)
: Conf(Conf), LogFS(LogFS) {
    DEBUG_PRINTLN("###########################################################");
    DEBUG_PRINTLN("#                  Starting Switch Manager                #");
    DEBUG_PRINTLN("###########################################################");
    // Print pin mapping
    DEBUG_PRINTLN("================ Switch Pin Map ==================");
    DEBUG_PRINTF("POWER_ON_SWITCH_PIN = GPIO %d (Boot / Mode button)\n", POWER_ON_SWITCH_PIN);
    DEBUG_PRINTLN("==================================================");
    pinMode(POWER_ON_SWITCH_PIN,INPUT_PULLUP);
    instance = this;  // Set the static instance pointer
}

void SwitchManager::actionLongHold_ResetDevice() {
    BlinkStatusLED(LED_G_PIN_DEFAULT, 100);
    DEBUG_PRINTLN("Long press detected 🕒  -> Factory Reset flag + reboot");
    if (LogFS) ICM_PWR_INFO((*LogFS), 101, "LONG_HOLD reset flag set, reboot soon");
    if (Conf) {
        Conf->PutBool(RESET_FLAG_KEY, true);   // Set the reset flag
        Conf->RestartSysDelayDown(3000);       // Delayed restart
    }
}

void SwitchManager::actionSingleTap() {
    BlinkStatusLED(LED_B_PIN_DEFAULT, 60);

}

void SwitchManager::actionTripleTap_SerialOnlyMode() {
    BlinkStatusLED(LED_R_PIN_DEFAULT, 120);
    DEBUG_PRINTLN("Triple tap detected 🖱️🖱️🖱️ -> Serial-only mode");
    if (LogFS) ICM_PWR_INFO((*LogFS), 103, "Entering SERIAL-ONLY mode; USB CLI enabled");
    if (Conf) {
        // Persist a flag if you want this to survive reboot; here we run inline mode
        Conf->PutBool(SERIAL_ONLY_FLAG_KEY, true);
    }
    if (LogFS) {
        Serial.println("\n=== SERIAL-ONLY MODE ===");
        Serial.println("USB CLI active. Type commands (e.g., FS.LS / LOG.LS / LOG.EVENT ...)");
        // Block this task into the CLI loop (other tasks keep running)
        LogFS->serveLoop(); // infinite; exit by reset
    }
}

void SwitchManager::detectTapOrHold() {
    uint8_t       tapCount    = 0;
    unsigned long pressStart  = 0;
    unsigned long lastTapTime = 0;

    while (true) {
        // Active-low pushbutton
        if (digitalRead(POWER_ON_SWITCH_PIN) == LOW) {
            pressStart = millis();

            // Wait while held
            while (digitalRead(POWER_ON_SWITCH_PIN) == LOW) {
                vTaskDelay(pdMS_TO_TICKS(10));
            }

            const unsigned long pressDuration = millis() - pressStart;

            // HOLD → long action, clear pending taps
            if (pressDuration >= HOLD_THRESHOLD_MS) {
                actionLongHold_ResetDevice();
                tapCount = 0;
                lastTapTime = 0;
                DEBUG_PRINTLN("Long hold detected ⏱️");
            }
            // Short press → accumulate taps
            else {
                tapCount++;
                lastTapTime = millis();
                DEBUG_PRINTF("Tap #%u\n", tapCount);

                // Immediate triple-tap path (within tight window)
                if (tapCount == 3) {
                    if ((millis() - lastTapTime) <= TAP_WINDOW_MS) {
                        actionTripleTap_ConfigTfAsB();
                        tapCount = 0;
                        lastTapTime = 0;
                        DEBUG_PRINTLN("Three taps detected 🖱️🖱️🖱️ -> TF-Luna B wizard");
                    } else {
                        // Window missed → reset the sequence
                        tapCount = 0;
                        lastTapTime = 0;
                        DEBUG_PRINTLN("Triple-tap window missed");
                    }
                }
            }
        }

        // If the tap sequence times out, resolve it
        if (tapCount > 0 && (millis() - lastTapTime) > TAP_TIMEOUT_MS) {
            if (tapCount == 1) {
                actionSingleTap();
                DEBUG_PRINTLN("One tap detected 🖱️");
            } else if (tapCount == 2) {
                actionDoubleTap_ConfigTfAsA();
                DEBUG_PRINTLN("Two taps detected 🖱️🖱️ -> TF-Luna A wizard");
            } else { // tapCount >= 3 (fallback if triple wasn't caught immediately)
                actionTripleTap_ConfigTfAsB();
                DEBUG_PRINTLN("Three+ taps detected 🖱️🖱️🖱️ -> TF-Luna B wizard");
            }
            tapCount = 0;
            lastTapTime = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(SWITCH_TASK_LOOP_DELAY_MS));
    }
}

// Global C-style FreeRTOS task function
void SwitchManager::SwitchTask(void* pvParameters) {
    for (;;) {
        if (SwitchManager::instance != nullptr) {
            SwitchManager::instance->detectTapOrHold();
        }
        vTaskDelay(pdMS_TO_TICKS(SWITCH_TASK_CALL_DELAY_MS));
    }
}

// Member function: launch RTOS task
void SwitchManager::TapDetect() {
    xTaskCreatePinnedToCore(
        SwitchTask,      // Pass the global function, not a member function
        "SwitchTask",
        SWITCH_TASK_STACK_SIZE,
        nullptr,         // No parameter needed since we use the static instance
        SWITCH_TASK_PRIORITY,
        nullptr,
        SWITCH_TASK_CORE
    );
}

void SwitchManager::actionDoubleTap_ConfigTfAsA() {
    BlinkStatusLED(LED_B_PIN_DEFAULT, 100);
    DEBUG_PRINTLN("Double tap -> TF-Luna address wizard (assign A)");
    if (!Tf) { DEBUG_PRINTLN("No TFLunaManager attached."); return; }
    if (Bz) Bz->bip(1100, 40); // entry beep

    Tf->begin(100, true); // make sure bus is up

    // Scan for exactly one TF-Luna on the bus
    uint8_t found = 0, addrFound = 0;
    for (uint8_t a = 0x08; a <= 0x77; ++a) {
        TFLunaManager::Sample s{};
        if (Tf->readOne(a, s)) { found++; addrFound = a; }
    }
    if (found != 1) {        // error: none or more than one
        DEBUG_PRINTF("WizardA: expected exactly 1 TF on bus, found=%u\n", found);
        if (Bz) { Bz->bip(400, 120); vTaskDelay(pdMS_TO_TICKS(80)); Bz->bip(400, 120); }
        return;
    }

    // Assign to A address (from NVS)
    uint8_t addrA = (uint8_t)Conf->GetInt(TFL_A_ADDR_KEY, TFL_A_ADDR_DEFAULT);
    bool ok = true;
    if (addrFound != addrA) {
        DEBUG_PRINTF("WizardA: setting TF at 0x%02X -> A(0x%02X)\n", addrFound, addrA);
        ok &= Tf->setAddresses(addrA, Tf->addrB());   // change A only
    } else {
        DEBUG_PRINTF("WizardA: already A (0x%02X), just enable\n", addrA);
        ok &= Tf->setEnable(true);
    }
    if (ok) { if (Bz) { Bz->bip(1200, 60); vTaskDelay(pdMS_TO_TICKS(50)); Bz->bip(1600, 80); } }
    else    { if (Bz) { Bz->bip(400, 160); vTaskDelay(pdMS_TO_TICKS(80)); Bz->bip(400, 160); } }
}

void SwitchManager::actionTripleTap_ConfigTfAsB() {
    BlinkStatusLED(LED_R_PIN_DEFAULT, 120);
    DEBUG_PRINTLN("Triple tap -> TF-Luna address wizard (assign B)");
    if (!Tf) { DEBUG_PRINTLN("No TFLunaManager attached."); return; }
    if (Bz) Bz->bip(1100, 40);

    Tf->begin(100, true);

    uint8_t found = 0, addrFound = 0;
    for (uint8_t a = 0x08; a <= 0x77; ++a) {
        TFLunaManager::Sample s{};
        if (Tf->readOne(a, s)) { found++; addrFound = a; }
    }
    if (found != 1) {
        DEBUG_PRINTF("WizardB: expected exactly 1 TF on bus, found=%u\n", found);
        if (Bz) { Bz->bip(400, 120); vTaskDelay(pdMS_TO_TICKS(80)); Bz->bip(400, 120); }
        return;
    }

    uint8_t addrB = (uint8_t)Conf->GetInt(TFL_B_ADDR_KEY, TFL_B_ADDR_DEFAULT);
    bool ok = true;
    if (addrFound != addrB) {
        DEBUG_PRINTF("WizardB: setting TF at 0x%02X -> B(0x%02X)\n", addrFound, addrB);
        ok &= Tf->setAddresses(Tf->addrA(), addrB);   // change B only
    } else {
        DEBUG_PRINTF("WizardB: already B (0x%02X), just enable\n", addrB);
        ok &= Tf->setEnable(true);
    }
    if (ok) { if (Bz) { Bz->bip(1200, 60); vTaskDelay(pdMS_TO_TICKS(50)); Bz->bip(1600, 80); } }
    else    { if (Bz) { Bz->bip(400, 160); vTaskDelay(pdMS_TO_TICKS(80)); Bz->bip(400, 160); } }
}
