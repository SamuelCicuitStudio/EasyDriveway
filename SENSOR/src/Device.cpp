
#include "Device.h"
// Buzzer header is available in your project — included by main.cpp
#include "BuzzerManager.h"

// ========== Ctor ==========
Device::Device(ConfigManager* cfg,
               SensorEspNowManager* now,
               TFLunaManager* tf,
               BME280Manager* bme,
               SwitchManager* sw,
               RGBLed* led,
               BuzzerManager* buz)
: cfg_(cfg), now_(now), tf_(tf), bme_(bme), sw_(sw), led_(led), buz_(buz) {}

// ========== Public ==========
bool Device::begin(){
  // Initial state determination
  mode_ = IDLE;

  // Create BME periodic task
  xTaskCreatePinnedToCore(&Device::taskBMEThunk, "dev.bme",
                          DEVICE_TASK_STACK, this,
                          DEVICE_TASK_PRIO, &hBME_, DEVICE_TASK_CORE);

  // Create TF watcher task
  xTaskCreatePinnedToCore(&Device::taskTFThunk, "dev.tf",
                          DEVICE_TF_TASK_STACK, this,
                          DEVICE_TF_TASK_PRIO, &hTF_, DEVICE_TF_TASK_CORE);
  return true;
}

void Device::loopOnce(){
  stepStateMachine();
}

// ========== Tasks ==========
void Device::taskBMEThunk(void* arg){ reinterpret_cast<Device*>(arg)->taskBMELoop(); }
void Device::taskTFThunk (void* arg){ reinterpret_cast<Device*>(arg)->taskTFLoop(); }

