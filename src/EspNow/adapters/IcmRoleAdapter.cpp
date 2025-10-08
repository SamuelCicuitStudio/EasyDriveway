#include "IcmRoleAdapter.h"
#include "../Opcodes.h"
#include "../EspNowCore.h"
#include "../TopologyTlv.h"
#include <cstring>

namespace espnow {

bool IcmRoleAdapter::handleRequest(const EspNowMsg& in, EspNowResp& out){
  switch(in.type){
    case PAIR_EXCHANGE: {
      // Minimal pairing: just ACK via built-in ESP-NOW, and let WiFiManager add peer out-of-band
      // Optionally parse MAC from payload and add peer
      if(in.payload_len == 6){
        EspNowCore::instance()->addPeer(in.payload, false, nullptr);
      }
      out.out_len = 0; // no body
      return true;
    }
    case REMOVE_PEER: {
      if(in.payload_len == 6){
        EspNowCore::instance()->removePeer(in.payload);
        out.out_len = 0; return true;
      }
      return false;
    }
    case PUSH_TOPOLOGY: {
      // forward to node is handled by controller, but here we may import locally
      EspNowCore::instance()->importLocalTopology(in.payload, in.payload_len);
      out.out_len = 0; return true;
    }
    default: break;
  }
  // fallthrough to common ops handled by other role adapters if needed
  return false;
}

void IcmRoleAdapter::onTopologyPushed(const uint8_t* tlv, uint16_t len){
  (void)tlv; (void)len;
}

} // namespace espnow
