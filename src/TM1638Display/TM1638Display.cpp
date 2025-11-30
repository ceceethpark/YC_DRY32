// TM1638Display.cpp - TM1638 FND 디스플레이 제어 클래스 구현

#include "TM1638Display.h"
#include "../dataClass/dataClass.h"
#include "../config.h"

//#define LSBFIRST 1
#define REVISION "H2TT0002"  // 버전 정보

#define TM_ACTIVATE  0x8F // Start up
#define TM_WRITE_LOC 0x44 // Write to a location
#define TM_WRITE_INC 0x40 // Incremental write
#define TM_SEG_ADR   0xC0  // leftmost segment Address C0 C2 C4 C6 C8 CA CC CE
#define TM_BRIGHT_ADR  0x88 // Brightness address
#define TM_BRIGHT_MASK 0x07 // Brightness mask
#define TM_DEFAULT_BRIGHTNESS 0x07 //can be 0x00 to 0x07
#define TM_DISPLAY_SIZE 10 //size of display
#define TM_READ_KEY 0x42

// 7세그먼트 테이블 (0-9, A-Z, a-z)
const uint8_t TM1638Display::DIGITS_TABLE[] = {
  0x3F, /* 0 */   0x06, /* 1 */   0x5B, /* 2 */   0x4F, /* 3 */
  0x66, /* 4 */   0x6D, /* 5 */   0x7D, /* 6 */   0x07, /* 7 */
  0x7F, /* 8 */   0x6F, /* 9 */   0x77, /* A */   0x7C, /* B */
  0x39, /* C */   0x5E, /* D */   0x79, /* E */   0x71, /* F */
  0x3D, /* G */   0x76, /* H */   0x30, /* I */   0x1E, /* J */
  0x75, /* K */   0x38, /* L */   0x15, /* M */   0x37, /* N */
  0x3F, /* O */   0x73, /* P */   0x6B, /* Q */   0x33, /* R */
  0x6D, /* S */   0x78, /* T */   0x3E, /* U */   0x3E, /* V */
  0x2A, /* W */   0x76, /* X */   0x6E, /* Y */   0x5B, /* Z */
  0x39, /* [ */   0x64, /* \ */   0x0F, /* ] */   0x23, /* ^ */
  0x08, /* _ */   0x02, /* ` */   0x5F, /* a */   0x7C, /* b */
  0x58, /* c */   0x5E, /* d */   0x7B, /* e */   0x71, /* f */
  0x6F, /* g */   0x74, /* h */   0x10, /* i */   0x0C, /* j */
  0x75, /* k */   0x30, /* l */   0x14, /* m */   0x54, /* n */
  0x5C, /* o */   0x73, /* p */   0x67, /* q */   0x50, /* r */
  0x6D, /* s */   0x78, /* t */   0x1C, /* u */   0x1C, /* v */
  0x14, /* w */   0x76, /* x */   0x6E, /* y */   0x5B, /* z */
};

TM1638Display::TM1638Display(uint8_t pinSTB, uint8_t pinCLK, uint8_t pinDIO, uint8_t brightness)
  : _pinSTB(pinSTB), _pinCLK(pinCLK), _pinDIO(pinDIO), _brightness(brightness) {
  memset(_displaySegment, 0, sizeof(_displaySegment));
}

void TM1638Display::begin() {
  pinMode(_pinSTB, OUTPUT);
  pinMode(_pinCLK, OUTPUT);
  pinMode(_pinDIO, OUTPUT);

  digitalWrite(_pinSTB, HIGH);
  digitalWrite(_pinCLK, HIGH);
  digitalWrite(_pinDIO, LOW);
  
  delay(100);  // 충분한 대기

  Serial.printf("TM1638 init: STB=%d CLK=%d DIO=%d Brightness=%d\n", _pinSTB, _pinCLK, _pinDIO, _brightness);

  // 1. 데이터 설정: 자동 증가 모드(0x40)
  sendCommand(0x40);
  delay(1);
  
  // 2. 디스플레이 클리어
  displayClear();
  
  // 3. 디스플레이 ON + 설정된 밝기
  sendCommand(0x88 | _brightness);
  
  Serial.println("TM1638 initialized");
}

