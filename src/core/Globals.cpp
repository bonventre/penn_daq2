#include <stdlib.h>

#include "Globals.h"
#include "XL3Registers.h"
#include "MTCRegisters.h"

MTCModel *mtc;
XL3Model *xl3s[MAX_XL3_CON];
ControllerLink *contConnection;
pthread_mutex_t startTestLock;
char finalTestIDs[19][16][500];
char ecalID[500];

long int startTime=0, endTime=0, lastPrintTime=0, recvBytes=0, megaBundleCount=0; 
int writeLog = 0;
FILE *logFile, *ecalLogFile;

int NEED_TO_SWAP;
char MTC_XILINX_LOCATION[100];
char DEFAULT_SSHKEY[100];
char DB_ADDRESS[100];
char DB_PORT[100];
char DB_USERNAME[100];
char DB_PASSWORD[100];
char DB_BASE_NAME[100];
char DB_VIEWDOC[100];
char FECDB_ADDRESS[100];
char FECDB_PORT[100];
char FECDB_USERNAME[100];
char FECDB_PASSWORD[100];
char FECDB_BASE_NAME[100];
char FECDB_VIEWDOC[100];
int MAX_PENDING_CONS;
int XL3_PORT;
int SBC_PORT;
char SBC_USER[100];
char SBC_SERVER[100];
int CONT_PORT;
char CONT_CMD_ACK[100];
char CONT_CMD_BSY[100];
int VIEW_PORT;
int BUNDLE_PRINT;
int CURRENT_LOCATION;
char ORCA_READOUT_PATH[100];

char DB_SERVER[100];
char FECDB_SERVER[100];

char *PENN_DAQ_ROOT;


struct event_base *evBase;

int LockConnections(int sbc, uint32_t xl3List)
{
  int failFlag = 0;
  pthread_mutex_lock(&startTestLock);
  if (sbc == 1){
    if (mtc->CheckLock())
      failFlag = 1;
  }
  if (sbc == 2){
    if (mtc->CheckLock() == 2)
      failFlag = 1;
  }
  for (int i=0;i<MAX_XL3_CON;i++){
    if ((0x1<<i) & xl3List){
      if (xl3s[i]->CheckLock()){
        failFlag = 1;
      }
    }
  }

  if (!failFlag){
    if (sbc){
      mtc->Lock();
    }
    for (int i=0;i<MAX_XL3_CON;i++){
      if ((0x1<<i) & xl3List){
        xl3s[i]->Lock();
      }
    }
  }
  pthread_mutex_unlock(&startTestLock);
  return failFlag;
}

int UnlockConnections(int sbc, uint32_t xl3List)
{
  pthread_mutex_lock(&startTestLock);
  if (sbc){
    mtc->UnLock();
  }
  for (int i=0;i<MAX_XL3_CON;i++){
    if ((0x1<<i) & xl3List){
      xl3s[i]->UnLock(); 
    }
  }
  pthread_mutex_unlock(&startTestLock);
  return 0;
}

void SwapLongBlock(void* p, int32_t n)
{
  if (NEED_TO_SWAP){
    int32_t* lp = (int32_t*)p;
    int32_t i;
    for(i=0;i<n;i++){
      int32_t x = *lp;
      *lp = (((x) & 0x000000FF) << 24) |
        (((x) & 0x0000FF00) << 8) |
        (((x) & 0x00FF0000) >> 8) |
        (((x) & 0xFF000000) >> 24);
      lp++;
    }
  }
}

void SwapShortBlock(void* p, int32_t n)
{
  if (NEED_TO_SWAP){
    int16_t* sp = (int16_t*)p;
    int32_t i;
    for(i=0;i<n;i++){
      int16_t x = *sp;
      *sp = ((x & 0x00FF) << 8) |
        ((x & 0xFF00) >> 8) ;
      sp++;
    }
  }
}

