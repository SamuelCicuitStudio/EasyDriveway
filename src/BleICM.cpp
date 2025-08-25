// =============================
// File: BleICM.cpp  (rewrite – notify/setValue fixes)
// =============================
#include "BleICM.h"
#include "WiFiManager.h"

// Required for notify CCC
#include <BLE2902.h>

// ESP-IDF 5+: mac helpers no longer re-exported by esp_system.h
#include "esp_mac.h"

// -------------- globals --------------
BLEServer*         ICM_pServer    = nullptr;
BLEService*        ICM_Service    = nullptr;
BLECharacteristic* ICM_chStatus   = nullptr;
BLECharacteristic* ICM_chWifi     = nullptr;
BLECharacteristic* ICM_chPeers    = nullptr;
BLECharacteristic* ICM_chTopo     = nullptr;
BLECharacteristic* ICM_chSeq      = nullptr;
BLECharacteristic* ICM_chPower    = nullptr;
BLECharacteristic* ICM_chExport   = nullptr;
BLECharacteristic* ICM_chOldApp   = nullptr;

BLEDescriptor*     ICM_descStatus = nullptr;
BLEDescriptor*     ICM_descWifi   = nullptr;
BLEDescriptor*     ICM_descPeers  = nullptr;
BLEDescriptor*     ICM_descTopo   = nullptr;
BLEDescriptor*     ICM_descSeq    = nullptr;
BLEDescriptor*     ICM_descPower  = nullptr;
BLEDescriptor*     ICM_descExport = nullptr;
BLEDescriptor*     ICM_descOldApp = nullptr;

static BLE2902*    ICM_cccStatus  = nullptr;
static BLE2902*    ICM_cccWifi    = nullptr;
static BLE2902*    ICM_cccPeers   = nullptr;
static BLE2902*    ICM_cccTopo    = nullptr;
static BLE2902*    ICM_cccSeq     = nullptr;
static BLE2902*    ICM_cccPower   = nullptr;
static BLE2902*    ICM_cccExport  = nullptr;
static BLE2902*    ICM_cccOldApp  = nullptr;

bool               ICM_isPaired   = false;
uint64_t           ICM_AdvStartMs = 0;
char               ICM_remoteAddr[18] = {0};

TaskHandle_t BleICM::advLedTaskHandle = nullptr;
BleICM*      BleICM::instance = nullptr;

