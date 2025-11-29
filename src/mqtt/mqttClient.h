// mqttClient.h - AWS IoT MQTT 클라이언트

#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "../config.h"

class MQTTClient {
public:
  void begin();
  void loop();
    bool publishData(float temperature, int elapsedMinutes);
    bool publishEvent(uint16_t xor_uEvent, uint16_t uEvent);
    bool isConnected();private:
  WiFiClientSecure _wifiClient;
  PubSubClient _mqttClient;
  unsigned long _lastReconnectAttempt = 0;
  unsigned long _lastPublishTime = 0;
  
  bool connect();
  void reconnect();
};

extern MQTTClient gMQTTClient;
