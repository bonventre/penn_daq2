#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "XL3PacketTypes.h"
#include "DBTypes.h"
#include "Globals.h"

#include "Pouch.h"
#include "Json.h"
#include "DB.h"

int GetNewID(char* newid)
{
  char get_db_address[500];
  sprintf(get_db_address,"%s/_uuids",DB_SERVER);
  pouch_request *pr = pr_init();
  pr_set_url(pr, get_db_address);
  pr_set_method(pr, GET);
  pr_do(pr);
  JsonNode *newerid = json_decode(pr->resp.data);
  int ret = sprintf(newid,"%s",json_get_string(json_find_element(json_find_member(newerid,"uuids"),0)));
  json_delete(newerid);
  pr_free(pr);
  if (ret)
    return 0;
  return 1;
}

int ParseMTC(JsonNode* value,MTC* mtc)
{
    int i;
    JsonNode* mtcd = json_find_member(value,"mtcd");
    JsonNode* mtca  = json_find_member(value,"mtca");
    JsonNode* nhit = json_find_member(mtca,"nhit");
    JsonNode* esum = json_find_member(mtca,"esum");
    JsonNode* spare = json_find_member(mtca,"spare");

    mtc->mtcd.lockoutWidth = (int)json_get_number(json_find_member(mtcd,"lockout_width"));
    mtc->mtcd.pedestalWidth = (int)json_get_number(json_find_member(mtcd,"pedestal_width"));
    mtc->mtcd.nhit100LoPrescale = (int)json_get_number(json_find_member(mtcd,"nhit100_lo_prescale"));
    mtc->mtcd.pulserPeriod = (int)json_get_number(json_find_member(mtcd,"pulser_period"));
    mtc->mtcd.low10MhzClock = (int)json_get_number(json_find_member(mtcd,"low10Mhz_clock"));
    mtc->mtcd.high10MhzClock = (int)json_get_number(json_find_member(mtcd,"high10Mhz_clock"));
    mtc->mtcd.fineSlope = (float) json_get_number(json_find_member(mtcd,"fine_slope"));
    mtc->mtcd.minDelayOffset = (float) json_get_number(json_find_member(mtcd,"min_delay_offset"));
    mtc->mtcd.coarseDelay = (int)json_get_number(json_find_member(mtcd,"coarse_delay"));
    mtc->mtcd.fineDelay = (int)json_get_number(json_find_member(mtcd,"fine_delay"));
    mtc->mtcd.gtMask = strtoul(json_get_string(json_find_member(mtcd,"gt_mask")),(char**) NULL, 16);
    mtc->mtcd.gtCrateMask = strtoul(json_get_string(json_find_member(mtcd,"gt_crate_mask")),(char**) NULL, 16);
    mtc->mtcd.pedCrateMask = strtoul(json_get_string(json_find_member(mtcd,"ped_crate_mask")),(char**) NULL, 16);
    mtc->mtcd.controlMask = strtoul(json_get_string(json_find_member(mtcd,"control_mask")),(char**) NULL, 16);

    for (i=0;i<6;i++){
        sprintf(mtc->mtca.triggers[i].id,"%s",json_get_string(json_find_element(json_find_member(nhit,"id"),i)));
        mtc->mtca.triggers[i].threshold = (int)json_get_number(json_find_element(json_find_member(nhit,"threshold"),i));
        mtc->mtca.triggers[i].mvPerAdc = (int)json_get_number(json_find_element(json_find_member(nhit,"mv_per_adc"),i));
        mtc->mtca.triggers[i].mvPerHit = (int)json_get_number(json_find_element(json_find_member(nhit,"mv_per_hit"),i));
        mtc->mtca.triggers[i].dcOffset = (int)json_get_number(json_find_element(json_find_member(nhit,"dc_offset"),i));
    }
    for (i=0;i<4;i++){
        sprintf(mtc->mtca.triggers[i+6].id,"%s",json_get_string(json_find_element(json_find_member(esum,"id"),i)));
        mtc->mtca.triggers[i+6].threshold = (int)json_get_number(json_find_element(json_find_member(esum,"threshold"),i));
        mtc->mtca.triggers[i+6].mvPerAdc = (int)json_get_number(json_find_element(json_find_member(esum,"mv_per_adc"),i));
        mtc->mtca.triggers[i+6].mvPerHit = (int)json_get_number(json_find_element(json_find_member(esum,"mv_per_hit"),i));
        mtc->mtca.triggers[i+6].dcOffset = (int)json_get_number(json_find_element(json_find_member(esum,"dc_offset"),i));
    }
    for (i=0;i<4;i++){
        sprintf(mtc->mtca.triggers[i+10].id,"%s",json_get_string(json_find_element(json_find_member(spare,"id"),i)));
        mtc->mtca.triggers[i+10].threshold = (int)json_get_number(json_find_element(json_find_member(spare,"threshold"),i));
        mtc->mtca.triggers[i+10].mvPerAdc = (int)json_get_number(json_find_element(json_find_member(spare,"mv_per_adc"),i));
        mtc->mtca.triggers[i+10].mvPerHit = (int)json_get_number(json_find_element(json_find_member(spare,"mv_per_hit"),i));
        mtc->mtca.triggers[i+10].dcOffset = (int)json_get_number(json_find_element(json_find_member(spare,"dc_offset"),i));
    }
    return 0;
}

