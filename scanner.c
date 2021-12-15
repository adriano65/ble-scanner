#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <pth.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <time.h>

#include "scanner_util.h"
#include "scanner_parser.h"
#include "scanner_connect.h"
#include "scanner.h"

#define SCANNER_DBG_MIN
#ifdef SCANNER_DBG_MIN
  #define DBG_MIN(fmt...) do { pdebug(&lu0cfg, __FUNCTION__, fmt); } while(0)
#else
  #define DBG_MIN(fmt...) do { } while(0)
#endif
//#define SCANNER_DBG_MAX
#ifdef SCANNER_DBG_MAX
  #define DBG_MAX(fmt...) do { pdebug(&lu0cfg, __FUNCTION__, fmt); } while(0)
#else
  #define DBG_MAX(fmt...) do { } while(0)
#endif

char banner[] = { 'S', 'C', 'A', 'N', 'N', 'E', 'R', ' ', 'v', SOFTREL+'0', '.', (SUBSREL>>4)+'0', (SUBSREL & 15)+'0', 0 };
_LUCONFIG lu0cfg;
_ble_data ble_data;
static _Settings *settings;
static struct termios orig_term, raw_term;

struct CMDS Lu0cmds[] = {
    {"HCIDevNumber",            doHCIDevNumber, },
    {"SerialProtocol",	        doSerialProtocol, },    
    {"BDAddresses",             doBDAddresses, },
    {"LogFileName",	 	          doLogFileName, },
    {"",        		            donull, },
    {NULLCHAR,			            donull, },
};

struct hci_request ble_hci_request(uint16_t ocf, int clen, void * status, void * cparam){
	struct hci_request rq;
	memset(&rq, 0, sizeof(rq));
	rq.ogf = OGF_LE_CTL;
	rq.ocf = ocf;
	rq.cparam = cparam;
	rq.clen = clen;
	rq.rparam = status;
	rq.rlen = 1;
	return rq;
}

