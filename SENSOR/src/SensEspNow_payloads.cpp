
#include "SensEspNow.h"
#include <math.h>
#include <string.h>

bool SensorEspNowManager::makeDayNight_(uint8_t &is_day_out){
  is_day_out = 255;
  if (!als_) return false;
  float lux = 0;
  if (!als_->read(lux)) return false;

  int t0 = cfg_->GetInt(ALS_T1_LUX_KEY, ALS_T1_LUX_DEFAULT);
  int t1 = cfg_->GetInt(ALS_T0_LUX_KEY, ALS_T0_LUX_DEFAULT);
  static uint8_t state = 255;
  uint8_t s = state;
  if (s == 255) s = (lux > t0) ? 1 : 0;
  else {
    if      (lux > t0) s = 1;
    else if (lux < t1) s = 0;
  }
  state = s;
  is_day_out = s;
  return true;
}

bool SensorEspNowManager::makeTfRaw_(uint8_t which, TfLunaRawPayload& out){
  memset(&out, 0, sizeof(out));
  out.ver = 1;
  out.which = which;
  out.t_ms = millis();

  if (!tf_) return false;

  TFLunaManager::Sample a{}, b{};
  uint16_t rate = 0;
  bool ok = false;

  if (which == 0) {
    ok = tf_->readBoth(a, b, rate);
  } else if (which == 1) {
    ok = tf_->readA(a);
    TFLunaManager::Sample ta{}, tb{}; uint16_t r=0;
    if (tf_->fetch(0, ta, tb, r)) rate = r;
  } else if (which == 2) {
    ok = tf_->readB(b);
    TFLunaManager::Sample ta{}, tb{}; uint16_t r=0;
    if (tf_->fetch(0, ta, tb, r)) rate = r;
  } else {
    ok = tf_->readBoth(a, b, rate);
  }

  if (!ok) return false;

  out.rate_hz  = rate;
  out.distA_mm = a.dist_mm; out.ampA = a.amp; out.okA = a.ok ? 1 : 0;
  out.distB_mm = b.dist_mm; out.ampB = b.amp; out.okB = b.ok ? 1 : 0;
  return true;
}

bool SensorEspNowManager::makeEnv_(SensorEnvPayload& out){
  memset(&out, 0, sizeof(out));
  out.ver = 1;
  if (bme_) {
    float t=0, rh=0, pPa=0;
    if (bme_->read(t, rh, pPa)) {
      out.tC_x100 = (int16_t)roundf(t * 100.0f); out.okT = 1;
      out.rh_x100 = (uint16_t)roundf(rh * 100.0f); out.okH = 1;
      out.p_Pa    = (int32_t)roundf(pPa); out.okP = 1;
    }
  }
  uint8_t dn=255; float lux=0;
  if (als_ && als_->read(lux)) {
    out.lux_x10 = (uint16_t)roundf(lux * 10.0f);
    out.okL = 1;
    if (makeDayNight_(dn)) out.is_day = dn; else out.is_day = 255;
  } else {
    out.is_day = 255;
  }
  return true;
}
