#include "globals.h"
#include "list.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

/*****************************************************************************
	addToRetList :
	receives an entry, and ThreadGlobals, and adds the entry to 
	entryRetList in ThreadGlobals as EntryL.
*****************************************************************************/
void addToRetList(ThreadGlobals* tg, Entry* entry) {
	EntryL* newEntry = (EntryL*) malloc(sizeof(EntryL));
	newEntry->entry = entry;
	list_add_tail(&newEntry->list1, &(tg->entryRetList->list1));
}
/*****************************************************************************
	addEntryLToRetList :
	receives an EntryL, and ThreadGlobals, and adds the EntryL to 
	entryRetList in ThreadGlobals as EntryL.
*****************************************************************************/
void addEntryLToRetList(ThreadGlobals* tg, EntryL* entry) {
	list_add_tail(&entry->list1, &(tg->entryRetList->list1));
}
/*****************************************************************************
	addToChunkRetList :
	receives a chunk, and ThreadGlobals, and adds the chunk to 
	chunkRetList in ThreadGlobals as ChunkL.
*****************************************************************************/
void addToChunkLRetList(ThreadGlobals* tg, ChunkL* chunk) {
	list_add_tail(&chunk->list1, &(tg->chunkRetList->list1));
}
/*****************************************************************************
	addChunkLToRetList :
	receives an ChunkL, and ThreadGlobals, and adds the ChunkL to 
	chunkRetList in memoryManager as ChunkL.
*****************************************************************************/
void addChunkToRetList(ThreadGlobals* tg, Chunk* chunk) {
	ChunkL* newChunk = (ChunkL*) malloc(sizeof(ChunkL));
	newChunk->chunk = chunk;
	list_add_tail(&(newChunk->list1), &(tg->chunkRetList->list1));
}
/*****************************************************************************
	popAllRetList :
	receives a ThreadGlobals, clears it's entryRetList, and moves it's items
	to retval.
*****************************************************************************/
void popAllRetList(ThreadGlobals* tg, EntryL* retval) {
	INIT_LIST_HEAD(&retval->list1);
	
	EntryL* entry = NULL;
	list_t* tmpEntry = NULL;
	
	while ((tmpEntry = list_pop(&tg->entryRetList->list1))) {
		entry = list_entry(tmpEntry, EntryL, list1);
		list_add_tail(&entry->list1, &(retval->list1));
	}

	return;
}
/*****************************************************************************
	popAllChunkRetList :
	receives a ThreadGlobals, clears it's chunkRetList, and moves it's items
	to retval.
*****************************************************************************/
void popAllChunkRetList(ThreadGlobals* tg, ChunkL* retval) {
	INIT_LIST_HEAD(&retval->list1);

	ChunkL* chunk = NULL;
	list_t* tmpChunk = NULL;

	while ((tmpChunk = list_pop(&tg->chunkRetList->list1))) {
		chunk = list_entry(tmpChunk, ChunkL, list1);
		list_add_tail(&(chunk->list1), &(retval->list1));
	}

	return;
}
/*****************************************************************************
	allocateChunk :
	allocates chunk in the pool
*****************************************************************************/
int allocateChunk(MemoryManager* memoryManager) {
	assert(memoryManager != NULL);
	
	uint64_t numOfFlagWords = MAX_CHUNKS/BITS_IN_WORD - 1;

	srand(time(NULL));
	int start_pnt = rand()%numOfFlagWords;

	//traverse all flag words
	for(int i=start_pnt+1; i!=start_pnt; i=(i+1)%numOfFlagWords) {
		
			
		//initial masks
		uint64_t add_mask = 0x0000000000000001;
		uint64_t remove_mask = 0xfffffffffffffffe;
		
		//traverse all bits in a single flag word
		for(int k=0; k < BITS_IN_WORD; ++k) {
			
			//set ON the bit we try to capture
			uint64_t replace = memoryManager->memoryPoolFlags[i] | add_mask;
			//expected bit should be OFF
			uint64_t expected = memoryManager->memoryPoolFlags[i] & remove_mask;
			
			//attempt to capture bit
			if ( CAS(&(memoryManager->memoryPoolFlags[i]), expected, replace) ) {
			
				//the index of the allocated chunk is:
				// (flag words we passed * bits in each word) + index of succesfuly captured bit
				int retval = i * BITS_IN_WORD + k;
				
				//initialize the chunk for usage
				//initChunk(&memoryManager->memoryPool[retval]);
				assert(retval >= 0 && retval<MAX_CHUNKS);
				return retval;
			}
			//failed to capture the current bit, advance to next bit in the word
			add_mask = add_mask << 1;
			remove_mask = (remove_mask << 1) +1;
		}
		
	}
	
	printf("OUT OF MEMORY\n");
	assert(FALSE); //we get here only if out of memory
	return -1;
}
/*****************************************************************************
	clearChunk :
	free chunk from the pool.
*****************************************************************************/
void clearChunk(MemoryManager* memoryManager, int index) {

	assert(memoryManager != NULL);
	
	//zero the memory of the chunk
	//Chunk* chunk = &(memoryManager->memoryPool[index]);
	//initChunk( chunk );

	//mark it as available for use
	CAS(&(memoryManager->memoryPoolFlags[index]), 1, 0);
}

