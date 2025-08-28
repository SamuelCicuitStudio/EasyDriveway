/**************************************************************
 *  Project : EasyDriveWay - PSM
 *  File    : PowerManager.cpp
 **************************************************************/
#include "PowerManager.h"

static PowerManager* s_pm = nullptr;

PowerManager::PowerManager(ConfigManager* cfg) : _cfg(cfg) {}

bool PowerManager::begin() {
  loadPins();
  loadScales();
  loadThresholds();

  if (_pinPwr48En >= 0) pinMode(_pinPwr48En, OUTPUT);
  if (_pinPwr5vEn >= 0) pinMode(_pinPwr5vEn, OUTPUT);
  if (_pinMainsSense >= 0) pinMode(_pinMainsSense, INPUT);

  if (_pinI48Adc >= 0) {
    _ibus = new ACS781(_pinI48Adc, _cfg);
    _ibus->begin();
  }
  if (_pinIBatAdc >= 0) {
    _ibat = new ACS781(_pinIBatAdc, _cfg);
    _ibat->begin();
  }
  return true;
}

// -------------------- Control --------------------
bool PowerManager::set48V(bool on) {
  if (_pinPwr48En < 0) return false;
  digitalWrite(_pinPwr48En, on ? HIGH : LOW);
  return true;
}

bool PowerManager::set5V(bool on) {
  if (_pinPwr5vEn < 0) return false;
  digitalWrite(_pinPwr5vEn, on ? HIGH : LOW);
  return true;
}

// -------------------- State --------------------
bool PowerManager::is48VOn() {
  // Consider on if measured VBUS >= UVP threshold; if no ADC, use output state
  if (_pinV48Adc >= 0) {
    uint16_t v = measure48V_mV();
    int thr = _vbus_uvp_mv > 0 ? _vbus_uvp_mv : 10000; // 10V fallback
    return v >= (uint16_t)thr;
  }
  if (_pinPwr48En >= 0) return digitalRead(_pinPwr48En) == HIGH;
  return false;
}

bool PowerManager::mainsPresent() {
  return readDigitalOrAnalogHigh(_pinMainsSense);
}

uint8_t PowerManager::readFaultBits() {
  uint8_t f = 0;
  // Measure once for coherent snapshot
  uint16_t vbus = measure48V_mV();
  uint16_t vbat = measureBat_mV();
  uint16_t ibus = measure48V_mA();
  uint16_t ibat = measureBat_mA();

  if (vbus > _vbus_ovp_mv || vbat > _vbat_ovp_mv) f |= PWR_FAULT_OVP;
  if (vbus < _vbus_uvp_mv /*|| vbat < _vbat_uvp_mv*/) f |= PWR_FAULT_UVP;
  if (ibus > _ibus_ocp_ma || ibat > _ibat_ocp_ma)     f |= PWR_FAULT_OCP;

  if (!isnan(_boardTempC) && _boardTempC > _otp_c)    f |= PWR_FAULT_OTP;
  if (!mainsPresent())                                 f |= PWR_FAULT_BROWNOUT;

  return f;
}

// -------------------- Measurements --------------------
uint16_t PowerManager::measure48V_mV() {
  if (_pinV48Adc < 0) return 0;
  uint16_t mvadc = analogReadMilliVolts(_pinV48Adc);
  return scale_mV(mvadc, _v48_num, _v48_den);
}

uint16_t PowerManager::measure48V_mA() {
  if (!_ibus) return 0;
  float a = _ibus->readAmps();
  long ma = lroundf(a * 1000.0f);
  if (ma < 0) ma = -ma;       // return magnitude in mA for fault logic
  if (ma > 65535) ma = 65535;
  return (uint16_t)ma;
}

uint16_t PowerManager::measureBat_mV() {
  if (_pinVBatAdc < 0) return 0;
  uint16_t mvadc = analogReadMilliVolts(_pinVBatAdc);
  return scale_mV(mvadc, _vbat_num, _vbat_den);
}

