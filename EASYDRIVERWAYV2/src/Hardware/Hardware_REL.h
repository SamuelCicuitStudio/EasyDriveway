/**************************************************************
 *  Project     : EasyDriveway
 *  File        : Hardware_REL.h
 *  Purpose     : Hardware pin assignment mapping (no optionals).
 *  Author      : Tshibangu Samuel
 *  Role        : Freelance Embedded Systems Engineer
 *  Expertise   : Secure IoT Systems, Embedded C++, RTOS, Control Logic
 *  Contact     : tshibsamuel47@gmail.com
 *  Phone       : +216 54 429 793
 *  Created     : 2025-10-05
 *  Version     : 1.0.0
 **************************************************************/
#ifndef HARDWARE_REL_H
#define HARDWARE_REL_H

// INCLUDES

/** @brief SPI SCK for NAND */
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

/** @brief Relay channel 1 driver output pin */
#define RELAY1_OUT_PIN         10
/** @brief Relay channel 2 driver output pin */
#define RELAY2_OUT_PIN         11
/** @brief DS18B20 (board temperature) OneWire pin */
#define ONEWIRE_DS18B20_PIN    26

/** @brief Discrete RGB LED: Red pin */
#define RGB_R_PIN              6
/** @brief Discrete RGB LED: Green pin */
#define RGB_G_PIN              7
/** @brief Discrete RGB LED: Blue pin */
#define RGB_B_PIN              15

#endif // HARDWARE_REL_H
