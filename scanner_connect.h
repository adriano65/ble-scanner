#ifndef __SCANNER_CONNECT_H
#define __SCANNER_CONNECT_H

#include "stdint.h"
#include "stdbool.h"

#define CONN_TH_WAIT_TMO     10
#define CONN_MAXDATALENGTH        255
#define CREATE_CONN_TMO     25000
#define WAIT_CONN_TMO       CREATE_CONN_TMO/CONN_TH_WAIT_TMO + 1000
#define WAIT_DATA_TMO       CREATE_CONN_TMO/CONN_TH_WAIT_TMO + 1000


typedef enum _CONN_STATEM {
	CONN_SM_WAIT,
  CONN_SM_RESETDONGLE,
  CONN_SM_OPENDEV,
  CONN_SM_SETSCANPARAM,
  CONN_SM_ADDWHITELIST,
  CONN_SM_TOGGLE_SCAN,
  CONN_SM_GET_ADV_DATA, 
  CONN_SM_CREATECONN,
  CONN_SM_WAITCONN,
  CONN_SM_CONNCOMPLETE,
  CONN_SM_WAITDATA,
  CONN_SM_DISCONN,
  CONN_SM_EVT_READ_REMOTE_VERSION_COMPLETE,

  CONN_SM_CONNFAILED,
  CONN_SM_BADFORCEDBIT,
  CONN_SM_UNEXPECDATATYPE,
	CONN_SM_END
} CONN_STATEM;

typedef enum _CONN_PARSEBUFFER {
  CONN_PARSEBUFFER_NO_ANSW,
  CONN_PARSEBUFFER_WARNRPT_ACK,
  CONN_PARSEBUFFER_ERROR,
} CONN_PARSEBUFFER;

struct _conn_bits {
  uint8_t b_task  : 1;
  uint8_t bEnableFrameParsing  : 1;  
  uint8_t bEnableSpare  : 1;  
  uint8_t spare  : 5;
};

union _conn_map {
  uint8_t bits;             // data input as 8-bit char
  struct _conn_bits bit_vars;      // data output as single bit field.
};

void scanner_connect_init(void * param);
void *scanner_connectThread(void *param);
void scanner_connect_end(void * param);

void set_scanner_sm(CONN_STATEM newstate);
CONN_STATEM get_scanner_sm();
void *createConn_Th(void *param);
void *readRemoteVer_Th(void *param);

#endif