int readConfigurationFile()
{

  PENN_DAQ_ROOT = getenv("PENN_DAQ_ROOT2");
  if (PENN_DAQ_ROOT == NULL){
    printf("You need to set the environment variable PENN_DAQ_ROOT2 to the penn_daq directory\n");
    exit(-1);
  }
  FILE *config_file;
  char filename[1000];
  sprintf(filename,"%s/%s",PENN_DAQ_ROOT,CONFIG_FILE_LOC);
  config_file = fopen(filename,"r");
  if (config_file == NULL){
    lprintf("WARNING! Could not open configuration file! Using default.\n");
    sprintf(filename,"%s/%s",PENN_DAQ_ROOT,DEFAULT_CONFIG_FILE_LOC);
    config_file = fopen(filename,"r");
    if (config_file == NULL){
      lprintf("Problem opening default configuration document! Looking for %s. Exiting\n",filename);
      return -1;
    }
  }
  int i,n = 0;
  char line_in[100][100];
  memset(DB_USERNAME,0,100);
  memset(DB_PASSWORD,0,100);
  memset(DEFAULT_SSHKEY,0,100);
  while (fscanf(config_file,"%s",line_in[n]) == 1){
    n++;
  }
  for (i=0;i<n;i++){
    char *var_name,*var_value;
    var_name = strtok(line_in[i],"=");
    if (var_name != NULL){
      var_value = strtok(NULL,"\n");
      if (var_name[0] != '#' && var_value != NULL){
        if (strcmp(var_name,"NEED_TO_SWAP")==0){
          NEED_TO_SWAP = atoi(var_value);
        }else if (strcmp(var_name,"MTC_XILINX_LOCATION")==0){
          strcpy(MTC_XILINX_LOCATION,var_value);
        }else if (strcmp(var_name,"DEFAULT_SSHKEY")==0){
          strcpy(DEFAULT_SSHKEY,var_value);
        }else if (strcmp(var_name,"DB_ADDRESS")==0){
          strcpy(DB_ADDRESS,var_value);
        }else if (strcmp(var_name,"DB_PORT")==0){
          strcpy(DB_PORT,var_value);
        }else if (strcmp(var_name,"DB_USERNAME")==0){
          strcpy(DB_USERNAME,var_value);
        }else if (strcmp(var_name,"DB_PASSWORD")==0){
          strcpy(DB_PASSWORD,var_value);
        }else if (strcmp(var_name,"DB_BASE_NAME")==0){
          strcpy(DB_BASE_NAME,var_value);
        }else if (strcmp(var_name,"DB_VIEWDOC")==0){
          strcpy(DB_VIEWDOC,var_value);
        }else if (strcmp(var_name,"FECDB_ADDRESS")==0){
          strcpy(FECDB_ADDRESS,var_value);
        }else if (strcmp(var_name,"FECDB_PORT")==0){
          strcpy(FECDB_PORT,var_value);
        }else if (strcmp(var_name,"FECDB_USERNAME")==0){
          strcpy(FECDB_USERNAME,var_value);
        }else if (strcmp(var_name,"FECDB_PASSWORD")==0){
          strcpy(FECDB_PASSWORD,var_value);
        }else if (strcmp(var_name,"FECDB_BASE_NAME")==0){
          strcpy(FECDB_BASE_NAME,var_value);
        }else if (strcmp(var_name,"FECDB_VIEWDOC")==0){
          strcpy(FECDB_VIEWDOC,var_value);
        }else if (strcmp(var_name,"MAX_PENDING_CONS")==0){
          MAX_PENDING_CONS = atoi(var_value);
        }else if (strcmp(var_name,"XL3_PORT")==0){
          XL3_PORT = atoi(var_value);
        }else if (strcmp(var_name,"SBC_PORT")==0){
          SBC_PORT = atoi(var_value);
        }else if (strcmp(var_name,"SBC_USER")==0){
          strcpy(SBC_USER,var_value);
        }else if (strcmp(var_name,"SBC_SERVER")==0){
          strcpy(SBC_SERVER,var_value);
        }else if (strcmp(var_name,"ORCA_READOUT_PATH")==0){
          strcpy(ORCA_READOUT_PATH,var_value);
        }else if (strcmp(var_name,"CONT_PORT")==0){
          CONT_PORT = atoi(var_value);
        }else if (strcmp(var_name,"CONT_CMD_ACK")==0){
          strcpy(CONT_CMD_ACK,var_value);
        }else if (strcmp(var_name,"CONT_CMD_BSY")==0){
          strcpy(CONT_CMD_BSY,var_value);
        }else if (strcmp(var_name,"VIEW_PORT")==0){
          VIEW_PORT = atoi(var_value);
        }else if (strcmp(var_name,"BUNDLE_PRINT")==0){
          BUNDLE_PRINT = atoi(var_value);
        }else if (strcmp(var_name,"CURRENT_LOCATION")==0){
          if (strncmp(var_value,"penn",3) == 0)
            CURRENT_LOCATION = PENN_TESTSTAND;
          else if (strncmp(var_value,"ag",2) == 0)
            CURRENT_LOCATION = ABOVE_GROUND_TESTSTAND;
          else
            CURRENT_LOCATION = UNDERGROUND;
        }
      }
    }
  }
  sprintf(DB_SERVER,"http://%s:%s@%s:%s",DB_USERNAME,DB_PASSWORD,DB_ADDRESS,DB_PORT);
  sprintf(FECDB_SERVER,"http://%s:%s@%s:%s",FECDB_USERNAME,FECDB_PASSWORD,FECDB_ADDRESS,FECDB_PORT);
  fclose(config_file);
  lprintf("done reading config\n");
  return 0; 
}

