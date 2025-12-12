#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ArduinoOTA.h>
#include <Preferences.h>

#include "config.h"
#include "typedef.h"
#include "dataClass/dataClass.h"
#include "TM1638Display/TM1638Display.h"
#include "mqtt/mqttClient.h"

// ========== 전역 변수 ==========
uint64_t gChipID = 0;            // ESP32 Chip ID (MAC 기반 고유 ID)
bool system_start_flag = false;  // 시스템 시작 플래그
bool system_running = false;     // 운전 중 플래그
uint16_t running_seconds = 0;    // 운전 경과 시간 (초)
bool smartconfig_request = false;  // SmartConfig 요청 플래그
bool smartconfig_running = false;  // SmartConfig 실행 중
unsigned long smartconfig_start_time = 0;  // SmartConfig 시작 시간

// ========== 전역 객체 ==========
dataClass gData;
TM1638Display gDisplay(PIN_FND_STB, PIN_FND_CLK, PIN_FND_DIO, FND_BRIGHTNESS);
// MQTT 클라이언트는 mqttClient.cpp에서 정의됨
Preferences preferences;

// ========== FreeRTOS Task 핸들 ==========
TaskHandle_t uiTaskHandle = NULL;

// ========== FreeRTOS Task 핸들 ==========
TaskHandle_t displayTaskHandle = NULL;
TaskHandle_t keyboardTaskHandle = NULL;

// ========== 타이머 인터럽트 변수 ==========
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
volatile uint16_t zc_count = 0;        // Zero-cross 카운터 (0~119) 또는 타이머 카운터
volatile uint8_t second_counter = 0;   // 초 카운터 (0~59)
volatile bool flag_1sec = false;       // 1초 플래그
volatile bool flag_1min = false;       // 1분 플래그

#ifdef DEBUG_MODE
// ========== DEBUG: 내부 타이머 인터럽트 핸들러 (1초 주기) ==========
hw_timer_t *timer1sec = NULL;

void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  
  // 1초 플래그 설정
  flag_1sec = true;
  
  // 1분 카운터 증가
  second_counter++;
  if (second_counter >= 60) {
    second_counter = 0;
    flag_1min = true;
  }
  
  portEXIT_CRITICAL_ISR(&timerMux);
}
#else
// ========== RELEASE: Zero-Cross 인터럽트 핸들러 (AC 60Hz) ==========
// 60Hz AC -> 120 zero-cross/sec (양방향)
void IRAM_ATTR onZeroCross() {
  portENTER_CRITICAL_ISR(&timerMux);
  
  zc_count++;
  
  // 120번 카운트 = 1초
  if (zc_count >= 120) {
    zc_count = 0;
    flag_1sec = true;
    
    // 1분 카운터 증가
    second_counter++;
    if (second_counter >= 60) {
      second_counter = 0;
      flag_1min = true;
    }
  }
  
  portEXIT_CRITICAL_ISR(&timerMux);
}
#endif

