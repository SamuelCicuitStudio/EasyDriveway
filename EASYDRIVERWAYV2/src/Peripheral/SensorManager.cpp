/**************************************************************
 *  Project : EasyDriveway
 *  File    : SensorManager.cpp
 **************************************************************/
#include "SensorManager.h"
#include "NVS/NVSConfig.h"
#if defined(NVS_ROLE_SENS) || defined(NVS_ROLE_SEMU)
static uint8_t readSemuCount(NvsManager* cfg){
#if defined(NVS_ROLE_SEMU)
  return (uint8_t)cfg->GetInt(NVS_KEY_SCOUNT,(int)NVS_DEF_SCOUNT);
#else
  (void)cfg; return 1;
#endif
}
bool SensorManager::begin(I2CBusHub* hub,bool useSYSforTF,uint16_t tfl_fps,bool tfl_cont,uint8_t muxAddr){
  _hub=hub; if(!_hub) return false;
  _tflW=useSYSforTF?&_hub->busSYS():&_hub->busENV();
  _envW=&_hub->busENV();
#if defined(NVS_ROLE_SEMU)
  if(!_tfl.begin(_hub,tfl_fps,tfl_cont,useSYSforTF,muxAddr)) return false;
#else
  if(!_tfl.begin(_hub,tfl_fps,tfl_cont,useSYSforTF)) return false;
#endif
  _als.setHub(_hub); _als.setWire(_envW);
  if(!_als.begin(VEML7700_I2C_ADDR)){ }
  _pairCount=readSemuCount(_cfg); if(!_isSEMU) _pairCount=1;
  return true;
}
bool SensorManager::begin(TwoWire* tflWire,TwoWire* envWire,uint16_t tfl_fps,bool tfl_cont,uint8_t muxAddr){
  _tflW=tflWire; _envW=envWire?envWire:tflWire; if(!_tflW) return false;
#if defined(NVS_ROLE_SEMU)
  if(!_tfl.begin(_tflW,tfl_fps,tfl_cont,muxAddr)) return false;
#else
  if(!_tfl.begin(_tflW,tfl_fps,tfl_cont)) return false;
#endif
  _als.setWire(_envW); if(!_als.begin(VEML7700_I2C_ADDR)){ }
  _pairCount=readSemuCount(_cfg); if(!_isSEMU) _pairCount=1;
  return true;
}
bool SensorManager::poll(Snapshot& out){
  out.pairs.clear();
  float lux;
  if(_als.read(lux)){ out.lux=lux; out.isDay=_als.computeDayNight(lux); }
  else{ out.lux=_als.lux(); out.isDay=_als.computeDayNight(isnan(out.lux)?(_als.lux()):out.lux); }
  if(!_isSEMU){
    PairReport pr{}; pr.index=0; if(!readCurrentPair(pr)) return false; pr.direction=inferDir(0,pr.presentA,pr.presentB); out.pairs.push_back(pr); return true;
  }
  for(uint8_t i=0;i<_pairCount&&i<MAX_PAIRS;++i){
    if(!selectPairIfNeeded(i)) continue;
    PairReport pr{}; pr.index=i; if(!readCurrentPair(pr)) continue; pr.direction=inferDir(i,pr.presentA,pr.presentB); out.pairs.push_back(pr);
  }
  return true;
}
bool SensorManager::pollPair(uint8_t idx,PairReport& outPr){
  if(_isSEMU){ if(idx>=_pairCount) return false; if(!selectPairIfNeeded(idx)) return false; }
  else{ if(idx!=0) return false; }
  if(!readCurrentPair(outPr)) return false; outPr.index=idx; outPr.direction=inferDir(idx,outPr.presentA,outPr.presentB); return true;
}
bool SensorManager::readALS(float& luxOut,uint8_t& isDayOut){
  if(!_als.read(luxOut)){ luxOut=_als.lux(); }
  isDayOut=_als.computeDayNight(isnan(luxOut)?0.f:luxOut); return true;
}
bool SensorManager::setTFLAddresses(uint8_t addrA,uint8_t addrB,int pairIndex){
#if defined(NVS_ROLE_SEMU)
  if(_isSEMU){ uint8_t idx=(pairIndex<0)?_tfl.currentPair():(uint8_t)pairIndex; if(idx>=_pairCount) return false; if(!selectPairIfNeeded(idx)) return false; }
  else{ if(pairIndex>=0&&pairIndex!=0) return false; }
#else
  (void)pairIndex;
#endif
  return _tfl.setAddresses(addrA,addrB);
}
bool SensorManager::setTFLFrameRate(uint16_t fps,int pairIndex){
#if defined(NVS_ROLE_SEMU)
  if(_isSEMU){ uint8_t idx=(pairIndex<0)?_tfl.currentPair():(uint8_t)pairIndex; if(idx>=_pairCount) return false; if(!selectPairIfNeeded(idx)) return false; }
  else{ if(pairIndex>=0&&pairIndex!=0) return false; }
#else
  (void)pairIndex;
#endif
  return _tfl.setFrameRate(fps);
}
uint8_t SensorManager::pairCount() const{
  return _pairCount;
}
SensorManager::Direction SensorManager::inferDir(uint8_t idx,bool nowA,bool nowB){
  Direction d=DIR_NONE; bool& lastA=_lastA[idx],& lastB=_lastB[idx];
  if(nowA&&nowB){ if(lastA&&!lastB) d=DIR_A_TO_B; if(!lastA&&lastB) d=DIR_B_TO_A; }
  lastA=nowA; lastB=nowB; return d;
}
bool SensorManager::readCurrentPair(PairReport& out){
  out.rate_hz=0;
  if(!_tfl.readBoth(out.A,out.B,out.rate_hz)){ out.presentA=out.presentB=false; out.direction=DIR_NONE; return false; }
  out.presentA=_tfl.isPresentA(out.A); out.presentB=_tfl.isPresentB(out.B); return true;
}
bool SensorManager::selectPairIfNeeded(uint8_t idx){
#if defined(NVS_ROLE_SEMU)
  if(!_isSEMU) return (idx==0); return _tfl.selectPair(idx);
#else
  (void)idx; return true;
#endif
}
#endif

