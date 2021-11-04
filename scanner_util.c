#include <stdlib.h>     // "C" standard include files
#include <pth.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <stdarg.h>
#include <pthread.h>

#include <sys/time.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <linux/types.h>
#include <linux/rtc.h>
#include <linux/watchdog.h>

#include "scanner_util.h"

#define SP_UTIL_DBG_MIN
#ifdef SP_UTIL_DBG_MIN
#define DBG_MIN(format, ...) do { pdebug(&utilcfg, __FUNCTION__, format, ## __VA_ARGS__); } while(0)
#else
#define DBG_MIN(format, ...) do { } while(0)
#endif

//#define SP_UTIL_DBG_MAX
#ifdef SP_UTIL_DBG_MAX
#define DBG_MAX(format, ...) do { pdebug(&utilcfg, __FUNCTION__, format, ## __VA_ARGS__); } while(0)
#else
#define DBG_MAX(format, ...) do { } while(0)
#endif

_LUCONFIG utilcfg;

struct CMDS Utilcmds[] = {
    {"ServerAddress", 	doServerAddress, },
    {"ServerPort",   	  doServerPort, },
    {"timestamp",	      dotimestamp, },
    {"deviceId",	      dodeviceId, }, 
    {"end",     	      doend, },
    {"ver",     	      dover, },
    {"help", 		        dohelp, },
    {"",        	      donull, },
    {NULLCHAR,		      donull, },
};

static pthread_mutex_t pdebug_mutex = PTHREAD_MUTEX_INITIALIZER;

void savebuf2TxtFile(unsigned char *buf, int buflen) {
  FILE * fptxtlog;

  if ((fptxtlog = fopen("file.txt", "w")) == NULL) {
    printf("Can't open file");
    }
  else { 
    fwrite(buf, 1, buflen, fptxtlog);
    fclose(fptxtlog);
    }
}

void savebuf2BinFile(unsigned char *buf, int buflen) {
	char filename[1024];
	time_t timenow;
	struct tm * actual;
  FILE * fpbinlog;

	time(&timenow) ;       // time since 1970
	actual = localtime(&timenow);

  sprintf(filename, "%04d%02d%02d_%02d.bin", actual->tm_year+1900, actual->tm_mon+1, actual->tm_mday, actual->tm_hour);
  if ((fpbinlog = fopen(filename, "a+")) == NULL ) { DBG_MIN("File open failed %s", filename); }
  else {
    fwrite(buf, 1, buflen, fpbinlog);
    fclose(fpbinlog);
    }

}

/*
void add2FileLog(const char * function, int i, ...) {
	char buffer[1024];
	va_list arguments;
	va_start ( arguments, i );

	snprintf(buffer, 1023, va_arg ( arguments, const char * restrict ) );

	add2log(function, buffer, 1, LOG2_FILE);
	va_end ( arguments);
}

void add2log(const char * function, char * buffer, int buflen, enum eLogTypes nLogType) {
	char filename[1024];
	time_t timenow ;
	struct tm * actual ;
	FILE * fp;

	time(&timenow) ;       // time since 1970
	actual = localtime(&timenow) ;
  switch (nLogType) {
    case LOG2BIN_FILE:
			sprintf(filename, "%s/%04d%02d%02d_%02d%02d%02d.bin", comdata.binLogPath, actual->tm_year+1900, actual->tm_mon+1, actual->tm_mday, actual->tm_hour, actual->tm_min, actual->tm_sec);
			if ((fp = fopen(filename, "a+")) == NULL ) { DBG_MIN("File open failed %s", filename); }
			else {
				fwrite(buffer, 1, buflen, fp);
				fclose(fp);
				}
      break;

    case LOG2_FILE:
			sprintf(filename, "%s/%04d%02d%02d.log", comdata.LogFileName, actual->tm_year+1900, actual->tm_mon+1, actual->tm_mday);
			if ((fp = fopen(filename, "a")) == NULL ) { DBG_MIN("File open failed %s", filename); }
			else {
				fprintf(fp, "%02d:%02d:%02d %s: %s\n", actual->tm_hour, actual->tm_min, actual->tm_sec, function, buffer);
				fclose(fp);
				}
			printf("%02d:%02d:%02d %s: %s\n", actual->tm_hour, actual->tm_min, actual->tm_sec, function, buffer);
      break;

    case LOG2_SCREEN:
			printf("%02d:%02d:%02d %s: %s\n", actual->tm_hour, actual->tm_min, actual->tm_sec, function, buffer);
      break;
		}
}
*/

