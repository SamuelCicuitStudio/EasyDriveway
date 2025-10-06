/**************************************************************
 *  Project     : EasyDriveway
 *  File        : Hardware_SENS.h
 *  Purpose     : Centralized configuration keys, defaults, and
 *                hardware pin assignment mapping for ICM firmware.
 *  Author      : Tshibangu Samuel
 *  Role        : Freelance Embedded Systems Engineer
 *  Expertise   : Secure IoT Systems, Embedded C++, RTOS, Control Logic
 *  Contact     : tshibsamuel47@gmail.com
 *  Phone       : +216 54 429 793
 *  Created     : 2025-10-05
 *  Version     : 1.0.0
 **************************************************************/
#ifndef HARDWARE_SENS_H
#define HARDWARE_SENS_H

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

/** @brief System I2C SCL (TF-Luna pair, no mux) */
#define I2C_SYS_SCL_PIN        33
/** @brief System I2C SDA (TF-Luna pair, no mux) */
#define I2C_SYS_SDA_PIN        34
/** @brief System I2C frequency (Hz) */
#define I2C_SYS_HZ             400000UL

/** @brief TF-Luna A I2C address */
#define TFL_ADDR_A             0x10
/** @brief TF-Luna B I2C address */
#define TFL_ADDR_B             0x11

/** @brief Environmental I2C SCL (BME280 + VEML7700) */
#define I2C_ENV_SCL_PIN        35
/** @brief Environmental I2C SDA (BME280 + VEML7700) */
#define I2C_ENV_SDA_PIN        36
/** @brief Environmental I2C frequency (Hz) */
#define I2C_ENV_HZ             400000UL
/** @brief BME280 I2C address */
#define BME280_I2C_ADDR        0x76
/** @brief VEML7700 I2C address */
#define VEML7700_I2C_ADDR      0x10

/** @brief Discrete RGB LED: Red pin */
#define RGB_R_PIN              6
/** @brief Discrete RGB LED: Green pin */
#define RGB_G_PIN              7
/** @brief Discrete RGB LED: Blue pin */
#define RGB_B_PIN              15

#endif // HARDWARE_SENS_H
