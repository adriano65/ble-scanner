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
static _Settings *pSettings;
static struct termios orig_term, raw_term;
static pthread_mutex_t s_mutex;

struct CMDS Lu0cmds[] = {
    {"HCIDevNumber",            doHCIDevNumber, },
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
  pSettings=(_Settings *)malloc(sizeof(_Settings));
  lu0cfg.Settings = pSettings;
  pSettings->map.bit_vars.bScanMode=FALSE;
  pSettings->map.bit_vars.bTestMode=FALSE;
  pSettings->map.bit_vars.bScanEn=FALSE;
  pSettings->map.bit_vars.loglevel=LOG_DATA_AND_HEADER;

  while ((opt = getopt(argc, argv, "?thnsS")) != -1) {		//: semicolon means that option need an arg!
    switch(opt) {
      case 't' :
        pSettings->map.bit_vars.bTestMode=TRUE;
        break ;
      case 'h' :
        usage(argv);
        break ;
      case 'n' :
        lu0cfg.daemonize = TRUE;
        break ;
      case 'S' :
        pSettings->map.bit_vars.bScanMode=TRUE;
        pSettings->map.bit_vars.bScanEn=TRUE;
        break ;
      case 's' :
        pSettings->map.bit_vars.bScanEn=TRUE;
        break ;
      default:
        usage(argv);
        break ;
        }
    }

  printf("\n%s\nLoad configuration...\n%s\n", banner, lu0cfg.daemonize ? "Log to file" : "Log to stdout");
  if (LoadConfig(&lu0cfg)) { DBG_MIN("Bad or missing configuration file"); return FALSE; }
  if(geteuid() != 0) { printf("must run as root!\n"); return 0; }

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

  scanner_parser_init(pSettings);
  scanner_connect_init(pSettings);
  lu0cfg.Running = TRUE;

  struct hci_filter orig_filter;
  socklen_t orig_len = sizeof(orig_filter);

  if ((pSettings->hci_dev = hci_open_dev(pSettings->HCIDevNumber)) < 0 ) {  // Get HCI device.
    DBG_MIN("Failed hci_open_dev %s (%d)\n", strerror(errno), errno);
    }
  #if 0
  struct hci_dev_info di;
  if (ioctl(pSettings->hci_dev, HCIGETDEVINFO, (void *) &di) < 0) {
    perror("Failed to get HCI device info");
    close(pSettings->hci_dev);
    return -1;
    }
  if (!hci_test_bit(HCI_UP, &di.flags)) {
      if (ioctl(pSettings->hci_dev, HCIDEVUP, pSettings->hci_dev) < 0) {
        if (errno != EALREADY) {
          perror("Failed to bring up HCI device");
          close(pSettings->hci_dev);
          return -1;
        }
      }
    }
  #endif


  #if 1  
  if (getsockopt(pSettings->hci_dev, SOL_HCI, HCI_FILTER, &orig_filter, &orig_len) < 0) {
    DBG_MIN("Failed to save original HCI filter %s (%d)\n", strerror(errno), errno);
    return -1;
    }
  struct hci_filter new_filter;
  hci_filter_clear(&new_filter);
  hci_filter_set_ptype(HCI_EVENT_PKT, &new_filter);
  hci_filter_set_event(EVT_LE_META_EVENT, &new_filter);
  if (hci_le_set_scan_parameters(pSettings->hci_dev,
                                0x01, /* 0x00 passive,  */
                                htobs(0x10), 
                                htobs(0x10), 
                                LE_PUBLIC_ADDRESS, 
                                0x00, /* filter_policy = 0x01; --> Whitelist */
                                1000) < 0) {
    DBG_MIN("Failed hci_le_set_scan_parameters %s (%d)\n", strerror(errno), errno);
    }

  hci_le_set_scan_enable(pSettings->hci_dev, pSettings->map.bit_vars.bScanEn ? SCAN_INQUIRY : SCAN_DISABLED, 0, 500);
  // set new filter to capture all LE events
  if (setsockopt(pSettings->hci_dev, SOL_HCI, HCI_FILTER, &new_filter, sizeof(new_filter)) < 0) {
    DBG_MIN("Failed to set new filter to capture all LE events %s (%d)\n", strerror(errno), errno);
    return -1;
    }
  #else
  struct hci_filter new_filter;

