#include <cstring>
#include <time.h>
#include <errno.h>

#include "Globals.h"

#include "NetUtils.h"
#include "XL3Link.h"


XL3Link::XL3Link(int crateNum) : GenericLink(XL3_PORT + crateNum)
{
  fBytesLeft = 0;
  fTempBytes = 0;
  memset(fTempPacket,0,sizeof(fTempPacket));
  fCrateNum = crateNum; 
  pthread_mutex_init(&fRecvQueueLock,NULL);
  pthread_cond_init(&fRecvQueueCond,NULL);
}

XL3Link::~XL3Link()
{
}

void XL3Link::RecvCallback(struct bufferevent *bev)
{
  int totalLength = 0;
  int n;
  char input[10000];
  memset(input,'\0',10000);
  while (1){
    bufferevent_lock(bev);
    n = bufferevent_read(bev, input+strlen(input), sizeof(input));
    bufferevent_unlock(bev);
    totalLength += n;
    if (n <= 0)
      break;
  }

  char *inputP = input;
  while (totalLength > 0){
    if (fTempBytes == 0){
      if (totalLength >= XL3_PACKET_SIZE){
        ProcessPacket((XL3Packet *)inputP); 
        totalLength -= XL3_PACKET_SIZE;
        inputP += XL3_PACKET_SIZE;
      }else{
        memcpy(fTempPacket,inputP,totalLength);
        fBytesLeft = XL3_PACKET_SIZE-totalLength; 
        fTempBytes = totalLength;
        break;
      }
    }else{
      if (totalLength >= fBytesLeft){
        memcpy(fTempPacket+fTempBytes,inputP,fBytesLeft);
        ProcessPacket((XL3Packet *)fTempPacket);
        memset(fTempPacket,0,sizeof(fTempPacket));
        inputP += fBytesLeft;
        totalLength -= fBytesLeft;
        fBytesLeft = 0;
        fTempBytes = 0;
      }else{
        memcpy(fTempPacket+fTempBytes,inputP,totalLength);
        fBytesLeft -= totalLength;
        fTempBytes += totalLength;
        break;
      }
    }
  }
}

void XL3Link::ProcessPacket(XL3Packet *packet)
{
//  uint32_t *p = (uint32_t *) packet;
//  for (int j=0;j<10;j++)
//    lprintf("%d: %08x\n",j,*(p+j));
  switch (packet->header.packetType){
    case PING_ID:
      {
      packet->header.packetType = PONG_ID;
      bufferevent_lock(fBev);
      bufferevent_write(fBev,packet,sizeof(XL3Packet));
      bufferevent_unlock(fBev);
      break;
      }
    case MESSAGE_ID:
      {
        lprintf("%s",packet->payload);
        break;
      }
    case CMD_ACK_ID:
      {
        pthread_mutex_lock(&fRecvQueueLock);
        MultiCommand *commands = (MultiCommand *) packet->payload;
        SwapLongBlock(&(commands->howMany),1);
        for (int i=0;i<commands->howMany;i++){
          SwapLongBlock(&(commands->cmd[i].cmdNum),1);
          SwapShortBlock(&(commands->cmd[i].packetNum),1);
          SwapLongBlock(&(commands->cmd[i].data),1);
          fRecvCmdQueue.push(commands->cmd[i]); 
        }
        if (commands->howMany > 0)
          pthread_cond_signal(&fRecvQueueCond);
        pthread_mutex_unlock(&fRecvQueueLock);
        break;
      }
    case MEGA_BUNDLE_ID:
      {
        struct timeval start,end;
        if (megaBundleCount == 0){
          gettimeofday(&start,0);
          startTime = start.tv_sec*1000000 + start.tv_usec;
          recvBytes = 0;
        }
        gettimeofday(&end,0);
        endTime = end.tv_sec*1000000 + end.tv_usec;
        long int avg_dt = endTime - startTime;

        MegaBundleHeader *mega = (MegaBundleHeader *) packet->payload;
        SwapLongBlock(mega,sizeof(MegaBundleHeader)/sizeof(uint32_t));
        int crate = (mega->info & 0x1F000000)>>24;
        lfprintf("Mega crate %d, passmin %d, xl3clock %d\n",crate,mega->passMin,mega->xl3Clock);
        int wordsLeft = (mega->info & 0xFFFFFF);
        SwapLongBlock(mega+1,wordsLeft);
        int numBundles = 0;
        MiniBundleHeader *mini = (MiniBundleHeader *) (mega+1);
        while (wordsLeft){
          wordsLeft--;
          int miniSize = (mini->info & 0xFFFFFF);
          if (mini->info & 0x80000000){
            uint32_t passcur = *(uint32_t *) (mini+1);
            lfprintf("     crate %d, passcur %d\n",crate,passcur);
            //lprintf("PassCur minibundle: %d\n",passcur);
          }else{
            numBundles += miniSize/3; 
            int card = (mini->info & 0x0F000000) >> 24;
            lfprintf("     crate %d, card %d, %d bundles\n",crate,card,numBundles);
          }
          if (miniSize > wordsLeft){
            lprintf("Corrupted minibundle! Size = %d\n",miniSize);
            break;
          }
          mini += 1 + miniSize;
          wordsLeft -= miniSize;
        }



        if (numBundles > 0){
          recvBytes += numBundles*12;
          megaBundleCount++;

          if (megaBundleCount%BUNDLE_PRINT == 0){
            long int inst_dt = endTime - lastPrintTime;
            lastPrintTime = endTime;
            lprintf("recv average: %8.2f Mb/s \t d/dt: %8.2f Mb/s (time: %2.4f us/bundle)\n",
                (float) (recvBytes*8/(float)avg_dt),
                (float)(numBundles*12*8*BUNDLE_PRINT/(float)inst_dt),
                1.0/(float)(recvBytes*8/(float)avg_dt)*96);
          }
        }
        break;
      }
    case ERROR_ID:
      {
        ErrorPacket *errors = (ErrorPacket *) packet->payload;
        lprintf("Error: cmdRejected:%d, transferError:%d, xl3DAUnknown:%d\n",errors->cmdRejected,errors->transferError,errors->xl3DataAvailUnknown);
        lprintf("bundleread: ");
        for (int i=0;i<16;i++)
          lprintf("%d ",errors->fecBundleReadError[i]);
        lprintf("\n");
        lprintf("bundleresync: ");
        for (int i=0;i<16;i++)
          lprintf("%d ",errors->fecBundleResyncError[i]);
        lprintf("\n");
        lprintf("memlevelunknown: ");
        for (int i=0;i<16;i++)
          lprintf("%d ",errors->fecMemLevelUnknown[i]);
        lprintf("\n");
        break;
      }
    case SCREWED_ID:
      {
        ScrewedPacket *screwed = (ScrewedPacket *) packet->payload;
        lprintf("Screwed:\n");
        for (int i=0;i<16;i++)
          lprintf("Slot #%d: %d\n",i,screwed->fecScrewed[i]);
        break;
      }
    default:
      pthread_mutex_lock(&fRecvQueueLock);
      fRecvQueue.push(*packet);
      pthread_cond_signal(&fRecvQueueCond);
      pthread_mutex_unlock(&fRecvQueueLock);
      break;
  }
}


