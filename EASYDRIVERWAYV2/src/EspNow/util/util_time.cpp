// util/util_time.cpp
// Time & nonce utilities for the ESPNOW stack.

// Be tolerant to header location.
#if __has_include("EspNowStack.h")
  #include "EspNowStack.h"
#else
  #include "EspNow/EspNowStack.h"
#endif

#include <cstdint>
#include <atomic>

#if defined(ESP_PLATFORM)
  #include "esp_timer.h"   // esp_timer_get_time()
#elif defined(ARDUINO)
  #include <Arduino.h>     // millis()
#else
  // desktop / test harness
  #include <chrono>
#endif

namespace espnow {

// --- monotonic ms ------------------------------------------------------------

uint64_t monotonic_ms() {
#if defined(ESP_PLATFORM)
  // microseconds since boot
  return static_cast<uint64_t>(esp_timer_get_time() / 1000ULL);
#elif defined(ARDUINO)
  return static_cast<uint64_t>(millis());
#else
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
#endif
}

// --- epoch ms (fallback to monotonic if no RTC wired yet) --------------------

/* 
 * If you later wire RTCManager into util_time, you can override epoch_ms()
 * to use RTC epoch (wall clock). For now, we return monotonic_ms().
 */
uint64_t epoch_ms() {
  return monotonic_ms();
}

// --- 48-bit nonce helpers ----------------------------------------------------

static inline uint64_t clamp48(uint64_t v) {
  return v & 0xFFFFFFFFFFFFULL; // keep lower 48 bits
}

// Simple, process-local counter. Seeded at startup so multiple boots don’t collide trivially.
static std::atomic<uint64_t> g_nonce{0xA5C3D2B10000ULL};

uint64_t next_nonce() {
  // Increment then clamp to 48 bits.
  uint64_t n = g_nonce.fetch_add(1, std::memory_order_relaxed) + 1;
  return clamp48(n);
}

/*
 * Sliding-window replay guard.
 * - last_nonce: caller-maintained "last accepted" value (48-bit).
 * - nonce: candidate.
 * - window: how far back we still accept (e.g., 32..256). 0 disables back-acceptance.
 * Returns true if acceptable and updates last_nonce.
 */
bool nonce_accept_and_update(uint64_t& last_nonce, uint64_t nonce, uint16_t window) {
  nonce      = clamp48(nonce);
  last_nonce = clamp48(last_nonce);

  // Accept strictly greater nonces
  if (nonce > last_nonce) {
    last_nonce = nonce;
    return true;
  }

  // Optionally accept within a small backward window (handles out-of-order frames).
  if (window > 0) {
    const uint64_t lower = (last_nonce >= window) ? (last_nonce - window) : 0;
    if (nonce >= lower && nonce <= last_nonce) {
      // Do not advance last_nonce on back-window accepts.
      return true;
    }
  }
  return false; // replay/too old
}

} // namespace espnow
