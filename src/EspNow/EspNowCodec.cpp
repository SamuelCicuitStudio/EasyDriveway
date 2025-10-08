// EspNowCodec.cpp
#include "EspNowCodec.h"

namespace espnow {

bool compose(Packet& out,
             const NowHeader& inHdr,
             const NowAuth128& inDev,
             const NowTopoToken128* inTopo,
             const void* body, uint16_t bodyLen)
{
  // Basic guards
  if (inHdr.proto_ver != NOW_PROTO_VER) return false;
  if (bodyLen > NOW_MAX_BODY)           return false;

  const bool needTopo = (inHdr.flags & NOW_FLAGS_HAS_TOPO) != 0;
  if (needTopo && !inTopo)              return false;
  if (!needTopo && inTopo)              return false; // prevent accidental mismatch

  const uint16_t prefix = wirePrefixLen(inHdr.flags);
  const uint32_t total  = (uint32_t)prefix + (uint32_t)bodyLen;
  if (total > NOW_MAX_FRAME)            return false;

  // Lay out into the buffer
  uint16_t off = 0;
  memcpy(out.buf + off, &inHdr, sizeof(NowHeader));  off += sizeof(NowHeader);
  memcpy(out.buf + off, &inDev, sizeof(NowAuth128)); off += sizeof(NowAuth128);
  if (needTopo) {
    memcpy(out.buf + off, inTopo, sizeof(NowTopoToken128));
    off += sizeof(NowTopoToken128);
  }
  if (bodyLen && body) {
    memcpy(out.buf + off, body, bodyLen);
    off += bodyLen;
  }

  // Fill output views
  out.len  = off;
  out.hdr  = reinterpret_cast<const NowHeader*>(out.buf);
  out.dev  = reinterpret_cast<const NowAuth128*>(out.buf + sizeof(NowHeader));
  out.topo = needTopo
             ? reinterpret_cast<const NowTopoToken128*>(out.buf + sizeof(NowHeader) + sizeof(NowAuth128))
             : nullptr;

  const uint16_t bodyOff = prefix;
  out.body    = (bodyLen ? (out.buf + bodyOff) : nullptr);
  out.bodyLen = bodyLen;

  return true;
}

ParseResult parse(const uint8_t* raw, uint16_t rawLen, Packet& out)
{
  // Minimum: header + device token
  const uint16_t minLen = sizeof(NowHeader) + sizeof(NowAuth128);
  if (rawLen < minLen) return PARSE_TOO_SMALL;
  if (rawLen > NOW_MAX_FRAME) return PARSE_OVERFLOW;

  // Copy into our stable buffer, then set views
  memcpy(out.buf, raw, rawLen);
  out.len = rawLen;

  out.hdr = reinterpret_cast<const NowHeader*>(out.buf);
  if (out.hdr->proto_ver != NOW_PROTO_VER) return PARSE_BAD_VER;

  // Device token view
  const uint16_t afterHdr = sizeof(NowHeader);
  out.dev = reinterpret_cast<const NowAuth128*>(out.buf + afterHdr);

  // Topology token (optional)
  const bool hasTopo = (out.hdr->flags & NOW_FLAGS_HAS_TOPO) != 0;
  uint16_t off = afterHdr + sizeof(NowAuth128);
  if (hasTopo) {
    if (rawLen < off + sizeof(NowTopoToken128)) return PARSE_FLAG_MISMATCH;
    out.topo = reinterpret_cast<const NowTopoToken128*>(out.buf + off);
    off += sizeof(NowTopoToken128);
  } else {
    out.topo = nullptr;
  }

  // Body view
  if (rawLen < off) return PARSE_OVERFLOW;
  out.bodyLen = (uint16_t)(rawLen - off);
  out.body    = (out.bodyLen ? (out.buf + off) : nullptr);

  return PARSE_OK;
}

} // namespace espnow
