#pragma once
/**
 * IRoleAdapter — contract for role-specific handlers (stable ABI).
 *
 * Notes:
 *  - No stack internals leak here.
 *  - Router will pass source MAC for policy/telemetry; adapters may ignore it.
 */

#include <stdint.h>

// Be tolerant to different include layouts (root vs "EspNow/" folder)
#if __has_include("EspNowAPI.h")
  #include "EspNowAPI.h"
#elif __has_include("EspNowAPI.h")
  #include "EspNow/EspNowAPI.h"
#else
  #error "EspNowAPI.h not found. Adjust include path."
#endif

#if __has_include("EspNowCodec.h")
  #include "EspNowCodec.h"
#elif __has_include("EspNow/EspNowCodec.h")
  #include "EspNow/EspNowCodec.h"
#else
  #error "EspNowCodec.h not found. Adjust include path."
#endif

namespace espnow {

struct IRoleAdapter {
  virtual ~IRoleAdapter() = default;
  virtual uint8_t role() const = 0;

  // Handle one incoming, optionally produce a reply in 'out'.
  // Return true if 'out' is populated and should be sent.
  virtual bool handle(const uint8_t srcMac[6], const Packet& in, Packet& out) = 0;

  // Periodic housekeeping (timers, pulses, etc.).
  virtual void tick() {}
};

} // namespace espnow
