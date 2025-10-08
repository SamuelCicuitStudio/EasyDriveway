// RoleAdapters/Adapter_SENS.h
#pragma once
/**
 * Adapter_SENS — Sensor node adapter
 *
 * Managers:
 *  - SensorManager (TF-Luna pairs + VEML7700 day/night)
 *  - DS18B20U (optional)
 *  - RTCManager (optional)
 *  - LogFS (optional)
 *
 * Ops:
 *  - NOW_MSG_SENS_REPORT: reply with compact report (≤ 200 bytes body)
 *  - NOW_MSG_PING:        reply with mini status (day/night, temp, uptime)
 */

#include "IRoleAdapter.h"
#include "Peripheral/SensorManager.h"
#include "Peripheral/DS18B20U.h"
#include "Peripheral/RTCManager.h"
#include "Peripheral/LogFS.h"
#include "EspNow/EspNowCompat.h"
#include <vector>

namespace espnow {

class Adapter_SENS : public IRoleAdapter {
public:
  // Tokens: device token required, topo token optional (included iff provided)
  Adapter_SENS(SensorManager* sm,
               DS18B20U* ds18,
               RTCManager* rtc,
               LogFS* log,
               const NowAuth128& selfAuthToken,
               const NowTopoToken128* topoTokenOrNull = nullptr,
               uint16_t topoVerHint = 0);

  uint8_t role() const override { return NOW_ROLE_SENS; }

  // Handle request → reply (echo seq for ACK semantics)
  bool handle(const uint8_t srcMac[6], const Packet& in, Packet& out) override;

  // Optional periodic housekeeping/caching (keeps SensorManager FPS under control)
  void tick() override;

  // Configure minimum interval between polls (ms)
  void setMinPollMs(uint32_t ms) { _minPollMs = ms; }

private:
  // Adapter-defined blob format (packed) following NowSensReportHdr
  // Keep compact; <= 200 bytes body budget including this blob.
  struct NOW_PACKED BlobV1 {
    uint32_t epoch_ms_lo; // 0 if no RTC
    uint32_t epoch_ms_hi; // 0 if no RTC
    float    lux;         // ambient light
    uint8_t  isDay;       // 1=day, 0=night
    uint8_t  nPairs;      // number of TF-Luna pairs reported
    int16_t  temp_c_x10;  // optional DS18B20 *10 (INT16_MIN if N/A)
    uint8_t  reserved0;   // padding to 16
    struct NOW_PACKED Pair {
      uint8_t index;
      uint8_t presentA;
      uint8_t presentB;
      uint8_t direction;  // SensorManager::Direction
      uint16_t rate_hz;   // effective or averaged frame rate
      uint16_t reserved1;
    } pairs[8]; // up to 8 pairs (SEMU); SENS uses 1
  };

  static constexpr uint16_t kFmtV1 = 0x0001;

  // Compose a header echoing the caller’s seq
  void fillHeaderEcho(NowHeader& h, uint8_t msg, uint16_t echoSeq, uint16_t flags = 0) const;

  // Fill BlobV1 from current sensors; returns blob size actually used
  uint16_t buildBlobV1(BlobV1& b);

  // Cache helper to respect FPS / min poll cadence
  void refreshCacheIfStale();

private:
  SensorManager*   _sm{nullptr};
  DS18B20U*        _ds{nullptr};
  RTCManager*      _rtc{nullptr};
  LogFS*           _log{nullptr};

  NowAuth128       _auth{};
  NowTopoToken128  _topo{};
  bool             _hasTopo{false};
  uint8_t          _selfMac[6]{};
  uint8_t          _selfRole{NOW_ROLE_SENS};
  uint16_t         _topoVer{0};

  // Cached snapshot to avoid over-polling
  SensorManager::Snapshot _snap{};
  uint32_t         _lastPollMs{0};
  uint32_t         _minPollMs{50}; // default throttle (20 Hz max)

  // Cached DS18 temp *10 if available
  int16_t          _dsTempX10{INT16_MIN};
};

} // namespace espnow
