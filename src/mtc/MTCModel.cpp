#include "Globals.h"
#include "MTCRegisters.h"
#include "Pouch.h"
#include "Json.h"

#include "MTCLink.h"
#include "MTCModel.h"

MTCModel::MTCModel()
{
  fLink = new MTCLink();
}

MTCModel::~MTCModel()
{
  delete fLink;
}

int MTCModel::RegWrite(uint32_t address, uint32_t data)
{
  SBCPacket *packet;
  packet = (SBCPacket *) malloc(sizeof(SBCPacket));
  packet->header.destination = 0x1;
  packet->header.cmdID = MTC_WRITE_ID;
  packet->header.numberBytesinPayload  = sizeof(SBCVmeWriteBlockStruct)+sizeof(uint32_t);
  //packet->cmdHeader.numberBytesinPayload  = 256+28;
  //packet->numBytes = 256+28+16;
  SBCVmeWriteBlockStruct *writestruct;
  writestruct = (SBCVmeWriteBlockStruct *) packet->payload;
  writestruct->address = address + MTCRegAddressBase;
  //writestruct->address = address;
  writestruct->addressModifier = MTCRegAddressMod;
  writestruct->addressSpace = MTCRegAddressSpace;
  writestruct->unitSize = 4;
  writestruct->numItems = 1;
  writestruct++;
  uint32_t *data_ptr = (uint32_t *) writestruct;
  *data_ptr = data;

  try{
    SendCommand(packet);
  }catch(const char* s){
    free(packet);
    throw s;
  }
  free(packet);
  return 0;
}

int MTCModel::RegRead(uint32_t address, uint32_t *data)
{
  SBCPacket *packet;
  packet = (SBCPacket *) malloc(sizeof(SBCPacket));
  uint32_t *result;
  packet->header.destination = 0x1;
  packet->header.cmdID = MTC_READ_ID;
  packet->header.numberBytesinPayload = sizeof(SBCVmeReadBlockStruct)+sizeof(uint32_t);
  //packet->numBytes = 256+27+16;
  SBCVmeReadBlockStruct *readstruct;
  readstruct = (SBCVmeReadBlockStruct *) packet->payload;
  readstruct->address = address + MTCRegAddressBase;
  readstruct->addressModifier = MTCRegAddressMod;
  readstruct->addressSpace = MTCRegAddressSpace;
  readstruct->unitSize = 4;
  readstruct->numItems = 1;

  try{
    SendCommand(packet);
  }catch(const char* s){
    free(packet);
    throw s;
  }
  result = (uint32_t *) (readstruct+1);
  *data = *result;
  free(packet);
  return 0;
}

int MTCModel::CheckLock()
{
  if (fLink->IsLocked())
    return 2;
  if (!fLink->IsConnected())
    return 1;
  return 0;
}

int MTCModel::SendCommand(SBCPacket *packet,int withResponse, int timeout)
{
  fLink->SendPacket(packet);
  if (withResponse){
    // look for the response. If you get the wrong packet type, try again, but
    // eventually raise an exception
    int err = fLink->GetNextPacket(packet,timeout);
    if (err)
      throw "Time out waiting for GetNextPacket";
  }
  return 0;
}

int MTCModel::GetGTCount(uint32_t *count)
{
  RegRead(MTCOcGtReg, count);
  *count &= 0x00FFFFFFF;
  return 0;
}

float MTCModel::SetGTDelay(float gtdel)
{
  int result;
  float offset_res, fine_delay, total_delay, fdelay_set;
  uint16_t cdticks, coarse_delay;

  offset_res = gtdel - (float)(18.35); //FIXME there is delay_offset in db?? check old code
  cdticks = (uint16_t) (offset_res/10.0);
  coarse_delay = cdticks*10;
  fine_delay = offset_res - ((float) cdticks*10.0);
  result = SetCoarseDelay(coarse_delay);
  fdelay_set = SetFineDelay(fine_delay);
  total_delay = ((float) coarse_delay + fdelay_set + (float)(18.35));
  if (((total_delay - gtdel) > 2) || ((total_delay - gtdel) < -2))
    lprintf("wanted %f, cdticks is %u, finedelay is %f, set to coarse_delay %u + fdelay_set %f + 18.35 = %f\n",gtdel,cdticks,fine_delay,coarse_delay,fdelay_set,total_delay);
  return total_delay;
}

int MTCModel::SoftGT()
{
  RegWrite(MTCSoftGTReg,0x0);
  return 0;
}

