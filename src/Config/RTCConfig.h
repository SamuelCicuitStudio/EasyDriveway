/**************************************************************
 *  Project     : EasyDriveway
 *  File        : RTCConfig.h
 *  Purpose     : DS3231 register map + RTC config keys/defaults.
 *  Author      : Tshibangu Samuel
 *  Role        : Freelance Embedded Systems Engineer
 *  Expertise   : Secure IoT Systems, Embedded C++, RTOS, Control Logic
 *  Contact     : tshibsamuel47@gmail.com
 *  Portfolio   : https://www.freelancer.com/u/tshibsamuel477
 *  Phone       : +216 54 429 793
 *  Created     : 2025-10-05
 *  Version     : 1.0.0
 **************************************************************/
#ifndef RTC_CONFIG_H
#define RTC_CONFIG_H

/**
 * @name Preferences Keys
 * @brief Keys used to persist RTC/I2C configuration.
 * @{ */
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
/** @} */

/**
 * @name Default Values
 * @brief Board defaults for RTC/I2C pins and model.
 * @{ */
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
/** @} */

/**
 * @name DS3231 Register Map & Bits
 * @brief I2C address, registers, and control/status bits.
 * @{ */
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
#define DS3231_CTRL_EOSC         0x80
#define DS3231_CTRL_BBSQW        0x40
#define DS3231_CTRL_CONV         0x20
#define DS3231_CTRL_RS2          0x10
#define DS3231_CTRL_RS1          0x08
#define DS3231_CTRL_INTCN        0x04
#define DS3231_CTRL_A2IE         0x02
#define DS3231_CTRL_A1IE         0x01
#define DS3231_STAT_OSF          0x80
#define DS3231_STAT_BB32KHZ      0x40
#define DS3231_STAT_CRATE1       0x20
#define DS3231_STAT_CRATE0       0x10
#define DS3231_STAT_EN32KHZ      0x08
#define DS3231_STAT_BSY          0x04
#define DS3231_STAT_A2F          0x02
#define DS3231_STAT_A1F          0x01
/** @} */

#endif // RTC_CONFIG_H
