#include "Adapter_REMU.h"

namespace espnow {

Adapter_REMU::Adapter_REMU(LogFS* log,
                           const NowAuth128& selfAuth,
                           const NowTopoToken128* topoToken,
                           uint16_t topoVerHint,
                           uint8_t maxVirtuals,
                           uint8_t chansPerVirt)
: _log(log), _auth(selfAuth), _topoVer(topoVerHint), _banks(maxVirtuals), _chPer(chansPerVirt)
{
  _hasTopo = (topoToken != nullptr);
  if (_hasTopo) _topo = *topoToken;
  now_get_mac_sta(_selfMac);

  if (_chPer > 32) _chPer = 32;
  if (_banks == 0) _banks = 1;

  _state.assign(_banks, 0u);
  _pulses.reserve(8);
}

void Adapter_REMU::fillHeaderEcho(NowHeader& h, uint8_t msg, uint16_t echoSeq,
                                  uint8_t virt, uint16_t flags) const
{
  memset(&h, 0, sizeof(h));
  h.proto_ver = NOW_PROTO_VER;
  h.msg_type  = msg;
  h.flags     = flags | (_hasTopo ? NOW_FLAGS_HAS_TOPO : 0);
  h.seq       = echoSeq;
  h.topo_ver  = _topoVer;
  h.virt_id   = virt;                 // echo caller's virt_id so their router maps it back
  h.reserved  = 0;
  const uint64_t ms = now_millis();
  for (int i=0;i<6;++i) h.ts_ms[i] = (uint8_t)((ms >> (8*i)) & 0xFF);
  memcpy(h.sender_mac, _selfMac, 6);
  h.sender_role = NOW_ROLE_REMU;
}

NowRlyState Adapter_REMU::makeState(uint8_t bank) const {
  NowRlyState st{};
  st.mask     = maskFor(bank);
  st.topo_ver = _topoVer;
  st.count    = _chPer;
  st.reserved = 0;
  return st;
}

void Adapter_REMU::applyOp(uint8_t bank, const NowCtrlRelay& req) {
  if (req.channel >= _chPer) return;

  uint32_t m = maskFor(bank);
  const uint32_t bit = (1u << req.channel);

  switch ((NowRlyOp)req.op) {
    case NOW_RLY_OP_OFF:    m &= ~bit; break;
    case NOW_RLY_OP_ON:     m |=  bit; break;
    case NOW_RLY_OP_TOGGLE: m ^=  bit; break;
    default: return;
  }

  // Pulse semantics: if pulse_ms > 0 and the resulting state is ON, schedule an OFF
  if (req.pulse_ms && (m & bit)) {
    _pulses.push_back(Pulse{ bank, req.channel, (uint32_t)(now_millis() + (uint32_t)req.pulse_ms) });
  }

  maskFor(bank) = m;
}

bool Adapter_REMU::handle(const uint8_t /*srcMac*/[6], const Packet& in, Packet& out) {
  if (!in.hdr) return false;

  const uint8_t bank = in.hdr->virt_id;
  if (bank == NOW_VIRT_PHY || !validBank(bank)) {
    // Not a virtual target or out of range → ignore
    return false;
  }

  switch (in.hdr->msg_type) {
    case NOW_MSG_CTRL_RELAY: {
      if (!in.body || in.bodyLen < sizeof(NowCtrlRelay)) return false;
      const NowCtrlRelay* req = reinterpret_cast<const NowCtrlRelay*>(in.body);

      applyOp(bank, *req);

      // ACK with current state for that bank
      NowHeader hdr; fillHeaderEcho(hdr, NOW_MSG_RLY_STATE, in.hdr->seq, bank, /*flags*/0);
      NowRlyState st = makeState(bank);
      return compose(out, hdr, _auth, _hasTopo ? &_topo : nullptr, &st, sizeof(st));
    }

    case NOW_MSG_RLY_STATE: {
      // Reply with current mask for bank
      NowHeader hdr; fillHeaderEcho(hdr, NOW_MSG_RLY_STATE, in.hdr->seq, bank, /*flags*/0);
      NowRlyState st = makeState(bank);
      return compose(out, hdr, _auth, _hasTopo ? &_topo : nullptr, &st, sizeof(st));
    }

    default:
      return false;
  }
}

void Adapter_REMU::tick() {
  if (_pulses.empty()) return;

  const uint32_t now = (uint32_t)now_millis();
  for (size_t i = 0; i < _pulses.size();) {
    if (_pulses[i].offAtMs <= now) {
      if (_pulses[i].idx < _chPer && validBank(_pulses[i].bank)) {
        maskFor(_pulses[i].bank) &= ~(1u << _pulses[i].idx);
      }
      _pulses.erase(_pulses.begin() + i);
    } else {
      ++i;
    }
  }
}

} // namespace espnow
