// dataClass.cpp - 데이터 관리 클래스 구현

#include "dataClass.h"
#include "../config.h"  // DEBUG_MODE 매크로 정의
#include "../TM1638Display/TM1638Display.h"
#include "../updateAPI/updateAPI.h"

// ADC 필터링 상수
#define ADC_SENSITIVITY 0.01f

// 전역 변수 정의
CURRENT_DATA gCUR;
extern TM1638Display gDisplay;

dataClass::dataClass() {
    clear();
    memset(&gCUR, 0, sizeof(CURRENT_DATA));
    
    // 온도 필터링 초기화
    _temp_buffer_idx = 0;
    _last_filtered_temp = 0.0f;
    _temp_initialized = false;
    memset(_temp_buffer, 0, sizeof(_temp_buffer));
    
    // ADC raw 필터링 초기화
    _adc_buffer_idx = 0;
    _adc_initialized = false;
    memset(_adc_buffer, 0, sizeof(_adc_buffer));
    
    // 팬 지연 제어 초기화
    _cooling_mode = false;
    _cooling_minutes = 0;
    _fan_started = false;
    _fan_start_delay = 5;  // 5초 후 팬 시작
    
    // 팬 전류 감시 초기화
    _fan_error_count = 0;
    
    // 건조기 상태 초기화
    gCUR.dry_state = DRY_FINISH;  // 초기 상태: FINISH
}

void dataClass::begin() {
    clear();
}

void dataClass::clear() {
    // 예약됨: 필요시 초기화 코드 추가
}

// ADC 필터링 함수 (지수 이동 평균)
float dataClass::get_m0_filter(int adc) {
    static float m0_value;
    m0_value = (m0_value * (1 - ADC_SENSITIVITY)) + (adc * ADC_SENSITIVITY);
    return m0_value;
}

// Flash에 데이터 저장
void dataClass::saveToFlash() {
    _preferences.begin("dryer", false);  // namespace "dryer", read/write mode
    
    // gCUR 데이터 저장
    _preferences.putUShort("cur_minute", gCUR.remaining_minute);
    _preferences.putUShort("sel_temp", gCUR.seljung_temp);
    _preferences.putUChar("damper", gCUR.auto_damper);
    _preferences.putUChar("soft_off", gCUR.flg.soft_off);  // Power 상타 저장
    _preferences.putUChar("dry_state", (uint8_t)gCUR.dry_state);  // DRY_STATE 저장
    
    _preferences.end();
    const char* state_str[] = {"DRY_RUN", "DRY_COOL", "DRY_FINISH"};
    printf("Data saved to flash - Power: %s, State: %s\n", 
           gCUR.flg.soft_off ? "OFF" : "ON",
           state_str[gCUR.dry_state]);
}

// Flash에서 데이터 로드
void dataClass::loadFromFlash() {
    _preferences.begin("dryer", true);  // namespace "dryer", read-only mode
    
    // 데이터 로드 (기본값 제공)
    gCUR.remaining_minute = _preferences.getUShort("cur_minute", 0);
    gCUR.seljung_temp = _preferences.getUShort("sel_temp", 45);
    gCUR.auto_damper = _preferences.getUChar("damper", 0);
    gCUR.flg.soft_off = _preferences.getUChar("soft_off", 1);  // Power 상타 로드 (기본값: OFF)
    gCUR.dry_state = (DRY_STATE)_preferences.getUChar("dry_state", DRY_FINISH);  // DRY_STATE 로드 (기본값: FINISH)
    
    _preferences.end();
    
    // 전원 투입 시 DRY_COOL 상태였다면 DRY_FINISH로 전환 (냉각 중단된 것으로 간주)
    if (gCUR.dry_state == DRY_COOL) {
        gCUR.dry_state = DRY_FINISH;
        // Flash에 즉시 저장하여 다음 부팅 시에도 FINISH 상태 유지
        _preferences.begin("dryer", false);
        _preferences.putUChar("dry_state", (uint8_t)gCUR.dry_state);
        _preferences.end();
        printf("Power-on: DRY_COOL detected, changed to DRY_FINISH and saved\n");
    }
    
    // LED 상태 동기화 (auto_damper 값에 따라)
    if (gCUR.auto_damper) {
        gCUR.led.damper_auto = 1;
        // 초기 상태: 댐퍼 열림으로 설정
        gCUR.led.damper_open = 1;
        gCUR.led.damper_close = 0;
    } else {
        gCUR.led.damper_auto = 0;
        gCUR.led.damper_open = 1;
        gCUR.led.damper_close = 0;
        // 수동 모드: 댐퍼 열림
        digitalWrite(PIN_DAMP, LOW);
    }
    
    const char* state_str[] = {"DRY_RUN", "DRY_COOL", "DRY_FINISH"};
    printf("Data loaded from flash - Time: %d min, Temp: %d C, Damper: %s, Power: %s, State: %s\n", 
           gCUR.remaining_minute, gCUR.seljung_temp, 
           gCUR.auto_damper ? "AUTO" : "MANUAL",
           gCUR.flg.soft_off ? "OFF" : "ON",
           state_str[gCUR.dry_state]);
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
    gCUR.measure_ntc_temp = readNTCtempC();
    gCUR.sht30_temp = readSHT30tempC();
    gCUR.sht30_humidity = readSHT30humidity();
    printf("NTC: %.1f℃ | SHT30: %.1f℃, %.1f%%, fan current: %d\n", gCUR.measure_ntc_temp, gCUR.sht30_temp, gCUR.sht30_humidity,gCUR.fan_current);
}

