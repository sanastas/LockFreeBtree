#include "lockfreeBtree.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <glib.h>

/*****************************************************************************
							Aux Functions declarations.
*****************************************************************************/
static inline HPRecord* allocate_availabe_record_in_HPArray(Btree* btree, int index);
/*****************************************************************************/




/*****************************************************************************
	searchInTree : search for a key
	returns:
		TRUE - if found 
			data will contain value
		FALSE - the key doesn't exist
*****************************************************************************/
Bool searchInTree(Btree* btree, ThreadGlobals* tg, int key, Data *data) {
	Chunk* chunk = findLeaf(btree, tg, key);
	assert(chunk);
	return searchInChunk(btree, tg, chunk, key, data);
}
/*****************************************************************************
	insertToTree : insert a new key
	returns:
		TRUE - if key was inserted succesfully
		FALSE - if the key already exists
*****************************************************************************/
Bool insertToTree(Btree* btree, ThreadGlobals* tg, int key, Data data) {
	Chunk* chunk = findLeaf(btree, tg, key);
	assert(chunk);
	if (extractFreezeState(chunk->mergeBuddy) == INFANT) {
		helpInfant(btree, tg, chunk);
	}
	assert(extractFreezeState(chunk->mergeBuddy) != INFANT);
	return insertToChunk(btree, tg, chunk, key, data);
}
/*****************************************************************************
	deleteFromTree : delete a key and associated value
	returns:
		TRUE - if the key was deleted
		FALSE - the key was not found
*****************************************************************************/
Bool deleteFromTree(Btree* btree, ThreadGlobals* tg, int key, Data data) {
	Chunk* chunk = findLeaf(btree, tg, key);
	assert(chunk);
	if (extractFreezeState(chunk->mergeBuddy) == INFANT) {
		helpInfant(btree, tg, chunk);
	}
	assert(extractFreezeState(chunk->mergeBuddy) != INFANT);
	return deleteInChunk(btree, tg, chunk, key, data);
}
/*****************************************************************************
	initialize : initialize and return new tree.
*****************************************************************************/
Btree* initialize() {

	Btree* btree = (Btree*)malloc(sizeof(Btree));
	btree->memoryManager = (MemoryManager*)malloc(sizeof(MemoryManager));
	
	memset((btree->memoryManager)->HPArray, 0, sizeof(HPRecord)*MAX_THREADS_NUM);
	
	for (int i=0; i<MAX_CHUNKS; ++i) {
		initChunk(&btree->memoryManager->memoryPool[i]);
	}
	
	btree->root = &btree->memoryManager->memoryPool[allocateChunk(btree->memoryManager)]; 
	btree->root->root = TRUE;
	btree->root->height = 0;

	// set the first chunk freeze state to NORMAL.
	btree->root->mergeBuddy = (Chunk*)C_NORMAL;
	
	for(int i=0; i<MAX_THREADS_NUM; ++i) {
		// initialize the reclamation DS
		INIT_LIST_HEAD(&((btree->memoryManager)->chunkRetList[i].list1));
		INIT_LIST_HEAD(&((btree->memoryManager)->entryRetList[i].list1));
	}
	return btree;
}
/*****************************************************************************
	start_routine :
	each operation will start in this function, Input argument will be passed
	to this function. Input has the Action should be done and the relevant
	parameters.
*****************************************************************************/
void* start_routine(void* arg) {

	Input* input1 = (Input*) arg;
	Bool result = FALSE;
	ThreadGlobals tg1;

	if (input1 == NULL) {
		return NULL;
	}
	
	// allocate a place in the array
	HPRecord* threadRecord = allocate_availabe_record_in_HPArray(input1[0].btree, input1[0].threadNum);
	threadRecord->hp0 = NULL;
	threadRecord->hp1 = NULL;
	
	// allocate hazard pointers
	tg1.hp0 = &(threadRecord->hp0);
	tg1.hp1 = &(threadRecord->hp1);	
	
	//allocate a place in chunkRetList and entryRetList array
	tg1.chunkRetList = &input1[0].btree->memoryManager->chunkRetList[input1[0].threadNum];
	tg1.entryRetList = &input1[0].btree->memoryManager->entryRetList[input1[0].threadNum];
	tg1.id = input1[0].id;
	
	//wait on barrier

	Barrier* bar = input1[0].barrier;
	g_mutex_lock(bar->lock);
	++(bar->waiting);
	if (bar->waiting == bar->size) {
		g_cond_broadcast(bar->cond);
	} else {
		g_cond_wait(bar->cond, bar->lock);
	}
	--(bar->waiting);
	g_mutex_unlock(bar->lock);



	for (int i=0; i < input1[0].operations_per_thread; ++i) {	

		g_timer_start(input1[i].timer);
		
		switch (input1[i].action) {
			case INSERT_OP:
				result = insertToTree(input1[i].btree, &tg1, input1[i].key, input1[i].data);
				break;

			case DELETE_OP:
				result = deleteFromTree(input1[i].btree, &tg1, input1[i].key, input1[i].data);
				break;

			case SEARCH_OP:
				result = searchInTree(input1[i].btree, &tg1, input1[i].key, input1[i].pdata);
				break;
				
		}

		g_timer_stop(input1[i].timer);

		input1[i].result = result;
	}
	

	return 0;
}
/*****************************************************************************
	allocate_availabe_record_in_HPArray :
	allocate HPArray record
*****************************************************************************/
static inline HPRecord* allocate_availabe_record_in_HPArray(Btree* btree, int index) {
	return &btree->memoryManager->HPArray[index];
}
/*****************************************************************************
	freeBtree :
	free all chunks and destroy tree
*****************************************************************************/
void freeBtree(Btree* btree) {
	
	for(int i=0; i<MAX_THREADS_NUM; ++i) {
		freeEntryLsList(&btree->memoryManager->entryRetList[i]);
		freeChunkLsList(&btree->memoryManager->chunkRetList[i]);
	}
	
	free(btree->memoryManager);
	free(btree);
}
