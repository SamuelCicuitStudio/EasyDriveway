/**************************************************************
 *  Project : EasyDriveway
 *  File    : SwitchManager.cpp
 **************************************************************/
#include "SwitchManager.h"
#if defined(NVS_ROLE_ICM)
  #include "WiFiManager.h"
#endif

SwitchManager* SwitchManager::instance = nullptr;
SwitchManager::SwitchManager(NvsManager* Conf, LogFS* log): Conf(Conf), log(log) {
  DEBUG_PRINTLN("###########################################################");
  DEBUG_PRINTLN("#                  Starting Switch Manager                #");
  DEBUG_PRINTLN("###########################################################");
  DEBUG_PRINTLN("================ Switch Pin Map ==================");
  DEBUG_PRINTF("POWER_ON_SWITCH_PIN = GPIO %d (Boot / Mode button)\n", POWER_ON_SWITCH_PIN);
  DEBUG_PRINTLN("==================================================");
  pinMode(POWER_ON_SWITCH_PIN, INPUT_PULLUP);
  instance = this;
}
void SwitchManager::actionLongHold_FactoryReset() {
  BlinkStatusLED(LED_ONBOARD_PIN, 100);
  DEBUG_PRINTLN("Long hold ⏱️ → mark factory-reset & reboot");
  if (log) LOGFS_PWR_INFO((*log), 101, "LONG_HOLD reset flag set, reboot soon");
  if (Conf) { Conf->PutBool(RESET_FLAG_KEY, true); Conf->RestartSysDelayDown(3000); }
}
void SwitchManager::actionSingleTap_ToggleAP() {
  BlinkStatusLED(LED_ONBOARD_PIN, 80);
#if defined(NVS_ROLE_ICM)
  if (!WiFi) { DEBUG_PRINTLN("Single tap → WiFiManager not attached"); if (Bz) { Bz->bip(400,140); vTaskDelay(pdMS_TO_TICKS(80)); Bz->bip(400,140);} return; }
  bool apOn = WiFi->isAPOn(); 
  if (apOn) { DEBUG_PRINTLN("Single tap → disable Wi-Fi AP"); /* WiFi->disableWiFiAP(); */ if (Bz) Bz->bip(900,60); }
  else { DEBUG_PRINTLN("Single tap → start Wi-Fi AP (hotspot)"); /* WiFi->forceAP(); */ if (Bz) { Bz->bip(1200,60); vTaskDelay(pdMS_TO_TICKS(50)); Bz->bip(1500,80);} }
#else
  actionSingleTap_PrintMac();
#endif
}
void SwitchManager::actionSingleTap_PrintMac() {
  BlinkStatusLED(LED_ONBOARD_PIN, 80);
  uint8_t mac[6] = {0};
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char macStr[18]; snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  Serial.print("MAC: "); Serial.println(macStr);
  if (log) LOGFS_PWR_INFO((*log), 102, "PRINT_MAC %s", macStr);
  if (Bz) { Bz->bip(1100,60); vTaskDelay(pdMS_TO_TICKS(40)); Bz->bip(1100,60); }
}
void SwitchManager::actionSerialOnlyMode() {
  BlinkStatusLED(LED_ONBOARD_PIN, 120);
  DEBUG_PRINTLN("Serial-only mode: enabling USB CLI");
  if (log) LOGFS_PWR_INFO((*log), 103, "Entering SERIAL-ONLY mode; USB CLI enabled");
  //if (Conf)  Conf->PutBool(SERIAL_ONLY_FLAG_KEY, true);
  if (log) { Serial.println("\n=== SERIAL-ONLY MODE ==="); Serial.println("USB CLI active. Type commands (e.g. FS.LS / LOG.LS / LOG.EVENT)."); log->serveLoop(); }
}
void SwitchManager::detectTapOrHold() {
  uint8_t tapCount = 0; unsigned long pressStart = 0; unsigned long lastTapTime = 0;
  while (true) {
    if (digitalRead(POWER_ON_SWITCH_PIN) == LOW) {
      pressStart = millis();
      while (digitalRead(POWER_ON_SWITCH_PIN) == LOW) { vTaskDelay(pdMS_TO_TICKS(10)); }
      const unsigned long pressDuration = millis() - pressStart;
      if (pressDuration >= HOLD_THRESHOLD_MS) { actionLongHold_FactoryReset(); tapCount = 0; lastTapTime = 0; }
      else {
        tapCount++; lastTapTime = millis(); DEBUG_PRINTF("Tap #%u\n", tapCount);
        if (tapCount == 3 && (millis() - lastTapTime) <= TAP_WINDOW_MS) { actionSerialOnlyMode(); tapCount = 0; lastTapTime = 0; }
      }
    }
    if (tapCount > 0 && (millis() - lastTapTime) > TAP_TIMEOUT_MS) {
#if defined(NVS_ROLE_ICM)
      if (tapCount == 1) { actionSingleTap_ToggleAP(); }
      else { actionSerialOnlyMode(); }
#else
      if (tapCount == 1) { actionSingleTap_PrintMac(); }
      else { actionSerialOnlyMode(); }
#endif
      tapCount = 0; lastTapTime = 0;
    }
    vTaskDelay(pdMS_TO_TICKS(SWITCH_TASK_LOOP_DELAY_MS));
  }
}
void SwitchManager::SwitchTask(void* pvParameters) {
  if (SwitchManager::instance) { SwitchManager::instance->detectTapOrHold(); }
  vTaskDelete(nullptr);
}
void SwitchManager::TapDetect() {
  xTaskCreatePinnedToCore(SwitchTask, "SwitchTask", SWITCH_TASK_STACK_SIZE, nullptr, SWITCH_TASK_PRIORITY, nullptr, SWITCH_TASK_CORE);
}
