/**************************************************************
 *  Project : EasyDriveWay - PSM
 *  File    : ACS781.h
 *  Purpose : Hall-effect current sensor helper (ACS781-100B)
 *            using ESP32 analogReadMilliVolts().
 *
 *  Notes   : Pulls calibration from ConfigManager:
 *            - Vref (mV), Zero offset (mV), Sensitivity (uV/A)
 *            - Averaging samples, inversion flag, ADC attenuation
 **************************************************************/
#pragma once
#include <Arduino.h>
#include "Config.h"
#include "ConfigManager.h"

// ---------- Config keys (≤6 chars) with compile-time fallbacks ----------
#ifndef ACS_MODEL_KEY
  #define ACS_MODEL_KEY     "ACSMOD"   // e.g., "ACS781-100B"
#endif
#ifndef ACS_VREF_MV_KEY
  #define ACS_VREF_MV_KEY   "AVREF"    // supply/reference (mV), informational
#endif
#ifndef ACS_ZERO_MV_KEY
  #define ACS_ZERO_MV_KEY   "AZERO"    // zero-current output (mV)
#endif
#ifndef ACS_SENS_UVPA_KEY
  #define ACS_SENS_UVPA_KEY "ASNSUV"   // sensitivity (uV/A), e.g., 13200 for 13.2 mV/A
#endif
#ifndef ACS_AVG_KEY
  #define ACS_AVG_KEY       "AAVG"     // samples per reading
#endif
#ifndef ACS_INV_KEY
  #define ACS_INV_KEY       "AINVRT"   // invert sign (0/1)
#endif
#ifndef ACS_ATTEN_KEY
  #define ACS_ATTEN_KEY     "AATTN"    // ADC attenuation (0=0dB,1=2.5dB,2=6dB,3=11dB)
#endif

class ACS781 {
public:
  ACS781(int adcPin, ConfigManager* cfg);

  bool begin();                       // apply attenuation & load config
  void reloadFromConfig();            // re-read parameters from NVS

  // Read helpers
  uint16_t readMilliVolts(uint8_t samples=0) const; // averaged mV
  float    readAmps(uint8_t samples=0) const;       // signed current in Amps

  // Calibration (runtime)
  uint16_t calibrateZeroAtRest(uint16_t settle_ms=10, uint8_t samples=32, bool persist=true);
  void     setZero_mV(uint16_t mv, bool persist=true);

  // Accessors
  inline uint16_t zero_mV()   const { return _zero_mV; }
  inline int32_t  sens_uV_A() const { return _sens_uV_A; }
  inline uint16_t vref_mV()   const { return _vref_mV; }
  inline bool     inverted()  const { return _invert; }
  inline uint8_t  avg()       const { return _avg; }
  inline uint8_t  atten()     const { return _atten; }
  inline int      pin()       const { return _adcPin; }

  // Mutators
  void setSensitivity_uV_A(int32_t uv_per_A, bool persist=true);
  void setVref_mV(uint16_t mv, bool persist=true);
  void setAvg(uint8_t n, bool persist=true);
  void setInvert(bool inv, bool persist=true);
  void setAttenuation(uint8_t att, bool persist=true);

private:
  static uint16_t readMilliVoltsOnce(int pin);
  void applyAttenuation() const;

private:
  int             _adcPin;
  ConfigManager*  _cfg;

  // Parameters (defaults are safe for ACS781-100B on ~3.3V ref)
  uint16_t _zero_mV   = 1650;     // default ~Vref/2
  int32_t  _sens_uV_A = 13200;    // 13.2 mV/A for 100A variant
  uint16_t _vref_mV   = 3300;     // board supply nominal
  uint8_t  _avg       = 16;       // samples
  bool     _invert    = false;    // sign flip
  uint8_t  _atten     = 3;        // 11 dB by default
};
