#include "Globals.h"

#define NEED_TO_SWAP 1

XL3Model *xl3s[MAX_XL3_CON];
ControllerLink *contConnection;
pthread_mutex_t startTestLock;

struct event_base *evBase;

int LockConnections(int sbc, uint32_t xl3List)
{
  int failFlag = 0;
  pthread_mutex_lock(&startTestLock);
  if (sbc){
  }
  for (int i=0;i<MAX_XL3_CON;i++){
    if ((0x1<<i) & xl3List){
      if (xl3s[i]->CheckLock()){
        failFlag = 1;
      }
    }
  }

  if (!failFlag){
    if (sbc){
    }
    for (int i=0;i<MAX_XL3_CON;i++){
      if ((0x1<<i) & xl3List){
        xl3s[i]->Lock();
      }
    }
  }
  pthread_mutex_unlock(&startTestLock);
  return failFlag;
}

int UnlockConnections(int sbc, uint32_t xl3List)
{
  pthread_mutex_lock(&startTestLock);
  if (sbc){
  }
  for (int i=0;i<MAX_XL3_CON;i++){
    if ((0x1<<i) & xl3List){
      xl3s[i]->UnLock(); 
    }
  }
  pthread_mutex_unlock(&startTestLock);
  return 0;
}

void SwapLongBlock(void* p, int32_t n)
{
  if (NEED_TO_SWAP){
    int32_t* lp = (int32_t*)p;
    int32_t i;
    for(i=0;i<n;i++){
      int32_t x = *lp;
      *lp = (((x) & 0x000000FF) << 24) |
        (((x) & 0x0000FF00) << 8) |
        (((x) & 0x00FF0000) >> 8) |
        (((x) & 0xFF000000) >> 24);
      lp++;
    }
  }
}

void SwapShortBlock(void* p, int32_t n)
{
  if (NEED_TO_SWAP){
    int16_t* sp = (int16_t*)p;
    int32_t i;
    for(i=0;i<n;i++){
      int16_t x = *sp;
      *sp = ((x & 0x00FF) << 8) |
        ((x & 0xFF00) >> 8) ;
      sp++;
    }
  }
}