int ParseFECDebug(JsonNode* value,MB* mb)
{
    int j,k;
    for (j=0;j<2;j++){
        for (k=0;k<32;k++){
            mb->vBal[j][k] = (int)json_get_number(json_find_element(json_find_element(json_find_member(value,"vbal"),j),k)); 
            //printsend("[%d,%d] - %d\n",j,k,mb->vbal[j][k]);
        }
    }
    for (k=0;k<32;k++){
        mb->vThr[k] = (int)json_get_number(json_find_element(json_find_member(value,"vthr"),k)); 
        //printsend("[%d] - %d\n",k,mb->vthr[k]);
    }
    for (j=0;j<8;j++){
        mb->tDisc.rmp[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(value,"tdisc"),"rmp"),j));	
        mb->tDisc.rmpup[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(value,"tdisc"),"rmpup"),j));	
        mb->tDisc.vsi[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(value,"tdisc"),"vsi"),j));	
        mb->tDisc.vli[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(value,"tdisc"),"vli"),j));	
        //printsend("[%d] - %d %d %d %d\n",j,mb->tdisc.rmp[j],mb_consts->tdisc.rmpup[j],mb_consts->tdisc.vsi[j],mb_consts->tdisc.vli[j]);
    }
    mb->tCmos.vMax = (int)json_get_number(json_find_member(json_find_member(value,"tcmos"),"vmax"));
    mb->tCmos.tacRef = (int)json_get_number(json_find_member(json_find_member(value,"tcmos"),"tacref"));
    //printsend("%d %d\n",mb->tcmos.vmax,mb_consts->tcmos.tacref);
    for (j=0;j<2;j++){
        mb->tCmos.isetm[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(value,"tcmos"),"isetm"),j));
        mb->tCmos.iseta[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(value,"tcmos"),"iseta"),j));
        //printsend("[%d] - %d %d\n",j,mb->tcmos.isetm[j],mb_consts->tcmos.iseta[j]);
    }
    for (j=0;j<32;j++){
        mb->tCmos.tacShift[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(value,"tcmos"),"tac_shift"),j));
        //printsend("[%d] - %d\n",j,mb->tcmos.tac_shift[j]);
    }
    mb->vInt = (int)json_get_number(json_find_member(value,"vint"));
    mb->hvRef = (int)json_get_number(json_find_member(value,"hvref"));
    //printsend("%d %d\n",mb->vres,mb_consts->hvref);
    for (j=0;j<32;j++){
        mb->tr100.mask[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(value,"tr100"),"mask"),j));
        mb->tr100.tDelay[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(value,"tr100"),"delay"),j));
        mb->tr20.mask[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(value,"tr20"),"mask"),j));
        mb->tr20.tDelay[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(value,"tr20"),"delay"),j));
        mb->tr20.tWidth[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(value,"tr20"),"width"),j));
        //printsend("[%d] - %d %d %d %d %d\n",j,mb->tr100.mask[j],mb_consts->tr100.tdelay[j],mb_consts->tr20.tdelay[j],mb_consts->tr20.twidth[j]);
    }
    for (j=0;j<32;j++){
        mb->sCmos[j] = (int)json_get_number(json_find_element(json_find_member(value,"scmos"),j));
        //printsend("[%d] - %d\n",j,mb->scmos[j]);
    }
    mb->disableMask = 0x0;
    for (j=0;j<32;j++){
        if ((int)json_get_number(json_find_element(json_find_member(value,"chan_disable"),j)) != 0)
            mb->disableMask |= (0x1<<j);
    }

    return 0;

}


