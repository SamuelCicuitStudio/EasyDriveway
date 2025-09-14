#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>
#include "esp_mac.h"

// Project config
#include "Config/Config_ICM.h"   // ICM_PIN_LED_B_DEFAULT etc.
#include "Config/NVSConfig.h"    // NVS keys/defs
#include "ConfigManager.h"

// ICM core (ESPNOW)
#include "EspNow/ICM_Nw.h"       // NwCore::Core (ICM networking)
#include "WiFiManager.h"   // safe to include even if unused
#include "Peripheral/ICMLogFS.h"
// Optional logging + WiFi manager (leave included; safe if you don’t use)
class ICMLogFS;
class WiFiManager;

namespace NwCore { class Core; }

// ---------------------- BLE objects (global) ----------------------
extern BLEServer*         ICM_pServer;
extern BLEService*        ICM_Service;

extern BLECharacteristic* ICM_chStatus;
extern BLECharacteristic* ICM_chWifi;
extern BLECharacteristic* ICM_chPeers;
extern BLECharacteristic* ICM_chTopo;
extern BLECharacteristic* ICM_chSeq;
extern BLECharacteristic* ICM_chPower;
extern BLECharacteristic* ICM_chExport;
extern BLECharacteristic* ICM_chOldApp;

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

// --------------------------- BleICM ---------------------------
class BleICM {
public:
  BleICM(ConfigManager* cfg, NwCore::Core* esn, WiFiManager* wifiMgr, ICMLogFS* log);

  void begin();
  void bleSecurity();
  void restartAdvertising();
  void notifyStatus();
  void onBLEConnected();
  void onBLEDisconnected();

  // blinking LED while advertising
  static void        advLedTask(void* pv);
  void               startAdvLedTask();
  void               stopAdvLedTask();
  bool addPeersFromRegistry(uint8_t ch);

  // collaborators
  static BleICM*     instance;
  static TaskHandle_t advLedTaskHandle;

  ConfigManager*     Cfg;
  NwCore::Core*      Esn;
  WiFiManager*       WMgr;
  ICMLogFS*          Log;

  // blue LED pin (from NVS; fallback to default)
  int                _pinB = PIN_LED_B_DEFAULT;

  // ---- Grant BLE characteristic callback access to private ESPNOW adapters ----
  // (Fixes “inaccessible” errors without changing method visibility or signatures)
  friend struct BleICMChCB;
  // (Optional, harmless)
  friend struct BleICMServerCB;
  friend struct BleICMSecurityCB;

private:
  // --------- ESPNOW adapters (map BLE cmds -> Core API) ---------
  bool   esnSetChannel(uint8_t ch, bool persist);
  bool   esnPair(const String& macHex, const String& type);
  bool   esnRemovePeer(const String& macHex);
  String esnSerializePeers();
  bool   esnConfigureTopology(JsonVariantConst  topoObj);
  String esnSerializeTopology();
  String esnExportConfiguration();
};

// ---------------------- BLE callbacks ----------------------
struct BleICMSecurityCB : public BLESecurityCallbacks {
  uint32_t onPassKeyRequest() override;
  void     onPassKeyNotify(uint32_t) override;
  bool     onConfirmPIN(uint32_t) override;
  bool     onSecurityRequest() override;
  void     onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) override;
};

struct BleICMServerCB : public BLEServerCallbacks {
  void onConnect(BLEServer* server, esp_ble_gatts_cb_param_t* param) override;
  void onDisconnect(BLEServer* server) override;
};

struct BleICMChCB : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) override;
  void onRead (BLECharacteristic* c) override;
};

// ---------------------- helpers (notify/ack/json) ----------------------
void icm_notifyText(BLECharacteristic* ch, const char* msg);
void icm_notifyJson(BLECharacteristic* ch, const String& json);
void icm_sendAck   (BLECharacteristic* ch, bool ok, const char* why=nullptr);
bool icm_parseJson (const std::string& v, DynamicJsonDocument& d);
