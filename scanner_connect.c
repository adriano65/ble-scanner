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

static union _conn_map map;

void scanner_connect_init(void * param) {
  _Settings *pSettings = (_Settings *)param;

  CONNtmo=CONN_TH_WAIT_TMO;
  //CONNsm=CONN_SM_RESETDONGLE;
  CONNsm=CONN_SM_WAIT;  
	map.bit_vars.b_task=TRUE;
  map.bit_vars.bEnableFrameParsing=FALSE;

  le_adv_inf=(le_advertising_info *)malloc(sizeof(le_advertising_info)+128);
  if( pthread_create(&smThread, NULL, scanner_connectThread, (void *)pSettings) != 0) { DBG_MIN("pthread_create error");} 
}

void *scanner_connectThread(void *param) {
  _Settings *pSettings = (_Settings *)param;  
	int nTMOCnt=0;
	char tmpbuf[80];

  SLEEPMS(CONNtmo*20);    // waiting sensors filling data...
	while(map.bit_vars.b_task) {
    SLEEPMS(CONNtmo);
    switch (CONNsm) {
      case CONN_SM_WAIT:
        if (((nTMOCnt++) % 40) == 0) { DBG_MAX("CONN_SM_WAIT nTMOCnt %d", nTMOCnt); }
        break;

      case CONN_SM_RESETDONGLE:
        if (pSettings->hci_dev) hci_close_dev(pSettings->hci_dev);
        sprintf(tmpbuf, "hciconfig hci%0d reset", pSettings->HCIDevNumber);
        DBG_MIN("CONN_SM_RESETDONGLE (%s)", tmpbuf);
        system(tmpbuf);
        //struct hci_request req;
        //hci_reset_req(&req, NULL);
        //set_bit(HCI_RESET, &req->hdev->flags);
        //hci_req_add(req, HCI_OP_RESET, 0, NULL);
        SLEEPMS(1000);
        CONNsm=CONN_SM_OPENDEV;
        break;

      case CONN_SM_OPENDEV:
        DBG_MIN("CONN_SM_OPENDEV");
        if ((pSettings->hci_dev = hci_open_dev(pSettings->HCIDevNumber)) < 0 ) {  // Get HCI device.
          DBG_MIN("Failed to open HCI device.");
          CONNsm=CONN_SM_WAIT;
          } 
        else {
          hci_le_set_scan_enable(pSettings->hci_dev, SCAN_DISABLED, 0, 10000); // disable in case already enabled
          CONNsm=CONN_SM_SETSCANPARAM;
          }
        break;

      case CONN_SM_SETSCANPARAM:
        DBG_MIN("CONN_SM_SETSCANPARAM");
        /* example https://github.com/dlenski/ttblue/blob/master/ttblue.c */
        if (hci_le_set_scan_parameters(pSettings->hci_dev,
                                      0x00, /* passive */
                                      htobs(0x10), 
                                      htobs(0x10), 
                                      LE_PUBLIC_ADDRESS, 
                                      0x00, /* filter_policy = 0x01; --> Whitelist */
                                      1000) < 0) {
          DBG_MIN("Failed to set BLE scan parameters: %s (%d)", strerror(errno), errno);
          CONNsm=CONN_SM_WAIT;
          }
        else {
          //CONNsm=CONN_SM_ADDWHITELIST;
          CONNsm=CONN_SM_TOGGLE_SCAN;          
          }
        break;

      case CONN_SM_ADDWHITELIST:
        DBG_MIN("CONN_SM_ADDWHITELIST");
        hci_le_add_white_list(pSettings->hci_dev, &pSettings->BDAddress[3], LE_PUBLIC_ADDRESS, 1000);
        if (pSettings->map.bit_vars.bScanEn) CONNsm=CONN_SM_TOGGLE_SCAN;
        else CONNsm=CONN_SM_WAIT;
        break;

      case CONN_SM_TOGGLE_SCAN:
        DBG_MIN("CONN_SM_TOGGLE_SCAN (%s)", pSettings->map.bit_vars.bScanEn ? "SCAN_INQUIRY" : "SCAN_DISABLED");
        if (hci_le_set_scan_enable(pSettings->hci_dev,
                                  pSettings->map.bit_vars.bScanEn ? SCAN_INQUIRY : SCAN_DISABLED,
                                  0x00,  /* include dupes */ 
                                  1000) < 0) {
          DBG_MIN("Failed to enable BLE scan: %s (%d)", strerror(errno), errno);
          }
        CONNsm=CONN_SM_WAIT;
        break;

      case CONN_SM_GET_ADV_DATA:
        DBG_MIN("CONN_SM_GET_ADV_DATA");
        //hci_send_cmd(pSettings->hci_dev, )
        CONNsm=CONN_SM_WAIT;
        break;


      case CONN_SM_CREATECONN:
        DBG_MIN("CONN_SM_CREATECONN");
        if (pSettings->createConnTent) { 
          DBG_MIN("Thread is running");
          pSettings->createConnTent=0;
          //pthread_kill(createConnTh, SIGKILL);
          //DBG_MIN("Killed.");
          }
        else {
          DBG_MIN("Creating Thread");
          }
        pthread_create(&createConnTh, NULL, createConn_Th, (void *)pSettings);          
        nTMOCnt=0; CONNsm=CONN_SM_WAITCONN;
        break;
        
      case CONN_SM_WAITCONN:
        if (((nTMOCnt++) % 40) == 0) { DBG_MIN("CONN_SM_WAITCONN nTMOCnt %d", nTMOCnt); }
        if (nTMOCnt>WAIT_CONN_TMO) {
          CONNsm=CONN_SM_WAIT;
          }
        break;
        
      case CONN_SM_CONNCOMPLETE:
        DBG_MIN("CONN_SM_CONNCOMPLETE hci_dev %d handle %d", pSettings->hci_dev, pSettings->handle);
        pthread_create(&readRemoteVerTh, NULL, readRemoteVer_Th, (void *)pSettings);
        nTMOCnt=0; CONNsm=CONN_SM_WAITDATA;
        break;

      case CONN_SM_WAITDATA:
        if (((nTMOCnt++) % 40) == 0) { DBG_MIN("CONN_SM_WAITDATA nTMOCnt %d", nTMOCnt); }

        if ( nTMOCnt>WAIT_DATA_TMO ) {
          DBG_MIN("WAIT_DATA_TMO elapsed"); 
          CONNsm=CONN_SM_WAIT;
          }
        break;

      case CONN_SM_DISCONN:
        DBG_MIN("CONN_SM_DISCONN");
        #if 1
        //int hci_le_conn_update(int dd, uint16_t handle, uint16_t min_interval,uint16_t max_interval, uint16_t latency,uint16_t supervision_timeout, int to);
        if (pSettings->handle) {
          DBG_MIN("hci_disconnect");
          hci_disconnect(pSettings->hci_dev, pSettings->handle, HCI_OE_USER_ENDED_CONNECTION, 5000);
          pSettings->handle=0;
          //pthread_kill(createConnTh, SIGKILL);
          hci_close_dev(pSettings->hci_dev);
          }
        #endif
        CONNsm=CONN_SM_WAIT;
        break;

      case CONN_SM_EVT_READ_REMOTE_VERSION_COMPLETE:
        DBG_MIN("CONN_SM_EVT_READ_REMOTE_VERSION_COMPLETE");
        nTMOCnt=0; CONNsm=CONN_SM_WAITDATA;
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
  void * retval;
  //_Settings *pSettings = (_Settings *)param;

  map.bit_vars.b_task=FALSE;
  pthread_join(smThread, &retval);
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
	int nRes;
  pSettings->createConnTent=1;

  while (pSettings->createConnTent--) {
    DBG_MIN("Creating conn");
    nRes = hci_le_create_conn(pSettings->hci_dev, 
                              htobs(0x0004),  /* interval ( htobs(0x0004), htobs(0x0500) )*/
                              htobs(0x0004),  /* window  ( < interval) ( htobs(0x0004), htobs(0x0200) )*/
                              0,              /* initiator filter */
                              LE_PUBLIC_ADDRESS, 
                              pSettings->BDAddress[3], 
                              LE_PUBLIC_ADDRESS,
                              htobs(0x000F),  /*min_interval*/
                              htobs(0x000F),  /*max_interval*/
                              htobs(0),       /*latency*/
                              htobs(0xC80),  /*supervision_timeout 0xC80==32000 ms, 3E8 == 10000 ms, 1F4 == 5000 ms, FA == 2500 ms */ 
                              htobs(0x0001),  /* min connection length */
                              htobs(0x0001),  /* max connection length */
                              &pSettings->handle, 
                              CREATE_CONN_TMO);             /* tmo before exit (if 0 wait forever) */
    if ( nRes < 0 ) {
      DBG_MIN("fails (nRes %d, nTent %d)", nRes, pSettings->createConnTent);
      SLEEPMS(1500);
      set_scanner_sm(CONN_SM_RESETDONGLE);
      break;
      }
    else {
      DBG_MIN("ok (pSettings->handle %d, nTent %d)", pSettings->handle, pSettings->createConnTent);
      set_scanner_sm(CONN_SM_CONNCOMPLETE);
      break;
      }
    }
  DBG_MIN("terminate thread");
  pthread_exit(NULL);
  return(NULL);
}

void *readRemoteVer_Th(void *param) {
  _Settings *pSettings = (_Settings *)param;
  char *ver;
  uint8_t features[8];
  struct hci_version version;
  //BtIOConnect connect_cb;
	int nRes, nTent=1;

  while (nTent--) {
    DBG_MIN("nTent %d", nTent);
    nRes=hci_read_remote_version(pSettings->hci_dev, pSettings->handle, &version, 5000);
    if (nRes == 0) {
        ver = lmp_vertostr(version.lmp_ver);
        printf("LMP Version: %s (0x%x) LMP Subversion: 0x%x\n", ver ? ver : "n/a", version.lmp_ver, version.lmp_subver);
        printf("Manufacturer: %s (%d)\n", bt_compidtostr(version.manufacturer), version.manufacturer);
        if (ver)
          bt_free(ver);
      bzero(features, sizeof(features));
      nRes=hci_le_read_remote_features(pSettings->hci_dev, pSettings->handle, features, 5000);
      if ( nRes < 0) {
        DBG_MIN("hci_le_read_remote_features fail: nRes %d", nRes);
        break;
        }
      else {
        printf("Features: 0x%2.2x 0x%2.2x 0x%2.2x 0x%2.2x 0x%2.2x 0x%2.2x 0x%2.2x 0x%2.2x\n", features[0], features[1], features[2], features[3], features[4], features[5], features[6], features[7]);
        break;
        }
      }
    else {
      DBG_MIN("Can't read_remote_version %s %d", strerror(errno), errno); 
      continue;
      }
    }
  set_scanner_sm(CONN_SM_DISCONN);
  SLEEPMS(1500);
  pthread_exit(NULL) ;        // terminate thread
  return(NULL);
}
