#include "IcmRoleAdapter.h"
#include "../Opcodes.h"
#include "../EspNowCore.h"
#include "../TopologyTlv.h"
#include "CommonOps.h"
#include <cstring>

namespace espnow {

bool IcmRoleAdapter::handleRequest(const EspNowMsg& in, EspNowResp& out){
  switch(in.type){
    case PAIR_EXCHANGE: {
      if(in.payload_len == 6){
        EspNowCore::instance()->addPeer(in.payload, false, nullptr);
      }
      out.out_len = 0; return true;
    }
    case REMOVE_PEER: {
      if(in.payload_len == 6){
        EspNowCore::instance()->removePeer(in.payload);
        out.out_len = 0; return true;
      }
      return false;
    }
    case PUSH_TOPOLOGY: {
      EspNowCore::instance()->importLocalTopology(in.payload, in.payload_len);
      out.out_len = 0; return true;
    }
    case GET_TOPOLOGY: {
      auto* core = EspNowCore::instance();
      if(!core) return false;
      std::vector<uint8_t> tlv; core->exportLocalTopology(tlv);
      if(tlv.size() > 256) return false;
      std::memcpy(out.out, tlv.data(), tlv.size());
      out.out_len = (uint16_t)tlv.size(); return true;
    }
    default: return false;
  }
}

void IcmRoleAdapter::onTopologyPushed(const uint8_t* tlv, uint16_t len){
  (void)tlv; (void)len;
}

} // namespace espnow
