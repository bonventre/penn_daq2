#include "Globals.h"
#include "MTCRegisters.h"

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
  SBCPacket packet;
  packet.header.destination = 0x1;
  packet.header.cmdID = MTC_WRITE_ID;
  packet.header.numberBytesinPayload  = sizeof(SBCVmeWriteBlockStruct)+sizeof(uint32_t);
  //packet->cmdHeader.numberBytesinPayload  = 256+28;
  //packet->numBytes = 256+28+16;
  SBCVmeWriteBlockStruct *writestruct;
  writestruct = (SBCVmeWriteBlockStruct *) packet.payload;
  writestruct->address = address + MTCRegAddressBase;
  //writestruct->address = address;
  writestruct->addressModifier = MTCRegAddressMod;
  writestruct->addressSpace = MTCRegAddressSpace;
  writestruct->unitSize = 4;
  writestruct->numItems = 1;
  writestruct++;
  uint32_t *data_ptr = (uint32_t *) writestruct;
  *data_ptr = data;

  SendCommand(&packet);
  return 0;
}

int MTCModel::RegRead(uint32_t address, uint32_t *data)
{
  SBCPacket packet;
  uint32_t *result;
  packet.header.destination = 0x1;
  packet.header.cmdID = MTC_READ_ID;
  packet.header.numberBytesinPayload = sizeof(SBCVmeReadBlockStruct)+sizeof(uint32_t);
  //packet->numBytes = 256+27+16;
  SBCVmeReadBlockStruct *readstruct;
  readstruct = (SBCVmeReadBlockStruct *) packet.payload;
  readstruct->address = address + MTCRegAddressBase;
  readstruct->addressModifier = MTCRegAddressMod;
  readstruct->addressSpace = MTCRegAddressSpace;
  readstruct->unitSize = 4;
  readstruct->numItems = 1;

  SendCommand(&packet);
  result = (uint32_t *) (readstruct+1);
  *data = *result;
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
      throw 2;
  }
  return 0;
}

