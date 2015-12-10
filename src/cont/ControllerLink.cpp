#include <cstring>
#include <csignal>
#include <pthread.h>
#include <stdlib.h>

#include "Globals.h"
#include "XL3Registers.h"
#include "MTCRegisters.h"

#include "BoardID.h"
#include "CaldTest.h"
#include "CGTTest.h"
#include "ChinjScan.h"
#include "CrateCBal.h"
#include "DiscCheck.h"
#include "FECTest.h"
#include "FifoTest.h"
#include "GTValidTest.h"
#include "MbStabilityTest.h"
#include "MemTest.h"
#include "PedRun.h"
#include "SeeReflection.h"
#include "TriggerScan.h"
#include "TTot.h"
#include "VMon.h"
#include "LocalVMon.h"
#include "ZDisc.h"
#include "RunPedestals.h"
#include "FinalTest.h"
#include "ECAL.h"
#include "FindNoise.h"
#include "DACSweep.h"
#include "MTCCmds.h"
#include "XL3Cmds.h"
#include "NetUtils.h"
#include "ControllerLink.h"


ControllerLink::ControllerLink() : GenericLink(CONT_PORT)
{
  memset(fInput,'\0',sizeof(fInput));
  pthread_mutex_init(&fInputLock,NULL);
  pthread_cond_init(&fInputCond,NULL);
}

ControllerLink::~ControllerLink()
{
}

int ControllerLink::fNumControllers = 0;

void ControllerLink::AcceptCallback(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *address, int socklen)
{
  if (fNumControllers >= MAX_CONT_CON){
    lprintf("Too many controllers already, rejecting connection\n");
    evutil_closesocket(fd);
    return;
  }

  fNumControllers++;
  GenericLink::AcceptCallback(listener,fd,address,socklen);
  lprintf("Controller connected\n");
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


  pthread_mutex_lock(&fInputLock);
  if (fLock){
    memset(fInput,'\0',sizeof(fInput));
    memcpy(fInput,input,totalLength); 
    fLock = 0;
    pthread_cond_signal(&fInputCond);
    pthread_mutex_unlock(&fInputLock);
    return;
  }
  pthread_mutex_unlock(&fInputLock);

  pthread_t commandThread;
  int ret = pthread_create(&commandThread,NULL,ProcessCommand,input);
}

void ControllerLink::EventCallback(struct bufferevent *bev, short what)
{
  if (what & BEV_EVENT_CONNECTED){
  }else if (what & BEV_EVENT_ERROR){
  }else if (what & BEV_EVENT_EOF){
    lprintf("Controller disconnected\n");
    bufferevent_free(fBev);
    fNumControllers--;
  }
}

void ControllerLink::GetInput(char *results,int maxLength)
{
  pthread_mutex_lock(&fInputLock);
  fLock = 1;
  while (fLock == 1)
    pthread_cond_wait(&fInputCond,&fInputLock);
  if (maxLength)
    memcpy(results,fInput,maxLength);
  else
    memcpy(results,fInput,strlen(fInput));
  pthread_mutex_unlock(&fInputLock);
}

int ControllerLink::GrabNextInput(char *results, int maxLength, int setup)
{
  pthread_mutex_lock(&fInputLock);
  if (setup){
    fLock = 1;
    pthread_mutex_unlock(&fInputLock);
    return 0;
  }else{
    if (fLock == 0){
      if (maxLength)
        memcpy(results,fInput,maxLength);
      else
        memcpy(results,fInput,strlen(fInput));
      pthread_mutex_unlock(&fInputLock);
      return 1;
    }else{
      pthread_mutex_unlock(&fInputLock);
      return 0;
    }
  }
}

