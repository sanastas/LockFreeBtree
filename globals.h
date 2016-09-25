#ifndef _GLOBALS_H_FILE_
#define _GLOBALS_H_FILE_

#include "entry.h"
#include "chunk.h"
#include "list.h"


#ifndef MAX_THREADS_NUM 
#define MAX_THREADS_NUM 10
#endif

#ifndef MAX_CHUNKS 
#define MAX_CHUNKS 1000000
#endif

#define BITS_IN_BYTE 8
#define BITS_IN_WORD ((sizeof(uint64_t))*(BITS_IN_BYTE))


/*****************************************************************************/
typedef struct HPRecord_t {
	// hazard pointers for entries
	Entry* hp0;
	Entry* hp1;
} HPRecord;
/*****************************************************************************/
typedef struct ChunkL_t { 
	Chunk* chunk;
	list_t list1;  // linux list.
} ChunkL;

typedef struct EntryL_t {
	Entry* entry;
	list_t list1;  // linux list.
} EntryL;
/*****************************************************************************/
typedef struct MemoryManager_t {
	HPRecord HPArray[MAX_THREADS_NUM]; // hazard Pointers Array
	ChunkL chunkRetList[MAX_THREADS_NUM];
	EntryL entryRetList[MAX_THREADS_NUM];
	Chunk memoryPool[MAX_CHUNKS];
	uint64_t memoryPoolFlags[MAX_CHUNKS/BITS_IN_WORD +1];
} MemoryManager;
/*****************************************************************************/
typedef struct Btree_t {
	Chunk* root;
	MemoryManager* memoryManager;
} Btree;
/*****************************************************************************/
typedef struct ThreadGlobals_t {	
	uint64_t* prev;
	uint64_t cur;
	uint64_t next;
	ChunkL* chunkRetList;
	EntryL* entryRetList;
	Entry** hp0;
	Entry** hp1;
	int id;
} ThreadGlobals;
/*****************************************************************************/







/*****************************************************************************
	addToRetList :
	receives an entry, and ThreadGlobals, and adds the entry to 
	entryRetList in ThreadGlobals as EntryL.
*****************************************************************************/
void addToRetList(ThreadGlobals* tg, Entry* entry);

/*****************************************************************************
	addEntryLToRetList :
	receives an EntryL, and ThreadGlobals, and adds the EntryL to 
	entryRetList in ThreadGlobals as EntryL.
*****************************************************************************/
void addEntryLToRetList(ThreadGlobals* tg, EntryL* entry);

/*****************************************************************************
	addToChunkRetList :
	receives a chunk, and ThreadGlobals, and adds the chunk to 
	chunkRetList in ThreadGlobals as ChunkL.
*****************************************************************************/
void addToChunkRetList(ThreadGlobals* tg, Chunk* chunk);

/*****************************************************************************
	addChunkLToRetList :
	receives an ChunkL, and ThreadGlobals, and adds the ChunkL to 
	chunkRetList in memoryManager as ChunkL.
*****************************************************************************/
void addChunkLToRetList(ThreadGlobals* tg, ChunkL* chunk);


/*****************************************************************************
	popAllRetList :
	receives a ThreadGlobals, clears it's entryRetList, and moves it's items
	to retval.
*****************************************************************************/
void popAllRetList(ThreadGlobals* tg, EntryL* retval);

/*****************************************************************************
	popAllChunkRetList :
	receives a ThreadGlobals, clears it's chunkRetList, and moves it's items
	to retval.
*****************************************************************************/
void popAllChunkRetList(ThreadGlobals* tg, ChunkL* retval);


/*****************************************************************************
	allocateChunk :
	allocates chunk in the pool
*****************************************************************************/
int allocateChunk(MemoryManager* memoryManager);

/*****************************************************************************
	clearChunk :
	free chunk from the pool.
*****************************************************************************/
void clearChunk(MemoryManager* memoryManager, int index);

/*****************************************************************************
	getSon :
	returns the son of the entry by translating data -> chunk in memorypool
*****************************************************************************/
Chunk* getSon(MemoryManager* memoryManager, Entry* entry);

/*****************************************************************************
	getChunkIndex :
	returns the index of the chunk in memorypool
*****************************************************************************/
int getChunkIndex(MemoryManager* memoryManager, Chunk* chunk);







/*****************************************************************************
							AUX FUNCTIONS
*****************************************************************************/


/*****************************************************************************
	lookUpEntryLsList :
	find entry in EntryL list
*****************************************************************************/
Bool lookUpEntryLsList(EntryL* head, EntryL* entry);

/*****************************************************************************
	lookUpChunkLsList :
	find chunk in ChunkL list
*****************************************************************************/
Bool lookUpChunkLsList(ChunkL* head, ChunkL* chunk);

/*****************************************************************************
	freeEntryLsList :
	free EntryL list
*****************************************************************************/
void freeEntryLsList(EntryL* head);

/*****************************************************************************
	freeChunkLsList :
	free ChunkL list
*****************************************************************************/
void freeChunkLsList(ChunkL* head);


#endif	//_GLOBALS_H_FILE_
