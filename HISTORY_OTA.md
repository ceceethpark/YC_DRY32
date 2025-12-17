개발 히스토리 및 OTA 진단 요약

- 날짜: 2025-12-16 — 프로젝트 점검 및 OTA 안정화 작업

1) 주요 변경사항 및 검사 내용
- `platformio.ini`에 OTA 환경 추가 및 `upload_protocol = espota` 설정
- `src/ota_only.cpp`(OTA 전용 테스터) 추가하여 USB로 먼저 올려 기기에서 OTA 서비스 확인
- `src/main.cpp`에 `ArduinoOTA.begin()` 및 `ArduinoOTA.handle()`를 루프 초기에 호출하도록 수정하여 OTA 응답성 보장
- PlatformIO/espota 업로드가 Windows 호스트에서 간헐적으로 0%에서 실패하는 문제를 진단 (인증 OK 후 전송 실패)
- Arduino IDE 및 Windows용 `espota.exe` 직접 실행에서 OTA 정상 완료 확인 → 기기측 OTA 서비스는 정상
- 호스트 네트워크 환경 점검: Windows에서 mDNS(`DryerESP32.local`)가 작동함(ping 성공)
- `platformio.ini`에 Windows용 `espota.exe` 직접 호출을 위한 `upload_command` 추가 및 `upload_port`에 mDNS 이름 사용 테스트

2) 현재 상태
- OTA 무선 업로드 정상 동작 확인(Arduino IDE 및 Windows `espota.exe` 직접 실행에서 성공)
- `esp32dev` 및 `ota` 환경에서 빌드 및 OTA 동작 점검 완료
- MQTT 및 온도 제어 로직은 코드에서 구현되어 있으며, 다음 하드웨어 테스트 권장: MQTT 연결/구독 확인, 원격 명령(PWR/TMP) 테스트, 과열 및 팬 전류 감시 동작 검증

3) 권장 다음 단계
- `OTA_PASSWORD` 및 AWS 인증 키 관리 정책 개선(코드에 키 포함 금지)
- `parseCommand` 파싱 로직을 더 견고한 JSON 파서로 대체 검토
- 팬 전류 임계값 및 센서 캘리브레이션 수행
- PlatformIO `upload_command` 정리 후 표준 업로더로 재검증

4) 노트
- 원하시면 README.md에 이 내용을 직접 병합해 드리겠습니다. 현재 적용 중인 자동 편집에서 README 수정이 실패하여 별도 파일로 보관했습니다.
