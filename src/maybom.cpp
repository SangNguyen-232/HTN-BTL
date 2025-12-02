#include "maybom.h"
#include "global.h"  
#define MAYBOM_PIN 6 

void task_maybom(void *pvParameters) {
    pinMode(MAYBOM_PIN, OUTPUT);
    digitalWrite(MAYBOM_PIN, LOW);  

    while (1) {
        float current_soil;
        bool current_manual_control;
        bool current_pump_state;
        
        if (xSemaphoreTake(xMutexSensorData, portMAX_DELAY) == pdTRUE) {
            current_soil = glob_soil;
            xSemaphoreGive(xMutexSensorData);
        }
        
        if (xSemaphoreTake(xMutexPumpControl, portMAX_DELAY) == pdTRUE) {
            current_manual_control = pump_manual_control;
            current_pump_state = pump_state;

            
            xSemaphoreGive(xMutexPumpControl);
        }
        
        if (current_manual_control) {
            digitalWrite(MAYBOM_PIN, current_pump_state ? HIGH : LOW);
        } else {
            // AUTO mode: bật khi độ ẩm < min, tắt khi độ ẩm >= max
            bool auto_state;
            if (current_pump_state) {
                // Nếu pump đang bật, chỉ tắt khi đạt ngưỡng max
                auto_state = (current_soil < pump_threshold_max);
            } else {
                // Nếu pump đang tắt, chỉ bật khi dưới ngưỡng min
                auto_state = (current_soil < pump_threshold_min);
            }
            digitalWrite(MAYBOM_PIN, auto_state ? HIGH : LOW);
            if (xSemaphoreTake(xMutexPumpControl, portMAX_DELAY) == pdTRUE) {
                pump_state = auto_state;
                xSemaphoreGive(xMutexPumpControl);
            }
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}