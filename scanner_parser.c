#include <stdio.h>
#include <pth.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <syslog.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <sys/ioctl.h>
#include <sys/stat.h>  
#include <sys/time.h>  
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>

#include "scanner_util.h"
#include "scanner.h"
#include "scanner_parser.h"

#define SCANNER_PARSER_DBG_MIN
#ifdef SCANNER_PARSER_DBG_MIN
  #define DBG_MIN(fmt...) do { pdebug(&lu0cfg, __FUNCTION__, fmt); } while(0)
#else
  #define DBG_MIN(fmt...) do { } while(0)
#endif
//#define SCANNER_PARSER_DBG_MAX
#ifdef SCANNER_PARSER_DBG_MAX
  #define DBG_MAX(fmt...) do { pdebug(&lu0cfg, __FUNCTION__, fmt); } while(0)
#else
  #define DBG_MAX(fmt...) do { } while(0)
#endif

static pthread_t frameThread;
static pthread_mutex_t mutex;
static SC_STATEM SCsm;
static unsigned int SCtmo;
static le_advertising_info *le_adv_inf;

_ble_data ble_data;
static union _dsm_map map;

void scanner_parser_init(void * param) {
  _Settings *pSettings = (_Settings *)param;

  SCtmo=SC_TH_WAIT_TMO;
  SCsm=SC_SM_WAIT;
	map.bit_vars.b_task=TRUE;
  map.bit_vars.FrameStat=SC_SEARCHING_START_OF_FRAME;
  map.bit_vars.bEnableFrameParsing=FALSE;

  le_adv_inf=(le_advertising_info *)malloc(sizeof(le_advertising_info));
  //le_adv_inf->data=(uint8_t *)malloc(128);
  pthread_create(&frameThread, NULL, scanner_parserThread, (void *)pSettings);
}

void *scanner_parserThread(void *param) {
  _Settings *pSettings = (_Settings *)param;
	unsigned char TxBuffer[SC_MAXDATALENGTH];
	unsigned int TxBufLen;

	while(map.bit_vars.b_task) {
    SLEEPMS(SCtmo);    // waiting sensors fill data...
    switch (SCsm) {
      case SC_SM_WAIT:
        DBG_MAX("SC_SM_WAIT");
        SCtmo=250;
        if (map.bit_vars.bEnableFrameParsing) {
          SCsm=SC_SM_PARSEBUFFER;
          }
        break;

      case SC_SM_PARSEBUFFER:
        DBG_MAX("SC_SM_PARSEBUFFER");
        switch(scanner_frame_parser()) {
          case SC_PARSEBUFFER_NO_ANSW:
            break;
          case SC_PARSEBUFFER_WARNRPT_ACK:
            DBG_MIN("Send WARNRPT_ACK");
            //TxBufLen=buildWarningRptAck(TxBuffer);
            //scanner_parser_sendbuffer(pSettings, TxBuffer, TxBufLen);
            break;
          case SC_PARSEBUFFER_ERROR:
          default:
            DBG_MIN("SC_PARSEBUFFER Unexpected");
            break;
          }
        map.bit_vars.bEnableFrameParsing=FALSE;
        SCsm=SC_SM_WAIT;
        break;
        
      case SC_SM_QUERY:
        DBG_MIN("SC_SM_QUERY");
        //TxBufLen=buildQueryFrame(TxBuffer);
        scanner_parser_sendbuffer(pSettings, TxBuffer, TxBufLen);
        SCsm=SC_SM_WAIT;
        break;

      case SC_SM_RESTOREDEFAULTS:
        DBG_MIN("SC_SM_RESTOREDEFAULTS");
        //TxBufLen=buildRestore_CmdFrame(TxBuffer);
        scanner_parser_sendbuffer(pSettings, TxBuffer, TxBufLen);
        SCsm=SC_SM_WAIT;
        break;

      case SC_SM_RT_DATACMD:
        DBG_MIN("SC_SM_RT_DATACMD");
        //TxBufLen=buildRT_CmdFrame(TxBuffer);
        scanner_parser_sendbuffer(pSettings, TxBuffer, TxBufLen);
        SCsm=SC_SM_WAIT;
        break;

      case SC_SM_QUERY_BASIC:
        DBG_MIN("SC_SM_QUERY_BASIC");
        //TxBufLen=buildQueryBasicFrame(TxBuffer);
        scanner_parser_sendbuffer(pSettings, TxBuffer, TxBufLen);
        SCsm=SC_SM_WAIT;
        break;

      case SC_SM_SETSYSPARAM:
        DBG_MIN("SC_SM_SETSYSPARAM");
        //TxBufLen=buildSetSysParam_CmdFrame(TxBuffer);          
        scanner_parser_sendbuffer(pSettings, TxBuffer, TxBufLen);
        SCsm=SC_SM_WAIT;
        break;

      case SC_SM_QUERY_STATE:
        DBG_MIN("SC_SM_QUERY_STATE");
        //TxBufLen=buildQueryStateFrame(TxBuffer);          
        scanner_parser_sendbuffer(pSettings, TxBuffer, TxBufLen);
        SCsm=SC_SM_WAIT;
        break;

      case SC_SM_MMQUERYCMD:
        DBG_MIN("SC_SM_MMQUERYCMD");
        //TxBufLen=buildMultimediaREQFrame(TxBuffer);          
        scanner_parser_sendbuffer(pSettings, TxBuffer, TxBufLen);
        SCsm=SC_SM_WAIT;
        break;

      case SC_SM_MMEDIA_DATA:
        DBG_MIN("SC_SM_MMEDIA_DATA");
        //TxBufLen=buildMMediaDataFrame(TxBuffer);          
        scanner_parser_sendbuffer(pSettings, TxBuffer, TxBufLen);
        SCsm=SC_SM_WAIT;
        break;

      case SC_SM_TAKESNAPSHOT:
        DBG_MIN("SC_SM_TAKESNAPSHOT");
        //TxBufLen=buildTakeSnapshotFrame(TxBuffer);          
        scanner_parser_sendbuffer(pSettings, TxBuffer, TxBufLen);
        SCsm=SC_SM_WAIT;
        break;

      case SC_SM_BADHEADERREPLY:
        DBG_MIN("SC_SM_BADHEADERREPLY");
        break;
      case SC_SM_BADFORCEDBIT:
        DBG_MIN("SC_SM_BADFORCEDBIT");
        break;
      case SC_SM_UNEXPECDATATYPE:
        DBG_MIN("SC_SM_UNEXPECDATATYPE");
        break;
      case SC_SM_END:
        DBG_MIN("SC_SM_END");
        break;
      }
    }
  DBG_MIN("end.");
	return 0;
}

