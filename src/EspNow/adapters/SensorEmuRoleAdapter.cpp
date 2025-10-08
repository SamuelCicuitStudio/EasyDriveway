#include "SensorEmuRoleAdapter.h"
#include "../Opcodes.h"
#include <cstring>

namespace espnow {

bool SensorEmuRoleAdapter::handleRequest(const EspNowMsg& in, EspNowResp& out){
  if(!S) return false;
  // Emu payloads prepend {uint8_t idx}
  if(in.payload_len < 1) return false;
  uint8_t idx = in.payload[0];
  const uint8_t* body = in.payload + 1;
  uint16_t blen = in.payload_len - 1;
  (void)idx; (void)body; (void)blen;
  switch(in.type){
    case GET_TFLUNA_RAW: {
      struct { int16_t A_mm; int16_t B_mm; } resp{0,0};
      std::memcpy(out.out,&resp,sizeof(resp)); out.out_len = sizeof(resp);
      return true;
    }
    case GET_ENV: {
      struct { float tempC; float hum; float press; } env{0,0,0};
      std::memcpy(out.out,&env,sizeof(env)); out.out_len = sizeof(env);
      return true;
    }
    case GET_LUX: {
      uint32_t lux = 0; std::memcpy(out.out, &lux, 4); out.out_len = 4; return true;
    }
    default: return false;
  }
}

} // namespace espnow
