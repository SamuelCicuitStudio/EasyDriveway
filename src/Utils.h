/**************************************************************
 *  Author      : Tshibangu Samuel
 *  Role        : Freelance Embedded Systems Engineer
 *  Expertise   : Secure IoT Systems, Embedded C++, RTOS, Control Logic
 *  Contact     : tshibsamuel47@gmail.com
 *  Portfolio   : https://www.freelancer.com/u/tshibsamuel477
 *  Phone       : +216 54 429 793
 **************************************************************/

#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>
 #define DEBUGMODE true
 #define ENABLE_SERIAL_DEBUG

 #ifdef ENABLE_SERIAL_DEBUG
 #define DEBUG_PRINT(x) if(DEBUGMODE)Serial.print(x)
 #define  DEBUG_PRINTF Serial.printf
 #define DEBUG_PRINTLN(x) if(DEBUGMODE)Serial.println(x)
 #else
 #define DEBUG_PRINT(x)
 #define DEBUG_PRINTLN(x)
 #endif

// Struct used to pass blink parameters to RTOS task
struct BlinkParams {
    uint8_t pin;
    int durationMs;
};

// Public function to blink a pin using a self-deleting RTOS task
void BlinkStatusLED(uint8_t pin, int durationMs = 100);

#endif // UTILS_H
