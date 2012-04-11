#include <cstring>

#include "NetUtils.h"
#include "XL3.h"


XL3::XL3(int crateNum)
{
  fCrateNum = crateNum; 

  // set up listener
  struct sockaddr_in sin;
  memset(&sin,0,sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_port = htons(XL3_PORT + fCrateNum);
  fListener = evconnlistener_new_bind(evBase,&XL3::AcceptCallbackHandler,this,LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, -1, (struct sockaddr*)&sin, sizeof(sin));
  if (!fListener){
    printf("Couldn't create XL3 %d listener\n",fCrateNum);
  }
}

XL3::~XL3()
{
  evconnlistener_free(fListener);
}

void XL3::AcceptCallback(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *address, int socklen)
{
  fFD = fd;
  fBev = bufferevent_socket_new(evBase,fFD,BEV_OPT_CLOSE_ON_FREE);
  bufferevent_setcb(fBev,&XL3::RecvCallbackHandler,&XL3::SentCallbackHandler,&XL3::EventCallbackHandler,this);
  bufferevent_enable(fBev,EV_READ);
  printf("XL3 %d connected.\n",fCrateNum);
}

void XL3::RecvCallback(struct bufferevent *bev)
{
  printf("recv callback\n");
}
