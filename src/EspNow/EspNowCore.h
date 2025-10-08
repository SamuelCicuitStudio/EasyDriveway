#pragma once
#include <cstdint>
#include <vector>

#include <esp_now.h>   // ensure esp_now_send_status_t is declared
#include "Frame.h"
#include "Peers.h"
#include "TopologyTlv.h"
#include "DeviceInfo.h"

namespace espnow {

class IRoleAdapter;
struct ServiceRefs;

class EspNowCore {
public:
  bool begin();   // WiFi.mode(WIFI_STA), esp_now_init, register cbs

  void setServices(const ServiceRefs* s);
  void setRoleAdapter(IRoleAdapter* r);

  // ========= Sending =========
  bool unicast(const uint8_t mac[6], uint8_t type, const void* payload, uint16_t len, uint16_t corr=0);
  bool broadcast(uint8_t type, const void* payload, uint16_t len, uint16_t corr=0);

  // ========= Peers =========
  bool addPeer(const uint8_t mac[6], bool encrypt=false, const uint8_t* lmk=nullptr);
  bool removePeer(const uint8_t mac[6]);
  const Peers& peers() const { return peers_; }

  // ========= Topology =========
  bool pushTopology(const uint8_t mac[6], const void* tlv, uint16_t len);
  void setLocalTopology(const Topology& t);
  const Topology& getLocalTopology() const;
  bool exportLocalTopology(std::vector<uint8_t>& tlvOut) const;
  bool importLocalTopology(const uint8_t* tlv, uint16_t len);

  // ========= Device Info =========
  const DeviceInfo& getLocalDeviceInfo() const { return dev_; }
  bool refreshDeviceInfoFromNvs();

  // ========= Logging tap =========
  using RxTap = void(*)(const uint8_t mac[6], const EspNowMsg&);
  void setRxTap(RxTap t){ tap_ = t; }

  // Singleton-ish access for adapters needing topology
  static EspNowCore* instance();

private:
  static void onSendStatic(const uint8_t* mac_addr, esp_now_send_status_t status);
  static void onRecvStatic(const uint8_t* mac, const uint8_t* data, int len);

  void onRecv(const uint8_t* mac, const uint8_t* data, int len, int32_t rssi);
  bool sendFrame(const uint8_t* mac, uint8_t type, uint8_t flags, uint16_t corr, const void* payload, uint16_t len);

  Peers peers_;
  IRoleAdapter* role_{nullptr};
  const ServiceRefs* services_{nullptr};
  DeviceInfo dev_{};
  Topology topo_{};
  RxTap tap_{nullptr};
};

} // namespace espnow
