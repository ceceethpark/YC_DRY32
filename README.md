# ESP32 Dryer Controller

ESP32 기반 건조기 제어 시스템

## 하드웨어 사양

- **MCU**: ESP32-D0WD (240MHz, 320KB RAM, 4MB Flash)
- **보드**: esp32dev
- **개발환경**: PlatformIO + Arduino Framework

## 주요 기능

### 1. 온도 제어
- NTC 온도 센서 (3개) - Pins: 36, 34, 35
- 실시간 온도 측정 및 필터링
- PID 제어 기반 히터 제어
- 과열 보호 (PIN_OH)

### 2. 운전 제어
- 전원 스위치 버튼 (PIN_PWR_SW)
- 시작/정지 토글 기능
- 자동 타이머 (설정: 30분)
- 완료 시 자동 정지 및 알림

### 3. 디스플레이
- TM1638 FND 7-segment 디스플레이
- 실시간 온도/시간 표시
- 교대 표시 모드 (3초 간격)

### 4. 네트워크 기능
- WiFi 연결 (SSID: cecee)
- OTA (Over-The-Air) 무선 업데이트
- 호스트명: DryerESP32.local

### 5. 안전 기능
- 과열 감지 및 자동 차단
- 팬 전류 모니터링
- 비정상 동작 감지

## ESP32 핀맵 (Pin Mapping)

### 전체 핀 배치표

| GPIO          | 물리핀 | 기능           | 신호명       | 입출력     | ADC       | 특수기능/제한사항              |
|:-------------:|:------:|:--------------:|:------------:|:----------:|:---------:|:------------------------------|
| GPIO 36 (VP)  | 4      | NTC 온도센서   | pAIN_NTC     | 입력 전용  | ADC1_CH0  | ✓ 입력 전용 핀                |
| GPIO 39 (VN)  | 5      | 팬 전류 감시   | pAIN_FAN     | 입력 전용  | ADC1_CH3  | ✓ 입력 전용 핀                |
| GPIO 34       | 6      | (예비)         | pAIN_PCB     | 입력 전용  | ADC1_CH6  | ✓ 입력 전용 핀                |
| GPIO 35       | 7      | SHT30 온도     | SHT30_T      | 입력 전용  | ADC1_CH7  | ✓ 입력 전용 핀                |
| GPIO 32       | 8      | SHT30 습도     | SHT30_H      | 입출력     | ADC1_CH4  |                               |
| GPIO 33       | 9      | LCD Reset      | pLCD_RES     | 출력       | ADC1_CH5  | LCD 제어                      |
| GPIO 25       | 10     | LCD CS         | pLCD_CS      | 출력       | ADC2_CH8  | LCD 제어                      |
| GPIO 26       | 11     | 과열 감지      | pOH_CTRL     | 입력       | ADC2_CH9  | HIGH=과열, LOW=정상           |
| GPIO 27       | 12     | FND STB        | FND_STB      | 출력       | ADC2_CH7  | TM1638 제어                   |
| GPIO 14       | 13     | FND CLK       | FND_CLK      | 출력       | ADC2_CH6  | TM1638 제어                   |
| GPIO 12       | 14     | (예비)         | -            | 입출력     | ADC2_CH5  | ⚠️ Boot 시 LOW 유지          |
| GPIO 13       | 16     | LCD SCK        | pLCD_SCK     | 출력       | ADC2_CH4  | LCD SPI 클럭                  |
| GPIO 15       | 23     | FND DIO        | FND_DIO      | 출력       | ADC2_CH3  | ⚠️ Boot 시 HIGH 유지         |
| GPIO 2        | 24     | (예비)         | -            | 입출력     | ADC2_CH2  | ⚠️ Boot 시 LOW 유지          |
| GPIO 0        | 25     | (예비)         | -            | 입출력     | ADC2_CH1  | ⚠️ Boot 모드 선택 핀         |
| GPIO 4        | 26     | AUX2 출력      | pAU2         | 출력       | ADC2_CH0  | 보조 출력                     |
| GPIO 16       | 27     | AUX1 출력      | pAUT         | 출력       | -         | 보조 출력                     |
| GPIO 17       | 28     | AUX0 출력      | pAU0         | 출력       | -         | 보조 출력                     |
| GPIO 5        | 29     | 히터 릴레이    | pHEATER      | 출력       | -         | 히터 제어                     |
| GPIO 18       | 30     | 팬 릴레이      | pFAN         | 출력       | -         | 팬 제어                       |
| GPIO 19       | 31     | 댐퍼 모터      | pDamp        | 출력       | -         | 댐퍼 제어                     |
| GPIO 21       | 33     | 전원 스위치    | PWR_SW       | 입력(IRQ)  | -         | 인터럽트 가능                 |
| GPIO 3 (RXD0) | 34     | UART RX        | RXD0         | 입력       | -         | USB 시리얼 통신               |
| GPIO 1 (TXD0) | 35     | UART TX        | TXD0         | 출력       | -         | USB 시리얼 통신               |
| GPIO 22       | 36     | 부저           | pBEEP        | 출력       | -         | 부저 제어                     |
| GPIO 23       | 37     | Zero-Cross     | pZC_IRQ      | 입력(IRQ)  | -         | AC 주파수 감지                |
| EN            | 3      | Reset          | EN           | 입력       | -         | 리셋 버튼 (10K Pull-up)       |

