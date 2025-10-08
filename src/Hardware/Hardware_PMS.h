/**************************************************************
 *  Project     : EasyDriveway
 *  File        : Hardware_PMS.h
 *  Purpose     : Hardware pin assignment mapping (no optionals).
 *  Author      : Tshibangu Samuel
 *  Role        : Freelance Embedded Systems Engineer
 *  Expertise   : Secure IoT Systems, Embedded C++, RTOS, Control Logic
 *  Contact     : tshibsamuel47@gmail.com
 *  Phone       : +216 54 429 793
 *  Created     : 2025-10-05
 *  Version     : 1.0.0
 **************************************************************/
#ifndef HARDWARE_PMS_H
#define HARDWARE_PMS_H

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

/** @brief DS18B20 (board temperature) OneWire pin */
#define ONEWIRE_DS18B20_PIN    26

/** @brief Wall current sensor (ACS781) ADC pin */
#define I_WALL_ADC_PIN          5
/** @brief Battery current sensor (ACS781) ADC pin */
#define I_BATT_ADC_PIN          6
/** @brief Wall voltage divider ADC pin */
#define V_WALL_ADC_PIN          7
/** @brief Battery voltage divider ADC pin */
#define V_BATT_ADC_PIN          8

/** @brief Source relay: wall input control pin */
#define REL_SRC_WALL_PIN       21
/** @brief Source relay: battery input control pin */
#define REL_SRC_BATT_PIN       22
/** @brief Sensor rail enable pin */
#define RAIL_SENSOR_EN_PIN     23
/** @brief Relay rail enable pin */
#define RAIL_RELAY_EN_PIN      24
/** @brief Emulator rail enable pin */
#define RAIL_EMUL_EN_PIN       25

/** @brief Service I2C SCL */
#define I2C_SYS_SCL_PIN        33
/** @brief Service I2C SDA */
#define I2C_SYS_SDA_PIN        34

/** @brief Discrete RGB LED: Red pin */
#define RGB_R_PIN              6
/** @brief Discrete RGB LED: Green pin */
#define RGB_G_PIN              7
/** @brief Discrete RGB LED: Blue pin */
#define RGB_B_PIN              15

#endif // HARDWARE_PMS_H
