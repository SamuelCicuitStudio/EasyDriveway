/**************************************************************
 *  Project : ICM (Interface Control Module)
 *  File    : RTCConfig.h
 *  Note    : DS3231 map + config keys/defaults
 **************************************************************/
#ifndef RTC_CONFIG_H
#define RTC_CONFIG_H

// -------- Config keys (values stored in Preferences) --------
#ifndef CFG_RTC_MODEL_KEY
#define CFG_RTC_MODEL_KEY        "RTC_MODEL"
#endif
#ifndef CFG_RTC_INT_PIN_KEY
#define CFG_RTC_INT_PIN_KEY      "RTC_INT_PIN"
#endif
#ifndef CFG_RTC_32K_PIN_KEY
#define CFG_RTC_32K_PIN_KEY      "RTC_32K_PIN"
#endif
#ifndef CFG_I2C_SCL_PIN_KEY
#define CFG_I2C_SCL_PIN_KEY      "I2C_SCL"
#endif
#ifndef CFG_I2C_SDA_PIN_KEY
#define CFG_I2C_SDA_PIN_KEY      "I2C_SDA"
#endif
#ifndef CFG_RTC_RST_PIN_KEY
#define CFG_RTC_RST_PIN_KEY      "RTC_RST"
#endif

// -------- Defaults (from your design) --------
#ifndef RTC_MODEL_DEFAULT
#define RTC_MODEL_DEFAULT        "DS3231MZ+TRL"
#endif
#ifndef RTC_INT_PIN_DEFAULT
#define RTC_INT_PIN_DEFAULT      19
#endif
#ifndef RTC_32K_PIN_DEFAULT
#define RTC_32K_PIN_DEFAULT      20
#endif
#ifndef I2C_SCL_PIN_DEFAULT
#define I2C_SCL_PIN_DEFAULT      4
#endif
#ifndef I2C_SDA_PIN_DEFAULT
#define I2C_SDA_PIN_DEFAULT      5
#endif
#ifndef RTC_RST_PIN_DEFAULT
#define RTC_RST_PIN_DEFAULT      40
#endif

// -------- DS3231 register map --------
#define DS3231_I2C_ADDR          0x68
#define DS3231_REG_SECONDS       0x00
#define DS3231_REG_MINUTES       0x01
#define DS3231_REG_HOURS         0x02
#define DS3231_REG_DAY           0x03
#define DS3231_REG_DATE          0x04
#define DS3231_REG_MONTH         0x05
#define DS3231_REG_YEAR          0x06
#define DS3231_REG_CONTROL       0x0E
#define DS3231_REG_STATUS        0x0F
// Control bits
#define DS3231_CTRL_EOSC         0x80  // 1 = oscillator disabled
#define DS3231_CTRL_BBSQW        0x40
#define DS3231_CTRL_CONV         0x20
#define DS3231_CTRL_RS2          0x10
#define DS3231_CTRL_RS1          0x08
#define DS3231_CTRL_INTCN        0x04  // 1 = use alarms on INT/SQW
#define DS3231_CTRL_A2IE         0x02
#define DS3231_CTRL_A1IE         0x01
// Status bits
#define DS3231_STAT_OSF          0x80  // 1 = Oscillator Stop Flag
#define DS3231_STAT_BB32KHZ      0x40
#define DS3231_STAT_CRATE1       0x20
#define DS3231_STAT_CRATE0       0x10
#define DS3231_STAT_EN32KHZ      0x08  // 1 = enable 32kHz on pin
#define DS3231_STAT_BSY          0x04
#define DS3231_STAT_A2F          0x02
#define DS3231_STAT_A1F          0x01

#endif // RTC_CONFIG_H
