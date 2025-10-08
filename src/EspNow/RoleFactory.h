#pragma once
#include <cstdint>

namespace espnow {

class IRoleAdapter;

IRoleAdapter* createRoleAdapter();   // uses #if/#elif on SetRole.* macros
uint8_t getLocalRoleCode();          // returns enum RoleCode (see Topology TLV)

} // namespace espnow