// ========== UI Task (Keyboard + Display) ==========
void uiTask(void *parameter) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(50);  // 50ms 주기
  
  uint8_t displayCounter = 0;  // Display 업데이트 카운터
  
  for (;;) {
    // Keyboard 처리 (매 50ms)
    gDisplay.key_process();
    
    // Display 처리 (매 100ms = 50ms x 2)
    displayCounter++;
    if (displayCounter >= 2) {
      gDisplay.sendToDisplay();
      displayCounter = 0;
    }
    
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

// ========== WiFi 연결 (ESPTouch SmartConfig 지원) ==========
void connectWiFi() {
  // Flash에서 저장된 WiFi 정보 읽기
  preferences.begin("wifi", true);
  String saved_ssid = preferences.getString("ssid", "");
  String saved_pass = preferences.getString("password", "");
  preferences.end();
  
  WiFi.mode(WIFI_STA);
  
  // 저장된 WiFi 정보가 있으면 사용
  if (saved_ssid.length() > 0) {
    Serial.println("Using saved WiFi credentials");
    Serial.print("SSID: ");
    Serial.println(saved_ssid);
    WiFi.begin(saved_ssid.c_str(), saved_pass.c_str());
  } else {
    // 없으면 config.h의 기본값 사용
    Serial.println("Using default WiFi credentials");
    WiFi.begin(WIFI_SSID, WIFI_PASS);
  }
  
  Serial.print("Connecting to WiFi");
  int cnt = 0;
  while (WiFi.status() != WL_CONNECTED && cnt < 20) {
    delay(500);
    Serial.print(".");
    cnt++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    gCUR.led.network = 1;  // 네트워크 활성화 LED ON
  } else {
    Serial.println("\nWiFi connection failed. Continuing without WiFi.");
    gCUR.led.network = 0;  // 네트워크 비활성화 LED OFF
  }
}

// ========== ESPTouch SmartConfig 시작 ==========
void startSmartConfig() {
  Serial.println("Starting SmartConfig...");
  
  // FND 상태 변경: SmartConfig 시작
  gCUR.fnd_state = FND_SMARTCONFIG_START;
  
  WiFi.mode(WIFI_STA);
  WiFi.beginSmartConfig();
  
  // FND 상태: SmartConfig 대기
  gCUR.fnd_state = FND_SMARTCONFIG_WAIT;
  
  smartconfig_running = true;
  smartconfig_start_time = millis();
  
  Serial.println("Waiting for SmartConfig (30 sec timeout)...");
}

// ========== SmartConfig 처리 (loop에서 호출) ==========
void processSmartConfig() {
  if (!smartconfig_running) return;
  
  // 타임아웃 체크 (30초)
  if (millis() - smartconfig_start_time > 30000) {
    Serial.println("\nSmartConfig Timeout (30 sec)");
    WiFi.stopSmartConfig();
    smartconfig_running = false;
    gCUR.led.network = 0;
    gCUR.fnd_state = FND_DRY_STATE;
    return;
  }
  
  // 성공 체크
  if (WiFi.smartConfigDone()) {
    smartconfig_running = false;
    
    Serial.println("\nSmartConfig Success!");
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());
    
    // FND 상태: SmartConfig 완료
    gCUR.fnd_state = FND_SMARTCONFIG_DONE;
    
    // WiFi 정보를 Flash에 저장
    preferences.begin("wifi", false);
    preferences.putString("ssid", WiFi.SSID());
    preferences.putString("password", WiFi.psk());
    preferences.end();
    
    Serial.println("WiFi credentials saved to Flash");
    gCUR.led.network = 1;
    
    // 2초 후 정상 모드로 복귀 (타이머로 처리)
    smartconfig_start_time = millis();  // 2초 대기용 재사용
  }
}

// ========== SmartConfig DONE 후 대기 ==========
void checkSmartConfigDone() {
  if (gCUR.fnd_state == FND_SMARTCONFIG_DONE) {
    if (millis() - smartconfig_start_time > 2000) {
      gCUR.fnd_state = FND_DRY_STATE;
    }
  }
}

