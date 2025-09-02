#include "VEML7700Manager.h"

bool VEML7700Manager::begin() {
  if (!cfg_ || !wire_) return false;

  // Pull pins, address, thresholds from NVS (with defaults from Config.h)
  sda_pin_  = cfg_->GetInt(I2C2_SDA_PIN_KEY, I2C2_SDA_PIN_DEFAULT);
  scl_pin_  = cfg_->GetInt(I2C2_SCL_PIN_KEY, I2C2_SCL_PIN_DEFAULT);
  i2c_addr_ = static_cast<uint8_t>(cfg_->GetInt(ALS_ADDR_KEY, ALS_ADDR_DEFAULT));
  als_t0_   = cfg_->GetInt(ALS_T0_LUX_KEY, ALS_T0_LUX_DEFAULT);
  als_t1_   = cfg_->GetInt(ALS_T1_LUX_KEY, ALS_T1_LUX_DEFAULT);

#if defined(ARDUINO_ARCH_ESP32)
  wire_->begin(sda_pin_, scl_pin_, i2c_freq_);
#else
  wire_->begin();
#endif

  // Begin sensor with provided TwoWire. SparkFun lib defaults to address 0x10.
  // Many releases accept wire reference; if your version only has begin(), drop the argument.
if (!als_.begin(i2c_addr_, *wire_)) {
    initialized_ = false;
    return false;
}

  // Optional: you can tune gain/integration here if desired, e.g.:
  // als_.setGain(VEML7700_GAIN_1);
  // als_.setIntegrationTime(VEML7700_IT_100MS);

  initialized_ = true;
  return true;
}

bool VEML7700Manager::read(float& lux) {
  if (!initialized_) return false;

  float lx = NAN;
  // SparkFun API returns 0 on success for getALSLux; adjust if your version differs
  int rc = als_.getLux(lx);
  if (rc != 0 || !isfinite(lx)) {
    return false;
  }

  last_lux_ = lx;
  last_read_ms_ = millis();
  lux = lx;
  return true;
}

uint8_t VEML7700Manager::computeDayNight(float luxNow) {
  // Hysteresis based on NVS thresholds:
  //  - If currently day and we drop below T0 => night
  //  - If currently night and we rise above T1 => day
  if (is_day_) {
    if (luxNow <= als_t0_) is_day_ = 0;
  } else {
    if (luxNow >= als_t1_) is_day_ = 1;
  }
  return is_day_;
}