void scanner_parser_end(void * param) {
  //int retcode ;
  //void * retval ;
  //_Settings *pSettings = (_Settings *)param;

  map.bit_vars.b_task=FALSE;
  //if ( (retcode = pthread_join(frameThread, &retval)) ) {}
  SLEEPMS(200);
}


void scanner_parser_sendbuffer(void * param, void * buf, int len) {
  //_Settings *pSettings = (_Settings *)param;
  #if 0
  int i;
  printf("SC tx: ");
  for(i=0 ; i<len; i++) {
    printf("0x%02X, ", *(((unsigned char *)(buf))+i));
    }
  printf("\n");
  #endif
  //write(pSettings->idComDev, buf, len);
  //fflush(pSettings->idComDev);
}

void get_ble_data(void * param) {
  _Settings *pSettings = (_Settings *)param;

  pthread_mutex_lock(&mutex);
  memcpy(pSettings->ble_data, &ble_data, sizeof(_ble_data));
  pthread_mutex_unlock(&mutex);
}

void set_ble_data(void * param) {
  _Settings *pSettings = (_Settings *)param;

  pthread_mutex_lock(&mutex);
  memcpy(&ble_data, pSettings->ble_data, sizeof(_ble_data));
  pthread_mutex_unlock(&mutex);
}

void set_ble_sm(SC_STATEM newstate) {
  pthread_mutex_lock(&mutex);
  SCsm=newstate;
  pthread_mutex_unlock(&mutex);
}

int ble_fill_rxbuf(le_advertising_info * le_adv_info) {
  _Settings *pSettings = (_Settings *)lu0cfg.Settings;
  bdaddr_t tmpbdadd;

  DBG_MAX(".");
  str2ba(pSettings->BDAddress, &tmpbdadd );
  if ( ! memcmp(&(le_adv_info->bdaddr), &tmpbdadd, sizeof(bdaddr_t)) ) {
    #if 0
    char addr[18];
    ba2str(&(le_adv_info->bdaddr), addr);
    DBG_MIN("addr %s", addr);
    printf("MAC %s, RSSI %d, data", addr, (int8_t)le_adv_info->data[le_adv_info->length]);
    for (int n = 0; n < le_adv_info->length; n++) {
      printf(" %02X", (unsigned char)le_adv_info->data[n]);
      }
    printf("\n");
    #else
    memcpy(le_adv_inf, le_adv_info, sizeof(le_advertising_info));
    memcpy(&le_adv_inf->data, le_adv_info->data, le_adv_info->length);
    map.bit_vars.bEnableFrameParsing=TRUE;    // NOT OVERLAP over other state machine states
    DBG_MAX("le_adv_inf.data[0] 0x%02X, le_adv_inf.length %d", le_adv_inf->data[0], le_adv_inf->length);
    #endif
    return TRUE;
    }
  return FALSE;
}


SC_PARSEBUFFER scanner_frame_parser() {
  _Settings *pSettings = (_Settings *)lu0cfg.Settings;
  SC_PARSEBUFFER nRet=SC_PARSEBUFFER_NO_ANSW;
  unsigned char n, idx=0, len=0, frame=0;

  #if 0
  for (int n = 0; n < le_adv_inf->length; n++) {
    printf(" %02X", (unsigned char)le_adv_inf->data[n]);
    }
  printf("\n\n");
  #endif
  DBG_MAX("n %d, frame 0x%02X, le_adv_inf->length %d", n, le_adv_inf->data[n], le_adv_inf->length);
  printf("MAC %s, RSSI %d\n", pSettings->BDAddress, (int8_t)le_adv_inf->data[le_adv_inf->length]);
  while ( idx<le_adv_inf->length ) {
    len=le_adv_inf->data[idx++];
    n=0;
    frame++;
    printf("len %d ->", len);
    while ((n++)<len) {
      switch (frame) {
        case 1:
          printf(" %02X", (unsigned char)le_adv_inf->data[idx++]);
          break;
        
        case 2:
          printf(" %02X", (unsigned char)le_adv_inf->data[idx++]);
          break;

        case 3:
          printf("%c", (unsigned char)le_adv_inf->data[idx++]);
          break;

        default:
          break;
        }
      }
    printf("\n");
    }

	return nRet;
}
