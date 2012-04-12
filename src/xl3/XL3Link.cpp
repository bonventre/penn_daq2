#include <cstring>
#include <time.h>
#include <errno.h>

#include "Globals.h"
#include "PacketTypes.h"

#include "NetUtils.h"
#include "XL3Link.h"


XL3Link::XL3Link(int crateNum) : GenericLink(XL3_PORT + crateNum)
{
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
    n = bufferevent_read(bev, input+strlen(input), sizeof(input));
    totalLength += n;
    if (n <= 0)
      break;
  }


  XL3Packet *packet = (XL3Packet *) input;
  switch (packet->header.packetType){
    case PING_ID:
      {
      packet->header.packetType = PONG_ID;
      bufferevent_write(fBev,packet,sizeof(XL3Packet));
      break;
      }
    case MESSAGE_ID:
      {
      printf("%s",packet->payload);
      break;
      }
    case CMD_ACK_ID:
      {
        pthread_mutex_lock(&fRecvQueueLock);
        MultiCommand *commands = (MultiCommand *) packet->payload;
        for (int i=0;commands->howMany;i++){
          SwapLongBlock(&(commands->cmd[i].cmdNum),1);
          SwapShortBlock(&(commands->cmd[i].packetNum),1);
          SwapLongBlock(&(commands->cmd[i].data),1);
          fRecvCmdQueue.push(commands->cmd[i]); 
        }
        if (commands->howMany > 0)
          pthread_cond_signal(&fRecvQueueCond);
        pthread_mutex_unlock(&fRecvQueueLock);
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
  printf("XL3 %d connected\n",fCrateNum);
}

int XL3Link::SendPacket(XL3Packet *packet)
{
  SwapShortBlock(&packet->header.packetNum,1);
  bufferevent_write(fBev,packet,sizeof(XL3Packet));
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
        printf("Wait timed out!\n");
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
        printf("Wait timed out!\n");
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
