#include "temp_humi_monitor.h"
#include "global.h"  
#include "lcd.h"
#include "mainserver.h"  // For CoreIOT data structure
DHT20 dht20;

void temp_humi_monitor(void *pvParameters){
    Wire.begin(11, 12);
    dht20.begin();
    lcd_init();

    lcd.setCursor(0, 0);  
    lcd.print("Tem:--.--C|Soil:");  
    lcd.setCursor(0, 1); 
    lcd.print("Hum:--.--%|00%");  

    while (1){
        dht20.read();
        float temperature = dht20.getTemperature();
        float humidity = dht20.getHumidity();

        if (isnan(temperature) || isnan(humidity)) {
            Serial.println("Failed to read from DHT sensor!");
            temperature = humidity = -1;
        }
        if (xSemaphoreTake(xMutexSensorData, portMAX_DELAY) == pdTRUE) {
            glob_temperature = temperature;
            glob_humidity = humidity;
            xSemaphoreGive(xMutexSensorData);
        }
        
        // THEO CHU TRÌNH MỚI: Chỉ lấy dữ liệu từ CoreIOT để hiển thị
        // Flow: Sensors → CoreIOT → LCD hiển thị từ CoreIOT
        // Khi đợi dữ liệu mới từ CoreIOT, vẫn hiển thị giá trị hiện tại (last known values)
        float temp_display, hum_display, soil_display;
        
        // Kiểm tra dữ liệu từ CoreIOT
        if (coreiot_data.is_valid) {
            // Luôn hiển thị giá trị từ CoreIOT (kể cả khi đợi cập nhật mới)
            temp_display = coreiot_data.temperature;
            hum_display = coreiot_data.humidity;
            soil_display = coreiot_data.soil;
        } else {
            // Chưa có dữ liệu từ CoreIOT lần đầu, hiển thị "--"
            temp_display = -1.0;
            hum_display = -1.0;
            soil_display = 0.0;
        }
        
        // Display on LCD
        lcd.setCursor(0, 0);  
        lcd.print("Tem:");
        if (temp_display < 0) {
            lcd.print("--.--");
        } else {
            lcd.print(temp_display, 2);  
        }
        lcd.print("C|Soil:");  
        lcd.setCursor(0, 1); 
        lcd.print("Hum:");
        if (hum_display < 0) {
            lcd.print("--.--");
        } else {
            lcd.print(hum_display, 2);  
        }
        lcd.print("%|");
        int soil_int = int(soil_display);  
        if (soil_int < 10) {
            lcd.print("0");  
        }
        lcd.print(soil_int);
        lcd.print("%");  
        vTaskDelay(3000 / portTICK_PERIOD_MS); 
    }
}