  //cmd_opcode_pack()
  hci_filter_clear(&new_filter);
  hci_filter_all_events(&new_filter);
  hci_filter_set_ptype(HCI_EVENT_PKT, &new_filter);

  //hci_filter_set_event(EVT_LE_META_EVENT, &new_filter);
  hci_filter_set_event(EVT_LE_ADVERTISING_REPORT, &new_filter);  
  if (setsockopt(pSettings->hci_dev, SOL_HCI, HCI_FILTER, &new_filter, sizeof(new_filter)) < 0) {
    fprintf(stderr, "Failed to set filter to capture LE events %s (%d)\n", strerror(errno), errno);
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
    FD_SET(STDIN_FILENO, &rdset);
    FD_SET(pSettings->hci_dev, &rdset);
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    retval = select(FD_SETSIZE, &rdset, NULL, NULL, &tv);
    //retval = pselect(FD_SETSIZE, &rdset, NULL, NULL, &tp, NULL);
    if (!retval) {          	// select exited due to timeout
      BLE_TmoManager();
      }
    else if (FD_ISSET(pSettings->hci_dev, &rdset)) {
            DBG_MAX("ble activity");
            if ((nbyte = read(pSettings->hci_dev, buf, sizeof(buf)))>0) {
              AdvAnalyze(buf, nbyte);

              }
            else {DBG_MAX("nbyte %d", nbyte);}
            }
    DBG_MAX("loop");
    int len = read(STDIN_FILENO, &ch, 1);
    if (len == 1) { 
      switch (ch) {
        case 'a':
          printf("Request adv data...\n");
          set_scanner_sm(CONN_SM_GET_ADV_DATA);
          break;  

        case 'c':
          printf("Connecting...\n");
          set_scanner_sm(CONN_SM_CREATECONN);
          break;

        case 'l':
          pSettings->map.bit_vars.loglevel=(pSettings->map.bit_vars.loglevel > 4) ? 0 : pSettings->map.bit_vars.loglevel+1;
          printf("log level %d\n", pSettings->map.bit_vars.loglevel);
          break;

        case 'q':
          printf("quitting...\n");
          End();
          lu0cfg.Running=FALSE;
          break;

        case 'r':
          printf("Resetting Dongle...\n");
          set_scanner_sm(CONN_SM_RESETDONGLE);
          break;

        case 't':
          pSettings->map.bit_vars.bTestMode=pSettings->map.bit_vars.bTestMode ? 0 : 1;
          printf("Test Mode %d\n", pSettings->map.bit_vars.bTestMode);
          break;



        case 'x':
          pSettings->map.bit_vars.bScanEn=pSettings->map.bit_vars.bScanEn ? FALSE : TRUE;
          set_scanner_sm(CONN_SM_TOGGLE_SCAN);
          break;

        case '?':
          printf("press single key for:\n");
          printf("a - adv data\n");
          printf("c - connect\n");
          printf("l - log level %d\n", pSettings->map.bit_vars.loglevel);
          printf("q - quit\n");
          printf("r - reset bt adapter\n");
          printf("t - Test Mode %d\n", pSettings->map.bit_vars.bTestMode);
          printf("x - disable/enable scan\n");
          break;

        default:
          printf("You pressed char 0x%02x : %c\n", ch, (ch >= 32 && ch < 127) ? ch : ' ');
          break;
        }
      }
		}
	DBG_MIN("Exiting...");
  //if (setsockopt(pSettings->hci_dev, SOL_HCI, HCI_FILTER, &of, sizeof(of)) < 0)  return -1;
  hci_le_set_scan_enable(pSettings->hci_dev, SCAN_DISABLED, 1, 500);
	DBG_MIN("Scanning disabled.");
	hci_close_dev(pSettings->hci_dev);
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
        //pthread_mutex_lock(&s_mutex);
        hci_le_set_scan_enable(pSettings->hci_dev, SCAN_DISABLED, 0, 800);

        le_adv_info = (le_advertising_info *)(meta_event->data + 1);
        if (pSettings->map.bit_vars.bScanMode) { 
          ble_show_rxbuf(le_adv_info);
          }
        else {
          ble_fill_rxbuf(le_adv_info);
          }