int GetInt(const char *input, char flag, int dflt)
{
  char buffer[10000];
  memset(buffer,'\0',10000);
  memcpy(buffer,input,strlen(input));
  char *words,*words2;
  words = strtok(buffer, " ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == flag){
        if ((words2 = strtok(NULL, " ")) != NULL)
          return atoi(words2);
      }
    }
    words = strtok(NULL, " ");
  }

  return dflt; 
}

uint32_t GetUInt(const char *input, char flag, uint32_t dflt)
{
  char buffer[10000];
  memset(buffer,'\0',10000);
  memcpy(buffer,input,strlen(input));
  char *words,*words2;
  words = strtok(buffer, " ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == flag){
        if ((words2 = strtok(NULL, " ")) != NULL){
          return strtoul(words2,(char**)NULL,16);
        }
      }
    }
    words = strtok(NULL, " ");
  }

  return dflt; 
}

int GetFlag(const char *input, char flag)
{
  char buffer[10000];
  memset(buffer,'\0',10000);
  memcpy(buffer,input,strlen(input));
  char *words,*words2;
  words = strtok(buffer, " ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == flag){
        return 1;
      }
    }
    words = strtok(NULL, " ");
  }

  return 0;
}

int GetMultiUInt(const char *input, int num, char flag, uint32_t *results, uint32_t dflt)
{
  for (int i=0;i<num;i++)
    results[i] = dflt; 
  int ascii = 48;
  char buffer[10000];
  memset(buffer,'\0',10000);
  memcpy(buffer,input,strlen(input));
  char *words,*words2;
  words = strtok(buffer, " ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == flag){
        if ((words2 = strtok(NULL, " ")) != NULL)
          for (int j=0;j<num;j++)
            results[j] = strtoul(words2,(char**) NULL,16); 
      }else{

        int done = 0;
        for (int i=0;i<=(num/10);i++){
          if (done) break;
          if (words[1] == (i+ascii)){
            for (int j=0;j<=((num-i*10)%10);j++){
              if (words[2] == (j+ascii)){
                if ((words2 = strtok(NULL, " ")) != NULL)
                  results[i*10+j] = strtoul(words2,(char**) NULL,16); 
                done = 1;
                break;
              }
            }
          }
        }
      }
    }
    words = strtok(NULL, " ");
  }
  return 0;
}

