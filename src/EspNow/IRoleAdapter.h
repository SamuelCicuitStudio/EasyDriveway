#pragma once
#include "Frame.h"
#include "ServiceRefs.h"

namespace espnow {

class IRoleAdapter {
public:
  virtual ~IRoleAdapter() = default;
  virtual void mount(const ServiceRefs* s) = 0;
  virtual bool handleRequest(const EspNowMsg& in, EspNowResp& out) = 0;
  virtual void onTopologyPushed(const uint8_t* tlv, uint16_t len) {}
};

} // namespace espnow