void pdebug(_LUCONFIG *lucfg, const char * fn, char * format, ...) {
    char buffer[LINEBUFFER];
    char datetimebuff[20];
    int nRet;
    FILE * fplog;
    va_list args;

    pthread_mutex_lock(&pdebug_mutex);
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
    if (lucfg->daemonize) {
      if ((fplog = fopen (lucfg->LogFileName, "a"))) {
        nRet=fprintf(fplog, STROUTPUT, datetimebuff, fn, buffer);
        if (nRet < 0) printf("fprintf err.\n");
        fclose(fplog);
        }
      //else { printf("%s - Cant write %s: %s (RO Filesys?)\n", datetimebuff, fn, buffer); }
      }
    else {
      printf(STROUTPUT, datetimebuff, fn, buffer);
      }

    va_end(args);
    pthread_mutex_unlock(&pdebug_mutex);
}

const char *byte2binary(unsigned int x) {
    static char b[9];
    b[0] = '\0';

    int z;
    for (z = 0x80; z > 0; z >>= 1) {
      strcat(b, ((x & z) == z) ? "1" : "0");
      }

    return b;
}

// -----------------------------------------------------------------------
int tty_open(char * pname, struct termios * pnewtio, struct termios * poldtio, int brate) {
  int fd ;

  // Open device for reading and writing and not as controlling tty
  // because we don't want to get killed if linenoise sends CTRL-C.

  fd = open(pname, O_RDWR | O_NOCTTY ) ;
  if (fd < 0) {
    return(fd) ;            // some error
    }

  tcgetattr(fd, poldtio) ;    // save current port settings

  // BAUDRATE: Set bps rate. You could also use cfsetispeed and cfsetospeed.
  // CRTSCTS : output hardware flow control (only used if the cable has
  //           all necessary lines. See sect. 7 of Serial-HOWTO)
  // CS8     : 8n1 (8bit,no parity,1 stopbit)
  // CSTOPB  : 2 stop bit
  // CLOCAL  : local connection, no modem contol
  // CREAD   : enable receiving characters
  // HUPCL   : lower modem control lines after last process closes
  //           the device (hang up).

  //pnewtio->c_cflag = brate | CRTSCTS | CS8 | HUPCL | CLOCAL | CREAD ;
  pnewtio->c_cflag = brate | CS8 | HUPCL | CLOCAL | CREAD ;

  // IGNBRK  : ignore breaks
  // IGNPAR  : ignore bytes with parity errors
  // otherwise make device raw (no other input processing)

  pnewtio->c_iflag = IGNPAR | IGNBRK ;

  // Raw output.
  pnewtio->c_oflag = 0 ;

  // Set input mode (non-canonical, no echo,...)
  pnewtio->c_lflag = 0;

  // initialize all control characters
  // default values can be found in /usr/include/termios.h, and are given
  // in the comments, but we don't need them here

  pnewtio->c_cc[VINTR]    = 0 ;       // Ctrl-c
  pnewtio->c_cc[VQUIT]    = 0 ;       // Ctrl-bkslash
  pnewtio->c_cc[VERASE]   = 0 ;       // del
  pnewtio->c_cc[VKILL]    = 0 ;       // @
  pnewtio->c_cc[VEOF]     = 0 ;       // Ctrl-d
  pnewtio->c_cc[VSWTC]    = 0 ;       // '0'
  pnewtio->c_cc[VSTART]   = 0 ;       // Ctrl-q
  pnewtio->c_cc[VSTOP]    = 0 ;       // Ctrl-s
  pnewtio->c_cc[VSUSP]    = 0 ;       // Ctrl-z
  pnewtio->c_cc[VEOL]     = 0 ;       // '0'
  pnewtio->c_cc[VREPRINT] = 0 ;       // Ctrl-r
  pnewtio->c_cc[VDISCARD] = 0 ;       // Ctrl-u
  pnewtio->c_cc[VWERASE]  = 0 ;       // Ctrl-w
  pnewtio->c_cc[VLNEXT]   = 0 ;       // Ctrl-v
  pnewtio->c_cc[VEOL2]    = 0 ;       // '0'

  //pnewtio->c_cc[VTIME]    = 1 ;       // inter-character timer unused 0.1 sec
  pnewtio->c_cc[VTIME]    = 0 ;       // no timeout (use select)
  pnewtio->c_cc[VMIN]     = 1; // blocking read until x chars received

  // cfmakeraw(pnewtio);
  
  //  now clean the modem line and activate the settings for the port
  tcflush(fd, TCIFLUSH) ;
  tcsetattr(fd, TCSANOW, pnewtio) ;

  SLEEPMS(10) ;         // settling time

  return(fd) ;
}

