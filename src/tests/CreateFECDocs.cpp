#include "Globals.h"
#include "DB.h"
#include "CreateFECDocs.h"

int CreateFECDocs(const char* ecalID){

  GenerateFECDocFromECAL(ecalID);
  
  return 0;
}

