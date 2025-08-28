#include "RGBLed.h"

// ---------- Constructor ----------
RGBLed::RGBLed(ConfigManager* cfg, ICMLogFS* log)
: _cfg(cfg), _log(log) {}

// ---------- Begin ----------
bool RGBLed::begin() {
  if (!_cfg) return false;
  loadPinsFromConfig();
  setupOutputs();
  setupPwmIfAvailable();
  setColor(0, 0, 0); // OFF
  return true;
}

// ---------- Config loading ----------
void RGBLed::loadPinsFromConfig() {
  // Pull from NVS; default to 5/6/7 if not set
  _pinR = _cfg->GetInt(LED_R_PIN_KEY, LED_R_PIN_DEFAULT);
  _pinG = _cfg->GetInt(LED_G_PIN_KEY, LED_G_PIN_DEFAULT);
  _pinB = _cfg->GetInt(LED_B_PIN_KEY, LED_B_PIN_DEFAULT);

  // Optional future: add a persisted "LED_ACTIVE_LOW" boolean key if needed.
  // For now, preserve previous behavior (active-low).
  _activeLow = true;

  if (_log) {
    _log->eventf(ICMLogFS::DOM_SYSTEM, ICMLogFS::EV_INFO, 3001,
                 "RGB pins R=%d G=%d B=%d activeLow=%d",
                 _pinR, _pinG, _pinB, (int)_activeLow);
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

// ---------- Public patterns ----------
void RGBLed::startRainbow() {
  stop();
  _mode = MODE_RAINBOW;
  xTaskCreatePinnedToCore(&RGBLed::taskThunk, "RGBRainbow",
                          RGB_TASK_STACK, this,
                          RGB_TASK_PRIORITY, &_taskHandle, RGB_TASK_CORE);
}

void RGBLed::startBlink(uint32_t color, uint16_t delayMs) {
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
  setColor(0, 0, 0); // OFF
}

// ---------- Direct control ----------
void RGBLed::setColorHex(uint32_t color) {
  uint8_t r = (color >> 16) & 0xFF;
  uint8_t g = (color >> 8)  & 0xFF;
  uint8_t b =  color        & 0xFF;
  setColor(r, g, b);
}

void RGBLed::setColor(uint8_t r, uint8_t g, uint8_t b) {
  // Apply inversion at the last stage so callers always use "human" values.
  if (_activeLow) {
    r = 255 - r;
    g = 255 - g;
    b = 255 - b;
  }
  writeRGB(r, g, b);
}

void RGBLed::writeRGB(uint8_t r, uint8_t g, uint8_t b) {
#if defined(ESP32)
  // Map 0..255 to LEDC duty
  const uint32_t dutyR = r;
  const uint32_t dutyG = g;
  const uint32_t dutyB = b;
  ledcWrite(RGB_LEDC_CH_R, dutyR);
  ledcWrite(RGB_LEDC_CH_G, dutyG);
  ledcWrite(RGB_LEDC_CH_B, dutyB);
#else
  analogWrite(_pinR, r);
  analogWrite(_pinG, g);
  analogWrite(_pinB, b);
#endif
}

// ---------- Task plumbing ----------
void RGBLed::taskThunk(void* arg) {
  static_cast<RGBLed*>(arg)->taskLoop();
}

void RGBLed::taskLoop() {
  if (_mode == MODE_RAINBOW) {
    float hue = 0.f;
    while (true) {
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
      setColor(r, g, b);
      vTaskDelay(pdMS_TO_TICKS(_blinkDelay));
      setColor(0, 0, 0);
      vTaskDelay(pdMS_TO_TICKS(_blinkDelay));
    }
  }

  vTaskDelete(nullptr);
}

// ---------- Test helper ----------
void RGBLed::testPatterns() {
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
    if (_log) _log->event(ICMLogFS::DOM_SYSTEM, ICMLogFS::EV_INFO, 3002, "RGB self-test: rainbow 5s", "RGBLed");
    return;
  }

  // After 5s of rainbow, cycle blink colors every 2s
  if (_mode == MODE_RAINBOW) {
    if (millis() - _lastChangeMs > 5000) {
      stop();
      _lastChangeMs = millis();
      startBlink(kColors[_testIndex], 300);
      if (_log) _log->event(ICMLogFS::DOM_SYSTEM, ICMLogFS::EV_INFO, 3003, "RGB self-test: blink phase", "RGBLed");
    }
    return;
  }

  if (millis() - _lastChangeMs > 2000) {
    _testIndex = (_testIndex + 1) % (int)(sizeof(kColors)/sizeof(kColors[0]));
    startBlink(kColors[_testIndex], 300);
    _lastChangeMs = millis();
  }
}

// ---------- HSV helper ----------
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
