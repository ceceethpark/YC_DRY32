// dataClass.cpp - 데이터 관리 클래스 구현

#include "dataClass.h"
#include "../config.h"  // DEBUG_MODE 매크로 정의

// 전역 변수 정의
CURRENT_DATA gCUR;

dataClass::dataClass() {
    clear();
    memset(&gCUR, 0, sizeof(CURRENT_DATA));
    
    // 온도 필터링 초기화
    _temp_buffer_idx = 0;
    _last_filtered_temp = 0.0f;
    _temp_initialized = false;
    memset(_temp_buffer, 0, sizeof(_temp_buffer));
    
    // 팬 지연 제어 초기화
    _cooling_mode = false;
    _cooling_minutes = 0;
    _fan_started = false;
    _fan_start_delay = 2;  // 2초 후 팬 시작
    
    // 팬 전류 감시 초기화
    _fan_error_count = 0;
}

void dataClass::begin() {
    clear();
}

void dataClass::clear() {
    for (int i = 0; i < 16; i++) {
        _data[i] = 0;
    }
}

void dataClass::setData(uint8_t index, uint16_t value) {
    if (index < 16) {
        _data[index] = value;
    }
}

uint16_t dataClass::getData(uint8_t index) {
    if (index < 16) {
        return _data[index];
    }
    return 0;
}

// Flash에 데이터 저장
void dataClass::saveToFlash() {
    _preferences.begin("dryer", false);  // namespace "dryer", read/write mode
    
    // gCUR 데이터 저장
    _preferences.putUShort("cur_minute", gCUR.current_minute);
    _preferences.putUShort("sel_temp", gCUR.seljung_temp);
    _preferences.putUChar("damper", gCUR.auto_damper);
    
    _preferences.end();
    printf("Data saved to flash\n");
}

// Flash에서 데이터 로드
void dataClass::loadFromFlash() {
    _preferences.begin("dryer", true);  // namespace "dryer", read-only mode
    
    // 데이터 로드 (기본값 제공)
    gCUR.current_minute = _preferences.getUShort("cur_minute", 0);
    gCUR.seljung_temp = _preferences.getUShort("sel_temp", 45);
    gCUR.auto_damper = _preferences.getUChar("damper", 0);
    
    _preferences.end();
    
    // LED 상태 동기화 (auto_damper 값에 따라)
    if (gCUR.auto_damper) {
        gCUR.led.b.damper_auto = 1;
        // 초기 상태: 댐퍼 열림으로 설정
        gCUR.led.b.damper_open = 1;
        gCUR.led.b.damper_close = 0;
    } else {
        gCUR.led.b.damper_auto = 0;
        gCUR.led.b.damper_open = 1;
        gCUR.led.b.damper_close = 0;
        // 수동 모드: 댐퍼 열림
        digitalWrite(PIN_DAMP, LOW);
    }
    
    printf("Data loaded from flash - Time: %d min, Temp: %d C, Damper: %s\n", 
           gCUR.current_minute, gCUR.seljung_temp, 
           gCUR.auto_damper ? "AUTO" : "MANUAL");
}

// Flash 데이터 지우기
void dataClass::clearFlash() {
    _preferences.begin("dryer", false);
    _preferences.clear();
    _preferences.end();
    printf("Flash data cleared\n");
}

// 온도 측정 및 필터링
void dataClass::measureAndFilterTemp() {
    // 온도 측정 (NTC1 핀 사용)
    const int NTC_PIN = 36;  // PIN_NTC1
    float raw_temp = readNTCtempC(NTC_PIN);
    
    // 이동 평균 필터 (10개 샘플)
    _temp_buffer[_temp_buffer_idx] = raw_temp;
    _temp_buffer_idx = (_temp_buffer_idx + 1) % 10;
    
    // 평균 계산
    float sum = 0.0f;
    for (int i = 0; i < 10; i++) {
        sum += _temp_buffer[i];
    }
    float avg_temp = sum / 10.0f;
    
    // 급격한 변화 방지 (변화율 제한: 최대 ±2℃/초)
    if (_temp_initialized) {
        float delta = avg_temp - _last_filtered_temp;
        if (delta > 2.0f) {
            avg_temp = _last_filtered_temp + 2.0f;
        } else if (delta < -2.0f) {
            avg_temp = _last_filtered_temp - 2.0f;
        }
    } else {
        // 첫 10초간 버퍼 채우기
        _temp_initialized = true;
    }
    
    _last_filtered_temp = avg_temp;
    gCUR.measure_ntc_temp = avg_temp;
    
    // printf("Temp: raw=%.2f, filtered=%.2f\n", raw_temp, avg_temp);
}

