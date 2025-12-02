#include "neo_blinky.h"

void neo_blinky(void *pvParameters){

    Adafruit_NeoPixel strip(LED_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);
    strip.begin();
    // Set all pixels to off to start
    strip.clear();
    strip.show();

    while(1) {                          
        if (!web_led2_control_enabled) {
            // Chỉ blink khi web không điều khiển LED2
            strip.setPixelColor(0, strip.Color(255, 0, 0)); // Red
            strip.show();
            vTaskDelay(500);
            
            strip.setPixelColor(0, strip.Color(0, 0, 0)); // Off
            strip.show();
            vTaskDelay(500);
        } else {
            // Web đang điều khiển LED2 → không blink
            vTaskDelay(100);
        }
    }
}