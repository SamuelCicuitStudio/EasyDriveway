// topology/topo_tlv.cpp
#include "EspNow/EspNowStack.h"
#include <cstring>

namespace espnow {

// TLV item type codes (from your spec)
static constexpr uint8_t TLV_NODE_ENTRY     = 0x10;
static constexpr uint8_t TLV_TOPO_VERSION   = 0x11;
static constexpr uint8_t TLV_AUTH_HMAC      = 0xF0; // preferred for small-footprint
static constexpr uint8_t TLV_AUTH_SIG       = 0xF1; // signature over TLV digest (future)

// Forward to store (implemented in topo_store.cpp)
void     topo_store_set(uint16_t ver, const uint8_t* tlv, size_t len);
uint16_t topo_store_version();

// Minimal TLV walker: items are <type:1><len:1><value:len>
static bool tlv_has_auth_item(const uint8_t* tlv, size_t len) {
  size_t off = 0;
  while (off + 2 <= len) {
    uint8_t t = tlv[off + 0];
    uint8_t L = tlv[off + 1];
    off += 2;
    if (off + L > len) return false; // malformed length
    if (t == TLV_AUTH_HMAC || t == TLV_AUTH_SIG) return true;
    off += L;
  }
  return false;
}

// Public apply function (call this from your router when handling TOPO_PUSH):
// - header_topo_ver is the 16-bit version in NowHeader.topo_ver
// - tlv points to the TLV blob that follows NowTopoPush header
// Returns true iff accepted into the store.
bool topo_apply_push_tlv(uint16_t header_topo_ver, const uint8_t* tlv, size_t len) {
  if (!tlv || len < 2) return false;

  // MUST contain at least one auth item (HMAC or SIG); real cryptographic
  // verification will be added once security exposes a TLV-HMAC helper.
  if (!tlv_has_auth_item(tlv, len)) {
    return false;
  }

  // Optional: reject if version would go backwards (allow equal to idempotently reapply)
  const uint16_t cur = topo_store_version();
  if (header_topo_ver + 0u < cur) {
    return false;
  }

  // Accept and cache. Persisting to NVS can be added later.
  topo_store_set(header_topo_ver, tlv, len);
  return true;
}

} // namespace espnow
