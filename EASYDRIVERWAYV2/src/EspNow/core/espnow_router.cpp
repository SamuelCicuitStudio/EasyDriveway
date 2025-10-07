// core/espnow_router.cpp
#include "EspNow/EspNowStack.h"
#include <cstring>
#include <cstdint>

#if defined(ARDUINO)
  #include <Arduino.h>
#endif

namespace espnow {

// radio RX callback signature (from transport)
using RxCallback = void(*)(const uint8_t* mac, const uint8_t* data, size_t len);
extern "C" {
  bool radio_set_rx(RxCallback cb);
}

// ---- small helpers ----
static inline bool mac_eq(const uint8_t a[6], const uint8_t b[6]) { return std::memcmp(a,b,6)==0; }

// Expected payload sizes per opcode (excludes header/auth/topo/trailer)
static int expected_payload_size(uint8_t mt) {
  switch (mt) {
    case NOW_MT_PAIR_REQ:     return 0;
    case NOW_MT_PAIR_ACK:     return 24;
    case NOW_MT_TOPO_PUSH:    return 4;   // header before TLV blob; router will validate tlv_len against frame
    case NOW_MT_NET_SET_CHAN: return 4;
    case NOW_MT_CTRL_RELAY:   return 4;
    case NOW_MT_SENS_REPORT:  return 30;
    case NOW_MT_RLY_STATE:    return 4;
    case NOW_MT_PMS_STATUS:   return 17;
    case NOW_MT_CONFIG_WRITE: return 8;   // +V, variable; we only check minimum here
    case NOW_MT_PING:         return 2;
    case NOW_MT_PING_REPLY:   return 5;
    case NOW_MT_TIME_SYNC:    return 8;
    case NOW_MT_FW_BEGIN:     return 52;
    case NOW_MT_FW_CHUNK:     return 12;  // +N
    case NOW_MT_FW_STATUS:    return 16;
    case NOW_MT_FW_COMMIT:    return 8;   // +S
    case NOW_MT_FW_ABORT:     return 8;
    default:                  return -1;
  }
}

// Small duplicate window by (mac,msg_type): track last seq; drop if too far behind
struct SeqState { uint8_t mac[6]; uint8_t msg_type; uint16_t last_seq; bool in_use; };
static SeqState g_seq[12];

static bool dup_window_ok_and_update(const uint8_t mac[6], uint8_t mt, uint16_t seq) {
  for (auto& s : g_seq) {
    if (s.in_use && s.msg_type == mt && mac_eq(s.mac, mac)) {
      // accept if seq is newer or within a small backward window (32)
      const uint16_t last = s.last_seq;
      const uint16_t diff = (uint16_t)(seq - last);
      if (diff == 0 || diff > 0x8000u /*very old wrap*/ ) return false; // treat as dup
      s.last_seq = seq;
      return true;
    }
  }
  for (auto& s : g_seq) {
    if (!s.in_use) {
      std::memcpy(s.mac, mac, 6);
      s.msg_type = mt;
      s.last_seq = seq;
      s.in_use = true;
      return true;
    }
  }
  // overwrite slot 0
  std::memcpy(g_seq[0].mac, mac, 6);
  g_seq[0].msg_type = mt;
  g_seq[0].last_seq = seq;
  g_seq[0].in_use = true;
  return true;
}

// ---- Router singleton bound to the public stack ----
class Router {
public:
  static Router& I() { static Router r; return r; }

  void attach(EspNowCallbacks* role) { role_ = role; }

  // Wire to radio
  bool bind_rx() {
    return radio_set_rx(&Router::on_rx_thunk);
  }

private:
  static void on_rx_thunk(const uint8_t* mac, const uint8_t* data, size_t len) {
    I().on_rx(mac, data, len);
  }

