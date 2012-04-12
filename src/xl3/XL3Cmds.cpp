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

int FECTest(int crateNum, uint32_t slotMask)
{
  printf("*** Starting FEC Test ******************\n");
  printf("****************************************\n");
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
    /*
       if (useVBal || useVThr || useTDisc || useTCmos || useAll)
       update_crate_config(arg.crate_num,arg.slot_mask,&thread_fdset);
     */ //FIXME

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

      MB* mb_consts = (MB *) (packet.payload+4);
      packet.header.packetType = CRATE_INIT_ID;

      ///////////////////////////
      // GET DEFAULT DB VALUES //
      ///////////////////////////

      ParseFECDebug(debug_doc,mb_consts);

      //////////////////////////////
      // GET VALUES FROM DEBUG DB //
      //////////////////////////////

      //FIXME

      /*
      // VBAL
      if ((useVBal || useAll) && ((0x1<<i) & slotMask)){
      if (crate_config[arg.crate_num][i].mb_id == 0x0000){
      printf("Warning: Slot %d: mb_id unknown. Using default values. Make sure to load xilinx before attempting to use debug db values.\n",i);
      }else{
      char config_string[500];
      sprintf(config_string,"\"%04x\",\"%04x\",\"%04x\",\"%04x\",\"%04x\"",
      crate_config[arg.crate_num][i].mb_id,crate_config[arg.crate_num][i].db_id[0],
      crate_config[arg.crate_num][i].db_id[1],crate_config[arg.crate_num][i].db_id[2],
      crate_config[arg.crate_num][i].db_id[3]);
      sprintf(get_db_address,"%s/%s/%s/get_crate_cbal?startkey=[%s,9999999999]&endkey=[%s,0]&descending=true",
      DB_SERVER,DB_BASE_NAME,DB_VIEWDOC,config_string,config_string);
      pouch_request *cbal_response = pr_init();
      pr_set_method(cbal_response, GET);
      pr_set_url(cbal_response, get_db_address);
      pr_do(cbal_response);
      if (cbal_response->httpresponse != 200){
      printf("Unable to connect to database. error code %d\n",(int)cbal_response->httpresponse);
      unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
      return NULL;
      }
      JsonNode *viewdoc = json_decode(cbal_response->resp.data);
      JsonNode* viewrows = json_find_member(viewdoc,"rows");
      int n = json_get_num_mems(viewrows);
      if (n == 0){
      printf("Warning: Slot %d: No crate_cbal documents for this configuration (%s). Continuing with default values.\n",i,config_string);
      }else{
      // these next three JSON nodes are pointers to the structure of viewrows; no need to delete
      JsonNode* cbal_doc = json_find_member(json_find_element(viewrows,0),"value");
      JsonNode* channels = json_find_member(cbal_doc,"channels");
      for (j=0;j<32;j++){
      JsonNode* one_channel = json_find_element(channels,j); 
      mb_consts->vbal[1][j] = (int)json_get_number(json_find_member(one_channel,"vbal_low"));
      mb_consts->vbal[0][j] = (int)json_get_number(json_find_member(one_channel,"vbal_high"));
      }
      }
      json_delete(viewdoc); // viewrows is part of viewdoc; only delete the head node
      pr_free(cbal_response);
      }
      }

      // VTHR
      if ((arg.use_vthr || arg.use_all) && ((0x1<<i) & arg.slot_mask)){
      if (crate_config[arg.crate_num][i].mb_id == 0x0000){
      printf("Warning: Slot %d: mb_id unknown. Using default values. Make sure to load xilinx before attempting to use debug db values.\n",i);
      }else{
      char config_string[500];
      sprintf(config_string,"\"%04x\",\"%04x\",\"%04x\",\"%04x\",\"%04x\"",
      crate_config[arg.crate_num][i].mb_id,crate_config[arg.crate_num][i].db_id[0],
      crate_config[arg.crate_num][i].db_id[1],crate_config[arg.crate_num][i].db_id[2],
      crate_config[arg.crate_num][i].db_id[3]);
      sprintf(get_db_address,"%s/%s/%s/get_zdisc?startkey=[%s,9999999999]&endkey=[%s,0]&descending=true",DB_SERVER,DB_BASE_NAME,DB_VIEWDOC,config_string,config_string);
      pouch_request *zdisc_response = pr_init();
      pr_set_method(zdisc_response, GET);
      pr_set_url(zdisc_response, get_db_address);
      pr_do(zdisc_response);
      if (zdisc_response->httpresponse != 200){
      printf("Unable to connect to database. error code %d\n",(int)zdisc_response->httpresponse);
      unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
      return NULL;
      }
      JsonNode *viewdoc = json_decode(zdisc_response->resp.data);
      JsonNode* viewrows = json_find_member(viewdoc,"rows");
      int n = json_get_num_mems(viewrows);
      if (n == 0){
      printf("Warning: Slot %d: No zdisc documents for this configuration (%s). Continuing with default values.\n",i,config_string);
      }else{
      JsonNode* zdisc_doc = json_find_member(json_find_element(viewrows,0),"value");
      JsonNode* vthr = json_find_member(zdisc_doc,"zero_dac");
      for (j=0;j<32;j++){
      mb_consts->vthr[j] = (int)json_get_number(json_find_element(vthr,j));
    }
    }
    json_delete(viewdoc); // only delete the head
    pr_free(zdisc_response);
    }
    }

    // TDISC
    if ((arg.use_tdisc || arg.use_all) && ((0x1<<i) & arg.slot_mask)){
      if (crate_config[arg.crate_num][i].mb_id == 0x0000){
        printf("Warning: Slot %d: mb_id unknown. Using default values. Make sure to load xilinx before attempting to use debug db values.\n",i);
      }else{
        char config_string[500];
        sprintf(config_string,"\"%04x\",\"%04x\",\"%04x\",\"%04x\",\"%04x\"",
            crate_config[arg.crate_num][i].mb_id,crate_config[arg.crate_num][i].db_id[0],
            crate_config[arg.crate_num][i].db_id[1],crate_config[arg.crate_num][i].db_id[2],
            crate_config[arg.crate_num][i].db_id[3]);
        sprintf(get_db_address,"%s/%s/%s/get_ttot?startkey=[%s,9999999999]&endkey=[%s,0]&descending=true",
            DB_SERVER,DB_BASE_NAME,DB_VIEWDOC,config_string,config_string);
        pouch_request *ttot_response = pr_init();
        pr_set_method(ttot_response, GET);
        pr_set_url(ttot_response, get_db_address);
        pr_do(ttot_response);
        if (ttot_response->httpresponse != 200){
          printf("Unable to connect to database. error code %d\n",(int)ttot_response->httpresponse);
          unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
          return NULL;
        }
        JsonNode *viewdoc = json_decode(ttot_response->resp.data);
        JsonNode* viewrows = json_find_member(viewdoc,"rows");
        int n = json_get_num_mems(viewrows);
        if (n == 0){
          printf("Warning: Slot %d: No set_ttot documents for this configuration (%s). Continuing with default values.\n",i,config_string);
        }else{
          JsonNode* ttot_doc = json_find_member(json_find_element(viewrows,0),"value");
          JsonNode* chips = json_find_member(ttot_doc,"chips");
          for (j=0;j<8;j++){
            JsonNode* one_chip = json_find_element(chips,j);
            mb_consts->tdisc.rmp[j] = (int)json_get_number(json_find_member(one_chip,"rmp"));
            mb_consts->tdisc.vsi[j] = (int)json_get_number(json_find_member(one_chip,"vsi"));
          }
        }
        json_delete(viewdoc);
        pr_free(ttot_response);
      }
    }

    // TCMOS
    if ((arg.use_tcmos || arg.use_all) && ((0x1<<i) & arg.slot_mask)){
      if (crate_config[arg.crate_num][i].mb_id == 0x0000){
        printf("Warning: Slot %d: mb_id unknown. Using default values. Make sure to load xilinx before attempting to use debug db values.\n",i);
      }else{
        char config_string[500];
        sprintf(config_string,"\"%04x\",\"%04x\",\"%04x\",\"%04x\",\"%04x\"",
            crate_config[arg.crate_num][i].mb_id,crate_config[arg.crate_num][i].db_id[0],
            crate_config[arg.crate_num][i].db_id[1],crate_config[arg.crate_num][i].db_id[2],
            crate_config[arg.crate_num][i].db_id[3]);
        sprintf(get_db_address,"%s/%s/%s/get_cmos?startkey=[%s,9999999999]&endkey=[%s,0]&descending=true",
            DB_SERVER,DB_BASE_NAME,DB_VIEWDOC,config_string,config_string);
        pouch_request *cmos_response = pr_init();
        pr_set_method(cmos_response, GET);
        pr_set_url(cmos_response, get_db_address);
        pr_do(cmos_response);
        if (cmos_response->httpresponse != 200){
          printf("Unable to connect to database. error code %d\n",(int)cmos_response->httpresponse);
          unthread_and_unlock(0,(0x1<<arg.crate_num),arg.thread_num);
          return NULL;
        }
        JsonNode *viewdoc = json_decode(cmos_response->resp.data);
        JsonNode* viewrows = json_find_member(viewdoc,"rows");
        int n = json_get_num_mems(viewrows);
        if (n == 0){
          printf("Warning: Slot %d: No cmos_m_gtvalid documents for this configuration (%s). Continuing with default values.\n",i,config_string);
        }else{
          JsonNode* cmos_doc = json_find_member(json_find_element(viewrows,0),"value");
          JsonNode* channels = json_find_member(cmos_doc,"channels");
          for (j=0;j<32;j++){
            JsonNode* one_chan = json_find_element(channels,j);
            mb_consts->tcmos.tac_shift[j] = (int)json_get_number(json_find_member(one_chan,"tac_shift"));
          }
          mb_consts->tcmos.vmax = (int)json_get_number(json_find_member(cmos_doc,"vmax"));
          mb_consts->tcmos.tacref = (int)json_get_number(json_find_member(cmos_doc,"tacref"));
          JsonNode *isetm = json_find_member(cmos_doc,"isetm");
          JsonNode *iseta = json_find_member(cmos_doc,"iseta");
          for (j=0;j<2;j++){
            mb_consts->tcmos.isetm[j] = (int)json_get_number(json_find_element(isetm,j));
            mb_consts->tcmos.iseta[j] = (int)json_get_number(json_find_element(iseta,j));
          }
        }
        json_delete(viewdoc);
        pr_free(cmos_response);
      }
    }
    */


      ///////////////////////////////
      // SEND THE DATABASE TO XL3s //
      ///////////////////////////////

      *(uint32_t *) packet.payload = i;

    SwapLongBlock(&(packet.payload),1);	
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

    SwapLongBlock(&(packet.payload),sizeof(CrateInitArgs)/sizeof(uint32_t));

    xl3s[crateNum]->SendCommand(&packet);

    // NOW PROCESS RESULTS AND POST TO DB
    //FIXME
    /*
       for (i=0;i<16;i++){
       crate_config[arg.crate_num][i] = packet_results->hware_vals[i];
       SwapShortBlock(&(crate_config[arg.crate_num][i].mb_id),1);
       SwapShortBlock(&(crate_config[arg.crate_num][i].db_id),4);
       }
     */


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