// 히터 온도 제어 (1도 히스테리시스) + 댐퍼 자동 연동
void dataClass::controlHeater() {
    // 냉각 모드이거나 타이머가 0이면 히터 제어 안함
    if (_cooling_mode || gCUR.current_minute == 0) {
        digitalWrite(PIN_HEATER, LOW);
        
        // 댐퍼 자동 모드일 때: 히터 OFF = 댐퍼 열림(0)
        if (gCUR.auto_damper) {
            digitalWrite(PIN_DAMP, LOW);
            gCUR.led.b.damper_open = 1;
            gCUR.led.b.damper_close = 0;
        }
        return;
    }
    
    // 설정 온도와 측정 온도 가져오기
    float set_temp = (float)gCUR.seljung_temp;
    float measured_temp = gCUR.measure_ntc_temp;
    
    // 히터 상태 읽기 (현재 ON/OFF)
    static bool heater_on = false;
    
    // 히스테리시스 제어 (1도)
    if (measured_temp < set_temp - 1.0f) {
        // 온도가 설정값보다 1도 이상 낮으면 히터 켜기
        heater_on = true;
    } else if (measured_temp > set_temp + 1.0f) {
        // 온도가 설정값보다 1도 이상 높으면 히터 끄기
        heater_on = false;
    }
    // 설정값 ±1도 범위 내에서는 현재 상태 유지 (히스테리시스)
    
    // 히터 릴레이 제어 (GPIO 5)
    digitalWrite(PIN_HEATER, heater_on ? HIGH : LOW);
    
    // 댐퍼 자동 모드일 때: 히터 연동 제어
    if (gCUR.auto_damper) {
        if (heater_on) {
            // 히터 ON = 댐퍼 닫힘(1)
            digitalWrite(PIN_DAMP, HIGH);
            gCUR.led.b.damper_open = 0;
            gCUR.led.b.damper_close = 1;
        } else {
            // 히터 OFF = 댐퍼 열림(0)
            digitalWrite(PIN_DAMP, LOW);
            gCUR.led.b.damper_open = 1;
            gCUR.led.b.damper_close = 0;
        }
    } else {
        // 수동 모드: 댐퍼 항상 열림(0)
        digitalWrite(PIN_DAMP, LOW);
        gCUR.led.b.damper_open = 1;
        gCUR.led.b.damper_close = 0;
    }
    
    // 디버그 출력 (필요시 주석 해제)
    // printf("Heater: %s, Damper: %s (Set=%.1f, Measured=%.1f)\n", 
    //        heater_on ? "ON" : "OFF", 
    //        gCUR.auto_damper ? (heater_on ? "CLOSE" : "OPEN") : "MANUAL",
    //        set_temp, measured_temp);
}

// 초 단위 콜백
void dataClass::onSecondElapsed() {
    extern bool system_start_flag;  // main.cpp에서 선언된 전역 변수
    
    gCUR.system_sec++;
    
    // 시스템 시작 중(3초)에는 제어 루프 스킵 (온도 측정과 과열 감지는 수행)
    if (system_start_flag) {
        printf("onSecondElapsed system_sec[%d] - System starting, skip control\n", gCUR.system_sec);
        // 시작 중에도 온도 측정과 과열 감지는 수행 (에러 체크용)
        measureAndFilterTemp();
        checkOverheat();
        return;
    }
    
    printf("onSecondElapsed system_sec[%d]\n", gCUR.system_sec);
    
    // 팬 시작 지연 처리 (시스템 시작 후 2초)
    if (!_fan_started) {
        if (_fan_start_delay > 0) {
            _fan_start_delay--;
        } else {
            // 2초 경과 후 팬 켜기
            digitalWrite(PIN_FAN, HIGH);
            _fan_started = true;
            printf("Fan started after 2 seconds\n");
        }
    }
    
    measureAndFilterTemp();  // 온도 측정 및 필터링
    checkOverheat();         // 과열 감지 (매초)
    controlHeater();         // 히터 제어
    
#ifndef DEBUG_MODE
    checkFanCurrent();       // 팬 전류 감시 (디버그 모드에서는 스킵)
#endif
}