int main(int argc, char *argv[]) {
	uint8_t buf[HCI_MAX_EVENT_SIZE];
  char ch = 0;
	int opt, nbyte, retval;
  fd_set rdset;

  lu0cfg.nLU=0;
  strncpy(lu0cfg.name, argv[0], STDLEN);
  gethostname(lu0cfg.HWCode, STDLEN);
  lu0cfg.cmds=Lu0cmds;
  lu0cfg.daemonize = FALSE;
  settings=(_Settings *)malloc(sizeof(_Settings));
  lu0cfg.Settings = settings;
  settings->map.bit_vars.bScanMode=FALSE;
  settings->map.bit_vars.bTestMode=FALSE;
  settings->map.bit_vars.bScanEn=FALSE;  

  while ((opt = getopt(argc, argv, "?thns")) != -1) {		//: semicolon means that option need an arg!
    switch(opt) {
      case 't' :
        settings->map.bit_vars.bTestMode=TRUE;
        break ;
      case 'h' :
        usage(argv);
        break ;
      case 'n' :
        lu0cfg.daemonize = TRUE;
        break ;
      case 's' :
        settings->map.bit_vars.bScanMode=TRUE;
        break ;
      default:
        usage(argv);
        break ;
        }
    }

  printf("\n%s\nLoad configuration...\n%s\n", banner, lu0cfg.daemonize ? "Log to file" : "Log to stdout");
  if (LoadConfig(&lu0cfg)) { DBG_MIN("Bad or missing configuration file"); return FALSE; }
  // Check daemonization
  if (lu0cfg.daemonize) {
    printf("Running as daemon...\n");
    daemon(TRUE, TRUE);
    }
  else {
    fflush(stdout);
    }

	signal(SIGINT, HandleSig);
	signal(SIGTERM, HandleSig);

  scanner_parser_init(settings);
  scanner_connect_init(settings);
  lu0cfg.Running = TRUE;


  #if 0
  struct hci_filter of;
  socklen_t olen = sizeof(of);

  if (getsockopt(settings->hci_dev, SOL_HCI, HCI_FILTER, &of, &olen) < 0) {
    DBG_MIN("Failed to save original HCI filter %s (%d)\n", strerror(errno), errno);
    return -1;
    }
  struct hci_filter nf;
  hci_filter_clear(&nf);
  hci_filter_set_ptype(HCI_EVENT_PKT, &nf);
  hci_filter_set_event(EVT_LE_META_EVENT, &nf);
  // set new filter to capture all LE events
  if (setsockopt(settings->hci_dev, SOL_HCI, HCI_FILTER, &nf, sizeof(nf)) < 0) {
    fprintf(stderr, "Failed to set new filter to capture all LE events %s (%d)\n", strerror(errno), errno);
    return -1;
    }
  #endif

  // Get terminal settings and save a copy for later -----------------------------------------------
  tcgetattr(STDIN_FILENO, &orig_term);
  raw_term = orig_term;
  // Turn off echoing and canonical mode
  raw_term.c_lflag &= ~(ECHO | ICANON);
  // Set min character limit and timeout to 0 so read() returns immediately
  // whether there is a character available or not
  raw_term.c_cc[VMIN] = 0;
  raw_term.c_cc[VTIME] = 0;
  // Apply new terminal settings
  tcsetattr(STDIN_FILENO, TCSANOW, &raw_term);

  struct timeval tv;
  //struct timespec tp;
	while ( lu0cfg.Running ) {    /* sudo btmon -i 0 is your friend */
    FD_ZERO(&rdset);
    //FD_SET(0, &rdset);
    FD_SET(settings->hci_dev, &rdset);
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    //tp.tv_sec = 1;
    //tp.tv_nsec = 0;

    retval = select(FD_SETSIZE, &rdset, NULL, NULL, &tv);
    //retval = pselect(FD_SETSIZE, &rdset, NULL, NULL, &tp, NULL);
    if (!retval) {          	// select exited due to timeout
      BLE_TmoManager();
      }
    else if (FD_ISSET(settings->hci_dev, &rdset)) {
            DBG_MAX("ble activity");
            if ((nbyte = read(settings->hci_dev, buf, sizeof(buf)))>0) {
              AdvAnalyze(buf, nbyte);
              }
            else {DBG_MAX("nbyte %d", nbyte);}
            }
         else { DBG_MIN("FD_ISNOTSET"); }

    DBG_MAX("loop");
    int len = read(STDIN_FILENO, &ch, 1);
    if (len == 1) { 
      switch (ch) {
        case 'c':
          printf("Connecting...\n");
          set_scanner_sm(CONN_SM_CREATECONN);
          break;

        case 'r':
          printf("Resetting Dongle...\n");
          set_scanner_sm(CONN_SM_RESETDONGLE);
          break;

        case 'x':
          settings->map.bit_vars.bScanEn=settings->map.bit_vars.bScanEn ? FALSE : TRUE;
          set_scanner_sm(CONN_SM_TOGGLE_SCAN);
          break;

        case '?':
          printf("press single key for:\n");
          printf("c - connect\n");
          printf("r - reset bt adapter\n");
          printf("x - disable/enable scan\n");
          break;

        default:
          printf("You pressed char 0x%02x : %c\n", ch, (ch >= 32 && ch < 127) ? ch : ' ');
          break;
        }
      }

		}

	DBG_MIN("Exiting");

  //if (setsockopt(settings->hci_dev, SOL_HCI, HCI_FILTER, &of, sizeof(of)) < 0)  return -1;

  if (hci_le_set_scan_enable(settings->hci_dev, SCAN_DISABLED, 1, 500) < 0)  return -1;
	DBG_MIN("Scanning disabled.");
	hci_close_dev(settings->hci_dev);
	return 0;
}

