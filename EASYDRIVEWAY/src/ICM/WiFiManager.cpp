#include "WiFiManager.h"
#include "WiFiAPI.h"
#include "Config.h"
#include "ConfigManager.h"
#include "Peripheral/ICMLogFS.h"
#include "Peripheral/CoolingManager.h"
#include "Peripheral/SleepTimer.h"
#include "EspNow/ICM_Nw.h"
#include <ArduinoJson.h>
#include <vector>
#include <algorithm>

using NwCore::Core;
#define  COOKIE_NAME "ICMSESS"
// -------------------------- local helpers -----------------------------------
namespace {
  static inline bool macStrToBytes(const String& mac, uint8_t out[6]) {
    return Core::macStrToBytes(mac.c_str(), out);
  }
  static inline String macBytesToStr(const uint8_t mac[6]) {
    return Core::macBytesToStr(mac);
  }

  static String toUpperCopy(String s) { s.toUpperCase(); return s; }

  static bool macEqStr(const uint8_t mac[6], const String& macStr) {
    return Core::macEqStr(mac, macStr.c_str());
  }

  struct LinkItem {
    String type;     // "ENTRANCE" | "SENSOR" | "RELAY" | "PARKING" | "PMS" (ignored in chain)
    String mac;      // "AA:BB:..:FF" (17‑char)
    String prev;     // optional; may be empty
    String next;     // optional; may be empty
  };

  // Load stored topology JSON string from NVS as‑is (no schema change)
  static String loadTopoJsonFromNvs(ConfigManager* cfg) {
    if (!cfg) return String("{}\n");
    String s = cfg->GetString(NVS_KEY_TOPO_STRING, String("{}"));
    if (s.length() == 0) s = "{}";
    return s;
  }

  static void saveTopoJsonToNvs(ConfigManager* cfg, const String& json) {
    if (!cfg) return;
    cfg->PutString(NVS_KEY_TOPO_STRING, json);
  }

  // Parse { "links": [ ... ] } into vector<LinkItem>
  static void parseLinks(JsonVariantConst root, std::vector<LinkItem>& out) {
    out.clear();
    if (root.isNull() || !root.containsKey(J_LINKS)) return;
    JsonArrayConst arr = root[J_LINKS].as<JsonArrayConst>();
    for (JsonObjectConst o : arr) {
      LinkItem li{};
      li.type = o[J_TYPE]  .isNull() ? String() : String(o[J_TYPE]  .as<const char*>());
      li.mac  = o[J_MAC]   .isNull() ? String() : String(o[J_MAC]   .as<const char*>());
      li.prev = o[J_PREV]  .isNull() ? String() : String(o[J_PREV]  .as<const char*>());
      li.next = o[J_NEXT]  .isNull() ? String() : String(o[J_NEXT]  .as<const char*>());
      out.push_back(li);
    }
  }

  // Re‑emit vector<LinkItem> back into a {links:[...]} object (schema unchanged)
  static void emitLinks(const std::vector<LinkItem>& links, DynamicJsonDocument& dst) {
    JsonArray arr = dst.createNestedArray(J_LINKS);
    for (const auto& li : links) {
      JsonObject o = arr.createNestedObject();
      if (li.type.length()) o[J_TYPE] = li.type;
      if (li.mac.length())  o[J_MAC]  = li.mac;
      if (li.prev.length()) o[J_PREV] = li.prev;  // omit if empty
      if (li.next.length()) o[J_NEXT] = li.next;  // omit if empty
    }
  }

  // Remove any element with this MAC and scrub all prev/next references
  static void scrubMacInLinks(std::vector<LinkItem>& links, const String& macUp) {
    // erase matching nodes
    links.erase(std::remove_if(links.begin(), links.end(), [&](const LinkItem& li){
      return toUpperCopy(li.mac) == macUp;
    }), links.end());
    // scrub dangling prev/next
    for (auto& li : links) {
      if (toUpperCopy(li.prev) == macUp) li.prev.clear();
      if (toUpperCopy(li.next) == macUp) li.next.clear();
    }
  }

  // Build E2P order (ENTRANCE → ... → PARKING).
  // We’ll traverse by following `next` from ENTRANCE; if not present, infer by `prev`.
  static std::vector<int> computeE2POrder(const std::vector<LinkItem>& links) {
    int entrance = -1, parking = -1;
    for (int i=0;i<(int)links.size();++i) {
      if (links[i].type == "ENTRANCE") entrance = i;
      if (links[i].type == "PARKING")  parking  = i;
    }
    std::vector<int> order;
    if (entrance >= 0) {
      // Follow next pointers from Entrance
      int cur = entrance;
      std::vector<bool> used(links.size(), false);
      while (cur >= 0 && cur < (int)links.size() && !used[cur]) {
        used[cur] = true; order.push_back(cur);
        if (!links[cur].next.length()) break;
        // find node with mac == next
        int nxt = -1; String target = toUpperCopy(links[cur].next);
        for (int j=0;j<(int)links.size();++j) {
          if (toUpperCopy(links[j].mac) == target) { nxt = j; break; }
        }
        cur = nxt;
      }
      // If we didn’t reach PARKING, try to append by inferring single‐in/ single‐out chain
      if (parking >= 0) {
        bool seenParking = false;
        for (int id : order) if (id == parking) { seenParking = true; break; }
        if (!seenParking) order.push_back(parking);
      }
    } else {
      // Fallback: just dump sensors/relays between special endpoints, sorted by name
      std::vector<std::pair<String,int>> tmp;
      for (int i=0;i<(int)links.size();++i) if (links[i].type != "PMS") tmp.push_back({links[i].type + links[i].mac, i});
      std::sort(tmp.begin(), tmp.end(),[](const std::pair<String,int>& a, const std::pair<String,int>& b){ return a.first < b.first;});

      for (auto& p : tmp) order.push_back(p.second);
    }
    return order;
  }

  // Normalize prev/next along E2P so UI always sees a clean chain.
  static void normalizePrevNextE2P(std::vector<LinkItem>& links) {
    std::vector<int> ord = computeE2POrder(links);
    if (ord.empty()) return;
    // Reset all
    for (auto& li : links) { li.prev.clear(); li.next.clear(); }
    // Assign prev/next along the order, skipping PMS nodes (not part of chain)
    int lastIdx = -1;
    for (int k=0;k<(int)ord.size();++k) {
      int i = ord[k]; if (links[i].type == "PMS") continue;
      if (lastIdx >= 0) {
        links[lastIdx].next = links[i].mac;
        links[i].prev = links[lastIdx].mac;
      }
      lastIdx = i;
    }
  }

  // Helper: is this MAC present in the ICM registry? (existence‑based)
  static bool macExistsInRegistry(Core* esn, const uint8_t mac[6]) {
    return esn ? esn->macInRegistry(mac) : false;
  }

  // Drop any links whose MACs are no longer present in registry
  static void dropDanglingByRegistry(Core* esn, std::vector<LinkItem>& links) {
    if (!esn) return;
    links.erase(std::remove_if(links.begin(), links.end(), [&](const LinkItem& li){
      if (!li.mac.length()) return false;
      uint8_t m[6]; if (!macStrToBytes(li.mac, m)) return false; // keep if unparsable
      return !macExistsInRegistry(esn, m);
    }), links.end());
  }

  // Build and push per‑node directional packets from stored topology
  static void pushDirectionalTopology(Core* esn, const std::vector<LinkItem>& links) {
    if (!esn) return;

    // Compute E2P order for directional neighbors
    std::vector<int> ord = computeE2POrder(links);

    auto isEndpoint = [&](const LinkItem& li){ return li.type == "ENTRANCE" || li.type == "PARKING"; };
    auto isRelay    = [&](const LinkItem& li){ return li.type == "RELAY"; };
    auto isSensor   = [&](const LinkItem& li){ return li.type == "SENSOR"; };

    // Map each index to its E2P prev/next (skipping PMS nodes)
    std::vector<int> chain; chain.reserve(ord.size());
    for (int idx : ord) if (links[idx].type != "PMS") chain.push_back(idx);

    auto nextIdxInChain = [&](int k) -> int { return (k+1 < (int)chain.size()) ? chain[k+1] : -1; };
    auto prevIdxInChain = [&](int k) -> int { return (k-1 >= 0) ? chain[k-1] : -1; };

    // For each node in chain, build and send its directional JSON
    for (int k=0;k<(int)chain.size();++k) {
      int i = chain[k];
      const LinkItem& me = links[i];

      // Determine neighbor objects for E2P and P2E
      auto makeNeighbor = [&](int idx, bool forSensor, bool e2p) -> DynamicJsonDocument {
        DynamicJsonDocument j(128);
        if (idx < 0) {
          // endpoint depending on direction
          const char* t = e2p ? (forSensor? "PARKING" : "SENSOR") : (forSensor? "ENTRANCE" : "SENSOR");
          j[J_TYPE] = t; // type label per spec (for endpoints on sensors we output ENTRANCE/PARKING)
          return j;
        }
        const LinkItem& n = links[idx];
        if (forSensor) {
          if (isSensor(n)) j[J_TYPE] = "SENSOR";
          else if (isRelay(n)) j[J_TYPE] = "RELAY";
          else if (n.type == "PARKING" || n.type == "ENTRANCE") j[J_TYPE] = n.type;
        } else {
          // relay’s neighbor type is SENSOR or the terminal endpoint
          if (isSensor(n)) j[J_TYPE] = "SENSOR";
          else if (isEndpoint(n)) j[J_TYPE] = (n.type == "PARKING" ? "PARKING" : "ENTRANCE");
        }
        if (n.mac.length()) j[J_MAC] = n.mac;
        return j;
      };

      // Compute MAC bytes for this node and figure type
      uint8_t selfMac[6] = {0};
      macStrToBytes(me.mac, selfMac);

      if (isSensor(me)) {
        // SENSOR packet schema
        DynamicJsonDocument pkt(1024);
        pkt["ver"] = 1;
        pkt["self"] = me.mac;

        int nxt = nextIdxInChain(k);
        int prv = prevIdxInChain(k);
        pkt["next_e2p"] = makeNeighbor(nxt, /*forSensor=*/true, /*e2p=*/true);
        pkt["next_p2e"] = makeNeighbor(prv, /*forSensor=*/true, /*e2p=*/false);

        // relays_e2p: scan forward until next sensor/endpoint; count offsets
        JsonArray re2p = pkt.createNestedArray("relays_e2p");
        int off = 0;
        for (int kk=k+1; kk<(int)chain.size(); ++kk) {
          const LinkItem& n = links[chain[kk]];
          if (isSensor(n) || isEndpoint(n)) break; // stop at next sensor or endpoint
          if (isRelay(n)) { JsonObject o = re2p.createNestedObject(); o[J_MAC] = n.mac; o["offset"] = off++; }
        }
        // relays_p2e: scan backward until prev sensor/endpoint
        JsonArray rp2e = pkt.createNestedArray("relays_p2e");
        off = 0;
        for (int kk=k-1; kk>=0; --kk) {
          const LinkItem& n = links[chain[kk]];
          if (isSensor(n) || isEndpoint(n)) break;
          if (isRelay(n)) { JsonObject o = rp2e.createNestedObject(); o[J_MAC] = n.mac; o["offset"] = off++; }
        }

        // Wire: _esn->topSetSensorJSON(sensorMac, json, len, wait)
        String s; serializeJson(pkt, s);
        esn->topSetSensorJSON(selfMac, s.c_str(), s.length(), 40);
      } else if (isRelay(me)) {
        // RELAY packet schema
        DynamicJsonDocument pkt(256);
        pkt["ver"] = 1;
        pkt["self"] = me.mac;
        pkt["neighbor_e2p"] = makeNeighbor(nextIdxInChain(k), /*forSensor=*/false, /*e2p=*/true);
        pkt["neighbor_p2e"] = makeNeighbor(prevIdxInChain(k), /*forSensor=*/false, /*e2p=*/false);
        String s; serializeJson(pkt, s);
        esn->topSetRelayJSON(selfMac, s.c_str(), s.length(), 40);
      } else {
        // Skip ENTRANCE/PARKING/PMS for wire push
      }
    }
  }
} // namespace (helpers)

