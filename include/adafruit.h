#ifndef __ADAFRUIT_H__
#define __ADAFRUIT_H__

#include <Arduino.h>
#include <WiFi.h>
#include "global.h"
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Adafruit IO configuration
// MQTT server: io.adafruit.com
// MQTT port: 1883 (non-SSL) or 8883 (SSL)
// Username: Your Adafruit IO username
// Key: Your Adafruit IO key (used as password)

// Feed names for Adafruit IO
#define ADAFRUIT_FEED_TEMPERATURE "temperature"
#define ADAFRUIT_FEED_HUMIDITY "humidity"
#define ADAFRUIT_FEED_SOIL "soil-moisture"
#define ADAFRUIT_FEED_ANOMALY_SCORE "anomaly-score"
#define ADAFRUIT_FEED_ANOMALY_MESSAGE "anomaly-message"
#define ADAFRUIT_FEED_PUMP_STATE "pump-state"
#define ADAFRUIT_FEED_PUMP_MODE "pump-mode"

// Adafruit IO topic format: username/feeds/feedname
// Subscribe: username/feeds/feedname
// Publish: username/feeds/feedname

void adafruit_task(void *pvParameters);

#endif
