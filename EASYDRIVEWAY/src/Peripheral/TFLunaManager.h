/**************************************************************
 *  TFLunaManager - Dual TF-Luna (I2C) manager for SSM
 *  Uses Bud Ryerson's TFLI2C library and reads pins/addresses
 *  from Config.h keys via ConfigManager.
 *
 *  Notes:
 *   - The TFLI2C library uses the global `Wire`. Pass the same
 *     TwoWire instance (usually &Wire) you want to use, and this
 *     class will call wire->begin(sda,scl) so TFLI2C rides on it.
 *   - Distances from TFLI2C are returned in centimeters; this
 *     manager converts to millimeters for convenience.
 **************************************************************/

 #pragma once

#include <Arduino.h>
#include <Wire.h>
#include "TFLI2C.h"
#include "Config.h"
#include "ConfigManager.h"
#ifdef NVS_ROLE_SENS
class TFLunaManager {
public:
  struct Sample {
    uint16_t dist_mm;       // distance in millimeters
    uint16_t amp;           // signal strength ("flux")
    int16_t  temp_c_x100;   // temperature in centi-deg C
    bool     ok;
  };

  TFLunaManager(ConfigManager* cfg, TwoWire* wire)
  : cfg_(cfg), wire_(wire) {}

  // Initialize I2C (pins from NVS) and configure both sensors.
  // fps_hz: typical 100. If 0, leaves default FPS.
  // continuous: true to Set_Cont_Mode, else trigger mode.
  bool begin(uint16_t fps_hz = 100, bool continuous = true);

  // Enable/disable both sensors (power-state bit via register)
  bool setEnable(bool en);

  // Change addresses (persist to NVS). Requires reboot to take effect.
  // Will attempt Soft_Reset after saving settings.
  bool setAddresses(uint8_t addrA, uint8_t addrB);

  // Set frame rate for both sensors (register FPS). Persists.
  bool setFrameRate(uint16_t fps_hz);

  // Read one or both sensors
  bool readA(Sample& s) { return readOne(addrA_, s); }
  bool readB(Sample& s) { return readOne(addrB_, s); }
  bool readBoth(Sample& a, Sample& b, uint16_t& rate_hz_out);

  // Presence helpers using near/far gates from NVS
  bool isPresentA(const Sample& s) const;
  bool isPresentB(const Sample& s) const;

  // Accessors
  uint8_t addrA() const { return addrA_; }
  uint8_t addrB() const { return addrB_; }
  uint8_t sdaPin() const { return sda_; }
  uint8_t sclPin() const { return scl_; }

  // Lightweight fetch adapter for external managers:
  // which: 0=Left(A), 1=Right(B) (or ignore & read both)
  // Returns both A and B samples plus an effective rate estimate.
  bool fetch(uint8_t which, Sample& A, Sample& B, uint16_t& rate_hz_out);

  bool readOne(uint8_t addr, Sample& s);

private:

  // Load pins/addresses/thresholds from NVS
  void loadConfig_();

  // Persist new addresses to NVS
  void saveAddresses_();

private:
  ConfigManager* cfg_ = nullptr;
  TwoWire*       wire_ = nullptr;
  TFLI2C         tfl_;          // library driver (stateless per call)

  // I2C1 pins & addresses (from NVS)
  uint8_t sda_   = PIN_I2C1_SDA_DEFAULT;
  uint8_t scl_   = PIN_I2C1_SCL_DEFAULT;
  uint8_t addrA_ = TFL_A_ADDR_DEFAULT;
  uint8_t addrB_ = TFL_B_ADDR_DEFAULT;

  // Presence gating
  uint16_t near_mm_ = TF_NEAR_MM_DEFAULT;
  uint16_t far_mm_  = TF_FAR_MM_DEFAULT;
};
#endif