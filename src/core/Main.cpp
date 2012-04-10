#include <cstdlib>
#include <cstdio>
#include <csignal>
#include <cstring>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <event2/event.h>
#include <event2/event_struct.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#include "NetUtils.h"
#include "Main.h"

static void cont_accept_cb(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *address, int socklen, void *ctx)
{
  bufferevent *bev = bufferevent_socket_new(evBase,fd,BEV_OPT_CLOSE_ON_FREE);
  char rejectmsg[100];
  memset(rejectmsg,'\0',100);
  sprintf(rejectmsg,"Go away\n");
  bufferevent_write(bev,&rejectmsg,sizeof(rejectmsg));
}

int main(int argc, char *argv[])
{
  setupListeners();

  struct sockaddr_in sin;
  memset(&sin,0,sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_port = htons(44599);
  printf("listener\n");
  contListener = evconnlistener_new_bind(evBase,cont_accept_cb,NULL,LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE,-1,(struct sockaddr*)&sin,sizeof(sin));; 
  printf("dispatching\n");
  event_base_dispatch(evBase);
  printf("done dispatching\n");

  return 0;
}
