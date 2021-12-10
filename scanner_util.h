#ifndef _SCANNER_UTIL_H_
#define _SCANNER_UTIL_H_

#ifndef NULLCHAR
#define NULLCHAR	0
#endif

#define COMMENT_CHAR	'#'
#define QUOTE   34      // char: "

#define CR	0x0D
#define LF	0x0A

#define STDLEN 		80               // standard length for string definitions
#define RXBUFLEN	180

//#define ENABLE	0x01
//#define DISABLE	0x00

#define STATUS_TOPIC	"mdvr/status"

#define LINEBUFFER	      512
#define MOSQ_RECON_NTENT  1000
#define MAX_PAYLOADLEN	  1024
#define MAX_TOPICLEN	    512

#define MAX_ARGS  30    // Max number of args to commands
#define MAXARGC   30    // MAX argc
typedef enum _CMDPARSING_RES {
	CMDUTILPARSING_OK,
	CMDUTILEXECUTED_OK,
  CMD_EXECUTED_OK,
	CMD_COMMENT_CHAR_FOUND,
	CMDARG_QUOTE_CHAR_FOUND,
	CMD_NO_COMMAND_MATCHES,
	CMD_PARSING_STATE,
	CMDPARSING_KO,
  CMD_EXECUTED_KO,
  CMD_IO_STATES_OK,
  CMD_IO_STATES_MOSQ_ERR,
  CMD_PERIODIC_SENDING_STORED,
  CMD_IPCREQ_FROM_LU1IO_STORED,
  CMD_HISTORICAL_POSITION_RECOVERY,
  CMD_PERIODIC_DATA_SAVING,
  CMD_GENERIC_IPCREQ_SENDING,
  CMD_BAD_TIME_FORMAT,
} CMDPARSING_RES;

struct CMDS {
  char *name;
  CMDPARSING_RES (*func)();
};

enum eLogTypes {
  LOG2BIN_FILE,
  LOG2_FILE,
  LOG2_SCREEN
};

// Configuration structure
typedef struct _LUCONFIG {
  char name[STDLEN];		// Logic Unit name
  unsigned int nLU;		// Logic Unit number
  char HWCode[STDLEN];
  unsigned char daemonize;
  char ServerAddress[STDLEN];
  int ServerPort;

  int activeSerials;
  char *argv[MAXARGC];

  char cmd_topic[STDLEN+80];
  char answ_topic[STDLEN+80];
  struct CMDS *cmds;
  time_t timestamp;
  int Running;
  
  int IPCRunning;
  int ipcServerHnd;
  
  char LogFileName[STDLEN];
  char binLogPath[STDLEN] ;      // Where to store bin log
  char ConfFileName[STDLEN];

  void *Settings;
} _LUCONFIG;

//extern void pdebug(char *args, ...) __attribute__((format(printf, 1, 2))) ;
extern void pdebug(_LUCONFIG *, const char * fn, char *format, ...);
const char *byte2binary(unsigned int);
void savebuf2TxtFile(unsigned char *buf, int buflen);
void savebuf2BinFile(unsigned char *buf, int buflen);

#include <termios.h>
int tty_open(char * pname, struct termios * pnewtio, struct termios * poldtio, int brate);
void tty_close(int fd, struct termios * pnewtio, struct termios * poldtio);

int LoadConfig(_LUCONFIG *); 	// configuration from file
CMDPARSING_RES cmdparse(_LUCONFIG *srclucfg, struct _LUCONFIG *destcfg, char *linebuf);
int cmdparseAll(_LUCONFIG *lucfg, char *linebuf);

// *********************************************************************
// Command line parser functions
CMDPARSING_RES donull(_LUCONFIG * lucfg, int argc, char *argv[]);
CMDPARSING_RES dohelp(_LUCONFIG * lucfg, int argc, char *argv[]);
CMDPARSING_RES doend(_LUCONFIG * lucfg, int argc, char *argv[]) ;
CMDPARSING_RES dotrace(_LUCONFIG * lucfg, int argc, char *argv[]) ;
CMDPARSING_RES dover(_LUCONFIG * lucfg, int argc, char *argv[]) ;
CMDPARSING_RES doServerAddress(_LUCONFIG * lucfg, int argc, char *argv[]);
CMDPARSING_RES doServerPort(_LUCONFIG * lucfg, int argc, char *argv[]);
CMDPARSING_RES dotimestamp(_LUCONFIG * lucfg, int argc, char *argv[]);
CMDPARSING_RES dorequestId(_LUCONFIG * lucfg, int argc, char *argv[]);
CMDPARSING_RES dodeviceId(_LUCONFIG * lucfg, int argc, char *argv[]);

// function declared in utilmdvr.c
extern int prints(const char *format, ...) __attribute__((format(printf, 1, 2))) ;

#define SLEEPMS(_MS) usleep((_MS)*1000)

#endif