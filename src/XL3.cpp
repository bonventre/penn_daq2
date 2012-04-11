#include <cstring>

#include "NetUtils.h"
#include "XL3.h"


XL3::XL3(int crateNum) : GenericCallback(XL3_PORT + crateNum)
{
  fCrateNum = crateNum; 
  fRecvCount = 0;
}

XL3::~XL3()
{
}

void XL3::RecvCallback(struct bufferevent *bev)
{
  printf("recv callback\n");
  fRecvCount++;
}


void XL3::AcceptCallback(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *address, int socklen)
{
  GenericCallback::AcceptCallback(listener,fd,address,socklen);
  printf("XL3 %d connected\n",fCrateNum);
}
