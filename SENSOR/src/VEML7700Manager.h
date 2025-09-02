#pragma once
/**
 * @file VEML7700Manager.h
 * @brief Wrapper around SparkFun VEML7700 ambient light sensor library.
 *        Initializes from NVS via ConfigManager and uses a caller-provided TwoWire.
 *
 * Default SSM wiring:
 *   - VEML7700-TR on I2C2 (SDA=16, SCL=17), address 0x10
 *
 * Usage:
 *   VEML7700Manager als(&cfg, &Wire2);
 *   if (als.begin()) { float lux; als.read(lux); }
 */
#include <Arduino.h>
#include <Wire.h>
#include "SparkFun_VEML7700_Arduino_Library.h" // Library: SparkFun VEML7700
#include "Config.h"
#include "ConfigManager.h"

class VEML7700Manager {
public:
  VEML7700Manager(ConfigManager* cfg, TwoWire* wire)
  : cfg_(cfg), wire_(wire) {}

  // Initialize I2C bus from NVS pins and bring sensor online
  bool begin();

  // Read ambient light in lux. Returns true if a valid reading was obtained.
  bool read(float& lux);

  // Convenience getters
  float lux() const { return last_lux_; }
  bool  isHealthy() const { return initialized_; }
  uint32_t lastReadMs() const { return last_read_ms_; }

  // Optional: day/night helper using NVS thresholds (ALS_T0/ALS_T1 with hysteresis)
  // Returns: 1 = day, 0 = night
  uint8_t computeDayNight(float luxNow);

  // Expose raw object for advanced configuration if needed
  VEML7700& raw() { return als_; }

private:
  ConfigManager* cfg_ = nullptr;
  TwoWire*       wire_ = nullptr;
  VEML7700       als_;           // SparkFun class

  bool initialized_ = false;
  uint8_t  i2c_addr_ = ALS_ADDR_DEFAULT;  // usually 0x10
  int      sda_pin_  = I2C2_SDA_PIN_DEFAULT;
  int      scl_pin_  = I2C2_SCL_PIN_DEFAULT;
  uint32_t i2c_freq_ = 400000;   // 400 kHz

  // Day/Night hysteresis thresholds from NVS
  int als_t0_ = ALS_T0_LUX_DEFAULT; // day->night threshold (down-cross)
  int als_t1_ = ALS_T1_LUX_DEFAULT; // night->day threshold (up-cross)
  uint8_t is_day_ = 1;              // cached state

  float    last_lux_ = NAN;
  uint32_t last_read_ms_ = 0;
};