int MTCModel::MultiSoftGT(int number)
{
  SBCPacket *packet;
  packet = (SBCPacket *) malloc(sizeof(SBCPacket));
  packet->header.destination = 0x3;
  packet->header.cmdID = 0x5;
  packet->header.numberBytesinPayload  = sizeof(SBCVmeWriteBlockStruct) + sizeof(uint32_t);
  //packet->numBytes = 256+28+16;
  SBCVmeWriteBlockStruct *writestruct;
  writestruct = (SBCVmeWriteBlockStruct *) packet->payload;
  writestruct->address = MTCSoftGTReg + MTCRegAddressBase;
  writestruct->addressModifier = MTCRegAddressMod;
  writestruct->addressSpace = MTCRegAddressSpace;
  writestruct->unitSize = 4;
  writestruct->numItems = number;
  writestruct++;
  uint32_t *data_ptr = (uint32_t *) writestruct;
  *data_ptr = 0x0;
  try{
    SendCommand(packet);
  }catch(const char* s){
    free(packet);
    throw s;
  }
  free(packet);
  return 0;
}

int MTCModel::SetupPedestals(float pulser_freq, uint32_t ped_width, uint32_t coarse_delay,
    uint32_t fine_delay, uint32_t ped_crate_mask, uint32_t gt_crate_mask)
{
  int result = 0;
  float fdelay_set;
  result += SetLockoutWidth(DEFAULT_LOCKOUT_WIDTH);
  if (result == 0)
    result += SetPulserFrequency(pulser_freq);
  if (result == 0)
    result += SetPedestalWidth(ped_width);
  if (result == 0)
    result += SetCoarseDelay(coarse_delay);
  if (result == 0)
    fdelay_set = SetFineDelay(fine_delay);
  if (result != 0){
    lprintf("setup pedestals failed\n");
    return -1;
  }
  EnablePulser();
  EnablePedestal();
  UnsetPedCrateMask(MASKALL);
  UnsetGTCrateMask(MASKALL);
  SetPedCrateMask(ped_crate_mask);
  SetGTCrateMask(gt_crate_mask);
  SetGTMask(DEFAULT_GT_MASK);
  //lprintf("new_daq: setup_pedestals complete\n");
  return 0;
}

void MTCModel::EnablePulser()
{
  uint32_t temp;
  RegRead(MTCControlReg,&temp);
  RegWrite(MTCControlReg,temp | PULSE_EN);
  //lprintf("Pulser enabled\n");
}

void MTCModel::DisablePulser()
{
  uint32_t temp;
  RegRead(MTCControlReg,&temp);
  RegWrite(MTCControlReg,temp & ~PULSE_EN);
  //lprintf("Pulser disabled\n");
}

void MTCModel::EnablePedestal()
{
  uint32_t temp;
  RegRead(MTCControlReg,&temp);
  RegWrite(MTCControlReg,temp | PED_EN);
  //lprintf("Pedestals enabled\n");
}

void MTCModel::DisablePedestal()
{
  uint32_t temp;
  RegRead(MTCControlReg,&temp);
  RegWrite(MTCControlReg,temp & ~PED_EN);
  //lprintf("Pedestals disabled\n");
}

int MTCModel::LoadMTCADacsByCounts(uint16_t *raw_dacs)
{
  char dac_names[][14]={"N100LO","N100MED","N100HI","NHIT20","NH20LB","ESUMHI",
    "ESUMLO","OWLEHI","OWLELO","OWLN","SPARE1","SPARE2",
    "SPARE3","SPARE4"};
  int i, j, bi, di;
  float mV_dacs;
  uint32_t shift_value;

  for (i=0;i<14;i++){
    float mV_dacs = (((float)raw_dacs[i]/2048) * 5000.0) - 5000.0;
    lprintf( "\t%s\t threshold set to %6.2f mVolts (%d counts)\n", dac_names[i],
        mV_dacs,raw_dacs[i]);
  }

  /* set DACSEL */
  RegWrite(MTCDacCntReg,DACSEL);

  /* shift in raw DAC values */

  for (i = 0; i < 4 ; i++) {
    RegWrite(MTCDacCntReg,DACSEL | DACCLK); /* shift in 0 to first 4 dummy bits */
    RegWrite(MTCDacCntReg,DACSEL);
  }

  shift_value = 0UL;
  for (bi = 11; bi >= 0; bi--) {                     /* shift in 12 bit word for each DAC */
    for (di = 0; di < 14 ; di++){
      if (raw_dacs[di] & (1UL << bi))
        shift_value |= (1UL << di);
      else
        shift_value &= ~(1UL << di);
    }
    RegWrite(MTCDacCntReg,shift_value | DACSEL);
    RegWrite(MTCDacCntReg,shift_value | DACSEL | DACCLK);
    RegWrite(MTCDacCntReg,shift_value | DACSEL);
  }
  /* unset DASEL */
  RegWrite(MTCDacCntReg,0x0);
  return 0;
}

