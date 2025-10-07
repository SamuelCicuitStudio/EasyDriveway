// codec/codec_build_fw.cpp

#include "EspNow/EspNowStack.h"
#include <cstring>

namespace espnow
{
// v2H FW size guards (from API)
static_assert(sizeof(NowFwBegin)  == 52, "NowFwBegin must be 52 bytes");
static_assert(sizeof(NowFwChunk)  == 12, "NowFwChunk must be 12 bytes");
static_assert(sizeof(NowFwStatus) == 16, "NowFwStatus must be 16 bytes");
static_assert(sizeof(NowFwCommit) == 8,  "NowFwCommit must be 8 bytes");
static_assert(sizeof(NowFwAbort)  == 8,  "NowFwAbort must be 8 bytes");

// Example initializers (uncomment/use when needed):
// void buildFwBegin(NowFwBegin& fb)   { std::memset(&fb, 0, sizeof(fb)); }
// void buildFwChunk(NowFwChunk& fc)   { std::memset(&fc, 0, sizeof(fc)); }
// void buildFwCommit(NowFwCommit& cm) { std::memset(&cm, 0, sizeof(cm)); }
// void buildFwAbort(NowFwAbort& ab)   { std::memset(&ab, 0, sizeof(ab)); }

} // namespace espnow
