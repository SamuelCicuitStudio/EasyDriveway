// core/espnow_scheduler.cpp
#include "EspNow/EspNowStack.h"
#include <cstring>
#include <cstdint>

#if defined(ARDUINO)
  #include <Arduino.h>
  static uint64_t now_ms() { return (uint64_t)millis(); }
#elif defined(ESP_PLATFORM)
  #include <esp_timer.h>
  static uint64_t now_ms() { return (uint64_t)(esp_timer_get_time() / 1000ULL); }
#else
  #include <chrono>
  static uint64_t now_ms() {
    using namespace std::chrono;
    static const auto t0 = steady_clock::now();
    return (uint64_t)duration_cast<milliseconds>(steady_clock::now() - t0).count();
  }
#endif

namespace espnow {

// ----- radio facade from transport (C-style wrappers) -----
extern "C" {
  bool radio_send(const uint8_t m[6], const uint8_t* data, size_t len);
  uint8_t radio_get_channel();
}

// ----- small opcode pacing table (ms between frames of same opcode) -----
static uint32_t per_opcode_interval_ms(uint8_t mt) {
  switch (mt) {
    case NOW_MT_PING:         return 50;   // can be chatty
    case NOW_MT_PING_REPLY:   return 0;
    case NOW_MT_SENS_REPORT:  return 80;   // up to ~12.5 Hz
    case NOW_MT_RLY_STATE:    return 40;
    case NOW_MT_PMS_STATUS:   return 250;
    case NOW_MT_CTRL_RELAY:   return 60;   // enforce dwell
    case NOW_MT_CONFIG_WRITE: return 120;
    case NOW_MT_TIME_SYNC:    return 500;
    case NOW_MT_NET_SET_CHAN: return 500;
    case NOW_MT_FW_BEGIN:     return 500;
    case NOW_MT_FW_CHUNK:     return 3;    // paced stream
    case NOW_MT_FW_COMMIT:    return 500;
    case NOW_MT_FW_ABORT:     return 200;
    default:                  return 100;
  }
}

// ----- queue entry -----
struct TxItem {
  uint8_t  mac[6];
  uint8_t  msg_type;
  uint8_t  retries_left;
  uint8_t  reserved;
  uint32_t min_interval_ms;
  uint64_t next_earliest_ms;
  uint16_t len;
  uint8_t  buf[256]; // ESPNOW payload shard (already built, including header+auth+payload+sec)
  bool     in_use;
};

static constexpr int QCAP = 8;
static TxItem g_q[QCAP];
static uint8_t g_head = 0, g_tail = 0;
static bool q_full() { return g_q[g_tail].in_use; }

static bool q_push(const uint8_t mac[6], uint8_t msg_type, const uint8_t* bytes, uint16_t len, uint8_t retries) {
  if (len > sizeof(g_q[0].buf)) return false;
  if (q_full()) return false;

  TxItem& it = g_q[g_tail];
  std::memset(&it, 0, sizeof(it));
  std::memcpy(it.mac, mac, 6);
  it.msg_type = msg_type;
  it.retries_left = retries;
  it.min_interval_ms = per_opcode_interval_ms(msg_type);
  it.next_earliest_ms = now_ms(); // ready now
  it.len = len;
  std::memcpy(it.buf, bytes, len);
  it.in_use = true;

  g_tail = (uint8_t)((g_tail + 1) % QCAP);
  return true;
}

static void q_pop() {
  if (!g_q[g_head].in_use) return;
  g_q[g_head].in_use = false;
  g_head = (uint8_t)((g_head + 1) % QCAP);
}

static TxItem* q_front() {
  if (!g_q[g_head].in_use) return nullptr;
  return &g_q[g_head];
}

// Public scheduling API (used by espnow_core)
bool sched_enqueue(const uint8_t mac[6], uint8_t msg_type, const uint8_t* bytes, uint16_t len, uint8_t retries) {
  return q_push(mac, msg_type, bytes, len, retries);
}

// Tick: send head if interval passed; on send failure, retry with small backoff
void sched_tick() {
  TxItem* it = q_front();
  if (!it) return;

  const uint64_t t = now_ms();
  if (t < it->next_earliest_ms) return;

  const bool ok = radio_send(it->mac, it->buf, it->len);
  if (ok) {
    // throttle by opcode to avoid flooding
    it->next_earliest_ms = t + it->min_interval_ms;
    // pop immediately since esp-now send is async; we assume best-effort.
    q_pop();
  } else {
    if (it->retries_left == 0) {
      q_pop(); // give up
      return;
    }
    it->retries_left--;
    // simple exponential-ish backoff
    uint32_t backoff = 10 + (per_opcode_interval_ms(it->msg_type) >> 1);
    it->next_earliest_ms = t + backoff;
  }
}

} // namespace espnow