int MTCModel::LoadMTCADacs(float *voltages)
{
  uint16_t raw_dacs[14];
  int i;
  lprintf("Loading MTC/A threshold DACs...\n");


  /* convert each threshold from mVolts to raw value and load into
     raw_dacs array */
  for (i = 0; i < 14; i++) {
    //raw_dacs[i] = ((2048 * rdbuf)/5000) + 2048;
    raw_dacs[i] = MTCA_DAC_SLOPE * voltages[i] + MTCA_DAC_OFFSET;
  }

  LoadMTCADacsByCounts(raw_dacs);

  lprintf("DAC loading complete\n");
  return 0;
}



void MTCModel::UnsetGTMask(uint32_t raw_trig_types)
{
  uint32_t temp;
  RegRead(MTCMaskReg, &temp);
  RegWrite(MTCMaskReg, temp & ~raw_trig_types);
  //lprintf("Triggers have been removed from the GT Mask\n");
}

void MTCModel::SetGTMask(uint32_t raw_trig_types)
{
  uint32_t temp;
  RegRead(MTCMaskReg, &temp);
  RegWrite(MTCMaskReg, temp | raw_trig_types);
  //lprintf("Triggers have been added to the GT Mask\n");
}

void MTCModel::UnsetPedCrateMask(uint32_t crates)
{
  uint32_t temp;
  RegRead(MTCPmskReg, &temp);
  RegWrite(MTCPmskReg, temp & ~crates);
  //lprintf("Crates have been removed from the Pedestal Crate Mask\n");
}

void MTCModel::SetPedCrateMask(uint32_t crates)
{
  if (CURRENT_LOCATION == PENN_TESTSTAND)
    crates = MASKALL;
  uint32_t temp;
  RegRead(MTCPmskReg, &temp);
  RegWrite(MTCPmskReg, temp | crates);
  //lprintf("Crates have been added to the Pedestal Crate Mask\n");
}

void MTCModel::UnsetGTCrateMask(uint32_t crates)
{
  uint32_t temp;
  RegRead(MTCGmskReg, &temp);
  RegWrite(MTCGmskReg, temp & ~crates);
  //lprintf("Crates have been removed from the GT Crate Mask\n");
}

void MTCModel::SetGTCrateMask(uint32_t crates)
{
  if (CURRENT_LOCATION == PENN_TESTSTAND)
    crates = MASKALL;
  uint32_t temp;
  RegRead(MTCGmskReg, &temp);
  RegWrite(MTCGmskReg, temp | crates);
  //lprintf("Crates have been added to the GT Crate Mask\n");
}

int MTCModel::SetLockoutWidth(uint16_t width)
{
  uint32_t gtlock_value;
  if ((width < 20) || (width > 5100)) {
    lprintf("Lockout width out of range\n");
    return -1;
  }
  gtlock_value = ~(width / 20);
  uint32_t temp;
  RegWrite(MTCGtLockReg,gtlock_value);
  RegRead(MTCControlReg,&temp);
  RegWrite(MTCControlReg,temp | LOAD_ENLK); /* write GTLOCK value */
  RegRead(MTCControlReg,&temp);
  RegWrite(MTCControlReg,temp & ~LOAD_ENLK); /* toggle load enable */
  //lprintf( "Lockout width is set to %u ns\n", width);
  return 0;
}

int MTCModel::SetGTCounter(uint32_t count)
{
  uint32_t shift_value;
  short j;
  uint32_t temp;

  for (j = 23; j >= 0; j--){
    shift_value = ((count >> j) & 0x01) == 1 ? SERDAT | SEN : SEN ;
    RegWrite(MTCSerialReg,shift_value);
    RegRead(MTCSerialReg,&temp);
    RegWrite(MTCSerialReg,temp | SHFTCLKGT); /* clock in SERDAT */
  }
  RegRead(MTCControlReg,&temp);
  RegWrite(MTCControlReg,temp | LOAD_ENGT); /* toggle load enable */
  RegRead(MTCControlReg,&temp);
  RegWrite(MTCControlReg,temp & ~LOAD_ENGT); /* toggle load enable */

  lprintf("The GT counter has been loaded. It is now set to %d\n",(int) count);
  return 0;
}

