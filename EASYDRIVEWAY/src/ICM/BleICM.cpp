// =============================
// File: BleICM.cpp  (drop-in fix, Core-correct)
// =============================
#include "BleICM.h"


using NwCore::Core;

// Fallback AP net if your project doesn’t define these
#ifndef LOCAL_IP
#define LOCAL_IP IPAddress(192,168,4,1)
#define GATEWAY  IPAddress(192,168,4,1)
#define SUBNET   IPAddress(255,255,255,0)
#endif

// ------------------------ globals ------------------------
BLEServer*         ICM_pServer     = nullptr;
BLEService*        ICM_Service     = nullptr;

BLECharacteristic* ICM_chStatus    = nullptr;
BLECharacteristic* ICM_chWifi      = nullptr;
BLECharacteristic* ICM_chPeers     = nullptr;
BLECharacteristic* ICM_chTopo      = nullptr;
BLECharacteristic* ICM_chSeq       = nullptr;
BLECharacteristic* ICM_chPower     = nullptr;
BLECharacteristic* ICM_chExport    = nullptr;
BLECharacteristic* ICM_chOldApp    = nullptr;

BLEDescriptor*     ICM_descStatus  = nullptr;
BLEDescriptor*     ICM_descWifi    = nullptr;
BLEDescriptor*     ICM_descPeers   = nullptr;
BLEDescriptor*     ICM_descTopo    = nullptr;
BLEDescriptor*     ICM_descSeq     = nullptr;
BLEDescriptor*     ICM_descPower   = nullptr;
BLEDescriptor*     ICM_descExport  = nullptr;
BLEDescriptor*     ICM_descOldApp  = nullptr;

static BLE2902*    ICM_cccStatus   = nullptr;
static BLE2902*    ICM_cccWifi     = nullptr;
static BLE2902*    ICM_cccPeers    = nullptr;
static BLE2902*    ICM_cccTopo     = nullptr;
static BLE2902*    ICM_cccSeq      = nullptr;
static BLE2902*    ICM_cccPower    = nullptr;
static BLE2902*    ICM_cccExport   = nullptr;
static BLE2902*    ICM_cccOldApp   = nullptr;

bool               ICM_isPaired    = false;
uint64_t           ICM_AdvStartMs  = 0;
char               ICM_remoteAddr[18] = {0};

BleICM*            BleICM::instance = nullptr;
TaskHandle_t       BleICM::advLedTaskHandle = nullptr;

// ------------------------ small utils ------------------------
static inline bool macFromHex(const String& s, uint8_t out[6]) {
  return Core::macStrToBytes(s.c_str(), out);
}
static inline String macToHex(const uint8_t b[6]) {
  return Core::macBytesToStr(b);
}

