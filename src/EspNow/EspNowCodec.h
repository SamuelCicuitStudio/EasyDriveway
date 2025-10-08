// EspNowCodec.h
#pragma once
/**
 * EspNowCodec — compose/parse frames that match EspNowAPI v3-T.
 *
 * Wire order:
 *   NowHeader (23B) → NowAuth128 (16B) → [NowTopoToken128 (16B) iff HAS_TOPO] → Payload (0..NOW_MAX_BODY)
 *
 * Guarantees:
 *  - Enforces NOW_PROTO_VER.
 *  - Always requires Device Token (NowAuth128).
 *  - Topology Token is required iff NOW_FLAGS_HAS_TOPO is set in header.
 *  - Bounds-safe against ESPNOW MTU (NOW_MAX_FRAME).
 *
 * No HMAC/signature trailer in v3-T (token-only).
 */

#include <stdint.h>
#include <string.h>
#include "EspNowCompat.h"   // for NOW_ALWAYS_INLINE and helpers
#include "EspNowAPI.h"      // header/flags/tokens/payload limits (v3-T)

namespace espnow {

struct Packet {
  uint8_t  buf[NOW_MAX_FRAME];  // full encoded frame (≤ 250B)
  uint16_t len{0};              // total used bytes
  // Parsed views (point into buf after parse/compose)
  const NowHeader*       hdr{nullptr};
  const NowAuth128*      dev{nullptr};   // device token (always present)
  const NowTopoToken128* topo{nullptr};  // topology token (nullable)
  const uint8_t*         body{nullptr};
  uint16_t               bodyLen{0};

  // Convenience
  bool hasTopo()    const { return topo != nullptr; }
  bool reliable()   const { return hdr && (hdr->flags & NOW_FLAGS_RELIABLE); }
};

enum ParseResult : uint8_t {
  PARSE_OK = 0,
  PARSE_TOO_SMALL,    // shorter than minimum header+device token
  PARSE_BAD_VER,      // proto_ver mismatch
  PARSE_FLAG_MISMATCH,// HAS_TOPO set but not enough bytes for topo token
  PARSE_OVERFLOW      // total length exceeds NOW_MAX_FRAME or internal overrun
};

// Compute the encoded prefix length for given flags (header + tokens)
NOW_ALWAYS_INLINE uint16_t wirePrefixLen(uint16_t flags) {
  uint16_t n = sizeof(NowHeader) + sizeof(NowAuth128);
  if (flags & NOW_FLAGS_HAS_TOPO) n += sizeof(NowTopoToken128);
  return n;
}

/**
 * Compose a frame into Packet::buf.
 * Returns false if size would exceed NOW_MAX_FRAME.
 *
 * @param hdr        Filled header (proto_ver must be NOW_PROTO_VER).
 * @param dev        16-byte device token (required).
 * @param topo       Optional 16-byte topology token (required iff HAS_TOPO in hdr.flags).
 * @param body       Pointer to body bytes (can be nullptr if bodyLen==0).
 * @param bodyLen    Body size in bytes (≤ NOW_MAX_BODY).
 */
bool compose(Packet& out,
             const NowHeader& hdr,
             const NowAuth128& dev,
             const NowTopoToken128* topo,
             const void* body, uint16_t bodyLen);

/**
 * Parse raw bytes into a Packet view (pointers reference memory inside out.buf).
 * Returns a ParseResult. On PARSE_OK, Packet views are valid.
 */
ParseResult parse(const uint8_t* raw, uint16_t rawLen, Packet& out);

} // namespace espnow