#include "esp_system.h"   // for esp_random()
#include <stdio.h>        // for snprintf

String WiFiManager::makeSessionToken() const {
  // Mix MAC + millis + random into a hex token
  uint8_t mac[6]; esp_read_mac(mac, ESP_MAC_WIFI_STA);    // needs esp_mac.h (already included via header)
  uint32_t t = (uint32_t)millis();
  uint32_t r = (uint32_t)esp_random();
  char buf[2*6 + 8 + 8 + 1]; // 12 hex (MAC) + 8 (t) + 8 (r) + NUL = 29 chars
  snprintf(buf, sizeof(buf),
           "%02X%02X%02X%02X%02X%02X%08X%08X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], t, r);
  return String(buf);
}

// Optional helpers declared in the header (not currently used by your code):
void WiFiManager::setSessionCookie(AsyncWebServerRequest* req, const String& token) {
  auto r = req->beginResponse(204); // No body
  r->addHeader("Set-Cookie", String(COOKIE_NAME) + "=" + token + "; Path=/; HttpOnly; SameSite=Lax");
  req->send(r);
}

void WiFiManager::clearSessionCookie(AsyncWebServerRequest* req) {
  auto r = req->beginResponse(204); // No body
  r->addHeader("Set-Cookie", String(COOKIE_NAME) + "=; Path=/; Max-Age=0; HttpOnly; SameSite=Lax");
  req->send(r);
}

// ============================== PAGES ========================================
void WiFiManager::begin() {
  DEBUG_PRINTLN("###########################################################");
  DEBUG_PRINTLN("#                 Starting WIFI Manager                   #");
  DEBUG_PRINTLN("###########################################################");

  _apOn = false;

  // Bring-up order
  startAccessPoint();
  registerRoutes();
  addCORS();

  // Static folders with cache (handled by AsyncStaticWebHandler)
  _server.serveStatic("/fonts/", SPIFFS, "/fonts/").setCacheControl("max-age=86400");
  _server.serveStatic("/css/",   SPIFFS, "/css/").setCacheControl("max-age=86400");
  _server.serveStatic("/js/",    SPIFFS, "/js/").setCacheControl("max-age=86400");
  _server.serveStatic("/icons/", SPIFFS, "/icons/").setCacheControl("max-age=86400");

  // Catch-all for anything not matched above
  _server.onNotFound([this](AsyncWebServerRequest* request) {
    const uint32_t ONE_WEEK = 604800;

    auto detectContentType = [](const String& p) -> String {
      if (p.endsWith(".html")) return "text/html";
      if (p.endsWith(".css"))  return "text/css";
      if (p.endsWith(".js"))   return "application/javascript";
      if (p.endsWith(".png"))  return "image/png";
      if (p.endsWith(".jpg") || p.endsWith(".jpeg")) return "image/jpeg";
      if (p.endsWith(".gif"))  return "image/gif";
      if (p.endsWith(".webp")) return "image/webp";
      if (p.endsWith(".svg"))  return "image/svg+xml";
      if (p.endsWith(".ico"))  return "image/x-icon";
      if (p.endsWith(".woff")) return "font/woff";
      if (p.endsWith(".woff2"))return "font/woff2";
      if (p.endsWith(".ttf"))  return "font/ttf";
      return "application/octet-stream";
    };

    auto sendFile = [request, &detectContentType](const String& path, uint32_t maxAge) {
      String mime = detectContentType(path);
      AsyncWebServerResponse* res = request->beginResponse(SPIFFS, path, mime);
      if (maxAge > 0) {
        res->addHeader("Cache-Control", "public, max-age=" + String(maxAge));
      } else {
        res->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        res->addHeader("Pragma", "no-cache");
        res->addHeader("Expires", "0");
      }
      request->send(res);
    };

    String uri = request->url();
    if (uri.endsWith("/")) uri += "index.html";  // default document

    // Never cache HTML shells
    if (uri.endsWith(".html")) {
      if (SPIFFS.exists(uri)) { sendFile(uri, 0); }
      else { request->send(404, "text/plain", "Not found"); }
      return;
    }

    // Static assets → cache
    if (uri.startsWith("/css/") || uri.startsWith("/js/") || uri.startsWith("/icons/") || uri.startsWith("/fonts/") ||
        uri.endsWith(".css") || uri.endsWith(".js") ||
        uri.endsWith(".png") || uri.endsWith(".jpg") || uri.endsWith(".jpeg") ||
        uri.endsWith(".gif") || uri.endsWith(".webp") || uri.endsWith(".svg") ||
        uri.endsWith(".ico") || uri.endsWith(".woff") || uri.endsWith(".woff2") ||
        uri.endsWith(".ttf"))
    {
      if (SPIFFS.exists(uri)) { sendFile(uri, ONE_WEEK); }
      else { request->send(404, "text/plain", "Not found"); }
      return;
    }

    // Fallback: serve if present, else 404
    if (SPIFFS.exists(uri)) {
      sendFile(uri, ONE_WEEK);
    } else {
      request->send(404, "text/plain", "Not found");
    }
  });

  _server.begin();
}

// --- add near other small helpers ---
void WiFiManager::forceAP() { startAccessPoint(); }  // uses existing private method
bool WiFiManager::isAPOn() const { return _apOn; }

void WiFiManager::handleHome(AsyncWebServerRequest* request) {
  if (!isAuthed(request)) { request->redirect(PAGE_LOGIN); return; }
  if (SPIFFS.exists(PAGE_HOME)) request->send(SPIFFS, PAGE_HOME, String());
  else request->send(200, "text/plain", "ICM — Home");
}
void WiFiManager::handleSettings(AsyncWebServerRequest* request) {
  if (!isAuthed(request)) { request->redirect(PAGE_LOGIN); return; }
  if (SPIFFS.exists(PAGE_SETTINGS)) request->send(SPIFFS, PAGE_SETTINGS, String());
  else request->send(200, "text/plain", "Settings");
}
void WiFiManager::handleWiFiPage(AsyncWebServerRequest* request) {
  if (!isAuthed(request)) { request->redirect(PAGE_LOGIN); return; }
  if (SPIFFS.exists(PAGE_WIFI)) request->send(SPIFFS, PAGE_WIFI, String());
  else request->send(200, "text/plain", "Wi‑Fi Credentials");
}
void WiFiManager::handleTopology(AsyncWebServerRequest* request) {
  if (!isAuthed(request)) { request->redirect(PAGE_LOGIN); return; }
  if (SPIFFS.exists(PAGE_TOPO)) request->send(SPIFFS, PAGE_TOPO, String());
  else request->send(200, "text/plain", "Topology");
}
void WiFiManager::handleLive(AsyncWebServerRequest* request) {
  if (!isAuthed(request)) { request->redirect(PAGE_LOGIN); return; }
  if (SPIFFS.exists(PAGE_LIVE)) request->send(SPIFFS, PAGE_LIVE, String());
  else request->send(200, "text/plain", "Live Session");
}
void WiFiManager::handleThanks(AsyncWebServerRequest* request) {
  if (SPIFFS.exists(PAGE_THANKYOU)) request->send(SPIFFS, PAGE_THANKYOU, String());
  else request->send(200, "text/plain", "Thanks");
}

// ============================== JSON HELPERS =================================
void WiFiManager::sendJSON(AsyncWebServerRequest* req, const JsonDocument& doc, int code) {
  String s; serializeJson(doc, s);
  req->send(code, "application/json", s);
}
void WiFiManager::sendError(AsyncWebServerRequest* req, const char* msg, int code) {
  DynamicJsonDocument d(128); d[J_ERR] = msg; sendJSON(req, d, code);
}
void WiFiManager::sendOK(AsyncWebServerRequest* req) {
  DynamicJsonDocument d(64); d[J_OK] = true; sendJSON(req, d, 200);
}

// Attach topology links for status payloads (normalizes to {links:[...]})
void WiFiManager::attachTopologyLinks(DynamicJsonDocument& dst) {
  // For status views we surface the stored NVS topology normalized to {links:[]}
  String topo = loadTopoJsonFromNvs(_cfg);
  DynamicJsonDocument t(65536);
  if (!deserializeJson(t, topo)) dst[J_LINKS] = t[J_LINKS];
}

// ============================== CONFIG =======================================
bool WiFiManager::applyExportedConfig(const JsonVariantConst& cfgObj) {
  bool ok = true;
  // Store {links:[...]} as‑is into NVS (schema unchanged)
  if (!cfgObj.isNull() && cfgObj.containsKey(J_LINKS)) {
    DynamicJsonDocument tmp(65536);
    tmp[J_LINKS] = cfgObj[J_LINKS];
    String s; serializeJson(tmp, s);
    saveTopoJsonToNvs(_cfg, s);
  }
  // Channel alignment: persist under NVS_KEY_NET_CHAN and re‑begin Core
  if (!cfgObj.isNull() && cfgObj.containsKey(J_ESN_CH)) {
    int ch = cfgObj[J_ESN_CH].as<int>();
    if (ch >= 1 && ch <= 13) {
      if (_cfg) _cfg->PutInt(NVS_KEY_NET_CHAN, ch);
      if (_esn) { _esn->end(); _esn->begin((uint8_t)ch, nullptr); }
    }
  }
  return ok;
}

// ============================== IMPORT (SPIFFS→NVS) ==========================
void WiFiManager::syncSpifToPrefs() {
  if (!SPIFFS.exists(SLAVE_CONFIG_PATH)) return;
  File f = SPIFFS.open(SLAVE_CONFIG_PATH, FILE_READ);
  if (!f) return;

  DynamicJsonDocument d(32768);
  if (deserializeJson(d, f)) { f.close(); return; }
  f.close();

  JsonVariantConst root = d.as<JsonVariantConst>();
  if (!root.isNull() && root.containsKey(J_CONFIG)) applyExportedConfig(root[J_CONFIG]);
  // Topology, if provided at top level, is stored as‑is into NVS
  if (!root.isNull() && root.containsKey(J_LINKS)) {
    DynamicJsonDocument tmp(65536); tmp[J_LINKS] = root[J_LINKS];
    String s; serializeJson(tmp, s); saveTopoJsonToNvs(_cfg, s);
  }
}

// ============================== WIFI BRING‑UP ================================
bool WiFiManager::tryConnectSTAFromNVS(uint32_t timeoutMs) {
  const String ssid = _cfg ? _cfg->GetString(NVS_KEY_WIFI_STSSID, "") : "";
  const String psk  = _cfg ? _cfg->GetString(NVS_KEY_WIFI_STKEY,  "") : "";

  _wifi->mode(WIFI_STA);
  _wifi->setAutoConnect(true);
  _wifi->setAutoReconnect(true);
  _wifi->disconnect(true);
  delay(50);

  if (ssid.length()) {
    _wifi->persistent(false);
    if (psk.length()) _wifi->begin(ssid.c_str(), psk.c_str());
    else              _wifi->begin(ssid.c_str());
  } else {
    _wifi->persistent(true);
    _wifi->begin();
  }

  const uint32_t start = millis();
  while (_wifi->status() != WL_CONNECTED && (millis() - start) < timeoutMs) delay(100);

  if (_wifi->status() != WL_CONNECTED) {
    if (_log) _log->event(ICMLogFS::DOM_WIFI, ICMLogFS::EV_WARN, 6010, "STA connect failed; will use AP", "WiFiMgr");
    return false;
  }

  alignESPNOWToCurrentChannel();
  return true;
}

void WiFiManager::startAccessPoint() {
  String apSsid = _cfg ? _cfg->GetString(NVS_KEY_WIFI_APSSID, NVS_DEF_WIFI_APSSID)
                       : String(NVS_DEF_WIFI_APSSID);
  String apPass = _cfg ? _cfg->GetString(NVS_KEY_WIFI_APKEY,      NVS_DEF_WIFI_APKEY)
                       : String(NVS_DEF_WIFI_APKEY);
  int ch = _cfg ? _cfg->GetInt(NVS_KEY_NET_CHAN, NVS_DEF_NET_CHAN) : NVS_DEF_NET_CHAN;
  if (ch < 1 || ch > 13) ch = NVS_DEF_NET_CHAN;

      DEBUG_PRINTLN("WiFiManager: Starting Access Point ✅");


    // Configure the AP with static IP settings
    if (!_wifi->softAPConfig(LOCAL_IP, GATEWAY, SUBNET)) {
        DEBUG_PRINTLN("Failed to set AP config ❌");
        return;
    };
  _wifi->softAP(apSsid.c_str(), apPass.c_str(), ch);
  _apOn = true;
  // Also align ESPNOW to this AP channel
  //if (_esn) { _esn->end(); _esn->begin((uint8_t)ch, nullptr); }
}

void WiFiManager::disableWiFiAP() {
  if (_apOn) {
    _wifi->softAPdisconnect(true);
    _wifi->mode(WIFI_OFF);
    _apOn = false;
  }
}

void WiFiManager::setAPCredentials(const char* ssid, const char* password) {
  if (_cfg) {
    _cfg->PutString(NVS_KEY_WIFI_APSSID, ssid);
    _cfg->PutString(NVS_KEY_WIFI_APKEY,  password);
  }
}

void WiFiManager::alignESPNOWToCurrentChannel() {
  int ch = _cfg ? _cfg->GetInt(NVS_KEY_NET_CHAN, NVS_DEF_NET_CHAN) : NVS_DEF_NET_CHAN;
  if (ch < 1 || ch > 13) ch = NVS_DEF_NET_CHAN;
  if (_cfg) _cfg->PutInt(NVS_KEY_NET_CHAN, ch); // persist
  if (_esn) {
    _esn->end();
    _esn->begin((uint8_t)ch, nullptr);
    // Rebuild peer table existence‑based from registry
    _esn->clearPeers();
    _esn->slotsLoadFromRegistry();
    // Add a peer for every existing registry MAC (sensor/relay/pms)
    auto addIf = [&](const NwCore::NodeSlot& s){
      if (s.index <= 0) return; // not present
      if (!s.mac[0] && !s.mac[1] && !s.mac[2] && !s.mac[3] && !s.mac[4] && !s.mac[5]) return;
      _esn->addPeer(s.mac, (uint8_t)ch, false, nullptr);
    };
    for (int i=0;i<NOW_MAX_SENSORS;i++) addIf(_esn->sensors[i]);
    for (int i=0;i<NOW_MAX_RELAYS;i++)   addIf(_esn->relays[i]);
    for (int i=0;i<NOW_MAX_POWER;i++)    addIf(_esn->pms[i]);
  }
}

// ============================== ROUTES / CORS ================================
void WiFiManager::registerRoutes() {
  // HTML pages
  _server.on("/",           HTTP_GET, [this](AsyncWebServerRequest* r){ handleHome(r); });
  _server.on(PAGE_HOME,     HTTP_GET, [this](AsyncWebServerRequest* r){ handleHome(r); });
  _server.on(PAGE_SETTINGS, HTTP_GET, [this](AsyncWebServerRequest* r){ handleSettings(r); });
  _server.on(PAGE_WIFI,     HTTP_GET, [this](AsyncWebServerRequest* r){ handleWiFiPage(r); });
  _server.on(PAGE_TOPO,     HTTP_GET, [this](AsyncWebServerRequest* r){ handleTopology(r); });
  _server.on(PAGE_LIVE,     HTTP_GET, [this](AsyncWebServerRequest* r){ handleLive(r); });
  _server.on(PAGE_THANKYOU, HTTP_GET, [this](AsyncWebServerRequest* r){ handleThanks(r); });

  // Config
  _server.on(API_CFG_LOAD,   HTTP_GET,  [this](AsyncWebServerRequest* r){ hCfgLoad(r); });
  _server.on(API_CFG_SAVE,   HTTP_POST, nullptr, nullptr,
            [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hCfgSave(r,d,l,i,t);} );
  //_server.on(API_CFG_EXPORT, HTTP_GET,  [this](AsyncWebServerRequest* r){ hCfgExport(r); });
  /*_server.on(API_CFG_IMPORT, HTTP_POST, nullptr, nullptr,
            [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hCfgImport(r,d,l,i,t);} );*/
  _server.on(API_CFG_FACTORY_RESET, HTTP_POST, nullptr, nullptr,
            [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hCfgFactoryReset(r,d,l,i,t);} );

  // Wi-Fi
  _server.on(API_WIFI_MODE,  HTTP_GET, [this](AsyncWebServerRequest* r){ hWiFiMode(r); });
  _server.on(API_WIFI_STA_CONNECT,    HTTP_POST, nullptr, nullptr,
            [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hWiFiStaConnect(r,d,l,i,t);} );
  _server.on(API_WIFI_STA_DISCONNECT, HTTP_POST, nullptr, nullptr,
            [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hWiFiStaDisconnect(r,d,l,i,t);} );
  _server.on(API_WIFI_AP_START,       HTTP_POST, nullptr, nullptr,
            [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hWiFiApStart(r,d,l,i,t);} );
  _server.on(API_WIFI_SCAN,  HTTP_GET, [this](AsyncWebServerRequest* r){ hWiFiScan(r); });

  // Peers / Topology
  _server.on(API_PEERS_LIST,  HTTP_GET, [this](AsyncWebServerRequest* r){ hPeersList(r); });
  _server.on(API_PEER_PAIR,   HTTP_POST, nullptr, nullptr,
            [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hPeerPair(r,d,l,i,t);} );
  _server.on(API_PEER_REMOVE, HTTP_POST, nullptr, nullptr,
            [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hPeerRemove(r,d,l,i,t);} );
  _server.on(API_TOPOLOGY_SET, HTTP_POST, nullptr, nullptr,
            [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hTopoSet(r,d,l,i,t);} );
  _server.on(API_TOPOLOGY_GET, HTTP_GET, [this](AsyncWebServerRequest* r){ hTopoGet(r); });

  // Live / Status
  // _server.on(API_LIVE,       HTTP_GET, [this](AsyncWebServerRequest* r){ hLiveStatus(r); });
  _server.on(API_SYS_STATUS,  HTTP_GET, [this](AsyncWebServerRequest* r){ hSysStatus(r); });

  // Quick controls / Cooling / Sleep
  _server.on(API_BUZZ_SET,    HTTP_POST, nullptr, nullptr,
            [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hBuzzerSet(r,d,l,i,t);} );
  _server.on(API_SYS_RESET,   HTTP_POST, nullptr, nullptr,
            [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hSystemReset(r,d,l,i,t);} );
  _server.on(API_SYS_RESTART, HTTP_POST, nullptr, nullptr,
            [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hSystemRestart(r,d,l,i,t);} );
  _server.on(API_SYS_MODE,    HTTP_POST, nullptr, nullptr,
            [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hSystemMode(r,d,l,i,t);} );

  _server.on(API_COOL_STATUS, HTTP_GET,  [this](AsyncWebServerRequest* r){ hCoolStatus(r); });
  _server.on(API_COOL_MODE,   HTTP_POST, nullptr, nullptr,
            [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hCoolMode(r,d,l,i,t);} );
  _server.on(API_COOL_SPEED,  HTTP_POST, nullptr, nullptr,
            [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hCoolSpeed(r,d,l,i,t);} );

  _server.on(API_SLEEP_STATUS, HTTP_GET, [this](AsyncWebServerRequest* r){ hSleepStatus(r); });
  _server.on(API_SLEEP_TIMEOUT, HTTP_POST, nullptr, nullptr,
            [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hSleepTimeout(r,d,l,i,t);} );
  _server.on(API_SLEEP_SCHED,  HTTP_POST, nullptr, nullptr,
            [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hSleepSchedule(r,d,l,i,t);} );

  // Auth
  _server.on(API_LOGIN_ENDPOINT,  HTTP_POST, nullptr, nullptr,
            [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t){ hLogin(r,d,l,i,t);} );
  _server.on(API_LOGOUT_ENDPOINT, HTTP_POST, [this](AsyncWebServerRequest* r){ hLogout(r); });
}

void WiFiManager::addCORS() {
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");
}

// ============================== WIFI JSON ====================================
void WiFiManager::hWiFiMode(AsyncWebServerRequest* req) {
  DynamicJsonDocument d(256);
  if (_apOn) {
    d[J_MODE] = "AP"; d["ip"] = _wifi->softAPIP().toString();
    d["ch"] = _cfg ? _cfg->GetInt(NVS_KEY_NET_CHAN, NVS_DEF_NET_CHAN) : NVS_DEF_NET_CHAN;
  } else if (_wifi->status() == WL_CONNECTED) {
    d[J_MODE] = "STA"; d["ip"] = _wifi->localIP().toString();
    d["ch"] = _wifi->channel(); d["rssi"] = _wifi->RSSI();
  } else { d[J_MODE] = "OFF"; }
  sendJSON(req, d);
}

void WiFiManager::hWiFiStaConnect(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
  static String body; if (index == 0) body.clear();
  body += String((char*)data).substring(0, len);
  if (index + len < total) return;
  DynamicJsonDocument d(512);
  if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }
  JsonVariantConst root = d.as<JsonVariantConst>();
  if (root.isNull() || !root.containsKey(J_WIFI_SSID) || !root.containsKey(J_WIFI_PSK)) { sendError(req, "Missing ssid/password"); return; }
  String ssid = root[J_WIFI_SSID].as<String>();
  String psk  = root[J_WIFI_PSK].as<String>();

  _wifi->mode(WIFI_STA);
  _wifi->persistent(true);
  _wifi->disconnect(true); delay(50);
  _wifi->begin(ssid.c_str(), psk.c_str());
  uint32_t start = millis();
  while (_wifi->status() != WL_CONNECTED && millis() - start < 15000) delay(100);
  DynamicJsonDocument res(256);
  if (_wifi->status() == WL_CONNECTED) {
    _apOn = false; alignESPNOWToCurrentChannel();
    res[J_OK] = true; res["ip"] = _wifi->localIP().toString();
  } else { res[J_OK] = false; }
  sendJSON(req, res);
}