void AdvAnalyze(uint8_t * buf, int nbyte) {
	le_advertising_info * le_adv_info;
	evt_le_meta_event * meta_event;
  char tmpbuf[20];

  if ( nbyte >= HCI_EVENT_HDR_SIZE ) {
    meta_event = (evt_le_meta_event*)(buf+HCI_EVENT_HDR_SIZE+1);

    switch (meta_event->subevent) {
      case EVT_LE_ADVERTISING_REPORT:
        le_adv_info = (le_advertising_info *)(meta_event->data + 1);
        if (settings->map.bit_vars.bScanMode) { ble_show_rxbuf(le_adv_info); }
        else { ble_fill_rxbuf(le_adv_info); }
        break;

      case EVT_LE_CONN_COMPLETE: {
        DBG_MIN("EVT_LE_CONN_COMPLETE");
        evt_le_connection_complete *cc = (void *)meta_event->data;
        /* Connection parameters:
        cc->status: connection status (0x00: Connection successfully completed); 
        cc->handle: connection handle to be used for the communication during the  connection;
        */
        if (cc->status) {
          DBG_MIN("set_scanner_sm(CONN_SM_CONNFAILED)");
          set_scanner_sm(CONN_SM_CONNFAILED);
          }
        else {
          ba2str(&cc->peer_bdaddr, tmpbuf);
          DBG_MIN("peer_bdaddr %s", tmpbuf);

          #if 0
          settings->handle=cc->handle;
          set_scanner_sm(CONN_SM_CONNCOMPLETE);
          #else
          char *ver;
          uint8_t features[8];
          struct hci_version version;
          int nRes;
          nRes=hci_read_remote_version(settings->hci_dev, cc->handle, &version, 5000);
          if (nRes == 0) {
              ver = lmp_vertostr(version.lmp_ver);
              printf("LMP Version: %s (0x%x) LMP Subversion: 0x%x\nManufacturer: %s (%d)\n", ver ? ver : "n/a", version.lmp_ver, version.lmp_subver,
                bt_compidtostr(version.manufacturer),
                version.manufacturer);
              if (ver)
                bt_free(ver);
            memset(features, 0, sizeof(features));
            nRes=hci_le_read_remote_features(settings->hci_dev, cc->handle, features, 5000);
            if ( nRes < 0) {
              DBG_MIN("hci_le_read_remote_features fail: nRes %d", nRes);
              }
            else {
              printf("Features: 0x%2.2x 0x%2.2x 0x%2.2x 0x%2.2x 0x%2.2x 0x%2.2x 0x%2.2x 0x%2.2x\n", features[0], features[1], features[2], features[3], features[4], features[5], features[6], features[7]);
              usleep(10000);
              }
            }
          else {
            DBG_MIN("Can't read_remote_version nRes, %d %s %d", nRes, strerror(errno), errno); 
            }
          set_scanner_sm(CONN_SM_DISCONN);
          #endif
          }
        }
        break;

      case EVT_CONN_REQUEST:
        #if 0
        {
        char locbuf[64];
        evt_conn_request * evt_conn_rq = (evt_conn_request *)meta_event->data;
        ba2str(&evt_conn_rq->bdaddr, locbuf);
        DBG_MIN("EVT_CONN_REQUEST %s", locbuf );
        } 
        #else
        DBG_MIN("EVT_CONN_REQUEST");
        //set_scanner_sm(CONN_SM_CONNCOMPLETE);
        #endif
        break;

      case EVT_READ_REMOTE_FEATURES_COMPLETE :
        DBG_MIN("EVT_READ_REMOTE_FEATURES_COMPLETE");
        /*
        evt_read_remote_ft=(evt_read_remote_features_complete *)offset;
        printf("\tFeatures: 0x%2.2x 0x%2.2x 0x%2.2x 0x%2.2x 0x%2.2x 0x%2.2x 0x%2.2x 0x%2.2x\n", evt_read_remote_ft->features[0], evt_read_remote_ft->features[1], evt_read_remote_ft->features[2], evt_read_remote_ft->features[3], evt_read_remote_ft->features[4], evt_read_remote_ft->features[5], evt_read_remote_ft->features[6], evt_read_remote_ft->features[7]);
        */
        break;

      case EVT_READ_REMOTE_VERSION_COMPLETE :
        DBG_MIN("EVT_READ_REMOTE_VERSION_COMPLETE");
        /*
        int nRes;
        evt_read_remote_features_complete * evt_read_remote_ft;
        char *ver;
        uint8_t features[8];
        struct hci_version version;
        */
        #if 0
        {
        evt_read_remote_version_complete * evt_read_remote_ver=(evt_read_remote_version_complete *)meta_event->data;
        //evt_read_remote_version_complete * evt_read_remote_ver=(evt_read_remote_version_complete *)(meta_event->data+1);
        //evt_read_remote_version_complete * evt_read_remote_ver=(evt_read_remote_version_complete *)buf;
        char *ver = lmp_vertostr(evt_read_remote_ver->lmp_ver);
        printf("\tLMP Version: %s (0x%x) LMP Subversion: 0x%x\n \tManufacturer: %s (%d)\n", ver ? ver : "n/a", evt_read_remote_ver->lmp_ver, evt_read_remote_ver->lmp_subver,
          bt_compidtostr(evt_read_remote_ver->manufacturer),
          evt_read_remote_ver->manufacturer);
        if (ver)
          bt_free(ver);
        }
        #endif
        //set_scanner_sm(CONN_SM_CONNCOMPLETE);
        break;

      case EVT_DISCONN_COMPLETE:
        DBG_MIN("EVT_DISCONN_COMPLETE");
        break;

      case 0:
        DBG_MIN("EVT 0");
        //set_scanner_sm(CONN_SM_CONNCOMPLETE);
        //tcflush(settings->hci_dev, TCIOFLUSH);
        break;

      case EVT_ROLE_CHANGE:
        DBG_MIN("EVT_ROLE_CHANGE");
        break;

      default :
        DBG_MIN("meta_event->subevent %d", meta_event->subevent);
        break;
      }

    }
  else { 
    DBG_MIN("nbyte %d", nbyte);
    }
    
}

