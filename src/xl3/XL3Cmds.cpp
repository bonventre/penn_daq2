#include <cstdio>

#include "Globals.h"
#include "DB.h"
#include "Pouch.h"

#include "NetUtils.h"
#include "XL3Link.h"
#include "XL3Model.h"
#include "XL3Cmds.h"

int XL3RW(int crateNum, uint32_t address, uint32_t data)
{
  printf("*** Starting XL3 RW ********************\n");
  uint32_t result;
  try{
    int errors = xl3s[crateNum]->RW(address,data,&result);
    if (errors)
      printf("There was a bus error.\n");
    else
      printf("Wrote to %08x, got %08x\n",address,result);
  }
  catch(int e){
    printf("There was a network error!\n");
  }

  printf("****************************************\n");
  return 0;
}

int XL3QueueRW(int crateNum, uint32_t address, uint32_t data)
{
  printf("*** Starting XL3 Queue RW ********************\n");

  XL3Packet packet;
  packet.header.packetNum = 0;
  packet.header.packetType = QUEUE_CMDS_ID;
  MultiCommand *commands = (MultiCommand *) packet.payload;
  commands->howMany = 1;
  SwapLongBlock(&(commands->howMany),1);
  for (int i=0;i<1;i++){
    commands->cmd[i].flags = 0;
    commands->cmd[i].data = data;
    commands->cmd[i].address = address;
    SwapLongBlock(&(commands->cmd[i].data),1);
    SwapLongBlock(&(commands->cmd[i].address),1);
  }
  try{
    xl3s[crateNum]->SendCommand(&packet);
    printf("Command queued\n");
    uint32_t result;
    xl3s[crateNum]->GetMultiFCResults(1, xl3s[crateNum]->GetLastCommandNum(), &result);
    printf("got %08x\n",result);
  }
  catch(int e){
    printf("There was a network error!\n");
  }

  printf("****************************************\n");
  return 0;
}

int FECTest(int crateNum, uint32_t slotMask)
{
  printf("*** Starting FEC Test ******************\n");
  XL3Packet packet;
  packet.header.packetType = FEC_TEST_ID;
  *(uint32_t *) packet.payload = slotMask;
  SwapLongBlock(packet.payload,1);
  try{
    xl3s[crateNum]->SendCommand(&packet);
  }
  catch(int e){
    printf("There was a network error!\n");
  }

  printf("****************************************\n");
  return 0;
}

