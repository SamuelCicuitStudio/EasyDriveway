#include "RelayEmuRoleAdapter.h"
#include "../Opcodes.h"
#include <cstring>

namespace espnow {

struct SetRelayPayload { uint8_t ch; uint8_t on; uint16_t ms; };

bool RelayEmuRoleAdapter::handleRequest(const EspNowMsg& in, EspNowResp& out){
  if(!S) return false;
  if(in.payload_len < 1) return false;
  uint8_t idx = in.payload[0];
  const uint8_t* body = in.payload + 1;
  uint16_t blen = in.payload_len - 1;
  (void)idx; (void)body; (void)blen;
  switch(in.type){
    case GET_RELAY_STATES: {
      uint32_t bitmap = 0;
      std::memcpy(out.out, &bitmap, sizeof(bitmap)); out.out_len = sizeof(bitmap);
      return true;
    }
    case SET_RELAY: {
      if(blen < sizeof(SetRelayPayload)) return false;
      out.out_len = 0; return true;
    }
    default: return false;
  }
}

} // namespace espnow
