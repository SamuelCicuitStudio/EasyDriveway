/**************************************************************
 *  Project     : ICM (Interface Control Module)
 *  File        : RGBConfig.h
 *  Purpose     : Centralized configuration for the RGB status LED.
 *                - Defines NVS keys (read via ConfigManager)
 *                - Provides sane compile-time defaults
 *                - Groups PWM/LEDC and RTOS task settings
 **************************************************************/

#ifndef RGB_CONFIG_H
#define RGB_CONFIG_H

// ---------------- Colors (unchanged) ----------------
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

// ---------------- RTOS / PWM config ----------------
#ifndef RGB_TASK_CORE
#define RGB_TASK_CORE        1
#endif

#ifndef RGB_TASK_PRIORITY
#define RGB_TASK_PRIORITY    1
#endif

#ifndef RGB_TASK_STACK
#define RGB_TASK_STACK       2048
#endif

#if defined(ESP32)
  #ifndef RGB_LEDC_FREQ_HZ
  #define RGB_LEDC_FREQ_HZ   1000
  #endif
  #ifndef RGB_LEDC_RES_BITS
  #define RGB_LEDC_RES_BITS  8
  #endif
  // 3 dedicated LEDC channels for R, G, B
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

#endif // RGB_CONFIG_H
