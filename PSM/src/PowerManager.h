/**************************************************************
 *  Project : EasyDriveWay - PSM
 *  File    : PowerManager.h
 *  Purpose : Unified power/measurement manager for the PSM.
 *            - Controls rails (48V / 5V) and reads mains sense
 *            - Measures VBUS(48V) / IBUS, VBAT / IBAT using ADC + ACS781
 *            - Computes fault bitfield (OVP/UVP/OCP/OTP/Brownout/MainsFail)
 *            - Exposes C-style wrappers expected by PSMEspNowManager
 *            - Controls CHARGER ENABLE (start/stop battery charging)
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

// Charger enable (output GPIO to enable/disable the charger)
#ifndef CHARGER_EN_PIN_KEY
  #define CHARGER_EN_PIN_KEY "CHEN"
#endif
#ifndef CHARGER_EN_PIN_DEFAULT
  #define CHARGER_EN_PIN_DEFAULT 10
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

// ------------------ Default scales/thresholds (48 V system) ------------------
// Adjust via NVS to your exact hardware; these are sane starting points.
#define DEF_V48_NUM         1
#define DEF_V48_DEN         1
#define DEF_VBAT_NUM        1
#define DEF_VBAT_DEN        1

#define DEF_VBUS_OVP_MV     56000   // 56.0 V (BUS OVP)
#define DEF_VBUS_UVP_MV     36000   // 36.0 V (BUS UVP / brownout)

#define DEF_IBUS_OCP_MA     20000   // 20 A (48 V bus)

#define DEF_VBAT_OVP_MV     58400   // 58.4 V (16S Li-ion @ 4.10–4.20 V/cell)
#define DEF_VBAT_UVP_MV     44000   // 44.0 V (typical low threshold)
#define DEF_IBAT_OCP_MA     10000   // 10 A (battery)

#define DEF_OTP_C           85.0f   // °C (board over-temp)

// ------------------ Fault bitfield mapping ------------------
enum PowerFaultBits : uint8_t {
  PWR_FAULT_OVP       = 1 << 0,  // Over-voltage on VBUS or VBAT
  PWR_FAULT_UVP       = 1 << 1,  // Under-voltage on VBUS/VBAT
  PWR_FAULT_OCP       = 1 << 2,  // Over-current on IBUS/IBAT
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
  bool setChargeEnable(bool en);   // NEW: enable/disable charger path

  // ---- State ----
  bool    is48VOn();               // sense VBUS vs UVP threshold (or > ~10V)
  bool    mainsPresent();          // digital/analog threshold
  bool    isChargeEnabled();       // NEW: read back charger enable state
  uint8_t readFaultBits();         // builds bitfield from thresholds & status

  // ---- Measurements (real-world units) ----
  uint16_t measure48V_mV();        // 48 V bus (scaled mV)
  uint16_t measure48V_mA();        // IBUS via ACS781
  uint16_t measureBat_mV();        // VBAT (scaled mV)
  uint16_t measureBat_mA();        // IBAT via ACS781 (signed -> uint16_t if needed)

  // ---- Aux ----
  void setBoardTempC(float tC) { _boardTempC = tC; }

  // ---- Raw accessors ----
  int pinPwr48()   const { return _pinPwr48En; }
  int pinPwr5()    const { return _pinPwr5vEn; }
  int pinMains()   const { return _pinMainsSense; }
  int pinCharger() const { return _pinChgEn; }      // NEW

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
  int _pinIBatAdc    = -1; // optional ACS781 for battery current
  int _pinChgEn      = -1; // NEW: charger enable (output)

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

  float _otp_c       = DEF_OTP_C;
  float _boardTempC  = NAN;

  // Sensors
  ACS781* _ibus = nullptr;  // 48 V bus current sensor
  ACS781* _ibat = nullptr;  // battery current sensor (optional)
};
