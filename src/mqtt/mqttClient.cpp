// mqttClient.cpp - AWS IoT MQTT 클라이언트 구현

#include "mqttClient.h"
#include "aws_certs.h"

MQTTClient gMQTTClient;

// MQTT 메시지 수신 콜백
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("[MQTT] Message arrived [topic: ");
  Serial.print(topic);
  Serial.print("] payload: ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
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

bool MQTTClient::publishData(float temperature, int elapsedMinutes) {
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
  uint8_t relay_state = 0x00;  // 릴레이 상태 (6비트)
  uint16_t sys_state = 0x0000;  // 시스템 상태
  //inx=0x01;
  snprintf(zz, sizeof(zz),
    "01252769611|770186056655075|%s|01|%04X|%04X|%04X|%04X|00|00|0000|0000|001E|0032|%02X|0000|%04X|0000|0000|0000|0000|0046|03E8|0000|%04X|",
    cpuid,                              // CPUID
    0x0000,                             // 압축기 전류
    0x0000,                             // 히터 전류  
    0x0000,                             // 팬 전류
    0x2511,                             // 펌웨어 버전
                                        // 재상 모드 00
                                        // O3 모드 00
                                        // 재상 주기 0000
                                        // 재상 시간 0000
                                        // 오존 주기 001E (30)
                                        // 오존 발생시간 0032 (50)
    relay_state,                        // 릴레이 상태
                                        // 오존 측정값 0000
    (int)(temperature * 10),            // 제어 온도 (x10)
                                        // T1 온도 0000
                                        // T2 온도 0000
                                        // 습도 0000
                                        // CO2 0000
                                        // 설정 온도 0046 (70)
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