        hci_le_set_scan_enable(pSettings->hci_dev, SCAN_INQUIRY, 0, 800);
        //pthread_mutex_unlock(&s_mutex);
        break;

      case EVT_LE_CONN_UPDATE_COMPLETE:
        //evt_le_connection_update_complete *cc = (void *)meta_event->data;
        DBG_MIN("EVT_LE_CONN_UPDATE_COMPLETE");        
        break;

      case EVT_LE_CONN_COMPLETE: {
        DBG_MIN("EVT_LE_CONN_COMPLETE");
        evt_le_connection_complete *cc = (void *)meta_event->data;
        /* Connection parameters:
        cc->status: connection status (0x00: Connection successfully completed); 
        cc->handle: connection handle to be used for the communication during the connection;
        */
        if (cc->status) {
          DBG_MIN("set_scanner_sm(CONN_SM_CONNFAILED)");
          set_scanner_sm(CONN_SM_CONNFAILED);
          }
        else {
          ba2str(&cc->peer_bdaddr, tmpbuf);
          DBG_MIN("peer_bdaddr %s", tmpbuf);
          pSettings->handle=cc->handle;
          set_scanner_sm(CONN_SM_CONNCOMPLETE);
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
        //tcflush(pSettings->hci_dev, TCIOFLUSH);
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
          if (pSettings->map.bit_vars.bScan) { 
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
  if (signo==SIGINT || signo==SIGTERM) { 
	  DBG_MIN("end");
    End();
    }
  else { DBG_MIN("unexpected signal: %d exiting anyway", signo); }
  exit(0);
}

void End(void) {
  scanner_parser_end(pSettings);
  scanner_connect_end(pSettings);
  lu0cfg.Running = FALSE;
  DBG_MIN("Running = FALSE");
  // Make sure no characters are left in the input stream as plenty of keys emit ESC sequences, otherwise they'll appear on the command-line after we exit.
  //while(read(STDIN_FILENO, &ch, 1)==1);
  // Restore original terminal settings
  tcsetattr(STDIN_FILENO, TCSANOW, &orig_term);
  //return 1;
}

void BLE_TmoManager() {
  DBG_MAX(".");
  //tcflush(pSettings->hci_dev, TCIOFLUSH);
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
  printf("\t-S\t\tScan all ble around\n") ;
  printf("\t-s\t\tScan configured ble only\n") ;
  exit(0) ;
}

// *********************************************************************
CMDPARSING_RES doHCIDevNumber(_LUCONFIG * lucfg, int argc, char *argv[]) {
  _Settings *pSettings = (_Settings *)lucfg->Settings;

  DBG_MAX("%d %s", argc, argv[1]);

  if (argc>1) {
    pSettings->HCIDevNumber=atoi(argv[1]);
    DBG_MIN("HCIDevNumber %d", pSettings->HCIDevNumber);
    }
  return CMD_EXECUTED_OK;
}

CMDPARSING_RES doBDAddresses(_LUCONFIG * lucfg, int argc, char *argv[]) {
  _Settings *pSettings = (_Settings *)lucfg->Settings;

  DBG_MAX("%d %s", argc, argv[1]);

  if (argc>8) {
    pSettings->proto[3]=atoi(argv[8]);
    }
  if (argc>7) {
    str2ba(argv[7], &pSettings->BDAddress[3] );
    DBG_MIN("BDAddress[3] %s, proto[3] %d", argv[7], pSettings->proto[3]);
    }

  if (argc>6) {
    pSettings->proto[2]=atoi(argv[6]);
    DBG_MIN("proto[2] %d", pSettings->proto[2]);
    }
  if (argc>5) {
    str2ba(argv[5], &pSettings->BDAddress[2] );
    }

  if (argc>4) {
    pSettings->proto[1]=atoi(argv[4]);
    DBG_MIN("proto[1] %d", pSettings->proto[1]);
    }
  if (argc>3) {
    str2ba(argv[3], &pSettings->BDAddress[1] );
    }

  if (argc>2) {
    pSettings->proto[0]=atoi(argv[2]);
    DBG_MIN("proto[0] %d", pSettings->proto[0]);
    }
  if (argc>1) {
    str2ba(argv[1], &pSettings->BDAddress[0] );
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