uint16_t PowerManager::measureBat_mA() {
  if (!_ibat) return 0;
  float a = _ibat->readAmps();
  long ma = lroundf(a * 1000.0f);
  // Sign convention: + charging, - discharging. Here we clamp to uint16 magnitude.
  if (ma < 0) ma = -ma;
  if (ma > 65535) ma = 65535;
  return (uint16_t)ma;
}

// -------------------- Internals --------------------
void PowerManager::loadPins() {
  _pinPwr48En    = _cfg->GetInt(PWR48_EN_PIN_KEY,  PWR48_EN_PIN_DEFAULT);
  _pinPwr5vEn    = _cfg->GetInt(PWR5V_EN_PIN_KEY,  PWR5V_EN_PIN_DEFAULT);
  _pinMainsSense = _cfg->GetInt(MAINS_SENSE_PIN_KEY, MAINS_SENSE_PIN_DEFAULT);
  _pinV48Adc     = _cfg->GetInt(V48_ADC_PIN_KEY,   V48_ADC_PIN_DEFAULT);
  _pinVBatAdc    = _cfg->GetInt(VBAT_ADC_PIN_KEY,  VBAT_ADC_PIN_DEFAULT);
  _pinI48Adc     = _cfg->GetInt(I48_ADC_PIN_KEY,   I48_ADC_PIN_DEFAULT);
  // IBAT pin may not exist in config; default to -1
  int ibat = _cfg->GetInt(IBAT_ADC_PIN_KEY, -1);
  _pinIBatAdc    = ibat;
}

void PowerManager::loadScales() {
  _v48_num  = _cfg->GetInt(V48_SCALE_NUM_KEY, DEF_V48_NUM);
  _v48_den  = _cfg->GetInt(V48_SCALE_DEN_KEY, DEF_V48_DEN);
  _vbat_num = _cfg->GetInt(VBAT_SCALE_NUM_KEY, DEF_VBAT_NUM);
  _vbat_den = _cfg->GetInt(VBAT_SCALE_DEN_KEY, DEF_VBAT_DEN);
  if (_v48_den == 0)  _v48_den = 1;
  if (_vbat_den == 0) _vbat_den = 1;
}

void PowerManager::loadThresholds() {
  _vbus_ovp_mv = _cfg->GetInt(VBUS_OVP_MV_KEY, DEF_VBUS_OVP_MV);
  _vbus_uvp_mv = _cfg->GetInt(VBUS_UVP_MV_KEY, DEF_VBUS_UVP_MV);
  _ibus_ocp_ma = _cfg->GetInt(IBUS_OCP_MA_KEY, DEF_IBUS_OCP_MA);

  _vbat_ovp_mv = _cfg->GetInt(VBAT_OVP_MV_KEY, DEF_VBAT_OVP_MV);
  _vbat_uvp_mv = _cfg->GetInt(VBAT_UVP_MV_KEY, DEF_VBAT_UVP_MV);
  _ibat_ocp_ma = _cfg->GetInt(IBAT_OCP_MA_KEY, DEF_IBAT_OCP_MA);

  // Temperature threshold (°C). Store as int and convert to float.
  int otpC = _cfg->GetInt(OTP_C_KEY, (int)DEF_OTP_C);
  _otp_c = (float)otpC;
}

// Scale integer safely (mV * num / den)
uint16_t PowerManager::scale_mV(uint16_t adc_mV, int num, int den) {
  uint32_t v = (uint32_t)adc_mV * (uint32_t)num;
  v /= (uint32_t)den;
  if (v > 65535U) v = 65535U;
  return (uint16_t)v;
}

bool PowerManager::readDigitalOrAnalogHigh(int pin) {
  if (pin < 0) return false;
  // Try digital read first; if always zero (floating), fall back to analog threshold
  pinMode(pin, INPUT);
  int d = digitalRead(pin);
  if (d == HIGH) return true;
  // Analog heuristic: > 800 mV considered HIGH (tune if needed)
  uint16_t mv = analogReadMilliVolts(pin);
  return (mv > 800);
}