void TM1638Display::shiftOut(uint8_t bitOrder, uint8_t val)
{
    uint8_t i;

    for (i = 0; i < 8; i++)  {
        if (bitOrder == LSBFIRST)
            digitalWrite(_pinDIO, !!(val & (1 << i)));
        else
            digitalWrite(_pinDIO, !!(val & (1 << (7 - i))));

        digitalWrite(_pinCLK, HIGH);
        digitalWrite(_pinCLK, LOW);
    }
}


void TM1638Display::sendData(uint8_t data)
{
  shiftOut(LSBFIRST, data);
}

void TM1638Display::displayClear() {
  sendCommand(0x40); // 자동 증가 모드
  
  digitalWrite(_pinSTB, LOW);
  writeByte(0xC0);   // 시작 주소 0
  
  for (uint8_t position = 0; position < 16; position++) {
    writeByte(0x00); // 모든 세그먼트 클리어
  }
  
  digitalWrite(_pinSTB, HIGH);
}

void TM1638Display::brightness(uint8_t brightness)
{
  uint8_t  value = 0;
  value = TM_BRIGHT_ADR + (TM_BRIGHT_MASK & brightness);
  sendCommand(value);
}

void TM1638Display::clear() {
  for (int i = 0; i < 8; i++) {
    setDigitRaw(i, 0x00);
  }
}

// void TM1638Display::displayTemp(int temp_x10) {
//   if (temp_x10 < 0) temp_x10 = 0;
//   if (temp_x10 > 9999) temp_x10 = 9999;

//   int d0 = temp_x10 % 10;         // 소수점 첫째 자리
//   int d1 = (temp_x10 / 10) % 10;  // 1의 자리
//   int d2 = (temp_x10 / 100) % 10; // 10의 자리
//   int d3 = (temp_x10 / 1000) % 10;// 100의 자리

//   // 표시 위치는 0,2,4,6만 사용 (TM1638는 짝수 위치가 세그먼트)
//   setDigitRaw(0, DIGITS_TABLE[d0]);           // 소수점 첫째 자리
//   setDigitRaw(2, DIGITS_TABLE[d1] | 0x80);    // 1의 자리 + 소수점 표시 (DP)
//   setDigitRaw(4, DIGITS_TABLE[d2]);           // 10의 자리
//   setDigitRaw(6, DIGITS_TABLE[d3]);           // 100의 자리
// }

// void TM1638Display::displayStateChar(char ch) {
//   uint8_t seg = 0x00;
//   switch (ch) {
//     case 'I': seg = 0b00000110; break;    // I 비슷하게
//     case 'H': seg = 0b01110110; break;    // H
//     case 'C': seg = DIGITS_TABLE[12]; break; // C
//     case 'E': seg = DIGITS_TABLE[14]; break; // E
//     default:  seg = 0x00; break;
//   }
//   // pos 1에 상태 문자 표시
//   setDigitRaw(1, seg);
// }

// void TM1638Display::setNumber(uint8_t pos, int num, uint8_t len) {
//   for (int i = len - 1; i >= 0; i--) {
//     _displaySegment[pos + i] = DIGITS_TABLE[num % 10];
//     num /= 10;
//   }
// }
void TM1638Display::setNumber(uint8_t pos, int num, uint8_t len) {
//  printf("Pos %i = %i\r\n", pos, num);
  for (int i = len - 1; i >= 0; i--) {
    _displaySegment[pos + i] = DIGITS_TABLE[num % 10];
//    printf("Digit %i = %i\r\n", pos+i, num % 10);
    num /= 10;
  }
}
// void TM1638Display::setString(uint8_t pos, const char* string, uint8_t len) {
//   for (int i = 0; i < len; i++) {
//     if (string[i] < 0x30 || string[i] > 0x7a) {
//       _displaySegment[pos + i] = 0;
//     } else {
//       _displaySegment[pos + i] = DIGITS_TABLE[string[i] - 48];
//     }
//   }
// }
void TM1638Display::setString(uint8_t pos, const char* string, uint8_t len) {
  for (int i = 0; i < len; i++) {
    char c = string[i];
    uint8_t index = 0;
    
    if (c >= '0' && c <= '9') {
      // 숫자 0-9 → 인덱스 0-9
      index = c - '0';
    } else if (c >= 'A' && c <= 'Z') {
      // 대문자 A-Z → 인덱스 10-35
      index = c - 'A' + 10;
    } else if (c >= 'a' && c <= 'z') {
      // 소문자 a-z → 인덱스 36-61
      index = c - 'a' + 36;
    } else {
      // 그 외 문자는 공백
      _displaySegment[pos + i] = 0;
      continue;
    }
    
    _displaySegment[pos + i] = DIGITS_TABLE[index];
  }
}

