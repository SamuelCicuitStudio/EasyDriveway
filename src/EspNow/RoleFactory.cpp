#include "RoleFactory.h"
#include "IRoleAdapter.h"
#include "TopologyTlv.h"

#ifdef __has_include
  #if __has_include("../config/SetRole.h")
    #include "../config/SetRole.h"
  #elif __has_include("config/SetRole.h")
    #include "config/SetRole.h"
  #endif
#endif

#include "adapters/IcmRoleAdapter.h"
#include "adapters/PmsRoleAdapter.h"
#include "adapters/SensorRoleAdapter.h"
#include "adapters/RelayRoleAdapter.h"
#include "adapters/SensorEmuRoleAdapter.h"
#include "adapters/RelayEmuRoleAdapter.h"

namespace espnow {

IRoleAdapter* createRoleAdapter(){
#if defined(ROLE_ICM)
  static IcmRoleAdapter a; return &a;
#elif defined(ROLE_PMS)
  static PmsRoleAdapter a; return &a;
#elif defined(ROLE_SENSOR)
  static SensorRoleAdapter a; return &a;
#elif defined(ROLE_RELAY)
  static RelayRoleAdapter a; return &a;
#elif defined(ROLE_SENSOR_EMU)
  static SensorEmuRoleAdapter a; return &a;
#elif defined(ROLE_RELAY_EMU)
  static RelayEmuRoleAdapter a; return &a;
#else
  #warning "No ROLE_* macro defined in SetRole.*; defaulting to SENSOR"
  static SensorRoleAdapter a; return &a;
#endif
}

uint8_t getLocalRoleCode(){
#if defined(ROLE_ICM)
  return RC_ICM;
#elif defined(ROLE_PMS)
  return RC_PMS;
#elif defined(ROLE_SENSOR)
  return RC_SENSOR;
#elif defined(ROLE_RELAY)
  return RC_RELAY;
#elif defined(ROLE_SENSOR_EMU)
  return RC_SEN_EMU;
#elif defined(ROLE_RELAY_EMU)
  return RC_REL_EMU;
#else
  return RC_SENSOR;
#endif
}

} // namespace espnow
