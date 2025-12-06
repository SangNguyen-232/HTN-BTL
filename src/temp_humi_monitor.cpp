#include "temp_humi_monitor.h"
#include "global.h"  
#include "lcd.h"
#include "mainserver.h" 
DHT20 dht20;

void temp_humi_monitor(void *pvParameters) {
    Wire.begin(11, 12);
    dht20.begin();
    lcd_init();

    lcd.setCursor(0, 0);  
    lcd.print("Tem:--.--C|Soil:");  
    lcd.setCursor(0, 1); 
    lcd.print("Hum:--.--%|00%");  
    
    // Load LCD refresh rate from preferences
    loadLCDRefreshRate();
    Serial.println("[LCD] Using refresh rate: " + String(lcd_refresh_rate) + "s");

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

        // Always display local sensor data (since adafruit_data is not being updated)
        // Clear previous values by printing spaces
        lcd.clear();
        
        // Line 1: Temperature and Soil
        lcd.setCursor(0, 0);  
        lcd.print("Tem:");
        if (sensor_data.temperature < 0) {
            lcd.print("--.--");
        } else {
            char tempStr[6];
            dtostrf(sensor_data.temperature, 5, 2, tempStr);  // Format as "XX.XX"
            lcd.print(tempStr);
        }
        lcd.print("C|Soil:");  
        
        // Line 2: Humidity and Soil percentage
        lcd.setCursor(0, 1); 
        lcd.print("Hum:");
        if (sensor_data.humidity < 0) {
            lcd.print("--.--");
        } else {
            char humStr[6];
            dtostrf(sensor_data.humidity, 5, 2, humStr);  // Format as "XX.XX"
            lcd.print(humStr);
        }
        lcd.print("%|");
        
        int soil_int = int(sensor_data.soil);  
        char soilStr[4];
        sprintf(soilStr, "%02d%%", soil_int);  // Format as "XX%"
        lcd.print(soilStr);
        
        
        // Sử dụng LCD refresh rate thay vì hard-code 3 giây
        // Kiểm tra và sử dụng giá trị refresh rate hiện tại (có thể đã được cập nhật)
        int current_refresh_rate = lcd_refresh_rate; // Atomic read
        vTaskDelay((current_refresh_rate * 1000) / portTICK_PERIOD_MS); 
    }
}