int ParseFECHw(JsonNode* value,MB* mb)
{
    int j,k;
    JsonNode* hw = json_find_member(value,"hw");
    mb->mbID = strtoul(json_get_string(json_find_member(value,"board_id")),(char**)NULL,16);
    mb->dbID[0] = strtoul(json_get_string(json_find_member(json_find_member(hw,"id"),"db0")),(char**)NULL,16);
    mb->dbID[1] = strtoul(json_get_string(json_find_member(json_find_member(hw,"id"),"db1")),(char**)NULL,16);
    mb->dbID[2] = strtoul(json_get_string(json_find_member(json_find_member(hw,"id"),"db2")),(char**)NULL,16);
    mb->dbID[3] = strtoul(json_get_string(json_find_member(json_find_member(hw,"id"),"db3")),(char**)NULL,16);
    //printsend("%04x,%04x,%04x,%04x\n",mb->mb_id,mb_consts->db_id[0],mb_consts->db_id[1],mb_consts->db_id[2],mb_consts->db_id[3]);
    for (j=0;j<2;j++){
        for (k=0;k<32;k++){
            mb->vBal[j][k] = (int)json_get_number(json_find_element(json_find_element(json_find_member(hw,"vbal"),j),k)); 
            //printsend("[%d,%d] - %d\n",j,k,mb->vbal[j][k]);
        }
    }
    for (k=0;k<32;k++){
        mb->vThr[k] = (int)json_get_number(json_find_element(json_find_member(hw,"vthr"),k)); 
        //printsend("[%d] - %d\n",k,mb->vthr[k]);
    }
    for (j=0;j<8;j++){
        mb->tDisc.rmp[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(hw,"tdisc"),"rmp"),j));	
        mb->tDisc.rmpup[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(hw,"tdisc"),"rmpup"),j));	
        mb->tDisc.vsi[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(hw,"tdisc"),"vsi"),j));	
        mb->tDisc.vli[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(hw,"tdisc"),"vli"),j));	
        //printsend("[%d] - %d %d %d %d\n",j,mb->tdisc.rmp[j],mb_consts->tdisc.rmpup[j],mb_consts->tdisc.vsi[j],mb_consts->tdisc.vli[j]);
    }
    mb->tCmos.vMax = (int)json_get_number(json_find_member(json_find_member(hw,"tcmos"),"vmax"));
    mb->tCmos.tacRef = (int)json_get_number(json_find_member(json_find_member(hw,"tcmos"),"tacref"));
    //printsend("%d %d\n",mb->tcmos.vmax,mb_consts->tcmos.tacref);
    for (j=0;j<2;j++){
        mb->tCmos.isetm[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(hw,"tcmos"),"isetm"),j));
        mb->tCmos.iseta[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(hw,"tcmos"),"iseta"),j));
        //printsend("[%d] - %d %d\n",j,mb->tcmos.isetm[j],mb_consts->tcmos.iseta[j]);
    }
    for (j=0;j<32;j++){
        mb->tCmos.tacShift[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(hw,"tcmos"),"tac_shift"),j));
        //printsend("[%d] - %d\n",j,mb->tcmos.tac_shift[j]);
    }
    mb->vInt = (int)json_get_number(json_find_member(hw,"vint"));
    mb->hvRef = (int)json_get_number(json_find_member(hw,"hvref"));
    //printsend("%d %d\n",mb->vres,mb_consts->hvref);
    for (j=0;j<32;j++){
        mb->tr100.mask[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(hw,"tr100"),"mask"),j));
        mb->tr100.tDelay[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(hw,"tr100"),"delay"),j));
        mb->tr20.mask[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(hw,"tr20"),"mask"),j));
        mb->tr20.tDelay[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(hw,"tr20"),"delay"),j));
        mb->tr20.tWidth[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(hw,"tr20"),"width"),j));
        //printsend("[%d] - %d %d %d %d %d\n",j,mb->tr100.mask[j],mb_consts->tr100.tdelay[j],mb_consts->tr20.tdelay[j],mb_consts->tr20.twidth[j]);
    }
    for (j=0;j<32;j++){
        mb->sCmos[j] = (int)json_get_number(json_find_element(json_find_member(hw,"scmos"),j));
        //printsend("[%d] - %d\n",j,mb->scmos[j]);
    }
    mb->disableMask = 0x0;
    for (j=0;j<32;j++){
        if ((int)json_get_number(json_find_element(json_find_member(hw,"chan_disable"),j)) != 0)
            mb->disableMask |= (0x1<<j);
    }
    return 0;

}

int SwapFECDB(MB* mb)
{
    SwapShortBlock(&(mb->mbID),1);
    SwapShortBlock((mb->dbID),4);
    SwapShortBlock((mb->sCmos),32);
    SwapLongBlock(&(mb->disableMask),1);
    return 0;
}

