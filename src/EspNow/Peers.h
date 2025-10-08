#pragma once
#include <cstdint>
#include <cstddef>

namespace espnow {

struct Peer {
  uint8_t  mac[6]{};
  uint8_t  role{0};
  int32_t  rssi{INT32_MIN};
  uint32_t lastSeenMs{0};
  char     name[32]{};
  char     token[32]{};
};

class Peers {
public:
  Peers();
  bool add(const uint8_t mac[6], uint8_t role=0);
  bool remove(const uint8_t mac[6]);
  bool has(const uint8_t mac[6]) const;
  bool setRole(const uint8_t mac[6], uint8_t role);
  bool setName(const uint8_t mac[6], const char* name32);
  bool setToken(const uint8_t mac[6], const char token32[32]);
  bool updateSeen(const uint8_t mac[6], int32_t rssi, uint32_t nowMs);
  size_t count() const;
  bool getByIndex(size_t i, Peer& out) const;
  bool getByMac(const uint8_t mac[6], Peer& out) const;
  void forEach(bool (*fn)(const Peer&)) const;

private:
  static constexpr size_t MAX_PEERS = 32;
  Peer table_[MAX_PEERS];
  size_t used_;
  int indexOf(const uint8_t mac[6]) const;
};

} // namespace espnow
