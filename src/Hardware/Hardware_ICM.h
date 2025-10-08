/**************************************************************
 *  Project     : EasyDriveway
 *  File        : Hardware_ICM.h
 *  Purpose     : Hardware pin assignment mapping (no optionals).
 *  Author      : Tshibangu Samuel
 *  Role        : Freelance Embedded Systems Engineer
 *  Expertise   : Secure IoT Systems, Embedded C++, RTOS, Control Logic
 *  Contact     : tshibsamuel47@gmail.com
 *  Phone       : +216 54 429 793
 *  Created     : 2025-10-05
 *  Version     : 1.0.0
 **************************************************************/
#ifndef HARDWARE_ICM_H
#define HARDWARE_ICM_H

// INCLUDES

/** @brief SPI SCK for MKDV8GIL-AST (SPI mode @ 40 MHz) */
#define SD_NAND_SCK_PIN        41
/** @brief SPI MISO for NAND */
#define SD_NAND_MISO_PIN       42
/** @brief SPI MOSI for NAND */
#define SD_NAND_MOSI_PIN       39
/** @brief SPI CS for NAND */
#define SD_NAND_CS_PIN         38
/** @brief SPI bus frequency (Hz) */
#define SD_NAND_SPI_HZ         40000000

/** @brief Buzzer PWM-capable pin */
#define BUZZER_PIN             4
/** @brief Fan PWM MOSFET gate pin */
#define FAN_PWM_PIN            14
/** @brief Onboard status LED pin */
#define LED_ONBOARD_PIN        RGB_R_PIN

/** @brief I2C SCL for DS3231MZ (system RTC) */
#define I2C_SYS_SCL_PIN        35
/** @brief I2C SDA for DS3231MZ (system RTC) */
#define I2C_SYS_SDA_PIN        36
/** @brief DS3231 SQW/INT pin */
#define DS3231_INT_PIN         18

/** @brief DS18B20 (board temperature) OneWire pin */
#define ONEWIRE_DS18B20_PIN    26

/** @brief Discrete RGB LED: Red pin */
#define RGB_R_PIN              15
/** @brief Discrete RGB LED: Green pin */
#define RGB_G_PIN              16
/** @brief Discrete RGB LED: Blue pin */
#define RGB_B_PIN              17

#endif // HARDWARE_ICM_H