// ---- helper: chunked JSON notify using std::string overload ----
static inline void icm_notifyJson(BLECharacteristic* ch, const String& s) {
  const char* data = s.c_str();
  size_t total = s.length();

#ifndef ICM_BLE_MAX_CHUNK
  const size_t kChunk = 20;     // safe default (MTU 23 -> 20 bytes payload)
#else
  const size_t kChunk = ICM_BLE_MAX_CHUNK;
#endif

  if (total == 0) {
    ch->setValue(std::string());  // empty
    ch->notify();
    return;
  }

  size_t sent = 0;
  while (sent < total) {
    size_t n = (total - sent > kChunk) ? kChunk : (total - sent);
    ch->setValue(std::string(data + sent, data + sent + n));  // <— std::string avoids constness issues
    ch->notify();
    sent += n;
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

// -------------- ctor --------------
BleICM::BleICM(ConfigManager* cfg, ESPNowManager* esn, WiFiManager* wifiMgr, ICMLogFS* log)
: Cfg(cfg), Esn(esn), WMgr(wifiMgr), Log(log) {
  instance = this;
}

// -------------- begin --------------
void BleICM::begin() {
  // status LED
   _pinB = Cfg->GetInt(LED_B_PIN_KEY, LED_B_PIN_DEFAULT);
  pinMode(_pinB, OUTPUT);
  digitalWrite(_pinB, LOW);

  // init BLE device with friendly name
  String devName = Cfg->GetString(DEVICE_BLE_NAME_KEY, DEVICE_BLE_NAME_DEFAULT) +
                   String("_") + Cfg->GetString(DEVICE_ID_KEY, DEVICE_ID_DEFAULT);
  BLEDevice::init(devName.c_str());

  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
  BLEDevice::setSecurityCallbacks(new BleICMSecurityCB());

  ICM_pServer = BLEDevice::createServer();
  ICM_pServer->setCallbacks(new BleICMServerCB());

  // create ICM service
  ICM_Service = ICM_pServer->createService(BLEUUID(ICM_BLE_SERVICE_UUID), 30, 0);

  // --------- CHARACTERISTICS + descriptors + CCC (notify) ---------
  ICM_chStatus = ICM_Service->createCharacteristic(
      ICM_CH_STATUS_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  ICM_descStatus = new BLEDescriptor((uint16_t)0x2901);
  ICM_descStatus->setValue("ICM Status"); ICM_chStatus->addDescriptor(ICM_descStatus);
  ICM_cccStatus = new BLE2902(); ICM_cccStatus->setNotifications(true); ICM_chStatus->addDescriptor(ICM_cccStatus);

  ICM_chWifi = ICM_Service->createCharacteristic(
      ICM_CH_WIFI_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  ICM_descWifi = new BLEDescriptor((uint16_t)0x2901);
  ICM_descWifi->setValue("WiFi Control"); ICM_chWifi->addDescriptor(ICM_descWifi);
  ICM_cccWifi = new BLE2902(); ICM_cccWifi->setNotifications(true); ICM_chWifi->addDescriptor(ICM_cccWifi);

  ICM_chPeers = ICM_Service->createCharacteristic(
      ICM_CH_PEERS_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  ICM_descPeers = new BLEDescriptor((uint16_t)0x2901);
  ICM_descPeers->setValue("Peers"); ICM_chPeers->addDescriptor(ICM_descPeers);
  ICM_cccPeers = new BLE2902(); ICM_cccPeers->setNotifications(true); ICM_chPeers->addDescriptor(ICM_cccPeers);

  ICM_chTopo = ICM_Service->createCharacteristic(
      ICM_CH_TOPO_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  ICM_descTopo = new BLEDescriptor((uint16_t)0x2901);
  ICM_descTopo->setValue("Topology"); ICM_chTopo->addDescriptor(ICM_descTopo);
  ICM_cccTopo = new BLE2902(); ICM_cccTopo->setNotifications(true); ICM_chTopo->addDescriptor(ICM_cccTopo);

  ICM_chSeq = ICM_Service->createCharacteristic(
      ICM_CH_SEQ_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  ICM_descSeq = new BLEDescriptor((uint16_t)0x2901);
  ICM_descSeq->setValue("Sequence"); ICM_chSeq->addDescriptor(ICM_descSeq);
  ICM_cccSeq = new BLE2902(); ICM_cccSeq->setNotifications(true); ICM_chSeq->addDescriptor(ICM_cccSeq);

  ICM_chPower = ICM_Service->createCharacteristic(
      ICM_CH_POWER_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  ICM_descPower = new BLEDescriptor((uint16_t)0x2901);
  ICM_descPower->setValue("Power"); ICM_chPower->addDescriptor(ICM_descPower);
  ICM_cccPower = new BLE2902(); ICM_cccPower->setNotifications(true); ICM_chPower->addDescriptor(ICM_cccPower);

  ICM_chExport = ICM_Service->createCharacteristic(
      ICM_CH_EXPORT_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  ICM_descExport = new BLEDescriptor((uint16_t)0x2901);
  ICM_descExport->setValue("Export/Import"); ICM_chExport->addDescriptor(ICM_descExport);
  ICM_cccExport = new BLE2902(); ICM_cccExport->setNotifications(true); ICM_chExport->addDescriptor(ICM_cccExport);

  ICM_chOldApp = ICM_Service->createCharacteristic(
      ICM_CH_OLDAPP_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  ICM_descOldApp = new BLEDescriptor((uint16_t)0x2901);
  ICM_descOldApp->setValue("Compat"); ICM_chOldApp->addDescriptor(ICM_descOldApp);
  ICM_cccOldApp = new BLE2902(); ICM_cccOldApp->setNotifications(true); ICM_chOldApp->addDescriptor(ICM_cccOldApp);

  // callbacks
  auto cbs = new BleICMCharCB();
  ICM_chStatus->setCallbacks(cbs);
  ICM_chWifi  ->setCallbacks(cbs);
  ICM_chPeers ->setCallbacks(cbs);
  ICM_chTopo  ->setCallbacks(cbs);
  ICM_chSeq   ->setCallbacks(cbs);
  ICM_chPower ->setCallbacks(cbs);
  ICM_chExport->setCallbacks(cbs);
  ICM_chOldApp->setCallbacks(cbs);

  // start service & advertise
  ICM_Service->start();
  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(ICM_BLE_SERVICE_UUID);
  adv->setScanResponse(false);
  adv->setMinPreferred(0x0);
  BLEDevice::startAdvertising();
  ICM_AdvStartMs = millis();

  bleSecurity();
  startAdvLedTask();
}

void BleICM::bleSecurity() {
  esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_MITM; // secure connections + MITM
  esp_ble_io_cap_t iocap = ESP_IO_CAP_OUT;               // show passkey
  uint8_t key_size = 16;
  uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
  uint8_t rsp_key  = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
  uint32_t passkey = Cfg->GetInt(DEVICE_BLE_AUTH_PASS_KEY, DEVICE_BLE_AUTH_PASS_DEFAULT);
  uint8_t auth_only = ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_DISABLE;

  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &passkey, sizeof(uint32_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_ONLY_ACCEPT_SPECIFIED_SEC_AUTH, &auth_only, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY,  &rsp_key,  sizeof(uint8_t));
}

void BleICM::notifyStatus() {
  DynamicJsonDocument d(512);
  d["mode"] = "OFF";
  if (WMgr) {
    if (WiFi.getMode() & WIFI_AP) {
      d["mode"] = "AP";
      d["ip"]   = WiFi.softAPIP().toString();
      d["ch"]   = Cfg->GetInt(ESPNOW_CH_KEY, ESPNOW_CH_DEFAULT);
    } else if (WiFi.status() == WL_CONNECTED) {
      d["mode"] = "STA";
      d["ip"]   = WiFi.localIP().toString();
      d["ch"]   = WiFi.channel();
      d["rssi"] = WiFi.RSSI();
    }
  }
  String s; serializeJson(d, s);
  icm_notifyJson(ICM_chStatus, s);
}

void BleICM::restartAdvertising() {
  ICM_Service->start();
  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(ICM_BLE_SERVICE_UUID);
  adv->setScanResponse(false);
  adv->setMinPreferred(0x0);
  BLEDevice::startAdvertising();
  ICM_AdvStartMs = millis();
}

void BleICM::onBLEConnected() {
  stopAdvLedTask();
  digitalWrite(_pinB, HIGH);
}

void BleICM::onBLEDisconnected() {
  startAdvLedTask();
}

void BleICM::advLedTask(void* pv) {
  // Use public member pin (e.g., `ledPin`), fallback to compile-time default
  const int pin = (BleICM::instance && BleICM::instance->_pinB > -1)
                    ? BleICM::instance->_pinB
                    : (int)LED_B_PIN_DEFAULT;

  pinMode(pin, OUTPUT);

  for (;;) {
    if ((ICM_pServer && ICM_pServer->getConnectedCount() > 0) || ICM_isPaired) break;

    digitalWrite(pin, HIGH);
    vTaskDelay(pdMS_TO_TICKS(500));
    digitalWrite(pin, LOW);
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  digitalWrite(pin, LOW);
  BleICM::advLedTaskHandle = nullptr;
  vTaskDelete(nullptr);
}


void BleICM::startAdvLedTask() {
  if (!advLedTaskHandle) {
    xTaskCreate(advLedTask, "ICMBLELed", 1024, nullptr, tskIDLE_PRIORITY+1, &advLedTaskHandle);
  }
}

void BleICM::stopAdvLedTask() {
  if (advLedTaskHandle) { vTaskDelete(advLedTaskHandle); advLedTaskHandle = nullptr; }
}

// ------------------- Security CB -------------------
uint32_t BleICMSecurityCB::onPassKeyRequest() {
  if (!BleICM::instance) return DEVICE_BLE_AUTH_PASS_DEFAULT;
  return BleICM::instance->Cfg->GetInt(DEVICE_BLE_AUTH_PASS_KEY, DEVICE_BLE_AUTH_PASS_DEFAULT);
}
void BleICMSecurityCB::onPassKeyNotify(uint32_t pass_key) {
  (void)pass_key;
}
bool BleICMSecurityCB::onConfirmPIN(uint32_t pass_key) {
  (void)pass_key; vTaskDelay(pdMS_TO_TICKS(500)); return true;
}
bool BleICMSecurityCB::onSecurityRequest() { return true; }
void BleICMSecurityCB::onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) {
  if (!cmpl.success && ICM_pServer) {
    ICM_pServer->removePeerDevice(ICM_pServer->getConnId(), true);
  }
}

// ------------------- Server CB -------------------
void BleICMServerCB::onConnect(BLEServer* server, esp_ble_gatts_cb_param_t* param) {
  ICM_isPaired = true;
  if (BleICM::instance) BleICM::instance->onBLEConnected();
  if (param) {
    snprintf(ICM_remoteAddr, sizeof(ICM_remoteAddr), "%02X:%02X:%02X:%02X:%02X:%02X",
      param->connect.remote_bda[0], param->connect.remote_bda[1], param->connect.remote_bda[2],
      param->connect.remote_bda[3], param->connect.remote_bda[4], param->connect.remote_bda[5]);
  }
}
void BleICMServerCB::onDisconnect(BLEServer* server) {
  if (server) server->removePeerDevice(server->getConnId(), true);
  ICM_AdvStartMs = millis();
  ICM_isPaired = false;
  if (BleICM::instance) BleICM::instance->onBLEDisconnected();
  BLEDevice::startAdvertising();
}

// ------------------- Char CB helpers -------------------
static void icm_sendAck(BLECharacteristic* ch, bool ok=true, const char* msg=nullptr) {
  DynamicJsonDocument d(192);
  d["ok"] = ok; if (msg) d["msg"] = msg;
  String s; serializeJson(d, s);
  icm_notifyJson(ch, s);
}
static bool icm_parseJson(const std::string& v, DynamicJsonDocument& d) {
  DeserializationError e = deserializeJson(d, v);
  return !e;
}

void BleICMCharCB::onWrite(BLECharacteristic* ch) {
  if (!BleICM::instance) return;
  auto& icm = *BleICM::instance;
  std::string v = ch->getValue();

  // STATUS: immediate snapshot request if any value written
  if (ch == ICM_chStatus) { icm.notifyStatus(); return; }

  // WIFI control
  if (ch == ICM_chWifi) {
    DynamicJsonDocument d(1024);
    if (!icm_parseJson(v, d)) { icm_sendAck(ch, false, "bad json"); return; }
    const char* cmd = d["cmd"] | "";
    if (!strcmp(cmd, "sta_connect")) {
      String ssid = d["ssid"] | ""; String psk = d["password"] | "";
      icm.Cfg->PutString(WIFI_STA_SSID_KEY, ssid);
      icm.Cfg->PutString(WIFI_STA_PASS_KEY, psk);
      WiFi.mode(WIFI_STA); WiFi.persistent(false); WiFi.disconnect(true); delay(50);
      if (ssid.length()) WiFi.begin(ssid.c_str(), psk.c_str()); else WiFi.begin();
      uint32_t t0=millis(); while (WiFi.status()!=WL_CONNECTED && millis()-t0<15000) vTaskDelay(pdMS_TO_TICKS(100));
      icm.notifyStatus(); icm_sendAck(ch, WiFi.status()==WL_CONNECTED);
      if (WiFi.status()==WL_CONNECTED && icm.Esn) icm.Esn->setChannel(WiFi.channel());
      if (WiFi.status()==WL_CONNECTED) icm.Cfg->PutInt(ESPNOW_CH_KEY, WiFi.channel());
      return;
    }
    if (!strcmp(cmd, "ap_start")) {
      String apSsid = d["ap_ssid"] | icm.Cfg->GetString(DEVICE_WIFI_HOTSPOT_NAME_KEY, DEVICE_WIFI_HOTSPOT_NAME_DEFAULT);
      String apPass = d["ap_password"] | icm.Cfg->GetString(DEVICE_AP_AUTH_PASS_KEY, DEVICE_AP_AUTH_PASS_DEFAULT);
      int chn = d["ch"] | icm.Cfg->GetInt(ESPNOW_CH_KEY, ESPNOW_CH_DEFAULT);
      icm.Cfg->PutString(DEVICE_WIFI_HOTSPOT_NAME_KEY, apSsid);
      icm.Cfg->PutString(DEVICE_AP_AUTH_PASS_KEY,      apPass);
      icm.Cfg->PutInt(ESPNOW_CH_KEY, chn);
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_AP);
      WiFi.softAPConfig(LOCAL_IP, GATEWAY, SUBNET);
      WiFi.softAP(apSsid.c_str(), apPass.c_str(), (chn<1||chn>13)?ESPNOW_CH_DEFAULT:chn);
      if (icm.Esn) icm.Esn->setChannel((uint8_t)((chn<1||chn>13)?ESPNOW_CH_DEFAULT:chn));
      icm.notifyStatus(); icm_sendAck(ch, true);
      return;
    }
    if (!strcmp(cmd, "scan")) {
      int n = WiFi.scanNetworks();
      DynamicJsonDocument out(2048); JsonArray a = out.createNestedArray("aps");
      for (int i=0;i<n;i++){
        JsonObject o=a.createNestedObject();
        o["ssid"]=WiFi.SSID(i); o["rssi"]=WiFi.RSSI(i); o["ch"]=WiFi.channel(i); o["enc"]=WiFi.encryptionType(i);
      }
      WiFi.scanDelete();
      String s; serializeJson(out,s);
      icm_notifyJson(ch, s);
      return;
    }
    icm_sendAck(ch, false, "unknown wifi cmd"); return;
  }

  // PEERS control
  if (ch == ICM_chPeers) {
    DynamicJsonDocument d(1024);
    if (!icm_parseJson(v, d)) { icm_sendAck(ch, false, "bad json"); return; }
    const char* cmd = d["cmd"] | "";
    if (!strcmp(cmd, "pair")) {
      String mac = d["mac"] | ""; String type = d["type"] | "";
      bool ok = icm.Esn ? icm.Esn->pair(mac, type) : false;
      icm_sendAck(ch, ok); return;
    }
    if (!strcmp(cmd, "remove")) {
      String mac = d["mac"] | ""; bool ok = icm.Esn ? icm.Esn->removePeer(mac) : false;
      icm_sendAck(ch, ok); return;
    }
    if (!strcmp(cmd, "list")) {
      String peers = icm.Esn ? icm.Esn->serializePeers() : String("{}");
      icm_notifyJson(ch, peers);
      return;
    }
    icm_sendAck(ch, false, "unknown peers cmd"); return;
  }

  // TOPOLOGY
  if (ch == ICM_chTopo) {
    DynamicJsonDocument d(8192);
    if (!icm_parseJson(v, d)) { icm_sendAck(ch, false, "bad json"); return; }
    const char* cmd = d["cmd"] | "";
    if (!strcmp(cmd, "set")) {
      bool ok = icm.Esn ? icm.Esn->configureTopology(d["links"]) : false;
      icm_sendAck(ch, ok); return;
    }
    if (!strcmp(cmd, "get")) {
      String topo = icm.Esn ? icm.Esn->serializeTopology() : String("{}");
      icm_notifyJson(ch, topo);
      return;
    }
    icm_sendAck(ch, false, "unknown topo cmd"); return;
  }

  // SEQUENCE (stubs; hook to your sequence engine)
  if (ch == ICM_chSeq) {
    DynamicJsonDocument d(512);
    if (!icm_parseJson(v, d)) { icm_sendAck(ch, false, "bad json"); return; }
    const char* cmd = d["cmd"] | "";
    if (!strcmp(cmd, "start") || !strcmp(cmd, "stop")) { icm_sendAck(ch, true); return; }
    icm_sendAck(ch, false, "unknown seq cmd"); return;
  }

  // POWER (stubs)
  if (ch == ICM_chPower) {
    DynamicJsonDocument out(256); out["status"] = "ok";
    String s; serializeJson(out, s);
    icm_notifyJson(ch, s);
    return;
  }

  // EXPORT/IMPORT
  if (ch == ICM_chExport) {
    DynamicJsonDocument d(8192);
    if (!icm_parseJson(v, d)) { icm_sendAck(ch, false, "bad json"); return; }
    const char* cmd = d["cmd"] | "";
    if (!strcmp(cmd, "export")) {
      String blob = icm.Esn ? icm.Esn->exportConfiguration() : String("{}");
      icm_notifyJson(ch, blob);
      return;
    }
    if (!strcmp(cmd, "import")) {
      bool ok = true;
      if (d.containsKey("config")) {
        JsonVariantConst cfg = d["config"];
        if (cfg.containsKey("esn_ch") && icm.Esn) {
          int chn = cfg["esn_ch"].as<int>();
          if (chn >= 1 && chn <= 13) ok &= icm.Esn->setChannel((uint8_t)chn, /*persist=*/true);
        }
        if (cfg.containsKey("links") && icm.Esn) {
          ok &= icm.Esn->configureTopology(cfg["links"]);
        }
      } else {
        ok = false;
      }
      icm_sendAck(ch, ok);
      return;
    }
    icm_sendAck(ch, false, "unknown export cmd"); return;
  }

  // Compatibility: echo or simple token router
  if (ch == ICM_chOldApp) {
    String s = String(v.c_str());
    if (s.equalsIgnoreCase("STATUS")) { icm.notifyStatus(); return; }
    icm_notifyJson(ch, String("{\"ok\":false,\"msg\":\"UNKNOWN\"}"));
    return;
  }
}

void BleICMCharCB::onRead(BLECharacteristic* ch) {
  if (ch == ICM_chStatus && BleICM::instance) BleICM::instance->notifyStatus();
}
