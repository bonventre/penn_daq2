#include <cstring>

#include "Globals.h"
#include "XL3PacketTypes.h"

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

int XL3Link::GetNextPacket(XL3Packet *packet)
{
  pthread_mutex_lock(&fRecvQueueLock);
  while (fRecvQueue.empty())
    pthread_cond_wait(&fRecvQueueCond,&fRecvQueueLock);

  *packet = fRecvQueue.front();
  SwapShortBlock(&packet->header.packetNum,1);
  fRecvQueue.pop();
  pthread_mutex_unlock(&fRecvQueueLock);
  return 0;
}
