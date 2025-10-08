#include "Utils.h"


// RTOS task that blinks the pin and then deletes itself
void BlinkTask(void* parameter) {
    DEBUG_PRINTLN("LED Blinking 💡");
    BlinkParams* params = static_cast<BlinkParams*>(parameter);

    digitalWrite(params->pin, HIGH);
    vTaskDelay(pdMS_TO_TICKS(params->durationMs));
    digitalWrite(params->pin, LOW);

    delete params;           // Free dynamically allocated memory
    vTaskDelete(NULL);       // Self-terminate task
}

// Creates the RTOS blink task
void BlinkStatusLED(uint8_t pin, int durationMs) {
    pinMode(pin, OUTPUT);   // Ensure pin is in OUTPUT mode

    auto* params = new BlinkParams{pin, durationMs};

    xTaskCreate(
        BlinkTask,          // Task function
        "BlinkTask",        // Task name
        2048,               // Stack size
        params,             // Task parameter
        1,                  // Priority
        nullptr            // Task handle
    );
}
