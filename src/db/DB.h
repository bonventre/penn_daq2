#ifndef _DB_H
#define _DB_H

#include <stdint.h>

#include "Json.h"
#include "DBTypes.h"

#define DEF_DB_ADDRESS "localhost"
#define DEF_DB_PORT "5984"
#define DEF_DB_USERNAME ""
#define DEF_DB_PASSWORD ""
#define DEF_DB_BASE_NAME "penndb1"
#define DEF_DB_VIEWDOC "_design/view_doc/_view"

int GetNewID(char* newid);

int ParseFECHw(JsonNode* value,MB* mb);
int ParseFECDebug(JsonNode* value,MB* mb);
int SwapFECDB(MB* mb);
int ParseMTC(JsonNode* value,MTC* mtc);

int CreateFECDBDoc(int crate, int card, JsonNode** doc_p, JsonNode *ecal_doc);
int AddECALTestResults(JsonNode *fec_doc, JsonNode *test_doc);
int PostFECDBDoc(int crate, int slot, JsonNode *doc);
int UpdateFECDBDoc(JsonNode *doc);
int GenerateFECDocFromECAL(uint32_t crateMask, uint32_t *slotMasks, const char* id);

void SetupDebugDoc(int crateNum, int slotNum, JsonNode* doc);
int PostDebugDoc(int crate, int card, JsonNode* doc, int updateConfig=1);
int PostDebugDocWithID(int crate, int card, char *id, JsonNode* doc);
int PostECALDoc(uint32_t crateMask, uint32_t *slotMasks, char *logfile, char *id);

int UpdateLocation(uint16_t *ids, int *crates, int *slots, int *positions, int boardcount);
int RemoveFromConfig(JsonNode *config_doc, char ids[][5], int boardcount);


#endif

