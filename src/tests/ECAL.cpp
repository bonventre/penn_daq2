#include "Globals.h"
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

#include "XL3Model.h"
#include "MTCModel.h"
#include "ControllerLink.h"
#include "XL3Cmds.h"
#include "MTCCmds.h"
#include "ECAL.h"

int ECAL(uint32_t crateMask, uint32_t *slotMasks, uint32_t testMask, const char* loadECAL)
{
  time_t curtime = time(NULL);
  struct timeval moretime;
  gettimeofday(&moretime,0);
  struct tm *loctime = localtime(&curtime);
  char logName[500] = {'\0'};  // random size, it's a pretty nice number though.

  strftime(logName, 256, "ECAL_%Y_%m_%d_%H_%M_%S_", loctime);
  sprintf(logName+strlen(logName), "%d.log", (int)moretime.tv_usec);
  //FIXME
  //start_logging_to_file(logName);


  printf("*** Starting ECAL **********************\n");

  char comments[1000];
  memset(comments,'\0',1000);

  printf("\nYou have selected the following configuration:\n\n");
  for (int i=0;i<19;i++)
    if ((0x1<<i) & crateMask)
      printf("crate %d: 0x%04x\n",i,slotMasks[i]);

  memset(ecalID,'\0',sizeof(ecalID));
  if (strlen(loadECAL)){
    printf("You will be updating ECAL %s\n",loadECAL);
    strcpy(ecalID,loadECAL);
  }else{
    GetNewID(ecalID);
    printf("Creating new ECAL %s\n",ecalID);
  }
  if (testMask == 0x0 || (testMask & 0x3FF) == 0x3FF){
    printf("Doing all tests\n");
  }else{
    printf("Doing ");
    for (int i=0;i<10;i++)
      if ((0x1<<i) & testMask)
        printf("%s ",testList[i]);
    printf("\n");
  }

  printf("------------------------------------------\n");
  printf("Hit enter to start, or type quit if anything is incorrect\n");
  contConnection->GetInput(comments);
  if (strncmp("quit",comments,4) == 0){
    printf("Exiting ECAL\n");
    //DeleteLogfile();
    return 0;
  }
  printf("------------------------------------------\n");

  for (int i=0;i<19;i++)
    if ((0x1<<i) & crateMask)
      CrateInit(i,slotMasks[i],1,1,0,0,0,0,0,0,0);
  MTCInit(1);

  printf("------------------------------------------\n");
  printf("If there were any problems initializing the crates, type quit to exit. Otherwise hit enter to continue.\n");
  contConnection->GetInput(comments);
  if (strncmp("quit",comments,4) == 0){
    printf("Exiting ECAL\n");
    //DeleteLogfile();
    return 0;
  }
  printf("------------------------------------------\n");


  printf("Creating ECAL document...\n");
  PostECALDoc(crateMask,slotMasks,logName,ecalID);
  printf("Created! Starting tests\n");
  printf("------------------------------------------\n");

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
      CrateInit(i,slotMasks[i],0,0,0,0,0,0,0,0,0);

  if ((0x1<<testCounter) & testMask)
    for (int i=0;i<19;i++)
      if ((0x1<<i) & crateMask)
        CrateCBal(i,slotMasks[i],0xFFFFFFFF,1,0,1);
  testCounter++;

  // load cbal values
  for (int i=0;i<19;i++)
    if ((0x1<<i) & crateMask)
      CrateInit(i,slotMasks[i],0,0,0,1,0,0,0,0,0);

  if ((0x1<<testCounter) & testMask)
    for (int i=0;i<19;i++)
      if ((0x1<<i) & crateMask)
        PedRun(i,slotMasks[i],0xFFFFFFFF,0,DEFAULT_GT_DELAY,DEFAULT_PED_WIDTH,50,1000,300,1,1,0,1);
  testCounter++;

  MTCInit(1);
  for (int i=0;i<19;i++)
    if ((0x1<<i) & crateMask)
      CrateInit(i,slotMasks[i],0,0,0,1,0,0,0,0,0);

  if ((0x1<<testCounter) & testMask)
    for (int i=0;i<19;i++)
      if ((0x1<<i) & crateMask)
        SetTTot(i,slotMasks[i],420,1,0,1);
  testCounter++;

  // load cbal and tdisc values
  for (int i=0;i<19;i++)
    if ((0x1<<i) & crateMask)
      CrateInit(i,slotMasks[i],0,0,0,1,0,1,0,0,0);

  if ((0x1<<testCounter) & testMask)
    for (int i=0;i<19;i++)
      if ((0x1<<i) & crateMask)
        GetTTot(i,slotMasks[i],400,1,0,1);
  testCounter++;

  for (int i=0;i<19;i++)
    if ((0x1<<i) & crateMask)
      CrateInit(i,slotMasks[i],0,0,0,1,0,1,0,0,0);

  if ((0x1<<testCounter) & testMask)
    for (int i=0;i<19;i++)
      if ((0x1<<i) & crateMask)
        DiscCheck(i,slotMasks[i],500000,1,0,1);
  testCounter++;

  MTCInit(1);
  for (int i=0;i<19;i++)
    if ((0x1<<i) & crateMask)
      CrateInit(i,slotMasks[i],0,0,0,1,0,1,0,0,0);

  if ((0x1<<testCounter) & testMask)
    for (int i=0;i<19;i++)
      if ((0x1<<i) & crateMask)
        GTValidTest(i,slotMasks[i],0xFFFFFFFF,410,0,1,0,1);
  testCounter++;

  MTCInit(1);
  // load cbal, tdisc, tcmos values
  for (int i=0;i<19;i++)
    if ((0x1<<i) & crateMask)
      CrateInit(i,slotMasks[i],0,0,0,1,0,1,1,0,0);

  if ((0x1<<testCounter) & testMask)
    for (int i=0;i<19;i++)
      if ((0x1<<i) & crateMask)
        ZDisc(i,slotMasks[i],10000,0,1,0,1);

  printf("ECAL finished!\n");
  printf("****************************************\n");
  //CloseLogfile();
  return 0;
}

