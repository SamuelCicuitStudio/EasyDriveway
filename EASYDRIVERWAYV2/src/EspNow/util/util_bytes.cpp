// util/util_bytes.cpp
// Byte & endian utilities + CRC16-CCITT for the ESPNOW stack.

// Be tolerant to header location.
#if __has_include("EspNowStack.h")
  #include "EspNowStack.h"
#else
  #include "EspNow/EspNowStack.h"  // for ByteSpan decl
#endif

#include <cstdint>
#include <cstring>
#include <algorithm>

namespace espnow {

// ---------- CRC16-CCITT (poly 0x1021, init 0xFFFF, no refin, no refout, xorout 0x0000) ----------

static inline uint16_t crc16_ccitt_step(uint16_t crc, uint8_t b) {
  crc ^= static_cast<uint16_t>(b) << 8;
  for (int i = 0; i < 8; ++i) {
    crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                         : static_cast<uint16_t>(crc << 1);
  }
  return crc;
}

uint16_t crc16_ccitt(const uint8_t* data, size_t len, uint16_t init) {
  uint16_t crc = init;
  for (size_t i = 0; i < len; ++i) {
    crc = crc16_ccitt_step(crc, data[i]);
  }
  return crc;
}

uint16_t crc16_ccitt(ByteSpan span, uint16_t init) {
  return crc16_ccitt(reinterpret_cast<const uint8_t*>(span.data),
                     static_cast<size_t>(span.len), init);
}

// ---------- Little-endian writes (no bounds checks here; caller ensures space) ----------

void writeLE16(uint16_t v, uint8_t* dst) {
  dst[0] = static_cast<uint8_t>(v & 0xFF);
  dst[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
}

void writeLE32(uint32_t v, uint8_t* dst) {
  dst[0] = static_cast<uint8_t>(v & 0xFF);
  dst[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
  dst[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
  dst[3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}

void writeLE64(uint64_t v, uint8_t* dst) {
  dst[0] = static_cast<uint8_t>( v        & 0xFF);
  dst[1] = static_cast<uint8_t>((v >> 8 ) & 0xFF);
  dst[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
  dst[3] = static_cast<uint8_t>((v >> 24) & 0xFF);
  dst[4] = static_cast<uint8_t>((v >> 32) & 0xFF);
  dst[5] = static_cast<uint8_t>((v >> 40) & 0xFF);
  dst[6] = static_cast<uint8_t>((v >> 48) & 0xFF);
  dst[7] = static_cast<uint8_t>((v >> 56) & 0xFF);
}

// ---------- Little-endian reads with bounds checks ----------

bool readLE16(const uint8_t* src, size_t len, uint16_t& out) {
  if (len < 2) return false;
  out = static_cast<uint16_t>(src[0]) |
        (static_cast<uint16_t>(src[1]) << 8);
  return true;
}

bool readLE32(const uint8_t* src, size_t len, uint32_t& out) {
  if (len < 4) return false;
  out =  (static_cast<uint32_t>(src[0])      )
       | (static_cast<uint32_t>(src[1]) <<  8)
       | (static_cast<uint32_t>(src[2]) << 16)
       | (static_cast<uint32_t>(src[3]) << 24);
  return true;
}

bool readLE64(const uint8_t* src, size_t len, uint64_t& out) {
  if (len < 8) return false;
  out =  (static_cast<uint64_t>(src[0])      )
       | (static_cast<uint64_t>(src[1]) <<  8)
       | (static_cast<uint64_t>(src[2]) << 16)
       | (static_cast<uint64_t>(src[3]) << 24)
       | (static_cast<uint64_t>(src[4]) << 32)
       | (static_cast<uint64_t>(src[5]) << 40)
       | (static_cast<uint64_t>(src[6]) << 48)
       | (static_cast<uint64_t>(src[7]) << 56);
  return true;
}

// ByteSpan overloads
bool readLE16(ByteSpan span, uint16_t& out) {
  return readLE16(reinterpret_cast<const uint8_t*>(span.data),
                  static_cast<size_t>(span.len), out);
}
bool readLE32(ByteSpan span, uint32_t& out) {
  return readLE32(reinterpret_cast<const uint8_t*>(span.data),
                  static_cast<size_t>(span.len), out);
}
bool readLE64(ByteSpan span, uint64_t& out) {
  return readLE64(reinterpret_cast<const uint8_t*>(span.data),
                  static_cast<size_t>(span.len), out);
}

// ---------- Safe copy with clamping ----------

size_t clamp_copy(ByteSpan src, uint8_t* dst, size_t dst_max) {
  const size_t n = std::min(dst_max, static_cast<size_t>(src.len));
  if (n) std::memcpy(dst, src.data, n);
  return n;
}

} // namespace espnow
