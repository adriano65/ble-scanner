#ifndef __SCANNER_H
#define __SCANNER_H

#include "stdint.h"
#include "stdbool.h"

#define SOFTREL 0x00	  // Software release
#define SUBSREL 0x01    // Software subrelease

typedef enum _SERIAL_PROTO {
	SERPROT_RK03Y = 0,
	SERPROT_WAYO3 = 1,
	SERPROT_DSM = 2,
	SERPROT_DSM_TEST = 3,
} SERIAL_PROTO;

struct _serprot_bits {
        uint8_t bRunning  : 1;
        SERIAL_PROTO protocol  : 3;
        uint8_t spare  : 4;
};

union _serprot_map {
        uint8_t bits;             // data input as 8-bit char
        struct _serprot_bits bit_vars;      // data output as single bit field.
};

typedef struct _Settings {
  unsigned int HCIDevNumber;
  char BDAddress[STDLEN];
  unsigned int SerialTMO;
  union _serprot_map map;
  pthread_t SerialThread;
} _Settings;

extern _LUCONFIG lu0cfg;

void structsInit(void);
void usage(char * argv[]);
void HandleSig(int signo);
void End(void);

CMDPARSING_RES doHCIDevNumber(_LUCONFIG * lucfg, int argc, char *argv[]);
CMDPARSING_RES doSerialProtocol(_LUCONFIG * lucfg, int argc, char *argv[]);
CMDPARSING_RES doBDAddress(_LUCONFIG * lucfg, int argc, char *argv[]);
CMDPARSING_RES doLogFileName(_LUCONFIG * lucfg, int argc, char *argv[]);

#endif