### 핀 사용

**출력 핀 (12개):**
- 릴레이/제어: GPIO 5(히터), GPIO 18(팬), GPIO 19(댐퍼), GPIO 17/16/4(AUX)
- 부저: GPIO 22
- FND: GPIO 27/14/15 (STB/CLK/DIO)
- LCD: GPIO 33/25/13 (CS/RES/SCK)

**입력 핀 (7개):**
- 인터럽트: GPIO 21(전원SW), GPIO 23(Zero-Cross)
- 아날로그: GPIO 36(NTC), GPIO 39(팬전류), GPIO 35(SHT30_T), GPIO 34(예비)
- 디지털: GPIO 26(과열감지)

**통신 핀:**
- UART: GPIO 1(TX), GPIO 3(RX)
- I2C: GPIO 32(SDA), GPIO 33(SCL)

### ⚠️ 중요 주의사항

- **입력 전용 핀**: GPIO 34, 35, 36, 39 (물리핀 4~7) - 출력으로 사용 불가
- **Boot 모드 핀**: GPIO 0, 2, 12, 15 - 부팅 시 상태에 영향
- **ADC2 제한**: WiFi 활성화 시 GPIO 0, 2, 4, 12~15, 25~27의 ADC 사용 불가
- **ADC1 권장**: GPIO 32~39는 WiFi와 함께 ADC 사용 가능
- **인터럽트**: 대부분의 GPIO 핀에서 사용 가능

## 빌드 및 업로드

### USB 업로드
```bash
platformio run --target upload
```

### OTA 무선 업로드
```bash
# platformio.ini에서 OTA 설정 활성화 후
platformio run --target upload
```

### 시리얼 모니터
```bash
platformio device monitor
```

## 프로젝트 구조

```
├── platformio.ini          # 프로젝트 설정
├── src/
│   ├── main.cpp           # 메인 프로그램
│   ├── config.h           # 핀 및 WiFi 설정
│   ├── typedef.h          # 데이터 구조체 정의
│   ├── dataClass/         # 데이터 관리 클래스
│   ├── TM1638Display/     # FND 디스플레이 제어
│   └── updateAPI/         # API 업데이트 모듈
└── logs/                  # 로그 파일
```

## 설정값

- **기본 온도**: 60℃
- **기본 시간**: 30분
- **온도 히스테리시스**: ±2℃
- **디스플레이 전환**: 3초
- **디버그 출력**: 5초 간격

## 버전 이력

### v1.1 (2025-11-29)
- 실제 동작하는 건조기 제어 프로그램
- 버튼 입력 처리 추가
- 자동 온도 제어 구현
- 디스플레이 모드 개선
- OTA 업데이트 지원

### v1.0
- 초기 버전
- 기본 하드웨어 제어
- WiFi 및 OTA 기능

## 라이선스

MIT License

## 작성자

thpark
