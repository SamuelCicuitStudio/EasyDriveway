// EspNowStack.h
#pragma once
/**
 * EspNowStack — Radio lifecycle + send/recv engine + token-only admission
 *
 * Responsibilities:
 *  - begin(cfg): init WiFi (STA), set channel/LR, esp_now_init, register callbacks
 *  - Callbacks: recv_cb → enqueue RX; send_cb → optional delivery hook
 *  - loop(router): drain RX → parse → token admission → route; drain TX with retries/ACK waits
 *  - Reliable sends: short ACK window (same seq echoed in reply) + capped retries/backoff
 *
 * Notes:
 *  - Application-level ACK is identified by (mac, seq) equality in a reply.
 *  - We do not parse opcodes for ACKs; any reply echoing seq counts as positive ACK.
 *  - Admission policy: known MAC && enabled && token match (Peers::tokenMatches).
 */

// RX duplicate window (per MAC)
#ifndef NOW_SEQ_WINDOW
#define NOW_SEQ_WINDOW 16
#endif
#ifndef NOW_SEQ_TRACK_MAX
#define NOW_SEQ_TRACK_MAX 16
#endif


#include <stdint.h>
#include <stddef.h>
#include <array>
#include <vector>
#include "EspNowCompat.h"
#include "EspNowPeers.h"
#include "EspNowQueue.h"
#include "EspNowCodec.h"
#include "EspNowAPI.h"

namespace espnow {

class Router; // forward

struct StackCfg {
  uint8_t channel = NOW_DEFAULT_CHANNEL;
  bool    longRange = false;
};

// Default timings (override with build flags if needed)
#ifndef NOW_ACK_TIMEOUT_MS
  #define NOW_ACK_TIMEOUT_MS 30
#endif
#ifndef NOW_MAX_AWAIT
  #define NOW_MAX_AWAIT 16
#endif
#ifndef NOW_MAX_ACKED
  #define NOW_MAX_ACKED 16
#endif

class Stack {
public:
  using SendHook = std::function<void(const uint8_t mac[6], bool ok)>;

  bool begin(const StackCfg& cfg, Peers* peers);
  void end();

  // Enqueue an already-encoded Packet to a target MAC (reliable => expect ACK/reply)
  bool send(const uint8_t mac[6], const Packet& p, bool reliable);

  // Pump RX/TX once; pass a Router to handle accepted packets
  void loop(Router* router);

  // Optional: user send-completion hook (non-reliable: immediate after esp_now_send)
  void onSend(SendHook cb) { _onSend = cb; }

private:
  // esp-now callbacks (C linkage glue)
  static void onRecvStatic(const uint8_t* mac, const uint8_t* data, int len);
  static void onSendStatic(const uint8_t* mac, esp_now_send_status_t status);
  void onRecv(const uint8_t* mac, const uint8_t* data, int len);
  void onSend(const uint8_t* mac, esp_now_send_status_t status);

  // Sequence window per MAC (duplicate rejection)
  struct SeqWinEntry {
    uint8_t  mac[6];
    uint16_t hi;
    uint16_t mask; // bit0=current hi, bit1=hi-1, ... bit15=hi-15
  };
  bool acceptSeq(const uint8_t mac[6], uint16_t seq);
  int  findSeqSlot(const uint8_t mac[6]);

  // ACK tracking
  struct Await {
    uint8_t  mac[6];
    uint16_t seq;
    uint32_t expiresMs;
  };
  struct Acked {
    uint8_t  mac[6];
    uint16_t seq;
    uint32_t tsMs;
  };

  void addAwait(const uint8_t mac[6], uint16_t seq, uint32_t windowMs);
  bool satisfyAwait(const uint8_t mac[6], uint16_t seq); // returns true if a waiter existed
  bool alreadyAcked(const uint8_t mac[6], uint16_t seq) const;
  void reapExpiredAwaits(std::vector<TxItem>& timeoutsOut /*seqs that timed out*/);

private:
  StackCfg   _cfg{};
  Peers*     _peers{nullptr};
  Queue      _q;
  SendHook   _onSend{nullptr};

  // Small rings for outstanding waits and recently-acked pairs
  std::array<Await, NOW_MAX_AWAIT> _await{};
  std::array<Acked, NOW_MAX_ACKED> _acked{};
  std::array<SeqWinEntry, NOW_SEQ_TRACK_MAX> _seqwins{};
};

} // namespace espnow
