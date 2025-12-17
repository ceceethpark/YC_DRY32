Project History and Changelog

요약: 최신 변경사항을 위로(최신 → 최초) 정리하고, 중복 설명을 제거하여 간결하게 정리했습니다.

---

## 2025-12-16 — OTA 점검 및 안정화

- `platformio.ini`에 OTA 환경 추가 및 `upload_protocol = espota` 설정
- `src/ota_only.cpp`(OTA 전용 테스터) 작성 — USB로 먼저 올려 ArduinoOTA 서비스 동작 확인
- `src/main.cpp`에서 `ArduinoOTA.begin()`/`ArduinoOTA.handle()`를 루프 초기에 호출하도록 변경하여 OTA 응답성 향상
- 문제: PlatformIO(Python) espota 업로더가 Windows에서 간헐적으로 전송이 진행되지 않음(인증 OK 후 0% 정지)
- 확인: Arduino IDE 및 Windows `espota.exe`로는 업로드 정상 완료 → 기기측 ArduinoOTA는 정상 동작
- 조치: `upload_command`로 Windows용 `espota.exe` 직접 호출을 테스트하여 호스트 업로더 차이 문제 우회
- 네트워크: mDNS 호스트명 `DryerESP32.local`이 Windows에서 정상 확인(ping)

---

## v1.1 (2025-11-29)

- 실제 동작하는 건조기 제어 기능 완성
- 버튼 입력 처리 개선 및 자동 온도 제어 구현
- 디스플레이(TM1638) 모드 개선
- OTA 업데이트 지원 추가

---

## 사용설명서 병합: 2024-12-12 주요 변경

### SmartConfig

- SmartConfig 종료 기능 추가: Power OFF 상태에서 `KEY_PWR` 길게 눌러 SmartConfig 진입, 동일 버튼으로 즉시 종료 가능
- 자동 타임아웃: SmartConfig 대기 중 30초 미입력 시 자동 종료 및 정상 모드 복귀
- SmartConfig 종료 후 Flash에 Power ON 상태 저장 후 `ESP.restart()`로 재시작

### 전원 제어

- Power OFF → `KEY_PWR` 짧게: Power ON, 상태 저장, 재시작
- Power ON → `KEY_PWR` 짧게: Power OFF, 상태 저장(재시작 없음)
- SmartConfig 중 `KEY_PWR` 누름: Power ON 저장 후 재시작(종료)

### 버튼 매핑

- `KEY_TEMP_UP`: FND 버튼 6 → 온도 증가
- `KEY_TEMP_DN`: FND 버튼 2 → 온도 감소

### 동작 요약

```
Power OFF 상태:
	- KEY_PWR 짧게 → Power ON + Flash 저장 + 재시작
	- KEY_PWR 길게 → SmartConfig 진입

Power ON 상태:
	- KEY_PWR 짧게 → Power OFF + Flash 저장(재시작 안함)
	- KEY_PWR 길게 → SmartConfig 진입 (30초 타임아웃)

SmartConfig 모드:
	- KEY_PWR 누름 → Power ON + Flash 저장 + 재시작 (종료)
	- 30초 미입력 → 자동 종료

정전 복구:
	- Flash에 저장된 이전 상태(ON/OFF)를 그대로 복원
```

---

## v1.0 (initial)

- 초기 버전: 기본 하드웨어 제어, WiFi 및 OTA 기능 포함

---

## Notes & Recommendations

- 민감 정보(예: `OTA_PASSWORD`, AWS 인증 키)는 코드에 직접 저장하지 말고 환경변수 또는 시크릿 매니저로 관리하세요.
- `parseCommand` 같은 문자열 기반 파싱은 메시지 형식 변경에 취약하므로 소규모 JSON 파서 도입을 권장합니다.
- 팬 전류 임계값, 센서 캘리브레이션 등 하드웨어별 설정 값은 실측으로 보정하세요.

---

파일 출처:

- 기존 `README.md` 버전 섹션
- 디버깅 히스토리: `HISTORY_OTA.md`
