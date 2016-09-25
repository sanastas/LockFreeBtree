#ifndef _TREE_ALGORITHM_H_FILE
#define _TREE_ALGORITHM_H_FILE


#include "entry.h"
#include "chunk.h"
#include "globals.h"
#include "chunkAlgorithm.h"



/*****************************************************************************
	searching
*****************************************************************************/
Chunk* findLeaf(Btree* btree, ThreadGlobals* tg, int key);
Chunk* findParent(Btree* btree, ThreadGlobals* tg, int key, Chunk* sonChunk,
	Entry** parentEnt, Entry** neighborEnt);
/*****************************************************************************
	enslave
*****************************************************************************/
Chunk* findMergeSlave(Btree* btree, ThreadGlobals* tg, Chunk* master);
Bool setSlave(Btree* btree, ThreadGlobals* tg, Chunk* master, Chunk* slave, int masterKey, int slaveKey);
/*****************************************************************************
	balancing
*****************************************************************************/
void split(Btree* btree, ThreadGlobals* tg, int sepKey, Chunk* chunk);
void merge(Btree* btree, ThreadGlobals* tg, Chunk* master);
void borrow(Btree* btree, ThreadGlobals* tg, Chunk* master, int sepKey);
void splitRoot(Btree* btree, ThreadGlobals* tg, Chunk* root, int sepKey);
void mergeRoot(Btree* btree, ThreadGlobals* tg, Chunk* root, Chunk* possibleNewRoot, int key, Data data, Chunk* master);
/*****************************************************************************
	synchronizing main structure
*****************************************************************************/
void callForUpdate(Btree* btree, ThreadGlobals* tg, RecoveryType recovType, Chunk* chunk, int sepKey);
void helpInfant(Btree* btree, ThreadGlobals* tg, Chunk* chunk);
void addRootSons(Btree* btree, Chunk* newRoot, int sepKey1, Chunk* newLow, int sepKey2, Chunk* newHigh);
/*****************************************************************************
	debug and test
*****************************************************************************/
void printTree(Btree* btree);
void printTree1(Btree* btree, Chunk* head, const char* tabs);
int TreeSize(Btree* btree);


#endif //_TREE_ALGORITHM_H_FILE



