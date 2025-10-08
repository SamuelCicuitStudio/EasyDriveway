// EspNowRouter.cpp
#include "EspNowRouter.h"
#include "EspNowStack.h"   // Stack::send(..)
#include <string.h>

#ifndef NOW_ROUTER_LOG
  #define NOW_ROUTER_LOG(...)  do{}while(0)
#endif

namespace espnow {

bool Router::requiresTopo(uint8_t msg_type) {
  switch (msg_type) {
    case NOW_MSG_CTRL_RELAY:    // physical topology sensitive
    case NOW_MSG_CONFIG_WRITE:  // topology-scoped configuration
    case NOW_MSG_TOPO_PUSH:     // explicit topology ops
      return true;
    default:
      return false;
  }
}


static bool is_fw_op(uint8_t t) {
  return t >= NOW_MSG_FW_BEGIN && t <= NOW_MSG_FW_ABORT;
}

bool Router::allowedByRole(uint8_t msg_type, uint8_t sender_role) {
  // ICM-only ops
  if (msg_type == NOW_MSG_TOPO_PUSH || msg_type == NOW_MSG_NET_SET_CHAN ||
      msg_type == NOW_MSG_TIME_SYNC || is_fw_op(msg_type)) {
    return sender_role == NOW_ROLE_ICM;
  }
  // Always allowed by role
  if (msg_type == NOW_MSG_SENS_REPORT) {
    return sender_role == NOW_ROLE_SENS || sender_role == NOW_ROLE_SEMU;
  }
  if (msg_type == NOW_MSG_PMS_STATUS) {
    return sender_role == NOW_ROLE_PMS;
  }
  // Otherwise allowed (subject to HAS_TOPO policy elsewhere)
  return true;
}
RouteResult Router::route(const uint8_t srcMac[6], const Packet& in, Stack& stack) {
  // Find adapter for our local role
  IRoleAdapter* a = adapterFor(_localRole);
  if (!a) {
    NOW_ROUTER_LOG("[NOW][ROUTER] No adapter for local role=%u\n", (unsigned)_localRole);
    return RouteResult::NO_ADAPTER;
  }

  // Privilege gate: sender_role must be allowed for this msg_type
  if (!allowedByRole(in.hdr->msg_type, in.hdr->sender_role)) {
    NOW_ROUTER_LOG("[NOW][ROUTER] Privilege reject role=%u op=0x%02X\n", (unsigned)in.hdr->sender_role, (unsigned)in.hdr->msg_type);
    return RouteResult::POLICY;
  }

  // Policy: topology-bound ops must carry HAS_TOPO
  if (requiresTopo(in.hdr->msg_type) && !(in.hdr->flags & NOW_FLAGS_HAS_TOPO)) {
    NOW_ROUTER_LOG("[NOW][ROUTER] Policy reject (HAS_TOPO missing) for op=0x%02X\n", (unsigned)in.hdr->msg_type);
    return RouteResult::POLICY;
  }

  // Hand off to the adapter
  Packet out{};
  const bool hasReply = a->handle(srcMac, in, out);
  if (!hasReply) {
    if (!seenUnimpl(in.hdr->msg_type)) {
      markUnimpl(in.hdr->msg_type);
      NOW_ROUTER_LOG("[NOW][ROUTER] Unimplemented op=0x%02X by role=%u\n",
                     (unsigned)in.hdr->msg_type, (unsigned)a->role());
    }
    return RouteResult::UNIMPLEMENTED;
  }

  // Send reply if adapter returned one
  if (out.len > 0 && out.hdr) {
    const bool reliable = (out.hdr->flags & NOW_FLAGS_RELIABLE) != 0;
    stack.send(srcMac, out, reliable);
  }
  return RouteResult::OK;
}

} // namespace espnow
