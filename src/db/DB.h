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

int PostDebugDoc(int crate, int card, JsonNode* doc);
//int post_debug_doc(int crate, int card, JsonNode* doc, fd_set *thread_fdset);
//int post_debug_doc_with_id(int crate, int card, char *id, JsonNode* doc, fd_set *thread_fdset);
//int post_debug_doc_mem_test(int crate, int card, JsonNode* doc, fd_set *thread_fdset);
//int post_ecal_doc(uint32_t crate_mask, uint16_t *slot_mask, char *logfile, char *id, fd_set *thread_fdset);

#endif

