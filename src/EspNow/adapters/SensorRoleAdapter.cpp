#include "SensorRoleAdapter.h"
#include "../Opcodes.h"
#include "../TopologyTlv.h"
#include "../EspNowCore.h"
#include "CommonOps.h"
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
#if __has_include("../../Peripheral/DS18B20U.h")
  #include "../../Peripheral/DS18B20U.h"
#endif
#if __has_include("../../Peripheral/CoolingManager.h")
  #include "../../Peripheral/CoolingManager.h"
#endif
#if __has_include("../../Peripheral/BuzzerManager.h")
  #include "../../Peripheral/BuzzerManager.h"
#endif
#if __has_include("../../Peripheral/RGBLed.h")
  #include "../../Peripheral/RGBLed.h"
#endif
#if __has_include("../../Peripheral/RTCManager.h")
  #include "../../Peripheral/RTCManager.h"
#endif
#if __has_include("../../Peripheral/LogFS.h")
  #include "../../Peripheral/LogFS.h"
#endif

namespace espnow {

struct LogsReq { uint32_t off; uint16_t max; } __attribute__((packed));

bool SensorRoleAdapter::handleRequest(const EspNowMsg& in, EspNowResp& out){
  switch(in.type){
    case GET_TEMP: {
      float c=0;
      if(S && S->ds18b20) { glue::TempReader<decltype(*S->ds18b20)>::read(S->ds18b20, c); }
      std::memcpy(out.out, &c, sizeof(c)); out.out_len = sizeof(c); return true;
    }
    case GET_TIME: {
      uint32_t t=0; if(S && S->rtc) glue::RtcGet<decltype(*S->rtc)>::get(S->rtc, t);
      std::memcpy(out.out,&t,sizeof(t)); out.out_len=sizeof(t); return true;
    }
    case SET_TIME: {
      if(in.payload_len < 4) return false;
      uint32_t t; std::memcpy(&t, in.payload, 4);
      bool ok = (S && S->rtc) ? glue::RtcSet<decltype(*S->rtc)>::set(S->rtc, t) : false;
      out.out_len = 0; return ok;
    }
    case GET_FAN_MODE: {
      uint8_t m=0; if(S && S->cooling) glue::CoolingGet<decltype(*S->cooling)>::get(S->cooling, m);
      out.out[0]=m; out.out_len=1; return true;
    }
    case SET_FAN_MODE: {
      if(in.payload_len<1) return false;
      uint8_t m=in.payload[0];
      bool ok = (S && S->cooling) ? glue::CoolingSet<decltype(*S->cooling)>::set(S->cooling, m) : false;
      out.out_len=0; return ok;
    }
    case BUZZ_PING: {
      bool ok = (S && S->buzzer) ? glue::BuzzerPing<decltype(*S->buzzer)>::go(S->buzzer) : false;
      out.out_len=0; return ok;
    }
    case LED_PING: {
      bool ok = (S && S->rgb) ? glue::LedPing<decltype(*S->rgb)>::go(S->rgb) : false;
      out.out_len=0; return ok;
    }
    case GET_LOGS: {
      if(!S || !S->logs || in.payload_len < sizeof(LogsReq)) return false;
      LogsReq r{}; std::memcpy(&r, in.payload, sizeof(r));
      size_t n = glue::LogRead<decltype(*S->logs)>::read(S->logs, r.off, out.out, r.max);
      out.out_len = (uint16_t)n; return true;
    }
    case GET_TOPOLOGY: {
      auto* core = EspNowCore::instance();
      if(!core) return false;
      std::vector<uint8_t> tlv; core->exportLocalTopology(tlv);
      if(tlv.size() > 256) return false;
      std::memcpy(out.out, tlv.data(), tlv.size()); out.out_len = (uint16_t)tlv.size();
      return true;
    }

    case GET_TFLUNA_RAW: {
      if(!(S && S->tfluna)){ std::memset(out.out,0,4); out.out_len=4; return true; }
      typename glue::TFLunaGet<decltype(*S->tfluna)>::Raw raw{0,0};
      glue::TFLunaGet<decltype(*S->tfluna)>::get(S->tfluna, raw);
      std::memcpy(out.out,&raw,sizeof(raw)); out.out_len=sizeof(raw); return true;
    }
    case GET_ENV: {
      typename glue::EnvGet<decltype(*S->bme)>::Env env{0,0,0};
      if(S && S->bme) glue::EnvGet<decltype(*S->bme)>::get(S->bme, env);
      std::memcpy(out.out,&env,sizeof(env)); out.out_len=sizeof(env); return true;
    }
    case GET_LUX: {
      uint32_t lux=0; if(S && S->veml) glue::LuxGet<decltype(*S->veml)>::get(S->veml,lux);
      std::memcpy(out.out,&lux,4); out.out_len=4; return true;
    }
    case SET_THRESHOLDS: {
      bool ok = (S && S->sensor) ? glue::SensorSetThresh<decltype(*S->sensor)>::set(S->sensor, in.payload, in.payload_len) : false;
      out.out_len=0; return ok;
    }

    default: return false;
  }
}

} // namespace espnow
