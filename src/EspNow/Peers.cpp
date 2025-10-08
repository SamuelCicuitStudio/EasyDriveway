#include "Peers.h"
#include <cstring>

namespace espnow {

Peers::Peers() : used_(0) {}

static inline bool macEq(const uint8_t* a, const uint8_t* b){ return std::memcmp(a,b,6)==0; }

int Peers::indexOf(const uint8_t mac[6]) const {
  for(size_t i=0;i<used_;++i) if(macEq(table_[i].mac, mac)) return (int)i;
  return -1;
}

bool Peers::add(const uint8_t mac[6], uint8_t role){
  int idx = indexOf(mac);
  if(idx >= 0) { table_[idx].role = role; return true; }
  if(used_ >= MAX_PEERS) return false;
  std::memcpy(table_[used_].mac, mac, 6);
  table_[used_].role = role;
  table_[used_].rssi = INT32_MIN;
  table_[used_].lastSeenMs = 0;
  table_[used_].name[0] = 0;
  std::memcpy(table_[used_].token, "\0\0\0\0\0\0\0\0", 8);
  used_++;
  return true;
}

bool Peers::remove(const uint8_t mac[6]){
  int idx = indexOf(mac);
  if(idx < 0) return false;
  if((size_t)idx != used_-1) table_[idx] = table_[used_-1];
  used_--;
  return true;
}

bool Peers::has(const uint8_t mac[6]) const { return indexOf(mac) >= 0; }

bool Peers::setRole(const uint8_t mac[6], uint8_t role){
  int idx = indexOf(mac);
  if(idx < 0) return false;
  table_[idx].role = role;
  return true;
}

bool Peers::setName(const uint8_t mac[6], const char* name32){
  int idx = indexOf(mac);
  if(idx < 0) return false;
  std::strncpy(table_[idx].name, name32, sizeof(table_[idx].name));
  table_[idx].name[sizeof(table_[idx].name)-1] = 0;
  return true;
}

bool Peers::setToken(const uint8_t mac[6], const char token32[32]){
  int idx = indexOf(mac);
  if(idx < 0) return false;
  std::memcpy(table_[idx].token, token32, 32);
  return true;
}

bool Peers::updateSeen(const uint8_t mac[6], int32_t rssi, uint32_t nowMs){
  int idx = indexOf(mac);
  if(idx < 0) { if(!add(mac)) return false; idx = indexOf(mac); }
  table_[idx].rssi = rssi;
  table_[idx].lastSeenMs = nowMs;
  return true;
}

size_t Peers::count() const { return used_; }

bool Peers::getByIndex(size_t i, Peer& out) const {
  if(i>=used_) return false;
  out = table_[i];
  return true;
}

bool Peers::getByMac(const uint8_t mac[6], Peer& out) const {
  int idx = indexOf(mac);
  if(idx < 0) return false;
  out = table_[idx];
  return true;
}

void Peers::forEach(bool (*fn)(const Peer&)) const {
  for(size_t i=0;i<used_;++i) if(!fn(table_[i])) break;
}

} // namespace espnow
