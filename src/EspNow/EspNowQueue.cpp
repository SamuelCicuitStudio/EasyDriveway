// EspNowQueue.cpp
#include "EspNowQueue.h"

namespace espnow {

Queue::Queue() {}
Queue::~Queue() { end(); }

bool Queue::begin() {
  // Depth can be tuned via NOW_RX_Q_DEPTH / NOW_TX_Q_DEPTH; use same for ACKs.
  _rx  = xQueueCreate(NOW_RX_Q_DEPTH,  sizeof(RxItem));
  _tx  = xQueueCreate(NOW_TX_Q_DEPTH,  sizeof(TxItem));
  _txUrgent = xQueueCreate(NOW_TX_Q_DEPTH,  sizeof(TxItem));
  _ack = xQueueCreate(NOW_TX_Q_DEPTH,  sizeof(AckEvent));
  return _rx && _tx && _txUrgent && _ack;
}

void Queue::end() {
  if (_rx)  { vQueueDelete(_rx);  _rx  = nullptr; }
  if (_tx)  { vQueueDelete(_tx);  _tx  = nullptr; }
  if (_txUrgent) { vQueueDelete(_txUrgent); _txUrgent = nullptr; }
  if (_ack) { vQueueDelete(_ack); _ack = nullptr; }
}

// ---------------- RX ----------------
bool Queue::pushRx(const RxItem& it) {
  if (!_rx) return false;
  return xQueueSend(_rx, &it, 0) == pdTRUE;
}

bool Queue::popRx(RxItem& it, uint32_t timeoutMs) {
  if (!_rx) return false;
  return xQueueReceive(_rx, &it, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}

// ---------------- TX ----------------
bool Queue::pushTx(const TxItem& it) {
  // Route by urgency
  QueueHandle_t q = (it.urgent && _txUrgent) ? _txUrgent : _tx;
  if (!q) return false;
  return xQueueSend(q, &it, 0) == pdTRUE;
}

bool Queue::popTx(TxItem& it, uint32_t timeoutMs) {
  // Try urgent first (non-blocking), then normal with timeout
  if (_txUrgent && xQueueReceive(_txUrgent, &it, 0) == pdTRUE) return true;
  if (!_tx) return false;
  return xQueueReceive(_tx, &it, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}

// ---------------- ACK ----------------
bool Queue::pushAck(const AckEvent& ev) {
  if (!_ack) return false;
  return xQueueSend(_ack, &ev, 0) == pdTRUE;
}

bool Queue::popAck(AckEvent& ev, uint32_t timeoutMs) {
  if (!_ack) return false;
  return xQueueReceive(_ack, &ev, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}

// ---------------- Backoff ----------------
uint32_t Queue::nextBackoffMs(uint8_t attemptIndex) {
  // attemptIndex: 0 => first retry window, 1 => second, etc.
  switch (attemptIndex) {
    case 0:  return NOW_BACKOFF_MS_0;
    case 1:  return NOW_BACKOFF_MS_1;
    default: return NOW_BACKOFF_MS_2;
  }
}

} // namespace espnow
