// =============================
// File: BleICM.h
// =============================
#pragma once

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include "esp_mac.h"

#include "Config.h"
#include "ConfigManager.h"
#include "ESPNowManager.h"
#include "ICMLogFS.h"
class WiFiManager; // fwd decl to avoid heavy include in header

// -------------------------------------------------------------
// Globals (similar pattern to your CarLock class)
// -------------------------------------------------------------
extern BLEServer*        ICM_pServer;
extern BLEService*       ICM_Service;

extern BLECharacteristic* ICM_chStatus;  // Read/Notify current status snapshot
extern BLECharacteristic* ICM_chWifi;    // Write commands to control STA/AP
extern BLECharacteristic* ICM_chPeers;   // Pair/remove/list peers
extern BLECharacteristic* ICM_chTopo;    // Set/get topology
extern BLECharacteristic* ICM_chSeq;     // Start/stop sequences
extern BLECharacteristic* ICM_chPower;   // Power info/commands
extern BLECharacteristic* ICM_chExport;  // Export/Import configuration
extern BLECharacteristic* ICM_chOldApp;  // Back-compat or simple text cmds

extern BLEDescriptor*     ICM_descStatus;
extern BLEDescriptor*     ICM_descWifi;
extern BLEDescriptor*     ICM_descPeers;
extern BLEDescriptor*     ICM_descTopo;
extern BLEDescriptor*     ICM_descSeq;
extern BLEDescriptor*     ICM_descPower;
extern BLEDescriptor*     ICM_descExport;
extern BLEDescriptor*     ICM_descOldApp;

extern bool               ICM_isPaired;
extern uint64_t           ICM_AdvStartMs;
extern char               ICM_remoteAddr[18];

// Helper to remove all bonded devices (same utility style you used)
static inline void ICM_removeBonded() {
  int n = esp_ble_get_bond_device_num();
  if (n <= 0) return;
  auto* list = (esp_ble_bond_dev_t*)malloc(sizeof(esp_ble_bond_dev_t) * n);
  if (!list) return;
  esp_ble_get_bond_device_list(&n, list);
  for (int i=0;i<n;i++) esp_ble_remove_bond_device(list[i].bd_addr);
  free(list);
}

// -------------------------------------------------------------
// BleICM class
// -------------------------------------------------------------
class BleICM {
public:
  BleICM(ConfigManager* cfg, ESPNowManager* esn, WiFiManager* wifiMgr, ICMLogFS* log);

  void begin();
  void bleSecurity();
  void restartAdvertising();

  // status push
  void notifyStatus();

  // lifecycle hooks for callbacks
  void onBLEConnected();
  void onBLEDisconnected();

  // singleton
  static BleICM* instance;

  // exposed collaborators
  ConfigManager* Cfg;
  ESPNowManager* Esn;
  WiFiManager*   WMgr;
  ICMLogFS*      Log;
  uint8_t _pinB = LED_B_PIN_DEFAULT;

  // optional LED task (blink while advertising)
  static void advLedTask(void* pv);
  static TaskHandle_t advLedTaskHandle;
  void startAdvLedTask();
  void stopAdvLedTask();
};

// -------------------------------------------------------------
// Security / Server / Characteristic callbacks in-header (as requested)
// -------------------------------------------------------------
class BleICMSecurityCB : public BLESecurityCallbacks {
public:
  uint32_t onPassKeyRequest() override;
  void onPassKeyNotify(uint32_t pass_key) override;
  bool onConfirmPIN(uint32_t pass_key) override;
  bool onSecurityRequest() override;
  void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) override;
};

class BleICMServerCB : public BLEServerCallbacks {
  void onConnect(BLEServer* server, esp_ble_gatts_cb_param_t* param) override;
  void onDisconnect(BLEServer* server) override;
};

class BleICMCharCB : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* ch) override;
  void onRead (BLECharacteristic* ch) override;
};


