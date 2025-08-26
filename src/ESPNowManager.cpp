/**************************************************************
 *  Project     : ICM (Interface Control Module)
 *  File        : ESPNowManager.cpp (token- & topology-aware rewrite)
 **************************************************************/
#include "ESPNowManager.h"

// ============ Static ===============
ESPNowManager* ESPNowManager::s_inst = nullptr;

// ============ Small logging helpers ===========
#define LOGI(code, fmt, ...) do{ if(_log) _log->eventf(ICMLogFS::DOM_SYSTEM, ICMLogFS::EV_INFO,  (code), "[ESPNOW] " fmt, ##__VA_ARGS__);}while(0)
#define LOGW(code, fmt, ...) do{ if(_log) _log->eventf(ICMLogFS::DOM_SYSTEM, ICMLogFS::EV_WARN,  (code), "[ESPNOW] " fmt, ##__VA_ARGS__);}while(0)
#define LOGE(code, fmt, ...) do{ if(_log) _log->eventf(ICMLogFS::DOM_SYSTEM, ICMLogFS::EV_ERROR, (code), "[ESPNOW] " fmt, ##__VA_ARGS__);}while(0)

// ============ Ctor =================
ESPNowManager::ESPNowManager(ConfigManager* cfg, ICMLogFS* log, RTCManager* rtc)
: _cfg(cfg), _log(log), _rtc(rtc) {
  s_inst = this;
}

