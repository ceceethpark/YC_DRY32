#include "updateAPI.h"

void UpdateAPI::begin() {
#if ENABLE_API_UPLOAD
  // FreeRTOS Queue 생성 (최대 10개 항목 저장)
  _uploadQueue = xQueueCreate(10, sizeof(APIUploadData));
  if (_uploadQueue == nullptr) {
    Serial.println("[UpdateAPI] Failed to create queue");
    return;
  }

  // FreeRTOS Task 생성 (Priority 1, Core 1에서 실행 - WiFi와 같은 코어)
  BaseType_t ret = xTaskCreatePinnedToCore(
    uploadTask,         // Task 함수
    "APIUploadTask",    // Task 이름
    16384,              // Stack 크기 (16KB - HTTPClient 메모리 확보)
    this,               // Parameter (this 포인터)
    1,                  // Priority (1 = 낮음)
    &_taskHandle,       // Task Handle
    1                   // Core 1 (WiFi/Network와 같은 코어에서 실행)
  );

  if (ret != pdPASS) {
    Serial.println("[UpdateAPI] Failed to create task");
    _taskHandle = nullptr;
  } else {
    Serial.println("[UpdateAPI] Task created successfully");
  }
#endif
}

void UpdateAPI::queueUpload(int measureValue, const char* departureYn) {
#if ENABLE_API_UPLOAD
  if (_uploadQueue == nullptr) {
    Serial.println("[UpdateAPI] Queue not initialized");
    return;
  }

  APIUploadData data;
  data.measureValue = measureValue;
  strncpy(data.departureYn, departureYn, sizeof(data.departureYn) - 1);
  data.departureYn[sizeof(data.departureYn) - 1] = '\0';

  // Queue에 데이터 추가 (대기 없음)
  if (xQueueSend(_uploadQueue, &data, 0) != pdTRUE) {
    Serial.println("[UpdateAPI] Queue full, upload skipped");
  } else {
    Serial.printf("[UpdateAPI] Queued: value=%d, departure=%s\n", measureValue, departureYn);
  }
#else
  (void)measureValue;
  (void)departureYn;
  Serial.println("[UpdateAPI] Upload disabled (ENABLE_API_UPLOAD=0)");
#endif
}

bool UpdateAPI::isTaskRunning() {
#if ENABLE_API_UPLOAD
  return (_taskHandle != nullptr);
#else
  return false;
#endif
}

void UpdateAPI::uploadTask(void* parameter) {
#if ENABLE_API_UPLOAD
  UpdateAPI* api = static_cast<UpdateAPI*>(parameter);
  APIUploadData data;

  Serial.printf("[APITask] Started on Core %d\n", xPortGetCoreID());

  while (true) {
    // Queue에서 데이터 수신 (최대 1초 대기)
    if (xQueueReceive(api->_uploadQueue, &data, pdMS_TO_TICKS(1000)) == pdTRUE) {
      // WiFi 연결 확인
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[APITask] Skip upload: WiFi disconnected");
        vTaskDelay(pdMS_TO_TICKS(5000)); // 5초 대기 후 재시도
        continue;
      }

      // HTTP POST 전송
      Serial.printf("[APITask] Uploading: value=%d, departure=%s\n", 
                    data.measureValue, data.departureYn);
      
      bool success = api->sendData(data.measureValue, data.departureYn);
      
      if (success) {
        Serial.println("[APITask] Upload successful");
      } else {
        Serial.println("[APITask] Upload failed");
      }

      // 전송 후 딜레이 (서버 부하 방지) - 메모리 정리 시간 확보
      vTaskDelay(pdMS_TO_TICKS(2000));
    }
    
    // CPU 양보 - 메모리 안정화
    vTaskDelay(pdMS_TO_TICKS(500));
  }
#endif
}

bool UpdateAPI::sendData(int measureValue, const char* departureYn) {
#if ENABLE_API_UPLOAD
  WiFiClient client;
  HTTPClient http;
  
  if (!http.begin(client, API_ENDPOINT_URL)) {
    Serial.println("[APITask] HTTP begin failed");
    return false;
  }

  // 헤더 설정 (Postman 스크린샷과 동일)
  http.addHeader(API_HEADER_SECURITY_KEY, API_HEADER_SECURITY_VALUE);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  // Body 데이터 생성 (form-data)
  String postData = "product_id=" + String(API_PRODUCT_ID);
  postData += "&partner_id=" + String(API_PARTNER_ID);
  postData += "&machine_id=" + String(API_MACHINE_ID);
  postData += "&record_id=" + String(API_RECORD_ID);
  postData += "&measure_value=" + String(measureValue);
  postData += "&departure_yn=" + String(departureYn);

  Serial.printf("[APITask] POST to: %s\n", API_ENDPOINT_URL);
  Serial.printf("[APITask] Body: %s\n", postData.c_str());

  int httpResponseCode = http.POST(postData);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.printf("[APITask] HTTP %d: %s\n", httpResponseCode, response.c_str());
    http.end();
    return (httpResponseCode >= 200 && httpResponseCode < 300);
  } else {
    Serial.printf("[APITask] POST failed: %s\n", http.errorToString(httpResponseCode).c_str());
    http.end();
    return false;
  }
#else
  (void)measureValue;
  (void)departureYn;
  return false;
#endif
}
