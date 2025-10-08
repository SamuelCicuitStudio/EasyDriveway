#pragma once
#include <cstdint>

namespace espnow {

struct DeviceInfo {
  char     deviceId[32]  = {0};
  char     deviceName[32]= {0};
  char     hwVersion[16] = {0};
  char     swVersion[16] = {0};
  uint8_t  deviceType    = 0;
  uint8_t  role          = 0; // RoleCode
};

} // namespace espnow
