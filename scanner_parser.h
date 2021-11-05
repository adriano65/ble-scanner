#ifndef __SCANNER_PARSER_H
#define __SCANNER_PARSER_H

#include "stdint.h"
#include "stdbool.h"

#define SC_MAXDATALENGTH        255
#define DEFAULTLEN      254     // default string length
#define SC_TH_WAIT_TMO     1000
#define MAXFRAMES_X_RXBUF      18

#define SC_FRAMELEN 1
#define SC_FRAMEHEADERLEN 8
#define SC_MAXFRAMES_XBUF 4

typedef enum _SC_FRAME {
	SC_START_OF_FRAME,
	SC_VERIFY_CODE,
	SC_SEQ_NUM_HI,
	SC_SEQ_NUM_LOW,
	SC_MFG_CODE_HI,    //Aka terminal ID
	SC_MFG_CODE_LOW,
	SC_DRIVERAID_ID,
	SC_FUNC_CODE,
	SC_DATA_CONTENT,
} SC_FRAME;

#define	SC_START_OF_FRAME_VAL	  0x7E
#define	SC_DRIVERAID_ID_VAL	  0x65
#define	SC_DRIVERAID_ID2_VAL	  0x64
#define	SC_END_OF_FRAME_VAL	  0x7E

#define	SC_7E_OR_7D_ESC_FIRSTVAL	  0x7D
#define	SC_7E_ESC_SECONDVAL	0x02
#define	SC_7D_ESC_SECONDVAL	0x01

typedef enum _SC_FUNCTION {
	SC_CMD_QUERY = 0x2F,
	SC_ANSW_QUERY = 0x2F,

	SC_CMD_RESTOREDEFAULTS = 0x30,
	SC_ANSW_RESTOREDEFAULTS = 0x30,

	SC_CMD_SEND_RT_DATA = 0x31,  // note driveraid ID 0x64!!
	// NO SC_ANSW_SEND_RT_DATA !!!!

	SC_CMD_QUERY_BASIC = 0x32,
	SC_ANSW_QUERY_BASIC = 0x32,

	SC_CMD_SETSYSPARAM = 0x35,
	SC_ANSW_SETSYSPARAM = 0x35,

	SC_EVT_WARNING_RPT = 0x36,
	SC_ANSW_WARNING_RPT = 0x36,

	SC_CMD_QUERY_STATE = 0x37,  
	//SC_ANSW_QUERY_STATE = 0x38,    ??? WHAT THE HELL SPECS MEANS ??
	SC_ANSW_QUERY_STATE = 0x37,

	SC_CMD_MULTIMEDIA_REQ = 0x50,
	SC_ANSW_MULTIMEDIA_REQ = 0x50,

	SC_CMD_MMEDIA_DATA = 0x51,
	SC_ANSW_MMEDIA_DATA = 0x51,

	SC_CMD_TAKESNAPSHOT = 0x52,
	SC_ANSW_TAKESNAPSHOT = 0x52,
} SC_FUNCTION;

typedef enum _SC_STATEM {
	SC_SM_WAIT,
  SC_SM_PARSEBUFFER,
  SC_SM_QUERY,
  SC_SM_RESTOREDEFAULTS,
  SC_SM_RT_DATACMD,
  SC_SM_QUERY_BASIC,
  SC_SM_SETSYSPARAM,
  SC_SM_QUERY_STATE,
  SC_SM_MMQUERYCMD,
  SC_SM_MMEDIA_DATA,
  SC_SM_TAKESNAPSHOT,

  SC_SM_BADHEADERREPLY,
  SC_SM_BADFORCEDBIT,
  SC_SM_UNEXPECDATATYPE,
	SC_SM_END
} SC_STATEM;

typedef enum _SC_PARSEBUFFER {
  SC_PARSEBUFFER_NO_ANSW,
  SC_PARSEBUFFER_WARNRPT_ACK,
  SC_PARSEBUFFER_ERROR,
} SC_PARSEBUFFER;

typedef enum _SC_FRAMESTAT {
  SC_SEARCHING_START_OF_FRAME,
  SC_ON_FRAME,

  SC_ON_7E_OR_7D_ESC_FIRSTVAL,
  SC_GO_PARSE_FRAME,
  SC_RESTART_SEARCHING,
} SC_FRAMESTAT;

struct _dsm_bits {
  uint8_t b_task  : 1;
  SC_FRAMESTAT FrameStat  : 4;
  uint8_t bEnableFrameParsing  : 1;  
  uint8_t spare  : 2;
};

union _dsm_map {
  uint8_t bits;             // data input as 8-bit char
  struct _dsm_bits bit_vars;      // data output as single bit field.
};

typedef struct _ble_dat {
  uint8_t sensorDataID[3];
  uint16_t temperature;
  uint16_t humidity;
  uint16_t luminosity;

  uint8_t battery;

} _ble_data;

extern _ble_data ble_data;

void scanner_parser_init(void * param);
void *scanner_parserThread(void *param);
void scanner_parser_end(void * param);

void scanner_parser_sendbuffer(void * param, void * buf, int len);
void get_ble_data(void * settings);
void set_ble_data(void * param);
void set_ble_sm(SC_STATEM newstate);
int ble_show_rxbuf(le_advertising_info * le_adv_info);
int ble_fill_rxbuf(le_advertising_info * le_adv_info);
int rxbuf2frames();
SC_PARSEBUFFER scanner_frame_parser();

unsigned char dsm_escape_TXbuf(unsigned char *buf, unsigned char nStart, unsigned char buflen);

#endif

