#include "Globals.h"
#include "DacNumber.h"

#include "XL3Model.h"
#include "MTCModel.h"
#include "ControllerLink.h"
#include "AlarmTest.h"


int AlarmTest(int crateNum, uint32_t slotMask, uint32_t dacMask, int dacNum, int updateDB)
{
  lprintf("*** Starting Alarm test *****************\n");
  lprintf("Hit enter to move on or type quit to exit\n");
  const char alarmName[8][20] = {"Vcc","Vee","Vp24","Vm24","Vp8(0)","temp"};
  for (int i=0;i<6;i++){
    printf("Testing alarm for %s\n",alarmName[i]);
    printf("Testing OK\n");
    
    printf("Testing low threshold\n");
  }
      if (dacNum == -1){
        for (int j=0;j<13;j++){
          if ((0x1<<j) & dacMask){
            lprintf("Checking %s dacs\n",dacNames[j]);
            for (int k=0;k<num_dacs[j];k++){
              lprintf("%d ",k);
              fflush(stdout);
              if (SingleDacSweep(crateNum,i,dac_nums[j][k])){
                lprintf("\n****************************************\n");
                return 0;
              }
            }
            lprintf("\n");
          }
        }
      }else{
        lprintf("Checking dac %d\n",dacNum);
        SingleDacSweep(crateNum,i,dacNum);
      }
    }
  }
  lprintf("****************************************\n");
  return 0;
}

int SingleDacSweep(int crateNum, int slotNum, int dacNum)
{
  int dacValue = 0;
  int direction = 1;

  char results[1000];
  memset(results,'\0',1000);
  contConnection->GrabNextInput(results,1000,1); 
  while(true){ 
    if (contConnection->GrabNextInput(results,1000,0)){
      if (strncmp(results,"quit",4) == 0){
        return 1;
      }
      break;
    }

    xl3s[crateNum]->LoadsDac(dacNum,dacValue,slotNum);
    dacValue += 1*direction;
    if (dacValue > 255 || dacValue < 0){
      direction *= -1;
      dacValue += 2*direction;
    }
  }
  return 0;
}
