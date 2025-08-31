/**************************************************************
 *  Project     : EasyDriveWay - Power Supply Module (PSM)
 *  File        : PSMCommandAPI.h  (FIXED to align with shared CommandAPI.h)
 *  Purpose     : PSM-specific payloads building on CommandAPI.h
 *                NOTE: Uses PowerStatusPayload and TempPayload from CommandAPI.h
 **************************************************************/
#pragma once
#include <Arduino.h>
#include <stdint.h>
#include "CommandAPI.h"   // shared domains, header, and common payloads

// ---- PSM hello / init payload (carried inside SYS_INIT from ICM) ----
// ICM sends this once after pairing. PSM stores token16 and optional channel.
struct __attribute__((packed)) PsmSysInitPayload {
  uint8_t token16[16];   // first 16B of the shared token to validate master frames
  uint8_t channel;       // ESP-NOW channel to lock to (1..13). 0 = ignore
  uint8_t rsv[2];        // align/future
};

// ---- Capabilities blob (optional) ----
struct __attribute__((packed)) PsmCapsPayload {
  uint8_t ver_major;     // PSM firmware major
  uint8_t ver_minor;     // PSM firmware minor
  uint8_t has_temp;      // 1 if temperature sensor is available
  uint8_t has_charger;   // 1 if battery charger/monitor present
  uint32_t features;     // feature bitfield (future)
};

// ---- Helper to encode TempPayload (uses the shared TempPayload) ----
inline TempPayload makeTempPayload(float tC, int16_t raw = 0, uint8_t src = 0, bool ok = true) {
  TempPayload p{};
  p.tC_x100 = (int16_t)lrintf(tC * 100.0f);
  p.raw = raw;
  p.ok = ok ? 1 : 0;
  p.src = src;
  return p;
}
