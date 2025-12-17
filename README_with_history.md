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

... (중략; 기존 README 내용 유지) ...

## 변경 이력

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

## 작성자

thpark

## 자세한 변경 이력

자세한 개발 히스토리와 디버깅 로그는 [HISTORY.md](HISTORY.md) 파일을 참고하세요.
