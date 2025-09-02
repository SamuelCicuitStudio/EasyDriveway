#include "TFLunaManager.h"

// Utility: clamp helper
template<typename T>
static inline T clamp_(T v, T lo, T hi){ return (v<lo)?lo:((v>hi)?hi:v); }

void TFLunaManager::loadConfig_(){
  // Pins
  sda_ = (uint8_t)cfg_->GetInt(I2C1_SDA_PIN_KEY, I2C1_SDA_PIN_DEFAULT);
  scl_ = (uint8_t)cfg_->GetInt(I2C1_SCL_PIN_KEY, I2C1_SCL_PIN_DEFAULT);
  // Addresses
  addrA_ = (uint8_t)cfg_->GetInt(TFL_A_ADDR_KEY, TFL_A_ADDR_DEFAULT);
  addrB_ = (uint8_t)cfg_->GetInt(TFL_B_ADDR_KEY, TFL_B_ADDR_DEFAULT);
  // Presence gates
  near_mm_ = (uint16_t)cfg_->GetInt(TF_NEAR_MM_KEY, TF_NEAR_MM_DEFAULT);
  far_mm_  = (uint16_t)cfg_->GetInt(TF_FAR_MM_KEY,  TF_FAR_MM_DEFAULT);
  if (near_mm_ > far_mm_) { uint16_t t=near_mm_; near_mm_=far_mm_; far_mm_=t; }
}

void TFLunaManager::saveAddresses_(){
  cfg_->PutInt(TFL_A_ADDR_KEY, addrA_);
  cfg_->PutInt(TFL_B_ADDR_KEY, addrB_);
}

bool TFLunaManager::begin(uint16_t fps_hz, bool continuous){
  if (!cfg_ || !wire_) return false;
  loadConfig_();

  // Start the selected I2C instance on requested pins
  // NOTE: TFLI2C uses the global Wire. Ensure the instance passed is &Wire.
  wire_->begin((int)sda_, (int)scl_);
  wire_->setClock(400000); // fast mode

  // Optionally set FPS and continuous/trigger mode on both sensors
  bool ok = true;
  if (continuous) {
    ok &= tfl_.Set_Cont_Mode(addrA_);
    ok &= tfl_.Set_Cont_Mode(addrB_);
  } else {
    ok &= tfl_.Set_Trig_Mode(addrA_);
    ok &= tfl_.Set_Trig_Mode(addrB_);
  }

  if (fps_hz > 0) {
    uint16_t fps = clamp_<uint16_t>(fps_hz, 1, 250);
    ok &= tfl_.Set_Frame_Rate(fps, addrA_);
    ok &= tfl_.Set_Frame_Rate(fps, addrB_);
    ok &= tfl_.Save_Settings(addrA_);
    ok &= tfl_.Save_Settings(addrB_);
  }

  // Enable devices (exit low-power), then soft reset to apply persisted config
  ok &= tfl_.Set_Enable(addrA_);
  ok &= tfl_.Set_Enable(addrB_);
  // Soft reset is optional; leave commented if you don't want a reboot here.
  // ok &= tfl_.Soft_Reset(addrA_);
  // ok &= tfl_.Soft_Reset(addrB_);

  return ok;
}

bool TFLunaManager::setEnable(bool en){
  bool ok=true;
  if (en){ ok &= tfl_.Set_Enable(addrA_); ok &= tfl_.Set_Enable(addrB_); }
  else   { ok &= tfl_.Set_Disable(addrA_); ok &= tfl_.Set_Disable(addrB_); }
  return ok;
}

bool TFLunaManager::setAddresses(uint8_t addrA, uint8_t addrB){
  bool ok=true;
  // Change A (command must be sent to the OLD address)
  if (addrA != addrA_) {
    ok &= tfl_.Set_I2C_Addr(addrA, addrA_);
    if (ok) addrA_ = addrA;
    ok &= tfl_.Save_Settings(addrA_);
    ok &= tfl_.Soft_Reset(addrA_);
  }
  // Change B
  if (addrB != addrB_) {
    ok &= tfl_.Set_I2C_Addr(addrB, addrB_);
    if (ok) addrB_ = addrB;
    ok &= tfl_.Save_Settings(addrB_);
    ok &= tfl_.Soft_Reset(addrB_);
  }
  if (ok) saveAddresses_();
  return ok;
}

bool TFLunaManager::setFrameRate(uint16_t fps_hz){
  uint16_t fps = clamp_<uint16_t>(fps_hz, 1, 250);
  bool ok = tfl_.Set_Frame_Rate(fps, addrA_);
  ok &= tfl_.Set_Frame_Rate(fps, addrB_);
  ok &= tfl_.Save_Settings(addrA_);
  ok &= tfl_.Save_Settings(addrB_);
  return ok;
}

bool TFLunaManager::readOne(uint8_t addr, Sample& s){
  int16_t d_cm=0, flux=0, t_cx100=0;
  if (!tfl_.getData(d_cm, flux, t_cx100, addr)) {
    s = {0, (uint16_t)flux, (int16_t)t_cx100, false};
    return false;
  }
  // Convert: cm -> mm; temp already in centi-C from library comment
  s.dist_mm = (uint16_t)((d_cm < 0) ? 0 : (d_cm * 10));
  s.amp     = (uint16_t)((flux < 0) ? 0 : flux);
  s.temp_c_x100 = (int16_t)t_cx100;
  s.ok = true;
  return true;
}

bool TFLunaManager::readBoth(Sample& a, Sample& b, uint16_t& rate_hz_out){
  bool okA = readOne(addrA_, a);
  bool okB = readOne(addrB_, b);

  // Rough rate estimate: try to fetch current FPS register (optional)
  uint16_t fpsA=0, fpsB=0; rate_hz_out = 0;
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

bool TFLunaManager::fetch(uint8_t which, Sample& A, Sample& B, uint16_t& rate_hz_out){
  (void)which; // currently unused (we read both)
  return readBoth(A, B, rate_hz_out);
}
