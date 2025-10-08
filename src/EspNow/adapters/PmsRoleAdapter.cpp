#include "PmsRoleAdapter.h"
#include "../Opcodes.h"
#include "../EspNowCore.h"
#include "CommonOps.h"
#include <cstring>

#if __has_include("../../Peripheral/PmsPower.h")
  #include "../../Peripheral/PmsPower.h"
#endif

namespace espnow {

bool PmsRoleAdapter::handleRequest(const EspNowMsg& in, EspNowResp& out){
  if(!S || !S->pms) return false;
  switch(in.type){
    case GET_VI: {
      typename glue::PmsGetVI<decltype(*S->pms)>::VI vi{0,0};
      glue::PmsGetVI<decltype(*S->pms)>::get(S->pms, vi);
      std::memcpy(out.out, &vi, sizeof(vi)); out.out_len = sizeof(vi);
      return true;
    }
    case GET_POWER_SOURCE: {
      uint8_t src=0; glue::PmsGetSrc<decltype(*S->pms)>::get(S->pms, src);
      out.out[0]=src; out.out_len=1; return true;
    }
    case SET_POWER_GROUPS: {
      bool ok = glue::PmsSetGroups<decltype(*S->pms)>::set(S->pms, in.payload, in.payload_len);
      out.out_len=0; return ok;
    }
    default: return false;
  }
}

} // namespace espnow