void AdvAnalyze_new(uint8_t * buf, int nbyte) {
	le_advertising_info * le_adv_info;
	evt_le_meta_event * meta_event;
  int count = 0;

  if ( nbyte >= HCI_EVENT_HDR_SIZE ) {
    DBG_MAX("count %d, nbyte %d\n", count, nbyte);
    count++;
    meta_event = (evt_le_meta_event*)(buf+HCI_EVENT_HDR_SIZE+1);

    if ( meta_event->subevent == EVT_LE_ADVERTISING_REPORT ) {
      le_adv_info = (le_advertising_info *)(meta_event->data + 1);
      DBG_MAX("Event %d, lenght %d", le_adv_info->evt_type, le_adv_info->length);

      if (le_adv_info->length==0) return;

      int current_index = 0;
      int data_error = 0;

      while(!data_error && current_index < le_adv_info->length) {
        size_t data_len = le_adv_info->data[current_index];

        if(data_len + 1 > le_adv_info->length) {
          DBG_MIN("EIR data length is longer than EIR packet length. %d + 1 > %d", data_len, le_adv_info->length);
          data_error = 1;
          }
        else {
          #if 1
          process_data(le_adv_info->data + current_index + 1, data_len, le_adv_info);
          #else
          if (settings->map.bit_vars.bScan) { 
            ble_show_rxbuf(le_adv_info);
            }
          else { 
            ble_fill_rxbuf(le_adv_info);
            }
          #endif
          current_index += data_len + 1;
        }
      }

      }
    else { 
      DBG_MIN("meta_event->subevent %d", meta_event->subevent);
      }
    }
  else { 
    DBG_MIN("nbyte %d", nbyte);
    }
    
}


void HandleSig(int signo) {
  if (signo==SIGINT || signo==SIGTERM) { End(); }
  else { DBG_MIN("unexpected signal: %d exiting anyway", signo); exit(0); }
}

