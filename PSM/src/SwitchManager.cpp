
#include "SwitchManager.h"

SwitchManager* SwitchManager::instance = nullptr;
extern DeviceState currentState; // if used elsewhere, keep extern

// Constructor
SwitchManager::SwitchManager(ConfigManager* Conf, PowerManager* Pwr, ICMLogFS* LogFS)
: Conf(Conf), Pwr(Pwr), LogFS(LogFS) {
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

void SwitchManager::actionSingleTap_Toggle48V() {
    BlinkStatusLED(LED_B_PIN_DEFAULT, 60);
    bool newState = false;
    if (Pwr) {
        bool on = Pwr->is48VOn();
        newState = !on;
        Pwr->set48V(newState);
        if (LogFS) ICM_PWR_INFO((*LogFS), 102, newState ? "48V rail -> ON" : "48V rail -> OFF");
        DEBUG_PRINTLN(newState ? "Single tap -> 48V ON" : "Single tap -> 48V OFF");
    } else {
        DEBUG_PRINTLN("Single tap but PowerManager not attached");
    }
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
    uint8_t tapCount = 0;
    unsigned long pressStart = 0;
    unsigned long lastTapTime = 0;

    while (true) {
        if (digitalRead(POWER_ON_SWITCH_PIN) == LOW) {
            pressStart = millis();

            // Wait until button is released
            while (digitalRead(POWER_ON_SWITCH_PIN) == LOW) {
                vTaskDelay(pdMS_TO_TICKS(10));
            }

            unsigned long pressDuration = millis() - pressStart;

            // If press lasted longer than threshold → HOLD
            if (pressDuration >= HOLD_THRESHOLD_MS) {
                actionLongHold_ResetDevice();
                tapCount = 0;                      // Cancel any tap sequence
            }
            // Otherwise → count as tap
            else {
                tapCount++;
                lastTapTime = millis();
            }

            // Triple-tap detection
            if (tapCount == 3) {
                if ((millis() - lastTapTime) <= TAP_WINDOW_MS) {
                    actionTripleTap_SerialOnlyMode();
                    tapCount = 0;
                } else {
                    tapCount = 0;
                }
            }
        }

        // Timeout to reset tap sequence
        if ((millis() - lastTapTime) > TAP_TIMEOUT_MS && tapCount > 0) {
            if (tapCount == 1) {
                actionSingleTap_Toggle48V();
                tapCount = 0;
                DEBUG_PRINTLN("One tap detected 🖱️");
            } else {
                tapCount = 0;
                DEBUG_PRINTLN("Tap timeout ⏱️");
            }
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
