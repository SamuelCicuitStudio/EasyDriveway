// topology/topo_store.cpp
#include "EspNow/EspNowStack.h"
#include <cstring>

namespace espnow {

// ---- Simple in-RAM store for the last accepted topology ----
// (kept tiny and static; persist to NVS later if you want durability)
static uint16_t g_topo_ver = 0;
static uint8_t  g_tlv_buf[512];
static size_t   g_tlv_len = 0;

// Internal setters used by topo_tlv.cpp
void topo_store_set(uint16_t ver, const uint8_t* tlv, size_t len) {
  g_topo_ver = ver;
  if (tlv && len) {
    if (len > sizeof(g_tlv_buf)) len = sizeof(g_tlv_buf);
    std::memcpy(g_tlv_buf, tlv, len);
    g_tlv_len = len;
  } else {
    g_tlv_len = 0;
  }
}

// Internal getters (optional; handy for debug/ICM echo)
uint16_t topo_store_version() { return g_topo_ver; }
size_t   topo_store_tlv(uint8_t* out, size_t cap) {
  if (!out || cap == 0) return 0;
  const size_t n = (g_tlv_len < cap) ? g_tlv_len : cap;
  if (n) std::memcpy(out, g_tlv_buf, n);
  return n;
}

// ---- Public helper declared in EspNowStack.h ----
bool topoRequiresToken(unsigned char msg_type) {
  // v2H spec: CTRL_RELAY (0x10) is topology-dependent and MUST carry a token.
  // You can add more opcodes later if needed.
  return msg_type == NOW_MT_CTRL_RELAY;
}

// ---- Public token validator is implemented in topo_token.cpp ----
// bool topoValidateToken(const NowTopoToken128& t);

} // namespace espnow