int CreateFECDBDoc(int crate, int card, JsonNode** doc_p, JsonNode *ecal_doc)
{
  int i,j;
  // lets pull out what we need from the configuration document
  JsonNode *time_stamp = json_find_member(ecal_doc,"formatted_timestamp");
  JsonNode *ecal_id = json_find_member(ecal_doc,"_id");

  time_t curtime = time(NULL);
  struct tm *loctime = localtime(&curtime);
  char generated_ts[500];
  strftime(generated_ts,256,"%Y-%m-%dT%H:%M:%S",loctime);
  JsonNode *generated = json_mkstring(generated_ts);

  JsonNode *config;
  JsonNode *crates = json_find_member(ecal_doc,"crates");
  int found_it = 0;
  for (i=0;i<json_get_num_mems(crates);i++){
    JsonNode *one_crate = json_find_element(crates,i);
    if (json_get_number(json_find_member(one_crate,"crate_id")) == crate){
      JsonNode *slots = json_find_member(one_crate,"slots");
      for (j=0;j<json_get_num_mems(slots);j++){
        JsonNode *one_slot = json_find_element(slots,j);
        if (json_get_number(json_find_member(one_slot,"slot_id")) == card){
          config = one_slot;
          found_it = 1;
          break;
        }
      }
    }
  }
  if (!found_it){
    lprintf("Couldn't find this crate/slot in ecal doc? exiting %d %d\n",crate,card);
    return 1;
  }
  JsonNode *settings = json_find_member(ecal_doc,"settings");

  JsonNode *doc = json_mkobject();


  json_append_member(doc,"name",json_mkstring("FEC"));
  json_append_member(doc,"crate",json_mknumber(crate));
  json_append_member(doc,"card",json_mknumber(card));


  json_append_member(doc,"timestamp_ecal",json_mkstring(json_get_string(time_stamp)));
  json_append_member(doc,"timestamp_generated",generated);
  json_append_member(doc,"approved",json_mkbool(0));
  
  json_append_member(doc,"ecal_id",json_mkstring(json_get_string(ecal_id)));

  json_append_member(doc,"board_id",json_mkstring(json_get_string(json_find_member(config,"mb_id"))));
  
  JsonNode *hw = json_mkobject();
  json_append_member(hw,"vint",json_mknumber(json_get_number(json_find_member(settings,"vint"))));
  json_append_member(hw,"hvref",json_mknumber(json_get_number(json_find_member(settings,"hvref"))));
  json_append_member(hw,"tr100",json_mkcopy(json_find_member(settings,"tr100")));
  json_append_member(hw,"tr20",json_mkcopy(json_find_member(settings,"tr20")));
  json_append_member(hw,"scmos",json_mkcopy(json_find_member(settings,"scmos")));
  json_append_member(doc,"hw",hw);
  JsonNode *id = json_mkobject();
  json_append_member(id,"db0",json_mkstring(json_get_string(json_find_member(config,"db0_id"))));
  json_append_member(id,"db1",json_mkstring(json_get_string(json_find_member(config,"db1_id"))));
  json_append_member(id,"db2",json_mkstring(json_get_string(json_find_member(config,"db2_id"))));
  json_append_member(id,"db3",json_mkstring(json_get_string(json_find_member(config,"db3_id"))));
  json_append_member(id,"hv",json_mkstring("0x0000")); //FIXME
  json_append_member(doc,"id",id);

  JsonNode *test = json_mkobject();
  json_append_member(doc,"test",test);

  JsonNode *tube = json_mkobject();
  JsonNode *problem = json_mkarray();
  for (i=0;i<32;i++){
    json_append_element(problem,json_mknumber(0)); //FIXME
  }
  json_append_member(tube,"problem",problem);
  json_append_member(tube,"cable_link",json_mkstring("0")); //FIXME
  json_append_member(doc,"tube",tube);


  JsonNode *channel = json_mkobject();
  JsonNode *chan_problem = json_mkarray();
  for (i=0;i<32;i++)
    json_append_element(chan_problem,json_mknumber(0));
  json_append_member(channel,"problem",chan_problem);
  json_append_member(doc,"channel",channel);


  JsonNode *relays = json_mkarray();
  for (i=0;i<4;i++){
    json_append_element(relays,json_mknumber(1));
  }
  json_append_member(doc,"relay_on",relays);

  JsonNode *comment = json_mkarray();
  json_append_member(doc,"comment",comment);

  *doc_p = doc;

  return 0;
}


