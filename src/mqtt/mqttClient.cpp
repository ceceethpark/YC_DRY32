// mqttClient.cpp - AWS IoT MQTT 클라이언트 구현

#include "mqttClient.h"
#include "aws_certs.h"
#include "../typedef.h"
#include "../dataClass/dataClass.h"
#include "../TM1638Display/TM1638Display.h"

MQTTClient gMQTTClient;

extern CURRENT_DATA gCUR;
extern dataClass gData;
extern TM1638Display gDisplay;

// 문자열 파싱 헬퍼 함수
void parseCommand(const char* cmd, const char* data) {
  int iData = atoi(data);
  
  if (strcmp(cmd, "PWR") == 0) {
    // 전원 ON/OFF - 소프트 오프 플래그 설정
    Serial.printf("[MQTT] Power command: %d\n", iData);
    if (iData == 0) {
      gCUR.flg.soft_off = 1;  // 전원 OFF
    } else {
      gCUR.flg.soft_off = 0;  // 전원 ON
    }
  }
  else if (strcmp(cmd, "TMP") == 0) {
    // 설정 온도 //서버에서 보내는값 x10이 되어 건조기에서는  온도값으로 사용 (예 6.5도 -> 65)
    Serial.printf("[MQTT] Temperature set: %d\n", iData);
    if (iData >= 0 && iData <= 255) {
      //Serial.printf("[MQTT] Before: gCUR.seljung_temp = %d\n", gCUR.seljung_temp);
      gDisplay.beep();  
      gCUR.seljung_temp = iData;
      //Serial.printf("[MQTT] After: gCUR.seljung_temp = %d\n", gCUR.seljung_temp);
      gData.saveToFlash();  // Flash에 저장
      Serial.println("[MQTT] Settings saved to flash");
    }
  }
  else if (strcmp(cmd, "JSP") == 0) {
    // 제상 주기 (분)를 건조기에서는 시간설정으로
    Serial.printf("[MQTT] Timer set: %d min\n", iData);
    if (iData >= 0 && iData <= 1440) {  // 0~24시간
      gDisplay.beep();  
      gCUR.current_minute = iData;
      gData.saveToFlash();  // Flash에 저장
      Serial.println("[MQTT] Settings saved to flash");
    }
  }
  else if (strcmp(cmd, "JSD") == 0) {
    // 제상 시간 (분)
    Serial.printf("[MQTT] Defrost duration: %d min\n", iData);
    if (iData >= 0 && iData <= 60) {
      // 제상 시간 설정 로직 추가 필요
    }
  }
  else if (strcmp(cmd, "JSF") == 0) {
    // 강제 제상
    Serial.printf("[MQTT] Force defrost: %d\n", iData);
    // 강제 제상 로직 추가 필요
  }
  else if (strcmp(cmd, "RES") == 0) {
    // 시스템 리셋
    if (iData) {
      Serial.println("[MQTT] System reset requested");
      delay(100);
      ESP.restart();
    }
  }
  else {
    Serial.printf("[MQTT] Unknown command: %s = %s\n", cmd, data);
  }
}

// MQTT 메시지 수신 콜백
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.printf("[MQTT] Message arrived [%s]: ", topic);
  
  // payload를 null-terminated 문자열로 변환
  char message[256];
  if (length >= sizeof(message)) {
    length = sizeof(message) - 1;
  }
  memcpy(message, payload, length);
  message[length] = '\0';
  
  Serial.println(message);
  
  // JSON 형식 파싱: {"PWR@":1, "TMP@":50, ...}
  // 간단한 파싱 (,로 분리하여 처리)
  char* token = strtok(message, ",");
  
  while (token != NULL) {
    // "CMD@":value 형식 파싱
    char* cmdStart = strchr(token, '"');
    if (cmdStart != NULL) {
      cmdStart++;  // " 다음 문자
      char* cmdEnd = strchr(cmdStart, '@');
      if (cmdEnd != NULL) {
        // 명령어 추출
        int cmdLen = cmdEnd - cmdStart;
        char cmd[8];
        if (cmdLen < sizeof(cmd)) {
          strncpy(cmd, cmdStart, cmdLen);
          cmd[cmdLen] = '\0';
          
          // 값 추출
          char* valueStart = strchr(cmdEnd, ':');
          if (valueStart != NULL) {
            valueStart++;
            // 공백 제거
            while (*valueStart == ' ') valueStart++;
            
            // 숫자 부분만 추출
            char value[16];
            int i = 0;
            while (valueStart[i] && (isdigit(valueStart[i]) || valueStart[i] == '-') && i < 15) {
              value[i] = valueStart[i];
              i++;
            }
            value[i] = '\0';
            
            if (i > 0) {
              parseCommand(cmd, value);
            }
          }
        }
      }
    }
    
    token = strtok(NULL, ",");
  }
}