// ============================== PEERS ========================================
void WiFiManager::hPeersList(AsyncWebServerRequest* req) {
  DynamicJsonDocument d(8192);
  JsonArray peers = d.createNestedArray("peers");
  if (_esn) {
    _esn->slotsLoadFromRegistry();
    auto emitSlot = [&](const NwCore::NodeSlot& s, const char* type){
      if (s.index <= 0) return;
      JsonObject o = peers.createNestedObject();
      o[J_MAC] = macBytesToStr(s.mac);
      o[J_TYPE] = type;
      o["idx"] = s.index;
      o["present"] = s.present ? true : false;
      o["rssi"] = (int)s.lastRSSI;
      o["updated_ms"] = (uint32_t)s.lastSeenMs;
    };
    for (int i=0;i<NOW_MAX_SENSORS;i++) emitSlot(_esn->sensors[i], "sensor");
    for (int i=0;i<NOW_MAX_RELAYS;i++)   emitSlot(_esn->relays[i],  "relay");
    for (int i=0;i<NOW_MAX_POWER;i++)    emitSlot(_esn->pms[i],     "pms");
  }
  sendJSON(req, d);
}

void WiFiManager::hPeerPair(AsyncWebServerRequest* req,
                            uint8_t* data, size_t len, size_t index, size_t total) {
  static String body;
  if (index == 0) body.clear();
  body += String((char*)data).substring(0, len);   // keep your original append style
  if (index + len < total) return;

  DynamicJsonDocument d(512);
  if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }

  JsonVariantConst root = d.as<JsonVariantConst>();
  if (root.isNull() || !root.containsKey(J_MAC)) { sendError(req, "Missing mac"); return; }
  if (!_esn) { sendError(req, "No Core"); return; }

  String macStr = root[J_MAC].as<String>();
  macStr.toUpperCase();                              // ok: returns void, mutates in place
  uint8_t mac[6];
  if (!Core::macStrToBytes(macStr.c_str(), mac)) {   // use helper with c_str()
    sendError(req, "Bad MAC"); return;
  }

  bool ok = false;
  if (root.containsKey(J_TYPE)) {
    // Typed pair → NOW_KIND_* via auto_pair_from_devinfo
    String type = root[J_TYPE].as<String>();         // <-- FIXED: no void conversion
    type.toLowerCase();                              // mutate in place

    uint8_t kind = NOW_KIND_UNKNOWN;
    if (type == "sensor") kind = NOW_KIND_SENS;
    else if (type == "relay") kind = NOW_KIND_RELAY;
    else if (type == "pms" || type == "power") kind = NOW_KIND_PMS;

    DevInfoPayload info{};
    info.kind = kind;                                // minimal devinfo
    ok = Core::auto_pair_from_devinfo(_esn, mac, &info);
  } else {
    // Fallback: addPeer then PR_DEVINFO query
    int ch = _cfg ? _cfg->GetInt(NVS_KEY_NET_CHAN, NVS_DEF_NET_CHAN) : NVS_DEF_NET_CHAN;
    if (ch < 1 || ch > 13) ch = NVS_DEF_NET_CHAN;
    ok = _esn->addPeer(mac, (uint8_t)ch, false, nullptr);
    if (ok) {
      esp_err_t e = _esn->prDevInfoQuery(mac, 120); // best-effort
      (void)e; // don’t block HTTP on it
    }
  }
  if (ok) sendOK(req); else sendError(req, "pair failed", 500);
}


