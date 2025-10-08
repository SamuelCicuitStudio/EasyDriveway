#pragma once
/**
 * Adapter_SEMU — Sensor emulator (virtual sensors selected by virt_id)
 *
 * Ops:
 *  - NOW_MSG_SENS_REPORT  → reply with deterministic synthetic report for that virt_id
 *  - NOW_MSG_PING         → small, deterministic mini-status
 *
 * Notes:
 *  - Each virt_id represents an independent virtual sensor bank with its own phase.
 *  - Router should gate topology-bound ops; SENS_REPORT is allowed without HAS_TOPO.
 */
#include <stdint.h>
#include <vector>
#include "IRoleAdapter.h"
#include "EspNow/EspNowAPI.h"
#include "Peripheral/LogFS.h"
#include "EspNow/EspNowCompat.h"
#include "EspNow/EspNowCodec.h"
#include <string.h>

namespace espnow {

class Adapter_SEMU : public IRoleAdapter {
public:
  /**
   * @param log           Optional logger
   * @param selfAuth      Device token to include in replies
   * @param topoToken     Optional topology token — included in replies iff non-null
   * @param topoVerHint   Topology version (diagnostic only; not required for SENS_REPORT)
   * @param maxVirtuals   Number of virtual sensor banks (virt_id in [0..maxVirtuals-1])
   */
  Adapter_SEMU(LogFS* log,
               const NowAuth128& selfAuth,
               const NowTopoToken128* topoToken,
               uint16_t topoVerHint,
               uint8_t maxVirtuals = 8);

  uint8_t role() const override { return NOW_ROLE_SEMU; }

  bool handle(const uint8_t srcMac[6], const Packet& in, Packet& out) override;
  void tick() override;

  void setTopoVersion(uint16_t v) { _topoVer = v; }
  uint8_t maxVirtuals() const { return _banks; }

private:
  // On-wire blob format for SEMU (version 1)
  enum : uint16_t { SEMU_FMT_V1 = 0x0001 };

  struct NOW_PACKED SemuV1 {
    uint32_t t_ms;        // uptime ms (clamped)
    int16_t  temp_c_x10;  // e.g. 253 = 25.3 C
    uint16_t humi_x10;    // e.g. 456 = 45.6 %
    uint16_t lux;         // simple photometric
    uint16_t dist_mm;     // range in mm
    uint16_t status;      // bitfield (sensor-present, ok flags)
  };

  // Per-virt synthetic phase
  struct Phase {
    uint32_t k; // evolves in tick()
  };

  void fillHeaderEcho(NowHeader& h, uint8_t msg, uint16_t echoSeq,
                      uint8_t virt, uint16_t flags = 0) const;

  bool validBank(uint8_t virt) const { return virt < _banks; }

  // Deterministic generators (no RNG)
  SemuV1 makeSemuV1(uint8_t virt) const;
  NowPingReply makePing(uint8_t virt) const;

private:
  LogFS*           _log{nullptr};
  NowAuth128       _auth{};
  NowTopoToken128  _topo{};
  bool             _hasTopo{false};
  uint8_t          _selfMac[6]{};
  uint16_t         _topoVer{0};

  uint8_t          _banks{0};
  std::vector<Phase> _phase; // one phase per virtual bank
};

} // namespace espnow
