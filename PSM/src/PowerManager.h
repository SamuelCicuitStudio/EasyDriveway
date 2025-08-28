/**************************************************************
 *  Project : EasyDriveWay - PSM
 *  File    : PowerManager.h
 *  Purpose : Unified power/measurement manager for the PSM.
 *            - Controls rails (48V / 5V) and reads mains sense
 *            - Measures VBUS(48V) / IBUS, VBAT / IBAT using ADC + ACS781
 *            - Computes fault bitfield (OVP/UVP/OCP/OTP/Brownout/MainsFail)
 *            - Exposes C-style wrappers expected by PSMEspNowManager
 **************************************************************/
#pragma once
#include <Arduino.h>
#include "Config.h"
#include "ConfigManager.h"
#include "ACS781.h"

// ------------------ Optional config keys (kept short ≤6) ------------------
// Battery current ADC pin (second ACS781). If absent, IBAT reading falls back to 0.
#ifndef IBAT_ADC_PIN_KEY
  #define IBAT_ADC_PIN_KEY  "IBATAD"
#endif

// Voltage divider scale = (ADC_mV * NUM / DEN). Defaults to 1:1 if not provided.
#ifndef V48_SCALE_NUM_KEY
  #define V48_SCALE_NUM_KEY "V48SN"
#endif
#ifndef V48_SCALE_DEN_KEY
  #define V48_SCALE_DEN_KEY "V48SD"
#endif
#ifndef VBAT_SCALE_NUM_KEY
  #define VBAT_SCALE_NUM_KEY "VBTSN"
#endif
#ifndef VBAT_SCALE_DEN_KEY
  #define VBAT_SCALE_DEN_KEY "VBTSD"
#endif

// Threshold keys (mV / mA / °C). If not present, defaults below are used.
#ifndef VBUS_OVP_MV_KEY
  #define VBUS_OVP_MV_KEY   "VBOVP"
#endif
#ifndef VBUS_UVP_MV_KEY
  #define VBUS_UVP_MV_KEY   "VBUVP"
#endif
#ifndef IBUS_OCP_MA_KEY
  #define IBUS_OCP_MA_KEY   "BIOCP"
#endif
#ifndef VBAT_OVP_MV_KEY
  #define VBAT_OVP_MV_KEY   "VBOVB"
#endif
#ifndef VBAT_UVP_MV_KEY
  #define VBAT_UVP_MV_KEY   "VBUVB"
#endif
#ifndef IBAT_OCP_MA_KEY
  #define IBAT_OCP_MA_KEY   "BAOCP"
#endif
#ifndef OTP_C_KEY
  #define OTP_C_KEY         "OTPC"
#endif

// Defaults (safe, adjust via NVS as needed)
#define DEF_V48_NUM      1
#define DEF_V48_DEN      1
#define DEF_VBAT_NUM     1
#define DEF_VBAT_DEN     1

#define DEF_VBUS_OVP_MV  56000   // 56V
#define DEF_VBUS_UVP_MV  36000   // 36V
#define DEF_IBUS_OCP_MA  20000   // 20A

#define DEF_VBAT_OVP_MV  14600   // 4S Li-ion max ~14.6V
#define DEF_VBAT_UVP_MV  11000   // UVP ~11.0V (example)
#define DEF_IBAT_OCP_MA  10000   // 10A

#define DEF_OTP_C        85.0f   // °C

// ------------------ Fault bitfield mapping ------------------
enum PowerFaultBits : uint8_t {
  PWR_FAULT_OVP       = 1 << 0,  // Over-voltage on VBUS or VBAT
  PWR_FAULT_UVP       = 1 << 1,  // Under-voltage on VBUS (or VBAT if relevant)
  PWR_FAULT_OCP       = 1 << 2,  // Over-current on IBUS or IBAT
  PWR_FAULT_OTP       = 1 << 3,  // Over-temperature (board)
  PWR_FAULT_BROWNOUT  = 1 << 4,  // Mains brownout / absent
  PWR_FAULT_RESERVED5 = 1 << 5,  // reserved (future: charger fault)
  PWR_FAULT_RESERVED6 = 1 << 6,
  PWR_FAULT_RESERVED7 = 1 << 7
};

class PowerManager {
public:
  explicit PowerManager(ConfigManager* cfg);

  bool begin();

  // ---- Control ----
  bool set48V(bool on);
  bool set5V(bool on);

  // ---- State ----
  bool    is48VOn();                   // by sensing VBUS vs UVP threshold (or simple >10V)
  bool    mainsPresent();              // digital/analog threshold; here non-zero -> present
  uint8_t readFaultBits();             // builds bitfield from thresholds & status

  // ---- Measurements (real-world units) ----
  uint16_t measure48V_mV();            // 48V bus (scaled from ADC mV)
  uint16_t measure48V_mA();            // IBUS via ACS781
  uint16_t measureBat_mV();            // VBAT (scaled from ADC mV)
  uint16_t measureBat_mA();            // IBAT via ACS781 (signed folded into uint16_t with clamp at 0..65535)
  // Note: IBAT sign convention used by payload is handled by PSM layer if needed.

  // ---- Aux ----
  void setBoardTempC(float tC) { _boardTempC = tC; }

  // ---- Raw accessors ----
  int pinPwr48() const { return _pinPwr48En; }
  int pinPwr5()  const { return _pinPwr5vEn; }
  int pinMains() const { return _pinMainsSense; }

private:
  // Helpers
  void loadPins();
  void loadScales();
  void loadThresholds();

  static uint16_t scale_mV(uint16_t adc_mV, int num, int den);
  static bool     readDigitalOrAnalogHigh(int pin);

private:
  ConfigManager* _cfg = nullptr;

  // Pins
  int _pinPwr48En    = -1;
  int _pinPwr5vEn    = -1;
  int _pinMainsSense = -1;
  int _pinV48Adc     = -1;
  int _pinVBatAdc    = -1;
  int _pinI48Adc     = -1;
  int _pinIBatAdc    = -1; // opACS781tional

  // Scales (numerator/denominator)
  int _v48_num  = DEF_V48_NUM;
  int _v48_den  = DEF_V48_DEN;
  int _vbat_num = DEF_VBAT_NUM;
  int _vbat_den = DEF_VBAT_DEN;

  // Thresholds
  int _vbus_ovp_mv = DEF_VBUS_OVP_MV;
  int _vbus_uvp_mv = DEF_VBUS_UVP_MV;
  int _ibus_ocp_ma = DEF_IBUS_OCP_MA;

  int _vbat_ovp_mv = DEF_VBAT_OVP_MV;
  int _vbat_uvp_mv = DEF_VBAT_UVP_MV;
  int _ibat_ocp_ma = DEF_IBAT_OCP_MA;

  float _otp_c     = DEF_OTP_C;
  float _boardTempC = NAN;

  // Sensors
  ACS781* _ibus = nullptr;  // 48V bus current sensor
  ACS781* _ibat = nullptr;  // battery current sensor (optional)
};