void *ControllerLink::ProcessCommand(void *arg)
{
  char *input = (char *) arg;

  if (strncmp(input,"exit",4) == 0){
    lprintf("penn_daq: exiting\n");
    raise(SIGINT);

  }else if (strncmp(input,"xl3_rw",6) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: xl3_rw -c [crate_num (int)] "
          "-a [address (hex)] -d [data (hex)]\n"
          "Please check xl3/xl3_registers.h for the address mapping\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    uint32_t address = GetUInt(input,'a',0x12000007);
    uint32_t data = GetUInt(input,'d',0x0);
    int busy = LockConnections(0,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("Those connections are currently in use.\n");
      return NULL;
    }
    XL3RW(crateNum,address,data);
    UnlockConnections(0,0x1<<crateNum);

  }else if (strncmp(input,"crate_init",10) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: crate_init -c [crate num (int)] "
          "-s [slot mask (hex)] -x (load xilinx) -X (load cald xilinx) "
          "-v (reset HV dac) -B (load vbal from db) -T (load vthr from find_noise db) "
          "-D (load tdisc from db) -C (load tcmos values from db) -A (load all from db) "
          "-N (load vthr from zdisc db) "
          "-e (use crate/card specific values from ECAL db) -t (enable nhit 100 and nhit 20 triggers)\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    int xilinxLoadNormal = GetFlag(input,'x');
    int xilinxLoadCald = GetFlag(input,'X');
    int hvReset = GetFlag(input,'v');
    int shiftRegOnly = GetInt(input,'S',0);
    uint32_t slotMask = GetUInt(input,'s',0xFFFF);
    int useVBal = GetFlag(input,'B');
    int useVThr = GetFlag(input,'T');
    int useTDisc = GetFlag(input,'D');
    int useTCmos = GetFlag(input,'C');
    int useAll = GetFlag(input,'A');
    int useNoise = GetFlag(input,'N');
    int useHw = GetFlag(input,'e');
    int enableTriggers = GetFlag(input,'t');
    int xilinxLoad = 0;
    if (xilinxLoadNormal)
      xilinxLoad = 1;
    else if (xilinxLoadCald)
      xilinxLoad = 2;
    int busy = LockConnections(0,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("Those connections are currently in use.\n");
      return NULL;
    }
    CrateInit(crateNum,slotMask,xilinxLoad,hvReset,shiftRegOnly,
        useVBal,useVThr,useTDisc,useTCmos,useAll,useNoise,useHw,enableTriggers);
    UnlockConnections(0,0x1<<crateNum);

  }else if (strncmp(input,"xr",2) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: xr -c [crate num (int)] -r [register number (int)]\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    int reg = GetInt(input,'c',2);
    int busy = LockConnections(0,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("Those connections are currently in use.\n");
      return NULL;
    }
    uint32_t address = xl3RegAddresses[reg] + READ_REG;
    XL3RW(crateNum,address,0x0);
    UnlockConnections(0,0x1<<crateNum);

  }else if (strncmp(input,"xw",2) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: xw -c [crate num (int)] -r [register number (int)] -d [data (hex)]\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    int reg = GetInt(input,'r',0);
    uint32_t data = GetUInt(input,'d',0x0);
    int busy = LockConnections(0,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("Those connections are currently in use.\n");
      return NULL;
    }
    uint32_t address = xl3RegAddresses[reg] + WRITE_REG;
    XL3RW(crateNum,address,data);
    UnlockConnections(0,0x1<<crateNum);

  }else if (strncmp(input,"xl3_queue_rw",12) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: xl3_queue_rw -c [crate num (int)] -a [address (hex)] -d [data (hex)]\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    uint32_t address = GetUInt(input,'a',0x0);
    uint32_t data = GetUInt(input,'d',0x0);
    int busy = LockConnections(0,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("Those connections are currently in use.\n");
      return NULL;
    }
    XL3QueueRW(crateNum, address, data);
    UnlockConnections(0,0x1<<crateNum);

  }else if (strncmp(input,"sm_reset",8) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: sm_reset -c [crate num (int)]\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    int busy = LockConnections(0,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("Those connections are currently in use.\n");
      return NULL;
    }
    SMReset(crateNum);
    UnlockConnections(0,0x1<<crateNum);

  }else if (strncmp(input,"debugging_on",12) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: debugging_on -c [crate num (int)]\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    int busy = LockConnections(0,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("Those connections are currently in use.\n");
      return NULL;
    }
    DebuggingMode(crateNum,1);
    UnlockConnections(0,0x1<<crateNum);

  }else if (strncmp(input,"debugging_off",13) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: debugging_off -c [crate num (int)]\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    int busy = LockConnections(0,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("Those connections are currently in use.\n");
      return NULL;
    }
    DebuggingMode(crateNum,0);
    UnlockConnections(0,0x1<<crateNum);

  }else if (strncmp(input,"change_mode",11) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: change_mode -c [crate num (int)] -n (normal mode)"
          "-s [slot mask (hex)]\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    int mode = GetFlag(input,'n');
    uint32_t dataAvailMask = GetUInt(input,'s',0xFFFF);
    int busy = LockConnections(0,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("Those connections are currently in use.\n");
      return NULL;
    }
    ChangeMode(crateNum,mode,dataAvailMask);
    UnlockConnections(0,0x1<<crateNum);

  }else if (strncmp(input,"check_xl3_status",16) == 0){
    int crateNum = GetInt(input,'c',2);
    int busy = LockConnections(0,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("Those connections are currently in use.\n");
      return NULL;
    }
    CheckXL3Status(crateNum);
    UnlockConnections(0,0x1<<crateNum);

  }else if (strncmp(input,"read_local_voltage",18) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: read_local_voltage -c [crate num (int)] -v [voltage select]\n"
          "0 - VCC\n1 - VEE\n2 - VP8\n3 - V24P\n4 - V24M\n5,6,7 - temperature monitors\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    int voltage = GetInt(input,'v',0);
    int busy = LockConnections(0,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("Those connections are currently in use.\n");
      return NULL;
    }
    ReadLocalVoltage(crateNum,voltage);
    UnlockConnections(0,0x1<<crateNum);

  }else if (strncmp(input,"hv_readback",11) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: hv_readback -c [crate num (int)]\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    int busy = LockConnections(0,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("Those connections are currently in use.\n");
      return NULL;
    }
    HVReadback(crateNum);
    UnlockConnections(0,0x1<<crateNum);

  }else if (strncmp(input,"set_alarm_level",15) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: set_alarm_level -c [crate num (int)] -l/L [lower limit (float/int dac value)] -u/U [upper limit (float/int dac value)]  -a [select alarm]\n"
          "0: Vcc\n1:Vee\n2:Vp24\n3:Vm24\n4:Vp8\n5:Temp\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    float lowAlarm = GetFloat(input,'l',-999.0);
    float highAlarm = GetFloat(input,'u',-999.0);
    uint32_t lowDac = (uint32_t) GetInt(input,'L',0xFFFFFFFF);
    uint32_t highDac = (uint32_t) GetInt(input,'U',0xFFFFFFFF);
    int alarm = GetInt(input,'a',0);
    int busy = LockConnections(0,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("Those connections are currently in use.\n");
      return NULL;
    }
    SetAlarmLevel(crateNum,lowAlarm,highAlarm,lowDac,highDac,alarm);
    UnlockConnections(0,0x1<<crateNum);

  }else if (strncmp(input,"set_alarm_dac",13) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: set_alarm_dac -c [crate num (int)] -0 [dac 0 setting (hex)] -1 [dac 1] -2 [dac 2]\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    uint32_t dacs[3];
    dacs[0] = GetUInt(input,'0',0xFFFFFFFF);
    dacs[1] = GetUInt(input,'1',0xFFFFFFFF);
    dacs[2] = GetUInt(input,'2',0xFFFFFFFF);
    int busy = LockConnections(0,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("Those connections are currently in use.\n");
      return NULL;
    }
    SetAlarmDac(crateNum,dacs);
    UnlockConnections(0,0x1<<crateNum);

  }else if (strncmp(input,"fr",2) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: fr -c [crate num (int)] -s [slot num (int)] -r [register num (int)]\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    int slotNum = GetInt(input,'s',13);
    int reg = GetInt(input,'c',2);
    int busy = LockConnections(0,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("Those connections are currently in use.\n");
      return NULL;
    }
    uint32_t address = fecRegAddresses[reg] + READ_REG + FEC_SEL*slotNum;
    XL3RW(crateNum,address,0x0);
    UnlockConnections(0,0x1<<crateNum);

  }else if (strncmp(input,"fw",2) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: fw -c [crate num (int)] -s [slot num (int)] -r [register num (int)] -d [data (hex)]\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    int slotNum = GetInt(input,'s',13);
    int reg = GetInt(input,'r',0);
    uint32_t data = GetUInt(input,'d',0x0);
    int busy = LockConnections(0,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("Those connections are currently in use.\n");
      return NULL;
    }
    uint32_t address = fecRegAddresses[reg] + WRITE_REG + FEC_SEL*slotNum;
    XL3RW(crateNum,address,data);
    UnlockConnections(0,0x1<<crateNum);

  }else if (strncmp(input,"load_relays",11) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: load_relays -c [crate num (int)] -p [set pattern for all slots (hex)] -(00-15) [set pattern for slot (hex)]\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    uint32_t patterns[16];
    GetMultiUInt(input,16,'p',patterns,0xF);
    int busy = LockConnections(0,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("Those connections are currently in use.\n");
      return NULL;
    }
    LoadRelays(crateNum,patterns);
    UnlockConnections(0,0x1<<crateNum);

  }else if (strncmp(input,"read_bundle",11) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: read_bundle -c [crate num (int)] -s [slot num (int)] -q (quiet mode)\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    int quiet = GetFlag(input,'q');
    int slotNum = GetInt(input,'s',13);
    int busy = LockConnections(0,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("Those connections are currently in use.\n");
      return NULL;
    }
    ReadBundle(crateNum,slotNum,quiet);
    UnlockConnections(0,0x1<<crateNum);

  }else if (strncmp(input,"setup_chinj",11) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: setup_chinj -c [crate num (int)] "
          "-s [slot mask (hex)] -p [channel mask (hex)] -d [dac value (int)]\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    uint32_t slotMask = GetUInt(input,'s',0xFFFF);
    uint32_t chanMask = GetUInt(input,'p',0xFFFFFFFF);
    int dacValue = GetInt(input,'d',255);
    int busy = LockConnections(0,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("Those connections are currently in use.\n");
      return NULL;
    }
    xl3s[crateNum]->SetupChargeInjection(slotMask,chanMask,dacValue);
    UnlockConnections(0,0x1<<crateNum);

  }else if (strncmp(input,"load_dac",8) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: load_dac -c [crate num (int)] "
          "-s [slot num (int)] -d [dac num (int)] -v [dac value (int)]\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    int slotNum = GetInt(input,'s',13);
    int dacNum = GetInt(input,'d',0);
    int dacValue = GetInt(input,'v',255);
    int busy = LockConnections(0,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("Those connections are currently in use.\n");
      return NULL;
    }
    xl3s[crateNum]->LoadsDac(dacNum,dacValue,slotNum);
    printf("loaded dac %d\n",dacNum);
    UnlockConnections(0,0x1<<crateNum);

  }else if (strncmp(input,"sbc_control",11) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: sbc_control -c (connect) | -k (kill) | -r (reconnect) "
          "-i [identity file] -m (start orcareadout manually)\n");
      return NULL;
    }
    int connect = GetFlag(input,'c');
    int kill = GetFlag(input,'k');
    int reconnect = GetFlag(input,'r');
    if (!connect && !kill && !reconnect)
      reconnect = 1;
    if (reconnect){
      connect = 1;
      kill = 1;
    }
    int manualStart = GetFlag(input,'m');
    char idFile[1000];
    GetString(input,idFile,'i',DEFAULT_SSHKEY);
    int busy = LockConnections(2,0x0);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("Those connections are currently in use.\n");
      return NULL;
    }
    SBCControl(connect,kill,manualStart,idFile);
    UnlockConnections(1,0x0);

  }else if (strncmp(input,"mtc_init",8) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: mtc_init -x (load xilinx)\n");
      return NULL;
    }
    int xilinx = GetFlag(input,'x');
    int busy = LockConnections(1,0x0);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("Those connections are currently in use.\n");
      return NULL;
    }
    MTCInit(xilinx);
    UnlockConnections(1,0x0);

  }else if (strncmp(input,"mr",2) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: mr -r [register number (int)]\n");
      lprintf("type \"help mtc_registers\" to get "
          "a list of registers with numbers and descriptions\n");
      return NULL;
    }
    int reg = GetInt(input,'r',0);
    int busy = LockConnections(1,0x0);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    uint32_t address = mtcRegAddresses[reg];
    MTCRead(address);
    UnlockConnections(1,0x0);

  }else if (strncmp(input,"mw",2) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: mw -r [register number (int)] -d [data (hex)]\n");
      lprintf("type \"help mtc_registers\" to get "
          "a list of registers with numbers and descriptions\n");
      return NULL;
    }
    int reg = GetInt(input,'r',0);
    uint32_t data = GetUInt(input,'d',0x0);
    int busy = LockConnections(1,0x0);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    uint32_t address = mtcRegAddresses[reg];
    MTCWrite(address,data);
    UnlockConnections(1,0x0);

  }else if (strncmp(input,"mtc_read",8) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: mtc_read -a [address (hex)]\n");
      return NULL;
    }
    int address = GetUInt(input,'a',0x0);
    int busy = LockConnections(1,0x0);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    MTCRead(address);
    UnlockConnections(1,0x0);

  }else if (strncmp(input,"mtc_write",9) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: mtc_write -a [address (hex)] -d [data (hex)]\n");
      return NULL;
    }
    uint32_t address = GetUInt(input,'a',0x0);
    uint32_t data = GetUInt(input,'d',0x0);
    int busy = LockConnections(1,0x0);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    MTCWrite(address,data);
    UnlockConnections(1,0x0);
  }else if (strncmp(input,"mtc_delay",9) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: mtc_delay -t [delay time in ns (float)]\n");
      return NULL;
    }
    float delaytime = GetFloat(input,'t',200);
    int busy = LockConnections(1,0x0);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    MTCDelay(delaytime);
    UnlockConnections(1,0x0);
  }else if (strncmp(input,"set_mtca_thresholds",19) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: set_mtca_thresholds -(00-13) [voltage in millivolts (float)] -v [set all voltages (float)]\n");
      return NULL;
    }

    float voltages[14];
    GetMultiFloat(input,14,'v',voltages,-4.9);
    int busy = LockConnections(1,0x0);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    mtc->LoadMTCADacs(voltages);
    lprintf("Finished loading MTCA dacs\n");
    UnlockConnections(1,0x0);

  }else if (strncmp(input,"set_gt_mask",11) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: set_gt_mask -m [mask (hex)] -o ('or' with current mask)\n");
      return NULL;
    }

    uint32_t mask = GetUInt(input,'m',0x0);
    int ored = GetFlag(input,'o');
    int busy = LockConnections(1,0x0);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    if (ored){
      mtc->SetGTMask(mask);
    }else{
      mtc->UnsetGTMask(0xFFFFFFFF);
      mtc->SetGTMask(mask);
    }
    lprintf("Set GT mask\n");
    UnlockConnections(1,0x0);

  }else if (strncmp(input,"set_gt_crate_mask",17) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: set_gt_crate_mask -m [mask (hex)] -o ('or' with current mask)\n");
      return NULL;
    }

    uint32_t mask = GetUInt(input,'m',0x0);
    int ored = GetFlag(input,'o');
    int busy = LockConnections(1,0x0);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    if (ored){
      mtc->SetGTCrateMask(mask);
    }else{
      mtc->UnsetGTCrateMask(0xFFFFFFFF);
      mtc->SetGTCrateMask(mask);
    }
    lprintf("Set GT crate Mask\n");
    UnlockConnections(1,0x0);

  }else if (strncmp(input,"set_ped_crate_mask",18) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: set_ped_crate_mask -m [mask (hex)] -o ('or' with current mask)\n");
      return NULL;
    }

    uint32_t mask = GetUInt(input,'m',0x0);
    int ored = GetFlag(input,'o');
    int busy = LockConnections(1,0x0);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    if (ored){
      mtc->SetPedCrateMask(mask);
    }else{
      mtc->UnsetPedCrateMask(0xFFFFFFFF);
      mtc->SetPedCrateMask(mask);
    }
    lprintf("Set Ped crate mask\n");
    UnlockConnections(1,0x0);

  }else if (strncmp(input,"enable_pulser",13) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: enable_pulser\n");
      return NULL;
    }
    int busy = LockConnections(1,0x0);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    mtc->EnablePulser();
    lprintf("Pulser enabled\n");
    UnlockConnections(1,0x0);

  }else if (strncmp(input,"disable_pulser",14) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: disable_pulser\n");
      return NULL;
    }
    int busy = LockConnections(1,0x0);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    mtc->DisablePulser();
    lprintf("Pulser disabled\n");
    UnlockConnections(1,0x0);

  }else if (strncmp(input,"enable_pedestal",15) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: enable_pedestal\n");
      return NULL;
    }
    int busy = LockConnections(1,0x0);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    mtc->EnablePedestal();
    lprintf("Pedestals enabled\n");
    UnlockConnections(1,0x0);

  }else if (strncmp(input,"disable_pedestal",16) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: disable_pedestal\n");
      return NULL;
    }
    int busy = LockConnections(1,0x0);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    mtc->DisablePedestal();
    lprintf("Pedestals disabled\n");
    UnlockConnections(1,0x0);

  }else if (strncmp(input,"set_pulser_freq",15) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: set_pulser_freq -f [frequency (float)]\n");
      return NULL;
    }
    float freq = GetFloat(input,'f',1);
    int busy = LockConnections(1,0x0);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    mtc->SetPulserFrequency(freq);
    lprintf("Pulser frequency set\n");
    UnlockConnections(1,0x0);

  }else if (strncmp(input,"send_softgt",11) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: send_softgt\n");
      return NULL;
    }
    int busy = LockConnections(1,0x0);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    mtc->SoftGT();
    lprintf("Soft gt sent\n");
    UnlockConnections(1,0x0);

  }else if (strncmp(input,"multi_softgt",12) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: multi_softgt -n [number of pulses (int)]\n");
      return NULL;
    }
    int num = GetInt(input,'n',10);
    int busy = LockConnections(1,0x0);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    mtc->MultiSoftGT(num);
    lprintf("Multi Soft gt sent\n");
    UnlockConnections(1,0x0);

  }else if (strncmp(input,"board_id",8) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: board_id -c [crate_num (int)] "
          "-s [slot mask (hex)] -l (update location in database)\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    int slotMask = GetUInt(input,'s',0xFFFF);
    int updateLocation = GetFlag(input,'l');
    int busy = LockConnections(0,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    BoardID(crateNum,slotMask,updateLocation);
    UnlockConnections(0,0x1<<crateNum);

  }else if (strncmp(input,"cald_test",9) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: cald_test -c [crate_num (int)] "
          "-s [slot mask (hex)] -u [upper limit] -l [lower limit] -n [num points to sample] -p [samples per point] -d (update database) \n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    uint32_t slotMask = GetUInt(input,'s',0xFFFF);
    int upper = GetInt(input,'u',3550);
    int lower = GetInt(input,'l',3000);
    int num = GetInt(input,'n',550);
    int samples = GetInt(input,'p',1);
    int update = GetFlag(input,'d');
    int busy = LockConnections(0,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    CaldTest(crateNum,slotMask,upper,lower,num,samples,update);
    UnlockConnections(0,0x1<<crateNum);

  }else if (strncmp(input,"cgt_test",8) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: cgt_test -c [crate_num (int)] "
          "-s [slot mask (hex)] -p [channel mask (hex)] -d (update database) \n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    uint32_t slotMask = GetUInt(input,'s',0xFFFF);
    uint32_t channelMask = GetUInt(input,'p',0xFFFFFFFF);
    int update = GetFlag(input,'d');
    int busy = LockConnections(1,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    CGTTest(crateNum,slotMask,channelMask,update);
    UnlockConnections(1,0x1<<crateNum);

  }else if (strncmp(input,"chinj_scan",10) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: chinj_scan -c [crate num (int)] "
          "-s [slot mask (hex)] -p [channel mask (hex)] "
          "-f [frequency] -t [gtdelay] -w [ped with] -n [num pedestals] "
          "-l [charge lower limit] -u [charge upper limit] "
          "-a [charge select (0=qhl,1=qhs,2=qlx,3=tac)] "
          "-e (enable pedestal) -d (update database)\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    uint32_t slotMask = GetUInt(input,'s',0xFFFF);
    uint32_t channelMask = GetUInt(input,'p',0xFFFFFFFF);
    float freq = GetFloat(input,'f',0);
    int gtDelay = GetInt(input,'t',DEFAULT_GT_DELAY);
    int pedWidth = GetInt(input,'w',DEFAULT_PED_WIDTH);
    int numPeds = GetInt(input,'n',1);
    float lower = GetFloat(input,'l',0);
    float upper = GetFloat(input,'u',5000);
    int qSelect = GetInt(input,'a',0);
    int pedOn = GetFlag(input,'e');
    int update = GetFlag(input,'d');
    int busy = LockConnections(1,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    ChinjScan(crateNum,slotMask,channelMask,freq,gtDelay,pedWidth,numPeds,upper,lower,qSelect,pedOn,update);
    UnlockConnections(1,0x1<<crateNum);

  }else if (strncmp(input,"crate_cbal",10) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: crate_cbal -c [crate num (int)] "
          "-s [slot mask (hex)] -p [channel mask (hex)] -d (update database)\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    uint32_t slotMask = GetUInt(input,'s',0xFFFF);
    uint32_t channelMask = GetUInt(input,'p',0xFFFFFFFF);
    int update = GetFlag(input,'d');
    int busy = LockConnections(1,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    CrateCBal(crateNum,slotMask,channelMask,update);
    UnlockConnections(1,0x1<<crateNum);

  }else if (strncmp(input,"disc_check",10) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: disc_check -c [crate num (int)] "
          "-s [slot mask (hex)] -n [num pedestals] -d (update database)\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    uint32_t slotMask = GetUInt(input,'s',0xFFFF);
    int numPeds = GetInt(input,'n',100000);
    int update = GetFlag(input,'d');
    int busy = LockConnections(1,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    DiscCheck(crateNum,slotMask,numPeds,update);
    UnlockConnections(1,0x1<<crateNum);

  }else if (strncmp(input,"fec_test",8) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: fec_test -c [crate_num (int)] "
          "-s [slot mask (hex)] -d (update database)\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    int update = GetFlag(input,'d');
    uint32_t slotMask = GetUInt(input,'s',0xFFFF);
    int busy = LockConnections(0,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    FECTest(crateNum,slotMask,update);
    UnlockConnections(0,0x1<<crateNum);

  }else if (strncmp(input,"fifo_test",9) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: fifo_test -c [crate num (int)] "
          "-s [slot mask (hex)] -d (update database)\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    uint32_t slotMask = GetUInt(input,'s',0xFFFF);
    int update = GetFlag(input,'d');
    int busy = LockConnections(1,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    FifoTest(crateNum,slotMask,update);
    UnlockConnections(1,0x1<<crateNum);

  }else if (strncmp(input,"gtvalid_test",12) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: gtvalid_test -c [crate num (int)] "
          "-s [slot mask (hex)] -p [channel mask (hex)] "
          "-g [gt cutoff] -t (use twiddle bits) -d (update database)\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    uint32_t slotMask = GetUInt(input,'s',0xFFFF);
    uint32_t channelMask = GetUInt(input,'p',0xFFFFFFFF);
    float gtCutoff = GetFloat(input,'g',0);
    int twiddleOn = GetFlag(input,'t');
    int update = GetFlag(input,'d');
    int busy = LockConnections(1,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    GTValidTest(crateNum,slotMask,channelMask,gtCutoff,twiddleOn,update);
    UnlockConnections(1,0x1<<crateNum);

  }else if (strncmp(input,"mb_stability_test",17) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: mb_stability_test -c [crate num (int)] "
          "-s [slot mask (hex)] -n [num pedestals (int)] -d (update database)\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    uint32_t slotMask = GetUInt(input,'s',0xFFFF);
    int numPeds = GetInt(input,'n',50);
    int update = GetFlag(input,'d');
    int busy = LockConnections(1,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    MbStabilityTest(crateNum,slotMask,numPeds,update);
    UnlockConnections(1,0x1<<crateNum);

  }else if (strncmp(input,"mem_test",8) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: mem_test -c [crate_num (int)] "
          "-s [slot num (int)] -d (update database)\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    int slotNum = GetInt(input,'s',13);
    int update = GetFlag(input,'d');
    int busy = LockConnections(0,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    MemTest(crateNum,slotNum,update);
    UnlockConnections(0,0x1<<crateNum);

  }else if (strncmp(input,"ped_run",7) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: ped_run -c [crate num (int)] "
          "-s [slot mask (hex)] -p [channel mask (hex)] "
          "-l [lower Q ped check value] -u [upper Q ped check value] "
          "-f [pulser frequency (0 for softgts)] -n [number of pedestals per cell] "
          "-t [gt delay] -w [pedestal width] -d (update database)\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    uint32_t slotMask = GetUInt(input,'s',0xFFFF);
    uint32_t channelMask = GetUInt(input,'p',0xFFFFFFFF);
    int lower = GetInt(input,'l',400);
    int upper = GetInt(input,'u',700);
    float frequency = GetFloat(input,'f',0);
    int numPeds = GetInt(input,'n',50);
    int gtDelay = GetInt(input,'t',DEFAULT_GT_DELAY);
    int pedWidth = GetInt(input,'w',DEFAULT_PED_WIDTH);
    int update = GetFlag(input,'d');
    int busy = LockConnections(1,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    PedRun(crateNum,slotMask,channelMask,frequency,gtDelay,pedWidth,numPeds,upper,lower,update);
    UnlockConnections(1,0x1<<crateNum);

  }else if (strncmp(input,"see_refl",8) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: see_refl -c [crate num (int)] "
          "-v [dac value (int)] -s [slot mask (hex)] "
          "-f [frequency (float)] -p [channel mask (hex)] "
          "-d (update database)\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    uint32_t slotMask = GetUInt(input,'s',0xFFFF);
    uint32_t channelMask = GetUInt(input,'p',0xFFFFFFFF);
    int dacValue = GetInt(input,'v',255);
    float frequency = GetFloat(input,'f',0);
    int update = GetFlag(input,'d');
    int busy = LockConnections(1,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    SeeReflection(crateNum,slotMask,channelMask,dacValue,frequency,update);
    UnlockConnections(1,0x1<<crateNum);

  }else if (strncmp(input,"trigger_scan",12) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: trigger_scan -c [crate mask (hex)] "
          "-t [trigger to enable (0-13)] -s [slot mask for all crates (hex)] "
          "-00..-18 [slot mask for crate 0-18 (hex)] -f [output file name] "
          "-n [max nhit to scan to (int)] -m [min adc count thresh to scan down to (int)] "
          "-d [threshold dac to program (by default the one you are triggering on)] "
          "-q (quick mode - samples every 10th dac count)\n");
      return NULL;
    }
    uint32_t crateMask = GetUInt(input,'c',0x4);
    uint32_t slotMasks[19];
    GetMultiUInt(input,19,'s',slotMasks,0xFFFF);
    int nhitMax = GetInt(input,'n',0);
    int threshMin = GetInt(input,'m',0);
    int triggerSelect = GetInt(input,'t',0);
    int dacSelect = GetInt(input,'d',-1);
    int quick = GetFlag(input,'q');
    char fileName[1000];
    GetString(input,fileName,'f',"data/triggerscan.dat");
    int busy = LockConnections(1,crateMask);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    TriggerScan(crateMask,slotMasks,triggerSelect,dacSelect,nhitMax,threshMin,fileName,quick);
    UnlockConnections(1,crateMask);

  }else if (strncmp(input,"get_ttot",8) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: get_ttot -c [crate num (int)] "
          "-s [slot mask (hex)] -t [target time] -d (update database)\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    uint32_t slotMask = GetUInt(input,'s',0xFFFF);
    int targetTime = GetInt(input,'t',400);
    int update = GetFlag(input,'d');
    int busy = LockConnections(1,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    GetTTot(crateNum,slotMask,targetTime,update);
    UnlockConnections(1,0x1<<crateNum);

  }else if (strncmp(input,"set_ttot",8) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: set_ttot -c [crate num (int)] "
          "-s [slot mask (hex)] -t [target time] -d (update database)\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    uint32_t slotMask = GetUInt(input,'s',0xFFFF);
    int targetTime = GetInt(input,'t',400);
    int update = GetFlag(input,'d');
    int busy = LockConnections(1,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    SetTTot(crateNum,slotMask,targetTime,update);
    UnlockConnections(1,0x1<<crateNum);

  }else if (strncmp(input,"vmon",4) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: vmon -c [crate num (int)]"
          "-s [slot mask (hex)] -d (update database)\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    uint32_t slotMask = GetUInt(input,'s',0xFFFF);
    int update = GetFlag(input,'d');
    int busy = LockConnections(0,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    VMon(crateNum,slotMask,update);
    UnlockConnections(0,0x1<<crateNum);

  }else if (strncmp(input,"local_vmon",10) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: local_vmon -c [crate num (int)]\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    int busy = LockConnections(0,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    LocalVMon(crateNum);
    UnlockConnections(0,0x1<<crateNum);


  }else if (strncmp(input,"zdisc",5) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: zdisc -c [crate num (int)] "
          "-s [slot mask (hex)] -o [offset] -r [rate] "
          "-d (update database)\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    uint32_t slotMask = GetUInt(input,'s',0xFFFF);
    int offset = GetInt(input,'o',0);
    float rate = GetFloat(input,'r',10000);
    int update = GetFlag(input,'d');
    int busy = LockConnections(0,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    ZDisc(crateNum,slotMask,rate,offset,update);
    UnlockConnections(0,0x1<<crateNum);

  }else if (strncmp(input,"run_pedestals_end",17) == 0){
    int crate = 1;
    int mtc = 1;
    if (strncmp(input,"run_pedestals_end_mtc",21)==0)
      crate = 0;
    if (strncmp(input,"run_pedestals_end_crate",23)==0)
      mtc = 0;
    if (GetFlag(input,'h')){
      if (crate && !mtc)
        lprintf("Usage: run_pedestals_end_crate -c [crate mask (hex)]\n");
      if (mtc && !crate)
        lprintf("Usage: run_pedestals_end_mtc\n");
      if (crate && mtc)
        lprintf("Usage: run_pedestals_end -c [crate mask (hex)]\n");
      return NULL;
    }
    uint32_t crateMask = GetUInt(input,'c',0x4);
    int busy = LockConnections(mtc,crateMask*crate);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    RunPedestalsEnd(crateMask,crate,mtc);
    UnlockConnections(mtc,crateMask*crate);

  }else if (strncmp(input,"run_pedestals",13) == 0){
    int crate = 1;
    int mtc = 1;
    if (strncmp(input,"run_pedestals_mtc",17)==0)
      crate = 0;
    if (strncmp(input,"run_pedestals_crate",19)==0)
      mtc = 0;
    if (GetFlag(input,'h')){
      if (crate && !mtc)
        lprintf("Usage: run_pedestals_crate ");
      if (mtc && !crate)
        lprintf("Usage: run_pedestals_mtc ");
      if (crate && mtc)
        lprintf("Usage: run_pedestals ");
      if (crate)
        lprintf("-c [crate mask (hex)] -s [all slot masks (hex)] -(00-18) [one slot mask (hex)] -p [channel mask (hex)] ");
      if (mtc)
        lprintf("-f [frequency (float)] -t [gt delay] -w [ped width]");
      lprintf("\n");
      return NULL;
    }
    uint32_t crateMask = GetUInt(input,'c',0x4);
    uint32_t slotMasks[19];
    GetMultiUInt(input,19,'s',slotMasks,0xFFFF);
    uint32_t channelMask = GetUInt(input,'p',0xFFFFFFFF);
    float frequency = GetFloat(input,'f',1000.0);
    int gtDelay = GetInt(input,'t',DEFAULT_GT_DELAY);
    int pedWidth = GetInt(input,'w',DEFAULT_PED_WIDTH);

    int busy = LockConnections(mtc,crateMask*crate);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    RunPedestals(crateMask,slotMasks,channelMask,frequency,gtDelay,pedWidth,crate,mtc);
    UnlockConnections(mtc,crateMask*crate);

  }else if (strncmp(input,"final_test",10) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: final_test -c [crate num (int)] -s [slot mask (hex)] -q (skip text input) -t [test mask if you want only a subset (hex)]\n");
      lprintf("For test mask, the bit map is: \n");
      lprintf("0: fec_test, 1: vmon, 2: cgt_test, 3: ped_run\n");
      lprintf("4: crate_cbal, 5: ped_run 6: chinj_scan, 7: set_ttot\n");
      lprintf("8: get_ttot, 9: disc_check, 10: gtvalid_test, 11: zdisc\n");
      lprintf("12: mb_stability_test, 13: fifo_test, 14: cald_test, 15: mem_test\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    uint32_t slotMask = GetUInt(input,'s',0xFFFF);
    uint32_t testMask = GetUInt(input,'t',0xFFFFFFFF);
    int skip = GetFlag(input,'q');
    int busy = LockConnections(1,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    FinalTest(crateNum,slotMask,testMask,skip);
    UnlockConnections(1,0x1<<crateNum);

  }else if (strncmp(input,"ecal",4) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: ecal -c [crate mask (hex)] -s [all slot masks (hex)] -(00-18) [one slot mask (hex)]\n");
      lprintf("-d (create FEC db docs)\n");
      lprintf("-l [ecal id to update / finish tests (string)] -t [test mask to update / finish (hex)]\n");
      lprintf("For test mask, the bit map is: \n");
      lprintf("0: fec_test, 1: board_id, 2: cgt_test, 3: crate_cbal\n");
      lprintf("4: ped_run, 5: set_ttot, 6: get_ttot, 7: disc_check\n");
      lprintf("8: gtvalid_test, 9: zdisc, 10: find_noise\n");
      return NULL;
    }
    uint32_t crateMask = GetUInt(input,'c',0x4);
    uint32_t slotMasks[19];
    GetMultiUInt(input,19,'s',slotMasks,0xFFFF);
    uint32_t testMask = GetUInt(input,'t',0xFFFFFFFF);
    char loadECAL[500];
    memset(loadECAL,'\0',sizeof(loadECAL));
    GetString(input,loadECAL,'l',"");
    int createDocs = GetFlag(input,'d');
    int busy = LockConnections(1,crateMask);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    ECAL(crateMask,slotMasks,testMask,loadECAL,createDocs);
    UnlockConnections(1,crateMask);

  }else if (strncmp(input,"find_noise",10) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: find_noise -c [crate mask (hex)] -(00-18) [slot masks (hex)] -s [all slot masks (hex)] -d (update database)\n");
      return NULL;
    }
    uint32_t crateMask = GetUInt(input,'c',0x4);
    uint32_t slotMasks[19];
    GetMultiUInt(input,19,'s',slotMasks,0xFFFF);
    int updateDB = GetFlag(input,'d');
    int busy = LockConnections(1,crateMask);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    FindNoise(crateMask,slotMasks,20,1,updateDB);
    UnlockConnections(1,crateMask);

  }else if (strncmp(input,"dac_sweep",9) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: dac_sweep -c [crate number (int)] -s [slot mask (hex)] -m [dac mask (hex)] -n [to pic a specific dac]\n");
      return NULL;
    }
    int crateNum = GetInt(input,'c',2);
    uint32_t slotMask = GetUInt(input,'s',0xFFFF);
    uint32_t dacMask = GetUInt(input,'m',0xFFFFFFFF);
    int updateDB = GetFlag(input,'d');
    int dacNum = GetInt(input,'n',-1);
    int busy = LockConnections(0,0x1<<crateNum);
    if (busy){
      if (busy > 9)
        lprintf("Trying to access a board that has not been connected\n");
      else
        lprintf("ThoseConnections are currently in use.\n");
      return NULL;
    }
    DACSweep(crateNum,slotMask,dacMask,dacNum,updateDB);
    UnlockConnections(0,0x1<<crateNum);

  }else if (strncmp(input,"check_recv_queues",17) == 0){
    int empty = GetFlag(input,'e');
    if (LockConnections(1,0x0)){
      int result = mtc->CheckQueue(empty);
      if (result == -1)
        lprintf("Fixed mtc queue\n");
      else if (result == -2)
        lprintf("MTC queue not empty!\n");
      UnlockConnections(1,0x0);
    }
    for (int i=0;i<19;i++){
      if (LockConnections(0,(0x1<<i))){
        int result = xl3s[i]->CheckQueue(empty);
        if (result == -1)
          lprintf("Fixed xl3 %d queue\n",i);
        else if (result == -2)
          lprintf("xl3 %d queue not empty!\n",i);
        UnlockConnections(0,(0x1<<i));
      }
    }

  }else if (strncmp(input,"run_macro",9) == 0){
    if (GetFlag(input,'h')){
      lprintf("Usage: run_macro -f [file name]\n");
      return NULL;
    }
    char fileName[1000];
    GetString(input,fileName,'f',"");
    lprintf("Starting macro\n");
    ParseMacro(fileName);
    lprintf("Macro finished.\n");

  }else if (strncmp(input,"reset_speed",11) == 0){
    megaBundleCount = 0;

  }else if (strncmp(input,"help",4) == 0){
    PrintHelp(input);

  }else if (strncmp(input,"clear_screen",12) == 0){
    system("clear");

  }else if (strncmp(input,"start_logging",13) == 0){
    StartLogging();

  }else if (strncmp(input,"stop_logging",12) == 0){
    StopLogging();

  }else if (strncmp(input,"print_connected",15) == 0){
    lprintf("CONNECTED CLIENTS:\n");
    if (contConnection->IsConnected())
      lprintf("Controller\n");
    if (mtc->IsConnected())
      lprintf("SBC\n");
    for (int i=0;i<19;i++){
      if (xl3s[i]->IsConnected())
        lprintf("XL3 #%d\n",i);
    }
  }
}