void WiFiManager::hPeerRemove(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
  static String body; if (index == 0) body.clear();
  body += String((char*)data).substring(0, len);
  if (index + len < total) return;
  DynamicJsonDocument d(256);
  if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }
  JsonVariantConst root = d.as<JsonVariantConst>();
  if (root.isNull() || !root.containsKey(J_MAC)) { sendError(req, "Missing mac"); return; }
  if (!_esn) { sendError(req, "No Core"); return; }

  String macStr = root[J_MAC].as<String>(); macStr.toUpperCase();
  uint8_t mac[6]; if (!macStrToBytes(macStr, mac)) { sendError(req, "Bad MAC"); return; }

  // Best‑effort unpair on the device first
  _esn->prUnpair(mac);
  _esn->delPeer(mac);

  // Existence‑based registry clearing
  uint8_t idx = 0;
  if (_esn->icmRegistryIndexOfSensorMac(mac, idx)) _esn->icmRegistryClearSensor(idx);
  if (_esn->icmRegistryIndexOfRelayMac (mac, idx)) _esn->icmRegistryClearRelay(idx);
  // PMS has only one slot; clear if MAC matches
  for (int i=0;i<NOW_MAX_POWER;i++) { if (Core::macEq(_esn->pms[i].mac, mac)) { _esn->icmRegistryClearPower(); break; } }

  // Scrub MAC from NVS topology string (remove links[] element; erase prev/next refs)
  String topo = loadTopoJsonFromNvs(_cfg);
  DynamicJsonDocument t(65536); if (!deserializeJson(t, topo)) {
    std::vector<LinkItem> links; parseLinks(t.as<JsonVariantConst>(), links);
    scrubMacInLinks(links, macStr);
    normalizePrevNextE2P(links);
    DynamicJsonDocument out(65536); emitLinks(links, out);
    String s; serializeJson(out, s); saveTopoJsonToNvs(_cfg, s);
  }
  sendOK(req);
}

