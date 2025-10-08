#pragma once
/**
 * EspNowRouter — dispatch incoming packets to the registered role adapter.
 *
 * Purpose:
 *  - Keep a small registry: role -> IRoleAdapter*
 *  - Validate simple policy (e.g., topology-bound ops require HAS_TOPO)
 *  - Call adapter->handle(); if it returns true, queue reply via Stack
 *
 * Notes:
 *  - Typically you register only ONE adapter for the local device role.
 *  - We do not enforce dstRole because v3-T header has sender_role only;
 *    instead we route to the adapter registered for our local role.
 */

#include <array>
#include <stdint.h>

#include "EspNowAPI.h"     // message/flags catalog (v3-T)
#include "EspNowCodec.h"   // Packet views
#include "EspNowCompat.h"  // NOW_ALWAYS_INLINE
#include "IRoleAdapter.h"  // single source of truth for the adapter interface

namespace espnow {

// Forward declaration to avoid tight coupling
class Stack;

// Result codes for route()
enum class RouteResult : uint8_t {
  OK = 0,
  NO_ADAPTER,
  POLICY,        // e.g., topo-required but HAS_TOPO missing
  UNIMPLEMENTED, // adapter returned false
};

class Router {
public:
  static constexpr size_t kMaxRoles = 8;

  Router() { _adapters.fill(nullptr); }

  void setLocalRole(uint8_t role) { _localRole = role; }
  void registerAdapter(uint8_t role, IRoleAdapter* a) {
    if (role < _adapters.size()) _adapters[role] = a;
  }
  IRoleAdapter* adapterFor(uint8_t role) const {
    return (role < _adapters.size()) ? _adapters[role] : nullptr;
  }

  // Route one incoming packet. If adapter returns a reply, send it via 'stack'.
  RouteResult route(const uint8_t srcMac[6], const Packet& in, Stack& stack);

private:
  static bool requiresTopo(uint8_t msg_type);
  static bool allowedByRole(uint8_t msg_type, uint8_t sender_role);

private:
  std::array<IRoleAdapter*, kMaxRoles> _adapters{};
  uint8_t _localRole{0};

  // Bitset to log each unimplemented opcode once (256 ops max)
  uint8_t _seenUnimpl[32]{}; // 256 bits
  void markUnimpl(uint8_t op) { _seenUnimpl[op >> 3] |=  (uint8_t)(1u << (op & 7)); }
  bool seenUnimpl(uint8_t op) const { return (_seenUnimpl[op >> 3] & (uint8_t)(1u << (op & 7))) != 0; }
};

} // namespace espnow
