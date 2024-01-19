#include <stdlib.h>
#include <errno.h>
#include <curses.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/ioctl.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include "advertisetest.h"

#define ADV_DBG_MIN
#ifdef ADV_DBG_MIN
  #define DBG_MIN(fmt...) do { pdebug(__FUNCTION__, fmt); } while(0)
#else
  #define DBG_MIN(fmt...) do { } while(0)
#endif

ADVCONFIG advcfg;

/* sudo ./advertisetest -M 1000 -m 900 -s 2 -r */
int main(int argc, char **argv){
  int dev_id, device_handle, opt;
	char tmpbuf[80];
  bdaddr_t bdaddr;
  ADV_SIMULTYPE adv_simtype=ADV_SIM_NOSIM;

  if(geteuid() != 0) { printf("must run as root!\n"); return 0; }

  advcfg.batt=37;
  advcfg.fuelLevelMax=MAXFUEL_LVL;
  advcfg.fuelLevelMin=MINFUEL_LVL;
  advcfg.fuelLevelStep=FUEL_LVL_STEP;  

  while ((opt = getopt(argc, argv, "bhurM:m:s:")) != -1) {		//: semicolon means that option need an arg!
    switch(opt) {
      case 'b' :
        advcfg.batt=29;
        break ;
      case 'u' :
        system("hciconfig hci0 up");
        break ;

      case 'M' :
        advcfg.fuelLevelMax=(unsigned int)strtol(optarg, NULL, 10);
        break ;

      case 'm' :
        advcfg.fuelLevelMin=(unsigned int)strtol(optarg, NULL, 10);
        break ;

      case 'r' :
        sprintf(tmpbuf, "hciconfig hci0 reset");
        //printf("%s\n", tmpbuf);
        system(tmpbuf);
        break ;
      case 's' :
        advcfg.fuelLevelStep=(unsigned int)strtol(optarg, NULL, 10);
        adv_simtype=ADV_SIM_FUELSTEAL;
        printf("Fuel Steal simulation\n");
        break ;
      case 'h' :
        usage(argv);
        break ;
      /*
      case 'd' :
        lu0cfg.daemonize = TRUE;
        break ;
      */
      default:
        usage(argv);
        break ;
        }
    }

  //dev_id = hci_for_each_dev(HCI_UP, find_conn, (long) &bdaddr);
  str2ba("00:13:25:AB:F2:D2", &bdaddr);     // my address 001325ABF2D2
  if ((dev_id = hci_get_route(&bdaddr)) < 0) {
    perror("Device is not available");
    exit(1);
		}

  if((device_handle = hci_open_dev(dev_id)) < 0) { perror("Could not open device"); exit(1); }

  set_Advertising_Parameters(dev_id, device_handle);
  set_Advertising_Data(dev_id, device_handle);
  
  #if 0
  unsigned int *uuid = uuid_str_to_data(argv[1]);
  for(int i=0; i<strlen(argv[1])/2; i++) { adv_data_cp.data[adv_data_cp.length + segment_length]  = htobs(uuid[i]); segment_length++; }

  // Major number
  int major_number = atoi(argv[2]);
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(major_number >> 8 & 0x00FF); segment_length++;
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(major_number & 0x00FF); segment_length++;

  // Minor number
  int minor_number = atoi(argv[3]);
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(minor_number >> 8 & 0x00FF); segment_length++;
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(minor_number & 0x00FF); segment_length++;

  // RSSI calibration
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(twoc(atoi(argv[4]), 8)); segment_length++;
  adv_data_cp.data[adv_data_cp.length] = htobs(segment_length - 1);
  adv_data_cp.length += segment_length;

  /* --------------------------------------------- FRAME */
  segment_length = 1;
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(EIR_NAME_COMPLETE); segment_length++;
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs('A'); segment_length++;
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs('D'); segment_length++;
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs('R'); segment_length++;
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs('Y'); segment_length++;
  adv_data_cp.data[adv_data_cp.length] = htobs(segment_length - 1);
  adv_data_cp.length += segment_length;

  /* --------------------------------------------- FRAME */
  segment_length = 1;
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(EIR_TX_POWER); segment_length++;
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(0xFC); segment_length++;
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(0x00); segment_length++;
  adv_data_cp.data[adv_data_cp.length] = htobs(segment_length - 1);
  adv_data_cp.length += segment_length;
  #endif

  set_Advertising_Enable(dev_id, device_handle);
  DBG_MIN("Advertising enabled on hci%d", dev_id);
  switch (adv_simtype) {
    case ADV_SIM_FUELSTEAL:
      fuelStealSimulator(dev_id, device_handle);
      break;

    default:
      break;
    }

  #if 0
  /* ---------------------------- request TX power on connected device ?*/
  bdaddr_t bdaddr;
  struct hci_conn_info_req *cr;
  int8_t level, type=0;
  //str2ba("00:13:25:AB:F2:AB", &bdaddr);
  str2ba("7C:D9:F4:1A:BA:9A", &bdaddr);
  cr = malloc(sizeof(*cr) + sizeof(struct hci_conn_info));
  if (!cr) { perror("Can't allocate memory");	exit(1);}
	bacpy(&cr->bdaddr, &bdaddr);
	cr->type = ACL_LINK;
	if (ioctl(device_handle, HCIGETCONNINFO, (unsigned long) cr) < 0) {perror("Get connection info failed");	exit(1);}
	if (hci_read_transmit_power_level(device_handle, htobs(cr->conn_info->handle), type, &level, 1000) < 0) { perror("HCI read transmit power level request failed");	exit(1);}
	printf("%s transmit power level: %d\n",	(type == 0) ? "Current" : "Maximum", level);
	free(cr);
  #endif

  hci_close_dev(device_handle);

}

