// config.h  -- ESP32 건조기 컨트롤러 핀맵 & 설정

#pragma once

#define PIN_PWR_SW      21    // IRQ
#define PIN_ZCIRQ       23    // IRQ
#define PIN_OH          26    // OverHeat 감지 (HIGH=과열, LOW=정상)
// ====== 릴레이 / 출력 핀 ======
#define PIN_FAN         18    // pFAN
#define PIN_HEATER       5    // pHEATER
#define PIN_AUX0        17    // pAu0
#define PIN_AUX1        16    // pAu1
#define PIN_AUX2         4    // pAu2
#define PIN_DAMP        19    // pDamp (댐퍼 모터 제어)
#define PIN_BEEP        22    // pBEEP (부저)

// ====== 아날로그 입력 (NTC / FAN 전류) ======
#define PIN_NTC1        36    // 메인 온도 NTC (ADC1_CH0)
#define PIN_NTC2        34    // 보조 NTC
#define PIN_NTC3        35    // 댐퍼 NTC
#define PIN_FAN_CURRENT 39    // 팬 전류 감시(옵션)

// ====== TM1638 FND ======
#define PIN_FND_STB     27    // STB
#define PIN_FND_CLK     14    // CLK
#define PIN_FND_DIO     15    // DIO

#define FND_BRIGHTNESS  0x0f  // 0x88~0x8f (밝기)

// ====== Wi-Fi / OTA 설정 ======
// TODO: 실제 WiFi 정보로 변경하세요!
#define WIFI_SSID       "cecee"      // 실제 WiFi SSID로 변경
#define WIFI_PASS       "898900898900"  // 실제 WiFi 비밀번호로 변경
#define OTA_HOSTNAME    "DryerESP32"
#define OTA_PASSWORD    "1234"    // OTA 접속 비밀번호 (보안을 위해 변경 권장)

// ====== 디버그 모드 ======
#define DEBUG_MODE  // 이 줄을 주석 해제하면 디버그 모드 (타이머 기반 콜백)

// ====== 건조 제어 파라미터 (임시값) ======
#define TARGET_TEMP_C   45.0f   // 목표 온도 (℃)
#define TEMP_HYST       2.0f    // 히스테리시스 (±℃)
#define CONTROL_PERIOD  1000UL  // 제어 주기 (ms)

// ====== NTC 측정용 기본값 ======
// 5K NTC (Beta=3970), 5K pull-up, 3.3V
#define NTC_PULL_RES    5000.0f   // 풀업 저항 5kΩ
#define NTC_BETA        3970.0f   // NTC Beta 값 3970
#define NTC_R25         5000.0f   // 25℃에서 5kΩ
#define NTC_T0_K        298.15f   // 25℃ = 298.15K

// ====== 공정 데이터 업로드 설정 ======
#define ENABLE_API_UPLOAD         1                     // 1로 설정하여 업로드 활성화
#define API_ENDPOINT_URL          "http://13.125.246.193/services/processData"
#define API_HEADER_SECURITY_KEY   "Security"
#define API_HEADER_SECURITY_VALUE "089d3f1ad4cb11a1cf4d2a4fb567ff3ceca2fecce86b492b673707fcf16d8c94"
#define API_PRODUCT_ID            "1006"
#define API_PARTNER_ID            "1001"
#define API_MACHINE_ID            "1008"
#define API_RECORD_ID             "1055"              // 필요 시 서버 요구사항에 맞게 변경
#define API_DEPARTURE_YN          "N"                 // 대문자 N (Postman과 동일)
#define API_UPLOAD_INTERVAL_MS    (5UL * 60UL * 1000UL) // 5분 주기
