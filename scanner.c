// Copyright (c) 2021 David G. Young
// Copyright (c) 2015 Damian Kołakowski. All rights reserved.

#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <stddef.h>
#include <string.h>
#include <pth.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <time.h>

#include "scanner_util.h"
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

static volatile int test_mode=FALSE;
char banner[] = { 'S', 'C', 'A', 'N', 'N', 'E', 'R', ' ', 'v', SOFTREL+'0', '.', (SUBSREL>>4)+'0', (SUBSREL & 15)+'0', 0 } ;
_LUCONFIG lu0cfg;
static _Settings *settings;

struct CMDS Lu0cmds[] = {
    {"HCIDevNumber",            doHCIDevNumber, },
    {"SerialProtocol",	        doSerialProtocol, },    
    {"BDAddress",               doBDAddress, },
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
	int ret, status;
	int opt;

  lu0cfg.nLU=0;
  strncpy(lu0cfg.name, argv[0], STDLEN);
  gethostname(lu0cfg.HWCode, STDLEN);
  lu0cfg.cmds=Lu0cmds;
  lu0cfg.daemonize = TRUE;
  settings=(_Settings *)malloc(sizeof(_Settings));
  lu0cfg.Settings = settings;

  while ((opt = getopt(argc, argv, "?dhn")) != -1) {		//: semicolon means that option need an arg!
    switch(opt) {
      case 'd' :
        test_mode=TRUE;
        lu0cfg.daemonize = FALSE;
        break ;
      case 'h' :
        usage(argv);
        break ;
      case 'n' :
        lu0cfg.daemonize = FALSE;
        break ;
      default:
        usage(argv);
        break ;
        }
    }

  printf("\n%s\nLoad configuration...\n%s\n", banner, lu0cfg.daemonize ? "Log to file" : "Log to stdout");
  if (LoadConfig(&lu0cfg)) {
    DBG_MIN("Bad or missing configuration file");
    return FALSE;
    }
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
	int device = hci_open_dev(settings->HCIDevNumber);
	if ( device < 0 ) {
		perror("Failed to open HCI device.");
		return 0;
		}

	// Set BLE scan parameters.

	le_set_scan_parameters_cp scan_params_cp;
	memset(&scan_params_cp, 0, sizeof(scan_params_cp));
	scan_params_cp.type 			= 0x00;
	scan_params_cp.interval 		= htobs(0x0010);
	scan_params_cp.window 			= htobs(0x0010);
	scan_params_cp.own_bdaddr_type 	= 0x00; // Public Device Address (default).
	scan_params_cp.filter 			= 0x00; // Accept all.

	struct hci_request scan_params_rq = ble_hci_request(OCF_LE_SET_SCAN_PARAMETERS, LE_SET_SCAN_PARAMETERS_CP_SIZE, &status, &scan_params_cp);

	ret = hci_send_req(device, &scan_params_rq, 1000);
	if ( ret < 0 ) {
		hci_close_dev(device);
		perror("Failed to set scan parameters data.");
		return 0;
		}

	// Set BLE events report mask.

	le_set_event_mask_cp event_mask_cp;
	memset(&event_mask_cp, 0, sizeof(le_set_event_mask_cp));
	int i = 0;
	for ( i = 0 ; i < 8 ; i++ ) event_mask_cp.mask[i] = 0xFF;

	struct hci_request set_mask_rq = ble_hci_request(OCF_LE_SET_EVENT_MASK, LE_SET_EVENT_MASK_CP_SIZE, &status, &event_mask_cp);
	ret = hci_send_req(device, &set_mask_rq, 1000);
	if ( ret < 0 ) {
		hci_close_dev(device);
		perror("Failed to set event mask.");
		return 0;
		}

	// Enable scanning.

	le_set_scan_enable_cp scan_cp;
	memset(&scan_cp, 0, sizeof(scan_cp));
	scan_cp.enable 		= 0x01;	// Enable flag.
	scan_cp.filter_dup 	= 0x00; // Filtering disabled.