// AWS IoT 설정
#define AWS_IOT_ENDPOINT "a14xbcv33glzp0-ats.iot.ap-northeast-2.amazonaws.com"
#define AWS_IOT_PORT 8883
#define AWS_IOT_CLIENT_ID "ESP32_Dryer_1008"
#define AWS_IOT_PUBLISH_TOPIC "skb"  // 송신 토픽
// 수신 토픽: "Hskb/{CHIP_ID}" - connect()에서 동적 생성

void MQTTClient::begin() {
#if ENABLE_API_UPLOAD
  Serial.println("[MQTT] Initializing...");
  
  // TLS 인증서 설정
  _wifiClient.setCACert(AWS_CERT_CA);
  _wifiClient.setCertificate(AWS_CERT_CRT);
  _wifiClient.setPrivateKey(AWS_CERT_PRIVATE);
  
  // MQTT 클라이언트 설정
  _mqttClient.setClient(_wifiClient);
  _mqttClient.setServer(AWS_IOT_ENDPOINT, AWS_IOT_PORT);
  _mqttClient.setCallback(mqttCallback);  // 메시지 수신 콜백 설정
  _mqttClient.setKeepAlive(60);
  _mqttClient.setBufferSize(512);  // 메모리 절약: 512 bytes
  
  Serial.println("[MQTT] Configuration complete");
  
  // 초기 연결 시도
  if (WiFi.status() == WL_CONNECTED) {
    connect();
  }
#else
  Serial.println("[MQTT] Disabled (ENABLE_API_UPLOAD=0)");
#endif
}

bool MQTTClient::connect() {
  Serial.printf("[MQTT] Connecting to %s:%d...\n", AWS_IOT_ENDPOINT, AWS_IOT_PORT);
  
  // Client ID에 ChipID 추가 (고유성 보장)
  char clientId[64];
  uint64_t chipid = ESP.getEfuseMac();
  snprintf(clientId, sizeof(clientId), "%s_%04X%08X", 
           AWS_IOT_CLIENT_ID, 
           (uint16_t)(chipid >> 32), 
           (uint32_t)chipid);
  
  if (_mqttClient.connect(clientId)) {
    Serial.printf("[MQTT] Connected! Client ID: %s\n", clientId);
    
    // 개별 수신 토픽 구독: Hskb/{24자리 CPUID}
    uint8_t mac[6];
    for (int i = 0; i < 6; i++) {
      mac[i] = (chipid >> (i * 8)) & 0xFF;
    }
    
    char cpuid[32];
    snprintf(cpuid, sizeof(cpuid), "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
      mac[5], mac[4], mac[3], mac[2], mac[1], mac[0],
      mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
    
    char subscribeTopic[64];
    snprintf(subscribeTopic, sizeof(subscribeTopic), "Hskb/%s", cpuid);
    
    if (_mqttClient.subscribe(subscribeTopic)) {
      Serial.printf("[MQTT] Subscribed to: %s\n", subscribeTopic);
    } else {
      Serial.println("[MQTT] Subscribe failed");
    }
    
    return true;
  } else {
    int state = _mqttClient.state();
    Serial.printf("[MQTT] Connection failed, rc=%d\n", state);
    return false;
  }
}

void MQTTClient::reconnect() {
#if ENABLE_API_UPLOAD
  unsigned long now = millis();
  
  // 재연결 시도는 5초마다 한 번씩만
  if (now - _lastReconnectAttempt > 5000) {
    _lastReconnectAttempt = now;
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("[MQTT] Attempting to reconnect...");
      if (connect()) {
        _lastReconnectAttempt = 0;  // 연결 성공 시 리셋
      }
    } else {
      Serial.println("[MQTT] WiFi not connected, skipping reconnect");
    }
  }
#endif
}

void MQTTClient::loop() {
#if ENABLE_API_UPLOAD
  if (!_mqttClient.connected()) {
    reconnect();
  } else {
    _mqttClient.loop();  // MQTT 메시지 처리
  }
#endif
}

