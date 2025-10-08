#include "RelayRoleAdapter.h"
#include "../Opcodes.h"
#include "../TopologyTlv.h"
#include <cstring>

// Try to include real RelayManager header if available
#if __has_include("../../Peripheral/RelayManager.h")
  #include "../../Peripheral/RelayManager.h"
#elif __has_include("../Peripheral/RelayManager.h")
  #include "../Peripheral/RelayManager.h"
#endif

namespace espnow {

struct SetRelayPayload { uint8_t ch; uint8_t on; uint16_t ms; };

bool RelayRoleAdapter::handleRequest(const EspNowMsg& in, EspNowResp& out){
  if(!S || !S->relay) return false;
  switch(in.type){
    case GET_RELAY_STATES: {
      // Expect RelayManager provides a method to get states bitmap or array
      // We try common patterns: getStates(uint8_t* buf, size_t max) or readStates()
      #if __cpp_if_constexpr >= 201606
      #endif
      // Fallback: assume a bitmap getter
      uint32_t bitmap = 0;
      // If your RelayManager exposes a different API, adjust here:
      // bitmap = S->relay->getStatesBitmap();
      std::memcpy(out.out, &bitmap, sizeof(bitmap));
      out.out_len = sizeof(bitmap);
      return true;
    }
    case SET_RELAY: {
      if(in.payload_len < sizeof(SetRelayPayload)) return false;
      SetRelayPayload p{}; std::memcpy(&p, in.payload, sizeof(p));
      // Adjust to your actual API:
      // if(p.ms==0) S->relay->set(p.ch, p.on!=0);
      // else S->relay->pulse(p.ch, p.ms, p.on!=0);
      out.out_len = 0;
      return true;
    }
    default: return false;
  }
}

} // namespace espnow