	struct hci_request enable_adv_rq = ble_hci_request(OCF_LE_SET_SCAN_ENABLE, LE_SET_SCAN_ENABLE_CP_SIZE, &status, &scan_cp);

	ret = hci_send_req(device, &enable_adv_rq, 1000);
	if ( ret < 0 ) {
		hci_close_dev(device);
		perror("Failed to enable scan.");
		return 0;
		}

	// Get Results.

	struct hci_filter nf;
	hci_filter_clear(&nf);
	hci_filter_set_ptype(HCI_EVENT_PKT, &nf);
	hci_filter_set_event(EVT_LE_META_EVENT, &nf);
	if ( setsockopt(device, SOL_HCI, HCI_FILTER, &nf, sizeof(nf)) < 0 ) {
		hci_close_dev(device);
		perror("Could not set socket options\n");
		return 0;
		}

	uint8_t buf[HCI_MAX_EVENT_SIZE];
	evt_le_meta_event * meta_event;
	le_advertising_info * le_adv_info;
  extended_inquiry_info * ext_inq_info;
	int len;

  int count = 0;
	// last_detection_time - now < 10
	while ( lu0cfg.Running ) {
		len = read(device, buf, sizeof(buf));
		if ( len >= HCI_EVENT_HDR_SIZE ) {
      count++;
			meta_event = (evt_le_meta_event*)(buf+HCI_EVENT_HDR_SIZE+1);
			if ( meta_event->subevent == EVT_LE_ADVERTISING_REPORT ) {
				uint8_t reports_count = meta_event->data[0];
        DBG_MAX("reports_count %d", reports_count);
				void * offset = meta_event->data + 1;
				while ( reports_count-- ) {
					le_adv_info = (le_advertising_info *)offset;
          if ( ble_fill_rxbuf(le_adv_info) ) { ; }
					offset = le_adv_info->data + le_adv_info->length + 2;
					}
				}
      else { DBG_MIN("meta_event->subevent %d\n", meta_event->subevent); }
			}
    SLEEPMS(300);
		}

	DBG_MIN("Disable scanning.");
	memset(&scan_cp, 0, sizeof(scan_cp));
	scan_cp.enable = 0x00;	// Disable flag.

	struct hci_request disable_adv_rq = ble_hci_request(OCF_LE_SET_SCAN_ENABLE, LE_SET_SCAN_ENABLE_CP_SIZE, &status, &scan_cp);
	ret = hci_send_req(device, &disable_adv_rq, 1000);
	if ( ret < 0 ) {
		hci_close_dev(device);
		perror("Failed to disable scan.");
		return 0;
	}

	hci_close_dev(device);

	return 0;
}

void HandleSig(int signo) {
  if (signo==SIGINT || signo==SIGTERM) {
    End();
    }
  else {
    DBG_MIN("unexpected signal: %d exiting anyway", signo);
    exit(0);
    }
}

void End(void) {
  lu0cfg.Running = FALSE;
  SLEEPMS(800);
}

void usage(char * argv[]) {
  printf("Usage:\n") ;
  printf("\t%s [par]\n", argv[0]) ;
  printf("Where [par]:\n") ;
  printf("\t-?\t\tThis help\n") ;
  printf("\t-n\t\tDon't run as daemon\n") ;
  printf("\t-d\t\tTest mode\n") ;
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

CMDPARSING_RES doBDAddress(_LUCONFIG * lucfg, int argc, char *argv[]) {
  _Settings *settings = (_Settings *)lucfg->Settings;

  DBG_MAX("%d %s", argc, argv[1]);

  if (argc>1) {
    strncpy(settings->BDAddress, argv[1], STDLEN-1);
    settings->BDAddress[STDLEN-1] = '\0';
    DBG_MIN("BDAddress 0 = %s", settings->BDAddress);
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
