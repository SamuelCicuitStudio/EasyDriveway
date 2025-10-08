#include "SensorRoleAdapter.h"
#include "../Opcodes.h"
#include "../TopologyTlv.h"
#include <cstring>

#if __has_include("../../Peripheral/SensorManager.h")
  #include "../../Peripheral/SensorManager.h"
#endif
#if __has_include("../../Peripheral/TFLunaManager.h")
  #include "../../Peripheral/TFLunaManager.h"
#endif
#if __has_include("../../Peripheral/BME280Manager.h")
  #include "../../Peripheral/BME280Manager.h"
#endif
#if __has_include("../../Peripheral/VEML7700Manager.h")
  #include "../../Peripheral/VEML7700Manager.h"
#endif

namespace espnow {

bool SensorRoleAdapter::handleRequest(const EspNowMsg& in, EspNowResp& out){
  if(!S) return false;
  switch(in.type){
    case GET_TFLUNA_RAW: {
      // Adjust to your API: write two int16 distances or whatever the manager returns
      struct { int16_t A_mm; int16_t B_mm; } resp{0,0};
      // if(S->tfluna) { auto r = S->tfluna->readRaw(); resp.A_mm = r.a; resp.B_mm = r.b; }
      std::memcpy(out.out, &resp, sizeof(resp)); out.out_len = sizeof(resp);
      return true;
    }
    case GET_ENV: {
      struct { float tempC; float hum; float press; } env{0,0,0};
      // if(S->bme) { auto e = S->bme->latest(); env.tempC=e.tempC; env.hum=e.hum; env.press=e.press; }
      std::memcpy(out.out, &env, sizeof(env)); out.out_len = sizeof(env);
      return true;
    }
    case GET_LUX: {
      uint32_t lux = 0; // if(S->veml) lux = S->veml->lux();
      std::memcpy(out.out, &lux, sizeof(lux)); out.out_len = sizeof(lux);
      return true;
    }
    case SET_THRESHOLDS: {
      // Pass-through thresholds to SensorManager
      // if(S->sensor) S->sensor->setThresholds(in.payload, in.payload_len);
      out.out_len = 0; return true;
    }
    default: return false;
  }
}

} // namespace espnow
