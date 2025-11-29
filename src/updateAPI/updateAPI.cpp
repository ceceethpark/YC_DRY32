#include "updateAPI.h"

void UpdateAPI::setConfig(const APIConfig& config) {
  process_data_input = config;
  Serial.println("[UpdateAPI] Config updated");
}

// JSON에서 record_id 추출 (메모리 효율적인 버전)
bool UpdateAPI::parseRecordId(const String& response) {
  // "record_id":1057 형식에서 숫자만 추출
  const char* ptr = strstr(response.c_str(), "\"record_id\":");
  if (!ptr) return false;
  
  ptr += 12; // "record_id": 길이 (12자)
  while (*ptr && (*ptr == ' ' || *ptr == '\t' || *ptr == ':')) ptr++; // 공백과 콜론 스킵
  
  // 숫자 복사
  int i = 0;
  while (*ptr && isdigit(*ptr) && i < 15) {
    _recordId[i++] = *ptr++;
  }
  _recordId[i] = '\0';
  
  Serial.printf("[ParseRecordId] Extracted: '%s' from response\n", _recordId);
  
  return (i > 0);
}

// 공정 시작 API 호출 (record_id 받아오기)
bool UpdateAPI::processStart() {
#if ENABLE_API_UPLOAD
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[ProcessStart] WiFi not connected");
    return false;
  }

  WiFiClient client;
  HTTPClient http;
  
  if (!http.begin(client, API_START_URL)) {
    Serial.println("[ProcessStart] HTTP begin failed");
    return false;
  }

  // 헤더 설정
  http.addHeader(process_data_input.securityKey, process_data_input.securityValue);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  // Body 데이터 생성
  String postData = "product_id=" + String(process_data_input.productId);
  postData += "&partner_id=" + String(process_data_input.partnerId);
  postData += "&machine_id=" + String(process_data_input.machineId);

  Serial.printf("[ProcessStart] POST to: %s\n", API_START_URL);
  Serial.printf("[ProcessStart] Body: %s\n", postData.c_str());

  int httpResponseCode = http.POST(postData);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.printf("[ProcessStart] HTTP %d\n", httpResponseCode);
    
    if (httpResponseCode == 200) {
      // record_id 추출
      if (parseRecordId(response)) {
        process_data_input.recordId = _recordId;
        Serial.printf("[ProcessStart] Received record_id: %s\n", _recordId);
        http.end();
        return true;
      } else {
        Serial.println("[ProcessStart] Failed to parse record_id");
      }
    }
    http.end();
  } else {
    Serial.printf("[ProcessStart] POST failed: code=%d\n", httpResponseCode);
    http.end();
  }
  
  return false;
#else
  return false;
#endif
}

// 공정 시작 데이터 준비
APIConfig UpdateAPI::start_process_data_input() {
  process_data_input.machine_status = 1;  // 시작 상태
  process_data_input.elapsed_minutes = 0;
  process_data_input.departureYn = (char*)"N";
  Serial.println("[UpdateAPI] Process START data prepared");
  return process_data_input;
}

// 공정 데이터 갱신 (진행 중)
APIConfig UpdateAPI::update_process_data_input() {
  process_data_input.machine_status = 2;  // 진행 중 상태
  process_data_input.departureYn = (char*)"N";
  process_data_input.endpointUrl = (char*)API_DATA_URL;  // processData URL 사용
  // measure_temperature는 uploadTask()에서 이미 process_data_input에 저장되어 있음
  Serial.printf("[UpdateAPI] Process DATA: temp=%.1f°C, record_id=%s\n", 
                process_data_input.measure_temperature, process_data_input.recordId);
  return process_data_input;
}

// 공정 종료 데이터 준비
APIConfig UpdateAPI::finish_process_data_input() {
  process_data_input.machine_status = 3;  // 종료 상태
  process_data_input.departureYn = (char*)"Y";  // 출고 완료
  Serial.println("[UpdateAPI] Process FINISH data prepared");
  return process_data_input;
}

void UpdateAPI::begin() {
#if ENABLE_API_UPLOAD
  // process_data_input 초기화 (config.h 기본값)
  process_data_input.endpointUrl = (char*)API_ENDPOINT_URL;
  process_data_input.securityKey = (char*)API_HEADER_SECURITY_KEY;
  process_data_input.securityValue = (char*)API_HEADER_SECURITY_VALUE;
  process_data_input.productId = (char*)API_PRODUCT_ID;
  process_data_input.partnerId = (char*)API_PARTNER_ID;
  process_data_input.machineId = (char*)API_MACHINE_ID;
  process_data_input.recordId = (char*)API_RECORD_ID;
  process_data_input.departureYn = (char*)API_DEPARTURE_YN;
  process_data_input.measure_temperature = 0.0f;
  process_data_input.measure_humidity = 0.0f;
  process_data_input.remain_minutes = 0;
  process_data_input.elapsed_minutes = 0;
  process_data_input.machine_status = 0;
  Serial.println("[UpdateAPI] Config initialized from config.h");

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
      
      // 온도 데이터 업데이트
      api->process_data_input.measure_temperature = (float)data.measureValue;
      
      // 공정 데이터 갱신으로 전송 (PROCESS_DATA)
      bool success = api->sendData(PROCESS_DATA);
      
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

bool UpdateAPI::sendData(uint8_t processType) {
#if ENABLE_API_UPLOAD
  // processType에 따라 데이터 준비
  APIConfig config;
  switch (processType) {
    case PROCESS_START:
      config = start_process_data_input();
      break;
    case PROCESS_DATA:
      config = update_process_data_input();
      break;
    case PROCESS_FINISH:
      config = finish_process_data_input();
      break;
    default:
      Serial.println("[APITask] Invalid process type");
      return false;
  }

  WiFiClient client;
  HTTPClient http;
  
  if (!http.begin(client, config.endpointUrl)) {
    Serial.println("[APITask] HTTP begin failed");
    return false;
  }

  // 헤더 설정 (Postman 스크린샷과 동일)
  http.addHeader(config.securityKey, config.securityValue);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  // Body 데이터 생성 (Postman과 동일하게)
  String postData = "product_id=" + String(config.productId);
  postData += "&partner_id=" + String(config.partnerId);
  postData += "&machine_id=" + String(config.machineId);
  postData += "&record_id=" + String(config.recordId);
  postData += "&measure_value=" + String((int)config.measure_temperature);  // measure_value로 변경, 정수로 전송
  postData += "&departure_yn=" + String(config.departureYn);

  Serial.printf("[APITask] POST to: %s\n", config.endpointUrl);
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
  (void)processType;
  return false;
#endif
}