int MTCModel::SetPrescale(uint16_t scale)
{
  uint32_t temp;
  if (scale < 2) {
    lprintf("Prescale value out of range\n");
    return -1;
  }
  RegWrite(MTCScaleReg,~(scale-1));
  RegRead(MTCControlReg,&temp);
  RegWrite(MTCControlReg,temp | LOAD_ENPR);
  RegRead(MTCControlReg,&temp);
  RegWrite(MTCControlReg,temp & ~LOAD_ENPR); /* toggle load enable */
  lprintf( "Prescaler set to %d NHIT_100_LO per PRESCALE\n", scale);
  return 0;
}     

int MTCModel::SetPulserFrequency(float freq)
{
  uint32_t pulser_value, shift_value, prog_freq;
  int16_t j;
  uint32_t temp;

  if (freq <= 1.0e-3) {                                /* SOFT_GTs as pulser */
    pulser_value = 0;
    lprintf("SOFT_GT is set to source the pulser\n");
  }
  else {
    pulser_value = (uint32_t)((781250 / freq) - 1);   /* 50MHz counter as pulser */
    prog_freq = (uint32_t)(781250/(pulser_value + 1));
    if ((pulser_value < 1) || (pulser_value > 167772216)) {
      lprintf( "Pulser frequency out of range\n", prog_freq);
      return -1;
    }
  }

  for (j = 23; j >= 0; j--){
    shift_value = ((pulser_value >> j) & 0x01) == 1 ? SERDAT|SEN : SEN; 
    RegWrite(MTCSerialReg,shift_value);
    RegRead(MTCSerialReg,&temp);
    RegWrite(MTCSerialReg,temp | SHFTCLKPS); /* clock in SERDAT */
  }
  RegRead(MTCControlReg,&temp);
  RegWrite(MTCControlReg,temp | LOAD_ENPS);
  RegRead(MTCControlReg,&temp);
  RegWrite(MTCControlReg,temp & ~LOAD_ENPS); /* toggle load enable */
  //lprintf( "Pulser frequency is set to %u Hz\n", prog_freq);
  return 0;
}

int MTCModel::SetPedestalWidth(uint16_t width)
{
  uint32_t temp, pwid_value;
  if ((width < 5) || (width > 1275)) {
    lprintf("Pedestal width out of range\n");
    return -1;
  }
  pwid_value = ~(width / 5);

  RegWrite(MTCPedWidthReg,pwid_value);
  RegRead(MTCControlReg,&temp);
  RegWrite(MTCControlReg, temp | LOAD_ENPW);
  RegRead(MTCControlReg,&temp);
  RegWrite(MTCControlReg, temp & ~LOAD_ENPW);
  //lprintf( "Pedestal width is set to %u ns\n", width);
  return 0;
}

int MTCModel::SetCoarseDelay(uint16_t delay)
{
  uint32_t temp, rtdel_value;

  if ((delay < 10) || (delay > 2550)) {
    lprintf("Coarse delay value out of range\n");
    return -1;
  } 
  rtdel_value = ~(delay / 10);

  RegWrite(MTCCoarseDelayReg,rtdel_value);
  RegRead(MTCControlReg,&temp);
  RegWrite(MTCControlReg, temp | LOAD_ENPW);
  RegRead(MTCControlReg,&temp);
  RegWrite(MTCControlReg, temp & ~LOAD_ENPW);
  //lprintf( "Coarse delay is set to %u ns\n", delay);
  return 0;
} 

float MTCModel::SetFineDelay(float delay)
{
  uint32_t temp, addel_value;;
  int result;
  float addel_slope;   /* ADDEL value per ns of delay */
  float fdelay_set;

  //FIXME
  //addel_slope = 0.1;
  // get up to date fine slope value
  pouch_request *response = pr_init();
  char get_db_address[500];
  sprintf(get_db_address,"%s/%s/MTC_doc",DB_SERVER,DB_BASE_NAME);
  pr_set_method(response, GET);
  pr_set_url(response, get_db_address);
  pr_do(response);
  if (response->httpresponse != 200){
    lprintf("Unable to connect to database. error code %d\n",(int)response->httpresponse);
    pr_free(response);
    return -1.0;
  }
  JsonNode *doc = json_decode(response->resp.data);
  addel_slope = (float) json_get_number(json_find_member(json_find_member(doc,"mtcd"),"fine_slope")); 
  addel_value = (uint32_t)(delay / addel_slope);
  json_delete(doc);
  pr_free(response);

  if (addel_value > 255) {
    lprintf("Fine delay value out of range\n");
    return -1.0;
  }
  addel_value = (uint32_t)(delay / addel_slope);

  RegWrite(MTCFineDelayReg,addel_value);
  RegRead(MTCControlReg,&temp);
  RegWrite(MTCControlReg, temp | LOAD_ENPW);
  RegRead(MTCControlReg,&temp);
  RegWrite(MTCControlReg, temp & ~LOAD_ENPW);

  fdelay_set = (float)addel_value*addel_slope;
  //lprintf( "Fine delay is set to %f ns\n", fdelay_set);
  return fdelay_set;
}

