#ifndef RGBLED_H
#define RGBLED_H

#include <Arduino.h>
#include "Config.h"
#include "ConfigManager.h"
#include "ICMLogFS.h"
#include "ICMLogFS.h"
#include"Config/RGBConfig.h"
// Forward-declare to avoid circular include
class ICMLogFS;

class RGBLed {
public:
  enum Mode { MODE_NONE, MODE_RAINBOW, MODE_BLINK };

  // Fully config-driven. Pass ConfigManager (required) and optional logger.
  explicit RGBLed(ConfigManager* cfg, ICMLogFS* log = nullptr);

  // Initialize: fetch pins from NVS (or defaults), init PWM, set OFF.
  bool begin();

  // Patterns
  void startRainbow();
  void startBlink(uint32_t color, uint16_t delayMs);
  void stop();

  // Direct control
  void setColor(uint8_t r, uint8_t g, uint8_t b);
  void setColorHex(uint32_t color);

  // Quick test helper (call repeatedly from loop if desired)
  void testPatterns();

  // Accessors
  int  pinR() const { return _pinR; }
  int  pinG() const { return _pinG; }
  int  pinB() const { return _pinB; }
  Mode mode() const { return _mode; }

private:
  // Config / setup
  void loadPinsFromConfig();
  void setupOutputs();
  void setupPwmIfAvailable();
  void writeRGB(uint8_t r, uint8_t g, uint8_t b);

  // Task plumbing
  static void taskThunk(void* arg);
  void        taskLoop();

  // Utils
  static void hsvToRgb(float h, uint8_t& r, uint8_t& g, uint8_t& b);

private:
  ConfigManager* _cfg;
  ICMLogFS*      _log;

  // Pins (from NVS or defaults)
  int _pinR = LED_R_PIN_DEFAULT;
  int _pinG = LED_G_PIN_DEFAULT;
  int _pinB = LED_B_PIN_DEFAULT;

  // If your hardware is low-side drivers on a common-anode LED,
  // logic is inverted (0 = ON). Keep true to match previous behavior.
  bool _activeLow = true;

  // State
  TaskHandle_t _taskHandle = nullptr;
  Mode         _mode       = MODE_NONE;
  uint32_t     _blinkColor = RGB_WHITE;
  uint16_t     _blinkDelay = 500;

  // testPatterns() state
  unsigned long _lastChangeMs = 0;
  int           _testIndex    = 0;
  bool          _testing      = false;
};

#endif // RGBLED_H