// Chunked notify (handles default 20-byte GATT payloads safely)
void icm_notifyJson(BLECharacteristic* ch, const String& s) {
  if (!ch) return;

#ifndef ICM_BLE_MAX_CHUNK
  const size_t kChunk = 20;   // MTU 23 -> 20 bytes payload
#else
  const size_t kChunk = ICM_BLE_MAX_CHUNK;
#endif

  const char* data = s.c_str();
  size_t total = s.length();

  if (total == 0) { ch->setValue(std::string()); ch->notify(); return; }

  size_t sent = 0;
  while (sent < total) {
    size_t n = (total - sent > kChunk) ? kChunk : (total - sent);
    ch->setValue(std::string(data + sent, data + sent + n));
    ch->notify();
    sent += n;
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

void icm_notifyText(BLECharacteristic* ch, const char* msg) {
  if (!ch || !msg) return;
  icm_notifyJson(ch, String(msg));
}

void icm_sendAck(BLECharacteristic* ch, bool ok, const char* why) {
  DynamicJsonDocument d(192);
  d["ok"] = ok; if (why) d["why"] = why;
  String out; serializeJson(d, out);
  icm_notifyJson(ch, out);
}

bool icm_parseJson(const std::string& v, DynamicJsonDocument& d) {
  return !deserializeJson(d, v);
}

// ------------------------ BleICM ------------------------
BleICM::BleICM(ConfigManager* cfg, Core* esn, WiFiManager* wifiMgr, ICMLogFS* log)
: Cfg(cfg), Esn(esn), WMgr(wifiMgr), Log(log) {
  instance = this;
}

void BleICM::begin() {
  // LED pin from NVS (fallback to default)
  _pinB = Cfg ? Cfg->GetInt(NVS_PIN_LED_B, PIN_LED_B_DEFAULT) : PIN_LED_B_DEFAULT;
  pinMode(_pinB, OUTPUT);
  digitalWrite(_pinB, LOW);

  // BLE name = <cfg name>_<device id>
  String devName = (Cfg ? Cfg->GetString(NVS_KEY_BLE_NAME, NVS_DEF_BLE_NAME) : String("ICM")) + String("_")
                 + (Cfg ? Cfg->GetString(NVS_KEY_SYS_DEVID, NVS_DEF_SYS_DEVID) : String("0000"));
  BLEDevice::init(devName.c_str());
  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
  BLEDevice::setSecurityCallbacks(new BleICMSecurityCB());

  ICM_pServer = BLEDevice::createServer();
  ICM_pServer->setCallbacks(new BleICMServerCB());

  // Service
  ICM_Service = ICM_pServer->createService(BLEUUID(ICM_BLE_SERVICE_UUID), 30, 0);

  // -------- characteristics + descriptors + CCC --------
  auto mk = [&](const char* uuid, const char* name, uint32_t props,
                BLECharacteristic*& ch, BLEDescriptor*& dsc, BLE2902*& ccc) {
    ch = ICM_Service->createCharacteristic(uuid, props);
    dsc = new BLEDescriptor((uint16_t)0x2901);
    dsc->setValue(name);
    ch->addDescriptor(dsc);
    ccc = new BLE2902();
    ccc->setNotifications(true);
    ch->addDescriptor(ccc);
  };

  mk(ICM_CH_STATUS_UUID, "ICM Status",
     BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY,
     ICM_chStatus, ICM_descStatus, ICM_cccStatus);

  mk(ICM_CH_WIFI_UUID, "WiFi Control",
     BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY,
     ICM_chWifi, ICM_descWifi, ICM_cccWifi);

  mk(ICM_CH_PEERS_UUID, "Peers",
     BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY,
     ICM_chPeers, ICM_descPeers, ICM_cccPeers);

  mk(ICM_CH_TOPO_UUID, "Topology",
     BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY,
     ICM_chTopo, ICM_descTopo, ICM_cccTopo);

  mk(ICM_CH_SEQ_UUID, "Sequence",
     BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY,
     ICM_chSeq, ICM_descSeq, ICM_cccSeq);

  mk(ICM_CH_POWER_UUID, "Power",
     BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY,
     ICM_chPower, ICM_descPower, ICM_cccPower);

  mk(ICM_CH_EXPORT_UUID, "Export/Import",
     BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY,
     ICM_chExport, ICM_descExport, ICM_cccExport);

  mk(ICM_CH_OLDAPP_UUID, "Compat",
     BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY,
     ICM_chOldApp, ICM_descOldApp, ICM_cccOldApp);

  // callbacks
  auto* cbs = new BleICMChCB();
  ICM_chStatus ->setCallbacks(cbs);
  ICM_chWifi   ->setCallbacks(cbs);
  ICM_chPeers  ->setCallbacks(cbs);
  ICM_chTopo   ->setCallbacks(cbs);
  ICM_chSeq    ->setCallbacks(cbs);
  ICM_chPower  ->setCallbacks(cbs);
  ICM_chExport ->setCallbacks(cbs);
  ICM_chOldApp ->setCallbacks(cbs);

  // Start + advertise
  ICM_Service->start();
  auto* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(ICM_BLE_SERVICE_UUID);
  adv->setScanResponse(false);
  adv->setMinPreferred(0x0);
  BLEDevice::startAdvertising();
  ICM_AdvStartMs = millis();

  bleSecurity();
  startAdvLedTask();

  // Ensure Core recv callback for ICM builds (safe no-op otherwise)
#ifdef NVS_ROLE_ICM
  if (Esn) Esn->setRecvCallback(&Core::icmRecvCallback);
#endif
}

void BleICM::bleSecurity() {
  esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_MITM; // secure connections + MITM
  esp_ble_io_cap_t   iocap    = ESP_IO_CAP_OUT;          // show passkey
  uint8_t key_size = 16;
  uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
  uint8_t rsp_key  = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
  uint32_t passkey = Cfg ? Cfg->GetInt(NVS_KEY_BLE_PASSK, NVS_DEF_BLE_PASSK) : NVS_DEF_BLE_PASSK;
  uint8_t auth_only = ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_DISABLE;

  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY,          &passkey,  sizeof(uint32_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE,             &auth_req, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE,                  &iocap,    sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE,                &key_size, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_ONLY_ACCEPT_SPECIFIED_SEC_AUTH, &auth_only, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY,                &init_key, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY,                 &rsp_key,  sizeof(uint8_t));
}

void BleICM::restartAdvertising() {
  ICM_Service->start();
  auto* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(ICM_BLE_SERVICE_UUID);
  adv->setScanResponse(false);
  adv->setMinPreferred(0x0);
  BLEDevice::startAdvertising();
  ICM_AdvStartMs = millis();
}

void BleICM::notifyStatus() {
  if (!ICM_chStatus) return;
  DynamicJsonDocument d(512);
  d["paired"] = ICM_isPaired;
  d["uptime"] = (uint32_t)(millis()/1000);
  if (WMgr) {
    if (WiFi.getMode() & WIFI_AP) {
      d["mode"] = "AP";
      d["ip"]   = WiFi.softAPIP().toString();
      d["ch"]   = Cfg ? Cfg->GetInt(NVS_KEY_NET_CHAN, NVS_DEF_NET_CHAN) : 1;
    } else if (WiFi.status() == WL_CONNECTED) {
      d["mode"] = "STA";
      d["ip"]   = WiFi.localIP().toString();
      d["ch"]   = WiFi.channel();
      d["rssi"] = WiFi.RSSI();
    } else {
      d["mode"] = "OFF";
    }
  }
  String s; serializeJson(d, s);
  icm_notifyJson(ICM_chStatus, s);
}

void BleICM::onBLEConnected()    { stopAdvLedTask(); if (_pinB >= 0) digitalWrite(_pinB, HIGH); }
void BleICM::onBLEDisconnected() { startAdvLedTask(); }

void BleICM::advLedTask(void* pv) {
  const int pin = (BleICM::instance && BleICM::instance->_pinB > -1)
                    ? BleICM::instance->_pinB
                    : (int)PIN_LED_B_DEFAULT;
  pinMode(pin, OUTPUT);
  for (;;) {
    if ((ICM_pServer && ICM_pServer->getConnectedCount() > 0) || ICM_isPaired) break;
    digitalWrite(pin, HIGH); vTaskDelay(pdMS_TO_TICKS(500));
    digitalWrite(pin, LOW ); vTaskDelay(pdMS_TO_TICKS(500));
  }
  digitalWrite(pin, LOW);
  BleICM::advLedTaskHandle = nullptr;
  vTaskDelete(nullptr);
}

void BleICM::startAdvLedTask() {
  if (!advLedTaskHandle) xTaskCreate(advLedTask, "ICMBLELed", 1024, nullptr, tskIDLE_PRIORITY+1, &advLedTaskHandle);
}
void BleICM::stopAdvLedTask() {
  if (advLedTaskHandle) { vTaskDelete(advLedTaskHandle); advLedTaskHandle = nullptr; }
}
// ------------------------ ESPNOW adapters ------------------------
bool BleICM::addPeersFromRegistry(uint8_t ch) {
#ifndef NVS_ROLE_ICM
  (void)ch;
  return false;
#else
  if (!Esn || !Cfg) return false;

  // (Re)load slot cache from registry; this uses Iskey() internally. :contentReference[oaicite:3]{index=3}
  Esn->slotsLoadFromRegistry();

  // --- Sensors S01..S16 ---
  for (uint8_t i = 1; i <= NVS_MAX_SENS; ++i) {                      // NVS_MAX_SENS defined in NVSConfig.h :contentReference[oaicite:4]{index=4}
    char kMac[7]; snprintf(kMac, sizeof(kMac), NVS_REG_S_MAC_FMT, i); // "S%02uMAC" key format :contentReference[oaicite:5]{index=5}
    if (!Cfg->Iskey(kMac)) continue;                                  // key absent => slot empty (removed)
    String macHex = Cfg->GetString(kMac, "");
    uint8_t mac[6];
    if (Core::macStrToBytes(macHex.c_str(), mac)) {                   // parse "AABBCCDDEEFF" / "AA:BB:..." :contentReference[oaicite:6]{index=6}
      Esn->addPeer(mac, ch, false, nullptr);                          // addPeer() safely replaces if exists :contentReference[oaicite:7]{index=7}
    }
  }

  // --- Relays R01..R16 ---
  for (uint8_t i = 1; i <= NVS_MAX_RELAY; ++i) {                      // NVS_MAX_RELAY in NVSConfig.h :contentReference[oaicite:8]{index=8}
    char kMac[7]; snprintf(kMac, sizeof(kMac), NVS_REG_R_MAC_FMT, i); // "R%02uMAC" key format :contentReference[oaicite:9]{index=9}
    if (!Cfg->Iskey(kMac)) continue;
    String macHex = Cfg->GetString(kMac, "");
    uint8_t mac[6];
    if (Core::macStrToBytes(macHex.c_str(), mac)) {
      Esn->addPeer(mac, ch, false, nullptr);                          // HW table add/replace :contentReference[oaicite:10]{index=10}
    }
  }

  // --- PMS P01 (single slot) ---
  if (Cfg->Iskey(NVS_REG_P_MAC_CONST)) {                              // "P01MAC" constant key :contentReference[oaicite:11]{index=11}
    String macHex = Cfg->GetString(NVS_REG_P_MAC_CONST, "");
    uint8_t mac[6];
    if (Core::macStrToBytes(macHex.c_str(), mac)) {
      Esn->addPeer(mac, ch, false, nullptr);
    }
  }

  return true;
#endif
}

bool BleICM::esnSetChannel(uint8_t ch, bool persist) {
  if (!Esn) return false;
  if (ch < 1 || ch > 13) ch = 1;

  Esn->end();
  bool ok = Esn->begin(ch, nullptr);                 // switch ESPNOW channel
  if (!ok) return false;                             // begin() is Core's API. :contentReference[oaicite:2]{index=2}
  if (persist && Cfg) Cfg->PutInt(NVS_KEY_NET_CHAN, ch);

#ifdef NVS_ROLE_ICM
  // Rebuild HW peer table strictly from NVS registry KEYS that exist.
  addPeersFromRegistry(ch);
#endif
  return true;
}

bool BleICM::esnPair(const String& macHex, const String& type) {
  if (!Esn) return false;

  uint8_t mac[6];
  if (!macFromHex(macHex, mac)) return false;

#ifdef NVS_ROLE_ICM
  // Map BLE "type" to protocol kind; if known, call auto-pair directly.
  uint8_t kind = 0xFF;
  if (type.length()) {
    if (type.equalsIgnoreCase("sensor") || type.equalsIgnoreCase("sens"))      kind = NOW_KIND_SENS;
    else if (type.equalsIgnoreCase("relay")  || type.equalsIgnoreCase("rly"))  kind = NOW_KIND_RELAY;
    else if (type.equalsIgnoreCase("pms")    || type.equalsIgnoreCase("power"))kind = NOW_KIND_PMS;
  }

  if (kind != 0xFF) {
    // Only 'kind' is actually used by auto_pair_from_devinfo(); the other
    // strings are optional here.
    DevInfoPayload info{};
    info.kind = kind;
    // (devid/hwrev/swver/build left zero-initialized)
    return Core::auto_pair_from_devinfo(Esn, mac, &info);  // ← direct auto-pair
  }
#endif

  // Unknown kind: fall back to DEVINFO query; dispatcher will auto-pair on reply.
  Esn->addPeer(mac, Esn->channel(), /*encrypt*/false, /*lmk*/nullptr);
  return Esn->prDevInfoQuery(mac, /*waitAckMs*/100) == ESP_OK;
}


bool BleICM::esnRemovePeer(const String& macHex) {
  if (!Esn) return false;

  uint8_t mac[6];
  if (!macFromHex(macHex, mac)) return false;

  // 1) If this MAC is known in the ICM registry, try to notify the node to unpair itself.
  //    prUnpair sends NOW_DOM_PR / PR_UNPAIR and waits for HW ACK.
  bool ok_unpair = false;
#ifdef NVS_ROLE_ICM
  if (Esn->macInRegistry(mac)) {
    ok_unpair = (Esn->prUnpair(mac, /*waitAckMs*/100) == ESP_OK);
  }
#endif

  // 2) Remove from local ESPNOW HW peer table (best-effort)
  Esn->delPeer(mac);

  // 3) Clear local ICM registry entries (existence-based keys)
#ifdef NVS_ROLE_ICM
  bool changed = false;
  uint8_t idx = 0;

  // Sensors S01..S16
  if (Esn->icmRegistryIndexOfSensorMac(mac, idx)) {
    changed |= Esn->icmRegistryClearSensor(idx);
  }

  // Relays R01..R16
  if (Esn->icmRegistryIndexOfRelayMac(mac, idx)) {
    changed |= Esn->icmRegistryClearRelay(idx);
  }

  // PMS P01 (single slot)
  if (Cfg && Cfg->Iskey(NVS_REG_P_MAC_CONST)) {
    String pHex = Cfg->GetString(NVS_REG_P_MAC_CONST, "");
    uint8_t p[6];
    if (!pHex.isEmpty() && macFromHex(pHex, p) && Core::macEq(p, mac)) {
      changed |= Esn->icmRegistryClearPower();
    }
  }

  // Return true if either remote unpair ACKed or local registry changed.
  return (ok_unpair || changed);
#else
  // Non-ICM builds: just report whether we managed to signal PR_UNPAIR.
  return ok_unpair;
#endif
}

String BleICM::esnSerializePeers() {
  DynamicJsonDocument d(4096);
  JsonArray sens = d.createNestedArray("sensors");
  JsonArray rlys = d.createNestedArray("relays");
  JsonArray pArr = d.createNestedArray("pms");

#ifdef NVS_ROLE_ICM
  if (Esn && Esn->slotsLoadFromRegistry()) {
    for (auto& s : Esn->sensors) if (!Core::macIsZero(s.mac)) {
      JsonObject o = sens.createNestedObject();
      o["idx"] = s.index; o["mac"] = macToHex(s.mac);
      o["present"] = s.present; o["lastSeenMs"] = s.lastSeenMs; o["rssi"] = s.lastRSSI;
    }
    for (auto& r : Esn->relays) if (!Core::macIsZero(r.mac)) {
      JsonObject o = rlys.createNestedObject();
      o["idx"] = r.index; o["mac"] = macToHex(r.mac);
      o["present"] = r.present; o["lastSeenMs"] = r.lastSeenMs; o["rssi"] = r.lastRSSI;
    }
    for (auto& p : Esn->pms) if (!Core::macIsZero(p.mac)) {
      JsonObject o = pArr.createNestedObject();
      o["idx"] = p.index; o["mac"] = macToHex(p.mac);
      o["present"] = p.present; o["lastSeenMs"] = p.lastSeenMs; o["rssi"] = p.lastRSSI;
    }
  }
#endif
  String out; serializeJson(d, out);
  return out;
}

bool BleICM::esnConfigureTopology(JsonVariantConst  topoObj) {
  if (!Esn || !topoObj.is<JsonObjectConst>()) return false;

  bool ok = true;
  // Expect: { "sensor": { "AA:..": {...} }, "relay": { "BB:..": {...} } }
  if (topoObj.containsKey("sensor")) {
    JsonObjectConst  sj = topoObj["sensor"].as<JsonObjectConst>();
    for (JsonPairConst  kv : sj) {
      uint8_t mac[6]; if (!macFromHex(String(kv.key().c_str()), mac)) { ok = false; continue; }
      String body; serializeJson(kv.value(), body);
      ok &= (Esn->topSetSensorJSON(mac, body.c_str(), body.length(), 80) == ESP_OK);
    }
  }
  if (topoObj.containsKey("relay")) {
    JsonObjectConst  rj = topoObj["relay"].as<JsonObjectConst >();
    for (JsonPairConst  kv : rj) {
      uint8_t mac[6]; if (!macFromHex(String(kv.key().c_str()), mac)) { ok = false; continue; }
      String body; serializeJson(kv.value(), body);
      ok &= (Esn->topSetRelayJSON(mac, body.c_str(), body.length(), 80) == ESP_OK);
    }
  }

  // Cache latest authored topology blob on ICM
  if (ok && Cfg) {
    String snap; serializeJson(topoObj, snap);
    Cfg->PutString(NVS_KEY_TOPO_STRING, snap);
  }
  return ok;
}

String BleICM::esnSerializeTopology() {
  if (!Cfg) return String("{\"ver\":1}");
  return Cfg->GetString(NVS_KEY_TOPO_STRING, NVS_DEF_TOPO_STRING);
}

String BleICM::esnExportConfiguration() {
  DynamicJsonDocument d(8192);
  d["icm_mac"] = Core::efuseMac12();
  d["esn_ch"]  = Esn ? Esn->channel() : 1;

  DynamicJsonDocument peers(4096);
  deserializeJson(peers, esnSerializePeers());
  d["peers"] = peers.as<JsonObject>();

  d["topology"] = esnSerializeTopology();

  String out; serializeJson(d, out);
  return out;
}

// ------------------------ BLE callbacks ------------------------
uint32_t BleICMSecurityCB::onPassKeyRequest() {
  return BleICM::instance && BleICM::instance->Cfg
         ? BleICM::instance->Cfg->GetInt(NVS_KEY_BLE_PASSK, NVS_DEF_BLE_PASSK)
         : NVS_DEF_BLE_PASSK;
}
void BleICMSecurityCB::onPassKeyNotify(uint32_t) {}
bool BleICMSecurityCB::onConfirmPIN(uint32_t) { vTaskDelay(pdMS_TO_TICKS(500)); return true; }
bool BleICMSecurityCB::onSecurityRequest() { return true; }
void BleICMSecurityCB::onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) {
  if (!cmpl.success && ICM_pServer) ICM_pServer->removePeerDevice(ICM_pServer->getConnId(), true);
}

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

void BleICMChCB::onWrite(BLECharacteristic* ch) {
  if (!BleICM::instance) return;
  BleICM& icm = *BleICM::instance;
  const std::string v = ch->getValue();

  // STATUS: any write triggers a snapshot
  if (ch == ICM_chStatus) { icm.notifyStatus(); return; }

  // WIFI control
  if (ch == ICM_chWifi) {
    DynamicJsonDocument d(1024);
    if (!icm_parseJson(v, d)) { icm_sendAck(ch, false, "bad json"); return; }
    const char* cmd = d["cmd"] | "";

    if (!strcmp(cmd, "chan")) {
      uint8_t chn = (uint8_t)(d["ch"] | (icm.Cfg ? icm.Cfg->GetInt(NVS_KEY_NET_CHAN, NVS_DEF_NET_CHAN) : 1));
      bool persist = d["persist"] | true;
      bool ok = icm.esnSetChannel(chn, persist);
      icm.notifyStatus(); icm_sendAck(ch, ok); return;
    }

    if (!strcmp(cmd, "sta_connect")) {
      String ssid = d["ssid"] | ""; String psk = d["password"] | "";
      if (ssid.length()) { WiFi.mode(WIFI_STA); WiFi.persistent(false); WiFi.disconnect(true); delay(50); WiFi.begin(ssid.c_str(), psk.c_str()); }
      else               { WiFi.mode(WIFI_STA); WiFi.persistent(false); WiFi.disconnect(true); delay(50); WiFi.begin(); }
      uint32_t t0 = millis(); while (WiFi.status()!=WL_CONNECTED && millis()-t0<15000) vTaskDelay(pdMS_TO_TICKS(100));
      if (WiFi.status()==WL_CONNECTED) { icm.esnSetChannel((uint8_t)WiFi.channel(), true); }
      icm.notifyStatus(); icm_sendAck(ch, WiFi.status()==WL_CONNECTED); return;
    }

    if (!strcmp(cmd, "ap_start")) {
      String apSsid = d["ap_ssid"] | (icm.Cfg ? icm.Cfg->GetString(NVS_KEY_WIFI_APSSID, NVS_DEF_WIFI_APSSID) : String("ICM_AP"));
      String apPass = d["ap_password"] | (icm.Cfg ? icm.Cfg->GetString(NVS_KEY_WIFI_APKEY,  NVS_DEF_WIFI_APKEY ) : String("12345678"));
      int chn = d["ch"] | (icm.Cfg ? icm.Cfg->GetInt(NVS_KEY_NET_CHAN, NVS_DEF_NET_CHAN) : 1);
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_AP);
      WiFi.softAPConfig(LOCAL_IP, GATEWAY, SUBNET);
      // Pick a valid AP channel: use chn if 1..13, else fall back to NVS value (or default)
      int apCh = (chn >= 1 && chn <= 13)
                  ? chn
                  : (icm.Cfg ? icm.Cfg->GetInt(NVS_KEY_NET_CHAN, NVS_DEF_NET_CHAN) : NVS_DEF_NET_CHAN);

      WiFi.softAP(apSsid.c_str(), apPass.c_str(), apCh);
      icm.esnSetChannel(static_cast<uint8_t>(apCh), true);

      icm.notifyStatus(); icm_sendAck(ch, true); return;
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
      icm_notifyJson(ch, s); return;
    }

    icm_sendAck(ch, false, "unknown wifi cmd"); return;
  }

  // PEERS
  if (ch == ICM_chPeers) {
    DynamicJsonDocument d(2048);
    if (!icm_parseJson(v, d)) { icm_sendAck(ch, false, "bad json"); return; }
    const char* cmd = d["cmd"] | "";

    if (!strcmp(cmd, "pair"))   { bool ok = icm.esnPair(String(d["mac"]|""), String(d["type"]|"")); icm_sendAck(ch, ok); return; }
    if (!strcmp(cmd, "remove")) { bool ok = icm.esnRemovePeer(String(d["mac"]|"")); icm_sendAck(ch, ok); return; }
    if (!strcmp(cmd, "list"))   { icm_notifyJson(ch, icm.esnSerializePeers()); return; }

    icm_sendAck(ch, false, "unknown peers cmd"); return;
  }

  // TOPOLOGY
  if (ch == ICM_chTopo) {
    DynamicJsonDocument d(8192);
    if (!icm_parseJson(v, d)) { icm_sendAck(ch, false, "bad json"); return; }
    const char* cmd = d["cmd"] | "";

    if (!strcmp(cmd, "set")) { bool ok = icm.esnConfigureTopology(d["links"]); icm_sendAck(ch, ok); return; }
    if (!strcmp(cmd, "get")) { icm_notifyJson(ch, icm.esnSerializeTopology()); return; }

    icm_sendAck(ch, false, "unknown topo cmd"); return;
  }

  // SEQ (stubs)
  if (ch == ICM_chSeq) {
    DynamicJsonDocument d(512);
    if (!icm_parseJson(v, d)) { icm_sendAck(ch, false, "bad json"); return; }
    const char* cmd = d["cmd"] | "";
    if (!strcmp(cmd, "start") || !strcmp(cmd, "stop")) { icm_sendAck(ch, true); return; }
    icm_sendAck(ch, false, "unknown seq cmd"); return;
  }

  // POWER (stub)
  if (ch == ICM_chPower) {
    DynamicJsonDocument out(256); out["status"] = "ok";
    String s; serializeJson(out, s);
    icm_notifyJson(ch, s); return;
  }

  // EXPORT/IMPORT
  if (ch == ICM_chExport) {
    DynamicJsonDocument d(8192);
    if (!icm_parseJson(v, d)) { icm_sendAck(ch, false, "bad json"); return; }
    const char* cmd = d["cmd"] | "";

    if (!strcmp(cmd, "export")) { icm_notifyJson(ch, icm.esnExportConfiguration()); return; }
    if (!strcmp(cmd, "import")) {
      bool ok = true;
      if (d.containsKey("config")) {
        JsonVariantConst cfg = d["config"];
        if (cfg.containsKey("esn_ch")) {
          int chn = cfg["esn_ch"].as<int>(); if (chn >= 1 && chn <= 13) ok &= icm.esnSetChannel((uint8_t)chn, true);
        }
        if (cfg.containsKey("links")) {
          ok &= icm.esnConfigureTopology(cfg["links"]);
        }
      } else ok = false;
      icm_sendAck(ch, ok); return;
    }

    icm_sendAck(ch, false, "unknown export cmd"); return;
  }

  // Back-compat simple echo
  if (ch == ICM_chOldApp) {
    String s = String(v.c_str());
    if (s.equalsIgnoreCase("STATUS")) { icm.notifyStatus(); return; }
    icm_notifyJson(ch, String("{\"ok\":false,\"msg\":\"UNKNOWN\"}")); return;
  }
}

void BleICMChCB::onRead(BLECharacteristic* c) {
  if (c == ICM_chStatus && BleICM::instance) BleICM::instance->notifyStatus();
}
