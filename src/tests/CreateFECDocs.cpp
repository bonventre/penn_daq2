#include "Globals.h"
#include "DB.h"
#include "Json.h"
#include "Pouch.h"

#include "ControllerLink.h"
#include "CreateFECDocs.h"

int CreateFECDocs(uint32_t crateMask, uint32_t *slotMasks, const char* ecalID){

  if(crateMask == 0x0 && *slotMasks == 0x0){
    // get the ecal document with the configuration
    char get_db_address[500];
    sprintf(get_db_address,"%s/%s/%s",DB_SERVER,DB_BASE_NAME,ecalID);
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

    for (int i=0;i<MAX_XL3_CON;i++){
      slotMasks[i] = 0x0;
    }

    // get the configuration
    JsonNode *crates = json_find_member(ecalconfig_doc,"crates");
    int num_crates = json_get_num_mems(crates);
    for (int i=0;i<num_crates;i++){
      JsonNode *one_crate = json_find_element(crates,i);
      int crate_num = (int) json_get_number(json_find_member(one_crate,"crate_id"));
      crateMask |= (0x1<<crate_num);
      JsonNode *slots = json_find_member(one_crate,"slots");
      int num_slots = json_get_num_mems(slots);
      for (int j=0;j<num_slots;j++){
        JsonNode *one_slot = json_find_element(slots,j);
        int slot_num = (int) json_get_number(json_find_member(one_slot,"slot_id"));
        slotMasks[crate_num] |= (0x1<<slot_num);
      }
    }

    for (int i=0;i<MAX_XL3_CON;i++)
      if ((0x1<<i) & crateMask)
        lprintf("Generate FEC doc for: crate %d: 0x%04x\n",i,slotMasks[i]);

    GenerateFECDocFromECAL(crateMask, slotMasks, ecalID);
  }
  else if((crateMask != 0x0 && *slotMasks == 0x0) ||
          (crateMask == 0x0 && *slotMasks != 0x0)){
    lprintf("Specify both a crate and slot mask if you wish to load FEC docs for a specific crate/slot, rather than all slots in the ECAL.\n");
  }
  else{
    GenerateFECDocFromECAL(crateMask, slotMasks, ecalID);
  }

  return 0;
}

