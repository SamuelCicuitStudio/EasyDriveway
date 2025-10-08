// RoleAdapters/Adapter_REL.h
#pragma once
/**
 * Adapter_REL — Relay node adapter
 *
 * Ops:
 *  - NOW_MSG_CTRL_RELAY {idx, op, pulse_ms} → apply + ACK with current mask
 *  - NOW_MSG_RLY_STATE → reply mask + topo_ver
 *
 * Requirements:
 *  - Topology-bound ops (CTRL_RELAY) are already gated by Router via HAS_TOPO.
 */

#include <vector>
#include "IRoleAdapter.h"
#include "Peripheral/RelayManager.h"
#include "Peripheral/LogFS.h"

namespace espnow {

class Adapter_REL : public IRoleAdapter {
public:
  Adapter_REL(RelayManager* rel,
              LogFS* log,
              const NowAuth128& selfAuthToken,
              const NowTopoToken128* topoTokenOrNull = nullptr,
              uint16_t topoVerHint = 0);

  uint8_t role() const override { return NOW_ROLE_REL; }

  bool handle(const uint8_t srcMac[6], const Packet& in, Packet& out) override;
  void tick() override;

  void setTopoVersion(uint16_t v) { _topoVer = v; }

private:
  struct Pulse {
    uint16_t idx;
    uint32_t offAtMs;
  };

  // Compose a header echoing the caller's seq for ACK semantics
  void fillHeaderEcho(NowHeader& h, uint8_t msg, uint16_t echoSeq, uint16_t flags = 0);
  uint32_t readMask() const; // from RelayManager shadow

private:
  RelayManager*     _rel{nullptr};
  LogFS*            _log{nullptr};
  NowAuth128        _auth{};
  NowTopoToken128   _topo{};
  bool              _hasTopo{false};
  uint8_t           _selfMac[6]{};
  uint8_t           _selfRole{NOW_ROLE_REL};
  uint16_t          _topoVer{0};
  std::vector<Pulse> _pulses;
};

} // namespace espnow
