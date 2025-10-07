#ifndef ESPNOW_STACK_H
#define ESPNOW_STACK_H

// Central public API header: declarations only.
// Include your canonical wire protocol definitions.
#include "EspNowAPI.h"

// Forward declares to avoid heavy includes here.
class RelayManager;
class SensorManager;
class BuzzerManager;
class RGBLed;
class CoolingManager;
class RTCManager;
class LogFS;
class NvsManager;

struct ByteSpan {
  const unsigned char* data;
  unsigned short len;
};

struct EspNowDeps {
  RelayManager*   relay   = nullptr;
  SensorManager*  sensors = nullptr;
  BuzzerManager*  buzzer  = nullptr;
  RGBLed*         rgb     = nullptr;
  CoolingManager* cooling = nullptr;
  RTCManager*     rtc     = nullptr;
  LogFS*          log     = nullptr;
  NvsManager*     nvs     = nullptr;   // NVS access for PMK/LMK/SALT etc.
};

// ---- Security secrets wiring (PMK/LMK/SALT) ----
struct EspNowSecrets {
  bool    has_pmk = false;
  bool    has_lmk = false;
  uint8_t pmk[16]  = {0};   // fleet/global
  uint8_t lmk[16]  = {0};   // per-device
  uint8_t salt[16] = {0};   // deployment salt (optional)
};

// Provide secrets to HMAC module (call during begin()).
void security_set_secrets(const EspNowSecrets& s);

// Security hooks — declarations only
bool verifyHmac(const NowHeader& h, const NowAuth128& a, const NowSecTrailer& s,
                const unsigned char* payload, unsigned short payload_len);
void deriveKeys();

struct EspNowSettings {
  // Populated from NVS (KIND__, ICMMAC, CHAN__, TOKEN_, PAIRED, etc.).
  // Declarations only; fill fields as you implement.
  unsigned char  proto_ver;   // expected 3
  unsigned char  channel;
  unsigned char  sender_role;
  unsigned char  reserved0;
  unsigned char  icm_mac[6];
  unsigned char  device_token[16];
  unsigned short topo_ver;
};

struct EspNowCallbacks {
  virtual void onPing(const NowPing& req) = 0;
  virtual void onPingReply(const NowPingReply& r) = 0;
  virtual void onConfigWrite(const NowConfigWrite& hdr, ByteSpan val) = 0;
  virtual void onCtrlRelay(const NowCtrlRelay& r) = 0;
  virtual void onSensReport(const NowSensReport& r) = 0;
  virtual void onPmsStatus(const NowPmsStatus& r) = 0;
  virtual void onFwStatus(const NowFwStatus& r) = 0;
  virtual void onTopoPush(ByteSpan tlv) = 0;
  virtual ~EspNowCallbacks() {}
};

class EspNowRadio;     // transport
class EspNowSecurity;  // hmac/replay
class EspNowRouter;    // inbound routing
class EspNowScheduler; // tx pacing
class TopoStore;       // topo version/blob
class FwUpdate;        // firmware sm

class EspNowStack {
public:
  EspNowStack() = default;
  void begin(const EspNowDeps& deps, const EspNowSettings& settings);
  void tick();

  // Outbound helpers (build+send). Declarations only.
  void sendPing();
  void sendConfigWrite(const NowConfigWrite& hdr, ByteSpan value);
  void sendCtrlRelay(const NowCtrlRelay& ctrl);
  void sendTopoPush(ByteSpan tlv);
  void sendFwBegin(const NowFwBegin& fb);
  void sendFwChunk(const NowFwChunk& fc, ByteSpan data);
  void sendFwCommit(const NowFwCommit& cm, ByteSpan sig);
  void sendFwAbort(const NowFwAbort& ab);

  // Role adapter injection
  void setRoleAdapter(EspNowCallbacks* adapter);

private:
  // Opaque internals; define in .cpp files.
  EspNowCallbacks* role_{nullptr};
};

// Codec surface (builders/parsers) — declarations only
void buildHeader(NowHeader& h);
void buildAuth(NowAuth128& a);
void buildSecTrailer(NowSecTrailer& s);

// Topology helpers — declarations only
bool topoRequiresToken(unsigned char msg_type);
bool topoValidateToken(const NowTopoToken128& t);

// Firmware service — declarations only
void fwInit();
void fwHandleStatus(const NowFwStatus& st);

#endif // ESPNOW_STACK_H
