#include <cstring>

#include "NetUtils.h"
#include "Controller.h"


Controller::Controller() : GenericCallback(CONT_PORT)
{
}

Controller::~Controller()
{
}

int Controller::fNumControllers = 0;

void Controller::AcceptCallback(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *address, int socklen)
{
  if (fNumControllers >= MAX_CONT_CON){
    printf("Too many controllers already, rejecting connection\n");
    evutil_closesocket(fd);
    return;
  }

  fNumControllers++;
  GenericCallback::AcceptCallback(listener,fd,address,socklen);
  printf("Controller connected\n");
}

void Controller::RecvCallback(struct bufferevent *bev)
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

void Controller::EventCallback(struct bufferevent *bev, short what)
{
  if (what & BEV_EVENT_CONNECTED){
  }else if (what & BEV_EVENT_ERROR){
  }else if (what & BEV_EVENT_EOF){
    printf("Controller disconnected\n");
    bufferevent_free(fBev);
    fNumControllers--;
  }
}

void Controller::ProcessCommand(char *input)
{
  printf("Got \"%s\"\n",input);
}
