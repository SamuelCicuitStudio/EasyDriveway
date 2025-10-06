/**************************************************************
 *  Project : EasyDriveway
 *  File    : VEML7700Manager.cpp
 **************************************************************/
#include "VEML7700Manager.h"

#if defined(NVS_ROLE_SENS) || defined(NVS_ROLE_SEMU)
bool VEML7700Manager::begin(uint8_t addr) {
  if (!_wire) {
    if (_bus) { _bus->bringUpENV(); _wire = &_bus->busENV(); }
    else { I2CBusHub::beginENV(); _wire = &I2CBusHub::env(); }
  }
  _i2cAddr = addr;
  loadThresholds();
  if (!_als.begin(_i2cAddr, *_wire)) { _initialized = false; return false; }
  _als.setIntegrationTime(VEML7700_INTEGRATION_100ms);
  _initialized = true;
  return true;
}
bool VEML7700Manager::read(float& lux) {
  if (!_initialized) return false;
  float lx = NAN;
  const int rc = _als.getLux(lx);
  if (rc != 0 || !isfinite(lx)) { return false; }
  _lastLux = lx;
  _lastReadMs = millis();
  lux = lx;
  return true;
}
uint8_t VEML7700Manager::computeDayNight(float luxNow) {
  if (_isDay) { if (luxNow <= _alsT0) _isDay = 0; }
  else { if (luxNow >= _alsT1) _isDay = 1; }
  return _isDay;
}
void VEML7700Manager::loadThresholds() {
  if (!_cfg) return;
  const int t0 = _cfg->GetInt("ALS_T0", _alsT0);
  const int t1 = _cfg->GetInt("ALS_T1", _alsT1);
  if (t0 > 0) _alsT0 = t0;
  if (t1 > 0) _alsT1 = t1;
}
#endif
