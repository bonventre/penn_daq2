#include <cstdio>

#include "Globals.h"

#include "NetUtils.h"
#include "XL3Link.h"
#include "XL3Model.h"
#include "XL3Cmds.h"

void *doXL3RW(void *arg)
{
  int crate = 2;
  uint32_t address = 0x02000007;
  uint32_t data = 0x0;

  char *buffer = (char *) arg;

  char *words,*words2;
  words = strtok(buffer, " ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'c'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          crate = atoi(words2);
      }else if (words[1] == 'a'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          address = strtoul(words2,(char**)NULL,16);
      }else if (words[1] == 'd'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          data = strtoul(words2,(char**)NULL,16);
      }else if (words[1] == 'h'){
        printf("Usage: xl3_rw -c [crate_num (int)] "
            "-a [address (hex)] -d [data (hex)]\n");
        printf("Please check xl3/xl3_registers.h for the address mapping\n");
        //free(args);
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }



  int sbc = 0;
  uint32_t xl3Mask = 0x1<<crate;
  int busy = LockConnections(sbc,xl3Mask);
  if (busy){
    printf("Those connections are currently in use.\n");
    return NULL;
  }

  uint32_t result;
  int errors = xl3s[crate]->RW(address,data,&result);
  if (errors)
    printf("There was a bus error.\n");
  else
    printf("Wrote to %08x, got %08x\n",address,result);

  UnlockConnections(sbc,xl3Mask);
  printf("exiting\n");
}

void *fecTest(void *arg)
{
  int crate = 2;
  uint32_t slotMask = 0x2000;

  char *buffer = (char *) arg;

  char *words,*words2;
  words = strtok(buffer, " ");
  while (words != NULL){
    if (words[0] == '-'){
      if (words[1] == 'c'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          crate = atoi(words2);
      }else if (words[1] == 's'){
        if ((words2 = strtok(NULL, " ")) != NULL)
          slotMask = strtoul(words2,(char**)NULL,16);
      }else if (words[1] == 'h'){
        printf("Usage: xl3_rw -c [crate_num (int)] "
            "-a [address (hex)] -d [data (hex)]\n");
        printf("Please check xl3/xl3_registers.h for the address mapping\n");
        //free(args);
        return 0;
      }
    }
    words = strtok(NULL, " ");
  }



  int sbc = 0;
  uint32_t xl3Mask = 0x1<<crate;
  int busy = LockConnections(sbc,xl3Mask);
  if (busy){
    printf("Those connections are currently in use.\n");
    return NULL;
  }

  XL3Packet packet;
  packet.header.packetType = FEC_TEST_ID;
  *(uint32_t *) packet.payload = slotMask;
  SwapLongBlock(packet.payload,1);
  xl3s[crate]->SendCommand(&packet);

  UnlockConnections(sbc,xl3Mask);
  printf("exiting\n");

}
