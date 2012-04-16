#include "Globals.h"
#include "DBTypes.h"
#include "Pouch.h"

#include "DB.h"
#include "MTCModel.h"
#include "MTCLink.h"
#include "MTCCmds.h"

int SBCControl(int connect, int kill, int manual, const char *idFile)
{
  char base_cmd[500];
  if (strcmp(idFile,"") != 0)
    sprintf(base_cmd,"ssh %s@%s -i %s",SBC_USER,SBC_SERVER,idFile);
  else
    sprintf(base_cmd,"ssh %s@%s",SBC_USER,SBC_SERVER);

  if (kill){
    mtc->CloseConnection(); 
    char kill_cmd[500];
    sprintf(kill_cmd,"%s %s stop",base_cmd,ORCA_READOUT_PATH);
    printf("sbc_control: Stopping remote OrcaReadout process\n");
    system(kill_cmd);
  }

  usleep(50000);

  if (connect){
    if (!manual){
      char start_cmd[500];
      sprintf(start_cmd,"%s %s start",base_cmd,ORCA_READOUT_PATH);
      printf("sbc_control: Starting remote OrcaReadout process\n");
      system(start_cmd);
    }
    mtc->Connect();
  }
  return 0;
}

int MTCInit(int xilinx)
{
  printf("Initializing MTCD/A+\n");
  MTC *mtc = ( MTC * ) malloc( sizeof(MTC));
  if ( mtc == ( MTC *) NULL )
  {
    printf("Error: malloc in mtc_init\n");
    free(mtc);
    return -1;
  }

  pouch_request *response = pr_init();
  char get_db_address[500];
  sprintf(get_db_address,"%s/%s/MTC_doc",DB_SERVER,DB_BASE_NAME);
  pr_set_method(response, GET);
  pr_set_url(response, get_db_address);
  pr_do(response);
  if (response->httpresponse != 200){
    printf("Unable to connect to database. error code %d\n",(int)response->httpresponse);
    free(response);
    free(mtc);
    return -1;
  }
  JsonNode *doc = json_decode(response->resp.data);
  ParseMTC(doc,mtc);
  json_delete(doc);
  pr_free(response);


  return 0;
}

int MTCRead(uint32_t address)
{
  uint32_t result;
  try{
    int errors = mtc->RegRead(address,&result);
    if (errors)
      printf("There was a bus error.\n");
    else
      printf("Read from %08x, got %08x\n",address,result);
  }
  catch(int e){
    printf("There was a network error!\n");
  }

  return 0;
}

int MTCWrite(uint32_t address, uint32_t data)
{
  uint32_t result;
  try{
    int errors = mtc->RegWrite(address,data);
    if (errors)
      printf("There was a bus error.\n");
    else
      printf("Wrote to %08x\n",address);
  }
  catch(int e){
    printf("There was a network error!\n");
  }

  return 0;
}

