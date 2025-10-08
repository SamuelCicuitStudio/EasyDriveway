/**************************************************************
 *  Project     : EasyDriveway
 *  File        : Hardware_REMU.h
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
#ifndef HARDWARE_REMU_H
#define HARDWARE_REMU_H

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
/** @brief Onboard status LED pin (repurposed to RGB red) */
#define LED_ONBOARD_PIN        RGB_R_PIN

/** @brief 74HC595 SER (data in) */
#define SR_SER_PIN             10
/** @brief 74HC595 SRCLK (shift clock) */
#define SR_SCK_PIN             11
/** @brief 74HC595 RCLK (latch clock) */
#define SR_RCK_PIN             12
/** @brief 74HC595 OE (active LOW) */
#define SR_OE_PIN              13
/** @brief 74HC595 MR (active LOW reset) */
#define SR_MR_PIN               9

/** @brief Total relay emulator channels (two 74HC595) */
#define REL_CH_COUNT           16

/** @brief Discrete RGB LED: Red pin */
#define RGB_R_PIN              6
/** @brief Discrete RGB LED: Green pin */
#define RGB_G_PIN              7
/** @brief Discrete RGB LED: Blue pin */
#define RGB_B_PIN              15

#endif // HARDWARE_REMU_H