// 팬 전류 감시
void dataClass::checkFanCurrent() {
    const int FAN_CURRENT_THRESHOLD = 100;  // ADC 임계값 (조정 필요)
    const int ERROR_COUNT_MAX = 3;  // 3초 연속 에러
    
    // 팬이 켜져 있는지 확인
    bool fan_on = digitalRead(PIN_FAN);
    
    if (fan_on && _fan_started) {
        // 팬이 켜져 있을 때 전류 측정
        int current_adc = analogRead(PIN_FAN_CURRENT);
        
        if (current_adc < FAN_CURRENT_THRESHOLD) {
            // 임계값 이하면 에러 카운터 증가
            _fan_error_count++;
            
            if (_fan_error_count >= ERROR_COUNT_MAX) {
                // 3초 연속 에러면 FAN 에러 플래그 설정
                gCUR.error_info.fan_error = 1;
                printf("FAN ERROR: Current too low (ADC=%d)\n", current_adc);
            }
        } else {
            // 정상이면 에러 카운터 리셋
            _fan_error_count = 0;
            gCUR.error_info.fan_error = 0;
        }
    } else {
        // 팬이 꺼져 있으면 에러 카운터 리셋
        _fan_error_count = 0;
    }
}

// 과열 감지 (PIN_OH: 0=정상, 1=과열 에러)
void dataClass::checkOverheat() {
    // PIN_OH 상태 읽기 (HIGH=1=과열, LOW=0=정상)
    bool overheat = digitalRead(PIN_OH);
    
    if (overheat) {
        // 과열 에러 발생
        if (!gCUR.error_info.thermo_state) {
            gCUR.error_info.thermo_state = 1;
            printf("OVERHEAT ERROR (PIN_OH=1): Turning off FAN and HEATER\n");
        }
        
        // FAN과 HEATER 즉시 OFF
        digitalWrite(PIN_FAN, LOW);
        digitalWrite(PIN_HEATER, LOW);
        
        // 댐퍼 열림 (안전)
        digitalWrite(PIN_DAMP, LOW);
        gCUR.led.b.damper_open = 1;
        gCUR.led.b.damper_close = 0;
        
        // 1초마다 비프음 발생 (50ms ON)
        digitalWrite(PIN_BEEP, HIGH);
        delay(50);
        digitalWrite(PIN_BEEP, LOW);
    } else {
        // 정상 상태
        if (gCUR.error_info.thermo_state) {
            gCUR.error_info.thermo_state = 0;
            printf("Overheat cleared (PIN_OH=0)\n");
        }
    }
}

// 분 단위 콜백
void dataClass::onMinuteElapsed() {
    // 냉각 모드 처리
    if (_cooling_mode) {
        if (_cooling_minutes > 0) {
            _cooling_minutes--;
            printf("Cooling mode - Remaining: %d min\n", _cooling_minutes);
            
            // 냉각 시간 종료
            if (_cooling_minutes == 0) {
                _cooling_mode = false;
                digitalWrite(PIN_FAN, LOW);  // 팬 끄기
                printf("Cooling complete - Fan OFF\n");
            }
        }
        return;  // 냉각 모드에서는 일반 타이머 처리 안함
    }
    
    // 일반 모드 - 건조 시간 카운트다운
    if (gCUR.current_minute > 0) {
        gCUR.current_minute--;
        saveToFlash();  // Flash에 저장
        printf("Drying - Remaining: %d min\n", gCUR.current_minute);
        
        // 남은 시간이 0분이 되면 냉각 모드 시작
        if (gCUR.current_minute == 0) {
            // 히터 끄기
            digitalWrite(PIN_HEATER, LOW);
            printf("Drying complete - Heater OFF\n");
            
            // 팬 5분 지연 시작
            _cooling_mode = true;
            _cooling_minutes = 5;
            printf("Cooling mode started - Fan will run for 5 minutes\n");
        }
    }
}

// NTC 온도 변환 (Steinhart-Hart 근사)
// 5K NTC (Beta=3970), 5K pull-up, 3.3V
float dataClass::readNTCtempC(int adcPin) {
    int raw = analogRead(adcPin);    // 0~4095 (12-bit ADC)
    if (raw <= 0) raw = 1;
    if (raw >= 4095) raw = 4094;

    // ADC 전압 계산 (ESP32 ADC는 3.3V 기준)
    float vADC = (float)raw * 3.3f / 4095.0f;
    
    // NTC 저항 계산 (Voltage Divider: Vout = Vin * R_NTC / (R_PU + R_NTC))
    // R_NTC = R_PU * vADC / (Vin - vADC)
    float rNTC = NTC_PULL_RES * vADC / (3.3f - vADC);

    // Steinhart-Hart 근사: 1/T = 1/T0 + 1/B * ln(R/R0)
    float lnRR0 = log(rNTC / NTC_R25);
    float invT = (1.0f / NTC_T0_K) + (lnRR0 / NTC_BETA);
    float tempK = 1.0f / invT;
    float tempC = tempK - 273.15f;
    
    return tempC;
}