void TM1638Display::setDot(uint8_t num, bool dot) {
  if (dot) {
    _displaySegment[num] = _displaySegment[num] | 0x80;
  } else {
    _displaySegment[num] = _displaySegment[num] & 0x7F;
  }
}

// void TM1638Display::setLED(uint8_t ledData) {
//   _displaySegment[8] = ledData;
// }

void TM1638Display::displayTime(uint16_t minute) {
  uint8_t hour = minute / 60;
  uint8_t min = minute % 60;
  if (hour > 99) hour = 99;
  
  setNumber(4, hour, 2);
  setNumber(6, min, 2);
}

void TM1638Display::displayDualTemp(uint16_t temp, uint16_t setTemp) {
  uint8_t t = temp;
  uint8_t s = setTemp;
  if (t > 99) t = 99;
  if (s > 99) s = 99;
  
  setNumber(0, t, 2);
  setNumber(2, s, 2);
}

void TM1638Display::setDigitRaw(uint8_t pos, uint8_t segments) {
  sendCommand(0x44); // 고정 주소 모드
  
  digitalWrite(_pinSTB, LOW);
  writeByte(0xC0 + (pos * 2));  // 주소는 2씩 증가 (C0, C2, C4, ...)
  writeByte(segments);
  digitalWrite(_pinSTB, HIGH);
}

void TM1638Display::setBrightness(uint8_t brightness) {
  _brightness = brightness & 0x0F;
  sendCommand(0x80 | _brightness);
}

void TM1638Display::writeBit(bool bitVal) {
  //digitalWrite(_pinCLK, LOW);
  digitalWrite(_pinDIO, bitVal ? HIGH : LOW);
  digitalWrite(_pinCLK, HIGH);
  digitalWrite(_pinCLK, LOW);
}

void TM1638Display::writeByte(uint8_t data) {
  for (int i = 0; i < 8; i++) {
    digitalWrite(_pinCLK, LOW);
    digitalWrite(_pinDIO, (data & 0x01) ? HIGH : LOW);
    data >>= 1;
    digitalWrite(_pinCLK, HIGH);
  }
}

void TM1638Display::sendCommand(uint8_t cmd) {
  digitalWrite(_pinSTB, LOW);
  writeByte(cmd);
  digitalWrite(_pinSTB, HIGH);
}

// uint8_t TM1638Display::receiveByte() {
//   uint8_t temp = 0;
//   pinMode(_pinDIO, INPUT);
  
//   for (int i = 0; i < 8; i++) {
//     temp >>= 1;
//     digitalWrite(_pinCLK, LOW);
//     if (digitalRead(_pinDIO)) {
//       temp |= 0x80;
//     }
//     digitalWrite(_pinCLK, HIGH);
//   }
  
//   pinMode(_pinDIO, OUTPUT);
//   return temp;
// }

uint8_t TM1638Display::receive()
{
  uint8_t temp = 0;
  
  for (int i = 0; i < 8; i++) {
    temp >>= 1;
    digitalWrite(_pinCLK, LOW);
    
    if (digitalRead(_pinDIO)) {
      temp |= 0x80;
    }
    
    digitalWrite(_pinCLK, HIGH);
  }
  
  return temp;
}

bool TM1638Display::getBit(uint8_t pos, uint8_t bit) {
  return (_displaySegment[pos] >> bit) & 0x01;
}

// void TM1638Display::sendToDisplay() {
//   uint8_t displayGrid[16] = {0};
  
