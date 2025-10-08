#pragma once
#include "../IRoleAdapter.h"

namespace espnow {

class PmsRoleAdapter final : public IRoleAdapter {
public:
  void mount(const ServiceRefs* s) override { S = s; }
  bool handleRequest(const EspNowMsg& in, EspNowResp& out) override;
private:
  const ServiceRefs* S = nullptr;
};

} // namespace espnow