// ============================== TOPOLOGY =====================================
void WiFiManager::hTopoGet(AsyncWebServerRequest* req) {
  // Load from NVS (already scrubbed on unpair), then drop any entries whose keys no longer exist
  String topo = loadTopoJsonFromNvs(_cfg);
  DynamicJsonDocument d(65536);
  if (deserializeJson(d, topo)) { d.clear(); d.createNestedArray(J_LINKS); }

  std::vector<LinkItem> links; parseLinks(d.as<JsonVariantConst>(), links);
  dropDanglingByRegistry(_esn, links);      // belt‑and‑suspenders
  normalizePrevNextE2P(links);              // recompute prev/next and order along E2P

  DynamicJsonDocument out(65536); emitLinks(links, out);
  sendJSON(req, out);
}

void WiFiManager::hTopoSet(AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
  static String body; if (index == 0) body.clear();
  body += String((char*)data).substring(0, len);
  if (index + len < total) return;

  DynamicJsonDocument d(65536);
  if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }
  JsonVariantConst root = d.as<JsonVariantConst>();
  if (root.isNull() || !root.containsKey(J_LINKS)) { sendError(req, "Missing links[]"); return; }

  // Store as‑is in NVS
  {
    DynamicJsonDocument tmp(65536); tmp[J_LINKS] = root[J_LINKS];
    String s; serializeJson(tmp, s); saveTopoJsonToNvs(_cfg, s);
  }

  // Optional push
  bool push = root.containsKey("push") && root["push"].as<bool>();
  if (push) {
    // Parse stored links again (ensures same source of truth)
    DynamicJsonDocument t(65536);
    String topo = loadTopoJsonFromNvs(_cfg);
    if (!deserializeJson(t, topo)) {
      std::vector<LinkItem> links; parseLinks(t.as<JsonVariantConst>(), links);
      normalizePrevNextE2P(links);
      pushDirectionalTopology(_esn, links);
    }
  }
  sendOK(req);
}

// ============================== LIVE =========================================
void WiFiManager::hLiveStatus(AsyncWebServerRequest* req) {
  DynamicJsonDocument d(8192);
  d["now_ms"] = millis();
  if (_esn) {
    // Sensors (env + ToF)
    JsonArray sens = d.createNestedArray("sensors");
    for (int i=0;i<NOW_MAX_SENSORS;i++) {
      const auto& s = _esn->sensors[i]; if (s.index <= 0) continue;
      JsonObject o = sens.createNestedObject();
      o[J_MAC] = macBytesToStr(s.mac);
      o["idx"] = s.index; o["present"] = s.present ? true : false; o["rssi"] = (int)s.lastRSSI; o["updated_ms"] = (uint32_t)s.lastSeenMs;
      const auto& lv = _esn->sensLive[i];
      o["temp_c_x100"] = (int)lv.t_c_x100;
      o["rh_x100"]     = (uint32_t)lv.rh_x100;
      o["p_Pa"]        = (int)lv.p_Pa;
      o["lux_x10"]     = (uint32_t)lv.lux_x10;
      o["is_day"]      = (uint8_t)lv.is_day;
      o["tfA_mm"]      = (uint16_t)lv.tfA_mm;
      o["tfB_mm"]      = (uint16_t)lv.tfB_mm;
    }
    // Relays (temp/flags available via REL_LIVE)
    JsonArray rels = d.createNestedArray("relays");
    for (int i=0;i<NOW_MAX_RELAYS;i++) {
      const auto& r = _esn->relays[i]; if (r.index <= 0) continue;
      JsonObject o = rels.createNestedObject();
      o[J_MAC] = macBytesToStr(r.mac);
      o["idx"] = r.index; o["present"] = r.present ? true : false; o["rssi"] = (int)r.lastRSSI; o["updated_ms"] = (uint32_t)r.lastSeenMs;
      const auto& lv = _esn->relayLive[i];
      o["temp_c_x100"] = (int)lv.temp_c_x100;
    }
    // PMS — power report cache
    JsonArray pms = d.createNestedArray("pms");
    for (int i=0;i<NOW_MAX_POWER;i++) {
      const auto& p = _esn->pms[i]; if (p.index <= 0) continue;
      JsonObject o = pms.createNestedObject();
      o[J_MAC] = macBytesToStr(p.mac); o["idx"] = p.index; o["present"] = p.present ? true : false; o["rssi"] = (int)p.lastRSSI; o["updated_ms"] = (uint32_t)p.lastSeenMs;
      // If you mirror PMS cache in Core, emit fields here.
    }
  }
  sendJSON(req, d);
}

void WiFiManager::hSysStatus(AsyncWebServerRequest* req) {
  DynamicJsonDocument d(4096);
  d[J_OK] = true;
  d["mode"] = _apOn ? "AP" : (_wifi->status()==WL_CONNECTED?"STA":"OFF");
  d["ip"] = _apOn ? _wifi->softAPIP().toString() : _wifi->localIP().toString();
  d["uptime_ms"] = millis();
  attachTopologyLinks(d);
  sendJSON(req, d);
}

// ============================================================================
// AUTH & UPLOADS (unchanged in terms of ESP-NOW usage)
// ============================================================================
void handleFileUpload(AsyncWebServerRequest *request,
                      const String& filename, size_t index,
                      uint8_t *data, size_t len, bool final) {
  static File up;
  if (index == 0) {
    if (SPIFFS.exists(SLAVE_CONFIG_PATH)) SPIFFS.remove(SLAVE_CONFIG_PATH);
    up = SPIFFS.open(SLAVE_CONFIG_PATH, FILE_WRITE);
    if (!up) { request->send(500, "text/plain", "open failed"); return; }
  }
  if (len) up.write(data, len);
  if (final) {
    up.close();
    if (WiFiManager::instance) WiFiManager::instance->syncSpifToPrefs();
    request->send(200, "text/plain", "OK");
  }
}

String WiFiManager::readCookie(AsyncWebServerRequest* req, const String& name) const {
  if (!req->hasHeader("Cookie")) return String();
  String cookie = req->header("Cookie");
  String key = name + "=";
  int p = cookie.indexOf(key);
  if (p < 0) return String();
  p += key.length();
  int end = cookie.indexOf(';', p);
  return end >= 0 ? cookie.substring(p, end) : cookie.substring(p);
}

bool WiFiManager::isAuthed(AsyncWebServerRequest* req) const {
  if (_sessionToken.isEmpty()) return false;
  String c = readCookie(req, COOKIE_NAME);
  return c.length() && c == _sessionToken;
}

void WiFiManager::hLogout(AsyncWebServerRequest* req) {
  _sessionToken.clear();
  auto r = req->beginResponse(302, "text/plain", "");
  r->addHeader("Set-Cookie", String(COOKIE_NAME) + "=; Path=/; Max-Age=0; HttpOnly; SameSite=Lax");
  r->addHeader("Location", PAGE_LOGIN);
  req->send(r);
}

void WiFiManager::hLogin(AsyncWebServerRequest* req,
                         uint8_t* data, size_t len, size_t index, size_t total) {
  static String body; if (index == 0) body.clear();
  body += String((char*)data).substring(0, len);
  if (index + len < total) return;

  const bool wantsJson = (req->hasHeader("Accept") && req->header("Accept").indexOf("json") >= 0);

  String u = _cfg ? _cfg->GetString(WEB_USER_KEY, "admin") : "admin";
  String p = _cfg ? _cfg->GetString(WEB_PASS_KEY, "admin") : "admin";

  DynamicJsonDocument d(256);
  if (!deserializeJson(d, body)) {
    JsonVariantConst root = d.as<JsonVariantConst>();
    if (root.containsKey("user") && root.containsKey("pass")) {
      String ru = root["user"].as<String>();
      String rp = root["pass"].as<String>();
      if (ru == u && rp == p) {
        _sessionToken = makeSessionToken();
        String cookie = String(COOKIE_NAME) + "=" + _sessionToken + "; Path=/; HttpOnly; SameSite=Lax";
        if (wantsJson) {
          auto r = req->beginResponse(200, "application/json", "{\"ok\":true}");
          r->addHeader("Set-Cookie", cookie);
          req->send(r);
        } else {
          auto r = req->beginResponse(302, "text/plain", "");
          r->addHeader("Set-Cookie", cookie);
          r->addHeader("Location", PAGE_HOME);
          req->send(r);
        }
        return;
      }
    }
  }
  if (wantsJson) req->send(403, "application/json", "{\"err\":\"login\"}");
  else {
    auto r = req->beginResponse(302, "text/plain", "");
    r->addHeader("Location", PAGE_LOGIN_FAIL);
    req->send(r);
  }
}

// ============================================================================
// SLEEP (unchanged relative to ESP-NOW)
// ============================================================================
void WiFiManager::hSleepSchedule(AsyncWebServerRequest* req,
                                 uint8_t* data, size_t len, size_t index, size_t total) {
  if (!isAuthed(req)) { req->send(401, "application/json", "{\"err\":\"auth\"}"); return; }
  static String body; if (index==0) body.clear();
  body += String((const char*)data).substring(0, len);
  if (index + len < total) return;

  DynamicJsonDocument d(256);
  if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }
  bool ok = false;

  if (!_slp) { sendError(req, "SleepTimer not available", 500); return; }

  if (d.containsKey("after_sec")) {
    uint32_t s = d["after_sec"] | 1;
    ok = _slp->sleepAfterSeconds(s);
  } else if (d.containsKey("wake_epoch")) {
    uint32_t e = d["wake_epoch"] | 0;
    if (e) ok = _slp->sleepUntilEpoch(e);
  } else if (d.containsKey("reset_activity") && d["reset_activity"].as<bool>()) {
    _slp->resetActivity();
    ok = true;
  } else {
    sendError(req, "Need 'after_sec' or 'wake_epoch' or 'reset_activity'"); return;
  }

  if (!ok) { sendError(req, "Schedule failed", 500); return; }
  sendOK(req);
}

