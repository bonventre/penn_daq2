#include <cstdio>

#include "Main.h"
#include "NetUtils.h"
#include "XL3Link.h"
#include "XL3Cmds.h"

extern XL3Link *xl3[MAX_XL3_CON];

void *doXL3RW(void *arg)
{
  char *buffer = (char *) arg;
  //printf("buffer is %s\n",buffer);
  pthread_mutex_lock(&startTestLock);
  printf("waiting for data\n");
  int curCount = xl3[3]->fRecvCount;
  while (xl3[3]->fRecvCount < curCount+3){}
  pthread_mutex_unlock(&startTestLock);
  printf("exiting\n");
}