int AddECALTestResults(JsonNode *fec_doc, JsonNode *test_doc)
{
  int i,j;
  char type[50];
  JsonNode *hw = json_find_member(fec_doc,"hw");
  JsonNode *test = json_find_member(fec_doc,"test");



  JsonNode *channel_status = json_find_member(fec_doc,"channel");
  json_remove_from_parent(channel_status);
  JsonNode *chan_problems = json_find_member(channel_status,"problem");
  uint32_t chan_prob_array = 0x0;
  for (i=0;i<32;i++)
    if ((int) json_get_number(json_find_element(chan_problems,i)) != 0)
      chan_prob_array |= (0x1<<i);

  JsonNode *relays = json_find_member(fec_doc,"relay_on");
  json_remove_from_parent(relays);


  sprintf(type,"%s",json_get_string(json_find_member(test_doc,"type")));
  JsonNode *test_entry = json_mkobject();
  json_append_member(test_entry,"test_id",json_mkstring(json_get_string(json_find_member(test_doc,"_id"))));
  json_append_member(test,type,test_entry);

  if (strcmp(type,"crate_cbal") == 0){
    JsonNode *vbal = json_mkarray();
    JsonNode *high = json_mkarray();
    JsonNode *low = json_mkarray();
    JsonNode *channels = json_find_member(test_doc,"channels");
    for (i=0;i<32;i++){
      JsonNode *one_chan = json_find_element(channels,i);
      json_append_element(high,json_mknumber(json_get_number(json_find_member(one_chan,"vbal_high"))));
      json_append_element(low,json_mknumber(json_get_number(json_find_member(one_chan,"vbal_low"))));
    }
    json_append_element(vbal,high);
    json_append_element(vbal,low);
    json_append_member(hw,"vbal",vbal);

  }else if (strcmp(type,"zdisc") == 0){
    JsonNode *vthr_zero = json_mkarray();
    JsonNode *vals = json_find_member(test_doc,"zero_dac");
    for (i=0;i<32;i++){
      json_append_element(vthr_zero,json_mknumber(json_get_number(json_find_element(vals,i))));
    }
    json_append_member(hw,"vthr_zero",vthr_zero);

  }else if (strcmp(type,"set_ttot") == 0){
   JsonNode *tdisc = json_mkobject();
   JsonNode *rmp = json_mkarray();
   JsonNode *rmpup = json_mkarray();
   JsonNode *vsi = json_mkarray();
   JsonNode *vli = json_mkarray();
   JsonNode *chips = json_find_member(test_doc,"chips");
   for (i=0;i<8;i++){
     JsonNode *one_chip = json_find_element(chips,i);
     JsonNode *channels = json_find_member(one_chip,"channels");
     for (j=0;j<4;j++){
      JsonNode *one_chan = json_find_element(channels,j);
      JsonNode *one_chan_stat = json_find_element(chan_problems,j);     
      if ((int)json_get_number(json_find_member(one_chan,"errors")) == 2){
        chan_prob_array |= (0x1<<(i*4+j));
      }
     }
     json_append_element(rmp,json_mknumber(json_get_number(json_find_member(one_chip,"rmp"))));
     json_append_element(vsi,json_mknumber(json_get_number(json_find_member(one_chip,"vsi"))));
     json_append_element(rmpup,json_mknumber(115)); //FIXME`
     json_append_element(vli,json_mknumber(120)); //FIXME`
   }
   json_append_member(tdisc,"rmp",rmp);
   json_append_member(tdisc,"rmpup",rmpup);
   json_append_member(tdisc,"vsi",vsi);
   json_append_member(tdisc,"vli",vli);
   json_append_member(hw,"tdisc",tdisc);
   
  }else if (strcmp(type,"cmos_m_gtvalid") == 0){
    JsonNode *tcmos = json_mkobject();
    json_append_member(tcmos,"vmax",json_mknumber(json_get_number(json_find_member(test_doc,"vmax"))));
    json_append_member(tcmos,"vtacref",json_mknumber(json_get_number(json_find_member(test_doc,"tacref"))));
    JsonNode *isetm_vals = json_find_member(test_doc,"isetm");
    JsonNode *iseta_vals = json_find_member(test_doc,"iseta");
    JsonNode *isetm = json_mkarray();
    JsonNode *iseta = json_mkarray();
    for (i=0;i<2;i++){
      json_append_element(isetm,json_mknumber(json_get_number(json_find_element(isetm_vals,i))));
      json_append_element(iseta,json_mknumber(json_get_number(json_find_element(iseta_vals,i))));
    }

    json_append_member(tcmos,"isetm",isetm);
    json_append_member(tcmos,"iseta",iseta);
    JsonNode *channels = json_find_member(test_doc,"channels");
    JsonNode *tac_trim = json_mkarray();
    for (i=0;i<32;i++){
      JsonNode *one_chan = json_find_element(channels,i);
      if (json_get_bool(json_find_member(one_chan,"errors"))){
        chan_prob_array |= (0x1<<i);
      }
      json_append_element(tac_trim,json_mknumber(json_get_number(json_find_member(one_chan,"tac_shift"))));
    }
    json_append_member(tcmos,"tac_trim",tac_trim);
    json_append_member(hw,"tcmos",tcmos);

  }else if (strcmp(type,"find_noise") == 0){
    JsonNode *vthr = json_mkarray();
    JsonNode *channels = json_find_member(test_doc,"channels");
    for (i=0;i<32;i++){
      JsonNode *one_chan = json_find_element(channels,i);
      uint32_t zero_used = json_get_number(json_find_member(one_chan,"zero_used"));
      JsonNode *points = json_find_member(one_chan,"points");
      int total_rows = json_get_num_mems(points); 
      JsonNode *final_point = json_find_element(points,total_rows-1);
      uint32_t readout_dac = json_get_number(json_find_member(final_point,"thresh_above_zero"));
      json_append_element(vthr,json_mknumber(zero_used+readout_dac));
    }
    json_append_member(hw,"vthr",vthr);
  
  }else if (strcmp(type,"ped_run") == 0){
    JsonNode *errors = json_find_member(test_doc,"error_flags");
    for (i=0;i<32;i++){
      if (((int)json_get_number(json_find_element(errors,i)) == 3) || ((int)json_get_number(json_find_element(errors,i)) == 1)){
        chan_prob_array |= (0x1<<i);
      }
    }

  }else if (strcmp(type,"cgt_test") == 0){
    JsonNode *errors = json_find_member(test_doc,"errors");
    for (i=0;i<32;i++){
      if (json_get_bool(json_find_element(errors,i))){
        chan_prob_array |= (0x1<<i);
      }
    }

  }else if (strcmp(type,"get_ttot") == 0){
    JsonNode *channels = json_find_member(test_doc,"channels");
    for (i=0;i<32;i++){
      JsonNode *onechan = json_find_element(channels,i);
      int chan_num = (int) json_get_number(json_find_member(onechan,"id"));
      if ((int) json_get_number(json_find_member(onechan,"errors")) == 1){
        chan_prob_array |= (0x1<<chan_num);
      }
    }

  }else if (strcmp(type,"disc_check") == 0){
    JsonNode *channels = json_find_member(test_doc,"channels");
    for (i=0;i<32;i++){
      JsonNode *onechan = json_find_element(channels,i);
      int chan_num = (int) json_get_number(json_find_member(onechan,"id"));
      if ((int) json_get_number(json_find_member(onechan,"count_minus_peds")) > 10000 || (int) json_get_number(json_find_member(onechan,"count_minus_peds")) < -10000){
        chan_prob_array |= (0x1<<chan_num);
      }
    }

  }else if (strcmp(type,"cmos_m_gtvalid") == 0){
    JsonNode *channels = json_find_member(test_doc,"channels");
    for (i=0;i<32;i++){
      JsonNode *onechan = json_find_element(channels,i);
      int chan_num = (int) json_get_number(json_find_member(onechan,"id"));
      if (json_get_bool(json_find_member(onechan,"errors"))){
        chan_prob_array |= (0x1<<chan_num);
      }
    }



  }

  JsonNode *new_channel = json_mkobject();
  JsonNode *new_chan_problems = json_mkarray();
  for (i=0;i<32;i++){
    if ((0x1<<i) & chan_prob_array)
      json_append_element(new_chan_problems,json_mknumber(1));
    else
      json_append_element(new_chan_problems,json_mknumber(0));
  }
  json_append_member(new_channel,"problem",new_chan_problems);
  json_append_member(fec_doc,"channel",new_channel);

  JsonNode *new_relays = json_mkarray();
  for (i=0;i<4;i++){
    if (((0xFF<<(i*8)) & chan_prob_array) == (0xFF<<(i*8)) || (int)json_get_number(json_find_element(relays,i)) == 0)
      json_append_element(new_relays,json_mknumber(0));
    else
      json_append_element(new_relays,json_mknumber(1));
  }
  json_append_member(fec_doc,"relay_on",new_relays);

  return 0;
}

