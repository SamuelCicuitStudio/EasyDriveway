// RoleAdapters/Adapter_ICM.cpp
#include "Adapter_ICM.h"
#include <string.h>

namespace espnow {

// Tiny ACK for PAIR: ok + chan
struct NOW_PACKED PairAckBody {
  uint8_t ok;     // 1=paired, 0=denied
  uint8_t chan;   // current network channel hint
  uint8_t rsv0{0}, rsv1{0};
};
static_assert(sizeof(PairAckBody) == 4, "PairAckBody size");

void Adapter_ICM::fillHeaderEcho(NowHeader& h, uint8_t msg, uint16_t echoSeq, uint16_t flags) const {
  memset(&h, 0, sizeof(h));
  h.proto_ver = NOW_PROTO_VER;
  h.msg_type  = msg;
  h.flags     = flags | (_hasTopo ? NOW_FLAGS_HAS_TOPO : 0);
  h.seq       = echoSeq;               // echo caller seq to satisfy their ACK
  h.topo_ver  = _topoVer;
  h.virt_id   = NOW_VIRT_PHY;
  const uint64_t ms = now_millis();
  for (int i=0;i<6;++i) h.ts_ms[i] = (uint8_t)((ms >> (8*i)) & 0xFF);
  memcpy(h.sender_mac, _selfMac, 6);
  h.sender_role = NOW_ROLE_ICM;
}

Adapter_ICM::PairResult
Adapter_ICM::doPair(const uint8_t reqMac[6], uint8_t reqRole,
                    const char* name, const NowAuth128& token)
{
  if (!_provisioning) {
    return PairResult(false, "closed");
  }

  // Require a non-zero device token (authorization)
  const uint8_t* tok = reinterpret_cast<const uint8_t*>(&token);
  bool tokenAllZero = true;
  for (int i=0;i<16;i++) if (tok[i] != 0) { tokenAllZero = false; break; }
  if (tokenAllZero) {
    return PairResult(false, "no_token");
  }

  // Persist peer (Peers handles NVS + esp_now peer add/remove)
  const bool ok = _peers && _peers->addPeer(reqMac, reqRole, tok, name ? name : "", /*enabled=*/true);
  if (!ok) return PairResult(false, "persist");

  // UX: blink / buzzer (optional, no-ops if nullptr)
  if (_rgb) { _rgb->startBlink(0x00FF00 /*green*/, 120); _rgb->stop(); }
  if (_buz) { _buz->play(BuzzerManager::EV_CONFIG_SAVED); }

  // Log (optional)
  if (_log) {
    char m[18]; fmtMac(reqMac, m);
    _log->eventf(LogFS::DOM_WIFI, LogFS::EV_INFO, 5101, "PAIR ok mac=%s role=%u", m, (unsigned)reqRole);
  }

  return PairResult(true, "ok");
}

bool Adapter_ICM::handleSetChannel(uint8_t newChan, NowNetSetChan& echoBody) {
  if (!validChan(newChan)) return false;

  if (_peers) _peers->setChannel(newChan);   // persist via Peers/NVS
  memset(&echoBody, 0, sizeof(echoBody));
  echoBody.channel = newChan;                   // field name is 'chan' in the wire body

  if (_log) _log->eventf(LogFS::DOM_WIFI, LogFS::EV_INFO, 5102, "NET_SET_CHAN=%u", (unsigned)newChan);
  return true;
}

bool Adapter_ICM::handle(const uint8_t srcMac[6], const Packet& in, Packet& out) {
  (void)srcMac;
  if (!in.hdr) return false;

  switch (in.hdr->msg_type) {

    case NOW_MSG_PAIR_REQ: {
      // Role from header; token from Packet::dev; body = optional raw node name
      char nameBuf[16] = {0};
      if (in.body && in.bodyLen) {
        const size_t n = (in.bodyLen < sizeof(nameBuf)-1) ? in.bodyLen : (sizeof(nameBuf)-1);
        memcpy(nameBuf, in.body, n);
        nameBuf[n] = 0;
      }

      if (_buz) { _buz->play(BuzzerManager::EV_PAIR_REQUEST); }
      if (_log) {
        char m[18]; fmtMac(in.hdr->sender_mac, m);
        _log->eventf(LogFS::DOM_WIFI, LogFS::EV_INFO, 5100,
                     "PAIR_REQ mac=%s role=%u name='%s'", m, (unsigned)in.hdr->sender_role, nameBuf);
      }

      if (!in.dev) return false; // no device token → drop
      PairResult pr = doPair(in.hdr->sender_mac, in.hdr->sender_role, nameBuf, *in.dev);

      // Build ACK (ok + current channel hint)
      NowHeader hdr; fillHeaderEcho(hdr, NOW_MSG_PAIR_ACK, in.hdr->seq, /*flags*/0);
      PairAckBody ack{};
      ack.ok   = pr.ok ? 1 : 0;
      ack.chan = _peers ? _peers->getChannel() : (uint8_t)NVS_DEF_CHAN;  // ← use Peers::getChannel()

      return compose(out, hdr, _auth, _hasTopo ? &_topo : nullptr, &ack, sizeof(ack));
    }

    case NOW_MSG_NET_SET_CHAN: {
      if (!in.body || in.bodyLen < sizeof(NowNetSetChan)) return false;
      const NowNetSetChan* req = reinterpret_cast<const NowNetSetChan*>(in.body);

      NowNetSetChan echo{};
      if (!handleSetChannel(req->channel, echo)) return false;

      NowHeader hdr; fillHeaderEcho(hdr, NOW_MSG_NET_SET_CHAN, in.hdr->seq, /*flags*/0);
      return compose(out, hdr, _auth, _hasTopo ? &_topo : nullptr, &echo, sizeof(echo));
    }

    case NOW_MSG_CONFIG_WRITE: {
      // Interpret key "CHAN__" as set-channel; value is the first byte AFTER the struct.
      if (!in.body || in.bodyLen < sizeof(NowConfigWrite)) return false;
      const NowConfigWrite* cfg = reinterpret_cast<const NowConfigWrite*>(in.body);
      if (!keyEqualsCHAN(reinterpret_cast<const char*>(cfg->key))) return false;

      // Tail payload begins right after the struct; require at least 1 byte
      const size_t tailLen = (size_t)in.bodyLen - sizeof(NowConfigWrite);
      if (tailLen < 1) return false;
      const uint8_t newChan = *(in.body + sizeof(NowConfigWrite));

      NowNetSetChan echo{};
      if (!handleSetChannel(newChan, echo)) return false;

      NowHeader hdr; fillHeaderEcho(hdr, NOW_MSG_NET_SET_CHAN, in.hdr->seq, /*flags*/0);
      return compose(out, hdr, _auth, _hasTopo ? &_topo : nullptr, &echo, sizeof(echo));
    }

    case NOW_MSG_TIME_SYNC:
      // ICM is the authority; ignore inbound TIME_SYNC
      return false;

    default:
      return false; // unhandled by this adapter
  }
}

} // namespace espnow
