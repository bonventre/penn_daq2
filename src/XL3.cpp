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
  fListener = evconnlistener_new_bind(evBase,(evconnlistener_cb) &XL3::AcceptCallbackHandler,this,LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, -1, (struct sockaddr*)&sin, sizeof(sin));
  if (!fListener){
    printf("Couldn't create XL3 %d listener\n",fCrateNum);
  }
}

XL3::~XL3()
{
  evconnlistener_free(fListener);
}

void XL3::AcceptCallbackHandler(evutil_socket_t fd, short events, void *ctx)
{
  printf("got it\n");
  (static_cast<XL3*>(ctx))->AcceptCallback(fd, events);
}

void XL3::AcceptCallback(evutil_socket_t fd, short events)
{
  printf("got it again\n");
}
