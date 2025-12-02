#ifndef ___MAIN_SERVER__
#define ___MAIN_SERVER__
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "global.h"

#define LED1_PIN 48
#define LED2_PIN 45
#define BOOT_PIN 0
//extern WebServer server;

//extern bool isAPMode;

// CoreIOT data structure
struct CoreIOTData {
  float temperature = 0.0;
  float humidity = 0.0;
  float soil = 0.0;
  float anomaly_score = 0.0;
  String anomaly_message = "Normal";
  bool pump_state = false;
  String pump_mode = "AUTO";
  bool led1_state = false;
  bool led2_state = false;
  unsigned long last_update = 0;
  bool is_valid = false;
};

// External references for CoreIOT data (defined in mainserver.cpp)
extern CoreIOTData coreiot_data;
extern bool use_coreiot_data;

String mainPage();
String settingsPage();

void startAP();
void setupServer();
void connectToWiFi();

void main_server_task(void *pvParameters);

#endif