void End(void) {
  scanner_parser_end(settings);
  scanner_connect_end(settings);
  lu0cfg.Running = FALSE;
  SLEEPMS(700);
  // Make sure no characters are left in the input stream as plenty of keys emit ESC sequences, otherwise they'll appear on the command-line after we exit.
  //while(read(STDIN_FILENO, &ch, 1)==1);
  // Restore original terminal settings
  tcsetattr(STDIN_FILENO, TCSANOW, &orig_term);    
  exit(0);
}

void BLE_TmoManager() {
  DBG_MIN(".");
  tcflush(settings->hci_dev, TCIOFLUSH);
  //pthread_mutex_lock(&gps_mutex);
  // ADD here
  //pthread_mutex_unlock(&gps_mutex);
}

void usage(char * argv[]) {
  printf("Usage:\n") ;
  printf("\t%s [par]\n", argv[0]) ;
  printf("Where [par]:\n") ;
  printf("\t-?\t\tThis help\n") ;
  printf("\t-t\t\tTest mode\n") ;
  printf("\t-n\t\tRun as daemon\n") ;
  printf("\t-s\t\tScan mode\n") ;
  exit(0) ;
}

// *********************************************************************
CMDPARSING_RES doHCIDevNumber(_LUCONFIG * lucfg, int argc, char *argv[]) {
  _Settings *settings = (_Settings *)lucfg->Settings;

  DBG_MAX("%d %s", argc, argv[1]);

  if (argc>1) {
    settings->HCIDevNumber=atoi(argv[1]);
    DBG_MIN("HCIDevNumber %d", settings->HCIDevNumber);
    }
  return CMD_EXECUTED_OK;
}

CMDPARSING_RES doBDAddresses(_LUCONFIG * lucfg, int argc, char *argv[]) {
  _Settings *settings = (_Settings *)lucfg->Settings;

  DBG_MAX("%d %s", argc, argv[1]);

  if (argc>8) {
    settings->BDAddressEn[3]=atoi(argv[8]);
    DBG_MIN("BDAddress[3] %s, enable %d", argv[7], settings->BDAddressEn[3]);
    }
  if (argc>7) {
    str2ba(argv[7], &settings->BDAddress[3] );
    }

  if (argc>6) {
    settings->BDAddressEn[2]=atoi(argv[6]);
    DBG_MIN("BDAddress[2] %s, enable %d", argv[5], settings->BDAddressEn[2]);
    }
  if (argc>5) {
    str2ba(argv[5], &settings->BDAddress[2] );
    }

  if (argc>4) {
    settings->BDAddressEn[1]=atoi(argv[4]);
    DBG_MIN("BDAddress[1] %s, enable %d", argv[3], settings->BDAddressEn[1]);
    }
  if (argc>3) {
    str2ba(argv[3], &settings->BDAddress[1] );
    }

  if (argc>2) {
    settings->BDAddressEn[0]=atoi(argv[2]);
    DBG_MIN("BDAddress[0] %s, enable %d", argv[1], settings->BDAddressEn[0]);
    }
  if (argc>1) {
    str2ba(argv[1], &settings->BDAddress[0] );
    }
  
  return CMD_EXECUTED_OK;
}

CMDPARSING_RES doSerialProtocol(_LUCONFIG * lucfg, int argc, char *argv[]) {
  _Settings *settings = (_Settings *)lucfg->Settings;

  DBG_MAX("%d %s", argc, argv[1]);
  if (argc>1) {
    settings->map.bit_vars.protocol=atoi(argv[1]);
    DBG_MIN("SerialPort 0 protocol = %d", settings->map.bit_vars.protocol);
    }

  return CMD_EXECUTED_OK;
}

CMDPARSING_RES doLogFileName(_LUCONFIG * lucfg, int argc, char *argv[]) {
  if (argc>1) {
    strncpy(lucfg->LogFileName, argv[1], STDLEN-1);
    }
  else return CMDPARSING_KO;
  return CMD_EXECUTED_OK;
}
