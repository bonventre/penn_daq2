#include <cstdio>

#include "Globals.h"
#include "DB.h"
#include "Pouch.h"
#include "XL3Registers.h"
#include "UnpackBundles.h"

#include "NetUtils.h"
#include "XL3Link.h"
#include "XL3Model.h"
#include "XL3Cmds.h"

int XL3RW(int crateNum, uint32_t address, uint32_t data)
{
  uint32_t result;
  try{
    int errors = xl3s[crateNum]->RW(address,data,&result);
    if (errors)
      lprintf("There was a bus error.\n");
    else
      lprintf("Wrote to %08x, got %08x\n",address,result);
  }
  catch(const char* s){
    lprintf("XL3RW: %s\n",s);
  }

  return 0;
}

int XL3QueueRW(int crateNum, uint32_t address, uint32_t data)
{
  lprintf("*** Starting XL3 Queue RW ********************\n");

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
    lprintf("Command queued\n");
    uint32_t result;
    xl3s[crateNum]->GetMultiFCResults(1, xl3s[crateNum]->GetLastCommandNum(), &result);
    lprintf("got %08x\n",result);
  }
  catch(const char* s){
    lprintf("XL3QueueRW: %s\n",s);
  }

  lprintf("****************************************\n");
  return 0;
}

int CrateInit(int crateNum,uint32_t slotMask, int xilinxLoad, int hvReset, int shiftRegOnly,
    int useVBal, int useVThr, int useTDisc, int useTCmos, int useAll, int useHw)
{
  lprintf("*** Starting Crate Init ****************\n");
  char get_db_address[500];
  char ctc_address[500];
  XL3Packet packet;
  CrateInitArgs *packetArgs = (CrateInitArgs *) packet.payload;

  lprintf("Initializing crate %d, slots %08x, xl: %d, hv: %d\n",
      crateNum,slotMask,xilinxLoad,hvReset);
  lprintf("Now sending database to XL3\n");

  try{

    pouch_request *hw_response = pr_init();
    JsonNode* hw_rows[16];
    pouch_request *debug_response = pr_init();
    JsonNode* debug_doc = NULL;

    if (useHw == 1){
      for (int i=0;i<16;i++){
        if ((0x1<<i) & slotMask){
          sprintf(get_db_address,"%s/%s/%s/get_fec_by_generated?startkey=[%d,%d,{}]&endkey=[%d,%d]&descending=True",FECDB_SERVER,FECDB_BASE_NAME,FECDB_VIEWDOC,crateNum,i,crateNum,i);
          pr_set_method(hw_response, GET);
          pr_set_url(hw_response, get_db_address);
          pr_do(hw_response);
          if (hw_response->httpresponse != 200){
            lprintf("Unable to connect to database. error code %d\n",(int)hw_response->httpresponse);
            return -1;
          }
          JsonNode *hw_doc = json_decode(hw_response->resp.data);
          JsonNode* totalrows = json_find_member(hw_doc,"total_rows");
          if ((int)json_get_number(totalrows) == 0){
            lprintf("Database error: No FEC entry for crate %d, card %d\n",crateNum,i);
            return -1;
          }
          hw_rows[i] = json_find_member(hw_doc,"rows");
          pr_free(hw_response);
        }
      }
    }
    sprintf(get_db_address,"%s/%s/CRATE_INIT_DOC",DB_SERVER,DB_BASE_NAME);
    pr_set_method(debug_response, GET);
    pr_set_url(debug_response, get_db_address);
    pr_do(debug_response);
    if (debug_response->httpresponse != 200){
      lprintf("Unable to connect to database. error code %d\n",(int)debug_response->httpresponse);
      return -1;
    }
    debug_doc = json_decode(debug_response->resp.data);
    pr_free(debug_response);

    // make sure crate_config is up to date
    if (useVBal || useVThr || useTDisc || useTCmos || useAll)
      xl3s[crateNum]->UpdateCrateConfig(slotMask);

    if (useVBal || useAll)
      lprintf("Using VBAL values from database\n");
    if (useVThr || useAll)
      lprintf("Using VTHR values from database\n");
    if (useTDisc || useAll)
      lprintf("Using TDISC values from database\n");
    if (useTCmos || useAll)
      lprintf("Using TCMOS values from database\n");

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

      if (useHw && ((0x1<<i) & slotMask)){
        JsonNode *next_row = json_find_element(hw_rows[i],0);
        JsonNode *value = json_find_member(next_row,"value");
        ParseFECHw(value,mb_consts);
      }

      //////////////////////////////
      // GET VALUES FROM DEBUG DB //
      //////////////////////////////

      // VBAL
      if ((useVBal || useAll) && ((0x1<<i) & slotMask)){
        if (xl3s[crateNum]->GetMBID(i) == 0x0000){
          lprintf("Warning: Slot %d: mb_id unknown. Using default values. Make sure to load xilinx before attempting to use debug db values.\n",i);
        }else{
          sprintf(get_db_address,"%s/%s/%s/get_crate_cbal?startkey=[%s,9999999999]&endkey=[%s,0]&descending=true",
              DB_SERVER,DB_BASE_NAME,DB_VIEWDOC,configString,configString);
          pouch_request *cbal_response = pr_init();
          pr_set_method(cbal_response, GET);
          pr_set_url(cbal_response, get_db_address);
          pr_do(cbal_response);
          if (cbal_response->httpresponse != 200){
            lprintf("Unable to connect to database. error code %d\n",(int)cbal_response->httpresponse);
            return -1;
          }
          JsonNode *viewdoc = json_decode(cbal_response->resp.data);
          JsonNode* viewrows = json_find_member(viewdoc,"rows");
          int n = json_get_num_mems(viewrows);
          if (n == 0){
            lprintf("Warning: Slot %d: No crate_cbal documents for this configuration (%s). Continuing with default values.\n",i,configString);
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
          lprintf("Warning: Slot %d: mb_id unknown. Using default values. Make sure to load xilinx before attempting to use debug db values.\n",i);
        }else{
          sprintf(get_db_address,"%s/%s/%s/get_zdisc?startkey=[%s,9999999999]&endkey=[%s,0]&descending=true",DB_SERVER,DB_BASE_NAME,DB_VIEWDOC,configString,configString);
          pouch_request *zdisc_response = pr_init();
          pr_set_method(zdisc_response, GET);
          pr_set_url(zdisc_response, get_db_address);
          pr_do(zdisc_response);
          if (zdisc_response->httpresponse != 200){
            lprintf("Unable to connect to database. error code %d\n",(int)zdisc_response->httpresponse);
            return -1;
          }
          JsonNode *viewdoc = json_decode(zdisc_response->resp.data);
          JsonNode* viewrows = json_find_member(viewdoc,"rows");
          int n = json_get_num_mems(viewrows);
          if (n == 0){
            lprintf("Warning: Slot %d: No zdisc documents for this configuration (%s). Continuing with default values.\n",i,configString);
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

      // TDISC
      if ((useTDisc || useAll) && ((0x1<<i) & slotMask)){
        if (xl3s[crateNum]->GetMBID(i) == 0x0000){
          lprintf("Warning: Slot %d: mb_id unknown. Using default values. Make sure to load xilinx before attempting to use debug db values.\n",i);
        }else{
          sprintf(get_db_address,"%s/%s/%s/get_ttot?startkey=[%s,9999999999]&endkey=[%s,0]&descending=true",DB_SERVER,DB_BASE_NAME,DB_VIEWDOC,configString,configString);
          pouch_request *ttot_response = pr_init();
          pr_set_method(ttot_response,GET);
          pr_set_url(ttot_response,get_db_address);
          pr_do(ttot_response);
          if (ttot_response->httpresponse != 200){
            lprintf("Unable to connect to database. error code %d\n",(int)ttot_response->httpresponse);
            return -1;
          }
          JsonNode *viewdoc = json_decode(ttot_response->resp.data);
          JsonNode *viewrows = json_find_member(viewdoc,"rows");
          int n = json_get_num_mems(viewrows);
          if (n == 0){
            lprintf("Warning: Slot %d: No set_ttot documents for this configuration (%s). Continuing with default values.\n",i,configString);
          }else{
            JsonNode *ttot_doc = json_find_member(json_find_element(viewrows,0),"value");
            JsonNode *chips = json_find_member(ttot_doc,"chips");
            for (int j=0;j<8;j++){
              JsonNode *one_chip = json_find_element(chips,j);
              mb_consts->tDisc.rmp[j] = (int)json_get_number(json_find_member(one_chip,"rmp"));
              mb_consts->tDisc.vsi[j] = (int)json_get_number(json_find_member(one_chip,"vsi"));
            }
          }
          json_delete(viewdoc);
          pr_free(ttot_response);
        }
      }

      // TCMOS
      if ((useTCmos || useAll) && ((0x1<<i) & slotMask)){
        if (xl3s[crateNum]->GetMBID(i) == 0x0000){
          lprintf("Warning: Slot %d: mb_id unknown. Using default values. Make sure to load xilinx before attempting to use debug db values.\n",i);
        }else{
          sprintf(get_db_address,"%s/%s/%s/get_cmos?startkey=[%s,9999999999]&endkey=[%s,0]&descending=true",DB_SERVER,DB_BASE_NAME,DB_VIEWDOC,configString,configString);
          pouch_request *cmos_response = pr_init();
          pr_set_method(cmos_response,GET);
          pr_set_url(cmos_response,get_db_address);
          pr_do(cmos_response);
          if (cmos_response->httpresponse != 200){
            lprintf("Unable to connect to database. error code %d\n",(int)cmos_response->httpresponse);
            return -1;
          }
          JsonNode *viewdoc = json_decode(cmos_response->resp.data);
          JsonNode *viewrows = json_find_member(viewdoc,"rows");
          int n = json_get_num_mems(viewrows);
          if (n == 0){
            lprintf("Warning: Slot %d: No cmos_m_gtvalid documents for this configuration (%s). Continuing with default values.\n",i,configString);
          }else{
            JsonNode *cmos_doc = json_find_member(json_find_element(viewrows,0),"value");
            JsonNode *channels = json_find_member(cmos_doc,"channels");
            for (int j=0;j<32;j++){
              JsonNode *one_chan = json_find_element(channels,j);
              mb_consts->tCmos.tacShift[j] = (int)json_get_number(json_find_member(one_chan,"tac_shift"));
            }
            mb_consts->tCmos.vMax = (int)json_get_number(json_find_member(cmos_doc,"vmax"));
            mb_consts->tCmos.tacRef = (int)json_get_number(json_find_member(cmos_doc,"tacref"));
            JsonNode *isetm = json_find_member(cmos_doc,"isetm");
            JsonNode *iseta = json_find_member(cmos_doc,"iseta");
            for (int j=0;j<2;j++){
              mb_consts->tCmos.isetm[j] = (int)json_get_number(json_find_element(isetm,j));
              mb_consts->tCmos.iseta[j] = (int)json_get_number(json_find_element(iseta,j));
            }
          }
          json_delete(viewdoc);
          pr_free(cmos_response);
        }
      }


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
      lprintf("Error getting ctc document, error code %d\n",(int)ctc_response->httpresponse);
      return -1;
    }
    JsonNode *ctc_doc = json_decode(ctc_response->resp.data);
    JsonNode *ctc_delay_a = json_find_member(ctc_doc,"delay");
    uint32_t ctc_delay = strtoul(json_get_string(json_find_element(ctc_delay_a,crateNum)),(char**) NULL,16);
    json_delete(ctc_doc); // delete the head node
    pr_free(ctc_response);


    // START CRATE_INIT ON ML403
    lprintf("Beginning crate_init.\n");

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

    lprintf("Crate configuration updated.\n");
    for (i=0;i<16;i++)
	if ((0x1<<i) & slotMask && hw_rows[i] != NULL)
	    json_delete(hw_rows[i]);
    json_delete(debug_doc);
  }
  catch(const char* s){
    lprintf("CrateInit: %s\n",s);
  }
  lprintf("****************************************\n");

  return 0;
}

int SMReset(int crateNum)
{
  XL3Packet packet;
  packet.header.packetType = STATE_MACHINE_RESET_ID;
  try{
    xl3s[crateNum]->SendCommand(&packet);
    lprintf("Reset state machine.\n");
  }
  catch(const char* s){
    lprintf("SMReset: %s\n",s);
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
      lprintf("Turned on debugging mode\n");
    else
      lprintf("Turned off debugging mode\n");
  }
  catch(const char* s){
    lprintf("DebuggingMode: %s\n",s);
  }

  return 0;
}

int ChangeMode(int crateNum, int mode, uint32_t dataAvailMask)
{
  XL3Packet packet;
  packet.header.packetType = CHANGE_MODE_ID;
  ChangeModeArgs *args = (ChangeModeArgs *) packet.payload;
  if (mode)
    args->mode = NORMAL_MODE;
  else
    args->mode = INIT_MODE;
  args->dataAvailMask = dataAvailMask;
  SwapLongBlock(packet.payload,sizeof(ChangeModeArgs)/sizeof(uint32_t));
  try{
    xl3s[crateNum]->SendCommand(&packet);
    if (mode)
      lprintf("Changed to normal mode\n");
    else
      lprintf("Changed to init mode\n");
  }
  catch(const char* s){
    lprintf("ChangeMode: %s\n",s);
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
    lprintf("Voltage #%d: %f\n",voltage,results->voltage);
  }
  catch(const char* s){
    lprintf("ReadLocalVoltage: %s\n",s);
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
    lprintf("Supply A - Voltage: %6.3f volts, Current: %6.4f mA\n",results->voltageA*300.0,results->currentA*10.0);
    lprintf("Supply B - Voltage: %6.3f volts, Current: %6.4f mA\n",results->voltageB*300.0,results->currentB*10.0);
  }
  catch(const char* s){
    lprintf("HVReadback: %s\n",s);
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
    lprintf("Dacs set\n");
  }
  catch(const char* s){
    lprintf("SetAlarmDac: %s\n",s);
  }

  return 0;
}

int LoadRelays(int crateNum, uint32_t *patterns)
{
  try{
    uint32_t result;
    for (int i=0;i<16;i++){
      for (int j=0;j<4;j++){
        if ((0x1<<j) & patterns[i]){
          xl3s[crateNum]->RW(XL_RELAY_R + WRITE_REG, 0x2,&result);
          xl3s[crateNum]->RW(XL_RELAY_R + WRITE_REG, 0xA,&result);
          xl3s[crateNum]->RW(XL_RELAY_R + WRITE_REG, 0x2,&result);
        }else{
          xl3s[crateNum]->RW(XL_RELAY_R + WRITE_REG, 0x0,&result);
          xl3s[crateNum]->RW(XL_RELAY_R + WRITE_REG, 0x8,&result);
          xl3s[crateNum]->RW(XL_RELAY_R + WRITE_REG, 0x0,&result);
        }
      }
    }
    usleep(1000);
    xl3s[crateNum]->RW(XL_RELAY_R + WRITE_REG, 0x4,&result);
    lprintf("Relays loaded\n");
  }
  catch(const char* s){
    lprintf("LoadRelays: %s\n",s);
  }
  return 0;
}

int ReadBundle(int crateNum, int slotNum, int quiet)
{
  try{
    int errors = 0;
    uint32_t crate,slot,chan,gt8,gt16,cmos_es16,cgt_es16,cgt_es8,nc_cc;
    int cell;
    double qlx,qhs,qhl,tac;
    uint32_t pmtword[3];
    errors += xl3s[crateNum]->RW(READ_MEM+slotNum*FEC_SEL,0x0,pmtword);
    errors += xl3s[crateNum]->RW(READ_MEM+slotNum*FEC_SEL,0x0,pmtword+1);
    errors += xl3s[crateNum]->RW(READ_MEM+slotNum*FEC_SEL,0x0,pmtword+2);
    if (errors != 0){
      lprintf("There were %d errors reading out the bundles.\n",errors);
      return -1;
    }
    lprintf("%08x %08x %08x\n",pmtword[0],pmtword[1],pmtword[2]);
    if (!quiet){
      crate = (uint32_t) UNPK_CRATE_ID(pmtword);
      slot = (uint32_t)  UNPK_BOARD_ID(pmtword);
      chan = (uint32_t)  UNPK_CHANNEL_ID(pmtword);
      cell = (int) UNPK_CELL_ID(pmtword);
      gt8 = (uint32_t)   UNPK_FEC_GT8_ID(pmtword);
      gt16 = (uint32_t)  UNPK_FEC_GT16_ID(pmtword);
      cmos_es16 = (uint32_t) UNPK_CMOS_ES_16(pmtword);
      cgt_es16 = (uint32_t)  UNPK_CGT_ES_16(pmtword);
      cgt_es8 = (uint32_t)   UNPK_CGT_ES_24(pmtword);
      nc_cc = (uint32_t) UNPK_NC_CC(pmtword);
      qlx = (double) MY_UNPK_QLX(pmtword);
      qhs = (double) UNPK_QHS(pmtword);
      qhl = (double) UNPK_QHL(pmtword);
      tac = (double) UNPK_TAC(pmtword);
      lprintf("crate %d, slot %d, chan %d, cell %d, gt8 %08x, gt16 %08x, cmos_es16 %08x,"
          " cgt_es16 %08x, cgt_es8 %08x, nc_cc %08x, qlx %6.1f, qhs %6.1f, qhl %6.1f, tac %6.1f\n",
          (int)crate,(int)slot,(int)chan,cell,gt8,
          gt16,cmos_es16,cgt_es16,cgt_es8,nc_cc,qlx,qhs,qhl,tac);
    }
  }
  catch(const char* s){
    lprintf("ReadBundle: %s\n",s);
  }
  return 0;
}

