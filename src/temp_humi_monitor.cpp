#include "temp_humi_monitor.h"
#include "global.h"  
#include "lcd.h"
#include "mainserver.h"  // For CoreIOT data structure
DHT20 dht20;

void temp_humi_monitor(void *pvParameters) {
    Wire.begin(11, 12);
    dht20.begin();
    lcd_init();

    lcd.setCursor(0, 0);  
    lcd.print("Tem:--.--C|Soil:");  
    lcd.setCursor(0, 1); 
    lcd.print("Hum:--.--%|00%");  

    while (1) {
        dht20.read();
        float temperature = dht20.getTemperature();
        float humidity = dht20.getHumidity();

        if (isnan(temperature) || isnan(humidity)) {
            Serial.println("Failed to read from DHT sensor!");
            temperature = humidity = -1;
        }

        // Update global sensor data
        if (xSemaphoreTake(xMutexSensorData, portMAX_DELAY) == pdTRUE) {
            glob_temperature = temperature;
            glob_humidity = humidity;
            xSemaphoreGive(xMutexSensorData);
        }
        
        // Get current sensor data (thread-safe)
        SensorData_t sensor_data;
        getSensorData(&sensor_data);
        
        // Get pump state (thread-safe)
        bool current_pump_state;
        bool current_pump_mode;
        if (xSemaphoreTake(xMutexPumpControl, portMAX_DELAY) == pdTRUE) {
            current_pump_state = pump_state;
            current_pump_mode = pump_manual_control;
            xSemaphoreGive(xMutexPumpControl);
        }

        // In AP mode or when CoreIOT data is not available, use local sensor data
        if (isAPMode || !coreiot_data.is_valid) {
            // Display local sensor data
            lcd.setCursor(0, 0);  
            lcd.print("Tem:");
            if (sensor_data.temperature < 0) {
                lcd.print("--.--");
            } else {
                lcd.print(sensor_data.temperature, 2);  
            }
            lcd.print("C|Soil:");  
            
            lcd.setCursor(0, 1); 
            lcd.print("Hum:");
            if (sensor_data.humidity < 0) {
                lcd.print("--.--");
            } else {
                lcd.print(sensor_data.humidity, 2);  
            }
            lcd.print("%|");
            
            int soil_int = int(sensor_data.soil);  
            if (soil_int < 10) {
                lcd.print("0");  
            }
            lcd.print(soil_int);
            lcd.print("%");
        
        } 
        // In STA mode with valid CoreIOT data
        else if (coreiot_data.is_valid) {
            // Display CoreIOT data
            lcd.setCursor(0, 0);  
            lcd.print("Tem:");
            lcd.print(coreiot_data.temperature, 2);  
            lcd.print("C|Soil:");  
            
            lcd.setCursor(0, 1); 
            lcd.print("Hum:");
            lcd.print(coreiot_data.humidity, 2);  
            lcd.print("%|");
            
            int soil_int = int(coreiot_data.soil);  
            if (soil_int < 10) {
                lcd.print("0");  
            }
            lcd.print(soil_int);
            lcd.print("%");
            
        }
        
        vTaskDelay(3000 / portTICK_PERIOD_MS); 
    }
}