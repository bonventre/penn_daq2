#include <cstdlib>
#include <cstdio>
#include <csignal>
#include <cstring>
#include <pthread.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <event2/event.h>
#include <event2/event_struct.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#include "Globals.h"

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

  pthread_mutex_init(&startTestLock,NULL);

  //pthread_t mythread;
  //int ret = pthread_create(&mythread,NULL,threadfunc,NULL);
  event_base_dispatch(evBase);
  printf("done dispatching\n");

  return 0;
}
