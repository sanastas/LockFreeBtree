#ifndef _CHUNK_ALGORITHM_H_FILE
#define _CHUNK_ALGORITHM_H_FILE

#include "entry.h"
#include "chunk.h"
#include "globals.h"
#include "treeAlgorithm.h"

typedef enum {EXISTED, SUCCESS_THIS, SUCCESS_OTHER, SUCCESS_DELAYED_INSERT} ReturnCode;
typedef enum {INSERT, DELETE, NONE, SWAP, ENSLAVE} FreezeTriggerType;

/*****************************************************************************
	Chunk Algorithm
*****************************************************************************/
Bool searchInChunk(Btree* btree, ThreadGlobals* tg, Chunk* chunk, int key, Data *data);
Bool insertToChunk(Btree* btree, ThreadGlobals* tg, Chunk* chunk, int key, Data data);
Bool deleteInChunk(Btree* btree, ThreadGlobals* tg, Chunk* chunk,int key, Data expectedData);

ReturnCode insertEntry(Btree* btree, ThreadGlobals* tg, Chunk* chunk, Entry* entry, int key);
Bool find(Btree* btree, ThreadGlobals* tg, Chunk* chunk, int key);
Bool replaceInChunk(Btree* btree, ThreadGlobals* tg, Chunk* chunk, \
			int key, uint64_t expected, uint64_t new);
/*****************************************************************************
	Freeze Algorithm
*****************************************************************************/
Chunk* freeze(Btree* btree, ThreadGlobals* tg, Chunk* chunk, uint64_t key, 
		uint64_t expected, uint64_t data, FreezeTriggerType trigger, Bool* result);
Chunk* freezeRecovery(Btree* btree, ThreadGlobals* tg, Chunk* oldChunk, 
			uint64_t key, uint64_t expected, uint64_t input, RecoveryType recovType,
			Chunk* mergeChunk, FreezeTriggerType trigger, Bool* result);
void stabilizeChunk(Btree* btree, ThreadGlobals* tg, Chunk* chunk);
/*****************************************************************************
	Reclamation Algorithm
*****************************************************************************/
void retireEntry(Btree* btree, ThreadGlobals* tg, Entry* entry);
void handleReclamationBuffer(Btree* btree, ThreadGlobals* tg);
Bool clearEntry(Btree* btree, ThreadGlobals* tg, Chunk* chunk, Entry* entry);

void retireChunk(Btree* btree,ThreadGlobals* tg, Chunk* chunk);
void handleChunkReclamationBuffer(Btree* btree, ThreadGlobals* tg);

#endif //_CHUNK_ALGORITHM_H_FILE


