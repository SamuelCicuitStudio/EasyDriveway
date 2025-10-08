#pragma once
#include <cstdint>

namespace espnow {

#pragma pack(push,1)
struct EspNowHeader {
  uint8_t  type;    // opcode (see Opcodes.h)
  uint8_t  flags;   // bit0: 1 = response, 0 = request
  uint16_t corr;    // correlation id (echoed back)
};
#pragma pack(pop)

struct EspNowMsg  {
  uint8_t  type;
  uint8_t  flags;
  uint16_t corr;
  const uint8_t* payload;
  uint16_t payload_len;
};

struct EspNowResp {
  uint8_t* out;
  uint16_t out_len;
};

static inline bool isResponse(uint8_t flags) { return (flags & 0x01) != 0; }
static inline uint8_t asResponse(uint8_t flags) { return flags | 0x01; }

} // namespace espnow
