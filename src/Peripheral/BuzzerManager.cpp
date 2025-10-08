/**************************************************************
 *  Project : EasyDriveway
 *  File    : BuzzerManager.cpp
 **************************************************************/

#include "BuzzerManager.h"
bool BuzzerManager::begin() {
  if (!_cfg) return false;
  setupPin();
  loadPolicy();
  pinMode(_pin, OUTPUT);
  idleLevel();
  return true;
}
void BuzzerManager::setupPin() {
  _pin = BUZZER_PIN;
}
void BuzzerManager::loadPolicy() {
  const bool ah     = _cfg->GetBool(NVS_KEY_BUZAHI, NVS_DEF_BUZAHI);
  const bool fbk    = _cfg->GetBool(NVS_KEY_BUZFBK, NVS_DEF_BUZFBK);
  const bool legacy = _cfg->GetBool(NVS_KEY_BUZDIS, NVS_DEF_BUZDIS);
  _activeHigh = ah;
  _enabled    = (fbk && !legacy);
}
void BuzzerManager::idleLevel() {
  digitalWrite(_pin, _activeHigh ? LOW : HIGH);
}
void BuzzerManager::toneOn(uint16_t freq) {
  if (!_enabled) { idleLevel(); return; }
  tone(_pin, freq);
}
void BuzzerManager::toneOff() {
  noTone(_pin);
  idleLevel();
}
void BuzzerManager::bip(uint16_t freq, uint16_t ms) {
  if (!_enabled) return;
  toneOn(freq);
  vTaskDelay(pdMS_TO_TICKS(ms));
  toneOff();
}
void BuzzerManager::setEnabled(bool en, bool persist) {
  _enabled = en;
  if (persist && _cfg) {
    _cfg->PutBool(NVS_KEY_BUZFBK, en);
    _cfg->PutBool(NVS_KEY_BUZDIS, !en);
  }
  if (!en) { stop(); } else { idleLevel(); }
}
void BuzzerManager::setActiveHigh(bool ah, bool persist) {
  _activeHigh = ah;
  if (persist && _cfg) _cfg->PutBool(NVS_KEY_BUZAHI, ah);
  idleLevel();
}
void BuzzerManager::stop() {
  if (_task) { vTaskDelete(_task); _task = nullptr; }
  toneOff();
}
void BuzzerManager::taskThunk(void* arg) {
  auto* pkg = static_cast<std::pair<BuzzerManager*, std::vector<Step>>*>(arg);
  BuzzerManager* self = pkg->first;
  std::vector<Step> steps = std::move(pkg->second);
  delete pkg;
  for (const auto& s : steps) {
    if (!self->_enabled) break;
    if (s.freq && s.durMs) {
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
  if (_task) { vTaskDelete(_task); _task = nullptr; }
  auto* pkg = new std::pair<BuzzerManager*, std::vector<Step>>(this, std::vector<Step>(steps, steps + count));
  xTaskCreate(taskThunk, "BZPAT", 2048, pkg, 1, &_task);
}
void BuzzerManager::play(Event ev) {
  if (!_enabled) return;
  switch (ev) {
    case EV_STARTUP:      { static const Step k[]={{700,60,40},{1200,60,40},{1700,80,0}};   runPattern(k, sizeof(k)/sizeof(k[0])); break; }
    case EV_PAIR_REQUEST: { static const Step k[]={{900,60,60},{1200,60,60},{1500,120,200}}; runPattern(k, sizeof(k)/sizeof(k[0])); break; }
    case EV_CONFIG_PROMPT:{ static const Step k[]={{1100,40,80},{1100,40,0}};               runPattern(k, sizeof(k)/sizeof(k[0])); break; }
    case EV_CONFIG_MODE:  { static const Step k[]={{1000,40,60},{1000,40,200},{1000,40,0}}; runPattern(k, sizeof(k)/sizeof(k[0])); break; }
    case EV_CONFIG_SAVED: { static const Step k[]={{1400,60,40},{1800,80,0}};               runPattern(k, sizeof(k)/sizeof(k[0])); break; }
    case EV_LINK_UP:      { static const Step k[]={{1100,40,30},{1400,50,0}};               runPattern(k, sizeof(k)/sizeof(k[0])); break; }
    case EV_LINK_DOWN:    { static const Step k[]={{1000,60,40},{800,60,0}};                runPattern(k, sizeof(k)/sizeof(k[0])); break; }
    case EV_MAINS_PRESENT:{ static const Step k[]={{900,60,30},{1200,60,0}};                runPattern(k, sizeof(k)/sizeof(k[0])); break; }
    case EV_MAINS_LOST:   { static const Step k[]={{500,180,120},{500,180,0}};              runPattern(k, sizeof(k)/sizeof(k[0])); break; }
    case EV_ON_BATTERY:   { static const Step k[]={{950,50,50},{1050,50,0}};                runPattern(k, sizeof(k)/sizeof(k[0])); break; }
    case EV_BAT_CHARGING: { static const Step k[]={{1200,30,40},{1200,30,0}};               runPattern(k, sizeof(k)/sizeof(k[0])); break; }
    case EV_BAT_FULL:     { static const Step k[]={{1600,80,0}};                            runPattern(k, sizeof(k)/sizeof(k[0])); break; }
    case EV_LOW_BAT:      { static const Step k[]={{450,120,120},{450,120,0}};              runPattern(k, sizeof(k)/sizeof(k[0])); break; }
    case EV_48V_ON:       { static const Step k[]={{1000,40,30},{1300,60,0}};               runPattern(k, sizeof(k)/sizeof(k[0])); break; }
    case EV_48V_OFF:      { static const Step k[]={{900,40,30},{700,60,0}};                 runPattern(k, sizeof(k)/sizeof(k[0])); break; }
    case EV_OVERCURRENT:  { static const Step k[]={{300,80,40},{300,80,40},{300,80,0}};     runPattern(k, sizeof(k)/sizeof(k[0])); break; }
    case EV_OVERTEMP:     { static const Step k[]={{2000,40,60},{2000,40,60},{2000,40,0}};  runPattern(k, sizeof(k)/sizeof(k[0])); break; }
    case EV_COMM_ERROR:   { static const Step k[]={{800,50,120},{800,50,0}};                runPattern(k, sizeof(k)/sizeof(k[0])); break; }
    case EV_BITE_PASS:    { static const Step k[]={{1200,50,30},{1500,50,30},{1800,60,0}};  runPattern(k, sizeof(k)/sizeof(k[0])); break; }
    case EV_BITE_FAIL:    { static const Step k[]={{500,80,60},{500,80,60},{500,120,0}};    runPattern(k, sizeof(k)/sizeof(k[0])); break; }
    case EV_SHUTDOWN:     { static const Step k[]={{1500,60,40},{1000,60,40},{700,60,0}};   runPattern(k, sizeof(k)/sizeof(k[0])); break; }
    case EV_FAULT:        { static const Step k[]={{350,100,60},{350,100,60},{350,100,60},{350,100,0}}; runPattern(k, sizeof(k)/sizeof(k[0])); break; }
  }
}
void BuzzerManager::playFromStatus(const Status& now, const Status* prev) {
  if (!_enabled) return;
  if (now.fault) { if (!prev || (prev && !prev->fault)) { play(EV_FAULT); } return; }
  if (now.lowBat) { if (!prev || (prev && !prev->lowBat)) { play(EV_LOW_BAT); } }
  if (now.overTemp) { if (!prev || (prev && !prev->overTemp)) { play(EV_OVERTEMP); } }
  if (now.overCurrent) { if (!prev || (prev && !prev->overCurrent)) { play(EV_OVERCURRENT); } }
  if (prev) {
    if (prev->mains && !now.mains) { play(EV_MAINS_LOST); return; }
    if (!prev->mains && now.mains) { play(EV_MAINS_PRESENT); }
    if (!prev->onBattery && now.onBattery) { play(EV_ON_BATTERY); }
    if (!prev->charging && now.charging) { play(EV_BAT_CHARGING); }
    if (!prev->batFull && now.batFull) { play(EV_BAT_FULL); }
    if (!prev->rail48V && now.rail48V) { play(EV_48V_ON); }
    if (prev->rail48V && !now.rail48V) { play(EV_48V_OFF); }
    if (!prev->linkUp && now.linkUp) { play(EV_LINK_UP); }
    if (prev->linkUp && !now.linkUp) { play(EV_LINK_DOWN); }
    if (!prev->commError && now.commError) { play(EV_COMM_ERROR); }
  } else {
    if (!now.mains) play(EV_MAINS_LOST);
    if (now.onBattery) play(EV_ON_BATTERY);
    if (now.lowBat) play(EV_LOW_BAT);
    if (now.overTemp) play(EV_OVERTEMP);
    if (now.overCurrent) play(EV_OVERCURRENT);
    if (now.rail48V) play(EV_48V_ON);
  }
}
void BuzzerManager::onSetRail48VResult(bool requestedOn, bool ok) { if (!ok) { play(EV_FAULT); return; } play(requestedOn ? EV_48V_ON : EV_48V_OFF); }
void BuzzerManager::onClearFaultResult(bool ok) { play(ok ? EV_BITE_PASS : EV_BITE_FAIL); }
void BuzzerManager::onEnterConfigMode() { play(EV_CONFIG_MODE); }
void BuzzerManager::onSaveConfig(bool ok) { play(ok ? EV_CONFIG_SAVED : EV_BITE_FAIL); }
void BuzzerManager::onLinkChange(bool up) { play(up ? EV_LINK_UP : EV_LINK_DOWN); }
void BuzzerManager::onShutdownRequested() { play(EV_SHUTDOWN); }
 