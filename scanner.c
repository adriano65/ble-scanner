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
static _Settings *settings;

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
	int ret, status, opt, i, nbyte, retval, hci_dev;
  fd_set rdset;
  struct timeval tv;

  lu0cfg.nLU=0;
  strncpy(lu0cfg.name, argv[0], STDLEN);
  gethostname(lu0cfg.HWCode, STDLEN);
  lu0cfg.cmds=Lu0cmds;
  lu0cfg.daemonize = FALSE;
  settings=(_Settings *)malloc(sizeof(_Settings));
  lu0cfg.Settings = settings;
  settings->map.bit_vars.bScan=FALSE;
  settings->map.bit_vars.bTestMode=FALSE;

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
        settings->map.bit_vars.bScan=TRUE;
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

  lu0cfg.Running = TRUE;


	// Get HCI device.
	if ((hci_dev = hci_open_dev(settings->HCIDevNumber)) < 0 ) { DBG_MIN("Failed to open HCI device."); return 0; }
  hci_le_set_scan_enable(hci_dev, 0, 0, 10000); // disable in case already enabled

	// example https://github.com/dlenski/ttblue/blob/master/ttblue.c
  struct hci_filter nf, of;
  int len;
  socklen_t olen = sizeof(of);
  char addr_str[18];
  le_advertising_info *info;

	if (hci_le_set_scan_parameters(hci_dev, /* passive */ 0x00, htobs(0x10), htobs(0x10), LE_PUBLIC_ADDRESS, 0x00, 10000) < 0) {
    fprintf(stderr, "Failed to set BLE scan parameters: %s (%d)\n", strerror(errno), errno);
    return -1;
    }
  if (hci_le_set_scan_enable(hci_dev, 0x01, /* include dupes */ 0x00, 10000) < 0) {
    fprintf(stderr, "Failed to enable BLE scan: %s (%d)\n", strerror(errno), errno);
    return -1;
  }
  // save HCI filter and set it to capture all LE events
  if (getsockopt(hci_dev, SOL_HCI, HCI_FILTER, &of, &olen) < 0) {return -1;}

  hci_filter_clear(&nf);
  hci_filter_set_ptype(HCI_EVENT_PKT, &nf);
  hci_filter_set_event(EVT_LE_META_EVENT, &nf);

  if (setsockopt(hci_dev, SOL_HCI, HCI_FILTER, &nf, sizeof(nf)) < 0) {return -1;}


	while ( lu0cfg.Running ) {
    FD_ZERO(&rdset);
    FD_SET(0, &rdset);
    FD_SET(hci_dev, &rdset);
    tv.tv_sec = 4;
    tv.tv_usec = 0;

    retval = select(FD_SETSIZE, &rdset, NULL, NULL, &tv);
    if (!retval) {          	// select exited due to timeout
      BLE_TmoManager();
      }
    else if (FD_ISSET(hci_dev, &rdset)) {
	    DBG_MAX("ble activity");
		  if ((nbyte = read(hci_dev, buf, sizeof(buf)))>0) {
	      AdvAnalyze(buf, nbyte);
	      }
	    }

		}

  if (setsockopt(hci_dev, SOL_HCI, HCI_FILTER, &of, sizeof(of)) < 0)  return -1;
  if (hci_le_set_scan_enable(hci_dev, 0x00, 1, 10000) < 0)  return -1;
	DBG_MIN("Scanning disabled.");
	hci_close_dev(hci_dev);

	return 0;
}

void AdvAnalyze(uint8_t * buf, int nbyte) {
	le_advertising_info * le_adv_info;
	evt_le_meta_event * meta_event;
  //extended_inquiry_info * ext_inq_info;
  int count = 0;

  if ( nbyte >= HCI_EVENT_HDR_SIZE ) {
    DBG_MAX("count %d, nbyte %d\n", count, nbyte);
    count++;
    meta_event = (evt_le_meta_event*)(buf+HCI_EVENT_HDR_SIZE+1);

    if ( meta_event->subevent == EVT_LE_ADVERTISING_REPORT ) {
      uint8_t reports_count = meta_event->data[0];
      DBG_MAX("reports_count %d", reports_count);
      void * offset = meta_event->data + 1;
      while ( reports_count-- ) {
        le_adv_info = (le_advertising_info *)offset;

        if (settings->map.bit_vars.bScan) { ble_show_rxbuf(le_adv_info); }
        else { ble_fill_rxbuf(le_adv_info); }

        offset = le_adv_info->data + le_adv_info->length + 2;
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

void AdvAnalyze_new(uint8_t * buf, int nbyte) {
	le_advertising_info * le_adv_info;
	evt_le_meta_event * meta_event;
  //extended_inquiry_info * ext_inq_info;
  uint8_t reports_count;
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
  if (signo==SIGINT || signo==SIGTERM) { End();  }
  else { DBG_MIN("unexpected signal: %d exiting anyway", signo); exit(0); }
}

void End(void) {
  lu0cfg.Running = FALSE;
  SLEEPMS(500);
}

void BLE_TmoManager() {
  DBG_MIN(".");
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
    DBG_MIN("BDAddress %s, enable %d", argv[7], settings->BDAddressEn[3]);
    }
  if (argc>7) {
    str2ba(argv[7], &settings->BDAddress[3] );
    }

  if (argc>6) {
    settings->BDAddressEn[2]=atoi(argv[6]);
    DBG_MIN("BDAddress %s, enable %d", argv[5], settings->BDAddressEn[2]);
    }
  if (argc>5) {
    str2ba(argv[5], &settings->BDAddress[2] );
    }

  if (argc>4) {
    settings->BDAddressEn[1]=atoi(argv[4]);
    DBG_MIN("BDAddress %s, enable %d", argv[3], settings->BDAddressEn[1]);
    }
  if (argc>3) {
    str2ba(argv[3], &settings->BDAddress[1] );
    }

  if (argc>2) {
    settings->BDAddressEn[0]=atoi(argv[2]);
    DBG_MIN("BDAddress %s, enable %d", argv[1], settings->BDAddressEn[0]);
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