float GetFloat(const char *input, char flag, float dflt)
{
  char buffer[10000];
  memset(buffer,'\0',10000);
  memcpy(buffer,input,strlen(input));
  char *words,*words2;
  words = strtok(buffer, " ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == flag){
        if ((words2 = strtok(NULL, " ")) != NULL){
          return (float) strtod(words2,(char**)NULL);
        }
      }
    }
    words = strtok(NULL, " ");
  }

  return dflt; 
}

int GetMultiFloat(const char *input, int num, char flag, float *results, float dflt)
{
  for (int i=0;i<num;i++)
    results[i] = dflt; 
  int ascii = 48;
  char buffer[10000];
  memset(buffer,'\0',10000);
  memcpy(buffer,input,strlen(input));
  char *words,*words2;
  words = strtok(buffer, " ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == flag){
        if ((words2 = strtok(NULL, " ")) != NULL)
          for (int j=0;j<num;j++)
            results[j] = (float) strtod(words2,(char**)NULL);
      }else{

        int done = 0;
        for (int i=0;i<=(num/10);i++){
          if (done) break;
          if (words[1] == (i+ascii)){
            for (int j=0;j<=((num-i*10)%10);j++){
              if (words[2] == (j+ascii)){
                if ((words2 = strtok(NULL, " ")) != NULL)
                  results[i*10+j] = (float) strtod(words2,(char**)NULL);
                done = 1;
                break;
              }
            }
          }
        }
      }
    }
    words = strtok(NULL, " ");
  }
  return 0;
}

int GetString(const char *input, char *result, char flag, const char *dflt)
{
  strcpy(result,dflt);
  char buffer[10000];
  memset(buffer,'\0',10000);
  memcpy(buffer,input,strlen(input));
  char *words,*words2;
  words = strtok(buffer, " ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == flag){
        if ((words2 = strtok(NULL, " ")) != NULL)
          strcpy(result,words2);
      }
    }
    words = strtok(NULL, " ");
  }
  return 0;
}

uint32_t GetBits(uint32_t value, uint32_t bit_start, uint32_t num_bits)
{
  uint32_t bits;
  bits = (value >> (bit_start + 1 - num_bits)) & ~(~0 << num_bits);
  return bits;
}

int ParseMacro(char *filename)
{
  FILE *file;
  char long_filename[250];
  sprintf(long_filename,"%s/macro/%s",PENN_DAQ_ROOT,filename);
  file = fopen(long_filename,"r");
  if (file == NULL){
    lprintf("Could not open macro file!\n");
    return -1;
  }
  char buffer[1000];
  while (fgets(buffer,1000,file) != NULL){
    lprintf("Command: %s\n",buffer);
    ControllerLink::ProcessCommand((void *) buffer); 
  }
  fclose(file);
  return 0;
}

int StartLogging(){
  if (!writeLog){
    writeLog = 1;
    char logName[500] = {'\0'};
    time_t curtime = time(NULL);
    struct timeval moretime;
    gettimeofday(&moretime,0);
    struct tm *loctime = localtime(&curtime);

    sprintf(logName,"%s/logs/",PENN_DAQ_ROOT);
    strftime(logName+strlen(logName), 256, "%Y_%m_%d_%H_%M_%S_", loctime);
    sprintf(logName+strlen(logName), "%d.log", (int)moretime.tv_usec);
    logFile = fopen(logName, "a+");
    if (logFile == NULL){
      lprintf("Problem enabling logging: Could not open log file!\n");
      writeLog = 0;
    }else{
      lprintf( "Enabled logging\n");
      lprintf( "Opened log file: %s\n", logName);
    }
  }else{
    lprintf("Logging already enabled\n");
  }
  return 0;
}

