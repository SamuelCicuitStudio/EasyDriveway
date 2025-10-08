#include "TopologyTlv.h"
#include <cstring>

namespace espnow {

// TLV types
static constexpr uint8_t T_DEVICE_TOKEN = 0x01;
static constexpr uint8_t T_ROLE         = 0x02;
static constexpr uint8_t T_NEIGHBORS    = 0x03;
static constexpr uint8_t T_ROLE_PARAMS  = 0x04;
static constexpr uint8_t T_EMU_COUNT    = 0x05;

static Topology g_localTopo;

static inline void putU16(std::vector<uint8_t>& v, uint16_t x){
  v.push_back(uint8_t(x & 0xFF)); v.push_back(uint8_t((x>>8)&0xFF));
}

static inline uint16_t getU16(const uint8_t* p){ return uint16_t(p[0] | (uint16_t(p[1])<<8)); }

bool topoEncode(const Topology& t, std::vector<uint8_t>& out){
  out.clear();
  // ROLE
  out.push_back(T_ROLE); out.push_back(1); out.push_back(t.role);
  // TOKEN
  out.push_back(T_DEVICE_TOKEN); out.push_back(32); for(int i=0;i<32;++i) out.push_back(uint8_t(t.token[i]));
  // NEIGHBORS
  if(!t.neighbors.empty()){
    out.push_back(T_NEIGHBORS);
    uint16_t len = (uint16_t)(t.neighbors.size()*6);
    out.push_back(uint8_t(len & 0xFF)); out.push_back(uint8_t((len>>8)&0xFF));
    for(auto& m: t.neighbors) for(int i=0;i<6;++i) out.push_back(m[i]);
  }
  // ROLE_PARAMS
  if(!t.roleParams.empty()){
    out.push_back(T_ROLE_PARAMS);
    uint16_t len = (uint16_t)t.roleParams.size();
    out.push_back(uint8_t(len & 0xFF)); out.push_back(uint8_t((len>>8)&0xFF));
    out.insert(out.end(), t.roleParams.begin(), t.roleParams.end());
  }
  // EMU_COUNT
  if(t.emuCount){
    out.push_back(T_EMU_COUNT); out.push_back(1); out.push_back(t.emuCount);
  }
  return true;
}

bool topoDecode(const uint8_t* tlv, uint16_t len, Topology& out){
  out = Topology{};
  const uint8_t* p = tlv;
  const uint8_t* e = tlv + len;
  while(p+2 <= e){
    uint8_t t = p[0]; uint8_t l = p[1]; p+=2;
    uint16_t L = l;
    if(l==0xFF){ if(p+2>e) return false; L = getU16(p); p+=2; }
    if(p+L>e) return false;
    switch(t){
      case T_ROLE: if(L>=1) out.role = p[0]; break;
      case T_DEVICE_TOKEN: if(L>=32) std::memcpy(out.token, p, 32); break;
      case T_NEIGHBORS: {
        out.neighbors.clear();
        for(uint16_t i=0;i+6<=L;i+=6){ std::array<uint8_t,6> m; std::memcpy(m.data(), p+i, 6); out.neighbors.push_back(m); }
      } break;
      case T_ROLE_PARAMS: out.roleParams.assign(p, p+L); break;
      case T_EMU_COUNT: if(L>=1) out.emuCount = p[0]; break;
    }
    p += L;
  }
  return true;
}

// Local store glue
void setLocalTopology(const Topology& t){ g_localTopo = t; }
const Topology& getLocalTopology(){ return g_localTopo; }
bool importLocalTopology(const uint8_t* tlv, uint16_t len){ Topology t; if(!topoDecode(tlv,len,t)) return false; g_localTopo = t; return true; }
bool exportLocalTopology(std::vector<uint8_t>& tlvOut){ return topoEncode(g_localTopo, tlvOut); }

} // namespace espnow