int CrateInit(int crateNum,uint32_t slotMask, int xilinxLoad, int hvReset, int shiftRegOnly,
    int useVBal, int useVThr, int useTDisc, int useTCmos, int useAll, int useHw)
{
  printf("*** Starting Crate Init ****************\n");
  char get_db_address[500];
  char ctc_address[500];
  XL3Packet packet;
  CrateInitArgs *packetArgs = (CrateInitArgs *) packet.payload;

  printf("Initializing crate %d, slots %08x, xl: %d, hv: %d\n",
      crateNum,slotMask,xilinxLoad,hvReset);
  printf("Now sending database to XL3\n");

  try{

    pouch_request *hw_response = pr_init();
    JsonNode* hw_rows = NULL;
    pouch_request *debug_response = pr_init();
    JsonNode* debug_doc = NULL;

    if (useHw == 1){
      sprintf(get_db_address,"%s/%s/%s/get_fec?startkey=[%d,0]&endkey=[%d,16]",FECDB_SERVER,FECDB_BASE_NAME,FECDB_VIEWDOC,crateNum,crateNum);
      pr_set_method(hw_response, GET);
      pr_set_url(hw_response, get_db_address);
      pr_do(hw_response);
      if (hw_response->httpresponse != 200){
        printf("Unable to connect to database. error code %d\n",(int)hw_response->httpresponse);
        return -1;
      }
      JsonNode *hw_doc = json_decode(hw_response->resp.data);
      JsonNode* totalrows = json_find_member(hw_doc,"total_rows");
      if ((int)json_get_number(totalrows) != 16){
        printf("Database error: not enough FEC entries\n");
        return -1;
      }
      hw_rows = json_find_member(hw_doc,"rows");
      //json_delete(hw_doc); // only delete the head node
      pr_free(hw_response);
    }else{
      sprintf(get_db_address,"%s/%s/CRATE_INIT_DOC",DB_SERVER,DB_BASE_NAME);
      pr_set_method(debug_response, GET);
      pr_set_url(debug_response, get_db_address);
      pr_do(debug_response);
      if (debug_response->httpresponse != 200){
        printf("Unable to connect to database. error code %d\n",(int)debug_response->httpresponse);
        return -1;
      }
      debug_doc = json_decode(debug_response->resp.data);
      pr_free(debug_response);
    }

    // make sure crate_config is up to date
    if (useVBal || useVThr || useTDisc || useTCmos || useAll)
      xl3s[crateNum]->UpdateCrateConfig(slotMask);

    if (useVBal || useAll)
      printf("Using VBAL values from database\n");
    if (useVThr || useAll)
      printf("Using VTHR values from database\n");
    if (useTDisc || useAll)
      printf("Using TDISC values from database\n");
    if (useTCmos || useAll)
      printf("Using TCMOS values from database\n");

    // GET ALL FEC DATA FROM DB
    int i,j,crate,card;
    int irow = 0;
    for (i=0;i<16;i++){

      char configString[500];
      sprintf(configString,"\"%04x\",\"%04x\",\"%04x\",\"%04x\",\"%04x\"",
          xl3s[crateNum]->GetMBID(i),xl3s[crateNum]->GetDBID(i,0),
          xl3s[crateNum]->GetDBID(i,1),xl3s[crateNum]->GetDBID(i,2),
          xl3s[crateNum]->GetDBID(i,3));

      MB* mb_consts = (MB *) (packet.payload+4);
      packet.header.packetType = CRATE_INIT_ID;

      ///////////////////////////
      // GET DEFAULT DB VALUES //
      ///////////////////////////

      ParseFECDebug(debug_doc,mb_consts);

      //////////////////////////////
      // GET VALUES FROM DEBUG DB //
      //////////////////////////////

      // VBAL
      if ((useVBal || useAll) && ((0x1<<i) & slotMask)){
        if (xl3s[crateNum]->GetMBID(i) == 0x0000){
          printf("Warning: Slot %d: mb_id unknown. Using default values. Make sure to load xilinx before attempting to use debug db values.\n",i);
        }else{
          sprintf(get_db_address,"%s/%s/%s/get_crate_cbal?startkey=[%s,9999999999]&endkey=[%s,0]&descending=true",
              DB_SERVER,DB_BASE_NAME,DB_VIEWDOC,configString,configString);
          pouch_request *cbal_response = pr_init();
          pr_set_method(cbal_response, GET);
          pr_set_url(cbal_response, get_db_address);
          pr_do(cbal_response);
          if (cbal_response->httpresponse != 200){
            printf("Unable to connect to database. error code %d\n",(int)cbal_response->httpresponse);
            return -1;
          }
          JsonNode *viewdoc = json_decode(cbal_response->resp.data);
          JsonNode* viewrows = json_find_member(viewdoc,"rows");
          int n = json_get_num_mems(viewrows);
          if (n == 0){
            printf("Warning: Slot %d: No crate_cbal documents for this configuration (%s). Continuing with default values.\n",i,configString);
          }else{
            // these next three JSON nodes are pointers to the structure of viewrows; no need to delete
            JsonNode* cbal_doc = json_find_member(json_find_element(viewrows,0),"value");
            JsonNode* channels = json_find_member(cbal_doc,"channels");
            for (j=0;j<32;j++){
              JsonNode* one_channel = json_find_element(channels,j); 
              mb_consts->vBal[1][j] = (int)json_get_number(json_find_member(one_channel,"vbal_low"));
              mb_consts->vBal[0][j] = (int)json_get_number(json_find_member(one_channel,"vbal_high"));
            }
          }
          json_delete(viewdoc); // viewrows is part of viewdoc; only delete the head node
          pr_free(cbal_response);
        }
      }

      // VTHR
      if ((useVThr || useAll) && ((0x1<<i) & slotMask)){
        if (xl3s[crateNum]->GetMBID(i) == 0x0000){
          printf("Warning: Slot %d: mb_id unknown. Using default values. Make sure to load xilinx before attempting to use debug db values.\n",i);
        }else{
          sprintf(get_db_address,"%s/%s/%s/get_zdisc?startkey=[%s,9999999999]&endkey=[%s,0]&descending=true",DB_SERVER,DB_BASE_NAME,DB_VIEWDOC,configString,configString);
          pouch_request *zdisc_response = pr_init();
          pr_set_method(zdisc_response, GET);
          pr_set_url(zdisc_response, get_db_address);
          pr_do(zdisc_response);
          if (zdisc_response->httpresponse != 200){
            printf("Unable to connect to database. error code %d\n",(int)zdisc_response->httpresponse);
            return -1;
          }
          JsonNode *viewdoc = json_decode(zdisc_response->resp.data);
          JsonNode* viewrows = json_find_member(viewdoc,"rows");
          int n = json_get_num_mems(viewrows);
          if (n == 0){
            printf("Warning: Slot %d: No zdisc documents for this configuration (%s). Continuing with default values.\n",i,configString);
          }else{
            JsonNode* zdisc_doc = json_find_member(json_find_element(viewrows,0),"value");
            JsonNode* vthr = json_find_member(zdisc_doc,"zero_dac");
            for (j=0;j<32;j++){
              mb_consts->vThr[j] = (int)json_get_number(json_find_element(vthr,j));
            }
          }
          json_delete(viewdoc); // only delete the head
          pr_free(zdisc_response);
        }
      }

      //FIXME TDISC, other parts, stopped cause not necessarily most up to date penn_daq

      ///////////////////////////////
      // SEND THE DATABASE TO XL3s //
      ///////////////////////////////

      *(uint32_t *) packet.payload = i;

      SwapLongBlock(packet.payload,1);	
      SwapFECDB(mb_consts);

      xl3s[crateNum]->SendCommand(&packet,0);

    }

    // GET CTC DELAY FROM CTC_DOC IN DB
    pouch_request *ctc_response = pr_init();
    sprintf(ctc_address,"%s/%s/CTC_doc",DB_SERVER,DB_BASE_NAME);
    pr_set_method(ctc_response, GET);
    pr_set_url(ctc_response, ctc_address);
    pr_do(ctc_response);
    if (ctc_response->httpresponse != 200){
      printf("Error getting ctc document, error code %d\n",(int)ctc_response->httpresponse);
      return -1;
    }
    JsonNode *ctc_doc = json_decode(ctc_response->resp.data);
    JsonNode *ctc_delay_a = json_find_member(ctc_doc,"delay");
    uint32_t ctc_delay = strtoul(json_get_string(json_find_element(ctc_delay_a,crateNum)),(char**) NULL,16);
    json_delete(ctc_doc); // delete the head node
    pr_free(ctc_response);


    // START CRATE_INIT ON ML403
    printf("Beginning crate_init.\n");

    packet.header.packetType = CRATE_INIT_ID;
    packetArgs->mbNum = 666;
    packetArgs->xilinxLoad = xilinxLoad;
    packetArgs->hvReset = hvReset;
    packetArgs->slotMask = slotMask;
    packetArgs->ctcDelay = ctc_delay;
    packetArgs->shiftRegOnly = shiftRegOnly;

    SwapLongBlock(packet.payload,sizeof(CrateInitArgs)/sizeof(uint32_t));

    xl3s[crateNum]->SendCommand(&packet);

    CrateInitResults *results = (CrateInitResults *) packet.payload;

    // NOW PROCESS RESULTS AND POST TO DB
    for (i=0;i<16;i++){
      SwapShortBlock(&(results->hwareVals[i].mbID),1);
      SwapShortBlock(results->hwareVals[i].dbID,4);
    }

    printf("Crate configuration updated.\n");
    json_delete(hw_rows);
    json_delete(debug_doc);
  }
  catch(int e){
    printf("There was a network error!\n");
  }
  printf("****************************************\n");

  return 0;
}

