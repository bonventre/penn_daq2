#include "Globals.h"

MTCModel *mtc;
XL3Model *xl3s[MAX_XL3_CON];
ControllerLink *contConnection;
pthread_mutex_t startTestLock;

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
  FILE *config_file;
  char filename[1000];
  sprintf(filename,"%s/%s",PENN_DAQ_ROOT,CONFIG_FILE_LOC);
  config_file = fopen(filename,"r");
  if (config_file == NULL){
    printf("WARNING! Could not open configuration file! Using default.\n");
    sprintf(filename,"%s/%s",PENN_DAQ_ROOT,DEFAULT_CONFIG_FILE_LOC);
    config_file = fopen(filename,"r");
    if (config_file == NULL){
      printf("Problem opening default configuration document! Looking for %s. Exiting\n",filename);
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
      var_value = strtok(NULL,"=");
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
  printf("done reading config\n");
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
      int done = 0;
      for (int i=0;i<=(num/10);i++){
        if (done) break;
        if (words[1] == flag){
          if ((words2 = strtok(NULL, " ")) != NULL)
            for (int j=0;j<num;j++)
              results[j] = strtoul(words2,(char**) NULL,16); 
        }
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
      int done = 0;
      for (int i=0;i<=(num/10);i++){
        if (done) break;
        if (words[1] == flag){
          if ((words2 = strtok(NULL, " ")) != NULL)
            for (int j=0;j<num;j++)
              results[j] = (float) strtod(words2,(char**)NULL); 
        }
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
