#include "Globals.h"
#include "PacketTypes.h"
#include "DBTypes.h"

#include "XL3Link.h"
#include "XL3Model.h"

XL3Model::XL3Model(int crateNum)
{
  fCrateNum = crateNum; 
  for (int i=0;i<16;i++){
    fFECs[i].mbID = 0x0;
    for (int j=0;j<4;j++)
      fFECs[i].dbID[j] = 0x0;
  }
  fLink = new XL3Link(crateNum);
}

XL3Model::~XL3Model()
{
  delete fLink;
}

int XL3Model::RW(uint32_t address, uint32_t data, uint32_t *result)
{
  XL3Packet packet;
  packet.header.packetType = FAST_CMD_ID;
  FastCmdArgs *args = (FastCmdArgs *) packet.payload;
  args->command.address = address;
  args->command.data = data;
  SwapLongBlock(&args->command.address,1);
  SwapLongBlock(&args->command.data,1);

  fLink->SendPacket(&packet);

  int numPackets = 0;
  int maxTries = 10;
  int err;
  // look for the response. If you get the wrong packet type, try again, but
  // eventually raise an exception
  do{
    err = fLink->GetNextPacket(&packet,1);
    numPackets++;
    if (numPackets > maxTries)
      throw 2;
  }while(packet.header.packetType != FAST_CMD_ID || err);

  SwapLongBlock(&args->command.data,1);
  *result = args->command.data;

  return 0;
}

int XL3Model::CheckLock()
{
  if (fLink->IsConnected() && !fLink->IsLocked())
    return 0;
  else
    return 1;
}

int XL3Model::SendCommand(XL3Packet *packet,int withResponse, int timeout)
{
  uint16_t type = packet->header.packetType;
  fLink->SendPacket(packet);
  if (withResponse){
    int numPackets = 0;
    int maxTries = 10;
    int err;
    // look for the response. If you get the wrong packet type, try again, but
    // eventually raise an exception
    do{
      err = fLink->GetNextPacket(packet,timeout);
      numPackets++;
      if (numPackets > maxTries)
        throw 2;
    }while(packet->header.packetType != type || err);
  }
  return 0;
}

int XL3Model::DeselectFECs()
{
  XL3Packet packet;
  packet.header.packetType = DESELECT_FECS_ID;
  SendCommand(&packet);
  return 0;
}

int XL3Model::GetMultiFCResults(int numCmds, int packetNum, uint32_t *result, int timeout)
{
  packetNum %= 65536;
  Command command;
  int cmdNum = 0;
  int started = 0;
  int busErrors = 0;
  while (numCmds > cmdNum){
    int err = fLink->GetNextCmdAck(&command,timeout);
    if (err)
      throw 1;
    if (command.packetNum != packetNum){
      if (started)
        throw 3;
      else
        continue;
    }
    if (command.cmdNum != cmdNum)
      throw 4;
    busErrors += command.flags;
    *(result + cmdNum) = command.data;
    cmdNum++;
  }
  return 0;
}

int XL3Model::UpdateCrateConfig(uint16_t slotMask)
{
  XL3Packet packet;
  packet.header.packetType = BUILD_CRATE_CONFIG_ID;
  BuildCrateConfigArgs *args = (BuildCrateConfigArgs *) packet.payload;
  BuildCrateConfigResults *results = (BuildCrateConfigResults *) packet.payload;
  args->slotMask = slotMask;
  SwapLongBlock(packet.payload,sizeof(BuildCrateConfigArgs)/sizeof(uint32_t));
  try{
    SendCommand(&packet);
    int errors = results->errorFlags;
    for (int i=0;i<16;i++){
      if ((0x1<<i) & slotMask){
        fFECs[i] = results->hwareVals[i];
        SwapShortBlock(&(fFECs[i].mbID),1);
        SwapShortBlock((fFECs[i].dbID),4);
      }
    }
    DeselectFECs();
  }
  catch(int e){
    printf("Error: Unable to update crate configuration\n");
    throw e;
  }
  return 0;
}

int XL3Model::ChangeMode(int mode, uint16_t dataAvailMask)
{
  XL3Packet packet;
  packet.header.packetType = CHANGE_MODE_ID;
  ChangeModeArgs *args = (ChangeModeArgs *) packet.payload;
  args->mode = mode;
  args->dataAvailMask = dataAvailMask;
  SwapLongBlock(packet.payload,sizeof(ChangeModeArgs)/sizeof(uint32_t));
  SendCommand(&packet);
  return 0;
}

int XL3Model::ConfigureCrate(FECConfiguration *fecs)
{
  for (int i=0;i<16;i++){
    fFECs[i] = fecs[i];
  }
  return 0;
}

