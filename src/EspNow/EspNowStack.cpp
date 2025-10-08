// EspNowStack.cpp
#include "EspNowStack.h"
#include "EspNowRouter.h"   // Router::route(mac, pkt, stack)
#include <string.h>

namespace espnow {

static Stack* g_now = nullptr; // singleton binding for C callbacks

// --------------------- Public ---------------------
bool Stack::begin(const StackCfg& cfg, Peers* peers) {
  _cfg = cfg; _peers = peers;

  if (!_q.begin()) return false;

  // Bring up WiFi radio in STA and set channel/LR
  now_wifi_sta_mode();
  now_wifi_set_channel(_cfg.channel);
  now_enable_long_range(_cfg.longRange, WIFI_IF_STA);

  if (esp_now_init() != ESP_OK) return false;
  g_now = this;

  // Register callbacks (Arduino signature; RSSI not available here)
  esp_now_register_recv_cb([](const uint8_t* mac, const uint8_t* data, int len){
    if (g_now) g_now->onRecv(mac, data, len);
  });
  esp_now_register_send_cb([](const uint8_t* mac, esp_now_send_status_t status){
    if (g_now) g_now->onSend(mac, status);
  });

  // Ensure radio peer table contains all enabled peers (safety pass)
  for (const auto& p : _peers->all()) {
    if (!p.enabled) continue;
    esp_now_peer_info_t info{};
    memcpy(info.peer_addr, p.mac, 6);
    info.ifidx   = WIFI_IF_STA;
    info.encrypt = false;
    info.channel = _cfg.channel;
    if (esp_now_is_peer_exist(info.peer_addr)) esp_now_del_peer(info.peer_addr);
    esp_now_add_peer(&info);
  }

  return true;
}

void Stack::end() {
  esp_now_deinit();
  _q.end();
  g_now = nullptr;
}

bool Stack::send(const uint8_t mac[6], const Packet& p, bool reliable) {
  TxItem it{};
  memcpy(it.mac, mac, 6);
  it.reliable  = reliable;
  it.urgent    = (p.hdr && (p.hdr->flags & NOW_FLAGS_URGENT));
  it.seq       = p.hdr ? p.hdr->seq : 0;
  it.len       = p.len;
  memcpy(it.raw, p.buf, p.len);
  it.triesLeft = reliable ? 3 : 1;
  it.deadlineMs = (uint32_t)now_millis();  // fire ASAP
  return _q.pushTx(it);
}

void Stack::loop(Router* router) {
  // ---------- RX ----------
  RxItem rx;
  while (_q.popRx(rx, 0)) {
    Packet in{};
    auto pr = parse(rx.raw, (uint16_t)rx.len, in);
    if (pr != PARSE_OK) {
      // TODO: log parse error pr
      continue;
    }

    // Sanity: sender role range
    if (in.hdr->sender_role > NOW_ROLE_SENS) {
      continue;
    }

    // Sequence window (duplicate rejection, small window per MAC)
    if (!acceptSeq(rx.mac, in.hdr->seq)) {
      continue;
    }

    // Admission: known MAC && enabled && token match
    const Peer* peer = _peers->findByMac(rx.mac);
    if (!peer || !peer->enabled) {
      // TODO: log E_TOKEN (unknown/disabled)
      continue;
    }
    if (memcmp(peer->token, in.dev->bytes, 16) != 0) {
      // TODO: log E_TOKEN (token mismatch)
      continue;
    }
    // Topology Token equality (iff HAS_TOPO)
    if ((in.hdr->flags & NOW_FLAGS_HAS_TOPO)) {
      uint8_t tok[16];
      if (!in.topo) { continue; }
      if (!_peers->getTopoToken(tok) || memcmp(tok, in.topo->bytes, 16) != 0) {
        continue;
      }
    }

    // If this is a reply echoing a pending seq, mark ACK success
    satisfyAwait(rx.mac, in.hdr->seq);

    // Hand off to the router for domain handling; router may send replies via this stack
    if (router) router->route(rx.mac, in, *this);
  }

  // ---------- TX ----------
  // We'll collect items that are not yet due and push them back once per cycle to avoid busy loops.
  std::vector<TxItem> carry;
  TxItem tx;
  const uint32_t now = (uint32_t)now_millis();

  while (_q.popTx(tx, 0)) {
    // Reliable: if already ACKed while we waited, complete success and drop
    if (tx.reliable && alreadyAcked(tx.mac, tx.seq)) {
      if (_onSend) _onSend(tx.mac, true);
      continue;
    }

    // Respect deadlines (ACK wait windows / backoff windows)
    if (tx.deadlineMs > now) {
      carry.push_back(tx);
      continue;
    }

    // Time to (re)send or finalize (timeout case after ACK wait)
    if (tx.reliable) {
      // If this item is waiting for ACK and deadline hit, we treat it as timeout -> retry/backoff
      // Send attempt
      esp_err_t e = esp_now_send(tx.mac, tx.raw, (int)tx.len);
      if (e == ESP_OK) {
        // Start a short ACK window; we don't decrement tries yet—only when the ACK window expires.
        addAwait(tx.mac, tx.seq, NOW_ACK_TIMEOUT_MS);
        // Requeue same item to check ACK later (or retry with backoff)
        carry.push_back(tx);
        // Next deadline becomes ACK timeout; tries decremented when window expires
        carry.back().deadlineMs = (uint32_t)now_millis() + NOW_ACK_TIMEOUT_MS;
      } else {
        // Immediate send failure: retry/backoff or give up
        if (tx.triesLeft > 1) {
          tx.triesLeft--;
          Queue::rescheduleTxWithBackoff(tx, 0 /*first backoff slot for immediate error*/);
          carry.push_back(tx);
        } else {
          if (_onSend) _onSend(tx.mac, false);
        }
      }
    } else {
      // Non-reliable: fire and forget
      esp_err_t e = esp_now_send(tx.mac, tx.raw, (int)tx.len);
      if (_onSend) _onSend(tx.mac, e == ESP_OK);
      // Nothing to carry
    }
  }

  // Handle ACK timeouts: any awaiting whose window expired should drive retries for matching TX items
  std::vector<TxItem> timeouts; // just to trigger "decrement tries and backoff" on matching TX
  reapExpiredAwaits(timeouts);

  // For every timed-out (mac,seq), find corresponding TX in carry and convert that deadline to a backoff retry
  for (auto& timed : timeouts) {
    // We only have (mac,seq) in Await; synthesize a probe to adjust any matching item in carry.
    for (auto& c : carry) {
      if (c.reliable && c.seq == timed.seq && now_same_mac(c.mac, timed.mac)) {
        if (c.triesLeft > 1) {
          c.triesLeft--;
          Queue::rescheduleTxWithBackoff(c, 1 /*advance backoff slot*/);
        } else {
          // Out of retries → final failure
          if (_onSend) _onSend(c.mac, false);
          // Mark this c as completed by setting triesLeft=0 and avoid requeue
          c.triesLeft = 0;
        }
      }
    }
  }

  // Requeue all pending items that still have work to do
  for (auto& c : carry) {
    if (!c.reliable) {
      // shouldn't happen (non-reliable never carried), but keep safe
      _q.pushTx(c);
      continue;
    }
    if (alreadyAcked(c.mac, c.seq)) {
      if (_onSend) _onSend(c.mac, true);
      continue;
    }
    if (c.triesLeft == 0) continue; // finalized as failure above
    _q.pushTx(c);
  }
}

// --------------------- Callbacks ---------------------
void Stack::onRecv(const uint8_t* mac, const uint8_t* data, int len) {
  RxItem it{};
  memcpy(it.mac, mac, 6);
  it.rssi = 0;        // Not available in Arduino 3-arg callback
  it.channel = _cfg.channel;
  it.len = (size_t)len;
  if (it.len > sizeof(it.raw)) it.len = sizeof(it.raw);
  memcpy(it.raw, data, it.len);
  _q.pushRx(it);
}

void Stack::onSend(const uint8_t* mac, esp_now_send_status_t status) {
  // Transport-level completion; application-level success is decided by ACK reply
  if (_onSend && status != ESP_NOW_SEND_SUCCESS) {
    _onSend(mac, false); // immediate transport failure
  }
}

void Stack::onRecvStatic(const uint8_t* mac, const uint8_t* data, int len) {
  if (g_now) g_now->onRecv(mac, data, len);
}
void Stack::onSendStatic(const uint8_t* mac, esp_now_send_status_t status) {
  if (g_now) g_now->onSend(mac, status);
}

// --------------------- Await / Ack rings ---------------------
void Stack::addAwait(const uint8_t mac[6], uint16_t seq, uint32_t windowMs) {
  // Replace oldest/expired slot
  uint32_t now = (uint32_t)now_millis();
  size_t idx = 0; uint32_t oldest = 0;
  for (size_t i=0;i<_await.size();++i) {
    if (_await[i].expiresMs == 0 || _await[i].expiresMs <= now) { idx = i; break; }
    if (_await[i].expiresMs > oldest) { oldest = _await[i].expiresMs; idx = i; }
  }
  memcpy(_await[idx].mac, mac, 6);
  _await[idx].seq = seq;
  _await[idx].expiresMs = now + windowMs;
}

bool Stack::satisfyAwait(const uint8_t mac[6], uint16_t seq) {
  // Mark as ACKed and push into "recent" ack ring
  for (auto& a : _await) {
    if (a.expiresMs == 0) continue;
    if (a.seq == seq && now_same_mac(a.mac, mac)) {
      a.expiresMs = 0;
      // record into acked
      size_t j=0; uint32_t oldest=UINT32_MAX;
      for (size_t i=0;i<_acked.size();++i) {
        if (_acked[i].tsMs == 0) { j=i; break; }
        if (_acked[i].tsMs < oldest) { oldest=_acked[i].tsMs; j=i; }
      }
      memcpy(_acked[j].mac, mac, 6);
      _acked[j].seq = seq;
      _acked[j].tsMs = (uint32_t)now_millis();
      // Also emit an AckEvent for anyone listening on Queue::ACK
      AckEvent ev{}; memcpy(ev.mac, mac, 6); ev.seq = seq; ev.ok = true;
      _q.pushAck(ev);
      return true;
    }
  }
  return false;
}

bool Stack::alreadyAcked(const uint8_t mac[6], uint16_t seq) const {
  for (const auto& a : _acked) {
    if (a.tsMs == 0) continue;
    if (a.seq == seq && now_same_mac(a.mac, mac)) return true;
  }
  return false;
}

void Stack::reapExpiredAwaits(std::vector<TxItem>& timeoutsOut) {
  uint32_t now = (uint32_t)now_millis();
  for (auto& a : _await) {
    if (a.expiresMs && a.expiresMs <= now) {
      // Emit timeout AckEvent (negative) and produce a TxItem probe with (mac,seq)
      AckEvent ev{}; memcpy(ev.mac, a.mac, 6); ev.seq = a.seq; ev.ok = false;
      _q.pushAck(ev);

      TxItem probe{}; memcpy(probe.mac, a.mac, 6); probe.seq = a.seq;
      timeoutsOut.push_back(probe);

      a.expiresMs = 0; // clear
    }
  }
}


// --------- Sequence window helpers (16-bit seq with wrap) ---------
int Stack::findSeqSlot(const uint8_t mac[6]) {
  int freeIdx = -1;
  for (size_t i=0;i<_seqwins.size();++i) {
    if (_seqwins[i].mac[0]==0 && _seqwins[i].mac[1]==0 && _seqwins[i].mac[2]==0 &&
        _seqwins[i].mac[3]==0 && _seqwins[i].mac[4]==0 && _seqwins[i].mac[5]==0) {
      if (freeIdx < 0) freeIdx = (int)i;
      continue;
    }
    if (now_same_mac(_seqwins[i].mac, mac)) return (int)i;
  }
  if (freeIdx >= 0) {
    now_copy_mac(_seqwins[freeIdx].mac, mac);
    _seqwins[freeIdx].hi = 0;
    _seqwins[freeIdx].mask = 0;
    return freeIdx;
  }
  // Evict slot 0 if table full (small table policy)
  now_copy_mac(_seqwins[0].mac, mac);
  _seqwins[0].hi = 0;
  _seqwins[0].mask = 0;
  return 0;
}

static inline bool seq_greater(uint16_t a, uint16_t b) {
  return (uint16_t)(a - b) != 0 && (uint16_t)(a - b) < 0x8000;
}

bool Stack::acceptSeq(const uint8_t mac[6], uint16_t seq) {
  int idx = findSeqSlot(mac);
  auto &e = _seqwins[idx];
  if (e.mask == 0) {
    e.hi = seq;
    e.mask = 1u; // mark hi seen
    return true;
  }
  if (!seq_greater(seq, e.hi)) {
    // seq <= hi (modulo): check window
    uint16_t back = (uint16_t)(e.hi - seq);
    if (back >= NOW_SEQ_WINDOW) return false; // too old
    uint16_t bit = (uint16_t)(1u << back);
    if (e.mask & bit) return false; // duplicate
    e.mask |= bit;
    return true;
  } else {
    // seq ahead of hi
    uint16_t diff = (uint16_t)(seq - e.hi);
    if (diff >= NOW_SEQ_WINDOW) {
      e.mask = 1u; // new hi; previous window discarded
    } else {
      e.mask = (uint16_t)((e.mask << diff) | 1u);
    }
    e.hi = seq;
    return true;
  }
}
} // namespace espnow