void XL3Link::AcceptCallback(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *address, int socklen)
{
  GenericLink::AcceptCallback(listener,fd,address,socklen);
  lprintf("XL3 %d connected\n",fCrateNum);
}

int XL3Link::SendPacket(XL3Packet *packet)
{
  SwapShortBlock(&packet->header.packetNum,1);
  bufferevent_lock(fBev);
  pthread_mutex_lock(&fRecvQueueLock);
  if (!fRecvQueue.empty())
    lprintf("Theres still stuff in the queue!\n");
  if (!fRecvCmdQueue.empty())
    lprintf("There are still cmd acks in the queue\n");
  pthread_mutex_unlock(&fRecvQueueLock);
  bufferevent_write(fBev,packet,sizeof(XL3Packet));
  bufferevent_unlock(fBev);
  return 0;
}

int XL3Link::GetNextPacket(XL3Packet *packet,int waitSeconds)
{
  pthread_mutex_lock(&fRecvQueueLock);
  if (waitSeconds){
    struct timeval tp;
    struct timespec ts;
    gettimeofday(&tp, NULL);
    ts.tv_sec  = tp.tv_sec;
    ts.tv_nsec = tp.tv_usec * 1000;
    ts.tv_sec += waitSeconds;
    while (fRecvQueue.empty()){
      int rc = pthread_cond_timedwait(&fRecvQueueCond,&fRecvQueueLock,&ts);
      if (rc == ETIMEDOUT) {
        lprintf("XL3Link::GetNextPacket: Wait timed out!\n");
        rc = pthread_mutex_unlock(&fRecvQueueLock);
        return 1;
      }
    }
  }else{
    while (fRecvQueue.empty())
      pthread_cond_wait(&fRecvQueueCond,&fRecvQueueLock);
  }

  *packet = fRecvQueue.front();
  SwapShortBlock(&packet->header.packetNum,1);
  fRecvQueue.pop();
  pthread_mutex_unlock(&fRecvQueueLock);
  return 0;
}

int XL3Link::GetNextCmdAck(Command *command,int waitSeconds)
{
  pthread_mutex_lock(&fRecvQueueLock);
  if (waitSeconds){
    struct timeval tp;
    struct timespec ts;
    gettimeofday(&tp, NULL);
    ts.tv_sec  = tp.tv_sec;
    ts.tv_nsec = tp.tv_usec * 1000;
    ts.tv_sec += waitSeconds;
    while (fRecvCmdQueue.empty()){
      int rc = pthread_cond_timedwait(&fRecvQueueCond,&fRecvQueueLock,&ts);
      if (rc == ETIMEDOUT) {
        lprintf("XL3Link::GetNextCmdAck: Wait timed out!\n");
        rc = pthread_mutex_unlock(&fRecvQueueLock);
        return 1;
      }
    }
  }else{
    while (fRecvCmdQueue.empty())
      pthread_cond_wait(&fRecvQueueCond,&fRecvQueueLock);
  }

  *command = fRecvCmdQueue.front();
  fRecvCmdQueue.pop();
  pthread_mutex_unlock(&fRecvQueueLock);
  return 0;
}

int XL3Link::CheckQueue(int empty)
{
  if (fRecvQueue.empty() && fRecvCmdQueue.empty()){
    if (fBytesLeft){
      lprintf("%d bytes left\n",fBytesLeft);
      return -3;
    }
    return 0;
  }else if (empty){
    while(!fRecvQueue.empty()) fRecvQueue.pop();
    while(!fRecvCmdQueue.empty()) fRecvCmdQueue.pop();
    return -1;
  }
  return -2;
}
