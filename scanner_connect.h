#ifndef __SCANNER_CONNECT_H
#define __SCANNER_CONNECT_H

#include "stdint.h"
#include "stdbool.h"

#define CONN_TH_WAIT_TMO     500
#define CONN_MAXDATALENGTH        255
#define CREATE_CONN_TMO     20000

typedef enum _CONN_STATEM {
	CONN_SM_WAIT,
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

typedef enum _CONN_FRAMESTAT {
  CONN_SEARCHING_START_OF_FRAME,
  CONN_ON_FRAME,

  CONN_ON_7E_OR_7D_ESC_FIRSTVAL,
  CONN_GO_PARSE_FRAME,
  CONN_RESTART_SEARCHING,
} CONN_FRAMESTAT;

struct _dsm_bits {
  uint8_t b_task  : 1;
  CONN_FRAMESTAT FrameStat  : 4;
  uint8_t bEnableFrameParsing  : 1;  
  uint8_t spare  : 2;
};

union _dsm_map {
  uint8_t bits;             // data input as 8-bit char
  struct _dsm_bits bit_vars;      // data output as single bit field.
};

void scanner_connect_init(void * param);
void *scanner_connectThread(void *param);
void scanner_connect_end(void * param);

void set_scanner_sm(CONN_STATEM newstate);
CONN_STATEM get_scanner_sm();
void *createConn_Th(void *param);
void *readRemoteVer_Th(void *param);

#endif

