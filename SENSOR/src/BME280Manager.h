#pragma once
/**
 * @file BME280Manager.h
 * @brief Thin wrapper around Adafruit_BME280 that initializes itself from NVS
 *        via ConfigManager and uses a caller-provided TwoWire instance.
 *
 * Hardware (SSM default):
 *   - BME280 on I2C2 (SDA=16, SCL=17), address 0x76
 *
 * Usage:
 *   BME280Manager bme(&cfg, &Wire2);
 *   if (bme.begin()) { float t,rh,p; bme.read(t,rh,p); }
 */
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include "Config.h"
#include "ConfigManager.h"

class BME280Manager {
public:
  // Construct with pointer to ConfigManager and TwoWire to use
  BME280Manager(ConfigManager* cfg, TwoWire* wire)
  : cfg_(cfg), wire_(wire) {}

  // Initialize bus (pins from NVS) and sensor
  bool begin();

  // Read values; returns true if all finite
  bool read(float& tC, float& rh, float& p_Pa);

  // Convenience getters
  float temperatureC() const { return last_tC_; }
  float humidityRH()   const { return last_rh_; }
  float pressurePa()   const { return last_pPa_; }

  bool isHealthy()     const { return initialized_; }
  uint32_t lastReadMs()const { return last_read_ms_; }

  Adafruit_BME280& raw() { return bme_; }

private:
  ConfigManager* cfg_ = nullptr;
  TwoWire*       wire_ = nullptr;
  Adafruit_BME280 bme_;
  bool initialized_ = false;

  // Cached config pulled from NVS
  uint8_t  i2c_addr_ = BME_ADDR_DEFAULT;
  int      sda_pin_  = I2C2_SDA_PIN_DEFAULT;
  int      scl_pin_  = I2C2_SCL_PIN_DEFAULT;
  uint32_t i2c_freq_ = 400000; // 400kHz

  // Last read cache
  float     last_tC_  = NAN;
  float     last_rh_  = NAN;
  float     last_pPa_ = NAN;
  uint32_t  last_read_ms_ = 0;
};