int PostFECDBDoc(int crate, int slot, JsonNode *doc)
{
  char new_id[500];
  GetNewID(new_id);

  char put_db_address[500];
  sprintf(put_db_address,"%s/%s/%s",FECDB_SERVER,FECDB_BASE_NAME,new_id);
  pouch_request *post_response = pr_init();
  pr_set_method(post_response, PUT);
  pr_set_url(post_response, put_db_address);
  char *data = json_encode(doc);
  pr_set_data(post_response, data);
  pr_do(post_response);
  int ret = 0;
  if (post_response->httpresponse != 201){
    lprintf("error code %d\n",(int)post_response->httpresponse);
    ret = -1;
  }
  pr_free(post_response);
  if(*data){
    free(data);
  }
  lprintf("Document posted to %s\n",put_db_address);
  return ret;
}

int UpdateFECDBDoc(JsonNode *doc)
{
  char put_db_address[500];
  sprintf(put_db_address,"%s/%s/%s",FECDB_SERVER,FECDB_BASE_NAME,json_get_string(json_find_member(doc,"_id")));
  pouch_request *post_response = pr_init();
  pr_set_method(post_response, PUT);
  pr_set_url(post_response, put_db_address);
  char *data = json_encode(doc);
  pr_set_data(post_response, data);
  pr_do(post_response);
  int ret = 0;
  if (post_response->httpresponse != 201){
    lprintf("error code %d\n",(int)post_response->httpresponse);
    ret = -1;
  }
  pr_free(post_response);
  if(*data){
    free(data);
  }
  return 0;
}

void SetupDebugDoc(int crateNum, int slotNum, JsonNode* doc)
{
  char mb_id[8],db_id[4][8];
  time_t the_time;
  the_time = time(0); //
  char datetime[100];
  sprintf(datetime,"%s",(char *) ctime(&the_time));
  datetime[strlen(datetime)-1] = '\0';

  sprintf(mb_id,"%04x",xl3s[crateNum]->GetMBID(slotNum));
  sprintf(db_id[0],"%04x",xl3s[crateNum]->GetDBID(slotNum,0));
  sprintf(db_id[1],"%04x",xl3s[crateNum]->GetDBID(slotNum,1));
  sprintf(db_id[2],"%04x",xl3s[crateNum]->GetDBID(slotNum,2));
  sprintf(db_id[3],"%04x",xl3s[crateNum]->GetDBID(slotNum,3));

  JsonNode *config = json_mkobject();
  JsonNode *db = json_mkarray();
  for (int i=0;i<4;i++){
    JsonNode *db1 = json_mkobject();
    json_append_member(db1,"db_id",json_mkstring(db_id[i]));
    json_append_member(db1,"slot",json_mknumber((double)i));
    json_append_element(db,db1);
  }
  json_append_member(config,"db",db);
  json_append_member(config,"fec_id",json_mkstring(mb_id));
  json_append_member(config,"crate_id",json_mknumber((double)crateNum));
  json_append_member(config,"slot",json_mknumber((double)slotNum));
  if (CURRENT_LOCATION == PENN_TESTSTAND)
    json_append_member(config,"loc",json_mkstring("penn"));
  if (CURRENT_LOCATION == ABOVE_GROUND_TESTSTAND)
    json_append_member(config,"loc",json_mkstring("surface"));
  if (CURRENT_LOCATION == UNDERGROUND)
    json_append_member(config,"loc",json_mkstring("underground"));
  json_append_member(doc,"config",config);

  json_append_member(doc,"timestamp",json_mknumber((double)(long int) the_time));
  json_append_member(doc,"created",json_mkstring(datetime));
}