// *********************************************************************
void tty_close(int fd, struct termios * pnewtio, struct termios * poldtio) {
  tcflush(fd, TCIOFLUSH) ;            // buffer flush
  tcsetattr(fd, TCSANOW, poldtio) ;
  close(fd) ;
}

// *********************************************************************
int LoadConfig(_LUCONFIG *lucfg) {
    int nline, retval;
    char linebuf[LINEBUFFER];
    FILE *fin ;

    retval = 0;
    nline = 1 ;
    strncpy(lucfg->ConfFileName, lucfg->name, STDLEN);	// my name is default configuration filename ;-)
    strcat(lucfg->ConfFileName, ".conf");
    DBG_MAX("filename %s", lucfg->ConfFileName);
    
    strncpy(lucfg->LogFileName, lucfg->name, STDLEN);	// my name is default log filename ;-)
    strcat(lucfg->LogFileName, ".log");
    DBG_MAX("LogFileName %s", lucfg->LogFileName);
    
    utilcfg.cmds=Utilcmds;
    
    fin = fopen(lucfg->ConfFileName, "r");         	// configuration file
    if (fin) {					// really exists ?
      DBG_MAX("fopen ok");
      while(fgets(linebuf, LINEBUFFER, fin)) {	// get lines
        if ( ! cmdparseAll(lucfg, linebuf) ) {
          printf("Config error at line %d\n", nline) ;
          retval = 1;
          }
        nline++ ;			// another line parsed
        }
      fclose(fin);
      } 
    else {
      retval = 1 ;			// file not found
      }
      
    sprintf(lucfg->cmd_topic, "mdvr/command/%s/lu%1d/", lucfg->HWCode, lucfg->nLU);    
    sprintf(lucfg->answ_topic, "mdvr/result/%s/lu%1d/+", lucfg->HWCode, lucfg->nLU);    
    
    return(retval) ;
}