// ========== OTA 설정 ==========
void setupOTA() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  
  ArduinoOTA.onStart([]() {
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
    Serial.println("Start updating " + type);
    gDisplay.clear();
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA Update End");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  
  ArduinoOTA.begin();
  Serial.println("OTA Ready");
}

// ========== 핀 초기화 ==========
void initPins() {
  // 출력 핀 설정
  pinMode(PIN_FAN, OUTPUT);
  pinMode(PIN_HEATER, OUTPUT);
  pinMode(PIN_AUX0, OUTPUT);
  pinMode(PIN_AUX1, OUTPUT);
  pinMode(PIN_AUX2, OUTPUT);
  pinMode(PIN_DAMP, OUTPUT);
  pinMode(PIN_BEEP, OUTPUT);
  
  // 입력 핀 설정
  pinMode(PIN_OH, INPUT);
  pinMode(PIN_ZCIRQ, INPUT);
  
  // 아날로그 핀은 자동 설정
  // PIN_NTC1, PIN_NTC2, PIN_NTC3, PIN_FAN_CURRENT
  
  // 초기 출력 상태
  digitalWrite(PIN_FAN, LOW);
  digitalWrite(PIN_HEATER, LOW);
  digitalWrite(PIN_AUX0, LOW);
  digitalWrite(PIN_AUX1, LOW);
  digitalWrite(PIN_AUX2, LOW);
  digitalWrite(PIN_DAMP, LOW);
  digitalWrite(PIN_BEEP, LOW);
}

// ========== 타이머/인터럽트 초기화 ==========
void initTimerInterrupt() {
#ifdef DEBUG_MODE
  // DEBUG 모드: 내부 타이머 사용 (1초 주기)
  timer1sec = timerBegin(0, 80, true);  // 80MHz / 80 = 1MHz
  timerAttachInterrupt(timer1sec, &onTimer, true);
  timerAlarmWrite(timer1sec, 1000000, true);  // 1초 = 1,000,000 μs
  timerAlarmEnable(timer1sec);
  Serial.println("DEBUG MODE: Internal timer initialized (1 sec)");
#else
  // RELEASE 모드: Zero-Cross 인터럽트 사용 (AC 60Hz)
  pinMode(PIN_ZCIRQ, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_ZCIRQ), onZeroCross, CHANGE);
  Serial.println("RELEASE MODE: Zero-Cross interrupt initialized (AC 60Hz -> 120 pulses/sec)");
#endif
}

// ========== setup ==========
void setup() {
  Serial.begin(115200);
 // delay(2000);
  
  Serial.println("\n\n===========================================");
  Serial.println("ESP32 Dryer Controller v1.1");
  Serial.println("===========================================");
  
  // ESP32 Chip ID 읽기 및 전역 변수에 저장 (MAC 주소 기반 Unique ID)
  gChipID = ESP.getEfuseMac();
  Serial.printf("ESP32 Chip ID: %04X%08X\n", (uint16_t)(gChipID >> 32), (uint32_t)gChipID);
  
  // 핀 초기화
  initPins();
  
  // ADC 설정 (11dB 감쇠로 0-3.3V 범위 사용)
  analogSetAttenuation(ADC_11db);
  analogReadResolution(12);  // 12비트 해상도 (0-4095)
  Serial.println("ADC configured: 11dB attenuation, 12-bit resolution (0-3.3V)");
  
  // TM1638 디스플레이 초기화
  gDisplay.begin();
  gDisplay.clear();
  
  // FND 부팅 화면 표시 (REVISION) - 즉시 시작
  gCUR.fnd_state = FND_BOOT;
  
  // FreeRTOS UI Task 생성 (Keyboard + Display 통합) - 최우선 실행
  xTaskCreatePinnedToCore(
    uiTask,             // Task 함수
    "UITask",           // Task 이름
    4096,               // Stack 크기
    NULL,               // Task 파라미터
    2,                  // 우선순위 (2 = 높음)
    &uiTaskHandle,      // Task 핸들
    1                   // Core 1에서 실행
  );
  
  // UI Task 시작 대기 (디스플레이가 업데이트되도록)
  delay(100);
  
  // dataClass 초기화
  gData.begin();
  
  // Flash에서 설정값 로드 (온도, 시간, 댐퍼 모드)
  gData.loadFromFlash();
  
  // 타이머/인터럽트 시작
  initTimerInterrupt();
  
  // WiFi 연결
  connectWiFi();
  
  // MQTT 클라이언트 초기화
  gMQTTClient.begin();
  
  // 시스템 시작 이벤트 발행 (Power-On)
  //delay(2000);  // MQTT 연결 대기
   delay(200);  // MQTT 연결 대기
  if (gMQTTClient.isConnected()) {
    // Power-On 이벤트 설정
    gCUR.pubEvent.uEvent.u16 = 0x0001;  // POWER_ON 비트
    gCUR.pubEvent.ex_uEvent.u16 = 0x0000;
    gCUR.pubEvent.xor_uEvent.u16 = 0x0001;  // 변경된 비트
    gMQTTClient.publishEvent(gCUR.pubEvent.xor_uEvent.u16, gCUR.pubEvent.uEvent.u16);
  }
  
  // OTA 설정
  if (WiFi.status() == WL_CONNECTED) {
    setupOTA();
  }
  
  Serial.println("Setup complete!");
  Serial.printf("Loaded: Temp=%d°C, Time=%dmin, Damper=%s\n", 
                gCUR.seljung_temp, gCUR.remaining_minute, 
                gCUR.auto_damper ? "AUTO" : "MANUAL");
  Serial.println("FreeRTOS UITask created: Keyboard(50ms) + Display(100ms)");
  Serial.println("Showing REVISION for 3 seconds...");
  Serial.println("===========================================\n");
}

