#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "../config.h"

class UpdateAPI {
public:
  void begin();
  void loop(float tempC);

private:
  bool sendProcessData(float tempC);
  unsigned long _lastUploadMs = 0;
};