void Device::taskBMELoop(){
  // 5-minute periodic read; optional: report to ICM
  for(;;){
    if (bme_) {
      float t= NAN, rh= NAN, p= NAN;
      if (bme_->read(t, rh, p)) {
        // Optionally mirror ENV upstream (lightweight)
        if (now_) now_->sendEnv();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(DEVICE_BME_PERIOD_MS));
  }
}

void Device::taskTFLoop(){
  // Poll the TF‑Luna pair frequently to detect direction/speed
  for(;;){
    if (mode_ == RUNNING && tf_ && now_ && now_->hasTopology()) {
      uint16_t speed=0; int8_t dir=0;
      if (detectVehicle_(speed, dir)) {
        fanoutWave_(speed, dir);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(DEVICE_TF_POLL_MS));
  }
}

// ========== State machine ==========
void Device::stepStateMachine(){
  switch (mode_) {
    case IDLE: {
      // Decide next step based on NVS
      const bool paired = checkPaired_();
      const bool tfcfg  = checkTfConfigured_();
      const bool topo   = hasTopology_();

      if (!paired) {
        mode_ = PAIRING; promptPairing_();
        break;
      }
      if (paired && !tfcfg) {
        mode_ = CONFIG_TF; promptConfigTF_();
        break;
      }
      if ((paired && tfcfg) && !topo) {
        mode_ = WAIT_TOPO; // wait silently for ICM to push topology
        break;
      }
      // All conditions satisfied → RUNNING
      mode_ = RUNNING; clearPrompts_();
      break;
    }

    case PAIRING: {
      // If the ESP‑NOW manager learns token+MAC and a topology arrives, we can mark paired later.
      // Here we just keep prompting until token is learned (haveTok_) — reflected by DEVICE_PAIRED_KEY later.
      if (checkPaired_()) {
        // Move on to TF config (or to WAIT_TOPO if already configured)
        clearPrompts_();
        mode_ = checkTfConfigured_() ? WAIT_TOPO : CONFIG_TF;
      }
      break;
    }

    case CONFIG_TF: {
      // We expect the operator to use SwitchManager gestures:
      //   - double tap → ConfigTfAsA()
      //   - triple tap → ConfigTfAsB()
      // Once both sides are configured and reachable, mark key.
      if (checkTfConfigured_()) {
        clearPrompts_();
        mode_ = WAIT_TOPO;
      } else {
        // Provide gentle hint pulse once in a while
        promptConfigTF_();
      }
      break;
    }

    case WAIT_TOPO: {
      if (hasTopology_()) {
        // Only now we mark PAIR + TFL configured as committed
        markPairedAndConfigured_();
        clearPrompts_();
        mode_ = RUNNING;
      }
      break;
    }

    case RUNNING:
      // Nothing to do at high-level; tasks handle runtime work
      break;

    case ERROR:
    default:
      break;
  }
}

// ========== Helpers ==========
bool Device::checkPaired_() const {
  return cfg_ && cfg_->GetBool(DEVICE_PAIRED_KEY, DEVICE_PAIRED_DEFAULT);
}
bool Device::checkTfConfigured_() const {
  return cfg_ && cfg_->GetBool(TFL_CONFIGURED_KEY, TFL_CONFIGURED_DEFAULT);
}
bool Device::hasTopology_() const {
  return now_ && now_->hasTopology();
}
void Device::markPairedAndConfigured_(){
  if (!cfg_) return;
  cfg_->PutBool(DEVICE_PAIRED_KEY, true);
  cfg_->PutBool(TFL_CONFIGURED_KEY, true);
}

// Presence detection & speed calc
bool Device::detectVehicle_(uint16_t& speed_mmps_out, int8_t& dir_out){
  if (!tf_) return false;

  TFLunaManager::Sample a{}, b{};
  uint16_t rate=0;
  if (!tf_->readBoth(a,b,rate)) return false;

  // Presence decision uses manager's near/far gates
  const bool presA = tf_->isPresentA(a);
  const bool presB = tf_->isPresentB(b);

  // Rising edges mark timestamps
  const uint32_t nowMs = millis();
  if (presA && !prs_.presentA) prs_.tA_ms = nowMs;
  if (presB && !prs_.presentB) prs_.tB_ms = nowMs;
  prs_.presentA = presA;
  prs_.presentB = presB;

  // A simple event: both seen within a short window → compute speed & dir
  const uint32_t window_ms = 1500; // generous crossing window
  if (prs_.tA_ms && prs_.tB_ms &&
      abs((int32_t)prs_.tA_ms - (int32_t)prs_.tB_ms) <= (int32_t)window_ms) {

    uint32_t dt = (prs_.tA_ms > prs_.tB_ms) ? (prs_.tA_ms - prs_.tB_ms) : (prs_.tB_ms - prs_.tA_ms);
    if (dt < 20) dt = 20; // clamp
    const uint16_t spacing_mm = (uint16_t)cfg_->GetInt(AB_SPACING_MM_KEY, AB_SPACING_MM_DEFAULT);
    const uint32_t sp = (uint32_t)spacing_mm * 1000UL / dt;
    speed_mmps_out = (sp > 0xFFFF) ? 0xFFFF : (uint16_t)sp;

    // Direction: if A timestamp < B, movement A→B is considered +1
    dir_out = (prs_.tA_ms < prs_.tB_ms) ? +1 : -1;

    // Reset edge detector for next vehicle
    prs_.clear();
    return true;
  }

  // Timeout old partial events
  if ((prs_.tA_ms && nowMs - prs_.tA_ms > window_ms) ||
      (prs_.tB_ms && nowMs - prs_.tB_ms > window_ms)) {
    prs_.clear();
  }

  return false;
}

void Device::fanoutWave_(uint16_t speed_mmps, int8_t dir){
  if (!now_ || !now_->hasTopology()) return;

  // Pulse width & inter‑relay spacing from NVS
  const uint16_t on_ms      =  (uint16_t)cfg_->GetInt(CONFIRM_MS_KEY, CONFIRM_MS_DEFAULT);
  const uint16_t spacing_mm =  (uint16_t)cfg_->GetInt(AB_SPACING_MM_KEY, AB_SPACING_MM_DEFAULT);
  const uint16_t ttl_ms     =  (uint16_t)cfg_->GetInt(STOP_MS_KEY, STOP_MS_DEFAULT);

  // Fan out the wave on both lanes for a bright runway effect
  now_->playWave(/*lane=*/0, dir, speed_mmps, spacing_mm, on_ms, /*all_on_ms=*/0, ttl_ms, /*requireAck=*/false);
  now_->playWave(/*lane=*/1, dir, speed_mmps, spacing_mm, on_ms, /*all_on_ms=*/0, ttl_ms, /*requireAck=*/false);
}

// Prompts
void Device::promptPairing_(){
  if (led_) led_->startBlink(colorPairing_, 400);
  if (buz_) buz_->play(BuzzerManager::EV_PAIR_REQUEST);
}
void Device::promptConfigTF_(){
  if (led_) led_->startBlink(colorConfig_, 250);
  if (buz_) buz_->play(BuzzerManager::EV_CONFIG_PROMPT);
}
void Device::clearPrompts_(){
  if (led_) led_->stop();
  if (buz_) buz_->stop();
}