// ========== loop ==========
void loop() {
  static unsigned long boot_display_start = millis();
  static bool boot_display_done = false;
  
  // 파워 ON 시 REVISION 3초 표시 후 FND_DRY_STATE로 전환
  if (!boot_display_done && gCUR.fnd_state == FND_BOOT) {
    if (millis() - boot_display_start >= 3000) {
      gCUR.fnd_state = FND_DRY_STATE;
      boot_display_done = true;
      Serial.println("Boot display done - Switching to FND_DRY_STATE");
    }
  }
  
  // OTA 핸들링
  ArduinoOTA.handle();
  
  // 네트워크 상태 LED 업데이트
  if (WiFi.status() == WL_CONNECTED) {
    gCUR.led.network = 1;  // 네트워크 활성화
  } else {
    gCUR.led.network = 0;  // 네트워크 비활성화
  }
  
  // 디스플레이와 키보드는 별도 Task에서 처리
  
  // SmartConfig 요청 체크 (TM1638Display::key_process()에서 설정)
  if (smartconfig_request) {
    smartconfig_request = false;
    Serial.println("KEY_PWR Long Press - Starting SmartConfig");
    startSmartConfig();
  }
  
  // SmartConfig 처리 (비블로킹)
  processSmartConfig();
  checkSmartConfigDone();
  
  // MQTT ID 수신 시 3초 후 FND_DRY_STATE로 복귀
  if (gCUR.fnd_state == FND_MQTT_RECV_ID) {
    if (millis() - gCUR.mqtt_recv_time >= 3000) {
      gCUR.fnd_state = FND_DRY_STATE;
      Serial.println("MQTT ID display timeout - Switching to FND_DRY_STATE");
    }
  }
  
  // 1초마다 실행
  if (flag_1sec) {
    portENTER_CRITICAL(&timerMux);
    flag_1sec = false;
    portEXIT_CRITICAL(&timerMux);
    
    // 1초 콜백 실행
    gData.onSecondElapsed();
  }
  
  // 1분마다 실행
  if (flag_1min) {
    portENTER_CRITICAL(&timerMux);
    flag_1min = false;
    portEXIT_CRITICAL(&timerMux);
    
    // 1분 콜백 실행
    gData.onMinuteElapsed();
  }
  
  // NTC1 ADC 필터링 (빠른 응답을 위해 main loop에서 처리)
  gCUR.avr_NTC1 = gData.get_m0_filter(analogReadMilliVolts(PIN_NTC1));
  
  // 네트워크 상타 LED 업데이트
  if (WiFi.status() == WL_CONNECTED) {
    gCUR.led.network = 1;  // 네트워크 활성화
  } else {
    gCUR.led.network = 0;  // 네트워크 비활성화
  }
  
  // MQTT loop (재연결 처리 및 메시지 처리)
  gMQTTClient.loop();
  
  // MQTT 데이터 전송 (1분 주기)
  #if ENABLE_API_UPLOAD
  static unsigned long lastPublishTime = 0;
  unsigned long currentTime = millis();
  
  if (currentTime - lastPublishTime >= API_UPLOAD_INTERVAL_MS) {
    lastPublishTime = currentTime;
    
    float tempC = gData.readNTCtempC();
    if (!isnan(tempC)) {
      int elapsedMinutes = running_seconds / 60;
      gMQTTClient.publishData();
    }
  }
  #endif
  
  delay(10);  // CPU 부하 감소
}
