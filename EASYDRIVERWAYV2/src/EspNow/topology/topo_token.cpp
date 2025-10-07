// topology/topo_token.cpp
#include "EspNow/EspNowStack.h"
#include <cstring>

namespace espnow {

// Access to current topo version from the store.
uint16_t topo_store_version();

// Utility: check if a 16-byte token is all zeros
static bool is_zero16(const uint8_t t[16]) {
  uint8_t acc = 0;
  for (int i = 0; i < 16; ++i) acc |= t[i];
  return acc == 0;
}

// ---- Public helper declared in EspNowStack.h ----
// Minimal policy today:
//  - MUST have a non-zero token
//  - MUST have a non-zero current topology version (i.e., we accepted a TOPO_PUSH)
// This immediately enforces that CTRL_RELAY is disabled until a topology
// is provisioned; cryptographic binding will be added once security exposes
// a small KDF for tokens (e.g., HMAC(PMK||"TTOK", topo_ver||role||virt||mac)).
bool topoValidateToken(const NowTopoToken128& tok) {
  if (topo_store_version() == 0) return false;        // no topology yet
  if (is_zero16(tok.token128)) return false;          // token must be present
  return true; // TODO: replace with real crypto: derive and compare
}

} // namespace espnow
