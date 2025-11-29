#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "../config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// API 업로드 데이터 구조체
struct APIUploadData {
  int measureValue;
  char departureYn[4];  // "N", "Y", "n", "y" + null
};

class UpdateAPI {
public:
  void begin();
  void queueUpload(int measureValue, const char* departureYn = "N");
  bool isTaskRunning();

private:
  static void uploadTask(void* parameter);
  bool sendData(int measureValue, const char* departureYn);
  
  TaskHandle_t _taskHandle = nullptr;
  QueueHandle_t _uploadQueue = nullptr;
};
