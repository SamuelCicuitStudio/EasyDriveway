#pragma once
#include <cstdint>
#include <vector>
#include <array>

namespace espnow {

enum RoleCode : uint8_t { RC_ICM=1, RC_PMS=2, RC_SENSOR=3, RC_RELAY=4, RC_SEN_EMU=5, RC_REL_EMU=6 };

struct Topology {
  uint8_t   role = 0;                  // RoleCode
  char      token[32] = {0};           // exactly 32 chars
  std::vector<std::array<uint8_t,6>> neighbors;
  std::vector<uint8_t> roleParams;
  uint8_t   emuCount = 0;
};

// TLV encode/decode
bool topoEncode(const Topology& t, std::vector<uint8_t>& outTLV);
bool topoDecode(const uint8_t* tlv, uint16_t len, Topology& out);

// Local store accessors (defined in EspNowCore.cpp)
void setLocalTopology(const Topology& t);
const Topology& getLocalTopology();
bool importLocalTopology(const uint8_t* tlv, uint16_t len);
bool exportLocalTopology(std::vector<uint8_t>& tlvOut);

} // namespace espnow
