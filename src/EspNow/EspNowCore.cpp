#include "EspNowCore.h"
#include "IRoleAdapter.h"
#include "ServiceRefs.h"
#include "Opcodes.h"
#include "Frame.h"
#include "TopologyTlv.h"
#include "RoleFactory.h"

#include <cstring>
#include <vector>

// Arduino/ESP-IDF includes
#include <WiFi.h>
#include <esp_now.h>
#include <esp_system.h>
#include <esp_wifi.h>

namespace espnow {

static EspNowCore* g_core = nullptr;

EspNowCore* EspNowCore::instance(){ return g_core; }

bool EspNowCore::begin(){
  g_core = this;
  WiFi.mode(WIFI_STA);
  if(esp_now_init() != ESP_OK){ return false; }
  esp_now_register_send_cb(+[](const uint8_t* mac_addr, esp_now_send_status_t status){
    EspNowCore::onSendStatic(mac_addr, status);
  });
  // Use vendor-specific arg to get RSSI where supported (ESP-IDF 5.x)
  esp_now_register_recv_cb([](const uint8_t* mac, const uint8_t* data, int len){
    // rssi not exposed directly here; we'll query station info if possible
    EspNowCore::onRecvStatic(mac, data, len, nullptr);
  });
  return true;
}

void EspNowCore::setServices(const ServiceRefs* s){
  services_ = s;
  if(role_) role_->mount(s);
}

void EspNowCore::setRoleAdapter(IRoleAdapter* r){
  role_ = r;
  if(role_) role_->mount(services_);
}

bool EspNowCore::addPeer(const uint8_t mac[6], bool encrypt, const uint8_t* lmk){
  esp_now_peer_info_t p{};
  std::memcpy(p.peer_addr, mac, 6);
  p.ifidx = ESP_IF_WIFI_STA;
  p.encrypt = encrypt;
  if(encrypt && lmk) std::memcpy(p.lmk, lmk, 16);
  esp_now_del_peer(mac); // idempotent
  if(esp_now_add_peer(&p) == ESP_OK){
    peers_.add(mac, 0);
    return true;
  }
  return false;
}

bool EspNowCore::removePeer(const uint8_t mac[6]){
  peers_.remove(mac);
  return esp_now_del_peer(mac) == ESP_OK;
}

bool EspNowCore::sendFrame(const uint8_t* mac, uint8_t type, uint8_t flags, uint16_t corr, const void* payload, uint16_t len){
  uint8_t buf[250];
  if(len + sizeof(EspNowHeader) > sizeof(buf)) return false;
  EspNowHeader* h = reinterpret_cast<EspNowHeader*>(buf);
  h->type = type; h->flags = flags; h->corr = corr;
  if(payload && len) std::memcpy(buf + sizeof(EspNowHeader), payload, len);
  return esp_now_send(mac, buf, sizeof(EspNowHeader) + len) == ESP_OK;
}

bool EspNowCore::unicast(const uint8_t mac[6], uint8_t type, const void* payload, uint16_t len, uint16_t corr){
  return sendFrame(mac, type, 0x00, corr, payload, len);
}

bool EspNowCore::broadcast(uint8_t type, const void* payload, uint16_t len, uint16_t corr){
  static const uint8_t bc[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
  return sendFrame(bc, type, 0x00, corr, payload, len);
}

void EspNowCore::onSendStatic(const uint8_t* mac_addr, esp_now_send_status_t status){
  (void)mac_addr; (void)status;
  // ack is handled by esp-now; no queues here
}

void EspNowCore::onRecvStatic(const uint8_t* mac, const uint8_t* data, int len, void* arg){
  (void)arg;
  int32_t rssi = 0;
  wifi_sta_list_t sta_list{};
  if(esp_wifi_ap_get_sta_list(&sta_list) == ESP_OK){
    // Attempt to find RSSI (not guaranteed for ESP-NOW peers)
    for(int i=0;i<sta_list.num; ++i){
      if(std::memcmp(sta_list.sta[i].mac, mac, 6)==0){ rssi = sta_list.sta[i].rssi; break; }
    }
  }
  if(g_core) g_core->onRecv(mac, data, len, rssi);
}

void EspNowCore::onRecv(const uint8_t* mac, const uint8_t* data, int len, int32_t rssi){
  if(len < (int)sizeof(EspNowHeader)) return;
  const EspNowHeader* h = reinterpret_cast<const EspNowHeader*>(data);
  EspNowMsg in{ h->type, h->flags, h->corr, data + sizeof(EspNowHeader), (uint16_t)(len - (int)sizeof(EspNowHeader)) };
  peers_.updateSeen(mac, rssi, (uint32_t)millis()); // millis() available on Arduino

  if(tap_) tap_(mac, in);

  if(!role_) return;
  uint8_t outBuf[256] = {0};
  EspNowResp out{ outBuf, 0 };
  bool ok = false;
  if(!isResponse(in.flags)){
    ok = role_->handleRequest(in, out);
    if(ok && out.out_len){
      sendFrame(mac, in.type, asResponse(in.flags), in.corr, out.out, out.out_len);
    }
  } else {
    // Response received - could be logged via tap_
  }
}

bool EspNowCore::pushTopology(const uint8_t mac[6], const void* tlv, uint16_t len){
  return sendFrame(mac, PUSH_TOPOLOGY, 0x00, 0, tlv, len);
}

void EspNowCore::setLocalTopology(const Topology& t){ topo_ = t; espnow::setLocalTopology(t); }
const Topology& EspNowCore::getLocalTopology() const { return topo_; }
bool EspNowCore::exportLocalTopology(std::vector<uint8_t>& tlvOut) const { return espnow::exportLocalTopology(tlvOut); }
bool EspNowCore::importLocalTopology(const uint8_t* tlv, uint16_t len){ bool ok = espnow::importLocalTopology(tlv,len); if(ok) topo_ = espnow::getLocalTopology(); return ok; }

bool EspNowCore::refreshDeviceInfoFromNvs(){
  // NOTE: Use existing NVS helpers if available via ServiceRefs->nvs
  // We avoid inventing keys; fall back to chip ID where needed.
  std::memset(&dev_, 0, sizeof(dev_));
  dev_.role = getLocalRoleCode();
  // deviceId fallback: chip id
  uint64_t mac = 0; esp_read_mac(reinterpret_cast<uint8_t*>(&mac), ESP_MAC_WIFI_STA);
  snprintf(dev_.deviceId, sizeof(dev_.deviceId), "%08X%08X", (unsigned)(mac>>32), (unsigned)(mac & 0xFFFFFFFF));
  // Leave other fields zero unless NVS is provided; WiFiManager can set later.
  return true;
}

} // namespace espnow
