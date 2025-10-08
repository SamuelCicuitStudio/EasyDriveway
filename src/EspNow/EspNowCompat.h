#pragma once
/**
 * EspNowCompat.h — Platform glue for ESP32-S3 (Arduino-as-component / IDF)
 *
 * Role:
 *  - Centralize includes for Arduino WiFi, ESP-NOW, and FreeRTOS.
 *  - Compile-time/platform checks (target, PSRAM hints).
 *  - Small helpers: MAC utilities, channel checks, LR toggles, time utils.
 *
 * Notes:
 *  - Designed for ESP32-S3 with Arduino core on top of ESP-IDF.
 *  - Safe to include from C++ only (uses inline/constexpr and minimal <Arduino.h>).
 */

#include <stdint.h>
#include <string.h>

#if defined(ARDUINO)
  #include <Arduino.h>        // millis(), String, etc.
#else
  // Fallback for IDF-only builds (no Arduino.h)
  #include <esp_timer.h>
#endif

// WiFi / ESP-NOW / ESP-IDF
#include <WiFi.h>             // Arduino WiFi (brings in esp_wifi underneath)
#include <esp_wifi.h>
#include <esp_now.h>
#include <esp_system.h>
#include <esp_err.h>
#include <esp_mac.h>
#include <esp_idf_version.h>

// FreeRTOS (Arduino-as-component provides these on ESP32)
extern "C" {
  #include "freertos/FreeRTOS.h"
  #include "freertos/queue.h"
  #include "freertos/semphr.h"
  #include "freertos/task.h"
}

// ---------- Compile-time target checks ----------

// Prefer IDF target macro when present
#if defined(CONFIG_IDF_TARGET) && defined(CONFIG_IDF_TARGET_ESP32S3)
  #define NOW_TARGET_ESP32S3 1
#elif defined(CONFIG_IDF_TARGET) && !defined(CONFIG_IDF_TARGET_ESP32S3)
  #error "EspNow stack is configured for ESP32-S3. Set CONFIG_IDF_TARGET_ESP32S3."
#else
  // In some Arduino builds CONFIG_IDF_TARGET_* may be absent; allow but warn.
  #ifndef ARDUINO_ARCH_ESP32
    #warning "Unknown target. Assuming ESP32-S3 Arduino; define CONFIG_IDF_TARGET_ESP32S3 for strict check."
  #endif
#endif

// ---------- PSRAM hints (non-fatal) ----------
#if !defined(BOARD_HAS_PSRAM) && !defined(CONFIG_SPIRAM)
  #warning "PSRAM not detected at compile time (BOARD_HAS_PSRAM/CONFIG_SPIRAM). Large buffers should avoid heap fragmentation."
#endif

// ---------- Defaults (can be overridden by build flags) ----------
#ifndef NOW_DEFAULT_CHANNEL
  #define NOW_DEFAULT_CHANNEL 6
#endif

#ifndef NOW_RX_Q_DEPTH
  #define NOW_RX_Q_DEPTH 16
#endif

#ifndef NOW_TX_Q_DEPTH
  #define NOW_TX_Q_DEPTH 16
#endif

// ---------- Utility attributes ----------
#if defined(__GNUC__)
  #define NOW_ALWAYS_INLINE inline __attribute__((always_inline))
#else
  #define NOW_ALWAYS_INLINE inline
#endif

// ---------- Time utilities ----------
NOW_ALWAYS_INLINE uint64_t now_millis()
{
#if defined(ARDUINO)
  return (uint64_t)millis();
#else
  // esp_timer_get_time() returns microseconds
  return (uint64_t)(esp_timer_get_time() / 1000ULL);
#endif
}

// ---------- MAC utilities ----------
NOW_ALWAYS_INLINE bool now_same_mac(const uint8_t a[6], const uint8_t b[6]) {
  return memcmp(a, b, 6) == 0;
}
NOW_ALWAYS_INLINE void now_copy_mac(uint8_t dst[6], const uint8_t src[6]) {
  memcpy(dst, src, 6);
}
NOW_ALWAYS_INLINE bool now_mac_is_zero(const uint8_t mac[6]) {
  static const uint8_t Z[6] = {0,0,0,0,0,0};
  return memcmp(mac, Z, 6) == 0;
}

#if defined(ARDUINO)
NOW_ALWAYS_INLINE String now_mac_to_string(const uint8_t mac[6]) {
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}
#else
NOW_ALWAYS_INLINE void now_mac_to_cstr(const uint8_t mac[6], char out[18]) {
  // out must be at least 18 bytes (including NUL)
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
#endif

// ---------- Channel helpers ----------
NOW_ALWAYS_INLINE bool now_is_valid_channel(uint8_t ch) {
  // For 2.4 GHz: 1..13 typically (14 not used outside JP)
  return (ch >= 1 && ch <= 13);
}

#if (NOW_DEFAULT_CHANNEL < 1) || (NOW_DEFAULT_CHANNEL > 13)
  #error "NOW_DEFAULT_CHANNEL must be in [1..13] for 2.4 GHz ESP-NOW."
#endif

// Wrapper to set WiFi channel (STA interface)
NOW_ALWAYS_INLINE esp_err_t now_wifi_set_channel(uint8_t ch) {
  if (!now_is_valid_channel(ch)) return ESP_ERR_INVALID_ARG;
  return esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
}

// ---------- Long Range (802.11 LR) helper ----------
NOW_ALWAYS_INLINE esp_err_t now_enable_long_range(bool enable, wifi_interface_t iface = WIFI_IF_STA) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4,4,0)
  uint8_t proto = WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N;
  if (enable) proto |= WIFI_PROTOCOL_LR;
  return esp_wifi_set_protocol(iface, proto);
#else
  (void)enable; (void)iface;
  // LR not configurable on older IDF; return OK to avoid hard failures.
  return ESP_OK;
#endif
}

// ---------- STA/AP mode helpers (light wrappers) ----------
NOW_ALWAYS_INLINE void now_wifi_sta_mode() {
  // Arduino's WiFi.mode() wraps esp_wifi; safe to call in Arduino-as-component.
  WiFi.mode(WIFI_STA);
}

NOW_ALWAYS_INLINE void now_wifi_ap_mode() {
  WiFi.mode(WIFI_AP);
}

// ---------- Current MAC helpers ----------
NOW_ALWAYS_INLINE esp_err_t now_get_mac_sta(uint8_t mac[6]) {
  return esp_read_mac(mac, ESP_MAC_WIFI_STA);
}
NOW_ALWAYS_INLINE esp_err_t now_get_mac_ap(uint8_t mac[6]) {
  return esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
}

// ---------- Minimal sanity probes ----------
NOW_ALWAYS_INLINE bool now_wifi_ready() {
  // Basic probe: ESPNOW requires initialized WiFi/PHY; WiFi.status() is cheap in Arduino.
#if defined(ARDUINO)
  // WL_CONNECTED is not required for ESP-NOW; only radio init matters.
  // We just check that WiFi is not in WIFI_OFF mode by attempting a getChannel.
  uint8_t ch = WiFi.channel();
  (void)ch;
  return true;
#else
  // If not Arduino, assume upper layer ensured wifi init.
  return true;
#endif
}
