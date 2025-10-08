#include "RelayRoleAdapter.h"
#include "../Opcodes.h"
#include "../TopologyTlv.h"
#include "../EspNowCore.h"
#include "CommonOps.h"
#include <cstring>

#if __has_include("../../Peripheral/RelayManager.h")
  #include "../../Peripheral/RelayManager.h"
#endif

namespace espnow {

struct SetRelayPayload { uint8_t ch; uint8_t on; uint16_t ms; };

bool RelayRoleAdapter::handleRequest(const EspNowMsg& in, EspNowResp& out){
  if(!S || !S->relay) return false;
  switch(in.type){
    case GET_RELAY_STATES: {
      uint16_t n = glue::RelayGetStates<decltype(*S->relay)>::get(S->relay, out.out, 256);
      out.out_len = n; return n>0;
    }
    case SET_RELAY: {
      if(in.payload_len < sizeof(SetRelayPayload)) return false;
      SetRelayPayload p{}; std::memcpy(&p, in.payload, sizeof(p));
      bool ok = glue::RelaySet<decltype(*S->relay)>::set(S->relay, p.ch, p.on!=0, p.ms);
      out.out_len = 0; return ok;
    }
    default: return false;
  }
}

} // namespace espnow
