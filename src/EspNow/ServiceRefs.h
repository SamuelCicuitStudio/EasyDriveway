#pragma once
#include <cstdint>

namespace espnow {

// Forward declarations only; include real headers in .cpp files
struct RelayManager; struct SensorManager; struct DS18B20U;
struct BME280Manager; struct VEML7700Manager; struct CoolingManager;
struct BuzzerManager; struct RGBLed; struct PmsPower;
struct RTCManager; struct LogFS; struct NvsManager; struct TFLunaManager;

struct ServiceRefs {
  RelayManager*    relay   = nullptr;
  SensorManager*   sensor  = nullptr;
  DS18B20U*        ds18b20 = nullptr;
  BME280Manager*   bme     = nullptr;
  VEML7700Manager* veml    = nullptr;
  CoolingManager*  cooling = nullptr;
  BuzzerManager*   buzzer  = nullptr;
  RGBLed*          rgb     = nullptr;
  PmsPower*        pms     = nullptr;     // VI + power source + power groups
  RTCManager*      rtc     = nullptr;
  LogFS*           logs    = nullptr;
  NvsManager*      nvs     = nullptr;
  TFLunaManager*   tfluna  = nullptr;     // if centralized; else via SensorManager
};

} // namespace espnow
