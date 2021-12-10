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
#include <bluetooth/sdp.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>  

//#include "btio.h"

#include "scanner_util.h"
#include "scanner_connect.h"
#include "scanner.h"

#define SCANNER_CONNECT_DBG_MIN
#ifdef SCANNER_CONNECT_DBG_MIN
  #define DBG_MIN(fmt...) do { pdebug(&lu0cfg, __FUNCTION__, fmt); } while(0)
#else
  #define DBG_MIN(fmt...) do { } while(0)
#endif
//#define SCANNER_CONNECT_DBG_MAX
#ifdef SCANNER_CONNECT_DBG_MAX
  #define DBG_MAX(fmt...) do { pdebug(&lu0cfg, __FUNCTION__, fmt); } while(0)
#else
  #define DBG_MAX(fmt...) do { } while(0)
#endif

static pthread_t smThread, createConnTh, readRemoteVerTh;
static pthread_mutex_t mutex;
static CONN_STATEM CONNsm;
static unsigned int CONNtmo;
static le_advertising_info *le_adv_inf;

static union _dsm_map map;

void scanner_connect_init(void * param) {
  _Settings *pSettings = (_Settings *)param;

  CONNtmo=CONN_TH_WAIT_TMO;
  CONNsm=CONN_SM_WAIT;
	map.bit_vars.b_task=TRUE;
  map.bit_vars.bEnableFrameParsing=FALSE;

  le_adv_inf=(le_advertising_info *)malloc(sizeof(le_advertising_info)+128);
  pthread_create(&smThread, NULL, scanner_connectThread, (void *)pSettings);
}

void *scanner_connectThread(void *param) {
  _Settings *pSettings = (_Settings *)param;
	int nTMOCnt=0;

	while(map.bit_vars.b_task) {
    SLEEPMS(CONNtmo);    // waiting sensors filling data...
    switch (CONNsm) {
      case CONN_SM_WAIT:
        //nTMOCnt++;
        if (((nTMOCnt++) % 4) == 0) { DBG_MIN("CONN_SM_WAIT"); }
        //CONNtmo=250;
        break;

      case CONN_SM_CREATECONN:
        DBG_MIN("CONN_SM_CREATECONN");
        pthread_create(&createConnTh, NULL, createConn_Th, (void *)pSettings);
        CONNsm=CONN_SM_WAITCONN;
        break;
        
      case CONN_SM_WAITCONN:
        if (((nTMOCnt++) % 4) == 0) { DBG_MIN("CONN_SM_WAITCONN"); }
        break;
        
      case CONN_SM_CONNCOMPLETE:
        DBG_MIN("CONN_SM_CONNCOMPLETE hci_dev %d handle %d", pSettings->hci_dev, pSettings->handle);
        pthread_create(&readRemoteVerTh, NULL, readRemoteVer_Th, (void *)pSettings);
        CONNsm=CONN_SM_WAITDATA;
        break;

      case CONN_SM_WAITDATA:
        if (((nTMOCnt++) % 4) == 0) { DBG_MIN("CONN_SM_WAITDATA"); }
        break;

      case CONN_SM_DISCONN:
        DBG_MIN("CONN_SM_DISCONN");
        #if 1
        //int hci_le_conn_update(int dd, uint16_t handle, uint16_t min_interval,uint16_t max_interval, uint16_t latency,uint16_t supervision_timeout, int to);
        if (pSettings->handle) {
          DBG_MIN("hci_disconnect");
          hci_disconnect(pSettings->hci_dev, pSettings->handle, HCI_OE_USER_ENDED_CONNECTION, CONN_TH_WAIT_TMO-10);
          pSettings->handle=0;
          }
        #endif
        CONNsm=CONN_SM_WAIT;
        break;

      case CONN_SM_EVT_READ_REMOTE_VERSION_COMPLETE:
        DBG_MIN("CONN_SM_EVT_READ_REMOTE_VERSION_COMPLETE");
        CONNsm=CONN_SM_WAITDATA;
        break;

      case CONN_SM_CONNFAILED:
        DBG_MIN("CONN_SM_CONNFAILED");
        CONNsm=CONN_SM_WAIT;
        break;

      case CONN_SM_BADFORCEDBIT:
        DBG_MIN("CONN_SM_BADFORCEDBIT");
        break;
      case CONN_SM_UNEXPECDATATYPE:
        DBG_MIN("CONN_SM_UNEXPECDATATYPE");
        break;
      case CONN_SM_END:
        DBG_MIN("CONN_SM_END");
        break;
      default:
        DBG_MIN("unexpected CONNsm %d", CONNsm);
        break;
      }
    }
  DBG_MIN("end.");
  pthread_exit(NULL) ;        // terminate thread
  return(NULL);
}

