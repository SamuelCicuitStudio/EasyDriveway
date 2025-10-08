// RoleAdapters/Adapter_REL.cpp
#include "Adapter_REL.h"
#include "EspNow/EspNowCompat.h"
#include <string.h>

namespace espnow {

Adapter_REL::Adapter_REL(RelayManager* rel,
                         LogFS* log,
                         const NowAuth128& selfAuthToken,
                         const NowTopoToken128* topoTokenOrNull,
                         uint16_t topoVerHint)
: _rel(rel), _log(log), _auth(selfAuthToken), _topoVer(topoVerHint)
{
  _hasTopo = (topoTokenOrNull != nullptr);
  if (_hasTopo) _topo = *topoTokenOrNull;
  now_get_mac_sta(_selfMac);
}

void Adapter_REL::fillHeaderEcho(NowHeader& h, uint8_t msg, uint16_t echoSeq, uint16_t flags) {
  memset(&h, 0, sizeof(h));
  h.proto_ver = NOW_PROTO_VER;
  h.msg_type  = msg;
  h.flags     = flags | (_hasTopo ? NOW_FLAGS_HAS_TOPO : 0);
  h.seq       = echoSeq;                 // echo caller seq to satisfy their ACK
  h.topo_ver  = _topoVer;
  h.virt_id   = NOW_VIRT_PHY;
  const uint64_t ms = now_millis();
  for (int i=0;i<6;++i) h.ts_ms[i] = (uint8_t)((ms >> (8*i)) & 0xFF);
  memcpy(h.sender_mac, _selfMac, 6);
  h.sender_role = _selfRole;
}

uint32_t Adapter_REL::readMask() const {
  if (!_rel) return 0;
  uint32_t m = 0;
  const uint16_t n = _rel->channels();
  for (uint16_t i=0; i<n && i<32; ++i) {
    if (_rel->get(i)) m |= (1u << i);
  }
  return m;
}

bool Adapter_REL::handle(const uint8_t /*srcMac*/[6], const Packet& in, Packet& out) {
  if (!_rel || !in.hdr) return false;

  switch (in.hdr->msg_type) {
    case NOW_MSG_CTRL_RELAY: {
      if (!in.body || in.bodyLen < sizeof(NowCtrlRelay)) return false;
      const NowCtrlRelay* req = reinterpret_cast<const NowCtrlRelay*>(in.body);

      // Bounds check
      const uint16_t n = _rel->channels();
      if (req->channel >= n) {
        // Invalid channel; NAK policy omitted to avoid info leak; just ignore.
        return false;
      }

      // Apply operation
      switch (req->op) {
        case NOW_RLY_OP_OFF:    _rel->set(req->channel, false); break;
        case NOW_RLY_OP_ON:     _rel->set(req->channel, true);  break;
        case NOW_RLY_OP_TOGGLE: _rel->toggle(req->channel);     break;
        default: return false;
      }

      // Pulse scheduling (non-blocking): ON then auto-OFF after pulse_ms
      if (req->pulse_ms > 0 && req->op != NOW_RLY_OP_OFF) {
        // Force ON and schedule OFF
        _rel->set(req->channel, true);
        Pulse p{ req->channel, (uint32_t)now_millis() + (uint32_t)req->pulse_ms };
        _pulses.push_back(p);
      }

      // Build ACK with current state bitmask
      NowHeader hdr; fillHeaderEcho(hdr, NOW_MSG_RLY_STATE, in.hdr->seq, /*flags*/0);
      NowRlyState st{};
      st.mask     = readMask();
      st.topo_ver = _topoVer;
      st.count    = (uint8_t)((n <= 255) ? n : 255);
      st.reserved = 0;

      return compose(out, hdr, _auth, _hasTopo ? &_topo : nullptr, &st, sizeof(st));
    }

    case NOW_MSG_RLY_STATE: {
      // Reply with current mask regardless of request body
      NowHeader hdr; fillHeaderEcho(hdr, NOW_MSG_RLY_STATE, in.hdr->seq, /*flags*/0);
      NowRlyState st{};
      const uint16_t n = _rel->channels();
      st.mask     = readMask();
      st.topo_ver = _topoVer;
      st.count    = (uint8_t)((n <= 255) ? n : 255);
      st.reserved = 0;

      return compose(out, hdr, _auth, _hasTopo ? &_topo : nullptr, &st, sizeof(st));
    }

    default:
      // Unhandled op
      return false;
  }
}

void Adapter_REL::tick() {
  if (!_rel) return;
  const uint32_t now = (uint32_t)now_millis();
  if (_pulses.empty()) return;

  // Turn OFF any elapsed pulses and erase them
  for (size_t i = 0; i < _pulses.size();) {
    if (_pulses[i].offAtMs <= now) {
      _rel->set(_pulses[i].idx, false);
      _pulses.erase(_pulses.begin() + i);
    } else {
      ++i;
    }
  }
}

} // namespace espnow
