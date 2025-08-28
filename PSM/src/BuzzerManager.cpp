#include "BuzzerManager.h"

bool BuzzerManager::begin() {
  if (!_cfg) return false;

  // fetch from NVS
  _pin        = _cfg->GetInt (BUZZER_PIN_KEY,         BUZZER_PIN_DEFAULT);
  _activeHigh = _cfg->GetBool(BUZZER_ACTIVE_HIGH_KEY, BUZZER_ACTIVE_HIGH_DEFAULT);
  _enabled    = _cfg->GetBool(BUZZER_FEEDBACK_KEY,    BUZZER_FEEDBACK_DEFAULT);

  pinMode(_pin, OUTPUT);
  applyIdle();               // silence

  return true;
}

void BuzzerManager::setEnabled(bool en) {
  _enabled = en;
  if (_cfg) _cfg->PutBool(BUZZER_FEEDBACK_KEY, en);
  if (!en) stop();
}

void BuzzerManager::applyIdle() {
  digitalWrite(_pin, _activeHigh ? LOW : HIGH);
}

void BuzzerManager::toneOn(uint16_t freq) {
  // Use Arduino tone() if available on your core
  tone(_pin, freq);
  // For active-buzzer (no frequency control), fallback to DC on/off
  // digitalWrite(_pin, _activeHigh ? HIGH : LOW);
}

void BuzzerManager::toneOff() {
  noTone(_pin);
  applyIdle();
}

void BuzzerManager::bip(uint16_t freq, uint16_t ms) {
  if (!_enabled) return;
  toneOn(freq);
  vTaskDelay(pdMS_TO_TICKS(ms));
  toneOff();
}

void BuzzerManager::stop() {
  if (_task) {
    vTaskDelete(_task);
    _task = nullptr;
  }
  toneOff();
}

void BuzzerManager::taskThunk(void* arg) {
  // Copy pattern from heap, then free immediately
  auto* pkg = static_cast<std::pair<BuzzerManager*, std::vector<Step>>*>(arg);
  BuzzerManager* self = pkg->first;
  std::vector<Step> steps = std::move(pkg->second);
  delete pkg;

  for (const auto& s : steps) {
    if (!self->_enabled) break;
    if (s.freq > 0 && s.durMs > 0) {
      self->toneOn(s.freq);
      vTaskDelay(pdMS_TO_TICKS(s.durMs));
      self->toneOff();
    }
    if (s.pauseMs) vTaskDelay(pdMS_TO_TICKS(s.pauseMs));
  }

  self->_task = nullptr;
  vTaskDelete(nullptr);
}

void BuzzerManager::runPattern(const Step* steps, size_t count) {
  if (!_enabled) return;

  // Stop any previous pattern
  if (_task) { vTaskDelete(_task); _task = nullptr; }

  // Package steps for the task
  auto* pkg = new std::pair<BuzzerManager*, std::vector<Step>>(
      this, std::vector<Step>(steps, steps + count));
  xTaskCreate(taskThunk, "BuzzSeq", 2048, pkg, 1, &_task);
}

void BuzzerManager::play(Event ev) {
  if (!_enabled) return;

  // Define all your patterns here
  // (freq Hz, duration ms, pause ms)
  switch (ev) {
    case EV_STARTUP: {
      static const Step k[] = {
        { 600, 80, 40 }, { 1000, 80, 40 }, { 1400, 80, 0 }
      };
      runPattern(k, sizeof(k)/sizeof(k[0])); break;
    }
    case EV_READY: {
      static const Step k[] = {
        { 2000, 50, 40 }, { 2500, 50, 0 }
      };
      runPattern(k, sizeof(k)/sizeof(k[0])); break;
    }
    case EV_WIFI_CONNECTED: {
      static const Step k[] = {
        { 1200, 100, 40 }, { 1500, 100, 0 }
      };
      runPattern(k, sizeof(k)/sizeof(k[0])); break;
    }
    case EV_WIFI_OFF: {
      static const Step k[] = {
        { 800, 150, 0 }
      };
      runPattern(k, sizeof(k)/sizeof(k[0])); break;
    }
    case EV_BLE_PAIRING: {
      static const Step k[] = {
        { 1000, 40, 40 }, { 1000, 40, 120 }, { 1000, 40, 40 }, { 1000, 40, 0 }
      };
      runPattern(k, sizeof(k)/sizeof(k[0])); break;
    }
    case EV_BLE_PAIRED: {
      static const Step k[] = {
        { 1300, 120, 0 }
      };
      runPattern(k, sizeof(k)/sizeof(k[0])); break;
    }
    case EV_BLE_UNPAIRED: {
      static const Step k[] = {
        { 1200, 80, 40 }, { 900, 60, 0 }
      };
      runPattern(k, sizeof(k)/sizeof(k[0])); break;
    }
    case EV_CLIENT_CONNECTED: {
      static const Step k[] = {
        { 1100, 50, 30 }, { 1300, 60, 0 }
      };
      runPattern(k, sizeof(k)/sizeof(k[0])); break;
    }
    case EV_CLIENT_DISCONNECTED: {
      static const Step k[] = {
        { 1200, 80, 40 }, { 900, 60, 0 }
      };
      runPattern(k, sizeof(k)/sizeof(k[0])); break;
    }
    case EV_POWER_GONE: {      // AC lost
      static const Step k[] = {
        { 400, 200, 100 }, { 400, 200, 100 }, { 400, 200, 0 }
      };
      runPattern(k, sizeof(k)/sizeof(k[0])); break;
    }
    case EV_ON_BATTERY: {
      static const Step k[] = {
        { 900, 60, 40 }, { 1000, 60, 40 }, { 1100, 60, 0 }
      };
      runPattern(k, sizeof(k)/sizeof(k[0])); break;
    }
    case EV_LOW_POWER: {
      static const Step k[] = {
        { 500, 120, 120 }, { 500, 120, 0 }
      };
      runPattern(k, sizeof(k)/sizeof(k[0])); break;
    }
    case EV_OVER_TEMP: {
      static const Step k[] = {
        { 2000, 40, 60 }, { 2000, 40, 60 }, { 2000, 40, 60 }, { 2000, 40, 0 }
      };
      runPattern(k, sizeof(k)/sizeof(k[0])); break;
    }
    case EV_FAULT: {
      static const Step k[] = {
        { 300, 80, 40 }, { 300, 80, 40 }, { 300, 80, 40 }, { 300, 80, 40 }, { 300, 80, 0 }
      };
      runPattern(k, sizeof(k)/sizeof(k[0])); break;
    }
    case EV_SHUTDOWN: {
      static const Step k[] = {
        { 1500, 80, 50 }, { 1000, 80, 50 }, { 600,  80, 0 }
      };
      runPattern(k, sizeof(k)/sizeof(k[0])); break;
    }
    case EV_SUCCESS: {
      static const Step k[] = {
        { 1000, 40, 30 }, { 1300, 40, 30 }, { 1600, 60, 0 }
      };
      runPattern(k, sizeof(k)/sizeof(k[0])); break;
    }
    case EV_FAILED: {
      static const Step k[] = {
        { 500, 50, 50 }, { 500, 50, 0 }
      };
      runPattern(k, sizeof(k)/sizeof(k[0])); break;
    }
  }
}
