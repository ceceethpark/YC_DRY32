// TM1638Display.h - TM1638 FND 디스플레이 제어 클래스

#pragma once
#include <Arduino.h>

// 전역 변수 extern 선언
extern bool system_start_flag;

typedef enum _KEY_MAP{
  KEY_NULL=0,
  KEY_MODE,
  KEY_TEMP_UP,
  KEY_TEMP_DN,
  KEY_TIME_UP,
  KEY_TIME_DN,
  KEY_DAMPER,
  KEY_123,
}KEY_MAP;

typedef struct {
  uint8_t b0:1;
  uint8_t b1:1;
  uint8_t b2:1;
  uint8_t b3:1;

  uint8_t b4:1;
  uint8_t b5:1;
  uint8_t b6:1;
  uint8_t b7:1;
 //------------
  uint8_t b8:1;
  uint8_t b9:1;
  uint8_t b10:1;
  uint8_t b11:1;

  uint8_t b12:1;
  uint8_t b13:1;
  uint8_t b14:1;
  uint8_t b15:1;
 //-------------
  uint8_t b16:1;
  uint8_t b17:1;
  uint8_t b18:1;
  uint8_t b19:1;

  uint8_t b20:1;
  uint8_t b21:1;
  uint8_t b22:1;
  uint8_t b23:1;
 //-------------
  uint8_t b24:1;
  uint8_t b25:1;
  uint8_t b26:1;
  uint8_t b27:1;
  uint8_t b28:1;
  uint8_t b29:1;
  uint8_t b30:1;
  uint8_t b31:1;
}bKEY;
typedef union _KEY_BUF
{
  bKEY b;
  uint8_t data[4];
}KEY_BUF;

class TM1638Display {
public:
  // 생성자
  TM1638Display(uint8_t pinSTB, uint8_t pinCLK, uint8_t pinDIO, uint8_t brightness = 0x07);
  
  // 초기화
  void begin();
  
  // 화면 지우기
  void clear();
  
  // 온도 표시 (xx.x 형태, temp_x10: 온도*10)
  //void displayTemp(int temp_x10);
  
  // 상태 문자 표시 (I, H, C, E 등)
  //void displayStateChar(char ch);
  
  // 원시 세그먼트 데이터 설정
  void setDigitRaw(uint8_t pos, uint8_t segments);
  
  // 밝기 설정 (0x00 ~ 0x07)
  void setBrightness(uint8_t brightness);
  
  // 숫자 설정 (pos: 시작위치, num: 숫자, len: 자릿수)
  void setNumber(uint8_t pos, int num, uint8_t len);
  
  // 문자열 설정 (pos: 시작위치, string: 문자열, len: 길이)
  //void setString(uint8_t pos, const char* string, uint8_t len);
  
  // 소수점 설정 (num: 위치, dot: true/false)
  void setDot(uint8_t num, bool dot);
  
  // LED 설정 (ledData: 8비트 LED 데이터)
 // void setLED(uint8_t ledData);
  
  // 키 입력 읽기 (4바이트 키 데이터 반환)
  //uint32_t getButtons();
  

  
  // 시간 표시 (분 단위)
  void displayTime(uint16_t minute);
  // 온도 2개 표시 (현재온도, 설정온도)
  void displayDualTemp(uint16_t temp, uint16_t setTemp);
  void key_process();
  void sendToDisplay();// 전체 디스플레이 업데이트
  void beep();  // 부저 소리 (50ms)
  void processPowerButton();  // 전원 버튼 처리 (디바운싱 없음)
  float  avr_NTC1=0;

private:
  uint8_t _pinSTB;
  uint8_t _pinCLK;
  uint8_t _pinDIO;
  uint8_t _brightness;
  
  // 디스플레이 세그먼트 버퍼 (8자리 + LED)
  uint8_t _displaySegment[16];
  
  // 7세그먼트 테이블 (0-9, A-Z, a-z)
  static const uint8_t DIGITS_TABLE[];
  
  // Low-level 함수
  void writeBit(bool bitVal);
  void writeByte(uint8_t data);
  void sendCommand(uint8_t cmd);
  //uint8_t receiveByte();
  uint8_t receive();
  bool getBit(uint8_t pos, uint8_t bit);
  void brightness(uint8_t brightness);
  void displayClear(void);
  void sendData(uint8_t data);
  void shiftOut(uint8_t bitOrder, uint8_t val);
  void setLED();
  void displaySegments(uint8_t segment, uint16_t digit);
  void setString(uint8_t pos, const char* string, uint8_t len);
  uint8_t getButtons(void);
  //int analogReadAndFilter_ntc1(int adcPin);
  float get_m0_filter(int adc);
};
