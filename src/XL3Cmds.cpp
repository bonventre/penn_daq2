#include <cstdio>

#include "Globals.h"

#include "NetUtils.h"
#include "XL3Link.h"
#include "XL3Model.h"
#include "XL3Cmds.h"

void *doXL3RW(void *arg)
{
  char *buffer = (char *) arg;

  int crate = 2;
  uint32_t address = 0x02000007;
  uint32_t data = 0x0;

  int sbc = 0;
  uint32_t xl3Mask = 0x1<<crate;
  int busy = LockConnections(sbc,xl3Mask);
  if (busy){
    printf("Those connections are currently in use.\n");
    return NULL;
  }

  int errors = xl3s[crate]->RW(address,data);
  if (errors)
    printf("There was a bus error.\n");
  else
    printf("Wrote to %08x\n",address);
  
  UnlockConnections(sbc,xl3Mask);
  printf("exiting\n");
}
