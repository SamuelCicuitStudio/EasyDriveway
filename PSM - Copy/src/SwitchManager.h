
#ifndef SWITCH_MANAGER_H
#define SWITCH_MANAGER_H

#include "Arduino.h"
#include "ConfigManager.h"
#include "Config.h"
#include "Utils.h"
#include "PowerManager.h"
#include "ICMLogFS.h"

// ===============================
// Constants for Tap/Hold Detection
// ===============================
#define SWITCH_TASK_LOOP_DELAY_MS         20      // Polling loop
#define SWITCH_TASK_CALL_DELAY_MS         500     // Re-check cycle
#define TAP_TIMEOUT_MS                    1500
#define HOLD_THRESHOLD_MS                 3000
#define TAP_WINDOW_MS                     1200


class SwitchManager {
public:
    // Constructor
    SwitchManager(ConfigManager* Conf, PowerManager* Pwr, ICMLogFS* LogFS);

    // Detect user interaction
    void detectTapOrHold();
    void TapDetect();

    // RTOS-compatible task that uses detectTapOrHold()
    static void SwitchTask(void* pvParameters);
    static SwitchManager* instance;

    // Dependency setters (optional if created after)
    void attachPower(PowerManager* pwr) { Pwr = pwr; }
    void attachLogFS(ICMLogFS* fs)      { LogFS = fs; }

    // Expose for tests
    ConfigManager* Conf = nullptr;
    PowerManager*  Pwr  = nullptr;
    ICMLogFS*      LogFS = nullptr;

private:
    // Actions bound to gestures
    void actionLongHold_ResetDevice();
    void actionSingleTap_Toggle48V();
    void actionTripleTap_SerialOnlyMode();
};

#endif // SWITCH_MANAGER_H
