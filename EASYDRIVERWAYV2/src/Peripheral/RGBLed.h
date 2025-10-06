/**************************************************************
 *  Project     : EasyDriveway
 *  File        : RGBLed.h
 *  Purpose     : RTOS-aware RGB LED driver with NVS-controlled policy.
 *  Author      : Tshibangu Samuel
 *  Role        : Freelance Embedded Systems Engineer
 *  Expertise   : Secure IoT Systems, Embedded C++, RTOS, Control Logic
 *  Contact     : tshibsamuel47@gmail.com
 *  Phone       : +216 54 429 793
 *  Created     : 2025-10-05
 *  Version     : 1.0.0
 **************************************************************/

#ifndef RGBLED_H
#define RGBLED_H

#include <Arduino.h>
#include "NVS/NvsManager.h"
#include "Config/RGBConfig.h"
#include "Peripheral/LogFS.h"
#include "NVS/NVSConfig.h" 
class LogFS;

/**
 * @class RGBLed
 * @brief RGB LED driver (3 channels) supporting patterns and direct color writes.
 * @details
 * - Policy is loaded from NVS: enable = (RGBFBK && !LEDDIS), polarity = RGBALW.
 * - Uses ESP32 LEDC PWM when available; falls back to analogWrite otherwise.
 * - All public control APIs are no-ops when disabled; LED is held OFF at correct idle level.
 * @note Create once, call begin() after NVS is ready. Pattern APIs run in a FreeRTOS task.
 */
class RGBLed {
public:
  /**
   * @brief Run-time operating mode of the LED driver.
   */
  enum Mode { MODE_NONE, MODE_RAINBOW, MODE_BLINK };

  /**
   * @brief Construct the RGB driver and bind NVS + optional logger.
   * @param cfg Pointer to NvsManager used to read/write indicator policy.
   * @param log Optional LogFS for diagnostic events.
   */
  explicit RGBLed(NvsManager* cfg, LogFS* log = nullptr);

  /**
   * @brief Initialize pins/PWM and load NVS policy (enable + polarity).
   * @return true on success, false if NVS manager is missing.
   * @post LED is driven to OFF at the correct idle level.
   */
  bool begin();

  /**
   * @brief Attach a logger after construction.
   * @param logger Pointer to LogFS instance.
   * @note Safe to call anytime; no effect if nullptr.
   */
  void attachLog(LogFS* logger) { _log = logger; }

  /**
   * @brief Start a smooth HSV rainbow animation (non-blocking).
   * @note No-op if feedback is disabled. Spawns a FreeRTOS task; call stop() to end.
   */
  void startRainbow();

  /**
   * @brief Start a blink pattern at a fixed color (non-blocking).
   * @param color 24-bit 0xRRGGBB color.
   * @param delayMs ON/OFF durations in milliseconds (uses same value for both).
   * @note No-op if feedback is disabled. Spawns a FreeRTOS task; call stop() to end.
   */
  void startBlink(uint32_t color, uint16_t delayMs);

  /**
   * @brief Stop any running pattern task and force the LED OFF.
   * @post Mode becomes MODE_NONE; outputs held at idle level.
   */
  void stop();

  /**
   * @brief Set the LED to an explicit RGB color (8-bit per channel).
   * @param r Red 0..255
   * @param g Green 0..255
   * @param b Blue 0..255
   * @note No-op if feedback is disabled; inversion is applied automatically if active-low.
   */
  void setColor(uint8_t r, uint8_t g, uint8_t b);

  /**
   * @brief Set the LED color from a 24-bit hex value.
   * @param color 0xRRGGBB
   * @note No-op if feedback is disabled; inversion handled internally.
   */
  void setColorHex(uint32_t color);

  /**
   * @brief Short self-test helper (rainbow then color blinks).
   * @note Respects enable policy; useful for bring-up diagnostics.
   */
  void testPatterns();

  /**
   * @brief Get the configured Red pin number.
   * @return GPIO for Red channel.
   */
  int  pinR() const { return _pinR; }

  /**
   * @brief Get the configured Green pin number.
   * @return GPIO for Green channel.
   */
  int  pinG() const { return _pinG; }

  /**
   * @brief Get the configured Blue pin number.
   * @return GPIO for Blue channel.
   */
  int  pinB() const { return _pinB; }

