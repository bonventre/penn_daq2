#include "Globals.h"
#include "XL3PacketTypes.h"

#include "XL3Link.h"
#include "XL3Model.h"

XL3Model::XL3Model(int crateNum)
{
  fCrateNum = crateNum; 
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
  fLink->GetNextPacket(&packet,1);

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

int XL3Model::SendCommand(XL3Packet *packet,int withResponse)
{
  uint16_t type = packet->header.packetType;
  fLink->SendPacket(packet);
  if (withResponse){
    fLink->GetNextPacket(packet);
    if (packet->header.packetType != type){
      printf("wrong type: expected %02x, got %02x\n",type,packet->header.packetType);
    }
  }
  return 0;
}
