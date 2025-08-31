/**************************************************************
 *  ESPNowManager_topology.cpp — Topology: serialize/parse, setters, pushers
 *  ArduinoJson v7 compatible (uses JsonArrayConst / JsonObjectConst when reading)
 **************************************************************/
#include "ESPNowManager.h"
#include "CommandAPI.h"

#include <ArduinoJson.h>
#include <esp_heap_caps.h>

// ======= Serialize / Export =======
String ESPNowManager::serializeTopology() const {
  // Emit: { "links": { "zc": [...], "boundaries": [...] } }
  DynamicJsonDocument doc(8192);
  JsonObject links = doc.createNestedObject("links");

  // --- Zero-centered sensors ---
  JsonArray zc = links.createNestedArray("zc");
  for (uint8_t s = 0; s < ICM_MAX_SENSORS + 2; ++s) {
    const auto& z = _zcSensors[s];
    if (!z.used) continue;

    JsonObject o = zc.createNestedObject();
    o["sensIdx"] = z.sensIdx;
    o["hasPrev"] = z.hasPrev;
    if (z.hasPrev) {
      o["prevIdx"] = z.prevSensIdx;
      o["prevMac"] = macBytesToStr(z.prevSensMac);
    }
    o["hasNext"] = z.hasNext;
    if (z.hasNext) {
      o["nextIdx"] = z.nextSensIdx;
      o["nextMac"] = macBytesToStr(z.nextSensMac);
    }

    JsonArray neg = o.createNestedArray("neg");
    for (uint8_t i = 0; i < z.nNeg; ++i) {
      JsonObject e = neg.createNestedObject();
      e["relayIdx"] = z.neg[i].relayIdx;
      e["pos"]      = z.neg[i].relPos;
      e["relayMac"] = macBytesToStr(z.neg[i].relayMac);
    }

    JsonArray pos = o.createNestedArray("pos");
    for (uint8_t i = 0; i < z.nPos; ++i) {
      JsonObject e = pos.createNestedObject();
      e["relayIdx"] = z.pos[i].relayIdx;
      e["pos"]      = z.pos[i].relPos;
      e["relayMac"] = macBytesToStr(z.pos[i].relayMac);
    }
  }

  // --- Relay boundaries ---
  JsonArray bounds = links.createNestedArray("boundaries");
  for (uint8_t r = 0; r < ICM_MAX_RELAYS; ++r) {
    const auto& b = _boundaries[r];
    if (!b.used) continue;

    JsonObject o = bounds.createNestedObject();
    o["relayIdx"]  = b.relayIdx;
    o["splitRule"] = b.splitRule;

    o["hasA"] = b.hasA;
    if (b.hasA) {
      o["aIdx"] = b.aSensIdx;
      o["aMac"] = macBytesToStr(b.aSensMac);
    }
    o["hasB"] = b.hasB;
    if (b.hasB) {
      o["bIdx"] = b.bSensIdx;
      o["bMac"] = macBytesToStr(b.bSensMac);
    }
  }

  String out;
  serializeJson(doc, out);
  return out;
}

String ESPNowManager::exportConfiguration() const {
  DynamicJsonDocument doc(6144);
  doc["channel"] = _channel;
  doc["mode"]    = _mode;

  // peers
  DynamicJsonDocument jPeers(2048);
  deserializeJson(jPeers, serializePeers());
  doc["peers"] = jPeers.as<JsonVariant>();

  // topology
  DynamicJsonDocument jTopo(4096);
  deserializeJson(jTopo, serializeTopology());
  doc["topology"] = jTopo.as<JsonVariant>();

  // Prefer PSRAM for big JSON buffers
  size_t len = measureJson(doc) + 1;
  char* buf = (char*)heap_caps_malloc(len, MALLOC_CAP_SPIRAM);
  if (!buf) {
    String out; serializeJson(doc, out);
    return out;
  }
  serializeJson(doc, buf, len);
  String out(buf);
  heap_caps_free(buf);
  return out;
}

