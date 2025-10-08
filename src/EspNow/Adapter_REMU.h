#pragma once
/**
 * Adapter_REMU — Relay emulator (virtual relays selected by virt_id)
 *
 * Ops:
 *  - NOW_MSG_CTRL_RELAY {channel, op, pulse_ms} → apply on current virt_id + ACK with state
 *  - NOW_MSG_RLY_STATE → reply with current state for current virt_id
 *
 * Notes:
 *  - Topology-bound ops (CTRL_RELAY, CONFIG_WRITE) must be gated by Router via HAS_TOPO.
 *  - Each virt_id represents an independent relay bank with its own bitmask.
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

class Adapter_REMU : public IRoleAdapter {
public:
  /**
   * @param log           Optional logger
   * @param selfAuth      Device token for replies
   * @param topoToken     Optional topology token (replies include it iff _hasTopo is true)
   * @param topoVerHint   Topology version used in RLY_STATE
   * @param maxVirtuals   Number of virtual banks (virt_id in [0..maxVirtuals-1])
   * @param chansPerVirt  Relay channels per bank (<= 32; represented as a 32-bit mask)
   */
  Adapter_REMU(LogFS* log,
               const NowAuth128& selfAuth,
               const NowTopoToken128* topoToken,
               uint16_t topoVerHint,
               uint8_t maxVirtuals = 8,
               uint8_t chansPerVirt = 16);

  uint8_t role() const override { return NOW_ROLE_REMU; }

  bool handle(const uint8_t srcMac[6], const Packet& in, Packet& out) override;
  void tick() override;

  void setTopoVersion(uint16_t v) { _topoVer = v; }
  uint8_t maxVirtuals() const { return _banks; }
  uint8_t channelsPerVirtual() const { return _chPer; }

private:
  struct Pulse {
    uint8_t bank;
    uint8_t idx;
    uint32_t offAtMs;
  };

  void fillHeaderEcho(NowHeader& h, uint8_t msg, uint16_t echoSeq,
                      uint8_t virt /* echo */, uint16_t flags = 0) const;

  bool validBank(uint8_t virt) const { return virt < _banks; }
  uint32_t&       maskFor(uint8_t bank)       { return _state[bank]; }
  const uint32_t& maskFor(uint8_t bank) const { return _state[bank]; }

  void applyOp(uint8_t bank, const NowCtrlRelay& req);
  NowRlyState makeState(uint8_t bank) const;

private:
  LogFS*           _log{nullptr};
  NowAuth128       _auth{};
  NowTopoToken128  _topo{};
  bool             _hasTopo{false};
  uint8_t          _selfMac[6]{};
  uint16_t         _topoVer{0};

  uint8_t          _banks{0};
  uint8_t          _chPer{0};
  std::vector<uint32_t> _state;   // per-bank ON/OFF bitmap
  std::vector<Pulse>    _pulses;  // scheduled OFFs for pulses
};

} // namespace espnow
