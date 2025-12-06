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

extern bool isAPMode;

// Adafruit IO data structure
struct AdafruitData {
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

// External references for Adafruit IO data (defined in mainserver.cpp)
extern AdafruitData adafruit_data;
extern bool use_adafruit_data;

String mainPage();
String settingsPage();

void startAP();
void setupServer();
void connectToWiFi();

void main_server_task(void *pvParameters);

#endif