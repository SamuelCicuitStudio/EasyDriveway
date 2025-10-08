/**************************************************************
 *  Project : EasyDriveway
 *  File    : RelayManager.cpp
 **************************************************************/
#include "RelayManager.h"
RelayManager::RelayManager(NvsManager* cfg, LogFS* log):_cfg(cfg),_log(log),_sr(log){}
bool RelayManager::begin(){
#if defined(NVS_ROLE_REMU)
  _useSR = _sr.beginAuto(); _count = _useSR ? REL_CH_COUNT : 0;
#elif defined(NVS_ROLE_RELAY)
  #if defined(SR_SER_PIN) && defined(SR_SCK_PIN) && defined(SR_RCK_PIN)
    _useSR = _sr.beginAuto(); _count = _useSR ? 8 : 0;
  #else
    _useSR = false; _count = 2;
    pinMode(RELAY1_OUT_PIN,OUTPUT);pinMode(RELAY2_OUT_PIN,OUTPUT);
    digitalWrite(RELAY1_OUT_PIN,LOW);digitalWrite(RELAY2_OUT_PIN,LOW);
  #endif
#elif defined(NVS_ROLE_PMS)
  #if defined(SR_SER_PIN) && defined(SR_SCK_PIN) && defined(SR_RCK_PIN)
    _useSR = _sr.beginAuto(); _count = _useSR ? 8 : 0;
  #else
    _useSR = false;
    #if defined(REL_SRC_WALL_PIN) && defined(REL_SRC_BATT_PIN)
      _count = 2;
      pinMode(REL_SRC_WALL_PIN,OUTPUT);pinMode(REL_SRC_BATT_PIN,OUTPUT);
      digitalWrite(REL_SRC_WALL_PIN,LOW);digitalWrite(REL_SRC_BATT_PIN,LOW);
    #else
      _count = 0;
    #endif
  #endif
#else
  _useSR=false;_count=0;
#endif
  _shadow=0;
  if(_useSR){_sr.setEnabled(true);_sr.resetMapping();}
  if(_log) _log->event(LogFS::DOM_REL,LogFS::EV_INFO,2200,String("RelayManager begin; useSR=")+(_useSR?"1":"0")+String(" count=")+String(_count),"RelayManager");
  return _count>0;
}
void RelayManager::enableLedFeedback(uint32_t onColor,uint32_t offColor,uint16_t blinkMs){
  _ledEnabled=true;_ledOnColor=onColor;_ledOffColor=offColor;_ledBlinkMs=blinkMs?blinkMs:120;
  if(_log) _log->event(LogFS::DOM_REL,LogFS::EV_INFO,2201,"LED feedback enabled","RelayManager");
}
bool RelayManager::assignRelayToOutput(uint16_t logical,uint16_t physical){
  if(!_useSR) return false;
  return _sr.assignLogicalToPhysical(logical,physical);
}
void RelayManager::resetMapping(){
  if(_useSR){_sr.resetMapping();}
}
void RelayManager::set(uint16_t idx,bool on){
  if(idx>=_count) return;
  if(_useSR){applySR(idx,on);}else{applyGPIO(idx,on);}
  if(on){_shadow|=(1UL<<idx);}else{_shadow&=~(1UL<<idx);}
  logRelay(idx,on);onFeedback(on);
}
void RelayManager::toggle(uint16_t idx){
  if(idx>=_count) return;
  set(idx,!get(idx));
}
bool RelayManager::get(uint16_t idx) const{
  if(idx>=_count) return false;
  return (_shadow>>idx)&1UL;
}
void RelayManager::writeMask(uint32_t mask){
  uint32_t keep=(_count>=32)?0xFFFFFFFFUL:((1UL<<_count)-1UL);
  _shadow=mask&keep;
  if(_useSR){
    _sr.writeMask(_shadow);
  }else{
#if defined(NVS_ROLE_RELAY)
    if(_count>=1) digitalWrite(RELAY1_OUT_PIN,(_shadow&0x1)?HIGH:LOW);
    if(_count>=2) digitalWrite(RELAY2_OUT_PIN,(_shadow&0x2)?HIGH:LOW);
#elif defined(NVS_ROLE_PMS)
  #if defined(REL_SRC_WALL_PIN) && defined(REL_SRC_BATT_PIN)
    if(_count>=1) digitalWrite(REL_SRC_WALL_PIN,(_shadow&0x1)?HIGH:LOW);
    if(_count>=2) digitalWrite(REL_SRC_BATT_PIN,(_shadow&0x2)?HIGH:LOW);
  #endif
#endif
  }
  if(_log) _log->event(LogFS::DOM_REL,LogFS::EV_INFO,2204,String("Mask=")+String(_shadow,16),"RelayManager");
}
void RelayManager::applySR(uint16_t idx,bool on){
  _sr.writeLogical(idx,on);
}
void RelayManager::applyGPIO(uint16_t idx,bool on){
#if defined(NVS_ROLE_RELAY)
  if(idx==0) digitalWrite(RELAY1_OUT_PIN,on?HIGH:LOW);
  else if(idx==1) digitalWrite(RELAY2_OUT_PIN,on?HIGH:LOW);
#elif defined(NVS_ROLE_PMS)
  #if defined(REL_SRC_WALL_PIN) && defined(REL_SRC_BATT_PIN)
    if(idx==0) digitalWrite(REL_SRC_WALL_PIN,on?HIGH:LOW);
    else if(idx==1) digitalWrite(REL_SRC_BATT_PIN,on?HIGH:LOW);
  #endif
#endif
}
void RelayManager::onFeedback(bool turnedOn){
  if(_ledEnabled && _led){
    _led->startBlink(turnedOn?_ledOnColor:_ledOffColor,_ledBlinkMs);
    vTaskDelay(pdMS_TO_TICKS(_ledBlinkMs));
    _led->stop();
  }
  if(_buzOnChange && _buz){
    _buz->bip(turnedOn?1400:900,60);
  }
}
void RelayManager::logRelay(uint16_t idx,bool on){
  if(!_log) return;
  _log->event(LogFS::DOM_REL,LogFS::EV_INFO,2202,String("CH ")+String(idx)+(on?" ON":" OFF"),"RelayManager");
}

