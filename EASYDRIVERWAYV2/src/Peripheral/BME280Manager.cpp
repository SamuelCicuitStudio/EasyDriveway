/**************************************************************
 *  Project : EasyDriveway
 *  File    : BME280Manager.cpp
 **************************************************************/

#include "BME280Manager.h"

#if defined(NVS_ROLE_SENS) || defined(NVS_ROLE_SEMU)
bool BME280Manager::begin(uint8_t addr) {
  if (!wire_) {
    if (hub_) { hub_->bringUpENV(); wire_ = &hub_->busENV(); }
    else { I2CBusHub::beginENV(); wire_ = &I2CBusHub::env(); }
  }
  i2c_addr_ = addr;
  if (!bme_.begin(i2c_addr_, wire_)) { initialized_ = false; return false; }
  bme_.setSampling(Adafruit_BME280::MODE_NORMAL,
                   Adafruit_BME280::SAMPLING_X4,
                   Adafruit_BME280::SAMPLING_X4,
                   Adafruit_BME280::SAMPLING_X4,
                   Adafruit_BME280::FILTER_X4,
                   Adafruit_BME280::STANDBY_MS_125);
  initialized_ = true;
  return true;
}
bool BME280Manager::read(float& tC, float& rh, float& p_Pa) {
  if (!initialized_) return false;
  const float t = bme_.readTemperature();
  const float h = bme_.readHumidity();
  const float p = bme_.readPressure();
  if (!isfinite(t) || !isfinite(h) || !isfinite(p)) return false;
  last_tC_ = t; last_rh_ = h; last_pPa_ = p; last_read_ms_ = millis();
  tC = t; rh = h; p_Pa = p;
  return true;
}
#endif
 