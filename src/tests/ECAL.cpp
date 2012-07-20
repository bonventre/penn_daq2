#include "Globals.h"
#include "Pouch.h"
#include "Json.h"
#include "DB.h"

#include "BoardID.h"
#include "FECTest.h"
#include "CGTTest.h"
#include "PedRun.h"
#include "TTot.h"
#include "DiscCheck.h"
#include "GTValidTest.h"
#include "CrateCBal.h"
#include "ZDisc.h"
#include "FindNoise.h"

#include "XL3Model.h"
#include "MTCModel.h"
#include "ControllerLink.h"
#include "XL3Cmds.h"
#include "MTCCmds.h"
#include "ECAL.h"

int ECAL(uint32_t crateMask, uint32_t *slotMasks, uint32_t testMask, const char* loadECAL, int createFECDocs)
{
  time_t curtime = time(NULL);
  struct timeval moretime;
  gettimeofday(&moretime,0);
  struct tm *loctime = localtime(&curtime);
  char logName[500] = {'\0'};  // random size, it's a pretty nice number though.

  strftime(logName, 256, "ECAL_%Y_%m_%d_%H_%M_%S_", loctime);
  sprintf(logName+strlen(logName), "%d.log", (int)moretime.tv_usec);
  ecalLogFile = fopen(logName,"a+");
  if (ecalLogFile == NULL)
    lprintf("Problem enabling logging for ecal, could not open log file!\n");


  lprintf("*** Starting ECAL **********************\n");

  char comments[1000];
  memset(comments,'\0',1000);

  lprintf("\nYou have selected the following configuration:\n\n");

  memset(ecalID,'\0',sizeof(ecalID));
  if (strlen(loadECAL)){
    // get the ecal document with the configuration
    char get_db_address[500];
    sprintf(get_db_address,"%s/%s/%s",DB_SERVER,DB_BASE_NAME,loadECAL);
    pouch_request *ecaldoc_response = pr_init();
    pr_set_method(ecaldoc_response, GET);
    pr_set_url(ecaldoc_response, get_db_address);
    pr_do(ecaldoc_response);
    if (ecaldoc_response->httpresponse != 200){
      lprintf("Unable to connect to database. error code %d\n",(int)ecaldoc_response->httpresponse);
      fclose(ecalLogFile);
      return -1;
    }
    JsonNode *ecalconfig_doc = json_decode(ecaldoc_response->resp.data);

    for (int i=0;i<19;i++){
      slotMasks[i] = 0x0;
    }

    // get the configuration
    JsonNode *crates = json_find_member(ecalconfig_doc,"crates");
    int num_crates = json_get_num_mems(crates);
    for (int i=0;i<num_crates;i++){
      JsonNode *one_crate = json_find_element(crates,i);
      int crate_num = json_get_number(json_find_member(one_crate,"crate_id"));
      crateMask |= (0x1<<crate_num);
      JsonNode *slots = json_find_member(one_crate,"slots");
      int num_slots = json_get_num_mems(slots);
      for (int j=0;j<num_slots;j++){
        JsonNode *one_slot = json_find_element(slots,j);
        int slot_num = json_get_number(json_find_member(one_slot,"slot_id"));
        slotMasks[crate_num] |= (0x1<<slot_num);
      }
    }
    pr_free(ecaldoc_response);
    json_delete(ecalconfig_doc);

    for (int i=0;i<19;i++)
      if ((0x1<<i) & crateMask)
        lprintf("crate %d: 0x%04x\n",i,slotMasks[i]);

    lprintf("You will be updating ECAL %s\n",loadECAL);
    strcpy(ecalID,loadECAL);
  }else{
    for (int i=0;i<19;i++)
      if ((0x1<<i) & crateMask)
    GetNewID(ecalID);
    lprintf("Creating new ECAL %s\n",ecalID);
  }
  if (testMask == 0xFFFFFFFF || (testMask & 0x3FF) == 0x3FF){
    lprintf("Doing all tests\n");
    testMask = 0xFFFFFFFF;
  }else if (testMask != 0x0){
    lprintf("Doing ");
    for (int i=0;i<11;i++)
      if ((0x1<<i) & testMask)
        lprintf("%s ",testList[i]);
    lprintf("\n");
  }else{
    lprintf("Not adding any tests\n");
  }

  lprintf("------------------------------------------\n");
  lprintf("Hit enter to start, or type quit if anything is incorrect\n");
  contConnection->GetInput(comments);
  if (strncmp("quit",comments,4) == 0){
    lprintf("Exiting ECAL\n");
    fclose(ecalLogFile);
    return 0;
  }
  lprintf("------------------------------------------\n");

  if (testMask != 0x0){

    for (int i=0;i<19;i++)
      if ((0x1<<i) & crateMask)
        CrateInit(i,slotMasks[i],1,1,0,0,0,0,0,0,0,0);
    MTCInit(1);

    lprintf("------------------------------------------\n");
    lprintf("If there were any problems initializing the crates, type quit to exit. Otherwise hit enter to continue.\n");
    contConnection->GetInput(comments);
    if (strncmp("quit",comments,4) == 0){
      lprintf("Exiting ECAL\n");
      fclose(ecalLogFile);
      return 0;
    }
    lprintf("------------------------------------------\n");


    if (strlen(loadECAL) == 0){
      lprintf("Creating ECAL document...\n");
      PostECALDoc(crateMask,slotMasks,logName,ecalID);
      lprintf("Created! Starting tests\n");
      lprintf("------------------------------------------\n");
    }

    int testCounter = 0;
    if ((0x1<<testCounter) & testMask)
      for (int i=0;i<19;i++)
        if ((0x1<<i) & crateMask)
          FECTest(i,slotMasks[i],1,0,1);
    testCounter++;

    if ((0x1<<testCounter) & testMask)
      for (int i=0;i<19;i++)
        if ((0x1<<i) & crateMask)
          BoardID(i,slotMasks[i]);
    testCounter++;

    if ((0x1<<testCounter) & testMask)
      for (int i=0;i<19;i++)
        if ((0x1<<i) & crateMask)
          CGTTest(i,slotMasks[i],0xFFFFFFFF,1,0,1);
    testCounter++;
    MTCInit(1);

    for (int i=0;i<19;i++)
      if ((0x1<<i) & crateMask)
        CrateInit(i,slotMasks[i],0,0,0,0,0,0,0,0,0,0);

    if ((0x1<<testCounter) & testMask)
      for (int i=0;i<19;i++)
        if ((0x1<<i) & crateMask)
          CrateCBal(i,slotMasks[i],0xFFFFFFFF,1,0,1);
    testCounter++;

    // load cbal values
    for (int i=0;i<19;i++)
      if ((0x1<<i) & crateMask)
        CrateInit(i,slotMasks[i],0,0,0,1,0,0,0,0,0,0);

    if ((0x1<<testCounter) & testMask)
      for (int i=0;i<19;i++)
        if ((0x1<<i) & crateMask)
          PedRun(i,slotMasks[i],0xFFFFFFFF,0,DEFAULT_GT_DELAY,DEFAULT_PED_WIDTH,50,1000,300,1,1,0,1);
    testCounter++;

    MTCInit(1);
    for (int i=0;i<19;i++)
      if ((0x1<<i) & crateMask)
        CrateInit(i,slotMasks[i],0,0,0,1,0,0,0,0,0,0);

    if ((0x1<<testCounter) & testMask)
      for (int i=0;i<19;i++)
        if ((0x1<<i) & crateMask)
          SetTTot(i,slotMasks[i],420,1,0,1);
    testCounter++;

    // load cbal and tdisc values
    for (int i=0;i<19;i++)
      if ((0x1<<i) & crateMask)
        CrateInit(i,slotMasks[i],0,0,0,1,0,1,0,0,0,0);

    if ((0x1<<testCounter) & testMask)
      for (int i=0;i<19;i++)
        if ((0x1<<i) & crateMask)
          GetTTot(i,slotMasks[i],400,1,0,1);
    testCounter++;

    for (int i=0;i<19;i++)
      if ((0x1<<i) & crateMask)
        CrateInit(i,slotMasks[i],0,0,0,1,0,1,0,0,0,0);

    if ((0x1<<testCounter) & testMask)
      for (int i=0;i<19;i++)
        if ((0x1<<i) & crateMask)
          DiscCheck(i,slotMasks[i],500000,1,0,1);
    testCounter++;

    MTCInit(1);
    for (int i=0;i<19;i++)
      if ((0x1<<i) & crateMask)
        CrateInit(i,slotMasks[i],0,0,0,1,0,1,0,0,0,0);

    if ((0x1<<testCounter) & testMask)
      for (int i=0;i<19;i++)
        if ((0x1<<i) & crateMask)
          GTValidTest(i,slotMasks[i],0xFFFFFFFF,410,0,1,0,1);
    testCounter++;

    MTCInit(1);
    // load cbal, tdisc, tcmos values
    for (int i=0;i<19;i++)
      if ((0x1<<i) & crateMask)
        CrateInit(i,slotMasks[i],0,0,0,1,0,1,1,0,0,0);

    if ((0x1<<testCounter) & testMask)
      for (int i=0;i<19;i++)
        if ((0x1<<i) & crateMask)
          ZDisc(i,slotMasks[i],10000,0,1,0,1);
    testCounter++;

    if ((0x1<<testCounter) & testMask)
      FindNoise(crateMask,slotMasks,20,1,1,1);

    lprintf("ECAL finished!\n");

  }

  if (createFECDocs)
    GenerateFECDocFromECAL(0x0, ecalID);

  lprintf("****************************************\n");
  fclose(ecalLogFile);
  return 0;
}

