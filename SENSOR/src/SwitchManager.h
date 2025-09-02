
#ifndef SWITCH_MANAGER_H
#define SWITCH_MANAGER_H

#include "Arduino.h"
#include "ConfigManager.h"
#include "Config.h"
#include "Utils.h"
#include "ICMLogFS.h"
#include "TFLunaManager.h"
#include "BuzzerManager.h"

// ===============================
// Constants for Tap/Hold Detection
// ===============================
#define SWITCH_TASK_LOOP_DELAY_MS         20      // Polling loop
#define SWITCH_TASK_CALL_DELAY_MS         500     // Re-check cycle
#define TAP_TIMEOUT_MS                    1500
#define HOLD_THRESHOLD_MS                 3000
#define TAP_WINDOW_MS                     1200
#define POWER_ON_SWITCH_PIN BOOT_SW_PIN
#define SWITCH_TASK_STACK_SIZE 2048
#define SWITCH_TASK_PRIORITY 1
#define SWITCH_TASK_CORE 1

class SwitchManager {
public:
    // Constructor
    SwitchManager(ConfigManager* Conf,ICMLogFS* LogFS);

    // Detect user interaction
    void detectTapOrHold();
    void TapDetect();
    void attachTFLuna(TFLunaManager* m) { Tf = m; }
    void attachBuzzer(BuzzerManager* b) { Bz = b; }
    void actionDoubleTap_ConfigTfAsA();
    void actionTripleTap_ConfigTfAsB();

    // RTOS-compatible task that uses detectTapOrHold()
    static void SwitchTask(void* pvParameters);
    static SwitchManager* instance;

    // Dependency setters (optional if created after)
    void attachLogFS(ICMLogFS* fs)      { LogFS = fs; }

    // Expose for tests
    ConfigManager* Conf = nullptr;
    ICMLogFS*      LogFS = nullptr;
    TFLunaManager* Tf  = nullptr;
    BuzzerManager*  Bz = nullptr;

private:
    // Actions bound to gestures
    void actionLongHold_ResetDevice();
    void actionSingleTap();
    void actionTripleTap_SerialOnlyMode();
};

#endif // SWITCH_MANAGER_H
