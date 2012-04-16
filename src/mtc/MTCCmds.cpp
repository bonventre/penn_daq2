#include "Globals.h"
#include "DBTypes.h"
#include "Pouch.h"
#include "MTCRegisters.h"

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
  int errors = 0;

  if (xilinx)
    errors = mtc->XilinxLoad();
  if (errors){
    printf("Problem loading xilinx\n");
    return -1;
  }
  MTC *mtcdb = ( MTC * ) malloc( sizeof(MTC));
  if ( mtcdb == ( MTC *) NULL )
  {
    printf("Error: malloc in mtc_init\n");
    free(mtcdb);
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
    free(mtcdb);
    return -1;
  }
  JsonNode *doc = json_decode(response->resp.data);
  ParseMTC(doc,mtcdb);
  json_delete(doc);
  pr_free(response);

  // hold errors here
  int result = 0;
 
  //unset all masks
  mtc->UnsetGTMask(MASKALL);
  mtc->UnsetPedCrateMask(MASKALL);
  mtc->UnsetGTCrateMask(MASKALL);

  // load the dacs
  float mtca_dac_values[14];

  int i;
  for (i=0; i<=13; i++)
  { 
    uint16_t raw_dacs = (uint16_t) mtcdb->mtca.triggers[i].threshold;
    mtca_dac_values[i] = (((float) raw_dacs/2048) * 5000.0) - 5000.0;
  }

  mtc->LoadMTCADacs(mtca_dac_values);

  // clear out the control register
  mtc->RegWrite(MTCControlReg,0x0);

  // set lockout width
  uint16_t lkwidth = (uint16_t)((~((u_short) mtcdb->mtcd.lockoutWidth) & 0xff)*20);
  result += mtc->SetLockoutWidth(lkwidth);

  // zero out gt counter
  mtc->SetGTCounter(0);

  // load prescaler
  uint16_t pscale = ~((uint16_t)(mtcdb->mtcd.nhit100LoPrescale)) + 1;
  result += mtc->SetPrescale(pscale);

  // load pulser
  float freq = 781250.0/(float)((u_long)(mtcdb->mtcd.pulserPeriod) + 1);
  result += mtc->SetPulserFrequency(freq);

  // setup pedestal width
  uint16_t pwid = (uint16_t)(((~(u_short)(mtcdb->mtcd.pedestalWidth)) & 0xff) * 5);
  result += mtc->SetPedestalWidth(pwid);


  // setup PULSE_GT delays
  printf("Setting up PULSE_GT delays...\n");
  uint16_t coarse_delay = (uint16_t)(((~(uint16_t)(mtcdb->mtcd.coarseDelay)) & 0xff) * 10);
  result += mtc->SetCoarseDelay(coarse_delay);
  float fine_delay = (float)(mtcdb->mtcd.fineDelay)*(float)(mtcdb->mtcd.fineSlope);
  float fdelay_set = mtc->SetFineDelay(fine_delay);
  printf( "PULSE_GET total delay has been set to %f\n",
      (float) coarse_delay+fine_delay+
      (float)(mtcdb->mtcd.minDelayOffset));

  // load 10 MHz counter???? guess not

  // reset memory
  mtc->ResetMemory();

  free(mtcdb);

  if (result < 0) {
    printf("errors in the MTC initialization!\n");
  }else{
    printf("MTC finished initializing\n");
  }
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

