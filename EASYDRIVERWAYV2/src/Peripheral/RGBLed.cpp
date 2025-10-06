/**************************************************************
 *  Project : EasyDriveway
 *  File    : RGBLed.cpp
 **************************************************************/
#include "RGBLed.h"
RGBLed::RGBLed(NvsManager* cfg, LogFS* log)
: _cfg(cfg), _log(log) {}
bool RGBLed::begin() {
  if (!_cfg) return false;
  loadConfig();
  setupOutputs();
  setupPwmIfAvailable();
  driveOff(); // OFF respecting polarity & enable
  return true;
}
void RGBLed::loadConfig() {
  // Pins from compile-time config (override here if you persist pins later)
  _pinR = RGB_R_PIN;
  _pinG = RGB_G_PIN;
  _pinB = RGB_B_PIN;

  // Polarity & feedback from NVS
  // active-low (RGBALW), feedback-on (RGBFBK), legacy disable (LEDDIS)
  const bool nvsActiveLow = _cfg->GetBool(NVS_KEY_RGBALW, NVS_DEF_RGBALW);
  const bool nvsFbkOn     = _cfg->GetBool(NVS_KEY_RGBFBK, NVS_DEF_RGBFBK);
  const bool legacyDis    = _cfg->GetBool(NVS_KEY_LEDDIS, NVS_DEF_LEDDIS);

  _activeLow = nvsActiveLow;
  _enabled   = (nvsFbkOn && !legacyDis);

  if (_log) {
    _log->eventf(LogFS::DOM_SYSTEM, LogFS::EV_INFO, 3101,
      "RGB cfg R=%d G=%d B=%d activeLow=%d enabled=%d (RGBFBK=%d LEDDIS=%d)",
      _pinR, _pinG, _pinB, (int)_activeLow, (int)_enabled, (int)nvsFbkOn, (int)legacyDis);
  }
}
void RGBLed::setupOutputs() {
  pinMode(_pinR, OUTPUT);
  pinMode(_pinG, OUTPUT);
  pinMode(_pinB, OUTPUT);
}
void RGBLed::setupPwmIfAvailable() {
#if defined(ESP32)
  ledcSetup(RGB_LEDC_CH_R, RGB_LEDC_FREQ_HZ, RGB_LEDC_RES_BITS);
  ledcSetup(RGB_LEDC_CH_G, RGB_LEDC_FREQ_HZ, RGB_LEDC_RES_BITS);
  ledcSetup(RGB_LEDC_CH_B, RGB_LEDC_FREQ_HZ, RGB_LEDC_RES_BITS);
  ledcAttachPin(_pinR, RGB_LEDC_CH_R);
  ledcAttachPin(_pinG, RGB_LEDC_CH_G);
  ledcAttachPin(_pinB, RGB_LEDC_CH_B);
#endif
}
void RGBLed::setEnabled(bool en, bool persist) {
  _enabled = en;
  if (persist && _cfg) {
    _cfg->PutBool(NVS_KEY_RGBFBK, en);      // primary flag
    _cfg->PutBool(NVS_KEY_LEDDIS, !en);     // keep legacy in sync
  }
  if (!en) { stop(); driveOff(); }
}
void RGBLed::setActiveLow(bool alw, bool persist) {
  _activeLow = alw;
  if (persist && _cfg) _cfg->PutBool(NVS_KEY_RGBALW, alw);
  // Re-apply OFF with new polarity to avoid ghost light
  driveOff();
}
void RGBLed::startRainbow() {
  if (!_enabled) { driveOff(); return; }
  stop();
  _mode = MODE_RAINBOW;
  xTaskCreatePinnedToCore(&RGBLed::taskThunk, "RGBRainbow",
                          RGB_TASK_STACK, this,
                          RGB_TASK_PRIORITY, &_taskHandle, RGB_TASK_CORE);
}
void RGBLed::startBlink(uint32_t color, uint16_t delayMs) {
  if (!_enabled) { driveOff(); return; }
  stop();
  _mode = MODE_BLINK;
  _blinkColor = color;
  _blinkDelay = delayMs ? delayMs : 300;
  xTaskCreatePinnedToCore(&RGBLed::taskThunk, "RGBBlink",
                          RGB_TASK_STACK, this,
                          RGB_TASK_PRIORITY, &_taskHandle, RGB_TASK_CORE);
}
void RGBLed::stop() {
  if (_taskHandle) {
    vTaskDelete(_taskHandle);
    _taskHandle = nullptr;
  }
  _mode = MODE_NONE;
  driveOff();
}
void RGBLed::setColorHex(uint32_t color) {
  if (!_enabled) { driveOff(); return; }
  uint8_t r = (color >> 16) & 0xFF;
  uint8_t g = (color >> 8)  & 0xFF;
  uint8_t b =  color        & 0xFF;
  setColor(r, g, b);
}
void RGBLed::setColor(uint8_t r, uint8_t g, uint8_t b) {
  if (!_enabled) { driveOff(); return; }

  // Apply inversion at the last stage so callers always use "human" values.
  if (_activeLow) {
    r = 255 - r;
    g = 255 - g;
    b = 255 - b;
  }
  writeRGB(r, g, b);
}
void RGBLed::writeRGB(uint8_t r, uint8_t g, uint8_t b) {
  if (!_enabled) {
    // Force OFF regardless of inputs
#if defined(ESP32)
    ledcWrite(RGB_LEDC_CH_R, _activeLow ? 255 : 0);
    ledcWrite(RGB_LEDC_CH_G, _activeLow ? 255 : 0);
    ledcWrite(RGB_LEDC_CH_B, _activeLow ? 255 : 0);
#else
    analogWrite(_pinR, _activeLow ? 255 : 0);
    analogWrite(_pinG, _activeLow ? 255 : 0);
    analogWrite(_pinB, _activeLow ? 255 : 0);
#endif
    return;
  }

#if defined(ESP32)
  // Map 0..255 to LEDC duty (LEDC resolution already set to 8b in config)
  ledcWrite(RGB_LEDC_CH_R, r);
  ledcWrite(RGB_LEDC_CH_G, g);
  ledcWrite(RGB_LEDC_CH_B, b);
#else
  analogWrite(_pinR, r);
  analogWrite(_pinG, g);
  analogWrite(_pinB, b);
#endif
}
void RGBLed::driveOff() {
  // Human OFF = (r,g,b) = 0. setColor() handles polarity & enabled guard.
  // But we need to force OFF even when disabled: call writeRGB directly.
  uint8_t r = 0, g = 0, b = 0;
  if (_activeLow) { r = 255; g = 255; b = 255; }
  writeRGB(r, g, b);
}
void RGBLed::taskThunk(void* arg) {
  static_cast<RGBLed*>(arg)->taskLoop();
}
void RGBLed::taskLoop() {
  if (_mode == MODE_RAINBOW) {
    float hue = 0.f;
    while (true) {
      if (!_enabled) { driveOff(); break; }
      uint8_t r, g, b;
      hsvToRgb(hue, r, g, b);
      setColor(r, g, b);
      hue += 1.f;
      if (hue >= 360.f) hue = 0.f;
      vTaskDelay(pdMS_TO_TICKS(20));
    }
  } else if (_mode == MODE_BLINK) {
    uint8_t r = (_blinkColor >> 16) & 0xFF;
    uint8_t g = (_blinkColor >> 8)  & 0xFF;
    uint8_t b =  _blinkColor        & 0xFF;

    while (true) {
      if (!_enabled) { driveOff(); break; }
      setColor(r, g, b);
      vTaskDelay(pdMS_TO_TICKS(_blinkDelay));
      setColor(0, 0, 0);
      vTaskDelay(pdMS_TO_TICKS(_blinkDelay));
    }
  }

  _taskHandle = nullptr;
  vTaskDelete(nullptr);
}
void RGBLed::testPatterns() {
  if (!_enabled) { driveOff(); return; }

  static const uint32_t kColors[] = {
    RGB_RED, RGB_GREEN, RGB_BLUE, RGB_YELLOW, RGB_CYAN,
    RGB_MAGENTA, RGB_ORANGE, RGB_PURPLE, RGB_PINK,
    RGB_WHITE, RGB_GRAY, RGB_BROWN
  };

  if (!_testing) {
    _testing = true;
    _testIndex = 0;
    _lastChangeMs = millis();
    startRainbow();
    if (_log) _log->event(LogFS::DOM_SYSTEM, LogFS::EV_INFO, 3002, "RGB self-test: rainbow 5s", "RGBLed");
    return;
  }

  if (_mode == MODE_RAINBOW) {
    if (millis() - _lastChangeMs > 5000) {
      stop();
      _lastChangeMs = millis();
      startBlink(kColors[_testIndex], 300);
      if (_log) _log->event(LogFS::DOM_SYSTEM, LogFS::EV_INFO, 3003, "RGB self-test: blink phase", "RGBLed");
    }
    return;
  }

  if (millis() - _lastChangeMs > 2000) {
    _testIndex = (_testIndex + 1) % (int)(sizeof(kColors)/sizeof(kColors[0]));
    startBlink(kColors[_testIndex], 300);
    _lastChangeMs = millis();
  }
}
void RGBLed::hsvToRgb(float h, uint8_t& r, uint8_t& g, uint8_t& b) {
  float s = 1.0f, v = 1.0f;
  int i = int(h / 60.0f) % 6;
  float f = (h / 60.0f) - i;
  float p = v * (1 - s);
  float q = v * (1 - f * s);
  float t = v * (1 - (1 - f) * s);
  float rf, gf, bf;

  switch (i) {
    case 0: rf = v; gf = t; bf = p; break;
    case 1: rf = q; gf = v; bf = p; break;
    case 2: rf = p; gf = v; bf = t; break;
    case 3: rf = p; gf = q; bf = v; break;
    case 4: rf = t; gf = p; bf = v; break;
    default: rf = v; gf = p; bf = q; break;
  }

  r = (uint8_t)(rf * 255);
  g = (uint8_t)(gf * 255);
  b = (uint8_t)(bf * 255);
}


