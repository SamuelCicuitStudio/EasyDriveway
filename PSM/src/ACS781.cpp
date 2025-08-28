/**************************************************************
 *  Project : EasyDriveWay - PSM
 *  File    : ACS781.cpp
 **************************************************************/
#include "ACS781.h"

#ifdef ARDUINO_ARCH_ESP32
  #include <esp32-hal-adc.h>   // for analogSetPinAttenuation / adc_attenuation_t
#endif

ACS781::ACS781(int adcPin, ConfigManager* cfg)
: _adcPin(adcPin), _cfg(cfg) {}

bool ACS781::begin() {
  pinMode(_adcPin, INPUT);
  applyAttenuation();
  reloadFromConfig();
  return true;
}

void ACS781::reloadFromConfig() {
  if (!_cfg) return;
  // Model is informational (not used in math)
  (void)_cfg->GetString(ACS_MODEL_KEY, "ACS781-100B");
  _vref_mV     = (uint16_t)_cfg->GetInt(ACS_VREF_MV_KEY,   3300);
  _zero_mV     = (uint16_t)_cfg->GetInt(ACS_ZERO_MV_KEY,   1650);
  _sens_uV_A   = (int32_t) _cfg->GetInt(ACS_SENS_UVPA_KEY, 13200);
  _avg         = (uint8_t)_cfg->GetInt(ACS_AVG_KEY,        16);
  _invert      =           _cfg->GetBool(ACS_INV_KEY,       false);
  _atten       = (uint8_t)_cfg->GetInt(ACS_ATTEN_KEY,      3);
  applyAttenuation();
}

uint16_t ACS781::readMilliVolts(uint8_t samples) const {
  uint32_t acc = 0;
  uint8_t n = samples ? samples : _avg;
  if (n == 0) n = 1;
  for (uint8_t i=0; i<n; ++i) {
    acc += (uint32_t)readMilliVoltsOnce(_adcPin);
  }
  return (uint16_t)(acc / n);
}

float ACS781::readAmps(uint8_t samples) const {
  const int32_t mv = (int32_t)readMilliVolts(samples);
  int32_t delta_mV = mv - (int32_t)_zero_mV;
  if (_invert) delta_mV = -delta_mV;
  // I[A] = (delta_mV * 1000) / sens_uV_A
  return (float)((int64_t)delta_mV * 1000LL) / (float)_sens_uV_A;
}

uint16_t ACS781::calibrateZeroAtRest(uint16_t settle_ms, uint8_t samples, bool persist) {
  delay(settle_ms);
  uint16_t z = readMilliVolts(samples ? samples : _avg);
  setZero_mV(z, persist);
  return z;
}

void ACS781::setZero_mV(uint16_t mv, bool persist) {
  _zero_mV = mv;
  if (persist && _cfg) _cfg->PutInt(ACS_ZERO_MV_KEY, (int)mv);
}

void ACS781::setSensitivity_uV_A(int32_t uv_per_A, bool persist) {
  _sens_uV_A = uv_per_A;
  if (persist && _cfg) _cfg->PutInt(ACS_SENS_UVPA_KEY, (int)uv_per_A);
}

void ACS781::setVref_mV(uint16_t mv, bool persist) {
  _vref_mV = mv;
  if (persist && _cfg) _cfg->PutInt(ACS_VREF_MV_KEY, (int)mv);
}

void ACS781::setAvg(uint8_t n, bool persist) {
  _avg = n ? n : 1;
  if (persist && _cfg) _cfg->PutInt(ACS_AVG_KEY, (int)_avg);
}

void ACS781::setInvert(bool inv, bool persist) {
  _invert = inv;
  if (persist && _cfg) _cfg->PutBool(ACS_INV_KEY, _invert);
}

void ACS781::setAttenuation(uint8_t att, bool persist) {
  _atten = (att > 3) ? 3 : att;
  if (persist && _cfg) _cfg->PutInt(ACS_ATTEN_KEY, (int)_atten);
  applyAttenuation();
}

uint16_t ACS781::readMilliVoltsOnce(int pin) {
  // Arduino ESP32 core provides millivolt conversion calibrated to attenuation
  return (uint16_t)analogReadMilliVolts(pin);
}

void ACS781::applyAttenuation() const {
#ifdef ARDUINO_ARCH_ESP32
  analogSetPinAttenuation(_adcPin, (adc_attenuation_t)_atten);
#else
  (void)_atten; // other MCUs: no-op
#endif
}
