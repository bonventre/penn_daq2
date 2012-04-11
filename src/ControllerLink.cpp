#include <cstring>
#include <csignal>
#include <pthread.h>

#include "XL3Cmds.h"
#include "NetUtils.h"
#include "ControllerLink.h"


ControllerLink::ControllerLink() : GenericLink(CONT_PORT)
{
}

ControllerLink::~ControllerLink()
{
}

int ControllerLink::fNumControllers = 0;

void ControllerLink::AcceptCallback(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *address, int socklen)
{
  if (fNumControllers >= MAX_CONT_CON){
    printf("Too many controllers already, rejecting connection\n");
    evutil_closesocket(fd);
    return;
  }

  fNumControllers++;
  GenericLink::AcceptCallback(listener,fd,address,socklen);
  printf("Controller connected\n");
}

void ControllerLink::RecvCallback(struct bufferevent *bev)
{
  int totalLength = 0;
  int n;
  char input[1000];
  memset(input,'\0',1000);
  while (1){
    n = bufferevent_read(bev, input+strlen(input), sizeof(input));
    totalLength += n;
    if (n <= 0)
      break;
  }

  ProcessCommand(input);
}

void ControllerLink::EventCallback(struct bufferevent *bev, short what)
{
  if (what & BEV_EVENT_CONNECTED){
  }else if (what & BEV_EVENT_ERROR){
  }else if (what & BEV_EVENT_EOF){
    printf("Controller disconnected\n");
    bufferevent_free(fBev);
    fNumControllers--;
  }
}

void ControllerLink::ProcessCommand(char *input)
{
  //printf("Got \"%s\"\n",input);
  if (strncmp(input,"exit",4) == 0){
    printf("penn_daq: exiting\n");
    raise(SIGINT);
  }else if(strncmp(input,"xl3_rw",6) == 0){
    pthread_t mythread;
    int ret = pthread_create(&mythread,NULL,doXL3RW,input);
  }
}
