#ifndef __ADVERTISE_H
#define __ADVERTISE_H

#define LINEBUFFER	      512
#define MAXFUEL_LVL	      4095
#define MINFUEL_LVL	      1
#define FUEL_LVL_STEP	    10

#define cmd_opcode_pack(ogf, ocf) (uint16_t)((ocf & 0x03ff)|(ogf << 10))

#define EIR_FLAGS                   0X01
#define EIR_SERVICE_UUIDS           0x07
#define EIR_NAME_SHORT              0x08
#define EIR_NAME_COMPLETE           0x09
#define EIR_TX_POWER                0x0A
#define EIR_MANUFACTURE_SPECIFIC    0xFF

#define STDLEN  80

typedef enum _ADV_SIMULTYPE {
	ADV_SIM_NOSIM,
	ADV_SIM_FUELSTEAL,
	ADV_SIM_FUELUP,
} ADV_SIMULTYPE;

typedef struct _ADVCONFIG {
  unsigned char batt;
  unsigned int fuel;
  unsigned int fuelLevelMax;
  unsigned int fuelLevelMin;
  unsigned int fuelLevelStep;
  char ServerAddress[STDLEN];
  int ServerPort;

  void *Settings;
} ADVCONFIG;

unsigned int *uuid_str_to_data(char *uuid);
unsigned int twoc(int in, int t);
void usage(char * argv[]);
void pdebug(const char * fn, char * format, ...);

void set_Advertising_Parameters(int device_id, int device_handle);
void set_Advertising_Data(int dev_id, int device_handle);
void set_Advertising_Enable(int device_id, int device_handle);
void fuelStealSimulator(int dev_id, int device_handle);

#endif