int StopLogging(){
  if(writeLog){
    writeLog = 0;
    lprintf("Disabled logging\n");
    if(logFile){
      lprintf("Closed log file\n");
      fclose(logFile);
    }
    else{
      lprintf("\tNo log file to close\n");
    }
  }
  else{
    lprintf("Logging is already disabled\n");
  }
  return 0;
}

int PrintHelp(char *buffer)
{
  int which = 0;
  char *words,*words2;
  words = strtok(buffer," ");
  while (words != NULL){
    if (strncmp(words,"xl3_registers",13) == 0){
      which = 1;
    }else if (strncmp(words,"fec_registers",13) == 0){
      which = 2;
    }else if (strncmp(words,"mtc_registers",13) == 0){
      which = 3;
    }else if (strncmp(words,"xl3",3) == 0){
      which = 4;
    }else if (strncmp(words,"fec",3) == 0){
      which = 5;
    }else if (strncmp(words,"mtc",3) == 0){
      which = 6;
    }else if (strncmp(words,"daq",3) == 0){
      which = 7;
    }else if (strncmp(words,"tests",4) == 0){
      which = 8;
    }else if (strncmp(words,"all",3) == 0){
      which = 9;
    }
    words = strtok(NULL," ");
  }
  int i,j;
  if (which == 0){
    lprintf("Type \"help topic\" to get more help about \"topic\"\n");
    lprintf("Topics:\n");
    lprintf("all\t\t\tView a list of all commands\n");
    lprintf("xl3\t\t\tView a list of xl3 commands\n");
    lprintf("fec\t\t\tView a list of fec commands\n");
    lprintf("mtc\t\t\tView a list of mtc commands\n");
    lprintf("daq\t\t\tView a list of daq commands\n");
    lprintf("tests\t\t\tView a list of tests\n");
    lprintf("xl3_registers\t\tView a list of register numbers and addresses\n");
    lprintf("fec_registers\t\tView a list of register numbers and addresses\n");
    lprintf("mtc_registers\t\tView a list of register numbers and addresses\n");
  }
  if (which == 1){
    lprintf("XL3 Registers:\n");
    for (i=0;i<14;i++)
      lprintf("%2d: (%08x) %s\n",i,xl3RegAddresses[i],xl3RegNames[i]);
  }
  if (which == 2){
    lprintf("FEC Registers:\n");
    for (i=0;i<20;i++)
      lprintf("%2d: (%08x) %s\n",i,fecRegAddresses[i],fecRegNames[i]);
    for (i=0;i<8;i++)
      lprintf("%3d - %3d: (%08x + 0x8*num) %s\n",20+i*32,20+i*32+31,fecRegAddresses[20+i],fecRegNames[20+i]);
  }
  if (which == 3){
    lprintf("MTC Registers:\n");
    for (i=0;i<21;i++)
      lprintf("%2d: (%08x) %s\n",i,mtcRegAddresses[i],mtcRegNames[i]);
  }
  if (which == 7 || which == 9){
    lprintf("print_connected\t\tPrints all connected devices\n");
    lprintf("stop_logging\t\tStop logging output to a file\n");
    lprintf("start_logging\t\tStart logging output to a file\n");
    //lprintf("set_location\t\tSet current location\n");
    lprintf("sbc_control\t\tConnect or reconnect to sbc\n");
    lprintf("clear_screen\t\tClear screen of output\n");
    lprintf("reset_speed\t\tReset speed calculated for pedestal runs\n");
    lprintf("run_macro\t\tRun a list of commands from a file\n");
  }
  if (which == 4 || which == 9){
    lprintf("xr\t\t\tRead from an xl3 register specified by number\n");
    lprintf("xw\t\t\tWrite to an xl3 register specified by number\n");
    lprintf("xl3_rw\t\t\tRead/Write to xl3/fec registers by address\n");
    lprintf("xl3_queue_rw\t\tRead/Write to xl3/fec using command queue\n");
    lprintf("debugging_on\t\tTurn on debugging output to serial port\n");
    lprintf("debugging_off\t\tTurn off debugging output to serial port\n");
    lprintf("change_mode\t\tTurn readout on or off\n");
    lprintf("sm_reset\t\tReset vhdl state mahine\n");
    lprintf("read_local_voltage\t\tRead a voltage on the xl3\n");
    lprintf("hv_readbck\t\tMonitor HV voltage and current\n");
    lprintf("crate_init\t\tInitialize crate\n");
  }
  if (which == 5 || which == 9){
    lprintf("fr\t\t\tRead from a fec register specified by number\n");
    lprintf("fw\t\t\tWrite to a fec register specified by number\n");
    lprintf("read_bundle\t\tRead a single bundle and print it\n");
  }
  if (which == 6 || which == 9){
    lprintf("mr\t\t\tRead from a mtc register specified by number\n");
    lprintf("mw\t\t\tWrite to a mtc register specified by number\n");
    lprintf("mtc_read\t\tRead from a mtc register by address\n");
    lprintf("mtc_write\t\tWrite to a mtc register by address\n");
    lprintf("mtc_init\t\tInitialize mtc\n");
    lprintf("set_mtca_thresholds\n");
    lprintf("set_gt_mask\n");
    lprintf("set_gt_crate_mask\n");
    lprintf("set_ped_crate_mask\n");
    lprintf("enable_pulser\n");
    lprintf("disable_pulser\n");
    lprintf("enable_pedestal\n");
    lprintf("disable_pedestal\n");
    lprintf("set_pulser_freq\n");
    lprintf("send_softgt\n");
    lprintf("multi_softgt\n");
    lprintf("mtc_delay\n");
  }
  if (which == 8 || which == 9){
    lprintf("run_pedestals\t\tEnable pedestals on mtc and readout on crates\n");
    lprintf("run_pedestals_mtc\t\tEnable pedestals and pulser on mtc\n");
    lprintf("run_pedestals_crate\t\tEnable pedestals and readout on crate\n");
    lprintf("run_pedestals_end\t\tStop pulser and end readout\n");
    lprintf("run_pedestals_end_mtc\t\tStop pulser and pedestals\n");
    lprintf("run_pedestals_end_crate\t\tDisable pedestals and stop readout\n");
    lprintf("trigger_scan\n");
    lprintf("fec_test\n");
    lprintf("mem_test\n");
    lprintf("vmon\n");
    lprintf("board_id\n");
    lprintf("ped_run\n");
    lprintf("zdisc\n");
    lprintf("crate_cbal\n");
    lprintf("cgt_test\n");
    lprintf("gtvalid_test\n");
    lprintf("cald_test\n");
    lprintf("get_ttot\n");
    lprintf("set_ttot\n");
    lprintf("fifo_test\n");
    lprintf("disc_check\n");
    lprintf("mb_stability_test\n");
    lprintf("chinj_scan\n");
    lprintf("final_test\n");
  }
  lprintf("-----------------------------------------\n");

  return 0;
}

int lfprintf(const char *fmt, ... )
{
  int ret;
  va_list arg;
  char psb[5000];
  va_start(arg,fmt);
  ret = vsprintf(psb,fmt,arg);

  if (writeLog && logFile)
    fprintf(logFile,"%s",psb);
  if (ecalID && ecalLogFile)
    fprintf(ecalLogFile,"%s",psb);
  return ret;
}

int lprintf(const char *fmt, ... )
{
  int ret;
  va_list arg;
  char psb[5000];
  va_start(arg,fmt);
  ret = vsprintf(psb,fmt,arg);
  fputs(psb,stdout);

  if (writeLog && logFile)
    fprintf(logFile,"%s",psb);
  if (ecalID && ecalLogFile)
    fprintf(ecalLogFile,"%s",psb);
  return ret;
}
