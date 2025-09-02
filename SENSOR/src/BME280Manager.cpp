#include "BME280Manager.h"

bool BME280Manager::begin() {
  if (!cfg_ || !wire_) return false;

  // Pull pins & address from NVS (with sane defaults from Config.h)
  sda_pin_  = cfg_->GetInt(I2C2_SDA_PIN_KEY, I2C2_SDA_PIN_DEFAULT);
  scl_pin_  = cfg_->GetInt(I2C2_SCL_PIN_KEY, I2C2_SCL_PIN_DEFAULT);
  i2c_addr_ = static_cast<uint8_t>(cfg_->GetInt(BME_ADDR_KEY, BME_ADDR_DEFAULT));

  // Bring up I2C bus with requested pins (ESP32 supports pin init)
#if defined(ARDUINO_ARCH_ESP32)
  wire_->begin(sda_pin_, scl_pin_, i2c_freq_);
#else
  wire_->begin();
#endif

  // Init sensor
  if (!bme_.begin(i2c_addr_, wire_)) {
    initialized_ = false;
    return false;
  }

  // Reasonable default sampling: Normal mode, x4 oversampling
  bme_.setSampling(Adafruit_BME280::MODE_NORMAL,
                   Adafruit_BME280::SAMPLING_X4,   // temperature
                   Adafruit_BME280::SAMPLING_X4,   // pressure
                   Adafruit_BME280::SAMPLING_X4,   // humidity
                   Adafruit_BME280::FILTER_X4,
                   Adafruit_BME280::STANDBY_MS_125);

  initialized_ = true;
  return true;
}

bool BME280Manager::read(float& tC, float& rh, float& p_Pa) {
  if (!initialized_) return false;

  float t = bme_.readTemperature();   // °C
  float h = bme_.readHumidity();      // %RH
  float p = bme_.readPressure();      // Pa

  if (!isfinite(t) || !isfinite(h) || !isfinite(p)) {
    return false;
  }

  last_tC_  = t;
  last_rh_  = h;
  last_pPa_ = p;
  last_read_ms_ = millis();

  tC  = t;
  rh  = h;
  p_Pa= p;
  return true;
}
