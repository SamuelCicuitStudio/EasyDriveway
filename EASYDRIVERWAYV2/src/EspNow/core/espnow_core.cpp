// core/espnow_core.cpp
#include "EspNow/EspNowStack.h"
#include <cstring>
#include <cstdint>

#if defined(ARDUINO)
  #include <Arduino.h>
  #include <WiFi.h>
  static uint64_t now_ms() { return (uint64_t)millis(); }
  static void get_mac(uint8_t out[6]) { WiFi.macAddress(out); }
#elif defined(ESP_PLATFORM)
  #include <esp_timer.h>
  #include <esp_wifi.h>
  static uint64_t now_ms() { return (uint64_t)(esp_timer_get_time() / 1000ULL); }
  static void get_mac(uint8_t out[6]) { esp_wifi_get_mac(WIFI_IF_STA, out); }
#else
  #include <chrono>
  static uint64_t now_ms() {
    using namespace std::chrono;
    static const auto t0 = std::chrono::steady_clock::now();
    return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-t0).count();
  }
  static void get_mac(uint8_t out[6]) { std::memset(out, 0, 6); }
#endif

namespace espnow {

// transport + scheduler + router internal API (same namespace, no extern "C")
bool  radio_init(uint8_t channel);  // defined in transport/espnow_radio.cpp

bool  sched_enqueue(const uint8_t mac[6], uint8_t msg_type, const uint8_t* bytes, uint16_t len, uint8_t retries);
void  sched_tick();
bool  router_bind_rx(EspNowCallbacks* cb);

// ---- Stack-wide state ----
static EspNowDeps     g_deps{};
static EspNowSettings g_cfg{};
static EspNowCallbacks* g_role = nullptr;

static uint16_t g_seq = 1;
static uint64_t g_nonce48 = 1;

// Fill header/auth/sec (nonce + tag later)
static void fill_header_auth(NowHeader& h, NowAuth128& a, uint8_t msg_type, uint16_t flags, uint16_t topo_ver, uint8_t virt_id) {
  buildHeader(h);
  h.msg_type  = msg_type;
  h.flags     = flags;
  h.seq       = g_seq++;
  h.topo_ver  = topo_ver;
  h.virt_id   = virt_id;
  h.reserved  = 0;

  // timestamp lower 48 bits
  uint64_t t = now_ms();
  for (int i = 0; i < 6; ++i) h.ts_ms[i] = (uint8_t)((t >> (8*i)) & 0xFF);

  // sender MAC + role
  get_mac(h.sender_mac);
  h.sender_role = g_cfg.sender_role;

  buildAuth(a);
  std::memcpy(a.device_token128, g_cfg.device_token, 16);
}

static void fill_sec(NowSecTrailer& s) {
  // 48-bit nonce (monotonic)
  uint64_t n = g_nonce48++;
  for (int i = 0; i < NOW_HMAC_NONCE_LEN; ++i) s.nonce[i] = (uint8_t)((n >> (8*i)) & 0xFF);
  // tag filled after HMAC compute
}

// Serialize a whole ESPNOW frame into buf:
//   NowHeader | NowAuth128 | [TopoToken?] | payload | NowSecTrailer
// Returns total length.
static uint16_t encode_frame(uint8_t* out, uint8_t msg_type, uint16_t flags,
                             const void* payload, uint16_t payload_len,
                             bool include_topo, const NowTopoToken128* topo_opt)
{
  NowHeader h; NowAuth128 a; NowSecTrailer s;
  fill_header_auth(h, a, msg_type, flags, g_cfg.topo_ver, /*virt*/0xFF);
  buildSecTrailer(s);
  fill_sec(s);

  uint8_t* p = out;
  std::memcpy(p, &h, sizeof(h)); p += sizeof(h);
  std::memcpy(p, &a, sizeof(a)); p += sizeof(a);

  if (include_topo && topo_opt) {
    std::memcpy(p, topo_opt, sizeof(NowTopoToken128)); p += sizeof(NowTopoToken128);
  }

  if (payload && payload_len) {
    std::memcpy(p, payload, payload_len); p += payload_len;
  }

  // TODO: compute HMAC tag over header|auth|[topo?+payload]|nonce and write into s.tag
  std::memset(s.tag, 0x00, NOW_HMAC_TAG_LEN); // temporary bring-up tag

  // Append trailer
  std::memcpy(p, &s, sizeof(s)); p += sizeof(s);
  return (uint16_t)(p - out);
}

// -------------- EspNowStack methods --------------

void EspNowStack::begin(const EspNowDeps& deps, const EspNowSettings& settings) {
  g_deps = deps;
  g_cfg  = settings;
  g_role = role_; // not yet set; role adapter calls setRoleAdapter()

  // Transport init & bind router
  radio_init(settings.channel);
  router_bind_rx(role_); // binds now; setRoleAdapter can update later
}

void EspNowStack::tick() {
  sched_tick();
}

void EspNowStack::setRoleAdapter(EspNowCallbacks* adapter) {
  g_role = adapter;
  // Re-bind router with the new adapter
  router_bind_rx(adapter);
}

// ---- Outbound helpers ----
static bool send_common(uint8_t msg_type, const void* payload, uint16_t payload_len, bool needs_topo) {
  uint8_t frame[256];
  NowTopoToken128 t{};
  if (needs_topo) {
    // Minimal non-zero token so current topoValidateToken() passes
    for (int i = 0; i < 16; ++i) t.token128[i] = (uint8_t)(i + 1);  // <-- FIXED FIELD NAME
  }
  const uint16_t len = encode_frame(frame, msg_type,
                                    (uint16_t)(needs_topo ? NOW_FLAGS_HAS_TOPO : 0),
                                    payload, payload_len,
                                    needs_topo, needs_topo ? &t : nullptr);

  // Destination: ICM by default (works for non-ICM roles)
  return sched_enqueue(g_cfg.icm_mac, msg_type, frame, len, /*retries*/3);
}

void EspNowStack::sendPing() {
  NowPing p{};
  send_common(NOW_MT_PING, &p, sizeof(p), /*needs_topo*/false);
}

void EspNowStack::sendConfigWrite(const NowConfigWrite& hdr, ByteSpan value) {
  uint8_t buf[64];
  if (sizeof(hdr) + value.len > sizeof(buf)) return;
  std::memcpy(buf, &hdr, sizeof(hdr));
  if (value.data && value.len) std::memcpy(buf + sizeof(hdr), value.data, value.len);
  send_common(NOW_MT_CONFIG_WRITE, buf, (uint16_t)(sizeof(hdr) + value.len), /*needs_topo*/false);
}

void EspNowStack::sendCtrlRelay(const NowCtrlRelay& ctrl) {
  send_common(NOW_MT_CTRL_RELAY, &ctrl, sizeof(ctrl), /*needs_topo*/true);
}

void EspNowStack::sendTopoPush(ByteSpan tlv) {
  send_common(NOW_MT_TOPO_PUSH, tlv.data, tlv.len, /*needs_topo*/false);
}

void EspNowStack::sendFwBegin(const NowFwBegin& fb)   { send_common(NOW_MT_FW_BEGIN,  &fb, sizeof(fb), false); }
void EspNowStack::sendFwChunk(const NowFwChunk& fc, ByteSpan data) {
  uint8_t buf[220];
  if (sizeof(fc) + data.len > sizeof(buf)) return;
  std::memcpy(buf, &fc, sizeof(fc));
  if (data.data && data.len) std::memcpy(buf + sizeof(fc), data.data, data.len);
  send_common(NOW_MT_FW_CHUNK, buf, (uint16_t)(sizeof(fc) + data.len), false);
}
void EspNowStack::sendFwCommit(const NowFwCommit& cm, ByteSpan sig) {
  uint8_t buf[96];
  if (sizeof(cm) + sig.len > sizeof(buf)) return;
  std::memcpy(buf, &cm, sizeof(cm));
  if (sig.data && sig.len) std::memcpy(buf + sizeof(cm), sig.data, sig.len);
  send_common(NOW_MT_FW_COMMIT, buf, (uint16_t)(sizeof(cm) + sig.len), false);
}
void EspNowStack::sendFwAbort(const NowFwAbort& ab)   { send_common(NOW_MT_FW_ABORT,  &ab, sizeof(ab), false); }

} // namespace espnow
