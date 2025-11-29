#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ArduinoOTA.h>
#include <Preferences.h>

#include "config.h"
#include "typedef.h"
#include "dataClass/dataClass.h"
#include "TM1638Display/TM1638Display.h"
#include "updateAPI/updateAPI.h"

// ========== 전역 변수 ==========
bool system_start_flag = false;  // 시스템 시작 플래그
bool system_running = false;     // 운전 중 플래그
uint16_t set_temperature = 60;   // 설정 온도 (℃)
uint16_t set_time_minutes = 30;  // 설정 시간 (분)
uint16_t running_seconds = 0;    // 운전 경과 시간 (초)

// ========== 전역 객체 ==========
dataClass gData;
TM1638Display gDisplay(PIN_FND_STB, PIN_FND_CLK, PIN_FND_DIO, FND_BRIGHTNESS);

// ========== 타이머 인터럽트 변수 ==========
hw_timer_t *timer1sec = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
volatile bool flag_1sec = false;

// ========== 타이머 인터럽트 핸들러 ==========
void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  flag_1sec = true;
  portEXIT_CRITICAL_ISR(&timerMux);
}

// ========== WiFi 연결 ==========
void connectWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
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
  } else {
    Serial.println("\nWiFi connection failed. Continuing without WiFi.");
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
  pinMode(PIN_PWR_SW, INPUT);
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

// ========== 타이머 초기화 (1초 주기) ==========
void initTimer() {
  timer1sec = timerBegin(0, 80, true);  // 80MHz / 80 = 1MHz
  timerAttachInterrupt(timer1sec, &onTimer, true);
  timerAlarmWrite(timer1sec, 1000000, true);  // 1초 = 1,000,000 μs
  timerAlarmEnable(timer1sec);
  Serial.println("Timer initialized (1 sec)");
}

// ========== setup ==========
void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n===========================================");
  Serial.println("ESP32 Dryer Controller v1.1");
  Serial.println("===========================================");
  
  // 핀 초기화
  initPins();
  
  // TM1638 디스플레이 초기화
  gDisplay.begin();
  gDisplay.clear();
  
  // dataClass 초기화
  gData.begin();
  
  // WiFi 연결
  connectWiFi();
  
  // OTA 설정
  if (WiFi.status() == WL_CONNECTED) {
    setupOTA();
  }
  
  // 타이머 시작
  initTimer();
  
  Serial.println("Setup complete!");
  Serial.printf("Default: Temp=%d°C, Time=%dmin\n", set_temperature, set_time_minutes);
  Serial.println("===========================================\n");
}

// ========== 버튼 처리 ==========
void handleButtons() {
  static bool lastPwrSwState = HIGH;
  static unsigned long lastDebounceTime = 0;
  const unsigned long debounceDelay = 50;
  
  bool currentPwrSwState = digitalRead(PIN_PWR_SW);
  
  // 디바운싱
  if (currentPwrSwState != lastPwrSwState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    // 버튼이 눌렸을 때 (LOW)
    if (currentPwrSwState == LOW && lastPwrSwState == HIGH) {
      // 운전 시작/정지 토글
      system_running = !system_running;
      
      if (system_running) {
        Serial.println("=== START ===");
        running_seconds = 0;
        gCUR.total_remain_minute = set_time_minutes;
        gCUR.ctrl.fan = 1;
        digitalWrite(PIN_FAN, HIGH);
        digitalWrite(PIN_BEEP, HIGH);
        delay(100);
        digitalWrite(PIN_BEEP, LOW);
      } else {
        Serial.println("=== STOP ===");
        gCUR.ctrl.heater = 0;
        gCUR.ctrl.fan = 0;
        digitalWrite(PIN_HEATER, LOW);
        digitalWrite(PIN_FAN, LOW);
        digitalWrite(PIN_BEEP, HIGH);
        delay(200);
        digitalWrite(PIN_BEEP, LOW);
      }
    }
  }
  
  lastPwrSwState = currentPwrSwState;
}

// ========== 디스플레이 업데이트 ==========
// void updateDisplay() {
//   static uint8_t displayMode = 0;  // 0: 온도, 1: 시간
//   static int blinkCount = 0;
  
//   if (system_running) {
//     // 운전 중: 교대로 온도/시간 표시
//     blinkCount++;
//     if (blinkCount >= 3) {  // 3초마다 전환
//       blinkCount = 0;
//       displayMode = !displayMode;
//     }
    