//   // 세그먼트 데이터를 그리드로 변환
//   for (int seg = 0; seg < 16; seg++) {
//     bool right = (seg >= 8);
//     for (int grid = (right ? 1 : 0); grid < 16; grid += 2) {
//       displayGrid[grid] |= getBit(seg, grid / 2) << (seg % 8);
//     }
//   }
  
//   // 전체 디스플레이 업데이트
//   sendCommand(0x40); // 자동 증가 모드
//   digitalWrite(_pinSTB, LOW);
//   writeByte(0xC0); // 주소 0부터 시작
  
//   for (int i = 0; i < 16; i++) {
//     writeByte(displayGrid[i]);
//   }
  
//   digitalWrite(_pinSTB, HIGH);
// }

void TM1638Display::setLED() {
  _displaySegment[8] = gCUR.led.data;  // LED 상태 반영
}


void TM1638Display::displaySegments(uint8_t segment, uint16_t digit)
{
  // sendCommand()는 begin()에서 한 번만 호출됨
  // 여기서는 STB 제어와 데이터 전송만
  digitalWrite(_pinSTB, LOW);
  writeByte(TM_SEG_ADR | segment);  // 주소 0xC0-0xCE
  writeByte(digit);  // 데이터
  digitalWrite(_pinSTB, HIGH);
}

void TM1638Display::sendToDisplay() {
  static bool sec_bling_flag = false;
  static unsigned long last_blink_ms = 0;
  static uint8_t debug_cnt = 0;
  
  // 1초마다 깜빡임 플래그 토글
  unsigned long now = millis();
  if (now - last_blink_ms >= 1000) {
    last_blink_ms = now;
    sec_bling_flag = !sec_bling_flag;
  }
  
  char string[3];
  memset(_displaySegment,0,sizeof(_displaySegment));
  setLED();
  
  // 10번마다 디버그 출력
  // if (++debug_cnt >= 100) {
  //   debug_cnt = 0;
  //   Serial.printf("Display: start=%d soft_off=%d error=0x%02X\n", 
  //                 system_start_flag, gCUR.flg.soft_off, gCUR.error_info.data);
  // }
  
  // 에러가 있으면 항상 에러 표시 우선 (시작 중에도)
  if(gCUR.error_info.data){
      //memset(displaySegment,0,sizeof(displaySegment));
      if(gCUR.error_info.fan_error)           sprintf(string, "E1");
      else if(gCUR.error_info.heater1_error||gCUR.error_info.heater2_error)   sprintf(string,"E2");
      else if(gCUR.error_info.thermist_open)  sprintf(string, "E3");
      else if(gCUR.error_info.thermist_short) sprintf(string, "E4");
      else if(gCUR.error_info.thermo_state)    sprintf(string, "E5");
      else if(gCUR.error_info.HiT_error)    sprintf(string, "HI");
      else if(gCUR.error_info.mem_error)    sprintf(string, "E7");
      setString(0,string, 2);
      //printf("\r\n\r\n@@@@something Err!![%x]\r\n",gCUR.error_info.data);
  }
  else if(system_start_flag && !gCUR.flg.soft_off){
    // 에러 없고 시작 중일 때만 REVISION 표시
    setString(0, REVISION + 0, 2); // "H2"
    setString(2, REVISION + 2, 2); // "TT"
    setString(4, REVISION + 4, 2); // "00"
    setString(6, REVISION + 6, 2); // "02"
  }
  else{
    if(gCUR.error_info.data){
      //memset(displaySegment,0,sizeof(displaySegment));
      if(gCUR.error_info.fan_error)           sprintf(string, "E1");
      else if(gCUR.error_info.heater1_error||gCUR.error_info.heater2_error)   sprintf(string,"E2");
      else if(gCUR.error_info.thermist_open)  sprintf(string, "E3");
      else if(gCUR.error_info.thermist_short) sprintf(string, "E4");
      else if(gCUR.error_info.thermo_state)    sprintf(string, "E5");
      else if(gCUR.error_info.HiT_error)    sprintf(string, "HI");
      else if(gCUR.error_info.mem_error)    sprintf(string, "E7");
      setString(0,string, 2);
      //printf("\r\n\r\n@@@@something Err!![%x]\r\n",gCUR.error_info.data);
    }
    else {
      //if(seljung_time_out>0)displayDualTemp(gCUR.disp_current_temp/10, gCUR.seljung_temp);
      //else displayDualTemp(gCUR.disp_current_temp/10, gCUR.disp_current_humidity/10);
      
      // 디버그: 표시할 값 출력
      // if (debug_cnt == 0) {
      //   Serial.printf("Temp=%d SetTemp=%d Time=%d min\n", 
      //                 gCUR.control_temp/10, gCUR.seljung_temp, gCUR.current_minute);
      // }
      
      displayDualTemp((uint16_t)gCUR.measure_ntc_temp, gCUR.seljung_temp);
      displayTime(gCUR.current_minute);
      if(sec_bling_flag)setDot(7,true);
      else setDot(7,false);
    }
  }

  if(gCUR.flg.soft_off){
    memset(_displaySegment,0,sizeof(_displaySegment));
    _displaySegment[8]=gCUR.led.data&0x02;
  }

  uint8_t displayGrid[16] = {
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0
  };

  // for (i = 0; i < 16; i++) Serial.printf("%0.2x ", displaySegment[i]);
  // Serial.print("\n");
  bool right = 0;
  for (int seg = 0; seg < 16; seg++) {
    if (seg >= 8) right = 1; //checking if we're on the left or right half of the table
    else right = 0;
    for (int grid = 0 + right; grid < 16; grid += 2) {
      displayGrid[grid] |= getBit(seg, grid/2) << seg % 8;
    }
  }
  // Serial.print("\n");
  // for (i = 0; i < 16; i++) Serial.printf("%0.2x ", displayGrid[i]);
  
  // 디버그: 세그먼트와 그리드 데이터 출력
  // if (debug_cnt == 0) {
  //   Serial.print("Segment: ");
  //   for (int i = 0; i < 8; i++) {
  //     Serial.printf("%02X ", _displaySegment[i]);
  //   }
  //   Serial.print("\nGrid: ");
  //   for (int i = 0; i < 16; i++) {
  //     Serial.printf("%02X ", displayGrid[i]);
  //   }
  //   Serial.println();
  // }
  
  for (int i=0; i < 16; i++) {
    //tm.DisplaySegments(i, displayGrid[i]);
    displaySegments(i, displayGrid[i]);
  }
}