int SMReset(int crateNum)
{
  XL3Packet packet;
  packet.header.packetType = STATE_MACHINE_RESET_ID;
  try{
    xl3s[crateNum]->SendCommand(&packet);
    printf("Reset state machine.\n");
  }
  catch(int e){
    printf("There was a network error!\n");
  }

  return 0;
}

int DebuggingMode(int crateNum, int on)
{
  XL3Packet packet;
  packet.header.packetType = DEBUGGING_MODE_ID;
  *(uint32_t *) packet.payload = on; 
  SwapLongBlock(packet.payload,1);
  try{
    xl3s[crateNum]->SendCommand(&packet);
    if (on)
      printf("Turned on debugging mode\n");
    else
      printf("Turned off debugging mode\n");
  }
  catch(int e){
    printf("There was a network error!\n");
  }

  return 0;
}

int ChangeMode(int crateNum, int mode, uint32_t dataAvailMask)
{
  XL3Packet packet;
  packet.header.packetType = CHANGE_MODE_ID;
  ChangeModeArgs *args = (ChangeModeArgs *) packet.payload;
  args->mode = mode;
  args->dataAvailMask = dataAvailMask;
  SwapLongBlock(packet.payload,sizeof(ChangeModeArgs)/sizeof(uint32_t));
  try{
    xl3s[crateNum]->SendCommand(&packet);
    if (mode)
      printf("Changed to init mode\n");
    else
      printf("Changed to normal mode\n");
  }
  catch(int e){
    printf("There was a network error!\n");
  }

  return 0;
}