  void on_rx(const uint8_t* mac, const uint8_t* bytes, size_t len) {
    if (!mac || !bytes || len < sizeof(NowHeader)) return;

    // 1) Parse header
    const NowHeader* h = reinterpret_cast<const NowHeader*>(bytes);
    if (h->proto_ver != NOW_PROTO_VER || h->reserved != 0) return;

    size_t off = sizeof(NowHeader);
    const bool needs_auth = (h->msg_type != NOW_MT_PAIR_REQ);
    if (needs_auth && len < off + sizeof(NowAuth128) + sizeof(NowSecTrailer)) return;

    const NowAuth128* a = needs_auth ? reinterpret_cast<const NowAuth128*>(bytes + off) : nullptr;
    off += needs_auth ? sizeof(NowAuth128) : 0;

    const bool has_topo = (h->flags & NOW_FLAGS_HAS_TOPO) != 0;
    const NowTopoToken128* topo = nullptr;
    if (has_topo) {
      if (len < off + sizeof(NowTopoToken128)) return;
      topo = reinterpret_cast<const NowTopoToken128*>(bytes + off);
      off += sizeof(NowTopoToken128);
    }

    if (needs_auth && len < off + sizeof(NowSecTrailer)) return;
    const size_t payload_len = len - off - (needs_auth ? sizeof(NowSecTrailer) : 0);
    const uint8_t* payload   = bytes + off;
    const NowSecTrailer* sec = needs_auth ? reinterpret_cast<const NowSecTrailer*>(payload + payload_len) : nullptr;

    // 2) Size sanity for payload (minimum)
    int exp = expected_payload_size(h->msg_type);
    if (exp < 0) return;
    if (payload_len < (size_t)exp) return;

    // 3) Privileged gating is enforced by the stack policy (security/topology).
    //    Security: HMAC verify (also enforces replay window).
    if (needs_auth) {
      if (!verifyHmac(*h, *a, *sec, payload, (unsigned short)payload_len)) return;
    }

    // 4) Duplicate suppression window by (mac,msg_type,seq)
    if (!dup_window_ok_and_update(mac, h->msg_type, h->seq)) return;

    // 5) Topology token when required
    if (topoRequiresToken(h->msg_type)) {
      if (!has_topo) return;
      if (!topoValidateToken(*topo)) return;
    }

    // 6) Dispatch to role adapter (if provided)
    if (!role_) return;

    switch (h->msg_type) {
      case NOW_MT_PING: {
        if (payload_len >= sizeof(NowPing)) {
          const NowPing& p = *reinterpret_cast<const NowPing*>(payload);
          role_->onPing(p);
        }
      } break;

      case NOW_MT_PING_REPLY: {
        if (payload_len >= sizeof(NowPingReply)) {
          const NowPingReply& r = *reinterpret_cast<const NowPingReply*>(payload);
          role_->onPingReply(r);
        }
      } break;

      case NOW_MT_CONFIG_WRITE: {
        if (payload_len >= sizeof(NowConfigWrite)) {
          const NowConfigWrite& cw = *reinterpret_cast<const NowConfigWrite*>(payload);
          const uint8_t* val = payload + sizeof(NowConfigWrite);
          const uint16_t vlen = (uint16_t)(payload_len - sizeof(NowConfigWrite));
          role_->onConfigWrite(cw, { val, vlen });
        }
      } break;

      case NOW_MT_CTRL_RELAY: {
        if (payload_len >= sizeof(NowCtrlRelay)) {
          const NowCtrlRelay& cr = *reinterpret_cast<const NowCtrlRelay*>(payload);
          role_->onCtrlRelay(cr);
        }
      } break;

      case NOW_MT_SENS_REPORT: {
        if (payload_len >= sizeof(NowSensReport)) {
          const NowSensReport& r = *reinterpret_cast<const NowSensReport*>(payload);
          role_->onSensReport(r);
        }
      } break;

      case NOW_MT_PMS_STATUS: {
        if (payload_len >= sizeof(NowPmsStatus)) {
          const NowPmsStatus& r = *reinterpret_cast<const NowPmsStatus*>(payload);
          role_->onPmsStatus(r);
        }
      } break;

      case NOW_MT_FW_STATUS: {
        if (payload_len >= sizeof(NowFwStatus)) {
          const NowFwStatus& s = *reinterpret_cast<const NowFwStatus*>(payload);
          role_->onFwStatus(s);
        }
      } break;

      case NOW_MT_TOPO_PUSH: {
        // Minimal: role adapter can parse TLV; or you can call topo_apply_push_tlv here if you expose it.
        role_->onTopoPush({ payload, (unsigned short)payload_len });
      } break;

      default:
        // ignore others here or add handlers as you implement callbacks
        break;
    }
  }

private:
  EspNowCallbacks* role_ = nullptr;
};

// Public glue to bind router with radio
bool router_bind_rx(EspNowCallbacks* cb) { Router::I().attach(cb); return Router::I().bind_rx(); }

} // namespace espnow