  /**
   * @brief Current operating mode.
   * @return MODE_NONE, MODE_RAINBOW, or MODE_BLINK.
   */
  Mode mode() const { return _mode; }

  /**
   * @brief Query whether LED feedback is enabled.
   * @return true if (RGBFBK && !LEDDIS), else false.
   */
  bool enabled() const { return _enabled; }

  /**
   * @brief Query current electrical polarity.
   * @return true if active-low (RGBALW), false if active-high.
   */
  bool activeLow() const { return _activeLow; }

  /**
   * @brief Enable/disable LED feedback and persist to NVS.
   * @param en true to enable, false to disable.
   * @param persist If true, writes RGBFBK and mirrors legacy LEDDIS.
   * @post When disabled, any running pattern is stopped and output forced OFF.
   */
  void setEnabled(bool en, bool persist = true);

  /**
   * @brief Set active-low polarity and persist to NVS.
   * @param alw true = active-low, false = active-high.
   * @param persist If true, writes RGBALW to NVS.
   * @post Re-applies OFF with the new polarity to avoid ghost output.
   */
  void setActiveLow(bool alw, bool persist = true);

private:
  /**
   * @brief Load pins and indicator policy from NVS (RGBALW, RGBFBK, LEDDIS).
   * @note Called by begin(); can be reused if you need to refresh policy.
   */
  void loadConfig();

  /**
   * @brief Configure GPIO directions for RGB pins.
   * @note Safe to call multiple times; used during (re)initialization.
   */
  void setupOutputs();

  /**
   * @brief Initialize ESP32 LEDC PWM channels if available.
   * @note Falls back to analogWrite on non-ESP32 targets.
   */
  void setupPwmIfAvailable();

  /**
   * @brief Final hardware write to the three channels (guarded by enable).
   * @param r Duty for Red (already polarity-adjusted).
   * @param g Duty for Green (already polarity-adjusted).
   * @param b Duty for Blue (already polarity-adjusted).
   * @note Internal low-level funnel; prefer setColor()/setColorHex() externally.
   */
  void writeRGB(uint8_t r, uint8_t g, uint8_t b);

  /**
   * @brief FreeRTOS task entry thunk.
   * @param arg Opaque pointer to RGBLed instance.
   * @internal Do not call directly; spawned by pattern starters.
   */
  static void taskThunk(void* arg);

  /**
   * @brief FreeRTOS pattern loop (rainbow/blink) until stopped or disabled.
   * @internal Implements MODE_RAINBOW / MODE_BLINK state machine.
   */
  void        taskLoop();

  /**
   * @brief Convert HSV hue (0..360) to RGB bytes (full saturation/value).
   * @param h Hue degrees.
   * @param r Out red 0..255
   * @param g Out green 0..255
   * @param b Out blue 0..255
   * @note Used by the rainbow pattern generator.
   */
  static void hsvToRgb(float h, uint8_t& r, uint8_t& g, uint8_t& b);

  /**
   * @brief Force outputs to the OFF state at the correct idle level.
   * @note Bypasses enable checks to guarantee a dark LED.
   */
  void        driveOff();

private:
  NvsManager* _cfg;          ///< Bound NVS manager (not owned)
  LogFS*      _log;          ///< Optional logger (not owned)

  // Pins (from config)
  int _pinR = RGB_R_PIN;     ///< Red pin (default from RGBConfig.h)
  int _pinG = RGB_G_PIN;     ///< Green pin (default from RGBConfig.h)
  int _pinB = RGB_B_PIN;     ///< Blue pin (default from RGBConfig.h)

  // Policy (from NVS)
  bool _activeLow = true;    ///< Electrical polarity (RGBALW)
  bool _enabled   = true;    ///< Effective enable (RGBFBK && !LEDDIS)

  // State
  TaskHandle_t _taskHandle = nullptr; ///< Pattern task handle
  Mode         _mode       = MODE_NONE; ///< Current mode
  uint32_t     _blinkColor = RGB_WHITE; ///< Blink color (0xRRGGBB)
  uint16_t     _blinkDelay = 500;       ///< Blink on/off delay (ms)

  // testPatterns() state
  unsigned long _lastChangeMs = 0; ///< Internal timer for test sequencing
  int           _testIndex    = 0; ///< Current color index in test mode
  bool          _testing      = false; ///< Test mode flag
};

#endif // RGBLED_H
