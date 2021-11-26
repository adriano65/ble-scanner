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
  struct hci_request hci_rq;

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

  #if 1
	// Set BLE scan parameters.
	le_set_scan_parameters_cp scan_params_cp;
	memset(&scan_params_cp, 0, sizeof(scan_params_cp));
	scan_params_cp.type 			= 0x00; //Passive scanning
	scan_params_cp.interval 		= htobs(8000);    // How frequently should scan (windows<interval)
	scan_params_cp.window 			= htobs(500);    // How much time execute the scan (Time * 0.625 ms)
	scan_params_cp.own_bdaddr_type 	= 0x00; // Public Device Address (default).
	scan_params_cp.filter 			= 0x02; // Accept all.

	hci_rq = ble_hci_request(OCF_LE_SET_SCAN_PARAMETERS, LE_SET_SCAN_PARAMETERS_CP_SIZE, &status, &scan_params_cp);

	if ((ret = hci_send_req(hci_dev, &hci_rq, 1000)) < 0 ) {
		hci_close_dev(hci_dev);
		DBG_MIN("Failed to set scan parameters data.");
		return 0;
		}

	// Set BLE events report mask.
	le_set_event_mask_cp event_mask_cp;
	memset(&event_mask_cp, 0, sizeof(le_set_event_mask_cp));
	for ( i = 0 ; i < 8 ; i++ ) event_mask_cp.mask[i] = 0xFF;

	hci_rq = ble_hci_request(OCF_LE_SET_EVENT_MASK, LE_SET_EVENT_MASK_CP_SIZE, &status, &event_mask_cp);
	if ((ret = hci_send_req(hci_dev, &hci_rq, 1000)) < 0 ) {
		hci_close_dev(hci_dev);
		DBG_MIN("Failed to set event mask.");
		return 0;
		}
  #endif

	// Enable scanning.
	le_set_scan_enable_cp le_scan_en_cp;
	memset(&le_scan_en_cp, 0, sizeof(le_set_scan_enable_cp));
	le_scan_en_cp.enable 		= 0x01;	// Enable flag.
	le_scan_en_cp.filter_dup 	= 0x00; // Filtering disabled.
	hci_rq = ble_hci_request(OCF_LE_SET_SCAN_ENABLE, LE_SET_SCAN_ENABLE_CP_SIZE, &status, &le_scan_en_cp);

	if ( (ret = hci_send_req(hci_dev, &hci_rq, 1000)) < 0 ) {
		hci_close_dev(hci_dev);
		DBG_MIN("Failed to enable scan.");
		return 0;
		}

	// Get Results.
	struct hci_filter nf;
	hci_filter_clear(&nf);
	hci_filter_set_ptype(HCI_EVENT_PKT, &nf);
	hci_filter_set_event(EVT_LE_META_EVENT, &nf);
	if ( setsockopt(hci_dev, SOL_HCI, HCI_FILTER, &nf, sizeof(nf)) < 0 ) {
		hci_close_dev(hci_dev);
		perror("Could not set socket options\n");
		return 0;
		}

	while ( lu0cfg.Running ) {
    FD_ZERO(&rdset);
    FD_SET(0, &rdset);
    FD_SET(hci_dev, &rdset);
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    retval = select(FD_SETSIZE, &rdset, NULL, NULL, &tv);
    if (!retval) {          	// select exited due to timeout
      SerialTMOManager();
      }
    else if (FD_ISSET(hci_dev, &rdset)) {
	    DBG_MAX("ble activity");
		  if ((nbyte = read(hci_dev, buf, sizeof(buf)))>0) {
	      AdvAnalyze(buf, nbyte);
	      }
	    }

		}

  /*
  if (current_hci_state.state == HCI_STATE_FILTERING)	{
    current_hci_state.state = HCI_STATE_SCANNING;
    setsockopt(hci_dev, SOL_HCI, HCI_FILTER, &current_hci_state.original_filter, sizeof(current_hci_state.original_filter));
    }
  */

	memset(&le_scan_en_cp, 0, sizeof(le_set_scan_enable_cp));
	le_scan_en_cp.enable = 0x00;	// Disable flag.
	hci_rq = ble_hci_request(OCF_LE_SET_SCAN_ENABLE, LE_SET_SCAN_ENABLE_CP_SIZE, &status, &le_scan_en_cp);
	if ((ret = hci_send_req(hci_dev, &hci_rq, 2500)) < 0 ) {
		hci_close_dev(hci_dev);
		DBG_MIN("Failed to disable scan: %s", strerror(errno));   //Connection timed out
		return 0;
	  }

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

void SerialTMOManager() {
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
    DBG_MIN("HCIDevNumber[0] %d", settings->HCIDevNumber);
    }
  return CMD_EXECUTED_OK;
}

CMDPARSING_RES doBDAddresses(_LUCONFIG * lucfg, int argc, char *argv[]) {
  _Settings *settings = (_Settings *)lucfg->Settings;

  DBG_MAX("%d %s", argc, argv[1]);

  if (argc>6) {
    settings->BDAddressEn[2]=atoi(argv[6]);
    }
  if (argc>5) {
    strncpy(settings->BDAddress[2], argv[5], STDLEN-1);
    settings->BDAddress[2][STDLEN-1] = '\0';
    DBG_MIN("BDAddress 2 = %s", settings->BDAddress[2]);
    }
  if (argc>4) {
    settings->BDAddressEn[1]=atoi(argv[4]);
    }
  if (argc>3) {
    strncpy(settings->BDAddress[1], argv[3], STDLEN-1);
    settings->BDAddress[1][STDLEN-1] = '\0';
    DBG_MIN("BDAddress 1 = %s", settings->BDAddress[1]);
    }
  if (argc>2) {
    settings->BDAddressEn[0]=atoi(argv[2]);
    }
  if (argc>1) {
    strncpy(settings->BDAddress[0], argv[1], STDLEN-1);
    settings->BDAddress[0][STDLEN-1] = '\0';
    DBG_MIN("BDAddress 0 = %s", settings->BDAddress[0]);
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
