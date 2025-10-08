// RoleAdapters/Adapter_ICM.h
#pragma once
/**
 * Adapter_ICM — Control-plane adapter for the ICM (coordinator)
 *
 * Handles:
 *  - NOW_MSG_PAIR_REQ      → if provisioning allowed, persist peer and reply with a tiny ACK
 *  - NOW_MSG_NET_SET_CHAN  → persist channel (via Peers::setChannel) and ACK
 *  - NOW_MSG_CONFIG_WRITE  → when key == "CHAN__", treat as NET_SET_CHAN
 *  - NOW_MSG_TIME_SYNC     → ignored on ICM (heartbeat is the authority)
 */

#include <stdint.h>
#include <string.h>
#include "IRoleAdapter.h"
#include "EspNowAPI.h"
#include "EspNowCodec.h"
#include "EspNowCompat.h"
#include "EspNowPeers.h"
#include "Peripheral/RTCManager.h"
#include "Peripheral/LogFS.h"
#include "Peripheral/RGBLed.h"
#include "Peripheral/BuzzerManager.h"
#include "NVS/NVSConfig.h"   // for NVS_DEF_CHAN

namespace espnow {

class Adapter_ICM : public IRoleAdapter {
public:
  Adapter_ICM(Peers* peers,
              RTCManager* rtc,
              LogFS* log,
              const NowAuth128& selfAuthToken,
              const NowTopoToken128* topoTokenOrNull = nullptr,
              RGBLed* rgb = nullptr,
              BuzzerManager* buz = nullptr,
              uint16_t topoVerHint = 0)
  : _peers(peers), _rtc(rtc), _log(log), _auth(selfAuthToken),
    _rgb(rgb), _buz(buz), _topoVer(topoVerHint)
  {
    _hasTopo = (topoTokenOrNull != nullptr);
    if (_hasTopo) _topo = *topoTokenOrNull;
    now_get_mac_sta(_selfMac);
  }

  uint8_t role() const override { return NOW_ROLE_ICM; }

  bool handle(const uint8_t srcMac[6], const Packet& in, Packet& out) override;
  void tick() override {} // TIME_SYNC cadence handled by heartbeat

  // Enable/disable provisioning window (pairing admission)
  void setProvisioning(bool en) { _provisioning = en; }
  void setTopoVersion(uint16_t v) { _topoVer = v; }

private:
  // Compose a header echoing the caller's seq (for reliable ACK matching)
  void fillHeaderEcho(NowHeader& h, uint8_t msg, uint16_t echoSeq, uint16_t flags = 0) const;

  // Small local MAC -> "AA:BB:CC:DD:EE:FF"
  static void fmtMac(const uint8_t mac[6], char out[18]) {
    static const char* hex = "0123456789ABCDEF";
    for (int i=0;i<6;i++) {
      out[i*3+0] = hex[(mac[i]>>4)&0xF];
      out[i*3+1] = hex[(mac[i]>>0)&0xF];
      out[i*3+2] = (i==5)?'\0':':';
    }
  }

  static bool validChan(uint8_t ch) { return ch >= 1 && ch <= 13; }

  // --- Pairing helpers ---
  struct PairResult {
    bool ok;
    const char* reason;
    PairResult(bool o=false, const char* r="denied") : ok(o), reason(r) {}
  };
  PairResult doPair(const uint8_t reqMac[6], uint8_t reqRole,
                    const char* name, const NowAuth128& token);

  // --- Channel set handlers (direct op and CONFIG_WRITE) ---
  bool handleSetChannel(uint8_t newChan, NowNetSetChan& echoBody);

  static inline bool keyEqualsCHAN(const char key[6]) {
    return key[0]=='C'&&key[1]=='H'&&key[2]=='A'&&key[3]=='N'&&key[4]=='_'&&key[5]=='_';
  }

private:
  Peers*         _peers{nullptr};
  RTCManager*    _rtc{nullptr};
  LogFS*         _log{nullptr};
  RGBLed*        _rgb{nullptr};
  BuzzerManager* _buz{nullptr};

  NowAuth128      _auth{};
  NowTopoToken128 _topo{};
  bool            _hasTopo{false};
  uint8_t         _selfMac[6]{};
  uint16_t        _topoVer{0};
  bool            _provisioning{false}; // start closed; open it explicitly when ready
};

} // namespace espnow
