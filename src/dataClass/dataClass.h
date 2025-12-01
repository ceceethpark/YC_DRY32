// dataClass.h - 데이터 관리 클래스

#ifndef DATACLASS_H
#define DATACLASS_H

#include <Arduino.h>
#include <Preferences.h>
#include "../typedef.h"

// 전역 변수 선언
extern CURRENT_DATA gCUR;

// 상수 정의
#define COOLING_TIME 5  // 냉각 시간 (분)

class dataClass {
public:
    dataClass();
    
    // 초기화
    void begin();
    void clear();
    
    // NTC 온도 읽기
    float readNTCtempC();
    
    // ADC 필터링 함수
    float get_m0_filter(int adc);
    
    // SHT30 온도/습도 읽기
    float readSHT30tempC();   // 온도 (℃)
    float readSHT30humidity(); // 습도 (%)
    
    // Flash 저장/로드
    void saveToFlash();
    void loadFromFlash();
    void clearFlash();
    
    // 콜백 함수
    void onSecondElapsed();
    void onMinuteElapsed();
    
    // 히터 온도 제어
    void controlHeater();
    
    // 팬 전류 감시
    void checkFanCurrent();
    
    // 과열 감지 (PIN_OH)
    void checkOverheat();
    
private:
    Preferences _preferences;  // Flash 저장용
    
    // 온도 필터링용
    float _temp_buffer[10];  // 이동 평균 버퍼
    uint8_t _temp_buffer_idx;
    float _last_filtered_temp;
    bool _temp_initialized;
    
    // ADC raw 값 필터링용
    int _adc_buffer[10];     // ADC raw 이동 평균 버퍼
    uint8_t _adc_buffer_idx; // ADC 버퍼 인덱스
    bool _adc_initialized;   // ADC 버퍼 초기화 플래그
    
    // 팬 지연 제어용
    bool _cooling_mode;           // 냉각 모드 플래그
    uint16_t _cooling_minutes;    // 냉각 남은 시간 (분)
    bool _fan_started;            // 팬 시작 플래그
    uint16_t _fan_start_delay;    // 팬 시작 지연 카운터 (초)
    
    // 팬 전류 감시용
    uint16_t _fan_error_count;    // 팬 에러 카운터
    void measure_fan_current();
    void measureAndFilterTemp(); // 온도 측정 및 필터링
};

#endif // DATACLASS_H