bool MQTTClient::publishData() {
#if ENABLE_API_UPLOAD
  if (!_mqttClient.connected()) {
    Serial.println("[MQTT] Not connected, cannot publish");
    return false;
  }
  
  // CPU ID 가져오기 (48비트 MAC을 24자리 HEX 문자열로)
  uint64_t chipid = ESP.getEfuseMac();
  uint8_t mac[6];
  for (int i = 0; i < 6; i++) {
    mac[i] = (chipid >> (i * 8)) & 0xFF;
  }
  
  char cpuid[32];
  snprintf(cpuid, sizeof(cpuid), "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
    mac[5], mac[4], mac[3], mac[2], mac[1], mac[0],
    mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
  
  // zz 필드 생성 (파이프 구분)
  // 형식: 전화번호|IMEI|CPUID|IDX|압축기전류|히터전류|팬전류|버전|재상모드|O3모드|재상주기|재상시간|오존주기|오존발생시간|릴레이상태|오존측정|제어온도|T1온도|T2온도|습도|CO2|설정온도|오존기준|습도상태|시스템상태
  char zz[256];
  uint16_t revision = 10;  // 펌웨어 버전 1.0
  uint16_t sys_state = 0x0000;  // 시스템 상태
  //inx=0x01;
  snprintf(zz, sizeof(zz),
    "01252769611|770186056655075|%s|01|%04X|%04X|%04X|%04X|00|00|%04X|0000|001E|0032|%02X|0000|%04X|%04X|0000|%04X|0000|%04X|03E8|0000|%04X|",
    cpuid,                              // CPUID
    0x0000,                             // 압축기 전류
    0x0000,                             // 히터 전류  
    gCUR.fan_current,                   // 팬 전류
    2511,                               // 펌웨어 버전
                                        // 재상 모드 00
                                        // O3 모드 00
    gCUR.current_minute,                // 재상 주기
                                        // 재상 시간 0000
                                        // 오존 주기 001E (30)
                                        // 오존 발생시간 0032 (50)
    gCUR.relay_state.u8,                // 릴레이 상태 (RY1~RY8)
                                        // 오존 측정값 0000
    (int)(gCUR.measure_ntc_temp * 10),  // 제어 온도 (NTC, x10)
    (int)(gCUR.sht30_temp * 10),        // T1 온도 (SHT30, x10)
                                        // T2 온도 0000
    (int)(gCUR.sht30_humidity * 10),    // 습도 (SHT30, x10)
                                        // CO2 0000
    (int)(gCUR.seljung_temp*10),        // 설정 온도
                                        // 오존 기준 03E8 (1000)
                                        // 습도 상태 0000
    sys_state                           // 시스템 상태
  );
  
  // 체크섬 계산
  uint8_t sum = 0;
  for (int i = 0; zz[i] != '\0'; i++) {
    sum += zz[i];
  }
  
  // JSON 페이로드 생성 (idx:0, zz)
  char payload[384];
  snprintf(payload, sizeof(payload),
    "{\"idx\":0,\"zz\":\"%s%02X\"}",
    zz, sum);
  
  Serial.printf("[MQTT] Publishing to %s: %s\n", AWS_IOT_PUBLISH_TOPIC, payload);
  
  bool success = _mqttClient.publish(AWS_IOT_PUBLISH_TOPIC, payload);
  
  if (success) {
    Serial.println("[MQTT] Publish successful");
    _lastPublishTime = millis();
  } else {
    Serial.println("[MQTT] Publish failed");
  }
  
  return success;
#else
  Serial.println("[MQTT] Upload disabled");
  return false;
#endif
}

bool MQTTClient::publishEvent(uint16_t xor_uEvent, uint16_t uEvent) {
#if ENABLE_API_UPLOAD
  if (!_mqttClient.connected()) {
    Serial.println("[MQTT] Not connected, cannot publish event");
    return false;
  }
  
  // CPU ID 가져오기 (24자리 HEX)
  uint64_t chipid = ESP.getEfuseMac();
  uint8_t mac[6];
  for (int i = 0; i < 6; i++) {
    mac[i] = (chipid >> (i * 8)) & 0xFF;
  }
  
  char cpuid[32];
  snprintf(cpuid, sizeof(cpuid), "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
    mac[5], mac[4], mac[3], mac[2], mac[1], mac[0],
    mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
  
  // evt 필드 생성: CPUID|xor_event|event
  // xor_event: 변경된 이벤트 비트, event: 현재 이벤트 상태
  char evt[64];
  snprintf(evt, sizeof(evt), "%s|%04X|%04X|", cpuid, xor_uEvent, uEvent);
  
  // 체크섬 계산
  uint8_t sum = 0;
  for (int i = 0; evt[i] != '\0'; i++) {
    sum += evt[i];
  }
  
  // JSON 페이로드 생성 (idx:1, evt)
  char payload[128];
  snprintf(payload, sizeof(payload),
    "{\"idx\":1,\"evt\":\"%s%02X\"}",
    evt, sum);
  
  Serial.printf("[MQTT] Publishing event to %s: %s\n", AWS_IOT_PUBLISH_TOPIC, payload);
  
  bool success = _mqttClient.publish(AWS_IOT_PUBLISH_TOPIC, payload);
  
  if (success) {
    Serial.println("[MQTT] Event publish successful");
  } else {
    Serial.println("[MQTT] Event publish failed");
  }
  
  return success;
#else
  Serial.println("[MQTT] Event upload disabled");
  return false;
#endif
}

bool MQTTClient::isConnected() {
#if ENABLE_API_UPLOAD
  return _mqttClient.connected();
#else
  return false;
#endif
}
