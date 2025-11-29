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

## 핀 배치

```
입력:
- PIN_PWR_SW (2): 전원 스위치
- PIN_OH (4): 과열 감지
- PIN_ZCIRQ (26): Zero-cross 인터럽트
- PIN_NTC1 (36): NTC 온도센서 1
- PIN_NTC2 (34): NTC 온도센서 2
- PIN_NTC3 (35): NTC 온도센서 3
- PIN_FAN_CURRENT (39): 팬 전류 측정

출력:
- PIN_FAN (18): 팬 제어
- PIN_HEATER (5): 히터 제어
- PIN_AUX0~2 (19, 23, 22): 보조 출력
- PIN_DAMP (21): 댐퍼 제어
- PIN_BEEP (25): 부저

TM1638 FND:
- PIN_FND_STB (27): Strobe
- PIN_FND_CLK (14): Clock
- PIN_FND_DIO (15): Data I/O
```

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
