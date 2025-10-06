/**************************************************************
 *  Project : EasyDriveway
 *  File    : TFLunaManager.cpp
 **************************************************************/

#include "TFLunaManager.h"

#if defined(NVS_ROLE_SENS) || defined(NVS_ROLE_SEMU)
template<typename T> static inline T clamp_(T v, T lo, T hi){ return (v<lo)?lo:((v>hi)?hi:v); }
#if defined(NVS_ROLE_SENS)
void TFLunaManager::loadConfig_(){
  addrA_   = (uint8_t) cfg_->GetInt(TFL_A_ADDR_KEY, TFL_ADDR_A);
  addrB_   = (uint8_t) cfg_->GetInt(TFL_B_ADDR_KEY, TFL_ADDR_B);
  near_mm_ = (uint16_t)cfg_->GetInt(TF_NEAR_MM_KEY, TF_NEAR_MM_DEFAULT);
  far_mm_  = (uint16_t)cfg_->GetInt(TF_FAR_MM_KEY,  TF_FAR_MM_DEFAULT);
  if (near_mm_ > far_mm_) { uint16_t t = near_mm_; near_mm_ = far_mm_; far_mm_ = t; }
}
void TFLunaManager::saveAddresses_(){
  cfg_->PutInt(TFL_A_ADDR_KEY, addrA_);
  cfg_->PutInt(TFL_B_ADDR_KEY, addrB_);
}
#endif
#if defined(NVS_ROLE_SEMU)
void TFLunaManager::loadConfigPair_(uint8_t idx){
  char key[12];
  snprintf(key, sizeof(key), "%s%d", TF_NEAR_MM_KEY_PFX, (int)idx);
  near_mm_ = (uint16_t)cfg_->GetInt(key, TF_NEAR_MM_DEFAULT);
  snprintf(key, sizeof(key), "%s%d", TF_FAR_MM_KEY_PFX, (int)idx);
  far_mm_  = (uint16_t)cfg_->GetInt(key, TF_FAR_MM_DEFAULT);
  if (near_mm_ > far_mm_) { uint16_t t = near_mm_; near_mm_ = far_mm_; far_mm_ = t; }
  snprintf(key, sizeof(key), "%s%d", TFL_A_ADDR_KEY_PFX, (int)idx);
  addrA_ = (uint8_t)cfg_->GetInt(key, TFL_ADDR_A_DEF);
  snprintf(key, sizeof(key), "%s%d", TFL_B_ADDR_KEY_PFX, (int)idx);
  addrB_ = (uint8_t)cfg_->GetInt(key, TFL_ADDR_B_DEF);
}
void TFLunaManager::saveAddressesPair_(uint8_t idx){
  char key[12];
  snprintf(key, sizeof(key), "%s%d", TFL_A_ADDR_KEY_PFX, (int)idx);
  cfg_->PutInt(key, addrA_);
  snprintf(key, sizeof(key), "%s%d", TFL_B_ADDR_KEY_PFX, (int)idx);
  cfg_->PutInt(key, addrB_);
}
bool TFLunaManager::ensureMuxOnPair_(uint8_t idx){
  if (!muxInited_) return false;
  if (idx > 7)     return false;
  return mux_.select(idx);
}
#endif
bool TFLunaManager::begin(I2CBusHub* hub, uint16_t fps_hz, bool continuous, bool useSYSbus, uint8_t muxAddr){
  hub_  = hub;
  wire_ = nullptr;
  if (!cfg_ || !hub_) return false;
  TwoWire* chosen = useSYSbus ? &hub_->busSYS() : &hub_->busENV();
  wire_ = chosen;
  return beginWithWire_(fps_hz, continuous, muxAddr);
}
bool TFLunaManager::begin(TwoWire* wire, uint16_t fps_hz, bool continuous, uint8_t muxAddr){
  hub_  = nullptr;
  wire_ = wire;
  if (!cfg_ || !wire_) return false;
  return beginWithWire_(fps_hz, continuous, muxAddr);
}
bool TFLunaManager::beginWithWire_(uint16_t fps_hz, bool continuous, uint8_t muxAddr){
#if defined(NVS_ROLE_SEMU)
  muxAddr_   = muxAddr;
  muxInited_ = mux_.begin(*wire_, muxAddr_, true);
  if (!muxInited_) return false;
  curPair_ = 0;
  if (!ensureMuxOnPair_(curPair_)) return false;
  loadConfigPair_(curPair_);
#elif defined(NVS_ROLE_SENS)
  loadConfig_();
#endif
  bool ok = true;
  if (continuous) { ok &= tfl_.Set_Cont_Mode(addrA_); ok &= tfl_.Set_Cont_Mode(addrB_); }
  else            { ok &= tfl_.Set_Trig_Mode(addrA_); ok &= tfl_.Set_Trig_Mode(addrB_); }
  if (fps_hz > 0) {
    uint16_t fps = clamp_<uint16_t>(fps_hz, 1, 250);
    ok &= tfl_.Set_Frame_Rate(fps, addrA_);
    ok &= tfl_.Set_Frame_Rate(fps, addrB_);
  #if defined(NVS_ROLE_SEMU)
    char k[12]; snprintf(k, sizeof(k), "%s%d", TFL_FPS_KEY_PFX, (int)curPair_);
    cfg_->PutInt(k, (int)fps);
  #endif
    ok &= tfl_.Save_Settings(addrA_);
    ok &= tfl_.Save_Settings(addrB_);
  }
  ok &= tfl_.Set_Enable(addrA_);
  ok &= tfl_.Set_Enable(addrB_);
  return ok;
}
bool TFLunaManager::setEnable(bool en){
  bool ok = true;
  if (en) { ok &= tfl_.Set_Enable(addrA_); ok &= tfl_.Set_Enable(addrB_); }
  else    { ok &= tfl_.Set_Disable(addrA_); ok &= tfl_.Set_Disable(addrB_); }
  return ok;
}
bool TFLunaManager::setAddresses(uint8_t addrA, uint8_t addrB){
  bool ok = true;
  if (addrA != addrA_) { ok &= tfl_.Set_I2C_Addr(addrA, addrA_); if (ok) addrA_ = addrA; ok &= tfl_.Save_Settings(addrA_); ok &= tfl_.Soft_Reset(addrA_); }
  if (addrB != addrB_) { ok &= tfl_.Set_I2C_Addr(addrB, addrB_); if (ok) addrB_ = addrB; ok &= tfl_.Save_Settings(addrB_); ok &= tfl_.Soft_Reset(addrB_); }
#if defined(NVS_ROLE_SEMU)
  saveAddressesPair_(curPair_);
#elif defined(NVS_ROLE_SENS)
  saveAddresses_();
#endif
  return ok;
}
bool TFLunaManager::setFrameRate(uint16_t fps_hz){
  uint16_t fps = clamp_<uint16_t>(fps_hz, 1, 250);
  bool ok = tfl_.Set_Frame_Rate(fps, addrA_);
  ok &= tfl_.Set_Frame_Rate(fps, addrB_);
#if defined(NVS_ROLE_SEMU)
  char k[12]; snprintf(k, sizeof(k), "%s%d", TFL_FPS_KEY_PFX, (int)curPair_);
  cfg_->PutInt(k, (int)fps);
#endif
  ok &= tfl_.Save_Settings(addrA_);
  ok &= tfl_.Save_Settings(addrB_);
  return ok;
}
bool TFLunaManager::readOne(uint8_t addr, Sample& s){
#if defined(NVS_ROLE_SEMU)
  if (!ensureMuxOnPair_(curPair_)) { s = {0,0,0,false}; return false; }
#endif
  int16_t d_cm = 0, flux = 0, t_cx100 = 0;
  if (!tfl_.getData(d_cm, flux, t_cx100, addr)) { s = {0, (uint16_t)((flux<0)?0:flux), (int16_t)t_cx100, false}; return false; }
  s.dist_mm     = (uint16_t)((d_cm < 0) ? 0 : (d_cm * 10));
  s.amp         = (uint16_t)((flux < 0) ? 0 : flux);
  s.temp_c_x100 = (int16_t)t_cx100;
  s.ok          = true;
  return true;
}
bool TFLunaManager::readBoth(Sample& a, Sample& b, uint16_t& rate_hz_out){
#if defined(NVS_ROLE_SEMU)
  if (!ensureMuxOnPair_(curPair_)) { a.ok = b.ok = false; rate_hz_out = 0; return false; }
#endif
  bool okA = readOne(addrA_, a);
  bool okB = readOne(addrB_, b);
  uint16_t fpsA = 0, fpsB = 0; rate_hz_out = 0;
  if (tfl_.Get_Frame_Rate(fpsA, addrA_)) rate_hz_out = fpsA;
  if (tfl_.Get_Frame_Rate(fpsB, addrB_)) rate_hz_out = (rate_hz_out==0)?fpsB:((fpsA+fpsB)/2);
  return okA && okB;
}
bool TFLunaManager::isPresentA(const Sample& s) const {
  if (!s.ok) return false;
  return (s.dist_mm >= near_mm_ && s.dist_mm <= far_mm_);
}
bool TFLunaManager::isPresentB(const Sample& s) const {
  if (!s.ok) return false;
  return (s.dist_mm >= near_mm_ && s.dist_mm <= far_mm_);
}
bool TFLunaManager::fetch(uint8_t /*which*/, Sample& A, Sample& B, uint16_t& rate_hz_out){
  return readBoth(A, B, rate_hz_out);
}
#if defined(NVS_ROLE_SEMU)
bool TFLunaManager::selectPair(uint8_t pairIndex){
  if (pairIndex > 7) return false;
  if (!ensureMuxOnPair_(pairIndex)) return false;
  curPair_ = pairIndex;
  loadConfigPair_(curPair_);
  return true;
}
#endif
#endif