// ======= Configure / Persist =======
bool ESPNowManager::configureTopology(const JsonVariantConst& topo) {
  // Reset mirrors
  for (uint8_t s=0; s<ICM_MAX_SENSORS+2; ++s) _zcSensors[s] = ZcSensorMirror{};
  for (uint8_t r=0; r<ICM_MAX_RELAYS;    ++r) _boundaries[r] = RelayBoundaryMirror{};

  // --- Zero-centered sensors ---
  if (topo.containsKey("zc")) {
    JsonArrayConst zc = topo["zc"].as<JsonArrayConst>();
    for (JsonObjectConst o : zc) {
      uint8_t sensIdx = o["sensIdx"] | 0xFF;
      if (sensIdx == 0xFF || sensIdx >= ICM_MAX_SENSORS+2) continue;
      auto& z = _zcSensors[sensIdx];
      z.used = true; z.sensIdx = sensIdx;

      z.hasPrev = o["hasPrev"] | false;
      if (z.hasPrev) {
        z.prevSensIdx = o["prevIdx"] | 0xFF;
        String pm = o["prevMac"] | "";
        macStrToBytes(pm, z.prevSensMac);
        String hex; loadOrCreateToken(ModuleType::PRESENCE, z.prevSensIdx, pm, hex, z.prevSensTok16);
      }

      z.hasNext = o["hasNext"] | false;
      if (z.hasNext) {
        z.nextSensIdx = o["nextIdx"] | 0xFF;
        String nm = o["nextMac"] | "";
        macStrToBytes(nm, z.nextSensMac);
        String hex; loadOrCreateToken(ModuleType::PRESENCE, z.nextSensIdx, nm, hex, z.nextSensTok16);
      }

      // neg list
      z.nNeg = 0;
      if (o.containsKey("neg")) {
        JsonArrayConst neg = o["neg"].as<JsonArrayConst>();
        for (JsonObjectConst e : neg) {
          if (z.nNeg >= ICM_MAX_RELAYS) break;
          auto& dst = z.neg[z.nNeg++];
          dst.relayIdx = e["relayIdx"] | 0xFF;
          dst.relPos   = (int8_t)(e["pos"] | -1);
          String rm = e["relayMac"] | "";
          macStrToBytes(rm, dst.relayMac);
          String hex; loadOrCreateToken(ModuleType::RELAY, dst.relayIdx, rm, hex, dst.relayTok16);
        }
      }

      // pos list
      z.nPos = 0;
      if (o.containsKey("pos")) {
        JsonArrayConst pos = o["pos"].as<JsonArrayConst>();
        for (JsonObjectConst e : pos) {
          if (z.nPos >= ICM_MAX_RELAYS) break;
          auto& dst = z.pos[z.nPos++];
          dst.relayIdx = e["relayIdx"] | 0xFF;
          dst.relPos   = (int8_t)(e["pos"] | +1);
          String rm = e["relayMac"] | "";
          macStrToBytes(rm, dst.relayMac);
          String hex; loadOrCreateToken(ModuleType::RELAY, dst.relayIdx, rm, hex, dst.relayTok16);
        }
      }
    }
  }

  // --- Boundaries ---
  if (topo.containsKey("boundaries")) {
    JsonArrayConst bounds = topo["boundaries"].as<JsonArrayConst>();
    for (JsonObjectConst o : bounds) {
      uint8_t rIdx = o["relayIdx"] | 0xFF;
      if (rIdx == 0xFF || rIdx >= ICM_MAX_RELAYS) continue;
      auto& b = _boundaries[rIdx];
      b.used = true; b.relayIdx = rIdx;
      b.splitRule = (uint8_t)(o["splitRule"] | SPLIT_RULE_MAC_ASC_IS_POS_LEFT);

      b.hasA = o["hasA"] | false;
      if (b.hasA) {
        b.aSensIdx = o["aIdx"] | 0xFF;
        String am = o["aMac"] | "";
        macStrToBytes(am, b.aSensMac);
        String hex; loadOrCreateToken(ModuleType::PRESENCE, b.aSensIdx, am, hex, b.aSensTok16);
      }
      b.hasB = o["hasB"] | false;
      if (b.hasB) {
        b.bSensIdx = o["bIdx"] | 0xFF;
        String bm = o["bMac"] | "";
        macStrToBytes(bm, b.bSensMac);
        String hex; loadOrCreateToken(ModuleType::PRESENCE, b.bSensIdx, bm, hex, b.bSensTok16);
      }
    }
  }

  return saveTopologyToNvs();
}

bool ESPNowManager::saveTopologyToNvs() const {
  String j = serializeTopology();
  _cfg->PutString(keyTopo().c_str(), j.c_str());
  return true;
}

