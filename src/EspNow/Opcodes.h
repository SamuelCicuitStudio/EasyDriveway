#pragma once
#include <cstdint>

namespace espnow {

enum : uint8_t {
  // Common (all roles)
  GET_TEMP        = 0x01,  // float °C
  GET_TIME        = 0x02,  // uint32_t unix
  GET_FAN_MODE    = 0x03,  // uint8_t enum {0..4}
  GET_LOGS        = 0x04,  // req:{uint32_t off,uint16_t max}; resp:{bytes}
  GET_FAULTS      = 0x05,  // role-defined small struct
  GET_TOPOLOGY    = 0x06,  // TLV blob
  BUZZ_PING       = 0x10,  // no body
  LED_PING        = 0x11,  // tiny rgb if supported
  SET_FAN_MODE    = 0x12,  // uint8_t
  RESET_SOFT      = 0x13,  // optional
  RESTART_HARD    = 0x14,  // esp_restart if allowed
  SILENCE_OUTPUTS = 0x15,  // buzzer/led off
  SET_TIME        = 0x16,  // uint32_t unix

  // Relay (production)
  GET_RELAY_STATES= 0x20,  // bitmap/array
  SET_RELAY       = 0x21,  // {uint8_t ch; uint8_t on; uint16_t ms}

  // Sensor (production)
  GET_TFLUNA_RAW  = 0x30,  // struct from Sensor/TFLuna
  GET_ENV         = 0x31,  // BME280 (tempC, hum, press)
  GET_LUX         = 0x32,  // VEML value
  SET_THRESHOLDS  = 0x33,  // SensorManager thresholds

  // PMS
  GET_VI          = 0x40,  // VI struct
  GET_POWER_SOURCE= 0x41,  // enum {0=WALL,1=BATTERY}
  SET_POWER_GROUPS= 0x42,  // pass-through payload

  // ICM
  PAIR_EXCHANGE   = 0x50,  // pairing
  REMOVE_PEER     = 0x51,  // MAC in payload
  PUSH_TOPOLOGY   = 0x52,  // TLV blob
  PUSH_CONFIG     = 0x53,  // role-specific config

  // Emulators mirror production; payload prepends {uint8_t idx;}
};

} // namespace espnow
