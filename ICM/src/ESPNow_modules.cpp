/**************************************************************
 *  ESPNow_modules.cpp — Module commands: power/relay/sensor, sequences
 **************************************************************/
#include "ESPNowManager.h"
#include "CommandAPI.h"

// ======= Power helpers =======
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
bool ESPNowManager::powerGetTemperature() {
  if (!_power.used) return false;
  return enqueueToPeer(&_power, CmdDomain::POWER, PWR_GET_TEMP, nullptr, 0, true);
}

// ======= Relays =======
bool ESPNowManager::relayGetStatus(uint8_t idx){ return enqueueToPeer(&_relays[idx],CmdDomain::RELAY,REL_GET,nullptr,0,true); }
bool ESPNowManager::relaySet(uint8_t idx, uint8_t ch, bool on){
  PeerRec* pr=nullptr; if(!ensurePeer(ModuleType::RELAY,idx,pr)) return false;
  uint8_t b[2]={ch,(uint8_t)(on?1:0)}; return enqueueToPeer(pr,CmdDomain::RELAY,REL_SET_CH,b,2,true);
}
bool ESPNowManager::relaySetMode(uint8_t idx, uint8_t ch, uint8_t mode){
  PeerRec* pr=nullptr; if(!ensurePeer(ModuleType::RELAY,idx,pr)) return false;
  uint8_t b[2]={ch,mode}; return enqueueToPeer(pr,CmdDomain::RELAY,REL_SET_MODE,b,2,true);
}
bool ESPNowManager::relayGetTemperature(uint8_t idx) {
  PeerRec* pr = nullptr; if(!ensurePeer(ModuleType::RELAY, idx, pr)) return false;
  return enqueueToPeer(pr, CmdDomain::RELAY, REL_GET_TEMP, nullptr, 0, true);
}

// ======= Sensors =======
bool ESPNowManager::presenceGetStatus(uint8_t idx){ return enqueueToPeer(&_sensors[idx],CmdDomain::SENS,SENS_GET,nullptr,0,true); }
bool ESPNowManager::presenceGetDayNight(uint8_t idx){ return enqueueToPeer(&_sensors[idx],CmdDomain::SENS,SENS_GET_DAYNIGHT,nullptr,0,true); }
bool ESPNowManager::presenceSetMode(uint8_t idx, uint8_t mode){
  PeerRec* pr=nullptr; if(!ensurePeer(ModuleType::PRESENCE,idx,pr)) return false;
  uint8_t b[1]={mode}; return enqueueToPeer(pr,CmdDomain::SENS,SENS_SET_MODE,b,1,true);
}
bool ESPNowManager::presenceGetDayNightByMac(const String& macStr){
  if (macStr.length() < 17) return false;

  // Specials first
  if (_entrance.used && macBytesToStr(_entrance.mac).equalsIgnoreCase(macStr)) {
    return enqueueToPeer(&_entrance, CmdDomain::SENS, SENS_GET_DAYNIGHT, nullptr, 0, true);
  }
  if (_parking.used && macBytesToStr(_parking.mac).equalsIgnoreCase(macStr)) {
    return enqueueToPeer(&_parking, CmdDomain::SENS, SENS_GET_DAYNIGHT, nullptr, 0, true);
  }

  // Middles
  for (size_t i=0; i<ICM_MAX_SENSORS; ++i){
    if (_sensors[i].used && macBytesToStr(_sensors[i].mac).equalsIgnoreCase(macStr)) {
      return presenceGetDayNight((uint8_t)i);
    }
  }
  return false;
}
int8_t ESPNowManager::lastDayFlagByMac(const String& macStr, uint32_t* outMs) const {
  if (outMs) *outMs = 0;
  if (macStr.length() < 17) return -1;

  if (_entrance.used && macBytesToStr(_entrance.mac).equalsIgnoreCase(macStr)) {
    if (outMs) *outMs = _entrDNMs; return _entrDNFlag;
  }
  if (_parking.used && macBytesToStr(_parking.mac).equalsIgnoreCase(macStr)) {
    if (outMs) *outMs = _parkDNMs; return _parkDNFlag;
  }
  for (size_t i=0; i<ICM_MAX_SENSORS; ++i){
    if (_sensors[i].used && macBytesToStr(_sensors[i].mac).equalsIgnoreCase(macStr)) {
      if (outMs) *outMs = _sensDNMs[i]; return _sensDayNight[i];
    }
  }
  return -1;
}

// ======= Manual helpers (via MAC) =======
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

// ======= Sequences =======
bool ESPNowManager::sequenceStart(SeqDir dir) {
  SeqStartPayload p{ (uint8_t)dir, {0,0,0} };
  bool ok = true;
  // Sensors first (they own timing/animation now)
  if (_entrance.used) ok &= enqueueToPeer(&_entrance, CmdDomain::SEQ, SEQ_START, (uint8_t*)&p, sizeof(p), true);
  if (_parking .used) ok &= enqueueToPeer(&_parking , CmdDomain::SEQ, SEQ_START, (uint8_t*)&p, sizeof(p), true);
  for (size_t i=0;i<ICM_MAX_SENSORS;i++) if (_sensors[i].used) ok &= enqueueToPeer(&_sensors[i], CmdDomain::SEQ, SEQ_START, (uint8_t*)&p, sizeof(p), true);
  // Also tell relays (optional)
  for (size_t i=0;i<ICM_MAX_RELAYS;i++) if (_relays[i].used) ok &= enqueueToPeer(&_relays[i], CmdDomain::SEQ, SEQ_START, (uint8_t*)&p, sizeof(p), true);
  return ok;
}
bool ESPNowManager::sequenceStop() {
  bool ok = true;
  if (_entrance.used) ok &= enqueueToPeer(&_entrance, CmdDomain::SEQ, SEQ_STOP, nullptr, 0, true);
  if (_parking .used) ok &= enqueueToPeer(&_parking , CmdDomain::SEQ, SEQ_STOP, nullptr, 0, true);
  for (size_t i=0;i<ICM_MAX_SENSORS;i++) if (_sensors[i].used) ok &= enqueueToPeer(&_sensors[i], CmdDomain::SEQ, SEQ_STOP, nullptr, 0, true);
  for (size_t i=0;i<ICM_MAX_RELAYS;i++)  if (_relays[i].used) ok &= enqueueToPeer(&_relays[i], CmdDomain::SEQ, SEQ_STOP,  nullptr, 0, true);
  return ok;
}
bool ESPNowManager::startSequence(const String& anchor, bool up) {
  (void)anchor; // advisory
  return sequenceStart(up ? SeqDir::UP : SeqDir::DOWN);
}

bool ESPNowManager::presenceGetTfRaw(uint8_t idx) {
  PeerRec* pr=nullptr; if(!ensurePeer(ModuleType::PRESENCE, idx, pr)) return false;
  return enqueueToPeer(pr, CmdDomain::SENS, SENS_GET_TFRAW, nullptr, 0, true);
}
bool ESPNowManager::presenceGetEnv(uint8_t idx) {
  PeerRec* pr=nullptr; if(!ensurePeer(ModuleType::PRESENCE, idx, pr)) return false;
  return enqueueToPeer(pr, CmdDomain::SENS, SENS_GET_ENV, nullptr, 0, true);
}