// ============ Begin/End ============
bool ESPNowManager::begin(uint8_t channelDefault, const char* pmk16) {
  _channel = (uint8_t)_cfg->GetInt(keyCh().c_str(), (int)channelDefault);
  _mode    = (uint8_t)_cfg->GetInt(keyMd().c_str(), (int)MODE_AUTO);

  // Try to load a saved topology mirror
  loadTopologyFromNvs();

  for (size_t i=0; i<ICM_MAX_RELAYS; ++i) { _relTempC[i] = NAN; _relTempMs[i] = 0; }
  _pwrTempC = NAN; _pwrTempMs = 0;

  WiFi.mode(WIFI_STA);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(_channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  if (esp_now_init() != ESP_OK) { LOGE(1001,"esp_now_init failed"); return false; }

  if (pmk16 && strlen(pmk16)==16) {
    memcpy(_pmk, pmk16, 16); _pmk[16]=0;
    esp_now_set_pmk((uint8_t*)_pmk);
  }

  esp_now_register_recv_cb(&ESPNowManager::onRecvThunk);
  esp_now_register_send_cb(&ESPNowManager::onSentThunk);

  _started = true;
  LOGI(1000,"ESP-NOW started ch=%u mode=%u", _channel, _mode);
  return true;
}
void ESPNowManager::end() {
  if (!_started) return;
  esp_now_unregister_recv_cb();
  esp_now_unregister_send_cb();
  esp_now_deinit();
  _started = false;
  LOGI(1010,"ESP-NOW stopped");
}

// ============ Poller (retries + timeouts) ============
void ESPNowManager::poll() {
  const uint32_t now = millis();
  for (int i=0;i<MAX_PENDING;i++) {
    PendingTx& tx = _pending[i];
    if (!tx.used) continue;
    if (now < tx.deadlineMs) continue;

    PeerRec* pr = findPeerByMac(tx.mac);
    if (!pr) { freePending(i); continue; }

    if (tx.requireAck) {
      if (tx.retriesLeft > 0) {
        scheduleRetry(tx);
        startSend(i);
        pr->activeTx = i;
      } else {
        LOGW(1503, "ACK timeout ctr=%u mac=%s", tx.ctr, macBytesToStr(tx.mac).c_str());
        markPeerFail(pr);
        freePending(i);
        pr->activeTx = -1;
      }
    } else {
      freePending(i);
      pr->activeTx = -1;
    }
  }
}

// ============ Utilities ============

bool ESPNowManager::macStrToBytes(const String& mac, uint8_t out[6]) {
  int v[6];
  if (sscanf(mac.c_str(), "%x:%x:%x:%x:%x:%x", &v[0],&v[1],&v[2],&v[3],&v[4],&v[5]) != 6) return false;
  for(int i=0;i<6;i++) out[i]=(uint8_t)v[i];
  return true;
}

String ESPNowManager::macBytesToStr(const uint8_t mac[6]) {
  char b[18];
  snprintf(b,sizeof(b),"%02X:%02X:%02X:%02X:%02X:%02X",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  return String(b);
}
String ESPNowManager::icmMacStr(){ return WiFi.macAddress(); }

String ESPNowManager::keyTok(ModuleType t, uint8_t index) {
  char k[8]={0};
  switch(t){
    case ModuleType::POWER: snprintf(k,sizeof(k), "PWTOK"); break;
    case ModuleType::RELAY: snprintf(k,sizeof(k), "RT%02uTK", index); break;        // 6 chars
    case ModuleType::PRESENCE:
      if (index==PRES_IDX_ENTRANCE) snprintf(k,sizeof(k), "SETNTK");
      else if(index==PRES_IDX_PARKING) snprintf(k,sizeof(k), "SPRKTK");
      else snprintf(k,sizeof(k), "ST%02uTK", index);
      break;
  }
  return String(k);
}
String ESPNowManager::keyMac(ModuleType t, uint8_t index) {
  char k[8]={0};
  switch(t){
    case ModuleType::POWER: snprintf(k,sizeof(k), "PWMAC"); break;
    case ModuleType::RELAY: snprintf(k,sizeof(k), "RM%02uMC", index); break;        // 6 chars
    case ModuleType::PRESENCE:
      if (index==PRES_IDX_ENTRANCE) snprintf(k,sizeof(k), "SETNMC");
      else if(index==PRES_IDX_PARKING) snprintf(k,sizeof(k), "SPRKMC");
      else snprintf(k,sizeof(k), "SM%02uMC", index);
      break;
  }
  return String(k);
}

void ESPNowManager::tokenCompute(const String& icmMac, const String& nodeMac,uint32_t counter, String& tokenHex32) {
  String s = icmMac + "|" + nodeMac + "|" + String(counter);
  uint8_t dig[32];
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts_ret(&ctx, 0);
  mbedtls_sha256_update_ret(&ctx, (const uint8_t*)s.c_str(), s.length());
  mbedtls_sha256_finish_ret(&ctx, dig);
  mbedtls_sha256_free(&ctx);
  char hex[65]; for(int i=0;i<32;i++) sprintf(hex+2*i,"%02X",dig[i]); hex[64]=0;
  tokenHex32 = String(hex);
}
void ESPNowManager::tokenHexTo16(const String& hex, uint8_t out[16]) {
  memset(out,0,16);
  for (int i=0;i<16 && (i*2+1)<(int)hex.length();i++){
    uint8_t v=0; sscanf(hex.substring(i*2,i*2+2).c_str(), "%02hhX",&v);
    out[i]=v;
  }
}
uint32_t ESPNowManager::takeAndBumpTokenCounter(){
  uint32_t ctr = (uint32_t)_cfg->GetInt(keyCtr().c_str(), 1);
  _cfg->PutInt(keyCtr().c_str(), (int)(ctr+1));
  return ctr;
}
bool ESPNowManager::loadOrCreateToken(ModuleType t, uint8_t index, const String& macStr,String& tokenHexOut, uint8_t token16Out[16]) {
  String kTok = keyTok(t,index);
  String hex  = _cfg->GetString(kTok.c_str(), "");
  if (hex.length()==64) {
    tokenHexOut = hex;
    tokenHexTo16(hex, token16Out);
    return true;
  }
  uint32_t ctr = takeAndBumpTokenCounter();
  String icm  = icmMacStr();
  tokenCompute(icm, macStr, ctr, hex);
  tokenHexOut = hex;
  tokenHexTo16(hex, token16Out);
  _cfg->PutString(kTok.c_str(), hex.c_str());
  return true;
}

// ============ Peer management =============
bool ESPNowManager::addOrUpdatePeer(ModuleType t, uint8_t index, const uint8_t mac[6]) {
  esp_now_peer_info_t p{}; memcpy(p.peer_addr, mac, 6);
  p.ifidx = WIFI_IF_STA; p.encrypt = 0; p.channel = _channel;
  esp_now_del_peer(mac); // idempotent
  if (esp_now_add_peer(&p) != ESP_OK) return false;

  PeerRec* pr = findPeer(t,index);
  if (!pr) {
    switch(t){
      case ModuleType::POWER:   pr=&_power; break;
      case ModuleType::RELAY:   pr=&_relays[index]; break;
      case ModuleType::PRESENCE:
        if (index==PRES_IDX_ENTRANCE) pr=&_entrance;
        else if (index==PRES_IDX_PARKING) pr=&_parking;
        else pr=&_sensors[index];
        break;
    }
  }
  pr->used = true; pr->type=t; pr->index=index; memcpy(pr->mac, mac, 6);
  pr->online = true; pr->consecFails = 0; pr->activeTx = -1;
  return true;
}
ESPNowManager::PeerRec* ESPNowManager::findPeerByMac(const uint8_t mac[6]) {
  auto eq = [&](const PeerRec& r){ return r.used && memcmp(r.mac,mac,6)==0; };
  if (eq(_power)) return &_power;
  if (eq(_entrance)) return &_entrance;
  if (eq(_parking )) return &_parking;
  for (size_t i=0;i<ICM_MAX_RELAYS;i++){ if (eq(_relays[i]))  return &_relays[i]; }
  for (size_t i=0;i<ICM_MAX_SENSORS;i++){ if (eq(_sensors[i])) return &_sensors[i]; }
  return nullptr;
}
ESPNowManager::PeerRec* ESPNowManager::findPeer(ModuleType t, uint8_t index) {
  switch(t){
    case ModuleType::POWER: return _power.used?&_power:nullptr;
    case ModuleType::RELAY: return (index<ICM_MAX_RELAYS && _relays[index].used)?&_relays[index]:nullptr;
    case ModuleType::PRESENCE:
      if (index==PRES_IDX_ENTRANCE) return _entrance.used?&_entrance:nullptr;
      if (index==PRES_IDX_PARKING ) return _parking .used?&_parking :nullptr;
      return (index<ICM_MAX_SENSORS && _sensors[index].used)?&_sensors[index]:nullptr;
  }
  return nullptr;
}
bool ESPNowManager::ensurePeer(ModuleType t, uint8_t index, PeerRec*& out) {
  out = findPeer(t,index);
  return out != nullptr;
}

// Auto-index helpers
uint8_t ESPNowManager::nextFreeRelayIndex() const {
  for (uint8_t i=0;i<ICM_MAX_RELAYS;i++) if (!_relays[i].used) return i;
  return 0xFF;
}
uint8_t ESPNowManager::nextFreeSensorIndex() const {
  for (uint8_t i=0;i<ICM_MAX_SENSORS;i++) if (!_sensors[i].used) return i;
  return 0xFF;
}

// ============ Pair/Unpair ============
bool ESPNowManager::pairPower(const String& macStr) {
  uint8_t mac[6]; if(!macStrToBytes(macStr, mac)) return false;
  if (!addOrUpdatePeer(ModuleType::POWER, 0, mac)) return false;
  _cfg->PutString(keyMac(ModuleType::POWER,0).c_str(), macStr.c_str());
  String hex; loadOrCreateToken(ModuleType::POWER, 0, macStr, hex, _power.token16);
  LOGI(1100,"Paired POWER %s", macStr.c_str());
  return true;
}
bool ESPNowManager::pairRelay(uint8_t idx, const String& macStr) {
  if (idx>=ICM_MAX_RELAYS) return false;
  uint8_t mac[6]; if(!macStrToBytes(macStr, mac)) return false;
  if (!addOrUpdatePeer(ModuleType::RELAY, idx, mac)) return false;
  _cfg->PutString(keyMac(ModuleType::RELAY,idx).c_str(), macStr.c_str());
  String hex; loadOrCreateToken(ModuleType::RELAY, idx, macStr, hex, _relays[idx].token16);
  LOGI(1110,"Paired RELAY[%u] %s", idx, macStr.c_str());
  return true;
}
bool ESPNowManager::pairPresence(uint8_t idx, const String& macStr) {
  if (idx>=ICM_MAX_SENSORS) return false;
  uint8_t mac[6]; if(!macStrToBytes(macStr, mac)) return false;
  if (!addOrUpdatePeer(ModuleType::PRESENCE, idx, mac)) return false;
  _cfg->PutString(keyMac(ModuleType::PRESENCE,idx).c_str(), macStr.c_str());
  String hex; loadOrCreateToken(ModuleType::PRESENCE, idx, macStr, hex, _sensors[idx].token16);
  LOGI(1120,"Paired SENS[%u] %s", idx, macStr.c_str());
  return true;
}
bool ESPNowManager::pairPresenceEntrance(const String& macStr) {
  uint8_t mac[6]; if(!macStrToBytes(macStr, mac)) return false;
  if (!addOrUpdatePeer(ModuleType::PRESENCE, PRES_IDX_ENTRANCE, mac)) return false;
  _cfg->PutString(keyMac(ModuleType::PRESENCE,PRES_IDX_ENTRANCE).c_str(), macStr.c_str());
  String hex; loadOrCreateToken(ModuleType::PRESENCE, PRES_IDX_ENTRANCE, macStr, hex, _entrance.token16);
  LOGI(1121,"Paired SENS[ENTR] %s", macStr.c_str());
  return true;
}
bool ESPNowManager::pairPresenceParking(const String& macStr) {
  uint8_t mac[6]; if(!macStrToBytes(macStr, mac)) return false;
  if (!addOrUpdatePeer(ModuleType::PRESENCE, PRES_IDX_PARKING, mac)) return false;
  _cfg->PutString(keyMac(ModuleType::PRESENCE,PRES_IDX_PARKING).c_str(), macStr.c_str());
  String hex; loadOrCreateToken(ModuleType::PRESENCE, PRES_IDX_PARKING, macStr, hex, _parking.token16);
  LOGI(1122,"Paired SENS[PARK] %s", macStr.c_str());
  return true;
}
bool ESPNowManager::pairRelayAuto(const String& macStr, uint8_t* outIdx){
  // try NVS hint first, then fall back to first free
  int hint = _cfg->GetInt(keyRNext().c_str(), 0);
  for (int k=0;k<ICM_MAX_RELAYS;k++){
    uint8_t idx = (hint + k) % ICM_MAX_RELAYS;
    if (!_relays[idx].used) {
      bool ok = pairRelay(idx, macStr);
      if (ok) { _cfg->PutInt(keyRNext().c_str(), (int)((idx+1)%ICM_MAX_RELAYS)); if(outIdx)*outIdx=idx; }
      return ok;
    }
  }
  return false;
}
bool ESPNowManager::pairPresenceAuto(const String& macStr, uint8_t* outIdx){
  int hint = _cfg->GetInt(keySNext().c_str(), 0);
  for (int k=0;k<ICM_MAX_SENSORS;k++){
    uint8_t idx = (hint + k) % ICM_MAX_SENSORS;
    if (!_sensors[idx].used) {
      bool ok = pairPresence(idx, macStr);
      if (ok) { _cfg->PutInt(keySNext().c_str(), (int)((idx+1)%ICM_MAX_SENSORS)); if(outIdx)*outIdx=idx; }
      return ok;
    }
  }
  return false;
}
bool ESPNowManager::unpairByMac(const String& macStr) {
  uint8_t mac[6]; if(!macStrToBytes(macStr, mac)) return false;
  esp_now_del_peer(mac);
  auto clearPR=[&](PeerRec& pr){
    if (pr.used && memcmp(pr.mac,mac,6)==0) { memset(&pr,0,sizeof(pr)); }
  };
  clearPR(_power); clearPR(_entrance); clearPR(_parking);
  for (size_t i=0;i<ICM_MAX_RELAYS;i++)  clearPR(_relays[i]);
  for (size_t i=0;i<ICM_MAX_SENSORS;i++) clearPR(_sensors[i]);
  LOGI(1130,"Unpaired %s", macStr.c_str());
  return true;
}

// ============ High-level helpers requested by WiFiManager ============

bool ESPNowManager::pair(const String& mac, const String& type) {
  String t = type; t.toLowerCase();
  if (t == "power")    return pairPower(mac);
  if (t == "entrance") return pairPresenceEntrance(mac);
  if (t == "parking")  return pairPresenceParking(mac);
  if (t.startsWith("relay")) {
    if (t == "relay") return pairRelayAuto(mac);
    int idx = t.substring(5).toInt();
    return pairRelay((uint8_t)idx, mac);
  }
  if (t.startsWith("sensor")) {
    if (t == "sensor") return pairPresenceAuto(mac);
    int idx = t.substring(6).toInt();
    return pairPresence((uint8_t)idx, mac);
  }
  return false;
}

void ESPNowManager::removeAllPeers() {
  if (_power.used)   { esp_now_del_peer(_power.mac);   memset(&_power,0,sizeof(_power)); }
  if (_entrance.used){ esp_now_del_peer(_entrance.mac);memset(&_entrance,0,sizeof(_entrance)); }
  if (_parking.used) { esp_now_del_peer(_parking.mac); memset(&_parking,0,sizeof(_parking)); }
  for (size_t i=0;i<ICM_MAX_RELAYS;i++){
    if (_relays[i].used){ esp_now_del_peer(_relays[i].mac); memset(&_relays[i],0,sizeof(_relays[i])); }
    _relayTopo[i] = RelayLink{};
  }
  for (size_t i=0;i<ICM_MAX_SENSORS;i++){
    if (_sensors[i].used){ esp_now_del_peer(_sensors[i].mac); memset(&_sensors[i],0,sizeof(_sensors[i])); }
    _sensorTopo[i] = SensorDep{};
  }
  _entrTopo = SensorDep{};
  _parkTopo = SensorDep{};
  LOGI(1140,"All peers removed (RAM)");
}

void ESPNowManager::clearAll() {
  removeAllPeers();
  // forget NVS MACs/tokens/mode/channel/topology
  _cfg->PutInt(keyCh().c_str(), 1);
  _cfg->PutInt(keyMd().c_str(), MODE_AUTO);
  _cfg->PutString(keyTopo().c_str(), "");
  _cfg->PutInt(keyRNext().c_str(), 0);
  _cfg->PutInt(keySNext().c_str(), 0);
  // Power
  _cfg->PutString(keyMac(ModuleType::POWER,0).c_str(), "");
  _cfg->PutString(keyTok(ModuleType::POWER,0).c_str(), "");
  // Relays & Sensors
  for (uint8_t i=0;i<ICM_MAX_RELAYS;i++){
    _cfg->PutString(keyMac(ModuleType::RELAY,i).c_str(), "");
    _cfg->PutString(keyTok(ModuleType::RELAY,i).c_str(), "");
  }
  for (uint8_t i=0;i<ICM_MAX_SENSORS;i++){
    _cfg->PutString(keyMac(ModuleType::PRESENCE,i).c_str(), "");
    _cfg->PutString(keyTok(ModuleType::PRESENCE,i).c_str(), "");
  }
  // Specials
  _cfg->PutString(keyMac(ModuleType::PRESENCE,PRES_IDX_ENTRANCE).c_str(), "");
  _cfg->PutString(keyTok(ModuleType::PRESENCE,PRES_IDX_ENTRANCE).c_str(), "");
  _cfg->PutString(keyMac(ModuleType::PRESENCE,PRES_IDX_PARKING ).c_str(), "");
  _cfg->PutString(keyTok(ModuleType::PRESENCE,PRES_IDX_PARKING ).c_str(), "");
  LOGI(1141,"NVS cleared (peers/tokens/mode/channel/topology)");
}

String ESPNowManager::serializePeers() const {
  DynamicJsonDocument doc(2048);
  doc["channel"] = _channel;
  doc["mode"]    = _mode;

  // Power
  JsonObject jp = doc.createNestedObject("power");
  if (_power.used) {
    jp["mac"]    = macBytesToStr(_power.mac);
    jp["online"] = _power.online;
    jp["fails"]  = _power.consecFails;
  } else {
    jp["mac"] = "";
  }

  // Entrance / Parking
  JsonObject je = doc.createNestedObject("entrance");
  if (_entrance.used) { je["mac"]=macBytesToStr(_entrance.mac); je["online"]=_entrance.online; } else je["mac"]="";
  JsonObject jk = doc.createNestedObject("parking");
  if (_parking .used) { jk["mac"]=macBytesToStr(_parking.mac);  jk["online"]=_parking.online; } else jk["mac"]="";

  // Relays
  JsonArray r = doc.createNestedArray("relays");
  for (size_t i=0;i<ICM_MAX_RELAYS;i++){
    if (!_relays[i].used) continue;
    JsonObject o = r.createNestedObject();
    o["idx"]    = (uint8_t)i;
    o["mac"]    = macBytesToStr(_relays[i].mac);
    o["online"] = _relays[i].online;
  }

  // Sensors (middle)
  JsonArray s = doc.createNestedArray("sensors");
  for (size_t i=0;i<ICM_MAX_SENSORS;i++){
    if (!_sensors[i].used) continue;
    JsonObject o = s.createNestedObject();
    o["idx"]    = (uint8_t)i;
    o["mac"]    = macBytesToStr(_sensors[i].mac);
    o["online"] = _sensors[i].online;
  }

  String out; serializeJson(doc, out);
  return out;
}

String ESPNowManager::serializeTopology() const {
  DynamicJsonDocument doc(3072);
  JsonArray links = doc.createNestedArray("links");

  // relay next hops (+ prev sensor if known)
  for (size_t i=0;i<ICM_MAX_RELAYS;i++){
    if (!_relays[i].used || !_relayTopo[i].used) continue;
    JsonObject l = links.createNestedObject();
    l["type"]     = "relay";
    l["idx"]      = (uint8_t)i;
    l["next_mac"] = macBytesToStr(_relayTopo[i].nextMac);
    l["next_ip"]  = _relayTopo[i].nextIPv4;
    if (_relayTopo[i].hasPrev) {
      l["prev_sens_idx"] = _relayTopo[i].prevSensIdx;
      l["prev_sens_mac"] = macBytesToStr(_relayTopo[i].prevSensMac);
    }
  }

  // sensor dependencies
  for (size_t i=0;i<ICM_MAX_SENSORS;i++){
    if (!_sensors[i].used || !_sensorTopo[i].used) continue;
    JsonObject l = links.createNestedObject();
    l["type"]        = "sensor";
    l["idx"]         = (uint8_t)i;
    l["target_type"] = _sensorTopo[i].targetType;
    l["target_idx"]  = _sensorTopo[i].targetIdx;
    l["target_mac"]  = macBytesToStr(_sensorTopo[i].targetMac);
    l["target_ip"]   = _sensorTopo[i].targetIPv4;
  }

  if (_entrance.used && _entrTopo.used) {
    JsonObject l = links.createNestedObject();
    l["type"]        = "entrance";
    l["idx"]         = (uint8_t)PRES_IDX_ENTRANCE;
    l["target_idx"]  = _entrTopo.targetIdx;
    l["target_mac"]  = macBytesToStr(_entrTopo.targetMac);
    l["target_ip"]   = _entrTopo.targetIPv4;
  }
  if (_parking.used && _parkTopo.used) {
    JsonObject l = links.createNestedObject();
    l["type"]        = "parking";
    l["idx"]         = (uint8_t)PRES_IDX_PARKING;
    l["target_idx"]  = _parkTopo.targetIdx;
    l["target_mac"]  = macBytesToStr(_parkTopo.targetMac);
    l["target_ip"]   = _parkTopo.targetIPv4;
  }

  String out; serializeJson(doc, out);
  return out;
}

String ESPNowManager::exportConfiguration() const {
  DynamicJsonDocument doc(6144);
  doc["channel"] = _channel;
  doc["mode"]    = _mode;

  // peers
  DynamicJsonDocument jPeers(2048);
  deserializeJson(jPeers, serializePeers());
  doc["peers"] = jPeers.as<JsonVariant>();

  // topology
  DynamicJsonDocument jTopo(4096);
  deserializeJson(jTopo, serializeTopology());
  doc["topology"] = jTopo.as<JsonVariant>();

  // Prefer PSRAM for big JSON buffers
  size_t len = measureJson(doc) + 1;
  char* buf = (char*)heap_caps_malloc(len, MALLOC_CAP_SPIRAM);
  if (!buf) {
    String out; serializeJson(doc, out);
    return out;
  }
  serializeJson(doc, buf, len);
  String out(buf);
  heap_caps_free(buf);
  return out;
}

String ESPNowManager::getEntranceSensorMac() const {
  if (!_entrance.used) return String("");
  return macBytesToStr(_entrance.mac);
}

// ============ Channel / Mode ============
bool ESPNowManager::setChannel(uint8_t ch, bool persist){
  if (ch < 1 || ch > 13) return false;
  if (ch == _channel) { if (persist) _cfg->PutInt(keyCh().c_str(), ch); return true; }

  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  _channel = ch;
  if (persist) _cfg->PutInt(keyCh().c_str(), ch);

  reAddAllPeersOnChannel();
  LOGI(1150,"Channel set to %u and peers re-added", ch);
  return true;
}
void ESPNowManager::reAddAllPeersOnChannel() {
  auto readd = [&](PeerRec& pr){
    if (!pr.used) return;
    esp_now_del_peer(pr.mac);
    esp_now_peer_info_t p{}; memcpy(p.peer_addr, pr.mac, 6);
    p.ifidx = WIFI_IF_STA; p.encrypt = 0; p.channel = _channel;
    esp_now_add_peer(&p);
  };
  readd(_power); readd(_entrance); readd(_parking);
  for (size_t i=0;i<ICM_MAX_RELAYS;i++)  readd(_relays[i]);
  for (size_t i=0;i<ICM_MAX_SENSORS;i++) readd(_sensors[i]);
}

// ============ Header + enqueue/send ============
void ESPNowManager::fillHeader(IcmMsgHdr& h, CmdDomain dom, uint8_t op, uint8_t flags,const uint8_t token16[16]) {
  h.ver   = 1;
  h.dom   = (uint8_t)dom;
  h.op    = op;
  h.flags = flags;
  h.ts    = _rtc ? (uint32_t)_rtc->getUnixTime() : (uint32_t)time(nullptr);
  h.ctr   = nextCtr();
  memcpy(h.tok16, token16, 16);
}

int ESPNowManager::allocPending() {
  for (int i=0;i<MAX_PENDING;i++) if (!_pending[i].used) { _pending[i].used=true; return i; }
  return -1;
}
void ESPNowManager::freePending(int idx) {
  if (idx < 0 || idx >= MAX_PENDING) return;
  _pending[idx] = PendingTx{};  // zero out the slot
}


int ESPNowManager::findPendingForPeer(const uint8_t mac[6]) {
  for (int i=0;i<MAX_PENDING;i++)
    if (_pending[i].used && memcmp(_pending[i].mac,mac,6)==0)
      return i;
  return -1;
}

bool ESPNowManager::enqueueToPeer(PeerRec* pr, CmdDomain dom, uint8_t op, const uint8_t* body, size_t blen, bool requireAck) {
  if (!pr || !pr->used) return false;

  if (pr->activeTx >= 0) {
    LOGW(1400,"Peer busy mac=%s op=%u (drop)", macBytesToStr(pr->mac).c_str(), op);
    return false;
  }

  int idx = allocPending();
  if (idx < 0) { LOGW(1401,"Pending queue full"); return false; }

  PendingTx& tx = _pending[idx];
  memcpy(tx.mac, pr->mac, 6);
  tx.dom = (uint8_t)dom; tx.op = op; tx.requireAck = requireAck;
  tx.retriesLeft = _maxRetries;

  IcmMsgHdr* hdr = (IcmMsgHdr*)tx.frame;
  fillHeader(*hdr, dom, op, requireAck?HDR_FLAG_ACKREQ:0, pr->token16);
  size_t off = sizeof(IcmMsgHdr);
  if (body && blen) { memcpy(tx.frame+off, body, blen); off += blen; }
  tx.len = off;
  tx.ctr = hdr->ctr;

  tx.backoffMs = 0;
  tx.deadlineMs = 0;
  pr->activeTx = idx;
  startSend(idx);
  return true;
}

void ESPNowManager::startSend(int pidx) {
  PendingTx& tx = _pending[pidx];
  esp_err_t e = esp_now_send(tx.mac, tx.frame, tx.len);
  if (e != ESP_OK) {
    PeerRec* pr = findPeerByMac(tx.mac);
    if (!pr) { freePending(pidx); return; }
    if (tx.retriesLeft > 0) {
      LOGW(1500,"esp_now_send err=%d -> retry ctr=%u mac=%s", (int)e, tx.ctr, macBytesToStr(tx.mac).c_str());
      scheduleRetry(tx);
    } else {
      LOGW(1501,"send giveup ctr=%u mac=%s", tx.ctr, macBytesToStr(tx.mac).c_str());
      markPeerFail(pr);
      freePending(pidx);
      pr->activeTx = -1;
    }
  } else {
    PeerRec* pr = findPeerByMac(tx.mac);
    if (!pr) { freePending(pidx); return; }
    if (!tx.requireAck) {
      markPeerOk(pr);
      freePending(pidx);
      pr->activeTx = -1;
    } else {
      tx.deadlineMs = millis() + _ackTimeoutMs;
    }
  }
}

void ESPNowManager::scheduleRetry(PendingTx& tx) {
  tx.retriesLeft--;
  tx.deadlineMs = millis() + _retryBackoffMs;
}

void ESPNowManager::markPeerFail(PeerRec* pr) {
  pr->consecFails++;
  if (pr->consecFails >= 2) pr->online = false;
  LOGW(1504,"markPeerFail mac=%s fails=%u online=%d", macBytesToStr(pr->mac).c_str(), pr->consecFails, pr->online);
}
void ESPNowManager::markPeerOk(PeerRec* pr) {
  pr->consecFails = 0;
  pr->online = true;
}

// Public send facade
bool ESPNowManager::relayGetStatus(uint8_t idx){ return enqueueToPeer(&_relays[idx],CmdDomain::RELAY,REL_GET,nullptr,0,true); }
bool ESPNowManager::relaySet(uint8_t idx, uint8_t ch, bool on){
  PeerRec* pr=nullptr; if(!ensurePeer(ModuleType::RELAY,idx,pr)) return false;
  uint8_t b[2]={ch,(uint8_t)(on?1:0)}; return enqueueToPeer(pr,CmdDomain::RELAY,REL_SET_CH,b,2,true);
}
bool ESPNowManager::relaySetMode(uint8_t idx, uint8_t ch, uint8_t mode){
  PeerRec* pr=nullptr; if(!ensurePeer(ModuleType::RELAY,idx,pr)) return false;
  uint8_t b[2]={ch,mode}; return enqueueToPeer(pr,CmdDomain::RELAY,REL_SET_MODE,b,2,true);
}
bool ESPNowManager::presenceGetStatus(uint8_t idx){ return enqueueToPeer(&_sensors[idx],CmdDomain::SENS,SENS_GET,nullptr,0,true); }
bool ESPNowManager::presenceSetMode(uint8_t idx, uint8_t mode){
  PeerRec* pr=nullptr; if(!ensurePeer(ModuleType::PRESENCE,idx,pr)) return false;
  uint8_t b[1]={mode}; return enqueueToPeer(pr,CmdDomain::SENS,SENS_SET_MODE,b,1,true);
}

// ============ Callbacks ============
void ESPNowManager::onRecvThunk(const uint8_t *mac, const uint8_t *data, int len) {
  if (s_inst) s_inst->onRecv(mac,data,len);
}
void ESPNowManager::onSentThunk(const uint8_t *mac, esp_now_send_status_t status) {
  if (s_inst) s_inst->onSent(mac,status);
}
bool ESPNowManager::tokenMatches(const PeerRec* pr, const IcmMsgHdr& h) const {
  return pr && memcmp(pr->token16, h.tok16, 16) == 0;
}

void ESPNowManager::onSent(const uint8_t *mac, esp_now_send_status_t status) {
  PeerRec* pr = findPeerByMac(mac);
  if (!pr) return;
  int pidx = pr->activeTx;
  if (pidx < 0) return;
  PendingTx& tx = _pending[pidx];

  if (status == ESP_NOW_SEND_SUCCESS) {
    // If ACK required, wait until deadline; else already freed in startSend()
  } else {
    if (tx.retriesLeft > 0) {
      LOGW(1502,"onSent fail -> retry ctr=%u mac=%s", tx.ctr, macBytesToStr(mac).c_str());
      scheduleRetry(tx);
    } else {
      LOGW(1505,"onSent giveup ctr=%u mac=%s", tx.ctr, macBytesToStr(mac).c_str());
      markPeerFail(pr);
      freePending(pidx);
      pr->activeTx = -1;
    }
  }
}

void ESPNowManager::handleAck(const uint8_t mac[6], const IcmMsgHdr& h, const uint8_t* payload, int plen) {
  uint16_t ackCtr = 0; uint8_t code = 0;
  if (plen >= (int)sizeof(SysAckPayload)) {
    const SysAckPayload* ap = (const SysAckPayload*)payload;
    ackCtr = ap->ctr; code = ap->code;
  } else if (plen >= 1) {
    code = payload[0];
    PeerRec* pr = findPeerByMac(mac);
    if (pr && pr->activeTx >= 0) ackCtr = _pending[pr->activeTx].ctr;
  }

  PeerRec* pr = findPeerByMac(mac);
  if (!pr) return;
  int pidx = pr->activeTx;
  if (pidx < 0) return;

  PendingTx& tx = _pending[pidx];
  if (ackCtr != tx.ctr) return;

  markPeerOk(pr);
  if (_onAck) _onAck(mac, ackCtr, code);
  freePending(pidx);
  pr->activeTx = -1;
}

void ESPNowManager::onRecv(const uint8_t *mac, const uint8_t *data, int len) {
  if (len < (int)sizeof(IcmMsgHdr)) return;
  const IcmMsgHdr* h = (const IcmMsgHdr*)data;
  const uint8_t* payload = data + sizeof(IcmMsgHdr);
  int plen = len - (int)sizeof(IcmMsgHdr);

  PeerRec* pr = findPeerByMac(mac);
  if (!pr) { LOGW(1200,"RX from unknown %s", macBytesToStr(mac).c_str()); return; }
  if (!tokenMatches(pr,*h)) { LOGW(1201,"Token mismatch from %s dom=%u op=%u", macBytesToStr(mac).c_str(), h->dom, h->op); return; }

  pr->lastSeen = millis();
  markPeerOk(pr);

  switch ((CmdDomain)h->dom) {
    case CmdDomain::SYS:
      if (h->op == SYS_ACK) { handleAck(mac, *h, payload, plen); }
      break;

    case CmdDomain::POWER: {
      // Intercept temperature reply
      if (h->op == PWR_GET_TEMP) {
        float tC;
        if (decodeTempPayload(payload, plen, tC)) {
          _pwrTempC  = tC;
          _pwrTempMs = millis();
          LOGI(1600,"POWER temp=%.2fC", tC);
        }
      }
      // Fan out to user callback unchanged
      if (_onPower) _onPower(mac, payload, (size_t)plen);
      break;
    }

    case CmdDomain::RELAY: {
      if (h->op == REL_GET_TEMP) {
        float tC;
        if (decodeTempPayload(payload, plen, tC)) {
          if (pr->index < ICM_MAX_RELAYS) {
            _relTempC[pr->index]  = tC;
            _relTempMs[pr->index] = millis();
            LOGI(1601,"RELAY[%u] temp=%.2fC", pr->index, tC);
          }
        }
      }
      if (_onRelay) _onRelay(mac, pr->index, payload, (size_t)plen);
      break;
    }

    case CmdDomain::SENS:
      if (_onPresence) _onPresence(mac, pr->index, payload, (size_t)plen);
      break;

    default:
      if (_onUnknown) _onUnknown(mac, *h, payload, (size_t)plen);
      break;
  }
}

// ============ System MODE ============
bool ESPNowManager::setSystemModeAuto(bool persist){
  _mode = MODE_AUTO; if (persist) _cfg->PutInt(keyMd().c_str(), _mode);
  SysModePayload p{ _mode };
  bool ok=true;
  if (_power.used) ok &= enqueueToPeer(&_power,CmdDomain::SYS,SYS_MODE,(uint8_t*)&p,sizeof(p),true);
  for (size_t i=0;i<ICM_MAX_RELAYS;i++)  if (_relays[i].used) ok &= enqueueToPeer(&_relays[i],CmdDomain::SYS,SYS_MODE,(uint8_t*)&p,sizeof(p),true);
  for (size_t i=0;i<ICM_MAX_SENSORS;i++) if (_sensors[i].used) ok &= enqueueToPeer(&_sensors[i],CmdDomain::SYS,SYS_MODE,(uint8_t*)&p,sizeof(p),true);
  if (_entrance.used) ok &= enqueueToPeer(&_entrance,CmdDomain::SYS,SYS_MODE,(uint8_t*)&p,sizeof(p),true);
  if (_parking .used) ok &= enqueueToPeer(&_parking ,CmdDomain::SYS,SYS_MODE,(uint8_t*)&p,sizeof(p),true);
  LOGI(1300,"Mode set AUTO");
  return ok;
}
bool ESPNowManager::setSystemModeManual(bool persist){
  _mode = MODE_MAN; if (persist) _cfg->PutInt(keyMd().c_str(), _mode);
  SysModePayload p{ _mode };
  bool ok=true;
  if (_power.used) ok &= enqueueToPeer(&_power,CmdDomain::SYS,SYS_MODE,(uint8_t*)&p,sizeof(p),true);
  for (size_t i=0;i<ICM_MAX_RELAYS;i++)  if (_relays[i].used) ok &= enqueueToPeer(&_relays[i],CmdDomain::SYS,SYS_MODE,(uint8_t*)&p,sizeof(p),true);
  for (size_t i=0;i<ICM_MAX_SENSORS;i++) if (_sensors[i].used) ok &= enqueueToPeer(&_sensors[i],CmdDomain::SYS,SYS_MODE,(uint8_t*)&p,sizeof(p),true);
  if (_entrance.used) ok &= enqueueToPeer(&_entrance,CmdDomain::SYS,SYS_MODE,(uint8_t*)&p,sizeof(p),true);
  if (_parking .used) ok &= enqueueToPeer(&_parking ,CmdDomain::SYS,SYS_MODE,(uint8_t*)&p,sizeof(p),true);
  LOGI(1301,"Mode set MANUAL");
  return ok;
}

// ============ Power helpers ============
bool ESPNowManager::powerGetStatus() { return enqueueToPeer(&_power,CmdDomain::POWER,PWR_GET,nullptr,0,true); }
bool ESPNowManager::powerSetOutput(bool on){ uint8_t b[1]={(uint8_t)(on?1:0)}; return enqueueToPeer(&_power,CmdDomain::POWER,PWR_SET,b,1,true); }
bool ESPNowManager::powerRequestShutdown(){ return enqueueToPeer(&_power,CmdDomain::POWER,PWR_REQSDN,nullptr,0,true); }
bool ESPNowManager::powerClearFault(){ return enqueueToPeer(&_power,CmdDomain::POWER,PWR_CLRF,nullptr,0,true); }
bool ESPNowManager::powerCommand(const String& action) {
  String a = action; a.toLowerCase();
  if (a=="on")  return powerSetOutput(true);
  if (a=="off") return powerSetOutput(false);
  if (a=="shutdown") return powerRequestShutdown();
  if (a=="clear" || a=="clear_faults") return powerClearFault();
  if (a=="status") return powerGetStatus();
  return false;
}
bool ESPNowManager::getPowerModuleInfo(JsonVariant out) {
  JsonObject o = out.to<JsonObject>();
  if (!_power.used) { o["present"]=false; return true; }
  o["present"] = true;
  o["mac"]     = macBytesToStr(_power.mac);
  o["online"]  = _power.online;
  o["fails"]   = _power.consecFails;
  return true;
}

// ============ Manual helpers (via MAC) ============
bool ESPNowManager::relayManualSet(const String& mac, uint8_t ch, bool on) {
  uint8_t m[6]; if(!macStrToBytes(mac,m)) return false;
  PeerRec* pr = findPeerByMac(m);
  if (!pr || pr->type!=ModuleType::RELAY) return false;
  return relaySet(pr->index, ch, on);
}
bool ESPNowManager::sensorSetMode(const String& mac, bool autoMode) {
  uint8_t m[6]; if(!macStrToBytes(mac,m)) return false;
  PeerRec* pr = findPeerByMac(m);
  if (!pr || pr->type!=ModuleType::PRESENCE) return false;
  return presenceSetMode(pr->index, autoMode?0:1);
}
bool ESPNowManager::sensorTestTrigger(const String& mac) {
  uint8_t m[6]; if(!macStrToBytes(mac,m)) return false;
  PeerRec* pr = findPeerByMac(m);
  if (!pr || pr->type!=ModuleType::PRESENCE) return false;
  return enqueueToPeer(pr, CmdDomain::SENS, SENS_TRIG, nullptr, 0, true);
}

// ============ Topology ============
bool ESPNowManager::topoSetRelayNext(uint8_t relayIdx, const uint8_t nextMac[6], uint32_t nextIPv4) {
  PeerRec* pr = nullptr; if(!ensurePeer(ModuleType::RELAY, relayIdx, pr)) return false;
  TopoNextHop t{}; t.myIdx = relayIdx; memcpy(t.nextMac, nextMac, 6); t.nextIPv4 = nextIPv4;
  _relayTopo[relayIdx].used = true;
  memcpy(_relayTopo[relayIdx].nextMac, nextMac, 6);
  _relayTopo[relayIdx].nextIPv4 = nextIPv4;
  // classic short message (backward compat)
  bool ok = enqueueToPeer(pr, CmdDomain::TOPO, TOPO_SET_NEXT, (uint8_t*)&t, sizeof(t), true);
  return ok;
}
bool ESPNowManager::topoSetSensorDependency(uint8_t sensIdx, uint8_t targetRelayIdx, const uint8_t targetMac[6], uint32_t targetIPv4) {
  PeerRec* pr = nullptr; if(!ensurePeer(ModuleType::PRESENCE, sensIdx, pr)) return false;
  TopoDependency d{}; d.sensIdx = sensIdx; d.targetType = 1; d.targetIdx = targetRelayIdx;
  memcpy(d.targetMac, targetMac, 6); d.targetIPv4 = targetIPv4;

  SensorDep* dep = nullptr;
  if (sensIdx==PRES_IDX_ENTRANCE) dep=&_entrTopo;
  else if (sensIdx==PRES_IDX_PARKING) dep=&_parkTopo;
  else if (sensIdx<ICM_MAX_SENSORS) dep=&_sensorTopo[sensIdx];
  if (dep){ dep->used=true; dep->targetType=1; dep->targetIdx=targetRelayIdx; memcpy(dep->targetMac,targetMac,6); dep->targetIPv4=targetIPv4; }

  // also update reverse link (previous sensor for that relay)
  topoSetRelayPrevFromSensor(targetRelayIdx, sensIdx, pr->mac);

  return enqueueToPeer(pr, CmdDomain::TOPO, TOPO_SET_DEP, (uint8_t*)&d, sizeof(d), true);
}
bool ESPNowManager::topoSetRelayPrevFromSensor(uint8_t relayIdx, uint8_t sensIdx, const uint8_t sensMac[6]) {
  if (relayIdx >= ICM_MAX_RELAYS) return false;
  _relayTopo[relayIdx].used = true;
  _relayTopo[relayIdx].hasPrev = true;
  _relayTopo[relayIdx].prevSensIdx = sensIdx;
  memcpy(_relayTopo[relayIdx].prevSensMac, sensMac, 6);
  return true;
}
bool ESPNowManager::topoClearPeer(ModuleType t, uint8_t idx) {
  PeerRec* pr = nullptr; if(!ensurePeer(t, idx, pr)) return false;
  if (t==ModuleType::RELAY && idx<ICM_MAX_RELAYS) _relayTopo[idx]=RelayLink{};
  if (t==ModuleType::PRESENCE){
    if (idx==PRES_IDX_ENTRANCE) _entrTopo=SensorDep{};
    else if (idx==PRES_IDX_PARKING) _parkTopo=SensorDep{};
    else if (idx<ICM_MAX_SENSORS) _sensorTopo[idx]=SensorDep{};
  }
  return enqueueToPeer(pr, CmdDomain::TOPO, TOPO_CLEAR, nullptr, 0, true);
}

// links = array of objects:
//  { "type":"relay", "idx":N, "next_mac":"AA:BB:..", "next_ip": 3232235777 }
//  { "type":"sensor","idx":K, "target_idx":R, "target_mac":"...", "target_ip": ... }
//  { "type":"entrance","target_idx":R, "target_mac":"...", "target_ip":... }  (optional)
//  { "type":"parking", ... }  (optional)
bool ESPNowManager::configureTopology(const JsonVariantConst& links) {
  if (!links.is<JsonArrayConst>()) return false;
  bool ok=true;
  for (JsonObjectConst l : links.as<JsonArrayConst>()) {
    String t = l["type"] | "";
    t.toLowerCase();
    if (t=="relay") {
      uint8_t idx = l["idx"] | 0;
      String nmac = l["next_mac"] | "";
      uint8_t m[6]; if(!macStrToBytes(nmac,m)) { ok=false; continue; }
      uint32_t nip = l["next_ip"] | 0;
      ok &= topoSetRelayNext(idx,m,nip);
    } else if (t=="sensor" || t=="entrance" || t=="parking") {
      uint8_t sidx;
      if (t=="entrance") sidx = PRES_IDX_ENTRANCE;
      else if (t=="parking") sidx = PRES_IDX_PARKING;
      else sidx = (uint8_t)(l["idx"] | 0);
      uint8_t rIdx = l["target_idx"] | 0;
      String tmac = l["target_mac"] | "";
      uint8_t m[6]; if(!macStrToBytes(tmac,m)) { ok=false; continue; }
      uint32_t tip = l["target_ip"] | 0;
      ok &= topoSetSensorDependency(sidx, rIdx, m, tip);
    }
  }
  rebuildReverseLinks();
  saveTopologyToNvs();
  ok &= topoPushAll(); // NEW: live push of tokens/IPs to every peer
  LOGI(1580,"Topology configured + pushed");
  return ok;
}

void ESPNowManager::rebuildReverseLinks(){
  // Clear all prev info, then rebuild from deps
  for (size_t i=0;i<ICM_MAX_RELAYS;i++){ _relayTopo[i].hasPrev=false; _relayTopo[i].prevSensIdx=0xFF; memset(_relayTopo[i].prevSensMac,0,6); }
  auto setPrev=[&](uint8_t sensIdx, const SensorDep& dep){
    if (dep.used && dep.targetIdx<ICM_MAX_RELAYS) {
      // find sensor MAC
      const PeerRec* spr = (sensIdx==PRES_IDX_ENTRANCE)?&_entrance : (sensIdx==PRES_IDX_PARKING)?&_parking : &_sensors[sensIdx];
      if (spr->used) topoSetRelayPrevFromSensor(dep.targetIdx, sensIdx, spr->mac);
    }
  };
  for (uint8_t i=0;i<ICM_MAX_SENSORS;i++) setPrev(i,_sensorTopo[i]);
  if (_entrTopo.used) setPrev(PRES_IDX_ENTRANCE,_entrTopo);
  if (_parkTopo.used) setPrev(PRES_IDX_PARKING ,_parkTopo);
}

bool ESPNowManager::saveTopologyToNvs() const {
  String j = serializeTopology();

  // PutString is void → just call it
  _cfg->PutString(keyTopo().c_str(), j);  // NOTE: PutString expects a String&, not c_str()

  // Best-effort verify by reading back
  String back = _cfg->GetString(keyTopo().c_str(), "");
  return (back == j);
}

bool ESPNowManager::loadTopologyFromNvs() {
  String j = _cfg->GetString(keyTopo().c_str(), "");
  if (j.isEmpty()) return true;
  DynamicJsonDocument doc(4096);
  DeserializationError err = deserializeJson(doc, j);
  if (err) { LOGW(1142,"Failed to parse NVS topology"); return false; }
  JsonArrayConst links = doc["links"].as<JsonArrayConst>();
  return configureTopology(links);
}

// ============ Topology PUSH (tokens+IPs) ============
bool ESPNowManager::topoPushRelay(uint8_t relayIdx){
  if (relayIdx>=ICM_MAX_RELAYS || !_relays[relayIdx].used) return false;
  PeerRec* rpr = &_relays[relayIdx];

  TopoRelayBundle b{};
  b.myIdx = relayIdx;

  // prev sensor info (optional)
  b.hasPrev = _relayTopo[relayIdx].hasPrev ? 1 : 0;
  b.prevSensIdx = _relayTopo[relayIdx].prevSensIdx;
  memcpy(b.prevSensMac, _relayTopo[relayIdx].prevSensMac, 6);

  // find prev sensor token (for optional acks)
  if (b.hasPrev) {
    const PeerRec* spr = (b.prevSensIdx==PRES_IDX_ENTRANCE)?&_entrance :
                         (b.prevSensIdx==PRES_IDX_PARKING )?&_parking  :
                         &_sensors[b.prevSensIdx];
    if (spr && spr->used) memcpy(b.prevSensTok16, spr->token16, 16);
  }

  // next relay hop (if any)
  b.hasNext = _relayTopo[relayIdx].used && _relayTopo[relayIdx].nextIPv4!=0 ? 1 : 0;
  // determine next relay index by MAC match
  uint8_t nextIdx = 0xFF;
  if (b.hasNext) {
    memcpy(b.nextMac, _relayTopo[relayIdx].nextMac, 6);
    b.nextIPv4 = _relayTopo[relayIdx].nextIPv4;
    for (uint8_t i=0;i<ICM_MAX_RELAYS;i++){
      if (_relays[i].used && memcmp(_relays[i].mac, b.nextMac, 6)==0) { nextIdx=i; break; }
    }
    b.nextIdx = nextIdx;
    if (nextIdx!=0xFF) memcpy(b.nextTok16, _relays[nextIdx].token16, 16); // token expected by NEXT relay
  }

  return enqueueToPeer(rpr, CmdDomain::TOPO, TOPO_PUSH, (uint8_t*)&b, sizeof(b), true);
}

bool ESPNowManager::topoPushSensor(uint8_t sensIdx){
  PeerRec* spr = nullptr;
  if (sensIdx==PRES_IDX_ENTRANCE) spr = &_entrance;
  else if (sensIdx==PRES_IDX_PARKING) spr = &_parking;
  else {
    if (sensIdx>=ICM_MAX_SENSORS || !_sensors[sensIdx].used) return false;
    spr = &_sensors[sensIdx];
  }
  const SensorDep* dep = (sensIdx==PRES_IDX_ENTRANCE)?&_entrTopo : (sensIdx==PRES_IDX_PARKING)?&_parkTopo : &_sensorTopo[sensIdx];
  if (!dep->used) return false;

  // find relay token to give to sensor (sensor must use relay's token in header)
  uint8_t ridx = dep->targetIdx;
  if (ridx>=ICM_MAX_RELAYS || !_relays[ridx].used) return false;

  TopoSensorBundle b{};
  b.sensIdx = sensIdx;
  b.targetIdx = ridx;
  memcpy(b.targetMac, dep->targetMac, 6);
  memcpy(b.targetTok16, _relays[ridx].token16, 16);
  b.targetIPv4 = dep->targetIPv4;

  return enqueueToPeer(spr, CmdDomain::TOPO, TOPO_PUSH, (uint8_t*)&b, sizeof(b), true);
}

bool ESPNowManager::topoPushAll(){
  bool ok = true;
  for (uint8_t i=0;i<ICM_MAX_RELAYS;i++)  if (_relays[i].used) ok &= topoPushRelay(i);
  for (uint8_t i=0;i<ICM_MAX_SENSORS;i++) if (_sensors[i].used && _sensorTopo[i].used) ok &= topoPushSensor(i);
  if (_entrance.used && _entrTopo.used) ok &= topoPushSensor(PRES_IDX_ENTRANCE);
  if (_parking .used && _parkTopo.used) ok &= topoPushSensor(PRES_IDX_PARKING);
  return ok;
}

// ============ Sequence ============
bool ESPNowManager::sequenceStart(SeqDir dir) {
  SeqStartPayload p{ (uint8_t)dir, {0,0,0} };
  bool ok = true;
  // Broadcast to sensors first (they’ll forward in AUTO)
  if (_entrance.used) ok &= enqueueToPeer(&_entrance, CmdDomain::SEQ, SEQ_START, (uint8_t*)&p, sizeof(p), true);
  if (_parking .used) ok &= enqueueToPeer(&_parking , CmdDomain::SEQ, SEQ_START, (uint8_t*)&p, sizeof(p), true);
  for (size_t i=0;i<ICM_MAX_SENSORS;i++) if (_sensors[i].used) ok &= enqueueToPeer(&_sensors[i], CmdDomain::SEQ, SEQ_START, (uint8_t*)&p, sizeof(p), true);
  // Also tell relays
  for (size_t i=0;i<ICM_MAX_RELAYS;i++) if (_relays[i].used) ok &= enqueueToPeer(&_relays[i], CmdDomain::SEQ, SEQ_START, (uint8_t*)&p, sizeof(p), true);
  LOGI(1600,"SEQ_START dir=%u", (unsigned)dir);
  return ok;
}
bool ESPNowManager::sequenceStop() {
  bool ok = true;
  if (_entrance.used) ok &= enqueueToPeer(&_entrance, CmdDomain::SEQ, SEQ_STOP, nullptr, 0, true);
  if (_parking .used) ok &= enqueueToPeer(&_parking , CmdDomain::SEQ, SEQ_STOP, nullptr, 0, true);
  for (size_t i=0;i<ICM_MAX_SENSORS;i++) if (_sensors[i].used) ok &= enqueueToPeer(&_sensors[i], CmdDomain::SEQ, SEQ_STOP, nullptr, 0, true);
  for (size_t i=0;i<ICM_MAX_RELAYS;i++)  if (_relays[i].used) ok &= enqueueToPeer(&_relays[i], CmdDomain::SEQ, SEQ_STOP,  nullptr, 0, true);
  LOGI(1601,"SEQ_STOP");
  return ok;
}
bool ESPNowManager::startSequence(const String& anchor, bool up) {
  (void)anchor; // advisory
  return sequenceStart(up ? SeqDir::UP : SeqDir::DOWN);
}

// ============ Status ============
bool ESPNowManager::isPeerOnline(ModuleType t, uint8_t index) const {
  const PeerRec* pr = nullptr;
  switch(t){
    case ModuleType::POWER: pr=&_power; break;
    case ModuleType::RELAY: if(index<ICM_MAX_RELAYS) pr=&_relays[index]; break;
    case ModuleType::PRESENCE:
      if (index==PRES_IDX_ENTRANCE) pr=&_entrance;
      else if(index==PRES_IDX_PARKING) pr=&_parking;
      else if(index<ICM_MAX_SENSORS) pr=&_sensors[index];
      break;
  }
  return (pr && pr->used && pr->online);
}
// ========= new request methods =========
bool ESPNowManager::powerGetTemperature() {
  // requires a paired POWER peer
  if (!_power.used) return false;
  return enqueueToPeer(&_power, CmdDomain::POWER, PWR_GET_TEMP, nullptr, 0, true);
}
bool ESPNowManager::relayGetTemperature(uint8_t idx) {
  PeerRec* pr = nullptr; if(!ensurePeer(ModuleType::RELAY, idx, pr)) return false;
  return enqueueToPeer(pr, CmdDomain::RELAY, REL_GET_TEMP, nullptr, 0, true);
}

// ========= helper to decode TempPayload =========
static inline bool decodeTempPayload(const uint8_t* p, int n, float& tCout) {
  if (n < (int)sizeof(TempPayload)) return false;
  const TempPayload* tp = (const TempPayload*)p;
  if (!tp->ok) return false;
  tCout = ((float)tp->tC_x100) / 100.0f;
  return true;
}