void MTCModel::ResetMemory()
{
  uint32_t temp;
  RegRead(MTCControlReg,&temp);
  RegWrite(MTCControlReg,temp | FIFO_RESET);
  RegRead(MTCControlReg,&temp);
  RegWrite(MTCControlReg,temp & ~FIFO_RESET);
  RegWrite(MTCBbaReg,0x0);  
  lprintf("The FIFO control has been reset and the BBA register has been cleared\n");
} 

int MTCModel::XilinxLoad()
{
  char *data;
  long howManybits;
  long bitIter;
  uint32_t word;
  uint32_t dp;
  uint32_t temp;

  lprintf("loading xilinx\n");
  data = GetXilinxData(&howManybits);
  lprintf("Got %ld bits of xilinx data\n",howManybits);
  if ((data == NULL) || (howManybits == 0)){
    lprintf("error getting xilinx data\n");
    return -1;
  }

  SBCPacket *packet; 
  packet = (SBCPacket *) malloc(sizeof(SBCPacket));
  packet->header.destination = 0x3;
  packet->header.cmdID = MTC_XILINX_ID;
  packet->header.numberBytesinPayload = sizeof(SNOMtcXilinxLoadStruct) + howManybits;
  packet->numBytes = packet->header.numberBytesinPayload+256+16;
  SNOMtcXilinxLoadStruct *payloadPtr = (SNOMtcXilinxLoadStruct *)packet->payload;
  payloadPtr->baseAddress = 0x7000;
  payloadPtr->addressModifier = 0x29;
  payloadPtr->errorCode = 0;
  payloadPtr->programRegOffset = MTCXilProgReg;
  payloadPtr->fileSize = howManybits;
  char *p = (char *)payloadPtr + sizeof(SNOMtcXilinxLoadStruct);
  strncpy(p, data, howManybits);
  free(data);
  data = (char *) NULL;

  int errors;
  try{
    errors = fLink->SendXilinxPacket(packet);
  }catch(const char* s){
    free(packet);
    throw s;
  }

  long errorCode = payloadPtr->errorCode;
  if (errorCode){
    lprintf("Error code: %d \n",(int)errorCode);
    lprintf("Failed to load xilinx!\n");  
    free(packet);
    return -5;
  }else if (errors){
    lprintf("Failed to load xilinx!\n");  
    free(packet);
    return errors;
  }

  lprintf("Xilinx loading complete\n");
  free(packet);
  return 0;
}

char* MTCModel::GetXilinxData(long *howManyBits)
{
  char c;
  FILE *fp;
  char *data = NULL;
  char filename[500];
  sprintf(filename,"%s/%s",PENN_DAQ_ROOT,MTC_XILINX_LOCATION);

  if ((fp = fopen(filename, "r")) == NULL ) {
    lprintf( "getXilinxData:  cannot open file %s\n", filename);
    return (char*) NULL;
  }

  if ((data = (char *) malloc(MAX_DATA_SIZE)) == NULL) {
    //perror("GetXilinxData: ");
    lprintf("GetXilinxData: malloc error\n");
    return (char*) NULL;
  }

  /* skip header -- delimited by two slashes. 
     if ( (c = getc(fp)) != '/') {
     fprintf(stderr, "Invalid file format Xilinx file.\n");
     return (char*) NULL;
     }
     while (( (c = getc(fp))  != EOF ) && ( c != '/'))
     ;
   */
  /* get real data now. */
  *howManyBits = 0;
  while (( (data[*howManyBits] = getc(fp)) != EOF)
      && ( *howManyBits < MAX_DATA_SIZE)) {
    /* skip newlines, tabs, carriage returns */
    if ((data[*howManyBits] != '\n') &&
        (data[*howManyBits] != '\r') &&
        (data[*howManyBits] != '\t') ) {
      (*howManyBits)++;
    }


  }
  fclose(fp);
  return data;
}