//=============================================
// 부저 제어 함수 (50ms 비프음)
void TM1638Display::beep() {
  digitalWrite(PIN_BEEP, HIGH);
  delay(50);
  digitalWrite(PIN_BEEP, LOW);
}

//=============================================
// 전원 버튼 처리 함수 (디바운싱 없음)
void TM1638Display::processPowerButton() {
  static uint8_t last_pwr_state = HIGH;  // 이전 상태 저장
  
  uint8_t current_pwr_state = digitalRead(PIN_PWR_SW);
  
  // 상태 변화 감지 (HIGH -> LOW 또는 LOW -> HIGH)
  if (current_pwr_state != last_pwr_state) {
    if (current_pwr_state == LOW) {
      // 버튼 눌림 (LOW): 파워 ON
      gCUR.flg.soft_off = 0;
      printf("Power Button: ON\n");
    } else {
      // 버튼 떼짐 (HIGH): 파워 OFF
      gCUR.flg.soft_off = 1;
      printf("Power Button: OFF\n");
    }
    last_pwr_state = current_pwr_state;
  }
}

//=============================================
void  TM1638Display::key_process(){
  static uint8_t ex_key=0;
  static uint16_t key_press_cnt=0;
  static unsigned long last_key_time = 0;  // 키 디바운싱용
  static unsigned long last_save_time = 0; // Flash 저장용
  static bool need_save = false;
  uint8_t beef_on=0;
  uint8_t idamper;
  uint8_t key = getButtons();
  int lval;
  int max_temp=1000;//MAX_TEMP;
  // 3초 경과 후 Flash 저장
  if (need_save && (millis() - last_save_time >= 3000)) {
    extern dataClass gData;
    gData.saveToFlash();
    need_save = false;
    printf("Settings saved to flash\n");
  }
  if(gCUR.flg.soft_off){
    return;
  }
  if(key ==KEY_NULL) {
    ex_key=0; key_press_cnt=0;
    //mode_key_cnt=0;
    return;
  }
  
  // 키 디바운싱: 같은 키를 누르고 있으면 일정 시간 대기
  if(ex_key == key) {
    key_press_cnt++;
    // 처음 누른 후 300ms 대기, 이후 100ms마다 반복
    if(key_press_cnt == 1) {
      last_key_time = millis();
      // 첫 입력은 바로 처리 (아래로 계속)
    } else {
      unsigned long elapsed = millis() - last_key_time;
      if(elapsed < 300) {
        return;  // 첫 입력 후 300ms 대기
      } else if((elapsed - 300) % 100 < 20) {
        // 300ms 이후부터 100ms마다 반복
        last_key_time = millis();
      } else {
        return;  // 아직 반복 타이밍 아님
      }
    }
  } else {
    // 새로운 키 입력
    key_press_cnt = 1;
    last_key_time = millis();
  }

#ifdef DEBUG
    printf("###key[%d]\r\n",key);
  // printf("[%d]err_release_key_buf[%d][%d][%d][%d]\r\n",key err_release_key_buf[0], err_release_key_buf[1], err_release_key_buf[2], err_release_key_buf[3]);
#endif

  switch(key){
    case KEY_123:
//       if(ex_key==key) key_press_cnt++;
//       else key_press_cnt=0;
//       if(key_press_cnt>10){
//        beef_on=1;
//        key_press_cnt=0;
// #ifdef DEBUG
//       printf("KEY_123!!!\r\n");
// #endif
//       err_release_flag=1;
// //        if(err_release_flag) gCUR.flg.process_seq=PROCESS_ING;
//       }
      break;
    case KEY_TEMP_UP:
    case KEY_TEMP_DN:
      // 온도 설정 변경
      lval = gCUR.seljung_temp;
      lval = (key == KEY_TEMP_UP) ? lval + 1 : lval - 1;
      
      // 범위 제한 (0~70도)
      if (lval > 70) lval = 70;
      if (lval < 0) lval = 0;
      
      gCUR.seljung_temp = lval;
      
      // 3초 후 Flash 저장 예약
      last_save_time = millis();
      need_save = true;
      
      beep();  // 부저 소리
      printf("Temp set: %d C\n", gCUR.seljung_temp);
      break;

    case KEY_TIME_UP:
    case KEY_TIME_DN:
      {
        // 시간 설정 변경 (30분 단위, 00분 또는 30분으로 정렬)
        lval = gCUR.current_minute;
        
        // 현재 값을 30분 단위로 정렬 (올림)
        int remainder = lval % 30;
        if (remainder != 0) {
          lval = lval - remainder + 30;  // 다음 30분 단위로 올림
        }
        
        // 증가/감소
        lval = (key == KEY_TIME_UP) ? lval + 30 : lval - 30;
        
        // 범위 제한 (0~12000분)
        if (lval > 12000) lval = 12000;
        if (lval < 0) lval = 0;
        
        gCUR.current_minute = lval;
        
        // 3초 후 Flash 저장 예약
        last_save_time = millis();
        need_save = true;
        
        beep();  // 부저 소리
        printf("Time set: %d min\n", gCUR.current_minute);
      }
      break;
    case KEY_MODE:
       //  gCUR.flg.soft_off=(gCUR.flg.soft_off)? 0:1;
      break;
    case KEY_DAMPER:
      // 댐퍼 자동 모드 토글 (0: 수동, 1: 자동)
      idamper = gCUR.auto_damper;
      idamper++;
      idamper %= 2;  // 0 또는 1로 토글
      gCUR.auto_damper = idamper;
      
      // LED 업데이트
      if(gCUR.auto_damper) {
        // 자동 모드: AUTO LED 켜짐, OPEN/CLOSE는 히터 상태에 따라 결정됨
        gCUR.led.damper_auto = 1;
        // 초기값으로 OPEN 설정 (실제 상태는 controlHeater()에서 업데이트)
        gCUR.led.damper_open = 1;
        gCUR.led.damper_close = 0;
      } else {
        // 수동 모드: 댐퍼 열림 표시
        gCUR.led.damper_auto = 0;
        gCUR.led.damper_open = 1;
        gCUR.led.damper_close = 0;
        digitalWrite(PIN_DAMP, LOW);  // 댐퍼 열림(0)
      }
      
      // 3초 후 Flash 저장 예약
      last_save_time = millis();
      need_save = true;
      
      beep();  // 부저 소리
      printf("DAMPER mode: %s\n", gCUR.auto_damper ? "AUTO" : "MANUAL");
      break;
  }
  //one_ms_tick=0;
 // if(beef_on && Beep_flag==0) beep_on();
  ex_key=(uint8_t)key;
#ifdef DEBUG
 // printf("--key_process key[%d] key_press[%d]\r\n", key, key_press);
#endif
}


