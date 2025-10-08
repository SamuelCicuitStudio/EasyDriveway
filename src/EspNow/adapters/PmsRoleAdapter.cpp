#include "PmsRoleAdapter.h"
#include "../Opcodes.h"
#include <cstring>

#if __has_include("../../Peripheral/PmsPower.h")
  #include "../../Peripheral/PmsPower.h"
#endif

namespace espnow {

bool PmsRoleAdapter::handleRequest(const EspNowMsg& in, EspNowResp& out){
  if(!S || !S->pms) return false;
  switch(in.type){
    case GET_VI: {
      // Adjust to VI struct provided by your PmsPower class
      struct { float v; float i; } vi{0,0};
      // auto r = S->pms->readVI(); vi.v=r.v; vi.i=r.i;
      std::memcpy(out.out, &vi, sizeof(vi)); out.out_len = sizeof(vi);
      return true;
    }
    case GET_POWER_SOURCE: {
      uint8_t src = 0; // src = S->pms->getPowerSource();
      out.out[0] = src; out.out_len = 1;
      return true;
    }
    case SET_POWER_GROUPS: {
      // S->pms->setGroups(in.payload, in.payload_len);
      out.out_len = 0; return true;
    }
    default: return false;
  }
}

} // namespace espnow