int PostDebugDoc(int crateNum, int slotNum, JsonNode* doc, int updateConfig)
{
  if (updateConfig)
    xl3s[crateNum]->UpdateCrateConfig(0x1<<slotNum);

  SetupDebugDoc(crateNum,slotNum,doc);

  // TODO: this might be leaking a lot...
  char put_db_address[500];
  sprintf(put_db_address,"%s/%s",DB_SERVER,DB_BASE_NAME);
  pouch_request *post_response = pr_init();
  pr_set_method(post_response, POST);
  pr_set_url(post_response, put_db_address);
  char *data = json_encode(doc);
  pr_set_data(post_response, data);
  pr_do(post_response);
  int ret = 0;
  if (post_response->httpresponse != 201){
    lprintf("error code %d\n",(int)post_response->httpresponse);
    ret = -1;
  }
  pr_free(post_response);
  if(*data){
    free(data);
  }
  return ret;
}

int PostDebugDocWithID(int crate, int card, char *id, JsonNode* doc)
{
  xl3s[crate]->UpdateCrateConfig(0x1<<card);
  SetupDebugDoc(crate,card,doc);

  char put_db_address[500];
  sprintf(put_db_address,"%s/%s/%s",DB_SERVER,DB_BASE_NAME,id);
  pouch_request *post_response = pr_init();
  pr_set_method(post_response, PUT);
  pr_set_url(post_response, put_db_address);
  char *data = json_encode(doc);
  pr_set_data(post_response, data);
  pr_do(post_response);
  int ret = 0;
  if (post_response->httpresponse != 201){
    lprintf("error code %d\n",(int)post_response->httpresponse);
    ret = -1;
  }
  pr_free(post_response);
  if(*data){
    free(data);
  }
  return 0;
}

int PostECALDoc(uint32_t crateMask, uint32_t *slotMasks, char *logfile, char *id)
{
  JsonNode *doc = json_mkobject();

  // lets get what we need from the current crate init doc
  char get_db_address[500];
  sprintf(get_db_address,"%s/%s/CRATE_INIT_DOC",DB_SERVER,DB_BASE_NAME);
  pouch_request *init_response = pr_init();
  pr_set_method(init_response, GET);
  pr_set_url(init_response, get_db_address);
  pr_do(init_response);
  if (init_response->httpresponse != 200){
    lprintf("Unable to connect to database. error code %d\n",(int)init_response->httpresponse);
    return -1;
  }
  JsonNode *init_doc = json_decode(init_response->resp.data);
  JsonNode *settings = json_mkobject();
  json_append_member(settings,"vint",json_mknumber(json_get_number(json_find_member(init_doc,"vint"))));
  json_append_member(settings,"hvref",json_mknumber(json_get_number(json_find_member(init_doc,"hvref"))));
  json_append_member(settings,"tr100",json_mkcopy(json_find_member(init_doc,"tr100")));
  json_append_member(settings,"tr20",json_mkcopy(json_find_member(init_doc,"tr20")));
  json_append_member(settings,"scmos",json_mkcopy(json_find_member(init_doc,"scmos")));
  json_append_member(doc,"settings",settings);

  time_t the_time;
  the_time = time(0); //
  char datetime[100];
  sprintf(datetime,"%s",(char *) ctime(&the_time));
  datetime[strlen(datetime)-1] = '\0';

  char masks[8];

  JsonNode *crates = json_mkarray();
  for (int i=0;i<19;i++){
    if ((0x1<<i) & crateMask){
      JsonNode *one_crate = json_mkobject();
      json_append_member(one_crate,"crate_id",json_mknumber(i));
      sprintf(masks,"%04x",slotMasks[i]);
      json_append_member(one_crate,"slot_mask",json_mkstring(masks));

      JsonNode *slots = json_mkarray();
      for (int j=0;j<16;j++){
        if ((0x1<<j) & slotMasks[i]){
          char mb_id[8],db_id[4][8];
          char put_db_address[500];
          xl3s[i]->UpdateCrateConfig(0x1<<j);

          sprintf(mb_id,"%04x",xl3s[i]->GetMBID(j));
          sprintf(db_id[0],"%04x",xl3s[i]->GetDBID(j,0));
          sprintf(db_id[1],"%04x",xl3s[i]->GetDBID(j,1));
          sprintf(db_id[2],"%04x",xl3s[i]->GetDBID(j,2));
          sprintf(db_id[3],"%04x",xl3s[i]->GetDBID(j,3));

          JsonNode *one_slot = json_mkobject();
          json_append_member(one_slot,"slot_id",json_mknumber(j));
          json_append_member(one_slot,"mb_id",json_mkstring(mb_id));
          json_append_member(one_slot,"db0_id",json_mkstring(db_id[0]));
          json_append_member(one_slot,"db1_id",json_mkstring(db_id[1]));
          json_append_member(one_slot,"db2_id",json_mkstring(db_id[2]));
          json_append_member(one_slot,"db3_id",json_mkstring(db_id[3]));
          json_append_element(slots,one_slot);
        }
      }
      json_append_member(one_crate,"slots",slots);

      json_append_element(crates,one_crate);
    }
  }

  json_append_member(doc,"crates",crates);
  json_append_member(doc,"logfile_name",json_mkstring(logfile));
  json_append_member(doc,"type",json_mkstring("ecal"));
  json_append_member(doc,"timestamp",json_mknumber((double)(long int) the_time));
  json_append_member(doc,"created",json_mkstring(datetime));


  time_t curtime = time(NULL);
  struct tm *loctime = localtime(&curtime);
  char time_stamp[500];
  strftime(time_stamp,256,"%Y-%m-%dT%H:%M:%S",loctime);
  json_append_member(doc,"formatted_timestamp",json_mkstring(time_stamp));

  char put_db_address[500];
  sprintf(put_db_address,"%s/%s/%s",DB_SERVER,DB_BASE_NAME,id);
  pouch_request *post_response = pr_init();
  pr_set_method(post_response, PUT);
  pr_set_url(post_response, put_db_address);
  char *data = json_encode(doc);
  pr_set_data(post_response, data);
  pr_do(post_response);
  int ret = 0;
  if (post_response->httpresponse != 201){
    lprintf("error code %d\n",(int)post_response->httpresponse);
    ret = -1;
  }
  pr_free(post_response);
  json_delete(doc);
  if(*data){
    free(data);
  }
  return 0;
}

