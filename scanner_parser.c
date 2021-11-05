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
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>  

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

  le_adv_inf=(le_advertising_info *)malloc(sizeof(le_advertising_info)+128);
  //(uint8_t *)&(le_adv_inf->data)=(uint8_t *)malloc(128);
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
  str2ba(pSettings->BDAddress[0], &tmpbdadd );
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
    memcpy(&le_adv_inf->data, le_adv_info->data, le_adv_info->length+1);
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
  unsigned char idx=0, i, sublen=0, frameCnt=0;

  #if 1
  printf("len %d -> ", le_adv_inf->length); for (int n = 0; n < le_adv_inf->length; n++) { printf(" %02X", (unsigned char)le_adv_inf->data[n]); }
  printf("\n\n");
  #endif
	time_t timenow;
	struct tm * actual;

	time(&timenow) ;       // time since 1970
	actual = localtime(&timenow);

  DBG_MAX("n %d, frame 0x%02X, le_adv_inf->length %d", n, le_adv_inf->data[n], le_adv_inf->length);
  printf("%02d:%02d:%02d -> MAC %s, RSSI %d\n", actual->tm_hour, actual->tm_min, actual->tm_sec, pSettings->BDAddress[0], (int8_t)le_adv_inf->data[le_adv_inf->length]);
  while ( idx<le_adv_inf->length ) {
    sublen=le_adv_inf->data[idx++];
    i=0;
    frameCnt++;
    printf("sublen %d -> ", sublen);
    while (i<sublen) {
      switch (frameCnt) {
        case 1:
          printf(" %02X", (unsigned char)le_adv_inf->data[i+idx]);
          break;
        
        case 2:
          if (le_adv_inf->data[i+idx]==0xFF) {
            i++;
            ble_data.sensorDataID[0]=le_adv_inf->data[i+idx]; i++;
            ble_data.sensorDataID[1]=le_adv_inf->data[i+idx]; i++;
            ble_data.sensorDataID[2]=le_adv_inf->data[i+idx]; i++;
            switch (ble_data.sensorDataID[2]) {
              case 0x03:         // 0x160F03
                ble_data.temperature=le_adv_inf->data[i+idx]+(le_adv_inf->data[i+idx+1]<<8); i+=2;
                ble_data.luminosity=le_adv_inf->data[i+idx]+(le_adv_inf->data[i+idx+1]<<8); i+=2;
                ble_data.battery=le_adv_inf->data[i+idx];
                printf("%02X%02X%02X, temp %.01f, lumin. %d lux, batt. %.01f", ble_data.sensorDataID[0], \
                                                                               ble_data.sensorDataID[1], \
                                                                               ble_data.sensorDataID[2], \
                                                                               (float)ble_data.temperature/10, \
                                                                               ble_data.luminosity, \
                                                                               (float)(ble_data.battery)/10);
                break;
              case 0x05:                
                #if 0
                for (int n = idx; n < idx+sublen-4; n++) { printf(" %02X", (unsigned char)le_adv_inf->data[n]); }
                idx+=sublen-5;
                #else
                ble_data.temperature=le_adv_inf->data[i+idx]+(le_adv_inf->data[i+idx+1]<<8); i+=2;
                i+=2; // tbd
                ble_data.humidity=le_adv_inf->data[i+idx]+(le_adv_inf->data[i+idx+1]<<8); i+=2;
                ble_data.battery=le_adv_inf->data[i+idx];
                printf("%02X%02X%02X, temp %.01f, humid. %.01f, batt. %.01f", ble_data.sensorDataID[0], \
                                                                               ble_data.sensorDataID[1], \
                                                                               ble_data.sensorDataID[2], \
                                                                               (float)ble_data.temperature/10, \
                                                                               (float)ble_data.humidity/10, \
                                                                               (float)(ble_data.battery)/10);
                #endif
                break;
              }
            i=sublen;
            }
          else {
            //printf(" %02X", (unsigned char)le_adv_inf->data[idx++]);
            printf("%c", (unsigned char)le_adv_inf->data[i+idx]);
            }
          break;

        case 3:
          printf("%c", (unsigned char)le_adv_inf->data[i+idx]);
          break;

        default:
          break;
        }
      i++;
      }
    idx+=sublen;
    printf("\n");
    }
  printf("\n");
	return nRet;
}