/*****************************************************************************
	getSon :
	returns the son of the entry by translating data -> chunk in memorypool
*****************************************************************************/
Chunk* getSon(MemoryManager* memoryManager, Entry* entry) {
	assert(entry);
	return &memoryManager->memoryPool[getData(entry)];
}


/*****************************************************************************
	getChunkIndex :
	returns the index of the chunk in memorypool
*****************************************************************************/
int getChunkIndex(MemoryManager* memoryManager, Chunk* chunk) {
	int retval = chunk - memoryManager->memoryPool;
	assert(retval >= 0 && retval<MAX_CHUNKS);
	return retval;
}








/*****************************************************************************
							AUX FUNCTIONS
*****************************************************************************/

/*****************************************************************************
	lookUpEntryLsList :
	find entry in EntryL list
*****************************************************************************/
Bool lookUpEntryLsList(EntryL* head, EntryL* entry) {
	EntryL* tmpEntry = NULL;
	list_t* pos = NULL;
	list_t* list1 = &head->list1;
	list_for_each(pos,list1) {
		tmpEntry = list_entry(pos,EntryL,list1);
		if (getKey(tmpEntry->entry) == getKey(entry->entry) ) {
			return TRUE;
		}
	}
	return FALSE;
}
/*****************************************************************************
	lookUpChunkLsList :
	find chunk in ChunkL list
*****************************************************************************/
Bool lookUpChunkLsList(ChunkL* head, ChunkL* chunk) {
/*
	ChunkL* tmpChunk = NULL;
	list_t* pos = NULL;
	list_t* list1 = &head->list1;
	list_for_each(pos,list1) {
		tmpChunk = list_entry(pos,ChunkL,list1);
		if ( tmpChunk->chunk == chunk->chunk ) {
			return TRUE;
		}
	}
	return FALSE;
	*/
}
/*****************************************************************************
	freeEntryLsList :
	free EntryL list
*****************************************************************************/
void freeEntryLsList(EntryL* head) {
	EntryL* entry1 = NULL;
	list_t* pos = NULL;
	list_t* n = NULL;
	list_t* list1 = &head->list1;
	list_for_each_safe(pos, n, list1) {
		entry1 = list_entry(pos,EntryL,list1);
		list_del(pos);
		free(entry1);
	}
}
/*****************************************************************************
	freeChunkLsList :
	free ChunkL list
*****************************************************************************/
void freeChunkLsList(ChunkL* head) {
/*
	ChunkL* tmpChunk = NULL;
	list_t* pos = NULL;
	list_t* n = NULL;
	list_t* list1 = &head->list1;
	list_for_each_safe(pos, n, list1) {
		tmpChunk = list_entry(pos,ChunkL,list1);
		list_del(pos);
		free(tmpChunk);
	}
	*/
}