#define ADC_SENSITIVITY 0.01f
float TM1638Display::get_m0_filter(int adc)
{
  static float m0_value;
  m0_value=(m0_value*(1-ADC_SENSITIVITY))+(adc*ADC_SENSITIVITY);
  return m0_value;
}

uint8_t TM1638Display::getButtons(void)
{
  // 전원 버튼 처리 (디바운싱 없음)
  processPowerButton();
  avr_NTC1 = get_m0_filter(analogReadMilliVolts(PIN_NTC1)); 
  KEY_BUF key_buf;
  uint8_t key=KEY_NULL;
///////////////////////////////////
  digitalWrite(_pinSTB, LOW);
  writeByte(0x42);
  pinMode(_pinDIO, INPUT);  // 외부 풀업 사용
  delayMicroseconds(50);
  
  for (int i = 0; i < 4; i++) {
    key_buf.data[i] = receive();
  }
  
  pinMode(_pinDIO, OUTPUT);
  digitalWrite(_pinSTB, HIGH);
///////////////////////////////////
  // 각 바이트의 비트를 확인하여 키 결정
  uint32_t all_bits = (key_buf.data[3] << 24) | (key_buf.data[2] << 16) | (key_buf.data[1] << 8) | key_buf.data[0];
  
  // 모든 키 데이터 출력 (디버그)
  static uint8_t last_key0=0, last_key1=0, last_key2=0, last_key3=0;
  if(key_buf.data[0] != last_key0 || key_buf.data[1] != last_key1 || 
     key_buf.data[2] != last_key2 || key_buf.data[3] != last_key3) {
    if(key_buf.data[0] || key_buf.data[1] || key_buf.data[2] || key_buf.data[3]) {
      printf(">>> KEY: %02X %02X %02X %02X (bits: ", key_buf.data[0], key_buf.data[1], key_buf.data[2], key_buf.data[3]);
      // 어떤 비트가 켜졌는지 출력
      for(int i=0; i<32; i++) {
        if(all_bits & (1 << i)) printf("b%d ", i);
      }
      printf(")\r\n");
    }
    last_key0 = key_buf.data[0];
    last_key1 = key_buf.data[1];
    last_key2 = key_buf.data[2];
    last_key3 = key_buf.data[3];
  }
  
  // 실제 하드웨어 매핑에 따라 수정
  // TIME_DN: 0x02 (b1), TIME_UP: 0x20 (b5)
  // 나머지 키들은 실제 테스트 필요
  
  if(key_buf.b.b1){  // 0x02
    key=KEY_TIME_DN;
  }
  if(key_buf.b.b5){  // 0x20
    key=KEY_TIME_UP;
  }
  if(key_buf.b.b6){  // 0x40 예상
    key=KEY_TEMP_DN;
  }
  if(key_buf.b.b2){  // 0x04 예상
    key=KEY_TEMP_UP;
  }
  if(key_buf.b.b9){  // DAMPER 키 (b0은 노이즈)
     key=KEY_DAMPER;
  }
//  if(key_buf.b.b10){
//    key=KEY_MODE;
//    //printf("#6\r\n");
//  }
#if 1

  //tempup/tempdn/timedn
  if(key_buf.b.b6 && key_buf.b.b2 && key_buf.b.b18){
    key=KEY_123;
    printf("#KEY_123\r\n");
  }
  // PWR_KEY는 이제 하드웨어 스위치(PIN_PWR_SW)로 처리됨
 #endif 
  return key;
}
