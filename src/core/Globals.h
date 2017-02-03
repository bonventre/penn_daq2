#ifndef _GLOBALS_H
#define _GLOBALS_H

#include <pthread.h>
#include "stdint.h"

#include "NetUtils.h"
#include "XL3Link.h"
#include "XL3Model.h"
#include "MTCModel.h"
#include "ControllerLink.h"


extern MTCModel *mtc;
extern XL3Model *xl3s[MAX_XL3_CON];
extern ControllerLink *contConnection;
extern pthread_mutex_t startTestLock;
extern struct event_base *evBase;
extern char finalTestIDs[MAX_XL3_CON][16][500];
extern char ecalID[500];

extern long int startTime, endTime, lastPrintTime, recvBytes, megaBundleCount; 
extern int writeLog;
extern FILE *logFile, *ecalLogFile;

int LockConnections(int sbc, uint32_t xl3List);
int UnlockConnections(int sbc, uint32_t xl3List);

void SwapLongBlock(void* p, int32_t n);
void SwapShortBlock(void* p, int32_t n);

int lfprintf(const char *fmt, ... );
int lprintf(const char *fmt, ... );
int PrintHelp(char *buffer);
int StartLogging();
int StopLogging();
int ParseMacro(char *filename);
int readConfigurationFile();
uint32_t GetUInt(const char *input, char flag, uint32_t dflt);
int GetInt(const char *input, char flag, int dflt);
int GetFlag(const char *input, char flag);
int GetMultiUInt(const char *input, int num, char flag, uint32_t *results, uint32_t dflt);
int GetString(const char *input, char *result, char flat, const char *dflt);
float GetFloat(const char *input, char flag, float dflt);
int GetMultiFloat(const char *input, int num, char flag, float *results, float dflt);
uint32_t GetBits(uint32_t value, uint32_t bit_start, uint32_t num_bits);


// configuration crap
extern int NEED_TO_SWAP;
extern char MTC_XILINX_LOCATION[100];
extern char DEFAULT_SSHKEY[100];
extern char DB_ADDRESS[100];
extern char DB_PORT[100];
extern char DB_USERNAME[100];
extern char DB_PASSWORD[100];
extern char DB_BASE_NAME[100];
extern char DB_VIEWDOC[100];
extern char FECDB_ADDRESS[100];
extern char FECDB_PORT[100];
extern char FECDB_USERNAME[100];
extern char FECDB_PASSWORD[100];
extern char FECDB_BASE_NAME[100];
extern char FECDB_VIEWDOC[100];
extern int MAX_PENDING_CONS;
extern int XL3_PORT;
extern int SBC_PORT;
extern char SBC_USER[100];
extern char SBC_SERVER[100];
extern int CONT_PORT;
extern char CONT_CMD_ACK[100];
extern char CONT_CMD_BSY[100];
extern int VIEW_PORT;
extern int BUNDLE_PRINT;
extern int CURRENT_LOCATION;
extern char ORCA_READOUT_PATH[100];

extern char DB_SERVER[100];
extern char FECDB_SERVER[100];

extern char *PENN_DAQ_ROOT;

#define CONFIG_FILE_LOC "config/local"
#define DEFAULT_CONFIG_FILE_LOC "config/default"

//#define PENN_DAQ_ROOT "."


#define ABOVE_GROUND_TESTSTAND 0
#define UNDERGROUND 1
#define PENN_TESTSTAND 2

#endif
