// codec/codec_build_control.cpp

#include "EspNow/EspNowStack.h"
#include <cstring>

namespace espnow {

// Keep this TU so callers can include a single header and link cleanly.
// If you later add explicit builder helpers (e.g., buildPing, buildCtrlRelay),
// define them here. For now, we only need size sanity where available.

// Where sizes are defined in the API, assert them here:
static_assert(sizeof(NowNetSetChan) == 4, "NowNetSetChan must be 4 bytes");

// If you add explicit control builders later, they go below.
// Example skeletons you might enable later (uncomment & adapt fields):
//
// void buildPing(NowPing& p) { std::memset(&p, 0, sizeof(p)); /* fill fields */ }
// void buildConfigWrite(NowConfigWrite& cw, uint16_t key, uint16_t len) {
//   std::memset(&cw, 0, sizeof(cw)); /* set key/len etc. */
// }
// void buildCtrlRelay(NowCtrlRelay& cr, uint8_t virt, uint8_t action) {
//   std::memset(&cr, 0, sizeof(cr)); cr.virt_id = virt; cr.action = action;
// }

} // namespace espnow