int cmdparseAll(_LUCONFIG *lucfg, char *linebuf) {
  int nRet=FALSE;
  switch (cmdparse(&utilcfg, lucfg, linebuf)) {
    case CMD_COMMENT_CHAR_FOUND:
    case CMDARG_QUOTE_CHAR_FOUND:
    case CMDUTILEXECUTED_OK:
      nRet=TRUE;
      break;
    case CMDPARSING_KO:
      DBG_MIN("CMDPARSING_KO %s", linebuf);
      break;

    case CMD_NO_COMMAND_MATCHES:
      switch (cmdparse(lucfg, lucfg, linebuf) ) {
        case CMD_COMMENT_CHAR_FOUND:
        case CMDARG_QUOTE_CHAR_FOUND:
        case CMD_IO_STATES_OK:
        case CMD_IO_STATES_MOSQ_ERR:
        case CMD_EXECUTED_OK:          
          nRet=TRUE;
          break;

        case CMD_PERIODIC_SENDING_STORED:
        case CMD_IPCREQ_FROM_LU1IO_STORED:
        case CMD_HISTORICAL_POSITION_RECOVERY:
        case CMD_PERIODIC_DATA_SAVING:
        case CMD_GENERIC_IPCREQ_SENDING:          
          nRet=TRUE;
          break;
          
        case CMD_NO_COMMAND_MATCHES:
          DBG_MIN("%s NO COMMAND MATCHES in LU %s", linebuf, lucfg->name);
          nRet=TRUE;
          break;
        case CMDUTILEXECUTED_OK:
          DBG_MIN("%s CMDUTILEXECUTED_OK !!! unexpected in LU %s", linebuf, lucfg->name);
          break;

        case CMD_BAD_TIME_FORMAT:
          DBG_MIN("%s CMD_BAD_TIME_FORMAT, LU %s", linebuf, lucfg->name);
          nRet=TRUE;
          break;

        case CMD_EXECUTED_KO:
          DBG_MIN("%s KO, LU %s", linebuf, lucfg->name);
          break;

        default:
          DBG_MIN("unexpected line %s", linebuf);
          break;
        }
      break;

    default:
      DBG_MIN("util unexpected line %s", linebuf);
      break;
    }

  return nRet;
}
  
/* COMMANDS ARE CASE SENSITIVE!! */
CMDPARSING_RES cmdparse(_LUCONFIG *srclucfg, _LUCONFIG *destcfg, char *linebuf) {
  register int i=0;
  struct CMDS *cmdp;
  char *argv[MAX_ARGS],*cp ;
  int argc, qflag;
  CMDPARSING_RES cmdpRes=CMD_PARSING_STATE;
  char * line;
  char *linePnt;

  line=(char *)malloc(8 * 1024);
  linePnt = line;
  memcpy((char *)line, linebuf, strlen(linebuf)+1 );
  // Remove cr/lf
  if((cp = strchr(line,'\015')) != NULLCHAR)
    *cp = '\0';
  if((cp = strchr(line,'\012')) != NULLCHAR)
    *cp = '\0';       					// shouldn't be necessary

  //DBG("resets argv[]");
  for(argc = 0; argc < MAX_ARGS; argc++)
    argv[argc] = NULLCHAR;

  for(argc = 0; argc < MAX_ARGS;) {
    qflag = 0;
    while(*line == ' ' || *line == '\t')		// Skip leading white space
      line++;
    argv[argc] = line ;
    if(*line == '\0')
      break;
    if(*line == QUOTE){					// Check for quoted token
      line++; 	// Suppress quote
      qflag = 1;
      }
    argv[argc++] = line;				// Beginning of token
    if(qflag){						// Find terminating delimiter
      if((cp = strchr(line, QUOTE)) == NULLCHAR){	// Find quote, it must be present
        //DBG("Find quote, it must be present");
        cmdpRes=CMDARG_QUOTE_CHAR_FOUND;
        break;
        }
      }
    else {
      if((cp = strchr(line,' ')) == NULLCHAR)		// Find space or tab. If not present, then we've already found the last token.
        if ((cp = strchr(line,'\t')) == NULLCHAR)
          break;
      }
    *cp++ = '\0';
    line = cp;
    }
  
  if (argv[0][0] == COMMENT_CHAR) { 
    cmdpRes=CMD_COMMENT_CHAR_FOUND;
    }
  else {
    if (cmdpRes==CMD_PARSING_STATE) {
      // Look up command in table; NULLCHAR signals end of table
      for(cmdp = srclucfg->cmds; cmdp->name != NULLCHAR; cmdp++) {
        DBG_MAX("argv[0] %s, cmdp->name %s", argv[0], cmdp->name);
        if (strcmp(argv[0], cmdp->name) == 0) {
          DBG_MAX("cmd found! cmdp->name %s", cmdp->name);
          break;
          }
        i++;
        }
        if(cmdp->name == NULLCHAR) {
          cmdpRes=CMD_NO_COMMAND_MATCHES;
          }
        else {
          DBG_MAX("return (*cmdp->func)(destcfg, argc, argv) %p", destcfg );
          cmdpRes = (*cmdp->func)(destcfg, argc, argv);
          }
      }
    }
  free(linePnt);
  return cmdpRes;
}

