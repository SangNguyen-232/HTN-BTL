#include "global.h"

#include "led_blinky.h"
#include "neo_blinky.h"
#include "temp_humi_monitor.h"
#include "soil_sensor.h"
#include "maybom.h"
#include "mainserver.h"
#include "tinyml.h"
#include "coreiot.h"


void setup() {
  Serial.begin(115200);
  delay(1000);

  xTaskCreate(led_blinky, "Task LED Blink" ,2048  ,NULL  ,2 , NULL);
  xTaskCreate(neo_blinky, "Task NEO Blink" ,2048  ,NULL  ,2 , NULL);
  xTaskCreate(temp_humi_monitor, "Task TEMP HUMI Monitor" ,4096  ,NULL  ,2 , NULL);
  xTaskCreate(task_soil_sensor, "Task Soil Sensor" ,2048  ,NULL  ,2 , NULL);
  xTaskCreate(task_maybom, "Task May Bom" ,2048  ,NULL  ,2 , NULL);
  xTaskCreate(main_server_task, "Task Main Server" ,24576  ,NULL  ,2 , NULL);
  xTaskCreate(tiny_ml_task, "Tiny ML Task" ,2048  ,NULL  ,2 , NULL);
  xTaskCreate(coreiot_task, "CoreIOT Task" ,4096  ,NULL  ,2 , NULL);
}

void loop() {
  
}