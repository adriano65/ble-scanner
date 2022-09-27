#ifndef __SCANNER_H
#define __SCANNER_H

#include "stdint.h"
#include "stdbool.h"

#define SOFTREL 0x00	  // Software release
#define SUBSREL 0x03    // Software subrelease

typedef enum _SERIAL_PROTO {
	SERPROT_DISABLED = 0,
	SERPROT_RK03Y,
	SERPROT_WAYO3,
	SERPROT_DSM,
	SERPROT_DSM_TEST,
	SERPROT_ESCORT = 5,
	SERPROT_TELTONIKA,
	SERPROT_TECHNOTON,
	SERPROT_WAYNOSE_1,
	SERPROT_MAX = 16,
} SERIAL_PROTO;

typedef enum {
    LOG_NOTHING,
    LOG_DATAONLY,
    LOG_DATA_AND_HEADER,
    LOG_DATA_EXT_DATAONLY,
} _LOGTYPE;


typedef struct _ble_dat {
  uint16_t companyID;
  uint8_t sensorDataID[3];
  uint16_t temperature;
  uint16_t humidity;
  uint16_t luminosity;
  uint16_t pgn;
  uint32_t frequency;
  uint8_t battery;

} _ble_data;

struct _serprot_bits {
        uint8_t bScanMode  : 1;
        uint8_t bTestMode  : 1;
        uint8_t bScanEn  : 1;
        uint8_t bleIdx  : 3;
        _LOGTYPE loglevel  : 2;
};

union _serprot_map {
        uint8_t bits;             // data input as 8-bit char
        struct _serprot_bits bit_vars;      // data output as single bit field.
};

typedef struct __Settings {
  unsigned int HCIDevNumber;
  bdaddr_t BDAddress[4];
  SERIAL_PROTO proto[4];
  int hci_dev;
  uint16_t handle;
  _ble_data *ble_data;
  unsigned int SerialTMO;
  union _serprot_map map;
  int createConnTent;
  int readRemotenTent;
} _Settings;

extern _LUCONFIG lu0cfg;
extern _ble_data ble_data;

void structsInit(void);
void usage(char * argv[]);
void HandleSig(int signo);
void End(void);
void BLE_TmoManager();
void AdvAnalyze(uint8_t * buf, int nbyte);
void AdvAnalyze_new(uint8_t * buf, int nbyte);

CMDPARSING_RES doHCIDevNumber(_LUCONFIG * lucfg, int argc, char *argv[]);
CMDPARSING_RES doBDAddresses(_LUCONFIG * lucfg, int argc, char *argv[]);
CMDPARSING_RES doLogFileName(_LUCONFIG * lucfg, int argc, char *argv[]);

#endif