void WiFiManager::hSleepReset(AsyncWebServerRequest* req) {
  if (!isAuthed(req)) { req->send(401, "application/json", "{\"err\":\"auth\"}"); return; }
  if (_slp) _slp->resetActivity();
  sendOK(req);
}
void WiFiManager::hSleepTimeout(AsyncWebServerRequest* req,
                                uint8_t* data, size_t len, size_t index, size_t total) {
  if (!isAuthed(req)) { req->send(401, "application/json", "{\"err\":\"auth\"}"); return; }
  static String body; if (index==0) body.clear();
  body += String((const char*)data).substring(0, len);
  if (index + len < total) return;

  DynamicJsonDocument d(128);
  if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }
  uint32_t sec = d["timeout_sec"] | 0;
  if (!sec) { sendError(req, "timeout_sec > 0"); return; }

  if (_slp) _slp->setInactivityTimeoutSec(sec);
  sendOK(req);
}

void WiFiManager::hSleepStatus(AsyncWebServerRequest* req) {
  if (!isAuthed(req)) { req->send(401, "application/json", "{\"err\":\"auth\"}"); return; }
  DynamicJsonDocument d(256);
  if (_slp) {
    const long secsLeft = _slp->secondsUntilSleep();         // negative => due/armed
    d["secs_to_sleep"]      = (int32_t)secsLeft;
    d["last_activity_epoch"]= _slp->lastActivityEpoch();
    d["next_wake_epoch"]    = _slp->nextWakeEpoch();
    d["armed"]              = _slp->isArmed();
  }
  sendJSON(req, d);
}
// ============================================================================
// COOLING (unchanged relative to ESP-NOW)
// ============================================================================
void WiFiManager::hCoolSpeed(AsyncWebServerRequest* req,
                             uint8_t* data, size_t len, size_t index, size_t total) {
  if (!isAuthed(req)) { req->send(401, "application/json", "{\"err\":\"auth\"}"); return; }
  static String body; if (index==0) body.clear();
  body += String((const char*)data).substring(0, len);
  if (index + len < total) return;

  DynamicJsonDocument d(128);
  if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }
  int pct = d["pct"] | -1;
  if (pct < 0 || pct > 100) { sendError(req, "pct 0..100"); return; }

  if (_cool) _cool->setManualSpeedPct((uint8_t)pct);
  sendOK(req);
}
void WiFiManager::hCoolMode(AsyncWebServerRequest* req,
                            uint8_t* data, size_t len, size_t index, size_t total) {
  if (!isAuthed(req)) { req->send(401, "application/json", "{\"err\":\"auth\"}"); return; }
  static String body; if (index==0) body.clear();
  body += String((const char*)data).substring(0, len);
  if (index + len < total) return;

  DynamicJsonDocument d(128);
  if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }
  const char* m = d["mode"] | nullptr;
  if (!m) { sendError(req, "Missing 'mode'"); return; }

  if (_cool) {
    auto strToMode = [](const String& s)->CoolingManager::Mode{
      if (s=="AUTO")    return CoolingManager::COOL_AUTO;
      if (s=="ECO")     return CoolingManager::COOL_ECO;
      if (s=="NORMAL")  return CoolingManager::COOL_NORMAL;
      if (s=="FORCED")  return CoolingManager::COOL_FORCED;
      if (s=="STOPPED") return CoolingManager::COOL_STOPPED;
      return CoolingManager::COOL_AUTO;
    };
    _cool->setMode(strToMode(String(m)));
  }
  sendOK(req);
}
void WiFiManager::hCoolStatus(AsyncWebServerRequest* req) {
  DynamicJsonDocument d(256);
  if (_cool) {
    // "auto" if requested mode is COOL_AUTO, otherwise treat as "manual"
    d["mode"]  = (_cool->modeRequested() == CoolingManager::COOL_AUTO) ? "auto" : "manual";
    d["speed"] = _cool->lastSpeedPct();  // 0..100 (%)
  }
  sendJSON(req, d);
}


// ============================================================================
// SYSTEM QUICK CONTROLS (align with Core where applicable)
// ============================================================================
void WiFiManager::hSystemMode(AsyncWebServerRequest* req,
                              uint8_t* data, size_t len, size_t index, size_t total) {
  static String body; if (index == 0) body.clear();
  body += String((char*)data).substring(0, len);
  if (index + len < total) return;

  DynamicJsonDocument d(256);
  if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }
  String mode = d["mode"].as<String>(); // "AUTO" or "MANUAL"
  uint8_t m = (mode.equalsIgnoreCase("MANUAL")) ? 1 : 0;

  if (_esn) {
    _esn->slotsLoadFromRegistry();
    // broadcast to all known nodes (sensors + relays + pms)
    auto sendAll = [&](const NwCore::NodeSlot* arr, int n){
      for (int i=0;i<n;i++){
        if (arr[i].index <= 0) continue;
        if (!arr[i].mac[0]&&!arr[i].mac[1]&&!arr[i].mac[2]&&!arr[i].mac[3]&&!arr[i].mac[4]&&!arr[i].mac[5]) continue;
        _esn->sysModeSet(arr[i].mac, m, 50);
      }
    };
    sendAll(_esn->sensors, NOW_MAX_SENSORS);
    sendAll(_esn->relays,  NOW_MAX_RELAYS);
    sendAll(_esn->pms,     NOW_MAX_POWER);
  }
  sendOK(req);
}

void WiFiManager::hSystemRestart(AsyncWebServerRequest* req,
                                 uint8_t* d, size_t l, size_t i, size_t t) {
  (void)d;(void)l;(void)i;(void)t;
  sendOK(req);
  delay(50);
  ESP.restart();
}
void WiFiManager::hSystemReset(AsyncWebServerRequest* req,
                               uint8_t* data, size_t len, size_t index, size_t total) {
  (void)data; (void)len; (void)index; (void)total;
  if (!isAuthed(req)) { req->send(401, "application/json", "{\"err\":\"auth\"}"); return; }

  // Mark for reset via NVS or however your bootloader watches for it
  if (_cfg) _cfg->PutBool(RESET_FLAG_KEY, true);

  // Optionally clear the in-RAM peer list only (keeps registry in NVS):
  if (_esn) _esn->clearPeers();  // available; safe no-op if empty

  sendOK(req);
}
void WiFiManager::hBuzzerSet(AsyncWebServerRequest* req,
                             uint8_t* data, size_t len, size_t index, size_t total) {
  (void)index;(void)total;
  DynamicJsonDocument d(128);
  if (deserializeJson(d, String((char*)data).substring(0,len))) { sendError(req,"json"); return; }
  uint8_t on  = d["on"] | 0;
  uint8_t pat = d["pat"]| 0;
  // local indicator (if you wire Core::indBuzzerSet you can forward as well)
  if (_esn) _esn->indBuzzerSet(on, pat, 30);
  sendOK(req);
}

// ============================================================================
// POWER (align to Core PMS ops; reflect presence/lastSeen; trigger query)
// ============================================================================
void WiFiManager::hPowerCmd(AsyncWebServerRequest* req,
                            uint8_t* data, size_t len, size_t index, size_t total) {
  if (!isAuthed(req)) { req->send(401, "application/json", "{\"err\":\"auth\"}"); return; }
  static String body; if (index==0) body.clear();
  body += String((const char*)data).substring(0, len);
  if (index + len < total) return;

  DynamicJsonDocument d(256);
  if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }

  JsonVariantConst root = d.as<JsonVariantConst>();
  const String mac = root.containsKey(J_MAC) ? String(root[J_MAC].as<const char*>()) : String();
  const String cmd = root.containsKey("cmd") ? String(root["cmd"].as<const char*>()) : String();
  if (!mac.length() || !cmd.length()) { sendError(req, "Missing mac/cmd"); return; }

  if (!_esn) { sendError(req, "Core not ready", 500); return; }

  uint8_t m[6]; if (!_esn->macStrToBytes(mac.c_str(), m)) { sendError(req, "Bad mac"); return; }
  esp_err_t rv = ESP_FAIL;

  if (cmd == "onoff") {
    uint8_t on = root["on"] | 0;
    rv = _esn->pwrOnOff(m, on, 40);
  } else if (cmd == "src") {
    uint8_t src = root["src"] | 0; // 0=WALL, 1=BAT, etc. (map your UI)
    rv = _esn->pwrSrcSet(m, src, 40);
  } else if (cmd == "clr_flags") {
    // New signature takes just mac (+ optional wait)
    rv = _esn->pwrClrFlags(m, 40);
  } else if (cmd == "query" || cmd == "info") {
    rv = _esn->pwrQuery(m, 40);
  } else {
    sendError(req, "Unknown cmd"); return;
  }

  DynamicJsonDocument res(128);
  res[J_OK] = (rv == ESP_OK);
  res["cmd"] = cmd;
  sendJSON(req, res);
}

void WiFiManager::hPowerInfo(AsyncWebServerRequest* req) {
  if (!isAuthed(req)) { req->send(401, "application/json", "{\"err\":\"auth\"}"); return; }
  DynamicJsonDocument d(512);
  d[J_OK] = true;

  if (_esn) {
    _esn->slotsLoadFromRegistry();
    JsonArray arr = d.createNestedArray("pms");
    for (int i=0;i<NOW_MAX_POWER;i++) {
      const auto& p = _esn->pms[i]; if (p.index <= 0) continue;
      JsonObject o = arr.createNestedObject();
      o[J_MAC]       = macBytesToStr(p.mac);
      o["idx"]       = p.index;
      o["present"]   = p.present ? true : false;
      o["rssi"]      = (int)p.lastRSSI;
      o["updated_ms"]= (uint32_t)p.lastSeenMs;
      // If your Core mirrors PMS telemetry, add fields here (voltage, src, etc)
    }
  }
  sendJSON(req, d);
}

// ============================================================================
// SEQUENCES (placeholders, unchanged)
// ============================================================================
void WiFiManager::hSeqStop(AsyncWebServerRequest* req,
                           uint8_t* data, size_t len, size_t index, size_t total) {
  (void)data;(void)len;(void)index;(void)total;
  sendOK(req);
}
void WiFiManager::hSeqStart(AsyncWebServerRequest* req,
                            uint8_t* data, size_t len, size_t index, size_t total) {
  (void)data;(void)len;(void)index;(void)total;
  sendOK(req);
}