/*  HCI_LE_Set_Advertising_Parameters - Used to set the advertising parameters */
void set_Advertising_Parameters(int dev_id, int device_handle) {
  int ret;
  le_set_advertising_parameters_cp adv_params_cp;

  memset(&adv_params_cp, 0, sizeof(adv_params_cp));
  /* advertisement time in ms */
  uint16_t interval = htobs(1000); // 1000 * 0.625ms = 625ms
  adv_params_cp.min_interval = interval;
  adv_params_cp.max_interval = interval;
  //adv_params_cp.advtype = 0x03; // non-connectable undirected advertising
  adv_params_cp.advtype = 0x00; // connectable ?
  adv_params_cp.chan_map = 7;   // all 3 channels

  uint8_t status;
  struct hci_request rq;
  bzero(&rq, sizeof(rq));
  rq.ogf = OGF_LE_CTL;
  rq.ocf = OCF_LE_SET_ADVERTISING_PARAMETERS;
  rq.cparam = &adv_params_cp;
  rq.clen = LE_SET_ADVERTISING_PARAMETERS_CP_SIZE;
  rq.rparam = &status;
  rq.rlen = 1;
  
  if ((ret = hci_send_req(device_handle, &rq, 1000)) < 0) {
    DBG_MIN("hci%d: %s (%d)", dev_id, strerror(errno), errno);
    }

  if (status) {
    DBG_MIN("hci%d returned status %d", dev_id, status);
    }
}

/* HCI_LE_Set_Advertising_Data - Used to set the data used in advertising packets that have a data field.
3.   HCI_LE_Set_Scan_Response_Data - Used to set the data used in scanning packets that have a data field.
4.   HCI_LE_Set_Advertise_Enable - Turn on Advertising.
*/
void set_Advertising_Data(int dev_id, int device_handle) {
  uint8_t segment_length;
  int ret;
  le_set_advertising_data_cp adv_data_cp;

  bzero(&adv_data_cp, sizeof(adv_data_cp));

  // 020106 0FFF 160F 0101 00 25178DBD 4600 00 0480 0A 0954445F333835363139CA30
  /* --------------------------------------------- FRAME 1 -> 02 02 02 */
  segment_length = 1;
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(EIR_FLAGS); segment_length++;   //0x01
  /*  Flags Advertising Data Type
  Bit 0 – Indicates LE Limited Discoverable Mode
  Bit 1 – Indicates LE General Discoverable Mode
  Bit 2 – Indicates whether BR/EDR is supported. This is used if your iBeacon is Dual Mode device
  Bit 3 – Indicates whether LE and BR/EDR Controller operates simultaneously
  Bit 4 – Indicates whether LE and BR/EDR Host operates simultaneously
  0b0001.1010   ==  0x1A
  0b0000.0010   ==  0x02
  */
  //adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(0x1A); segment_length++;
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(0x02); segment_length++;
  adv_data_cp.data[adv_data_cp.length] = htobs(segment_length - 1); // Set lenght of frame
  adv_data_cp.length += segment_length;

  /* --------------------------------------------- FRAME 2 */
  segment_length = 1;
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(EIR_MANUFACTURE_SPECIFIC); segment_length++;
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(0x16); segment_length++;    // Escort matching
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(0x0F); segment_length++;    // Escort matching
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(0x01); segment_length++;    // Escort matching

  //adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(0xFE); segment_length++;    // Big Endian Fuel
  //adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(0x04); segment_length++;    // Big Endian Fuel

  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs((unsigned char)(advcfg.fuel & 0x00FF)); segment_length++;    // Big Endian Fuel low
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs((unsigned char)((advcfg.fuel & 0xFF00)>>8)); segment_length++;    // Big Endian Fuel high

  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(advcfg.batt); segment_length++;    // battery

  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(0x17); segment_length++;    // temperature

  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(0x8D); segment_length++;    // ?
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(0xBD); segment_length++;
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(0x46); segment_length++;
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(0x00); segment_length++;
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(0x00); segment_length++;
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(0x04); segment_length++;
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(0x80); segment_length++;
  
  adv_data_cp.data[adv_data_cp.length] = htobs(segment_length - 1);
  adv_data_cp.length += segment_length;
  
  uint8_t status;
  struct hci_request rq;
  bzero(&rq, sizeof(rq));
  rq.ogf = OGF_LE_CTL;
  rq.ocf = OCF_LE_SET_ADVERTISING_DATA;
  rq.cparam = &adv_data_cp;
  rq.clen = LE_SET_ADVERTISING_DATA_CP_SIZE;
  rq.rparam = &status;
  rq.rlen = 1;
  if ((ret = hci_send_req(device_handle, &rq, 1000)) < 0) {
    DBG_MIN("hci%d: %s (%d)", dev_id, strerror(errno), errno);
    }

  if (status) {
    DBG_MIN("hci%d returned status %d", dev_id, status);
    }
}

