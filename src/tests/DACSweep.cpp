#include "Globals.h"
#include "DacNumber.h"

#include "XL3Model.h"
#include "MTCModel.h"
#include "ControllerLink.h"
#include "DACSweep.h"


int DACSweep(int crateNum, uint32_t slotMask, uint32_t dacMask, int dacNum, int updateDB)
{
  printf("*** Starting DAC Sweep *****************\n");
  printf("Hit enter to move on to the next dac or type quit to exit\n");
  const unsigned short *dac_nums[13] = {d_rmp,d_vli,d_vsi,d_vthr,d_vbal_hgain,d_vbal_lgain,d_rmpup,d_iseta,d_isetm,&d_tacref,&d_vmax,&d_vres,&d_hvref};
  int num_dacs[13] = {8,8,8,32,32,32,8,2,2,1,1,1,1};  

  for (int i=0;i<16;i++){
    if ((0x1<<i) & slotMask){
      if (dacNum == -1){
        for (int j=0;j<13;j++){
          if ((0x1<<j) & dacMask){
            printf("Checking %s dacs\n",dacNames[j]);
            for (int k=0;k<num_dacs[j];k++){
              printf("%d ",k);
              fflush(stdout);
              if (SingleDacSweep(crateNum,i,dac_nums[j][k])){
                printf("\n****************************************\n");
                return 0;
              }
            }
            printf("\n");
          }
        }
      }else{
        printf("Checking dac %d\n",dacNum);
        SingleDacSweep(crateNum,i,dacNum);
      }
    }
  }
  printf("****************************************\n");
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