// 히터 온도 제어 (1도 히스테리시스) + 댐퍼 자동 연동
void dataClass::controlHeater() {
    // 에러 발생 시 히터 제어 안함 (안전 우선)
    if (gCUR.error_info.data != 0) {
        digitalWrite(PIN_HEATER, LOW);
        gCUR.relay_state.RY2 = 0;  // 히터 OFF
        return;
    }
    
    // 냉각 모드이거나 타이머가 0이면 히터 제어 안함
    if (_cooling_mode || gCUR.remaining_minute == 0) {
        digitalWrite(PIN_HEATER, LOW);
        gCUR.relay_state.RY2 = 0;  // 히터 OFF
        
        // 댑퍼 자동 모드일 때: 히터 OFF = 댑퍼 열림(0)
        if (gCUR.auto_damper) {
            digitalWrite(PIN_DAMP, LOW);
            gCUR.relay_state.RY4 = 0;  // 댑퍼 열림
            gCUR.led.damper_open = 1;
            gCUR.led.damper_close = 0;
        }
        return;
    }
    
    // 설정 온도와 측정 온도 가져오기
    float set_temp = (float)gCUR.seljung_temp;
    float measured_temp = gCUR.measure_ntc_temp;
    
    // 히터 상태 읽기 (현재 ON/OFF)
    static bool heater_on = false;
    
    // 히스테리시스 제어 (HEATER_HYSTERESIS = 0.5도)
    if (measured_temp < set_temp - HEATER_HYSTERESIS) {
        // 온도가 설정값보다 히스테리시스 이상 낮으면 히터 켜기
        heater_on = true;
    } else if (measured_temp > set_temp + HEATER_HYSTERESIS) {
        // 온도가 설정값보다 히스테리시스 이상 높으면 히터 끄기
        heater_on = false;
    }
    // 설정값 녤HEATER_HYSTERESIS 범위 내에서는 현재 상태 유지 (히스테리시스)
    
    // 히터 릴레이 제어 (GPIO 5)
    digitalWrite(PIN_HEATER, heater_on ? HIGH : LOW);
    gCUR.relay_state.RY2 = heater_on ? 1 : 0;  // 히터 상태 기록
    
    // 댓퍼 자동 모드일 때: 히터 연동 제어
    if (gCUR.auto_damper) {
        if (heater_on) {
            // 히터 ON = 댓퍼 닫힘(1)
            digitalWrite(PIN_DAMP, HIGH);
            gCUR.relay_state.RY4 = 1;  // 댓퍼 닫힘
            gCUR.led.damper_open = 0;
            gCUR.led.damper_close = 1;
        } else {
            // 히터 OFF = 댓퍼 열림(0)
            digitalWrite(PIN_DAMP, LOW);
            gCUR.relay_state.RY4 = 0;  // 댓퍼 열림
            gCUR.led.damper_open = 1;
            gCUR.led.damper_close = 0;
        }
    } else {
        // 수동 모드: 댓퍼 항상 열림(0)
        digitalWrite(PIN_DAMP, LOW);
        gCUR.relay_state.RY4 = 0;  // 댓퍼 열림
        gCUR.led.damper_open = 1;
        gCUR.led.damper_close = 0;
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
    measureAndFilterTemp();
    measure_fan_current();  
    // 과열 감지는 항상 수행 (최우선)
    checkOverheat();  

    // Power OFF 상태: FAN, HEATER 모두 OFF
    if (gCUR.flg.soft_off) {
        // Power OFF 시 DRY_RUN 상태면 즉시 DRY_FINISH로 전환 (냉각 과정 생략)
        if (gCUR.dry_state == DRY_RUN) {
            gCUR.dry_state = DRY_FINISH;
            saveToFlash();  // Flash에 저장
            printf("Power OFF: DRY_RUN -> DRY_FINISH\n");
        }
        
        digitalWrite(PIN_HEATER, LOW);
        gCUR.relay_state.RY2 = 0;  // 히터 OFF
        digitalWrite(PIN_FAN, LOW);
        gCUR.relay_state.RY3 = 0;  // 팬 OFF
               
        digitalWrite(PIN_DAMP, LOW); // 댐퍼 열림 (안전)
        gCUR.relay_state.RY4 = 0;  // 댐퍼 열림
        gCUR.led.damper_open = 1;
        gCUR.led.damper_close = 0;
        return;
    }

    // if (system_start_flag) {
    //     printf("onSecondElapsed system_sec[%d] - System starting, skip control\n", gCUR.system_sec);
    //     checkOverheat();
    //     return;
    // }
    
    const char* state_str[] = {"DRY_RUN", "DRY_COOL", "DRY_FINISH"};
    printf("onSecondElapsed system_sec[%d] Remaining_minute[%d] DRY_STATE[%s]\n", gCUR.system_sec,gCUR.remaining_minute, state_str[gCUR.dry_state]);
   
    // 상태 일관성 체크: remaining_minute과 dry_state 동기화
    if (gCUR.dry_state == DRY_FINISH && gCUR.remaining_minute > 0) {
        // 시간이 남았는데 FINISH 상태 → RUN으로 전환
        gCUR.dry_state = DRY_RUN;
        // 팬 즉시 시작 (지연 없음)
        _fan_started = false;
        _fan_start_delay = 0;
        printf("State sync: remaining_minute > 0, DRY_FINISH -> DRY_RUN (Fan will start immediately)\n");
    } else if (gCUR.dry_state == DRY_RUN && gCUR.remaining_minute == 0) {
        // DRY_RUN 상태에서 시간이 00:00이면 DRY_COOL로 즉시 전환
        gCUR.dry_state = DRY_COOL;
        _cooling_minutes = COOLING_TIME;
        printf("Time is 00:00 - Immediate transition to DRY_COOL (%d min)\n", COOLING_TIME);
    }
   
    // 에러 발생 시 제어 루프 중단 (안전 동작 + 부저)
    if (gCUR.error_info.data != 0) {
        // 안전 동작: 히터와 팬 즉시 OFF
        digitalWrite(PIN_HEATER, LOW);
        gCUR.relay_state.RY2 = 0;  // 히터 OFF
        digitalWrite(PIN_FAN, LOW);
        gCUR.relay_state.RY3 = 0;  // 팬 OFF
        
        // 댐퍼 열림 (안전)
        digitalWrite(PIN_DAMP, LOW);
        gCUR.relay_state.RY4 = 0;  // 댐퍼 열림
        gCUR.led.damper_open = 1;
        gCUR.led.damper_close = 0;
        
        // 모든 에러에 대해 1초마다 부저 울림
        digitalWrite(PIN_BEEP, HIGH);
        delay(50);
        digitalWrite(PIN_BEEP, LOW);
        
        printf("ERROR DETECTED - Control loop stopped (error_info: 0x%02X)\n", gCUR.error_info.data);
        return;  // 에러 발생 시 제어 중단
    }
    
    // DRY_STATE에 따른 제어
    switch (gCUR.dry_state) {
        case DRY_RUN:
            // 팬 시작 지연 처리 (부팅 시에만 2초 지연, 상태 전환 시에는 즉시)
            if (!_fan_started) {
                if (_fan_start_delay > 0) {
                    _fan_start_delay--;
                    printf("Fan start delay: %d seconds remaining\n", _fan_start_delay);
                } else {
                    // 지연 완료 후 팬 켜기
                    digitalWrite(PIN_FAN, HIGH);
                    gCUR.relay_state.RY3 = 1;  // 팬 ON
                    _fan_started = true;
                    printf("Fan started\n");
                }
            } else {
                // 팬이 이미 시작된 경우에도 계속 ON 유지
                digitalWrite(PIN_FAN, HIGH);
                gCUR.relay_state.RY3 = 1;  // 팬 ON
            }
             // 정상 동작: 히터 제어
            controlHeater();
            checkFanCurrent();       // 팬 전류 감시 (디버그 모드에서는 스킵)
            break;
            
        case DRY_COOL:
            // 냉각 모드: 히터 OFF, 팬 ON 유지
            digitalWrite(PIN_HEATER, LOW);
            gCUR.relay_state.RY2 = 0;  // 히터 OFF
            digitalWrite(PIN_FAN, HIGH);
            gCUR.relay_state.RY3 = 1;  // 팬 ON
            printf("DRY_COOL: Heater OFF, Fan ON (Remaining: %d min)\n", _cooling_minutes);
            break;
            
        case DRY_FINISH:
            // 완료: 히터 OFF, 팬 OFF
            digitalWrite(PIN_HEATER, LOW);
            gCUR.relay_state.RY2 = 0;  // 히터 OFF
            digitalWrite(PIN_FAN, LOW);
            gCUR.relay_state.RY3 = 0;  // 팬 OFF
            break;
    }
}

// 팬 전류 감시
void dataClass::checkFanCurrent() {
    const int FAN_CURRENT_THRESHOLD = 100;  // ADC 임계값 (조정 필요)
    const int ERROR_COUNT_MAX = 3;  // 3초 연속 에러
    
    // 팬이 켜져 있는지 확인
    //bool fan_on = digitalRead(PIN_PWR_SW);
    
    if (_fan_started) {
        // 팬이 켜져 있을 때 전류 측정
        int current_adc = gCUR.fan_current;
        
        if (current_adc < FAN_CURRENT_THRESHOLD) {
            // 임계값 이하면 에러 카운터 증가
            _fan_error_count++;
            
            if (_fan_error_count >= ERROR_COUNT_MAX) {
                // 3초 연속 에러면 FAN 에러 플래그 설정
                if (!gCUR.error_info.fan_error) {
                    gCUR.error_info.fan_error = 1;
                    printf("FAN ERROR: Current too low (ADC=%d)\n", current_adc);
                }
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
        gCUR.relay_state.RY3 = 0;  // 팬 OFF
        digitalWrite(PIN_HEATER, LOW);
        gCUR.relay_state.RY2 = 0;  // 히터 OFF
        
        // 댓퍼 열림 (안전)
        digitalWrite(PIN_DAMP, LOW);
        gCUR.relay_state.RY4 = 0;  // 댓퍼 열림
        gCUR.led.damper_open = 1;
        gCUR.led.damper_close = 0;
        
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
    switch (gCUR.dry_state) {
        case DRY_RUN:
            // 건조 운전 중 - 시간 카운트다운
            if (gCUR.remaining_minute > 0) {
                gCUR.remaining_minute--;
                saveToFlash();  // Flash에 저장
                printf("DRY_RUN - Remaining: %d min\n", gCUR.remaining_minute);
                
                // 시간이 00:00 도달 → DRY_COOL로 전환
                if (gCUR.remaining_minute == 0) {
                    gCUR.dry_state = DRY_COOL;
                    _cooling_minutes = COOLING_TIME;
                    printf("Time reached 00:00 - Transition to DRY_COOL (%d min)\n", COOLING_TIME);
                }
            }
            break;
            
        case DRY_COOL:
            // 냉각 모드 - COOLING_TIME 카운트다운
            if (_cooling_minutes > 0) {
                _cooling_minutes--;
                printf("DRY_COOL - Remaining: %d min\n", _cooling_minutes);
                
                // 냉각 시간 종료 → DRY_FINISH로 전환
                if (_cooling_minutes == 0) {
                    gCUR.dry_state = DRY_FINISH;
                    printf("Cooling complete - Transition to DRY_FINISH\n");
                }
            }
            break;
            
        case DRY_FINISH:
            // 완료 상태 - 아무것도 안함
            break;
    }
}

 void dataClass::measure_fan_current() {
        // 팬이 켜져 있는지 확인
    if(digitalRead(PIN_PWR_SW)) {
        gCUR.fan_current = analogRead(PIN_FAN_CURRENT);
    }
    else {
        gCUR.fan_current = 500;
    }
    return;
}

// NTC 온도 변환 (Steinhart-Hart 근사)
// 5K NTC (Beta=3970), 5K pull-up, 3.3V
float dataClass::readNTCtempC() {
    // 실시간 raw 값과 필터링된 값 비교
    //int raw_now = analogRead(PIN_NTC1);
    float vADC = gCUR.avr_NTC1/1000.0f;
    //printf("NTC ADC Voltage: %.3f V\n", vADC);

    // NTC Short 검출: 전압이 거의 0V (100mV 이하) 0.2V(약100도) 이하로 수정
    if (vADC < 0.2f) {
        if (!gCUR.error_info.thermist_short) {
            gCUR.error_info.thermist_short = 1;
            gCUR.error_info.thermist_open = 0;
            printf("NTC SHORT ERROR: Voltage too low (%.2fV)\n", vADC);
        }
        return 99.9f;  // 에러 온도값
    }
    
    // NTC Open 검출: 전압이 거의 3.3V (3.2V 이상) -32도 정도
    if (vADC > 3.0f) {
        if (!gCUR.error_info.thermist_open) {
            gCUR.error_info.thermist_open = 1;
            gCUR.error_info.thermist_short = 0;
            printf("NTC OPEN ERROR: Voltage too high (%.2fV)\n", vADC);
        }
        return 99.9f;  // 에러 온도값
    }
    
    // 정상 범위: 에러 플래그 클리어
    gCUR.error_info.thermist_short = 0;
    gCUR.error_info.thermist_open = 0;
    
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

// SHT30 온도 센서 읽기
// 공식: T[℃] = -66.875 + 218.75 * (VT / VDD)
// VDD = 3.3V
// 결과 범위 제한: -20℃ ~ 199℃
float dataClass::readSHT30tempC() {
    const float VDD = 3300.0f;  // 3.3V = 3300mV
    const float TEMP_MIN = -20.0f;  // 최소 온도
    const float TEMP_MAX = 199.0f;  // 최대 온도
    
    // analogReadMilliVolts()는 보정된 전압(mV)을 반환
    int milliVolts = analogReadMilliVolts(PIN_SHT30_T);
    
    // 온도 계산 (원래 공식)
    float tempC = -66.875f + (218.75f * (milliVolts / VDD));
    
    // 범위 제한: -20℃ 이하는 -20℃, 199℃ 이상은 199℃
    if (tempC < TEMP_MIN) tempC = TEMP_MIN;
    if (tempC > TEMP_MAX) tempC = TEMP_MAX;
    
    return tempC;
}

// SHT30 습도 센서 읽기
// 공식: RH = -19.7/0.54 + (100/0.54) * (VRH / VDD)
// 단순화: RH = -36.48 + 185.19 * (VRH / VDD)
// VDD = 3.3V
float dataClass::readSHT30humidity() {
    const float VDD = 3300.0f;  // 3.3V = 3300mV
    
    // analogReadMilliVolts()는 보정된 전압(mV)을 반환
    int milliVolts = analogReadMilliVolts(PIN_SHT30_H);
    float vRH = (float)milliVolts / 1000.0f;  // mV → V
    
    // 습도 계산
    float humidity = -19.7f / 0.54f + (100.0f / 0.54f) * (milliVolts / VDD);
    
    // 습도는 0-100% 범위로 제한
    if (humidity < 0.0f) humidity = 0.0f;
    if (humidity > 100.0f) humidity = 100.0f;
    
    return humidity;
}
