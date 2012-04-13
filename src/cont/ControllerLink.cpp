#include <cstring>
#include <csignal>
#include <pthread.h>

#include "Globals.h"
#include "XL3Registers.h"

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

  pthread_t commandThread;
  int ret = pthread_create(&commandThread,NULL,ProcessCommand,input);
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

void *ControllerLink::ProcessCommand(void *arg)
{
  char *input = (char *) arg;

  if (strncmp(input,"exit",4) == 0){
    printf("penn_daq: exiting\n");
    raise(SIGINT);

  }else if (strncmp(input,"xl3_rw",6) == 0){
    if (GetFlag(input,'h')){
        printf("Usage: xl3_rw -c [crate_num (int)] "
            "-a [address (hex)] -d [data (hex)]\n"
            "Please check xl3/xl3_registers.h for the address mapping\n");
        return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    uint32_t address = GetUInt(input,'a',0x12000007);
    uint32_t data = GetUInt(input,'d',0x0);
    int busy = LockConnections(0,0x1<<crateNum);
    if (busy){
      printf("Those connections are currently in use.\n");
      return NULL;
    }
    XL3RW(crateNum,address,data);
    UnlockConnections(0,0x1<<crateNum);

  }else if (strncmp(input,"fec_test",8) == 0){
    if (GetFlag(input,'h')){
        printf("Usage: fec_test -c [crate_num (int)] "
            "-s [slot mask (hex)]\n");
        return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    uint32_t slotMask = GetUInt(input,'s',0xFFFF);
    int busy = LockConnections(0,0x1<<crateNum);
    if (busy){
      printf("Those connections are currently in use.\n");
      return NULL;
    }
    FECTest(crateNum,slotMask);
    UnlockConnections(0,0x1<<crateNum);

  }else if (strncmp(input,"crate_init",10) == 0){
    if (GetFlag(input,'h')){
      printf("Usage: crate_init -c [crate num (int)]"
          "-s [slot mask (hex)] -x (load xilinx) -X (load cald xilinx)"
          "-v (reset HV dac) -B (load vbal from db) -T (load vthr from db)"
          "-D (load tdisc from db) -C (load tcmos values from db) -A (load all from db)"
          "-H (use crate/card specific values from db)\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    int xilinxLoadNormal = GetFlag(input,'x');
    int xilinxLoadCald = GetFlag(input,'X');
    int hvReset = GetFlag(input,'v');
    int shiftRegOnly = 0;
    uint32_t slotMask = GetUInt(input,'s',0xFFFF);
    int useVBal = GetFlag(input,'B');
    int useVThr = GetFlag(input,'T');
    int useTDisc = GetFlag(input,'D');
    int useTCmos = GetFlag(input,'C');
    int useAll = GetFlag(input,'A');
    int useHw = GetFlag(input,'H');
    int xilinxLoad = 0;
    if (xilinxLoadNormal)
      xilinxLoad = 1;
    else if (xilinxLoadCald)
      xilinxLoad = 2;
    int busy = LockConnections(0,0x1<<crateNum);
    if (busy){
      printf("Those connections are currently in use.\n");
      return NULL;
    }
    CrateInit(crateNum,slotMask,xilinxLoad,hvReset,shiftRegOnly,
        useVBal,useVThr,useTDisc,useTCmos,useAll,useHw);
    UnlockConnections(0,0x1<<crateNum);

  }else if (strncmp(input,"xr",2) == 0){
  if (GetFlag(input,'h')){
      printf("Usage: xr -c [crate num (int)] -r [register number (int)]\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    int reg = GetInt(input,'c',2);
    int busy = LockConnections(0,0x1<<crateNum);
    if (busy){
      printf("Those connections are currently in use.\n");
      return NULL;
    }
    uint32_t address = xl3RegAddresses[reg] + READ_REG;
    XL3RW(crateNum,address,0x0);
    UnlockConnections(0,0x1<<crateNum);

  }else if (strncmp(input,"xw",2) == 0){
    if (GetFlag(input,'h')){
      printf("Usage: xw -c [crate num (int)] -r [register number (int)] -d [data (hex)]\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    int reg = GetInt(input,'r',0);
    uint32_t data = GetUInt(input,'d',0x0);
    int busy = LockConnections(0,0x1<<crateNum);
    if (busy){
      printf("Those connections are currently in use.\n");
      return NULL;
    }
    uint32_t address = xl3RegAddresses[reg] + WRITE_REG;
    XL3RW(crateNum,address,data);
    UnlockConnections(0,0x1<<crateNum);

  }else if (strncmp(input,"xl3_queue_rw",12) == 0){
    if (GetFlag(input,'h')){
      printf("Usage: xl3_queue_rw -c [crate num (int)] -a [address (hex)] -d [data (hex)]\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    uint32_t address = GetUInt(input,'a',0x0);
    uint32_t data = GetUInt(input,'d',0x0);
    int busy = LockConnections(0,0x1<<crateNum);
    if (busy){
      printf("Those connections are currently in use.\n");
      return NULL;
    }
    XL3QueueRW(crateNum, address, data);
    UnlockConnections(0,0x1<<crateNum);

  }else if (strncmp(input,"sm_reset",8) == 0){
    if (GetFlag(input,'h')){
      printf("Usage: sm_reset -c [crate num (int)]\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    int busy = LockConnections(0,0x1<<crateNum);
    if (busy){
      printf("Those connections are currently in use.\n");
      return NULL;
    }
    SMReset(crateNum);
    UnlockConnections(0,0x1<<crateNum);

  }else if (strncmp(input,"debugging_on",12) == 0){
    if (GetFlag(input,'h')){
      printf("Usage: debugging_on -c [crate num (int)]\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    int busy = LockConnections(0,0x1<<crateNum);
    if (busy){
      printf("Those connections are currently in use.\n");
      return NULL;
    }
    DebuggingMode(crateNum,1);
    UnlockConnections(0,0x1<<crateNum);

  }else if (strncmp(input,"debugging_off",13) == 0){
    if (GetFlag(input,'h')){
      printf("Usage: debugging_off -c [crate num (int)]\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    int busy = LockConnections(0,0x1<<crateNum);
    if (busy){
      printf("Those connections are currently in use.\n");
      return NULL;
    }
    DebuggingMode(crateNum,0);
    UnlockConnections(0,0x1<<crateNum);

  }
}
