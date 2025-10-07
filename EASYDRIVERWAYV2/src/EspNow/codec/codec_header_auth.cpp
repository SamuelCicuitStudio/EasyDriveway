// codec/codec_header_auth.cpp

#include "EspNow/EspNowStack.h"
#include <cstring>  // std::memset

namespace espnow {

// ---------------- v2H size guards (wire) ----------------
static_assert(sizeof(NowHeader)      == 23, "NowHeader must be 23 bytes (packed)");
static_assert(sizeof(NowAuth128)     == 16, "NowAuth128 must be 16 bytes");
static_assert(sizeof(NowTopoToken128)== 16, "NowTopoToken128 must be 16 bytes");
static_assert(sizeof(NowSecTrailer)  == (NOW_HMAC_NONCE_LEN + NOW_HMAC_TAG_LEN),
              "NowSecTrailer size mismatch");

// ---------------- Builders (init with safe defaults) ----------------

void buildHeader(NowHeader& h) {
  std::memset(&h, 0, sizeof(h));
  h.proto_ver  = NOW_PROTO_VER;   // v2H
  // other fields intentionally left for callers:
  // h.msg_type, h.flags, h.seq, h.topo_ver, h.virt_id, h.ts_ms[6], h.sender_mac[6], h.sender_role
}

void buildAuth(NowAuth128& a) {
  // Caller should copy its 16B device token later.
  std::memset(&a, 0, sizeof(a));
}

void buildSecTrailer(NowSecTrailer& s) {
  // Caller later sets nonce[6] and HMAC tag[12]
  std::memset(&s, 0, sizeof(s));
}

} // namespace espnow