int ReadLocalVoltage(int crateNum, int voltage)
{
  XL3Packet packet;
  packet.header.packetType = READ_LOCAL_VOLTAGE_ID;
  ReadLocalVoltageArgs *args = (ReadLocalVoltageArgs *) packet.payload;
  ReadLocalVoltageResults *results = (ReadLocalVoltageResults *) packet.payload;
  args->voltageSelect = voltage;
  SwapLongBlock(packet.payload,sizeof(ReadLocalVoltageArgs)/sizeof(uint32_t));
  try{
    xl3s[crateNum]->SendCommand(&packet);
    SwapLongBlock(packet.payload,sizeof(ReadLocalVoltageResults)/sizeof(uint32_t));
    printf("Voltage #%d: %f\n",voltage,results->voltage);
  }
  catch(int e){
    printf("There was a network error!\n");
  }

  return 0;
}

int HVReadback(int crateNum)
{
  XL3Packet packet;
  packet.header.packetType = HV_READBACK_ID;
  HVReadbackResults *results = (HVReadbackResults *) packet.payload;
  try{
    xl3s[crateNum]->SendCommand(&packet);
    SwapLongBlock(packet.payload,sizeof(HVReadbackResults)/sizeof(uint32_t));
    printf("Supply A - Voltage: %6.3f volts, Current: %6.4f mA\n",results->voltageA*300.0,results->currentA*10.0);
    printf("Supply B - Voltage: %6.3f volts, Current: %6.4f mA\n",results->voltageB*300.0,results->currentB*10.0);
  }
  catch(int e){
    printf("There was a network error!\n");
  }

  return 0;
}

int SetAlarmDac(int crateNum, uint32_t *dacs)
{
  XL3Packet packet;
  packet.header.packetType = SET_ALARM_DAC_ID;
  SetAlarmDacArgs *args = (SetAlarmDacArgs *) packet.payload;
  for (int i=0;i<3;i++)
    args->dacs[i] = dacs[i];
  SwapLongBlock(packet.payload,sizeof(SetAlarmDacArgs)/sizeof(uint32_t));

  try{
    xl3s[crateNum]->SendCommand(&packet);
    printf("Dacs set\n");
  }
  catch(int e){
    printf("There was a network error!\n");
  }

  return 0;
}