// Call a subcommand based on the first token in an already-parsed line
int subcmd(struct CMDS tab[], int argc, char *argv[]) {
	register struct CMDS *cmdp;

	// Strip off first token and pass rest of line to subcommand
	argc--;
	argv++;

	for(cmdp = tab;cmdp->name != NULLCHAR;cmdp++) {
	  if(strncmp(argv[0],cmdp->name,strlen(argv[0])) == 0) {
	    return (*cmdp->func)(argc,argv);
	    }
	  }
	return -1;
}

// *********************************************************************
CMDPARSING_RES donull(_LUCONFIG * lucfg, int argc, char *argv[]) {
    DBG_MAX(" .");
    return CMDUTILEXECUTED_OK;
}

CMDPARSING_RES dohelp(_LUCONFIG * lucfg, int argc, char *argv[]) {
  /*
  printf("end\nset\nmem\ngprs\ndata\nvoice(int argc, char *argv[])\n");
  printf("vol(int argc, char *argv[])\ntrace ON OFF\nver\ngps(int argc, char *argv[])\ngsm\nio\n");
  printf("ad\nsetsernum 123456\npid\ntimeout 12\nupgrade\n");
  */
    return CMDUTILEXECUTED_OK;
}

// *********************************************************************
CMDPARSING_RES doend(_LUCONFIG * lucfg, int argc, char *argv[]) {
  //char pBuf[16];
  
  if (argc<2) {
    
    } 
  else if (!strcasecmp(argv[1], "sleep")) {
	  ;
	  } 
	else if (!strcasecmp(argv[1], "standby")) {
		;
		}
    return CMDUTILEXECUTED_OK;
}

// *********************************************************************
CMDPARSING_RES dover(_LUCONFIG * lucfg, int argc, char *argv[]) {
  return CMDUTILEXECUTED_OK;
}

// *********************************************************************
CMDPARSING_RES doServerAddress(_LUCONFIG * lucfg, int argc, char *argv[]) {
  strncpy(lucfg->ServerAddress, argv[1], STDLEN-1);
  lucfg->ServerAddress[STDLEN-1] = '\0';
  DBG_MAX("lucfg->ServerAddress = %s", lucfg->ServerAddress);
  return CMDUTILEXECUTED_OK;
}

// *********************************************************************
CMDPARSING_RES doServerPort(_LUCONFIG * lucfg, int argc, char *argv[]) {
  // lucfg->ServerPort=1883;
  lucfg->ServerPort=atoi(argv[1]);
  DBG_MAX("lucfg->ServerPort = %d\n", lucfg->ServerPort);
  return CMDUTILEXECUTED_OK;
}

CMDPARSING_RES dotimestamp(_LUCONFIG * lucfg, int argc, char *argv[]) {
  lucfg->timestamp = atoi(argv[1]);
  DBG_MAX("lucfg->timestamp = %d\n", lucfg->timestamp);
  return CMDUTILEXECUTED_OK;
}

CMDPARSING_RES dodeviceId(_LUCONFIG * lucfg, int argc, char *argv[]) {
  return CMDUTILEXECUTED_OK;
}

