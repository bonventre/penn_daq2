#include <cstring>

#include "XL3PacketTypes.h"

#include "NetUtils.h"
#include "XL3Link.h"


XL3Link::XL3Link(int crateNum) : GenericLink(XL3_PORT + crateNum)
{
  fCrateNum = crateNum; 
  fRecvCount = 0;
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
      printf("ping!\n");
      packet->header.packetType = PONG_ID;
      bufferevent_write(fBev,packet,sizeof(XL3Packet));
      break;
    case MESSAGE_ID:
      printf("%s",packet->payload);
      break;
    default:
      break;
  }

}


void XL3Link::AcceptCallback(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *address, int socklen)
{
  GenericLink::AcceptCallback(listener,fd,address,socklen);
  printf("XL3 %d connected\n",fCrateNum);
}
