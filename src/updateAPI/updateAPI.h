#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "../config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// API 설정 구조체
typedef struct _APIConfig 
{
   char* endpointUrl;
   char* securityKey;
   char* securityValue;
   char* productId;
   char* partnerId;
   char* machineId;
   char* recordId;
   char* departureYn;
   float measure_temperature;
   float measure_humidity;
   int remain_minutes;
   int elapsed_minutes;
   int machine_status;
}
APIConfig;

// 공정 타입 정의
enum ProcessType {
  PROCESS_START = 0,    // 공정 시작
  PROCESS_DATA = 1,     // 공정 데이터 입력
  PROCESS_FINISH = 2    // 공정 종료
};

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
  void setConfig(const APIConfig& config);  // API 설정 변경
  bool processStart();                       // 공정 시작 (record_id 받아오기)
  
  // 공정 타입별 데이터 준비 함수
  APIConfig start_process_data_input();      // 공정 시작
  APIConfig update_process_data_input();     // 공정 데이터 갱신
  APIConfig finish_process_data_input();     // 공정 종료

private:
  static void uploadTask(void* parameter);
  bool sendData(uint8_t processType);        // processType에 따라 전송
  bool parseRecordId(const String& response);  // JSON에서 record_id 추출
  
  TaskHandle_t _taskHandle = nullptr;
  QueueHandle_t _uploadQueue = nullptr;
  APIConfig process_data_input;
  char _recordId[16] = {0};                  // 서버에서 받은 record_id (최대 15자)
  // APIConfig _config = {  // 기본 설정 (config.h 값)
  //   API_ENDPOINT_URL,
  //   API_HEADER_SECURITY_KEY,
  //   API_HEADER_SECURITY_VALUE,
  //   API_PRODUCT_ID,
  //   API_PARTNER_ID,
  //   API_MACHINE_ID,
  //   API_RECORD_ID,
  //   API_DEPARTURE_YN
  // };
};
