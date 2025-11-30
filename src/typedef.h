#pragma once

typedef union _RELAY_DATA {
    struct {
      uint8_t RY1:1;//compressor
      uint8_t RY2:1;//heater
      uint8_t RY3:1;//fan
      uint8_t RY4:1;//damper
      uint8_t RY5:1;//reserved
      uint8_t RY6:1;//reserved
      uint8_t RY7:1;//reserved
      uint8_t RY8:1;//reserved
    };
    uint8_t u8;
} RELAY_DATA;

typedef struct {
//  u8 damper_close:1;
  uint8_t fan:1;
  uint8_t heater:1;
  uint8_t open_damper:1;
//  u8 mode_current_seq:4;
}bCTRL;

typedef struct {
  uint8_t EEPROM_save_flag:2;  //0:x 1:time 2:temp 3:time&temp temp(with pgm, step, ..etc)
  uint8_t blink:1;
  uint8_t sec_toggle:1;
  uint8_t finish_flag:1;
  uint8_t open_high_temp_flag:1;
  uint8_t process_seq:4;
  uint8_t fnd_temp_time_display_on:1;
  uint8_t soft_off;
}bFLAG;

typedef struct {
  uint8_t heater:1;        //C
  uint8_t fan:1;           //B
  uint8_t ilban:1;
  uint8_t papper:1;
  uint8_t goggam:1;
  uint8_t damper_auto:1;   //E
  uint8_t damper_open:1;   //
  uint8_t damper_close:1;   //F
}bLED;

typedef union _LED_INFO
{
  bLED b;
  uint8_t data;
}LED_INFO;

typedef union _ERROR_INFO
{
  struct {
    uint8_t thermist_short:1;
    uint8_t thermist_open:1;
    uint8_t thermo_state:1;
    uint8_t fan_error:1;
    uint8_t heater1_error:1;
    uint8_t heater2_error:1;
    uint8_t HiT_error:1;
    uint8_t mem_error:1;
  };
  uint8_t data;
}ERROR_INFO;

typedef union _EEPROM_DATA
{
  uint8_t  data[4];
  uint32_t d32;
}EEPROM_DATA;

typedef struct
{
  uint8_t temperature;
  uint16_t duration;//min
  uint8_t damper; //0:Auto 1:열림 2:닫힘
}CTRL_OBJECT;

typedef struct
{
  uint8_t step;
  CTRL_OBJECT node[16];
}MODE_TABLE;


// 이벤트 비트 정의
typedef struct {
  uint8_t POWER_ON:1;
  uint8_t HIGH_PLASMA_ALARM:1;
  uint8_t HIGH_TEMPER_ALARM:1;
  uint8_t LOW_TEMPER_ALARM:1;

  uint8_t LOW_HUMIDITY_ALARM:1;
  uint8_t TEMPERATURE_SENSOR_ALARM:1;
  uint8_t COMP_CURRENT_ALARM:1;
  uint8_t EVAFAN_CURRENT_ALARM:1;

  uint8_t EVAHEATER_CURRENT_ALARM:1;
  uint8_t ICE_ALARM:1;
  uint8_t OCR_ALARM:1;
  uint8_t EVAHEATER_HTC_ALARM:1;

  uint8_t SUPER_COOL_ALARM:1;
  uint8_t CHANGGO_OPEN_DOOR:1;
  uint8_t JAESANG_ON:1;
  uint8_t NO_SENSOR_EVENT:1;
}EVENT_BIT;

typedef union _UNION_EVENT
{
  EVENT_BIT bEvent;
  uint16_t u16;
}UNION_EVENT;

typedef struct _PUBLISH_EVENT
{
  UNION_EVENT uEvent;
  UNION_EVENT ex_uEvent;
  UNION_EVENT xor_uEvent;
  uint16_t event_state;
}PUBLISH_EVENT;

typedef struct
{
  uint8_t auto_damper;
  int seljung_temp;//0~255
  int control_temp;
  int ex_current_temp;
 
  float measure_ntc_temp;
  float sht30_temp;
  float sht30_humidity; 

  uint16_t system_sec;
  uint16_t key_adc;
  uint16_t ntc_adc;
  uint16_t humidity_adc;
  uint16_t total_remain_minute;
  uint16_t current_minute;
  uint16_t fan_current;
//  uint16_t fan_pending_cnt;
//  uint16_t finish_cnt;
 // uint16_t adc[6];
  RELAY_DATA relay_state;
  LED_INFO led;
  bCTRL ctrl;
  bFLAG flg;
  ERROR_INFO error_info;
  PUBLISH_EVENT pubEvent;  // 이벤트 상태
}CURRENT_DATA;