
#include "SensEspNow.h"
#include <string.h>

bool SensorEspNowManager::sendRelayRawByPos(int8_t relPos, CmdDomain dom, uint8_t op, const void* body, size_t blen, bool requireAck){
  RelayPeer rp;
  if (!findRelayByPos_(relPos, rp)) return false;
  ensurePeer_(rp.mac);
  return sendToMacWithToken_(rp.mac, rp.tok16, dom, op, body, blen, requireAck);
}
bool SensorEspNowManager::sendRelayRawByIdx(uint8_t relayIdx, CmdDomain dom, uint8_t op, const void* body, size_t blen, bool requireAck){
  RelayPeer rp;
  if (!findRelayByIdx_(relayIdx, rp)) return false;
  ensurePeer_(rp.mac);
  return sendToMacWithToken_(rp.mac, rp.tok16, dom, op, body, blen, requireAck);
}

// Convenience wrappers declared in header (not present in original .cpp):
bool SensorEspNowManager::sendRelayOnForByPos(int8_t relPos, uint8_t chMask, uint16_t on_ms,
                          uint16_t delay_ms, uint16_t ttl_ms, bool requireAck){
  RelayPeer rp;
  if (!findRelayByPos_(relPos, rp)) return false;
  RelOnForPayload p{}; p.ver=1; p.chMask=chMask; p.on_ms=on_ms; p.delay_ms=delay_ms; p.ttl_ms=ttl_ms;
  ensurePeer_(rp.mac);
  return sendToMacWithToken_(rp.mac, rp.tok16, CmdDomain::RELAY, REL_ON_FOR, &p, sizeof(p), requireAck);
}
bool SensorEspNowManager::sendRelayOnForByIdx(uint8_t relayIdx, uint8_t chMask, uint16_t on_ms,
                          uint16_t delay_ms, uint16_t ttl_ms, bool requireAck){
  RelayPeer rp;
  if (!findRelayByIdx_(relayIdx, rp)) return false;
  RelOnForPayload p{}; p.ver=1; p.chMask=chMask; p.on_ms=on_ms; p.delay_ms=delay_ms; p.ttl_ms=ttl_ms;
  ensurePeer_(rp.mac);
  return sendToMacWithToken_(rp.mac, rp.tok16, CmdDomain::RELAY, REL_ON_FOR, &p, sizeof(p), requireAck);
}

// High-level: moving wave fanout (1-2-3 or 3-2-1) with optional all-on flash
bool SensorEspNowManager::playWave(uint8_t lane, int8_t dir, uint16_t speed_mmps,
                                   uint16_t spacing_mm, uint16_t on_ms,
                                   uint16_t all_on_ms, uint16_t ttl_ms, bool requireAck){
  if (!hasTopology()) return false;
  if (lane > 1) lane = 0;
  if (dir == 0) dir = +1;
  if (speed_mmps == 0) speed_mmps = 1000;
  if (spacing_mm == 0) spacing_mm = 1500;
  if (on_ms == 0) on_ms = 150;

  uint8_t chMask = (lane == 0) ? REL_CH_LEFT : REL_CH_RIGHT;

  uint32_t step_ms = (uint32_t)((uint64_t)spacing_mm * 1000ULL / (uint64_t)speed_mmps);
  if (step_ms < 80)  step_ms = 80;
  if (step_ms > 300) step_ms = 300;

  size_t N = (dir > 0) ? relPos_.size() : relNeg_.size();
  uint32_t total_ms = (uint32_t)all_on_ms + (uint32_t)(N ? (N-1) : 0) * step_ms + on_ms + 400;
  if (ttl_ms == 0) ttl_ms = (uint16_t)(total_ms > 0xFFFF ? 0xFFFF : total_ms);

  if (all_on_ms > 0) {
    if (dir > 0) {
      for (size_t i=0; i<relPos_.size(); ++i) {
        const auto& rp = relPos_[i];
        RelOnForPayload p{}; p.ver=1; p.chMask=chMask; p.on_ms=all_on_ms; p.delay_ms=0; p.ttl_ms=ttl_ms;
        ensurePeer_(rp.mac);
        sendToMacWithToken_(rp.mac, rp.tok16, CmdDomain::RELAY, REL_ON_FOR, &p, sizeof(p), requireAck);
      }
    } else {
      for (size_t i=0; i<relNeg_.size(); ++i) {
        const auto& rp = relNeg_[i];
        RelOnForPayload p{}; p.ver=1; p.chMask=chMask; p.on_ms=all_on_ms; p.delay_ms=0; p.ttl_ms=ttl_ms;
        ensurePeer_(rp.mac);
        sendToMacWithToken_(rp.mac, rp.tok16, CmdDomain::RELAY, REL_ON_FOR, &p, sizeof(p), requireAck);
      }
    }
  }

  uint32_t baseDelay = all_on_ms;
  if (dir > 0) {
    for (size_t i=0; i<relPos_.size(); ++i) {
      const auto& rp = relPos_[i];
      uint32_t dly = baseDelay + (uint32_t)i * step_ms;
      RelOnForPayload p{}; p.ver=1; p.chMask=chMask; p.on_ms=on_ms;
      p.delay_ms = (uint16_t)(dly > 0xFFFF ? 0xFFFF : dly);
      p.ttl_ms   = ttl_ms;
      ensurePeer_(rp.mac);
      sendToMacWithToken_(rp.mac, rp.tok16, CmdDomain::RELAY, REL_ON_FOR, &p, sizeof(p), requireAck);
    }
  } else {
    for (size_t ri=0; ri<relNeg_.size(); ++ri) {
      size_t i = relNeg_.size() - 1 - ri;
      const auto& rp = relNeg_[i];
      uint32_t dly = baseDelay + (uint32_t)ri * step_ms;
      RelOnForPayload p{}; p.ver=1; p.chMask=chMask; p.on_ms=on_ms;
      p.delay_ms = (uint16_t)(dly > 0xFFFF ? 0xFFFF : dly);
      p.ttl_ms   = ttl_ms;
      ensurePeer_(rp.mac);
      sendToMacWithToken_(rp.mac, rp.tok16, CmdDomain::RELAY, REL_ON_FOR, &p, sizeof(p), requireAck);
    }
  }
  return true;
}