bool ESPNowManager::loadTopologyFromNvs() {
  String j = _cfg->GetString(keyTopo().c_str(), "");
  if (j.isEmpty()) return true;
  DynamicJsonDocument doc(8192);
  if (deserializeJson(doc, j)) return false;
  JsonVariantConst root = doc.as<JsonVariantConst>();
  JsonVariantConst links = root.containsKey("links") ? root["links"] : root;
  return configureTopology(links);
}


// ======= New-only Topology setters & pushers =======
bool ESPNowManager::topoSetSensorNeighbors(uint8_t sensIdx,
                                           bool hasPrev, uint8_t prevIdx, const uint8_t prevMac[6],
                                           bool hasNext, uint8_t nextIdx, const uint8_t nextMac[6]) {
  if (sensIdx >= ICM_MAX_SENSORS) return false;
  auto& z = _zcSensors[sensIdx];
  z.used = true; z.sensIdx = sensIdx;

  z.hasPrev = hasPrev; z.prevSensIdx = prevIdx;
  if (hasPrev && prevMac) {
    memcpy(z.prevSensMac, prevMac, 6);
    String m = macBytesToStr(prevMac), hex; uint8_t tok16[16];
    loadOrCreateToken(ModuleType::PRESENCE, prevIdx, m, hex, tok16);
    memcpy(z.prevSensTok16, tok16, 16);
  } else {
    memset(z.prevSensMac, 0, 6);
    memset(z.prevSensTok16, 0, 16);
  }

  z.hasNext = hasNext; z.nextSensIdx = nextIdx;
  if (hasNext && nextMac) {
    memcpy(z.nextSensMac, nextMac, 6);
    String m = macBytesToStr(nextMac), hex; uint8_t tok16[16];
    loadOrCreateToken(ModuleType::PRESENCE, nextIdx, m, hex, tok16);
    memcpy(z.nextSensTok16, tok16, 16);
  } else {
    memset(z.nextSensMac, 0, 6);
    memset(z.nextSensTok16, 0, 16);
  }

  return saveTopologyToNvs();
}

bool ESPNowManager::topoSetSensorRelaysZeroCentered(uint8_t sensIdx,
                                                    const ZcRelEntry* negList, uint8_t nNeg,
                                                    const ZcRelEntry* posList, uint8_t nPos) {
  if (sensIdx >= ICM_MAX_SENSORS) return false;
  auto& z = _zcSensors[sensIdx];
  z.used = true; z.sensIdx = sensIdx;

  z.nNeg = min<uint8_t>(nNeg, ICM_MAX_RELAYS);
  for (uint8_t i=0; i<z.nNeg; ++i) {
    z.neg[i] = negList[i];
    String m = macBytesToStr(z.neg[i].relayMac), hex; uint8_t tok16[16];
    loadOrCreateToken(ModuleType::RELAY, z.neg[i].relayIdx, m, hex, tok16);
    memcpy(z.neg[i].relayTok16, tok16, 16);
  }

  z.nPos = min<uint8_t>(nPos, ICM_MAX_RELAYS);
  for (uint8_t i=0; i<z.nPos; ++i) {
    z.pos[i] = posList[i];
    String m = macBytesToStr(z.pos[i].relayMac), hex; uint8_t tok16[16];
    loadOrCreateToken(ModuleType::RELAY, z.pos[i].relayIdx, m, hex, tok16);
    memcpy(z.pos[i].relayTok16, tok16, 16);
  }

  return saveTopologyToNvs();
}

bool ESPNowManager::topoSetRelayBoundaries(uint8_t relayIdx,
                                           bool hasA, uint8_t aIdx, const uint8_t aMac[6],
                                           bool hasB, uint8_t bIdx, const uint8_t bMac[6],
                                           uint8_t splitRule) {
  if (relayIdx >= ICM_MAX_RELAYS) return false;
  auto& rb = _boundaries[relayIdx];
  rb.used = true;
  rb.relayIdx = relayIdx;
  rb.splitRule = splitRule;

  rb.hasA = hasA; rb.aSensIdx = aIdx;
  if (hasA && aMac) {
    memcpy(rb.aSensMac, aMac, 6);
    String m = macBytesToStr(aMac), hex; uint8_t tok16[16];
    loadOrCreateToken(ModuleType::PRESENCE, aIdx, m, hex, tok16);
    memcpy(rb.aSensTok16, tok16, 16);
  } else {
    memset(rb.aSensMac, 0, 6);
    memset(rb.aSensTok16, 0, 16);
  }

  rb.hasB = hasB; rb.bSensIdx = bIdx;
  if (hasB && bMac) {
    memcpy(rb.bSensMac, bMac, 6);
    String m = macBytesToStr(bMac), hex; uint8_t tok16[16];
    loadOrCreateToken(ModuleType::PRESENCE, bIdx, m, hex, tok16);
    memcpy(rb.bSensTok16, tok16, 16);
  } else {
    memset(rb.bSensMac, 0, 6);
    memset(rb.bSensTok16, 0, 16);
  }

  return saveTopologyToNvs();
}

