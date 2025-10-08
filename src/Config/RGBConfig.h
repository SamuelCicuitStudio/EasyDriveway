/**************************************************************
 *  Project     : EasyDriveway
 *  File        : RGBConfig.h
 *  Purpose     : Centralized configuration for the RGB status LED.
 *  Author      : Tshibangu Samuel
 *  Role        : Freelance Embedded Systems Engineer
 *  Expertise   : Secure IoT Systems, Embedded C++, RTOS, Control Logic
 *  Contact     : tshibsamuel47@gmail.com
 *  Portfolio   : https://www.freelancer.com/u/tshibsamuel477
 *  Phone       : +216 54 429 793
 *  Created     : 2025-10-05
 *  Version     : 1.0.0
 **************************************************************/
#ifndef RGB_CONFIG_H
#define RGB_CONFIG_H

/***********************
 * INCLUDES
 ***********************/
// (none)

/**
 * @name Predefined RGB Colors
 * @brief 24-bit RGB constants for convenience (0xRRGGBB).
 * @details Values preserved exactly as in the original source.
 * @{ */
#define RGB_RED     0xFF0000
#define RGB_GREEN   0x00FF00
#define RGB_BLUE    0x0000FF
#define RGB_YELLOW  0xFFFF00
#define RGB_CYAN    0x00FFFF
#define RGB_MAGENTA 0xFF00FF
#define RGB_ORANGE  0xFFA500
#define RGB_PURPLE  0x800080
#define RGB_PINK    0xFFC0CB
#define RGB_WHITE   0xFFFFFF
#define RGB_GRAY    0x808080
#define RGB_BROWN   0xA52A2A
/** @} */

/**
 * @name RTOS / Task Settings
 * @brief Core, priority, and stack for the RGB worker task.
 * @details Defaults applied only if not already defined.
 * @{ */
#ifndef RGB_TASK_CORE
#define RGB_TASK_CORE        1
#endif
#ifndef RGB_TASK_PRIORITY
#define RGB_TASK_PRIORITY    1
#endif
#ifndef RGB_TASK_STACK
#define RGB_TASK_STACK       2048
#endif
/** @} */

/**
 * @name ESP32 LEDC (PWM) Settings
 * @brief PWM frequency, resolution, and per-channel indices for R/G/B.
 * @details Active only on ESP32 targets. Values unchanged.
 * @{ */
#if defined(ESP32)
  #ifndef RGB_LEDC_FREQ_HZ
  #define RGB_LEDC_FREQ_HZ   1000
  #endif
  #ifndef RGB_LEDC_RES_BITS
  #define RGB_LEDC_RES_BITS  8
  #endif
  #ifndef RGB_LEDC_CH_R
  #define RGB_LEDC_CH_R      3
  #endif
  #ifndef RGB_LEDC_CH_G
  #define RGB_LEDC_CH_G      4
  #endif
  #ifndef RGB_LEDC_CH_B
  #define RGB_LEDC_CH_B      5
  #endif
#endif
/** @} */

#endif // RGB_CONFIG_H
