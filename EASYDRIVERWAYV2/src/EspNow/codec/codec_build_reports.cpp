// codec/codec_build_reports.cpp

#include "EspNow/EspNowStack.h"
#include <cstring>

namespace espnow
{
// Add report-size guards here when you freeze those structs’ sizes in EspNowAPI.h.
// Example (uncomment once present in API):
// static_assert(sizeof(NowSensReport) == /*X*/, "NowSensReport size mismatch");
// static_assert(sizeof(NowRlyState)   == /*Y*/, "NowRlyState size mismatch");
// static_assert(sizeof(NowPmsStatus)  == /*Z*/, "NowPmsStatus size mismatch");

// Example future helpers:
// void buildSensReport(NowSensReport& r) { std::memset(&r, 0, sizeof(r)); }
// bool parseSensReport(const uint8_t* buf, size_t len, NowSensReport& out);

} // namespace espnow
