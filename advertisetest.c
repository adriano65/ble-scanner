#include <stdlib.h>
#include <errno.h>
#include <curses.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/ioctl.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#define cmd_opcode_pack(ogf, ocf) (uint16_t)((ocf & 0x03ff)|(ogf << 10))

#define EIR_FLAGS                   0X01
#define EIR_SERVICE_UUIDS           0x07
#define EIR_NAME_SHORT              0x08
#define EIR_NAME_COMPLETE           0x09
#define EIR_TX_POWER                0x0A
#define EIR_MANUFACTURE_SPECIFIC    0xFF

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


int main(int argc, char **argv){
  int ret;
  bdaddr_t bdaddr;

  if(argc != 5) {
    fprintf(stderr, "Usage: %s <UUID> <major number> <minor number> <RSSI calibration amount>\n", argv[0]);
    exit(1);
    }

  //int device_id = hci_get_route(NULL);
  str2ba("00:13:25:AB:F2:AB", &bdaddr);
  int device_id = hci_get_route(&bdaddr);

  int device_handle;
  if((device_handle = hci_open_dev(device_id)) < 0) { perror("Could not open device"); exit(1); }

  le_set_advertising_parameters_cp adv_params_cp;
  memset(&adv_params_cp, 0, sizeof(adv_params_cp));
  /* advertisement time in ms */
  uint16_t interval = htobs(30000); // 30000 * 0.625ms = 18650ms
  adv_params_cp.min_interval = interval;
  adv_params_cp.max_interval = interval;
  adv_params_cp.advtype = 0x03; // non-connectable undirected advertising
  adv_params_cp.chan_map = 7;   // all 3 channels

  uint8_t status;
  struct hci_request rq;
  memset(&rq, 0, sizeof(rq));
  rq.ogf = OGF_LE_CTL;
  rq.ocf = OCF_LE_SET_ADVERTISING_PARAMETERS;
  rq.cparam = &adv_params_cp;
  rq.clen = LE_SET_ADVERTISING_PARAMETERS_CP_SIZE;
  rq.rparam = &status;
  rq.rlen = 1;
  if ((ret = hci_send_req(device_handle, &rq, 1000))< 0) goto done;

  le_set_advertising_data_cp adv_data_cp;
  bzero(&adv_data_cp, sizeof(adv_data_cp));

  /* --------------------------------------------- FRAME */
  uint8_t segment_length = 1;
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(EIR_FLAGS); segment_length++;
  /*  Flags Advertising Data Type

    Bit 0 – Indicates LE Limited Discoverable Mode
    Bit 1 – Indicates LE General Discoverable Mode
    Bit 2 – Indicates whether BR/EDR is supported. This is used if your iBeacon is Dual Mode device
    Bit 3 – Indicates whether LE and BR/EDR Controller operates simultaneously
    Bit 4 – Indicates whether LE and BR/EDR Host operates simultaneously
    0b0001.1010
  */
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(0x1A); segment_length++;
  adv_data_cp.data[adv_data_cp.length] = htobs(segment_length - 1);
  adv_data_cp.length += segment_length;

  /* --------------------------------------------- FRAME */
  segment_length = 1;
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(EIR_MANUFACTURE_SPECIFIC); segment_length++;
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(0xA9); segment_length++;    // company ID Low Byte
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(0x00); segment_length++;    // company ID High Byte
  /* Manufacturer Specific Data */
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(0x02); segment_length++;
  /* Header of Manufacturer Payload */
  adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(0x15); segment_length++;

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


  bzero(&rq, sizeof(rq));
  rq.ogf = OGF_LE_CTL;
  rq.ocf = OCF_LE_SET_ADVERTISING_DATA;
  rq.cparam = &adv_data_cp;
  rq.clen = LE_SET_ADVERTISING_DATA_CP_SIZE;
  rq.rparam = &status;
  rq.rlen = 1;
  if ((ret = hci_send_req(device_handle, &rq, 1000)) < 0) goto done;

  le_set_advertise_enable_cp advertise_cp;
  bzero(&advertise_cp, sizeof(advertise_cp));
  advertise_cp.enable = 0x01;

  bzero(&rq, sizeof(rq));
  rq.ogf = OGF_LE_CTL;
  rq.ocf = OCF_LE_SET_ADVERTISE_ENABLE;
  rq.cparam = &advertise_cp;
  rq.clen = LE_SET_ADVERTISE_ENABLE_CP_SIZE;
  rq.rparam = &status;
  rq.rlen = 1;

  if ((ret = hci_send_req(device_handle, &rq, 1000)) < 0) goto done;

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


done:
  hci_close_dev(device_handle);

  if (ret < 0) {
    fprintf(stderr, "Can't set advertise mode on hci%d: %s (%d)\n", device_id, strerror(errno), errno);
    exit(1);
  }

  if (status) {
    fprintf(stderr, "LE set advertise enable on hci%d returned status %d\n", device_id, status);
    exit(1);
  }
}