void set_Advertising_Enable(int dev_id, int device_handle) {
  int ret;
  le_set_advertise_enable_cp advertise_cp;

  bzero(&advertise_cp, sizeof(advertise_cp));
  advertise_cp.enable = 0x01;

  uint8_t status;
  struct hci_request rq;
  bzero(&rq, sizeof(rq));
  rq.ogf = OGF_LE_CTL;
  rq.ocf = OCF_LE_SET_ADVERTISE_ENABLE;
  rq.cparam = &advertise_cp;
  rq.clen = LE_SET_ADVERTISE_ENABLE_CP_SIZE;
  rq.rparam = &status;
  rq.rlen = 1;

  if ((ret = hci_send_req(device_handle, &rq, 1000)) < 0) {
    DBG_MIN("hci%d: %s (%d)", dev_id, strerror(errno), errno);
    }

  if (status) {
    DBG_MIN("hci%d returned status %d", dev_id, status);
    }
}

void fuelStealSimulator(int dev_id, int device_handle){
  for (advcfg.fuel=advcfg.fuelLevelMax; advcfg.fuel >advcfg.fuelLevelMin; advcfg.fuel-=advcfg.fuelLevelStep) {
    sleep(20);
    set_Advertising_Data(dev_id, device_handle);
    DBG_MIN("MAXFUEL_LVL %i, MINFUEL_LVL %i, actual fuel %i, actual batt %i", advcfg.fuelLevelMax, advcfg.fuelLevelMin, advcfg.fuel, advcfg.batt);
    }

}


unsigned int *uuid_str_to_data(char *uuid) {
  char conv[] = "0123456789ABCDEF";
  int len = strlen(uuid);
  unsigned int *data = (unsigned int*)malloc(sizeof(unsigned int) * len);
  unsigned int *dp = data;
  char *cu = uuid;

  for(; cu<uuid+len; dp++,cu+=2) {
    *dp = ((strchr(conv, toupper(*cu)) - conv) * 16) 
        + (strchr(conv, toupper(*(cu+1))) - conv);
    }

  return data;
}

unsigned int twoc(int in, int t) {  return (in < 0) ? (in + (2 << (t-1))) : in;}

void usage(char * argv[]) {
  printf("Usage:\n") ;
  printf("\t%s [par]\n", argv[0]) ;
  printf("Where [par]:\n") ;
  printf("\t-?\t\tThis help\n") ;
  printf("\t-b\t\tset battery level to 2.9 V\n") ;
  printf("\t-r\t\treset hci0\n") ;
  printf("\t-M max\t\tFuel MAX\n") ;
  printf("\t-m min step\t\tFuel MIN\n") ;
  printf("\t-s step\t\tFuel STEP\n") ;
  printf("\t-u\t\tswitch on hci0\n") ;
  exit(0) ;
}

void pdebug(const char * fn, char * format, ...) {
    char buffer[LINEBUFFER];
    char datetimebuff[20];
    va_list args;

    va_start(args, format);
    vsnprintf(buffer, LINEBUFFER, format, args);
    if(strlen(buffer) >= LINEBUFFER - 1) {
      strcpy((buffer + LINEBUFFER - 4), "...");
      }
    
    struct tm *sTm;

    time_t now = time (0);
    sTm = gmtime (&now);

    //strftime (datetimebuff, sizeof(datetimebuff), "%Y%m%dT%H%M%S", sTm);
    //strftime (datetimebuff, sizeof(datetimebuff), "%Y%m%d, %H%M%S", sTm);
    strftime (datetimebuff, sizeof(datetimebuff), "%Y%m%d,%H:%M:%S", sTm);
    #define STROUTPUT "%s,%s, %s\n"
    printf(STROUTPUT, datetimebuff, fn, buffer);

    va_end(args);
}
