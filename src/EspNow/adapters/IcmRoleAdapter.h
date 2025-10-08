#pragma once
#include "../IRoleAdapter.h"

namespace espnow {

class IcmRoleAdapter final : public IRoleAdapter {
public:
  void mount(const ServiceRefs* s) override { S = s; }
  bool handleRequest(const EspNowMsg& in, EspNowResp& out) override;
  void onTopologyPushed(const uint8_t* tlv, uint16_t len) override;
private:
  const ServiceRefs* S = nullptr;
};

} // namespace espnow
