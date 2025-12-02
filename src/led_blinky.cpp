#include "led_blinky.h"

void led_blinky(void *pvParameters){
    pinMode(LED_GPIO, OUTPUT);
  
  while(1) {                        
     // **CHECK WEB CONTROL FLAG**
        if (!web_led1_control_enabled) {
            // Chỉ blink khi web không điều khiển
            digitalWrite(LED_GPIO, HIGH);
            vTaskDelay(1000);
            digitalWrite(LED_GPIO, LOW);
            vTaskDelay(1000);
        } else {
            // Web đang điều khiển → không blink
            vTaskDelay(100);
        }
  }
}