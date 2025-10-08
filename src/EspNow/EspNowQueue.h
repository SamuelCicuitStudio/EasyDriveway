// EspNowQueue.h
#pragma once
/**
 * EspNowQueue — RTOS-safe TX/RX queues and simple ACK event channel
 *
 * Role:
 *  - Provide FreeRTOS queues for incoming frames (RX) and outbound requests (TX).
 *  - Carry TX metadata: length, reliable flag, retries left, deadline (ms), seq.
 *  - Provide a lightweight ACK event queue (producer = stack when ACK/response is seen).
 *  - Offer a simple backoff helper (e.g., 10/20/40 ms).
 *
 * Notes:
 *  - Parsing/encoding is handled by EspNowCodec / higher layers.
 *  - Matching ACKs to TX is done in the Stack/Router; this file only transports events.
 */

#include "EspNowCompat.h"
#include <stdint.h>
#include <stddef.h>

namespace espnow {

// Conservative ESPNOW limits
static constexpr size_t NOW_MAX_FRAME_BYTES = 250;

// -------- Frame containers --------
struct RxItem {
  uint8_t mac[6];
  int     rssi;       // 0 if unknown
  uint8_t channel;    // 0 if unknown
  size_t  len;        // bytes in 'raw'
  uint8_t raw[NOW_MAX_FRAME_BYTES];
};

struct TxItem {
  uint8_t  mac[6];
  bool     reliable;     // true => expect ACK/reply; use backoff/retries
  bool     urgent;       // true => route via urgent TX queue
  uint16_t seq;          // sequence (echoed in ACK/reply, if your protocol does that)
  size_t   len;          // bytes in 'raw'
  uint8_t  raw[NOW_MAX_FRAME_BYTES];
  uint8_t  triesLeft;    // remaining attempts (>=1)
  uint32_t deadlineMs;   // when to (re)send next
};

// Optional ACK event emitted by the stack once an ACK/reply is matched
struct AckEvent {
  uint8_t  mac[6];
  uint16_t seq;
  bool     ok;           // true if positively ACKed/matched, false if NAK/failed
};

// -------- Backoff policy --------
// You can override at build time by defining NOW_BACKOFF_MS_{0,1,2}.
#ifndef NOW_BACKOFF_MS_0
  #define NOW_BACKOFF_MS_0 10
#endif
#ifndef NOW_BACKOFF_MS_1
  #define NOW_BACKOFF_MS_1 20
#endif
#ifndef NOW_BACKOFF_MS_2
  #define NOW_BACKOFF_MS_2 40
#endif

class Queue {
public:
  Queue();
  ~Queue();

  bool begin();
  void end();

  // RX queue
  bool pushRx(const RxItem& it);
  bool popRx(RxItem& it, uint32_t timeoutMs);

  // TX queue
  bool pushTx(const TxItem& it);
  bool popTx(TxItem& it, uint32_t timeoutMs);

  // ACK queue (optional; stack pushes when ACK/reply is observed)
  bool pushAck(const AckEvent& ev);
  bool popAck(AckEvent& ev, uint32_t timeoutMs);

  // Backoff helpers
  static uint32_t nextBackoffMs(uint8_t attemptIndex /*0-based*/);
  static bool     rescheduleTxWithBackoff(TxItem& it, uint8_t attemptIndex /*0-based*/) {
    it.deadlineMs = (uint32_t)now_millis() + nextBackoffMs(attemptIndex);
    return true;
  }

private:
  QueueHandle_t _rx  = nullptr;
  QueueHandle_t _tx  = nullptr;
  QueueHandle_t _txUrgent = nullptr;
  QueueHandle_t _ack = nullptr;
};

} // namespace espnow