void scanner_connect_end(void * param) {
  //int retcode ;
  //void * retval ;
  //_Settings *pSettings = (_Settings *)param;

  map.bit_vars.b_task=FALSE;
  //if ( (retcode = pthread_join(frameThread, &retval)) ) {}
  SLEEPMS(200);
}

void set_scanner_sm(CONN_STATEM newstate) {
  pthread_mutex_lock(&mutex);
  CONNsm=newstate;
  pthread_mutex_unlock(&mutex);
}

CONN_STATEM get_scanner_sm() {
  CONN_STATEM currentstate;
  pthread_mutex_lock(&mutex);
  currentstate=CONNsm;
  pthread_mutex_unlock(&mutex);
  return currentstate;
}

void *createConn_Th(void *param) {
  _Settings *pSettings = (_Settings *)param;
	int nRes, nTent=40;

  while (nTent--) {
    DBG_MIN("nTent %d", nTent);
    nRes = hci_le_create_conn(pSettings->hci_dev, 
                              htobs(0x0500),  /* interval ( htobs(0x0004) )*/
                              htobs(0x0400),  /* window  ( < interval) ( htobs(0x0004) )*/
                              0,              /* initiator filter */
                              LE_RANDOM_ADDRESS, 
                              pSettings->BDAddress[3], 
                              LE_PUBLIC_ADDRESS,
                              htobs(0x000F),  /*min_interval*/
                              htobs(0x000F),  /*max_interval*/
                              htobs(20),       /*latency*/
                              //htobs(CREATE_CONN_TMO/10),  /*supervision_timeout 0xC80==32000 ms, 3E8 == 10000 ms, 1F4 == 5000 ms */ 
                              htobs(0x1F4),  /*supervision_timeout 0xC80==32000 ms, 3E8 == 10000 ms, 1F4 == 5000 ms */ 
                              htobs(0x0000),  /* min connection length */
                              htobs(0x0000),  /* max connection length */
                              &pSettings->handle, 
                              2000);             /* tmo before exit (if 0 wait forever) */
    if ( nRes < 0 ) {
      DBG_MIN("hci_le_create_conn, fails (%d)", nRes);
      //if (get_scanner_sm()!=CONN_SM_WAITCONN) break;
      continue;
      }
    else {
      DBG_MIN("hci_le_create_conn, ok (%d)", nRes);
      set_scanner_sm(CONN_SM_CONNCOMPLETE);
      break;
      }
    }
  pthread_exit(NULL) ;        // terminate thread
  return(NULL);
}

void *readRemoteVer_Th(void *param) {
  _Settings *pSettings = (_Settings *)param;
  char *ver;
  uint8_t features[8];
  struct hci_version version;
  //BtIOConnect connect_cb;
	int nRes, nTent=2;

  while (nTent--) {
    DBG_MIN("nTent %d", nTent);
    nRes=hci_read_remote_version(pSettings->hci_dev, pSettings->handle, &version, 2000);
    if (nRes == 0) {
        ver = lmp_vertostr(version.lmp_ver);
        printf("LMP Version: %s (0x%x) LMP Subversion: 0x%x\nManufacturer: %s (%d)\n", ver ? ver : "n/a", version.lmp_ver, version.lmp_subver, bt_compidtostr(version.manufacturer),version.manufacturer);
        if (ver)
          bt_free(ver);
      bzero(features, sizeof(features));
      nRes=hci_le_read_remote_features(pSettings->hci_dev, pSettings->handle, features, 2000);
      if ( nRes < 0) {
        DBG_MIN("hci_le_read_remote_features fail: nRes %d", nRes);
        break;
        }
      else {
        printf("\tFeatures: 0x%2.2x 0x%2.2x 0x%2.2x 0x%2.2x 0x%2.2x 0x%2.2x 0x%2.2x 0x%2.2x\n", features[0], features[1], features[2], features[3], features[4], features[5], features[6], features[7]);
        break;
        }
      }
    else {
      DBG_MIN("Can't read_remote_version %s %d", strerror(errno), errno); 
      continue;
      }
    }
  set_scanner_sm(CONN_SM_DISCONN);
  pthread_exit(NULL) ;        // terminate thread
  return(NULL);
}