bool ESPNowManager::topoPushZeroCenteredSensor(uint8_t sensIdx) {
  if (sensIdx >= ICM_MAX_SENSORS) return false;
  const auto& z = _zcSensors[sensIdx];
  if (!z.used) return false;

  PeerRec* pr = nullptr;
  if (!ensurePeer(ModuleType::PRESENCE, sensIdx, pr)) return false;

  // Build variable-length body
  uint8_t buf[250]; size_t off = 0;
  TopoZeroCenteredSensor hdr{};
  hdr.sensIdx = sensIdx;

  hdr.hasPrev = z.hasPrev; hdr.prevSensIdx = z.prevSensIdx;
  memcpy(hdr.prevSensMac, z.prevSensMac, 6);
  memcpy(hdr.prevSensTok16, z.prevSensTok16, 16);

  hdr.hasNext = z.hasNext; hdr.nextSensIdx = z.nextSensIdx;
  memcpy(hdr.nextSensMac, z.nextSensMac, 6);
  memcpy(hdr.nextSensTok16, z.nextSensTok16, 16);

  hdr.nNeg = z.nNeg; hdr.nPos = z.nPos; hdr.rsv = 0;
  if (off + sizeof(hdr) > sizeof(buf)) return false;
  memcpy(buf+off, &hdr, sizeof(hdr)); off += sizeof(hdr);

  size_t needNeg = z.nNeg * sizeof(ZcRelEntry);
  size_t needPos = z.nPos * sizeof(ZcRelEntry);
  if (off + needNeg + needPos > sizeof(buf)) return false;

  if (z.nNeg) { memcpy(buf+off, z.neg, needNeg); off += needNeg; }
  if (z.nPos) { memcpy(buf+off, z.pos, needPos); off += needPos; }

  return enqueueToPeer(pr, CmdDomain::TOPO, TOPO_PUSH_ZC_SENSOR, buf, off, /*ack*/true);
}

bool ESPNowManager::topoPushBoundaryRelay(uint8_t relayIdx) {
  if (relayIdx >= ICM_MAX_RELAYS) return false;
  const auto& rb = _boundaries[relayIdx];
  if (!rb.used) return false;

  PeerRec* pr = nullptr;
  if (!ensurePeer(ModuleType::RELAY, relayIdx, pr)) return false;

  TopoRelayBoundary b{};
  b.myIdx = relayIdx;

  b.hasA = rb.hasA; b.aSensIdx = rb.aSensIdx;
  memcpy(b.aSensMac, rb.aSensMac, 6);
  memcpy(b.aSensTok16, rb.aSensTok16, 16);

  b.hasB = rb.hasB; b.bSensIdx = rb.bSensIdx;
  memcpy(b.bSensMac, rb.bSensMac, 6);
  memcpy(b.bSensTok16, rb.bSensTok16, 16);

  b.splitRule = rb.splitRule;
  memset(b.rsv, 0, sizeof(b.rsv));

  return enqueueToPeer(pr, CmdDomain::TOPO, TOPO_PUSH_BOUNDARY_RELAY,
                       reinterpret_cast<uint8_t*>(&b), sizeof(b), /*ack*/true);
}

bool ESPNowManager::topoPushAllZeroCentered() {
  bool ok = true;
  for (uint8_t s = 0; s < ICM_MAX_SENSORS; ++s) if (_zcSensors[s].used) ok &= topoPushZeroCenteredSensor(s);
  for (uint8_t r = 0; r < ICM_MAX_RELAYS;  ++r) if (_boundaries[r].used) ok &= topoPushBoundaryRelay(r);
  return ok;
}
