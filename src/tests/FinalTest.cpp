#include "Globals.h"
#include "Json.h"
#include "DB.h"

#include "BoardID.h"
#include "FECTest.h"
#include "VMon.h"
#include "CGTTest.h"
#include "PedRun.h"
#include "TTot.h"
#include "DiscCheck.h"
#include "GTValidTest.h"
#include "CrateCBal.h"
#include "ChinjScan.h"
#include "ZDisc.h"
#include "MbStabilityTest.h"
#include "FifoTest.h"
#include "CaldTest.h"
#include "SeeReflection.h"
#include "MemTest.h"

#include "XL3Model.h"
#include "MTCModel.h"
#include "ControllerLink.h"
#include "XL3Cmds.h"
#include "MTCCmds.h"
#include "FinalTest.h"

int FinalTest(int crateNum, uint32_t slotMask, uint32_t testMask, int skip)
{

  int updateDB = 1;
  lprintf("*** Starting Final Test ****************\n");

  JsonNode *ftDocs[16];
  char comments[1000];
  memset(comments,'\0',1000);
  int errors;
  // initialize the crate
  errors = CrateInit(crateNum,slotMask,1,1,0,0,0,0,0,0,0,0);
  if (errors){
    lprintf("Problem initializing the crate, exiting final test\n");
    return -1;
  }

  errors = MTCInit(1); 
  if (errors){
    lprintf("Problem intializing the mtcd, exiting final test\n");
  }

  lprintf("----------------------------------------\n");
  lprintf("If any boards could not initialize properly, type \"quit\" now "
      "to exit the test.\n Otherwise hit enter to continue.\n");
  contConnection->GetInput(comments,1000);
  if (strncmp("quit",comments,4) == 0){
    lprintf("Exiting final test\n");
    lprintf("****************************************\n");
    return 0;
  }

  for (int i=0;i<16;i++){
    if ((0x1<<i) & slotMask){
      GetNewID(finalTestIDs[crateNum][i]);
      ftDocs[i] = json_mkobject();
    }
  }

  lprintf("Now starting board_id\n");
  BoardID(crateNum,slotMask);
  lprintf("----------------------------------------\n");

  if (!skip && updateDB){
    for (int i=0;i<16;i++){
      if ((0x1<<i) & slotMask){
        lprintf("Please enter any comments for slot %i motherboard now.\n",i);
        contConnection->GetInput(comments);
        json_append_member(ftDocs[i],"fec_comments",json_mkstring(comments));
        lprintf("Has this slot been refurbished? (y/n)\n",i);
        contConnection->GetInput(comments);
        json_append_member(ftDocs[i],"refurbished",json_mkbool(comments[0] == 'y'));
        lprintf("Has this slot been cleaned? (y/n)\n",i);
        contConnection->GetInput(comments);
        json_append_member(ftDocs[i],"cleaned",json_mkbool(comments[0] == 'y'));
        lprintf("Time to measure resistance across analog outs and cmos address lines. For the cmos address lines"
            "it's easier if you do it during the fifo mod\n");
        contConnection->GetInput(comments);
        json_append_member(ftDocs[i],"analog_out_res",json_mkstring(comments));
        lprintf("Please enter any comments for slot %i db 0 now.\n",i);
        contConnection->GetInput(comments);
        json_append_member(ftDocs[i],"db0_comments",json_mkstring(comments));
        lprintf("Please enter any comments for slot %i db 1 now.\n",i);
        contConnection->GetInput(comments);
        json_append_member(ftDocs[i],"db1_comments",json_mkstring(comments));
        lprintf("Please enter any comments for slot %i db 2 now.\n",i);
        contConnection->GetInput(comments);
        json_append_member(ftDocs[i],"db2_comments",json_mkstring(comments));
        lprintf("Please enter any comments for slot %i db 3 now.\n",i);
        contConnection->GetInput(comments);
        json_append_member(ftDocs[i],"db3_comments",json_mkstring(comments));
        lprintf("Please enter dark matter measurements for slot %i db 0 now.\n",i);
        contConnection->GetInput(comments);
        json_append_member(ftDocs[i],"db0_dark_matter",json_mkstring(comments));
        lprintf("Please enter dark matter measurements for slot %i db 1 now.\n",i);
        contConnection->GetInput(comments);
        json_append_member(ftDocs[i],"db1_dark_matter",json_mkstring(comments));
        lprintf("Please enter dark matter measurements for slot %i db 2 now.\n",i);
        contConnection->GetInput(comments);
        json_append_member(ftDocs[i],"db2_dark_matter",json_mkstring(comments));
        lprintf("Please enter dark matter measurements for slot %i db 3 now.\n",i);
        contConnection->GetInput(comments);
        json_append_member(ftDocs[i],"db3_dark_matter",json_mkstring(comments));
      }
    }


    lprintf("Enter N100 DC offset\n");
    contConnection->GetInput(comments);
    for (int i=0;i<16;i++){
      if ((0x1<<i) & slotMask){
        json_append_member(ftDocs[i],"dc_offset_n100",json_mkstring(comments));
      }
    }
    lprintf("Enter N20 DC offset\n");
    contConnection->GetInput(comments);
    for (int i=0;i<16;i++){
      if ((0x1<<i) & slotMask){
        json_append_member(ftDocs[i],"dc_offset_n20",json_mkstring(comments));
      }
    }
    lprintf("Enter esum hi DC offset\n");
    contConnection->GetInput(comments);
    for (int i=0;i<16;i++){
      if ((0x1<<i) & slotMask){
        json_append_member(ftDocs[i],"dc_offset_esumhi",json_mkstring(comments));
      }
    }
    lprintf("Enter esum lo DC offset\n");
    contConnection->GetInput(comments);
    for (int i=0;i<16;i++){
      if ((0x1<<i) & slotMask){
        json_append_member(ftDocs[i],"dc_offset_esumlo",json_mkstring(comments));
      }
    }

    lprintf("Thank you. Please hit enter to continue with the rest of final test. This may take a while.\n");
    contConnection->GetInput(comments);
  }

  // update the database
  if (updateDB){
  for (int i=0;i<16;i++){
    if ((0x1<<i) & slotMask){
      json_append_member(ftDocs[i],"type",json_mkstring("final_test"));
      PostDebugDocWithID(crateNum, i, finalTestIDs[crateNum][i], ftDocs[i]);
    }
  }
  }

  lprintf("----------------------------------------\n");

  int testCounter = 0;

  if ((0x1<<testCounter) & testMask)
    FECTest(crateNum,slotMask,updateDB,1);
  testCounter++;

  if ((0x1<<testCounter) & testMask)
    VMon(crateNum,slotMask,updateDB,1);
  testCounter++;
  if ((0x1<<testCounter) & testMask)
    CGTTest(crateNum,slotMask,0xFFFFFFFF,updateDB,1);
  testCounter++;
  CrateInit(crateNum,slotMask,1,0,0,0,0,0,0,0,0,0);
  if ((0x1<<testCounter) & testMask)
    PedRun(crateNum,slotMask,0xFFFFFFFF,0,DEFAULT_GT_DELAY,DEFAULT_PED_WIDTH,50,1000,300,updateDB,0,1);
  testCounter++;
  CrateInit(crateNum,slotMask,1,0,0,0,0,0,0,0,0,0);
  MTCInit(1);
  if ((0x1<<testCounter) & testMask)
    CrateCBal(crateNum,slotMask,0xFFFFFFFF,updateDB,1);
  testCounter++;

  lprintf("----------------------------------------\n");

  // load cbal values now
  CrateInit(crateNum,slotMask,0,0,0,1,0,0,0,0,0,0);

  if ((0x1<<testCounter) & testMask)
    PedRun(crateNum,slotMask,0xFFFFFFFF,0,DEFAULT_GT_DELAY,DEFAULT_PED_WIDTH,50,1000,300,updateDB,1,1);
  testCounter++;

  if ((0x1<<testCounter) & testMask)
    ChinjScan(crateNum,slotMask,0xFFFFFFFF,0,DEFAULT_GT_DELAY,DEFAULT_PED_WIDTH,10,5000,400,0,1,updateDB,1);
  testCounter++;
  if ((0x1<<testCounter) & testMask)
    SetTTot(crateNum,slotMask,400,updateDB,1);
  testCounter++;

  // load cbal and ttot values now
  CrateInit(crateNum,slotMask,0,0,0,1,0,1,0,0,0,0);

  if ((0x1<<testCounter) & testMask)
    GetTTot(crateNum,slotMask,390,updateDB,1);
  testCounter++;
  if ((0x1<<testCounter) & testMask)
    DiscCheck(crateNum,slotMask,500000,updateDB,1);
  testCounter++;
  if ((0x1<<testCounter) & testMask)
    GTValidTest(crateNum,slotMask,0xFFFFFFFF,400,0,updateDB,1);
  testCounter++;
  if ((0x1<<testCounter) & testMask)
    ZDisc(crateNum,slotMask,10000,0,updateDB,1);
  testCounter++;

  MTCInit(0);
  CrateInit(crateNum,slotMask,1,0,0,0,0,0,0,0,0,0);

  if ((0x1<<testCounter) & testMask)
    MbStabilityTest(crateNum,slotMask,50,updateDB,1);
  testCounter++;
  if ((0x1<<testCounter) & testMask)
    FifoTest(crateNum,slotMask,updateDB,1);
  testCounter++;

  // load alternate xilinx
  CrateInit(crateNum,slotMask,2,0,0,0,0,0,0,0,0,0);

  if ((0x1<<testCounter) & testMask)
    CaldTest(crateNum,slotMask,3500,750,200,1,updateDB,1);
  testCounter++;

  for (int i=0;i<16;i++){
    if ((0x1<<i) & slotMask){
      CrateInit(crateNum,slotMask,1,0,0,0,0,0,0,0,0,0); 
      if ((0x1<<testCounter) & testMask)
        MemTest(crateNum,i,updateDB,1);
    }
  }
  testCounter++;

  CrateInit(crateNum,slotMask,1,0,0,0,0,0,0,0,0,0); 

  if ((0x1<<testCounter) & testMask){
    lprintf("Ready for see_refl test. You should check if the xilinx loaded correctly. If it did, hit enter to continue. Otherwise, power cycle the crate and then type 'init' to crate_init again, or type 'quit' to skip see_refl and end the final test\n");

    contConnection->GetInput(comments);
    if (strncmp("init",comments,4) == 0){
      CrateInit(crateNum,slotMask,1,0,0,0,0,0,0,0,0,0); 
      SeeReflection(crateNum,slotMask,0xFFFFFFFF,255,1000,updateDB,1);
    }else if (strncmp("quit",comments,4) != 0){
      SeeReflection(crateNum,slotMask,0xFFFFFFFF,255,1000,updateDB,1);
    }
  }

  lprintf("----------------------------------------\n");
  lprintf("Final Test finished!\n");
  lprintf("****************************************\n");
  return 0;
}

