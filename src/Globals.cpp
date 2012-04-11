#include "Globals.h"

XL3Model *xl3s[MAX_XL3_CON];
XL3Link *xl3Connections[MAX_XL3_CON];
ControllerLink *contConnection;
pthread_mutex_t startTestLock;

struct event_base *evBase;

int LockConnections(int sbc, uint32_t xl3s)
{
  int failFlag = 0;
  pthread_mutex_lock(&startTestLock);
  if (sbc){
  }
  for (int i=0;i<MAX_XL3_CON;i++){
    if ((0x1<<i) & xl3s){
      if (xl3Connections[i]->fConnected != 1 || xl3Connections[i]->fLock != 0){
        failFlag = 1;
      }
    }
  }

  if (!failFlag){
    if (sbc){
    }
    for (int i=0;i<MAX_XL3_CON;i++){
      if ((0x1<<i) & xl3s){
        xl3Connections[i]->fLock = 1;
      }
    }
  }
  pthread_mutex_unlock(&startTestLock);
  return failFlag;
}

int UnlockConnections(int sbc, uint32_t xl3s)
{
  pthread_mutex_lock(&startTestLock);
  if (sbc){
  }
  for (int i=0;i<MAX_XL3_CON;i++){
    if ((0x1<<i) & xl3s){
      xl3Connections[i]->fLock = 0;
    }
  }
  pthread_mutex_unlock(&startTestLock);
  return 0;
}
