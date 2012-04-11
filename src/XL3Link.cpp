#include <cstring>

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
  printf("recv callback\n");
  fRecvCount++;
}


void XL3Link::AcceptCallback(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *address, int socklen)
{
  GenericLink::AcceptCallback(listener,fd,address,socklen);
  printf("XL3 %d connected\n",fCrateNum);
}