// ============================================================================
// SENSORS — LIVE (Core mirrors + nudge via senRequest)
// ============================================================================
int WiFiManager::findSensorIndexByMac(const String& mac) const {
  if (!_esn || !mac.length()) return -1;
  ((NwCore::Core*)_esn)->slotsLoadFromRegistry();
  for (int i=0;i<NOW_MAX_SENSORS;i++) {
    const auto& s = _esn->sensors[i];
    if (s.index <= 0) continue;
    if (_esn->macEqStr(s.mac, mac.c_str())) return i;
  }
  return -1;
}

void WiFiManager::hSensorDayNight(AsyncWebServerRequest* req,
                                  uint8_t* data, size_t len, size_t index, size_t total){
  static String body; if (index == 0) body.clear();
  body += String((char*)data).substring(0, len);
  if (index + len < total) return;

  DynamicJsonDocument d(256);
  if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }
  const String mac = d.containsKey(J_MAC) ? String(d[J_MAC].as<const char*>()) : String();
  if (!mac.length()) { sendError(req, "Missing mac"); return; }

  int idx = findSensorIndexByMac(mac);
  if (idx < 0) { sendError(req, "Unknown sensor mac", 404); return; }

  bool requested = false;
  if (_esn) {
    uint8_t m[6]; if (_esn->macStrToBytes(mac.c_str(), m)) { _esn->senRequest(m, 30); requested = true; }
  }

  const auto& L = _esn->sensLive[idx];
  DynamicJsonDocument res(256);
  res[J_OK]        = true;
  res["is_day"]    = (int)L.is_day;
  res["updated_ms"]= (uint32_t)L.updated_ms;
  res["requested"] = requested;
  sendJSON(req, res);
}

void WiFiManager::hSensorEnv(AsyncWebServerRequest* req,
                             uint8_t* data, size_t len, size_t index, size_t total) {
  if (!isAuthed(req)) { req->send(401, "application/json", "{\"err\":\"auth\"}"); return; }
  static String body; if (index==0) body.clear();
  body += String((const char*)data).substring(0, len);
  if (index + len < total) return;

  DynamicJsonDocument d(256);
  if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }
  const String mac = d.containsKey(J_MAC) ? String(d[J_MAC].as<const char*>()) : String();
  if (!mac.length()) { sendError(req, "Missing mac"); return; }

  int idx = findSensorIndexByMac(mac);
  if (idx < 0) { sendError(req, "Unknown sensor mac", 404); return; }

  bool requested = false;
  if (_esn) {
    uint8_t m[6]; if (_esn->macStrToBytes(mac.c_str(), m)) { _esn->senRequest(m, 30); requested = true; }
  }

  const auto& L = _esn->sensLive[idx];   // uses t_c_x100, rh_x100, p_Pa, lux_x10, is_day, updated_ms
  DynamicJsonDocument res(384);
  res[J_OK]      = true;
  res["temp_c"]  = (L.t_c_x100 == INT32_MIN) ? NAN : (float)L.t_c_x100 / 100.0f;
  res["rh"]      = (float)L.rh_x100 / 100.0f;
  res["press_Pa"]= (int32_t)L.p_Pa;
  res["lux"]     = (float)L.lux_x10 / 10.0f;
  res["is_day"]  = (int)L.is_day;
  res["updated_ms"] = (uint32_t)L.updated_ms;
  res["requested"]  = requested;
  sendJSON(req, res);
}

void WiFiManager::hSensorTfRaw(AsyncWebServerRequest* req,
                               uint8_t* data, size_t len, size_t index, size_t total) {
  static String body; if (index == 0) body.clear();
  body += String((const char*)data).substring(0, len);
  if (index + len < total) return;

  DynamicJsonDocument d(256);
  if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }
  const String mac = d.containsKey(J_MAC) ? String(d[J_MAC].as<const char*>()) : String();
  if (!mac.length()) { sendError(req, "Missing mac"); return; }

  int idx = findSensorIndexByMac(mac);
  if (idx < 0) { sendError(req, "Unknown sensor mac", 404); return; }

  bool requested = false;
  if (_esn) {
    uint8_t m[6]; if (_esn->macStrToBytes(mac.c_str(), m)) { _esn->senRequest(m, 30); requested = true; }
  }

  const auto& L = _esn->sensLive[idx];
  DynamicJsonDocument res(256);
  res[J_OK]        = true;
  res["distA_mm"]  = (int)L.tfA_mm;
  res["distB_mm"]  = (int)L.tfB_mm;
  res["ampA"]      = 0;  // not mirrored by Core (no amplitude in SensorLive)
  res["ampB"]      = 0;
  res["updated_ms"]= (uint32_t)L.updated_ms;
  res["requested"] = requested;
  sendJSON(req, res);
}

// ============================================================================
// WIFI (align ESP-NOW channel begin/end + rebuild peer table existence-based)
// ============================================================================
void WiFiManager::hWiFiScan(AsyncWebServerRequest* req) {
  int n = _wifi->scanNetworks();
  DynamicJsonDocument d(4096);
  JsonArray arr = d.createNestedArray("aps");
  for (int i=0;i<n;i++) {
    JsonObject o = arr.createNestedObject();
    o["ssid"] = _wifi->SSID(i);
    o["rssi"] = _wifi->RSSI(i);
    o["ch"]   = _wifi->channel(i);
    o["enc"]  = _wifi->encryptionType(i);
  }
  _wifi->scanDelete();
  sendJSON(req, d);
}

void WiFiManager::hWiFiApStart(AsyncWebServerRequest* req,
                               uint8_t* data, size_t len, size_t index, size_t total) {
  static String body; if (index == 0) body.clear();
  body += String((char*)data).substring(0, len);
  if (index + len < total) return;

  DynamicJsonDocument d(512);
  if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }
  JsonVariantConst root = d.as<JsonVariantConst>();

  String apSsid = root.containsKey(J_AP_SSID) ? root[J_AP_SSID].as<String>()
                                              : _cfg->GetString(NVS_KEY_WIFI_APSSID, NVS_DEF_WIFI_APSSID);
  String apPass = root.containsKey(J_AP_PSK)  ? root[J_AP_PSK].as<String>()
                                              : _cfg->GetString(NVS_KEY_WIFI_APKEY,  NVS_DEF_WIFI_APKEY);
  if (root.containsKey(J_AP_SSID)) _cfg->PutString(NVS_KEY_WIFI_APSSID, apSsid.c_str());
  if (root.containsKey(J_AP_PSK))  _cfg->PutString(NVS_KEY_WIFI_APKEY,  apPass.c_str());

  int ch = _cfg->GetInt(NVS_KEY_NET_CHAN, NVS_DEF_NET_CHAN);
  if (root.containsKey(J_ESN_CH)) {
    int reqCh = root[J_ESN_CH].as<int>();
    if (reqCh >= 1 && reqCh <= 13) ch = reqCh;
  }
  _cfg->PutInt(NVS_KEY_NET_CHAN, ch);

  // bring up AP
  disableWiFiAP();
  _wifi->mode(WIFI_AP);
  _wifi->softAPdisconnect(true);
  delay(20);
  if (!_wifi->softAPConfig(LOCAL_IP, GATEWAY, SUBNET)) { sendError(req, "AP cfg"); return; }
  if (!_wifi->softAP(apSsid.c_str(), apPass.c_str(), (uint8_t)ch)) { sendError(req, "AP start"); return; }
  _apOn = true;

  // Align ESP-NOW by re-begin + rebuild peers from registry
  if (_esn) {
    _esn->end();
    _esn->begin((uint8_t)ch, nullptr);
    _esn->clearPeers();
    _esn->slotsLoadFromRegistry();
    auto addIf = [&](const NwCore::NodeSlot& s){
      if (s.index <= 0) return;
      if (!s.mac[0]&&!s.mac[1]&&!s.mac[2]&&!s.mac[3]&&!s.mac[4]&&!s.mac[5]) return;
      _esn->addPeer(s.mac, (uint8_t)ch, false, nullptr);
    };
    for (int i=0;i<NOW_MAX_SENSORS;i++) addIf(_esn->sensors[i]);
    for (int i=0;i<NOW_MAX_RELAYS; i++) addIf(_esn->relays[i]);
    for (int i=0;i<NOW_MAX_POWER;  i++) addIf(_esn->pms[i]);
  }

  sendOK(req);
}

void WiFiManager::hWiFiStaDisconnect(AsyncWebServerRequest* req,
                                     uint8_t* data, size_t len, size_t index, size_t total) {
  (void)data;(void)len;(void)index;(void)total;
  _wifi->disconnect(true);
  sendOK(req);
}