int GenerateFECDocFromECAL(uint32_t testMask, const char* id)
{
  lprintf("*** Generating FEC documents from ECAL ***\n");

  lprintf("Using ECAL %s\n",id); 

  // get the ecal document with the configuration
  char get_db_address[500];
  sprintf(get_db_address,"%s/%s/%s",DB_SERVER,DB_BASE_NAME,id);
  pouch_request *ecaldoc_response = pr_init();
  pr_set_method(ecaldoc_response, GET);
  pr_set_url(ecaldoc_response, get_db_address);
  pr_do(ecaldoc_response);
  if (ecaldoc_response->httpresponse != 200){
    lprintf("Unable to connect to database. error code %d\n",(int)ecaldoc_response->httpresponse);
    return -1;
  }
  JsonNode *ecalconfig_doc = json_decode(ecaldoc_response->resp.data);

  uint32_t testedMask = 0x0;
  uint32_t crateMask = 0x0;
  uint16_t slotMasks[20];
  for (int i=0;i<20;i++){
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


  // get all the ecal test results for all crates/slots
  sprintf(get_db_address,"%s/%s/%s/get_ecal?startkey=\"%s\"&endkey=\"%s\"",DB_SERVER,DB_BASE_NAME,DB_VIEWDOC,id,id);
  pouch_request *ecal_response = pr_init();
  pr_set_method(ecal_response, GET);
  pr_set_url(ecal_response, get_db_address);
  pr_do(ecal_response);
  if (ecal_response->httpresponse != 200){
    lprintf("Unable to connect to database. error code %d\n",(int)ecal_response->httpresponse);
    return -1;
  }

  JsonNode *ecalfull_doc = json_decode(ecal_response->resp.data);
  JsonNode *ecal_rows = json_find_member(ecalfull_doc,"rows");
  int total_rows = json_get_num_mems(ecal_rows); 
  if (total_rows == 0){
    lprintf("No documents for this ECAL yet! (id %s)\n",id);
    return -1;
  }

  // loop over crates/slots, create a fec document for each
  for (int i=0;i<19;i++){
    if ((0x1<<i) & crateMask){
      for (int j=0;j<16;j++){
        if ((0x1<<j) & slotMasks[i]){
          lprintf("crate %d slot %d\n",i,j);
          testedMask = 0x0;

          // lets generate the fec document
          JsonNode *doc;
          CreateFECDBDoc(i,j,&doc,ecalconfig_doc);

          for (int k=0;k<total_rows;k++){
            JsonNode *ecalone_row = json_find_element(ecal_rows,k);
            JsonNode *test_doc = json_find_member(ecalone_row,"value");
            JsonNode *config = json_find_member(test_doc,"config");
            char *testtype = json_get_string(json_find_member(test_doc,"type"));
            if ((json_get_number(json_find_member(config,"crate_id")) == i) && (json_get_number(json_find_member(config,"slot")) == j)){
              lprintf("test type is %s\n",json_get_string(json_find_member(test_doc,"type")));
              AddECALTestResults(doc,test_doc);
            }
          }

          PostFECDBDoc(i,j,doc);

          json_delete(doc); // only delete the head node
        }
      }
    }
  }

  json_delete(ecalfull_doc);
  pr_free(ecal_response);
  json_delete(ecalconfig_doc);
  pr_free(ecaldoc_response);

  lprintf("Finished creating fec document!\n");
  lprintf("**************************\n");

  return 0;
}