//     if (displayMode == 0) {
//       // 현재 온도 표시 (예: 45.2℃)
//       int tempX10 = (int)(gCUR.measure_ntc_temp * 10.0f);
//       gDisplay.setNumber(0, tempX10, 1);  // 소수점 1자리
//       gDisplay.setDot(2, true);  // 소수점 표시
//     } else {
//       // 남은 시간 표시 (분)
//       gDisplay.setNumber(0, gCUR.total_remain_minute, 0);
//     }
//   } else {
//     // 대기 중: 설정 온도 표시
//     gDisplay.setNumber(0, set_temperature, 0);
//   }
// }

// ========== loop ==========
void loop() {
  // OTA 핸들링
  ArduinoOTA.handle();
  
  // 버튼 입력 처리
 // handleButtons();
  gDisplay.key_process();
  gDisplay.sendToDisplay();

  // 1초마다 실행
  if (flag_1sec) {
    portENTER_CRITICAL(&timerMux);
    flag_1sec = false;
    portEXIT_CRITICAL(&timerMux);
    
    // dataClass 1초 주기 처리
    gData.onSecondElapsed();
    
    // 온도 측정 및 필터링
    gData.measureAndFilterTemp();
    
    // 운전 중일 때만 제어
    if (system_running) {
      running_seconds++;
      
      // 1분마다 남은 시간 감소
      if (running_seconds % 60 == 0 && gCUR.total_remain_minute > 0) {
        gCUR.total_remain_minute--;
        
        // 시간 종료 체크
        if (gCUR.total_remain_minute == 0) {
          Serial.println("=== FINISH ===");
          system_running = false;
          gCUR.ctrl.heater = 0;
          gCUR.ctrl.fan = 0;
          digitalWrite(PIN_HEATER, LOW);
          digitalWrite(PIN_FAN, LOW);
          
          // 완료 부저
          for (int i = 0; i < 3; i++) {
            digitalWrite(PIN_BEEP, HIGH);
            delay(200);
            digitalWrite(PIN_BEEP, LOW);
            delay(200);
          }
        }
      }
      
      // 히터 제어 (온도 기반)
      if (gCUR.measure_ntc_temp < set_temperature - 2.0) {
        // 온도가 낮으면 히터 ON
        if (gCUR.ctrl.heater == 0) {
          gCUR.ctrl.heater = 1;
          digitalWrite(PIN_HEATER, HIGH);
        }
      } else if (gCUR.measure_ntc_temp >= set_temperature) {
        // 설정 온도에 도달하면 히터 OFF
        if (gCUR.ctrl.heater == 1) {
          gCUR.ctrl.heater = 0;
          digitalWrite(PIN_HEATER, LOW);
        }
      }
    }
    
    // 과열 체크 (항상)
    gData.checkOverheat();
    
    // 팬 전류 체크 (운전 중일 때만)
    if (system_running) {
      gData.checkFanCurrent();
    }
    
    // 디스플레이 업데이트
    //updateDisplay();
    
    // 1분마다 처리
    static int secondCount = 0;
    secondCount++;
    if (secondCount >= 60) {
      secondCount = 0;
      gData.onMinuteElapsed();
      
      // API 업로드 (활성화 시)
      #if ENABLE_API_UPLOAD
      static unsigned long lastUpload = 0;
      if (millis() - lastUpload >= API_UPLOAD_INTERVAL_MS) {
        lastUpload = millis();
        
        // 데이터 준비 및 전송
        int temp = (int)gCUR.measure_ntc_temp;
        int humidity = 50;  // 예시값
        int remainMin = gCUR.total_remain_minute;
        
        bool success = uploadProcessData(
          API_PRODUCT_ID, API_PARTNER_ID, API_MACHINE_ID,
          API_RECORD_ID, API_DEPARTURE_YN,
          temp, humidity, remainMin
        );
        
        if (success) {
          Serial.println("API upload success");
        } else {
          Serial.println("API upload failed");
        }
      }
      #endif
    }
    
    // 디버그 출력
    static int debugCount = 0;
    debugCount++;
    if (debugCount >= 5) {  // 5초마다
      debugCount = 0;
      Serial.printf("[%s] Temp: %.1f°C (Set:%d°C) | Time: %d/%dmin | Heater:%d Fan:%d | OH:%d\n",
        system_running ? "RUN" : "STOP",
        gCUR.measure_ntc_temp,
        set_temperature,
        (set_time_minutes - gCUR.total_remain_minute),
        set_time_minutes,
        gCUR.ctrl.heater,
        gCUR.ctrl.fan,
        digitalRead(PIN_OH)
      );
    }
  }
  
  delay(10);  // CPU 부하 감소
}