// ============================================================================
// CONFIG (unchanged behavior — uses applyExportedConfig which already aligns ESN)
// ============================================================================
void WiFiManager::hCfgFactoryReset(AsyncWebServerRequest* req,
                                   uint8_t* data, size_t len, size_t index, size_t total) {
  (void)data; (void)len; (void)index; (void)total;
  if (!isAuthed(req)) { req->send(401, "application/json", "{\"err\":\"auth\"}"); return; }

  if (_esn) {
    // Load registry → iterate existence-based, best-effort unpair + delPeer + clear NVS slots
    _esn->slotsLoadFromRegistry();

    // Sensors 1..16
    for (uint8_t i = 1; i <= 16; ++i) {
      uint8_t mac[6] = {0};
      char   tok[32];
      // Ask Core to see if slot i exists via index-of helpers; simpler: use serializePeers
      // Here we try to find the mac by scanning the registry arrays we just loaded:
      if (i-1 < NOW_MAX_SENSORS) {
        const auto& sl = _esn->sensors[i-1];
        if (sl.index == i && sl.mac[0]|sl.mac[1]|sl.mac[2]|sl.mac[3]|sl.mac[4]|sl.mac[5]) {
          memcpy(mac, sl.mac, 6);
          // Best-effort unpair, then drop from peer list, then clear NVS record
          _esn->prUnpair(mac, 40);
          _esn->delPeer(mac);
          _esn->icmRegistryClearSensor(i);
        }
      }
    }
    // Relays 1..16
    for (uint8_t i = 1; i <= 16; ++i) {
      if (i-1 < NOW_MAX_RELAYS) {
        const auto& rl = _esn->relays[i-1];
        if (rl.index == i && (rl.mac[0]|rl.mac[1]|rl.mac[2]|rl.mac[3]|rl.mac[4]|rl.mac[5])) {
          _esn->prUnpair(rl.mac, 40);
          _esn->delPeer(rl.mac);
          _esn->icmRegistryClearRelay(i);
        }
      }
    }
    // PMS (single)
    if (NOW_MAX_POWER >= 1) {
      const auto& pl = _esn->pms[0];
      if (pl.index == 1 && (pl.mac[0]|pl.mac[1]|pl.mac[2]|pl.mac[3]|pl.mac[4]|pl.mac[5])) {
        _esn->prUnpair(pl.mac, 40);
        _esn->delPeer(pl.mac);
        _esn->icmRegistryClearPower();
      }
    }

    // Clear in-RAM peers (registry already scrubbed)
    _esn->clearPeers();
  }

  // NVS config reset (your ConfigManager likely has a helper; otherwise minimally clear topology JSON)
  if (_cfg) {
    _cfg->PutString(NVS_KEY_TOPO_STRING, "{}");   // keep same key; empty links
  }

  DynamicJsonDocument res(64);
  res[J_OK] = true;
  sendJSON(req, res);
}


void WiFiManager::hCfgImport(AsyncWebServerRequest* req,
                             uint8_t* data, size_t len, size_t index, size_t total) {
 /* static String body; if (index == 0) body.clear();
  body += String((char*)data).substring(0, len);
  if (index + len < total) return;

  DynamicJsonDocument d(65536);
  if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }
  JsonVariantConst root = d.as<JsonVariantConst>();
  if (!root.isNull() && root.containsKey(J_CONFIG)) {
    if (applyExportedConfig(root[J_CONFIG])) { sendOK(req); return; }
    sendError(req, "Config import failed", 500);
    return;
  }
  sendError(req, "Missing 'config' field");*/
}


void WiFiManager::hCfgExport(AsyncWebServerRequest* req) {
   /* DynamicJsonDocument d(65536);
    String blob = _esn ? _esn->exportConfiguration() : "{}";
    d[J_EXPORT] = blob;
    sendJSON(req, d);*/
}

void WiFiManager::hCfgLoad(AsyncWebServerRequest* req) {
  DynamicJsonDocument d(2048);  // a bit more headroom

  // ---- STA (wifi.html expects these exact keys) ----
  d[J_WIFI_SSID] = _cfg ? _cfg->GetString(NVS_KEY_WIFI_STSSID, NVS_DEF_WIFI_STSSID)
                        : String(NVS_DEF_WIFI_STSSID);
  d[J_WIFI_PSK]  = _cfg ? _cfg->GetString(NVS_KEY_WIFI_STKEY, NVS_DEF_WIFI_STKEY)
                        : String(NVS_DEF_WIFI_STKEY);

  // ---- AP (existing) ----
  d[J_AP_SSID] = _cfg ? _cfg->GetString(NVS_KEY_WIFI_APSSID, NVS_DEF_WIFI_APSSID)
                      : String(NVS_DEF_WIFI_APSSID);
  d[J_AP_PSK]  = _cfg ? _cfg->GetString(NVS_KEY_WIFI_APKEY,      NVS_DEF_WIFI_APKEY)
                      : String(NVS_DEF_WIFI_APKEY);

  // ---- ESP-NOW channel (existing) ----
  {
    int ch = _cfg ? _cfg->GetInt(NVS_KEY_NET_CHAN, NVS_DEF_NET_CHAN) : NVS_DEF_NET_CHAN;
    if (ch < 1 || ch > 13) ch = NVS_DEF_NET_CHAN;
    d[J_ESN_CH] = ch;
  }

  // ---- BLE + Identity ----
  d[J_BLE_NAME] = _cfg ? _cfg->GetString(NVS_KEY_BLE_NAME, NVS_DEF_BLE_NAME)
                       : String(NVS_DEF_BLE_NAME);
  d[J_BLE_PASS] = _cfg ? _cfg->GetInt(NVS_KEY_BLE_PASSK, NVS_DEF_BLE_PASSK)
                       : NVS_DEF_BLE_PASSK;

  d[J_DEV_ID]        = _cfg ? _cfg->GetString(NVS_KEY_SYS_DEVID,        NVS_DEF_SYS_DEVID)        : String(NVS_DEF_SYS_DEVID);
  d[J_HOST_NAME]     = _cfg ? _cfg->GetString(NVS_KEY_WIFI_APSSID,    NVS_DEF_WIFI_APSSID)    : String(NVS_DEF_WIFI_APSSID);
  d[J_FRIENDLY_NAME] = _cfg ? _cfg->GetString(NVS_KEY_SYS_DEFNM,        NVS_DEF_SYS_DEFNM)        : String(NVS_DEF_SYS_DEFNM);

  // ---- Versions ----
  d[J_FW_VER] = _cfg ? _cfg->GetString(NVS_KEY_SYS_SWVER,    NVS_DEF_SYS_SWVER)    : String(NVS_DEF_SYS_SWVER);
  d[J_SW_VER] = _cfg ? _cfg->GetString(NVS_KEY_SYS_SWVER,    NVS_DEF_SYS_SWVER)    : String(NVS_DEF_SYS_SWVER);
  d[J_HW_VER] = _cfg ? _cfg->GetString(NVS_KEY_SYS_HWREV,    NVS_DEF_SYS_HWREV)    : String(NVS_DEF_SYS_HWREV);
  d[J_BUILD]  = _cfg ? _cfg->GetString(NVS_KEY_SYS_BUILD, NVS_DEF_SYS_BUILD) : String(NVS_DEF_SYS_BUILD);

  // ---- Access / PINs ----
  d[J_PASS_PIN] = _cfg ? _cfg->GetString(PASS_PIN_KEY, PASS_PIN_DEFAULT) : String(PASS_PIN_DEFAULT);
  d[J_WEB_USER] = _cfg ? _cfg->GetString(WEB_USER_KEY, WEB_USER_DEFAULT) : String(WEB_USER_DEFAULT);
  d[J_WEB_PASS] = _cfg ? _cfg->GetString(WEB_PASS_KEY, WEB_PASS_DEFAULT) : String(WEB_PASS_DEFAULT);

  // ---- Read-only network status (unchanged shape) ----
  JsonObject net = d.createNestedObject("net");
  if (_apOn) {
    net[J_MODE] = "AP";
    net["ip"]   = _wifi ? _wifi->softAPIP().toString() : String();
    net["ch"]   = _cfg ? _cfg->GetInt(NVS_KEY_NET_CHAN, NVS_DEF_NET_CHAN) : NVS_DEF_NET_CHAN;
  } else if (_wifi && _wifi->status() == WL_CONNECTED) {
    net[J_MODE] = "STA";
    net["ip"]   = _wifi->localIP().toString();
    net["ch"]   = _wifi->channel();
    net["rssi"] = _wifi->RSSI();
  } else {
    net[J_MODE] = "OFF";
  }

  sendJSON(req, d);
}
void WiFiManager::hCfgSave(AsyncWebServerRequest* req,
                           uint8_t* data, size_t len, size_t index, size_t total) {
  static String body; if (index == 0) body.clear();
  body += String((char*)data).substring(0, len);
  if (index + len < total) return;

  DynamicJsonDocument d(2048);
  if (deserializeJson(d, body)) { sendError(req, "Invalid JSON"); return; }
  JsonVariantConst root = d.as<JsonVariantConst>();

  if (root.containsKey(J_FW_VER)) _cfg->PutString(NVS_KEY_SYS_SWVER, root[J_FW_VER].as<const char*>());
  if (root.containsKey(J_SW_VER)) _cfg->PutString(NVS_KEY_SYS_SWVER, root[J_SW_VER].as<const char*>());
  if (root.containsKey(J_HW_VER)) _cfg->PutString(NVS_KEY_SYS_HWREV, root[J_HW_VER].as<const char*>());
  if (root.containsKey(J_BUILD))  _cfg->PutString(NVS_KEY_SYS_BUILD, root[J_BUILD].as<const char*>());

  if (root.containsKey(J_WEB_USER)) _cfg->PutString(WEB_USER_KEY, root[J_WEB_USER].as<const char*>());
  if (root.containsKey(J_WEB_PASS)) _cfg->PutString(WEB_PASS_KEY, root[J_WEB_PASS].as<const char*>());

  if (root.containsKey(J_PASS_PIN)) _cfg->PutString(PASS_PIN_KEY, root[J_PASS_PIN].as<const char*>());

  // keep channel write; actual Core alignment happens in applyExportedConfig()/hWiFiApStart
  if (root.containsKey(J_ESN_CH)) {
    int ch = root[J_ESN_CH].as<int>();
    if (ch >= 1 && ch <= 13) _cfg->PutInt(NVS_KEY_NET_CHAN, ch);
  }

  if (root.containsKey(J_WIFI_SSID)) _cfg->PutString(NVS_KEY_WIFI_STSSID, root[J_WIFI_SSID].as<const char*>());
  if (root.containsKey(J_WIFI_PSK))  _cfg->PutString(NVS_KEY_WIFI_STKEY,  root[J_WIFI_PSK].as<const char*>());
  sendOK(req);
}

// ============================================================================
// HTML ROOT (unchanged)
// ============================================================================
void WiFiManager::handleRoot(AsyncWebServerRequest* request) {
  if (!isAuthed(request)) { request->redirect(PAGE_LOGIN); return; }
  request->redirect(PAGE_HOME);
}
