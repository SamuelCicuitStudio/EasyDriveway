// transport/espnow_radio.cpp
// Thin ESP-NOW wrapper: init, channel, encrypted peers, send, RX callback.

#include "EspNow/EspNowStack.h"   // forward decl + ByteSpan etc. :contentReference[oaicite:0]{index=0}
#include "EspNow/EspNowAPI.h"     // roles/opcodes are defined here (sizes, etc.). :contentReference[oaicite:1]{index=1}

#include <cstdint>
#include <cstddef>
#include <cstring> 

#if defined(ESP_PLATFORM) || (defined(ARDUINO) && defined(ESP32))
  // ESP32 (ESP-IDF or Arduino-ESP32)
  #if defined(ARDUINO)
    #include <WiFi.h>
    #include <esp_wifi.h>
  #endif
  #include <esp_now.h>
  #include <esp_wifi.h>
  #include <esp_system.h>
#else
  // Non-ESP32 build: provide no-op stubs so desktop tests link
  #warning "Building espnow_radio.cpp without ESP32 support — transport is stubbed."
#endif

namespace espnow {

using RxCallback = void(*)(const uint8_t* mac, const uint8_t* data, size_t len);

class EspNowRadio {
public:
  static EspNowRadio& instance() {
    static EspNowRadio s;
    return s;
  }

  bool init(uint8_t channel) {
#if defined(ESP_PLATFORM) || (defined(ARDUINO) && defined(ESP32))
    // WiFi init (STA, fixed channel, long range optional)
    if (!wifiInitSTA(channel)) return false;

    // Init ESP-NOW once
    if (esp_now_init() != ESP_OK) return false;

    // Register RX callback
    esp_now_register_recv_cb(&EspNowRadio::onRecvThunk);

    // PMK is optional if peers specify LMK+encrypt, but we set a deterministic PMK slot anyway.
    // NOTE: Replace this with your fleet PMK before production.
    uint8_t dummy_pmk[16] = {0}; // You will set a real PMK via config if desired.
    esp_now_set_pmk(dummy_pmk);

    channel_ = channel;
    return true;
#else
    (void)channel;
    return true;
#endif
  }

  bool setRxCallback(RxCallback cb) {
    rx_ = cb;
    return true;
  }

  bool addEncryptedPeer(const uint8_t mac[6], const uint8_t lmk[16], const uint8_t pmk[16]) {
#if defined(ESP_PLATFORM) || (defined(ARDUINO) && defined(ESP32))
    if (!mac) return false;

    // Optional: set global PMK (applies to all peers). If you manage PMK elsewhere, remove this.
    if (pmk) esp_now_set_pmk(const_cast<uint8_t*>(pmk));

    esp_now_peer_info_t p{};
    std::memcpy(p.peer_addr, mac, 6);
    p.channel = channel_;        // stay on our operating channel
    p.ifidx   = WIFI_IF_STA;     // ESPNOW uses station interface
    p.encrypt = true;
    if (lmk) std::memcpy(p.lmk, lmk, 16);

    // Remove first if it exists (safe no-op if not)
    esp_now_del_peer(mac);
    return esp_now_add_peer(&p) == ESP_OK;
#else
    (void)mac; (void)lmk; (void)pmk;
    return true;
#endif
  }

  bool removePeer(const uint8_t mac[6]) {
#if defined(ESP_PLATFORM) || (defined(ARDUINO) && defined(ESP32))
    if (!mac) return false;
    return esp_now_del_peer(mac) == ESP_OK;
#else
    (void)mac;
    return true;
#endif
  }

  bool send(const uint8_t mac[6], const uint8_t* data, size_t len) {
#if defined(ESP_PLATFORM) || (defined(ARDUINO) && defined(ESP32))
    if (!mac || !data || len == 0) return false;
    // esp_now_send is async; success here means queued to WiFi driver.
    return esp_now_send(mac, data, static_cast<uint8_t>(len)) == ESP_OK;
#else
    (void)mac; (void)data; (void)len;
    return true;
#endif
  }

  bool setChannel(uint8_t ch) {
#if defined(ESP_PLATFORM) || (defined(ARDUINO) && defined(ESP32))
    // Must stop ESPNOW or reconfigure WiFi before channel change on some SDK versions.
    // Easiest: set primary channel on WIFI, then keep peers on that channel.
    if (!wifiInitSTA(ch)) return false;
    channel_ = ch;
    return true;
#else
    channel_ = ch;
    return true;
#endif
  }

  uint8_t getChannel() const { return channel_; }

private:
  EspNowRadio() = default;

#if defined(ESP_PLATFORM) || (defined(ARDUINO) && defined(ESP32))
  static void onRecvThunk(const uint8_t* mac, const uint8_t* data, int len) {
    // Forward to user callback if set
    EspNowRadio& self = EspNowRadio::instance();
    if (self.rx_ && mac && data && len > 0) {
      self.rx_(mac, data, static_cast<size_t>(len));
    }
  }

  static bool wifiInitSTA(uint8_t channel) {
    // Ensure WiFi is in STA mode and set channel without connecting to AP.
    // Arduino or ESP-IDF paths.
    #if defined(ARDUINO)
      if (WiFi.getMode() != WIFI_STA) WiFi.mode(WIFI_STA);
      // Disconnect from any AP to allow fixed-channel operation.
      WiFi.disconnect(true, true);
      // esp_wifi_set_channel expects primary+second (HT40), we set primary only.
      return esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE) == ESP_OK;
    #else
      // ESP-IDF
      wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
      static bool wifi_inited = false;
      if (!wifi_inited) {
        if (esp_wifi_init(&cfg) != ESP_OK) return false;
        wifi_inited = true;
      }
      esp_wifi_set_storage(WIFI_STORAGE_RAM);
      esp_wifi_set_mode(WIFI_MODE_STA);
      if (esp_wifi_start() != ESP_OK) return false;
      return esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE) == ESP_OK;
    #endif
  }
#endif

private:
  RxCallback rx_{nullptr};
  uint8_t    channel_{1};
};

// ------------ minimal C-style facade so core can use it without extra headers ------------

static EspNowRadio& R() { return EspNowRadio::instance(); }

bool radio_init(uint8_t channel)                    { return R().init(channel); }
bool radio_set_rx(RxCallback cb)                    { return R().setRxCallback(cb); }
bool radio_add_encrypted_peer(const uint8_t m[6],
                              const uint8_t lmk[16],
                              const uint8_t pmk[16]){ return R().addEncryptedPeer(m,lmk,pmk); }
bool radio_remove_peer(const uint8_t m[6])          { return R().removePeer(m); }
bool radio_send(const uint8_t m[6], const uint8_t* data, size_t len)
                                                   { return R().send(m,data,len); }
bool radio_set_channel(uint8_t ch)                  { return R().setChannel(ch); }
uint8_t radio_get_channel()                         { return R().getChannel(); }

} // namespace espnow
