// RoleAdapters/Adapter_PMS.h
#pragma once
/**
 * Adapter_PMS — Power Management role adapter
 *
 * Managers:
 *  - CoolingManager (fan/setpoint)
 *  - DS18B20U temperature sensor (local board temp)
 *  - LogFS (optional)
 *  - External voltage/current/fault telemetry via callbacks
 *
 * Ops handled:
 *  - NOW_MSG_PMS_STATUS  → reply with NowPmsStatus
 *  - NOW_MSG_CONFIG_WRITE (key "COOL..") → COOL_SET{pct} → CoolingManager::setManualSpeedPct(pct)
 *
 * Notes:
 *  - Echoes caller's seq in replies for reliable ACK matching.
 *  - Keeps replies ≤ NOW_MAX_BODY by using the fixed NowPmsStatus payload.
 */

#include <functional>
#include "IRoleAdapter.h"
#include "EspNow/EspNowCompat.h"
#include "Peripheral/CoolingManager.h"
#include "Peripheral/DS18B20U.h"
#include "Peripheral/LogFS.h"

namespace espnow {

class Adapter_PMS : public IRoleAdapter {
public:
  struct Telemetry {
    std::function<uint16_t()> readVbus_mV;   // input voltage (mV)
    std::function<uint16_t()> readVsys_mV;   // system rail (mV)
    std::function<int16_t()>  readIout_mA;   // load/output current (mA), signed
    std::function<uint16_t()> readFaults;    // bitmask
  };

  Adapter_PMS(CoolingManager* cool,
              DS18B20U* ds18,
              LogFS* log,
              const Telemetry& tele,
              const NowAuth128& selfAuthToken,
              const NowTopoToken128* topoTokenOrNull = nullptr,
              uint16_t topoVerHint = 0);

  uint8_t role() const override { return NOW_ROLE_PMS; }

  bool handle(const uint8_t srcMac[6], const Packet& in, Packet& out) override;
  void tick() override {}

  void setTopoVersion(uint16_t v) { _topoVer = v; }

private:
  void fillHeaderEcho(NowHeader& h, uint8_t msg, uint16_t echoSeq, uint16_t flags = 0) const;
  NowPmsStatus makeStatus() const;

  // Parse CONFIG_WRITE as COOL_SET when key starts with "COOL"
  bool tryHandleCoolSet(const NowConfigWrite& cfg, const uint8_t* bodyEnd, NowPmsStatus& outStatus) const;

private:
  CoolingManager* _cool{nullptr};
  DS18B20U*       _ds{nullptr};
  LogFS*          _log{nullptr};
  Telemetry       _t;

  NowAuth128      _auth{};
  NowTopoToken128 _topo{};
  bool            _hasTopo{false};
  uint8_t         _selfMac[6]{};
  uint8_t         _selfRole{NOW_ROLE_PMS};
  uint16_t        _topoVer{0};
};

} // namespace espnow
