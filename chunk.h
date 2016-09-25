#ifndef __CHUNK_H__
#define __CHUNK_H__

#include "entry.h"
#include <stdio.h>
#include <glib.h>


#define CAS(ptr, oldval, newval) \
g_atomic_pointer_compare_and_exchange \
(((gpointer*)(ptr)),((gpointer)(oldval)),((gpointer)(newval)))


typedef enum {INFANT, NORMAL, FREEZE, COPY, SPLIT, MERGE, REQUEST_SLAVE, SLAVE_FREEZE} FreezeState;
typedef enum {R_COPY, R_MERGE, R_SPLIT} RecoveryType;

#define FREEZE_STATE_MASK 0x0000000000000007

//predefined freeze states with NULL pointer
#define C_INFANT	(0x0000000000000000)
#define C_NORMAL	(0x0000000000000001)
#define C_FREEZE	(0x0000000000000002)
#define C_COPY		(0x0000000000000003)
#define C_SPLIT		(0x0000000000000004)


typedef struct Chunk_t {
	uint64_t counter;
	Entry entryHead;
	Entry entriesArray[MAX_ENTRIES];
	struct Chunk_t* newChunk;
	struct Chunk_t* mergeBuddy;
	struct Chunk_t* nextChunk;
	
	// btree addition
	Bool root;
	int height;
	struct Chunk_t* creator;
} Chunk;



/*****************************************************************************
	getEntrNum :
	Goes over all reachable entries in Chunk c, counts them, 
	and returns the number of entries.
	Chunk c is assumed to be frozen and thus cannot be modified.
*****************************************************************************/
uint64_t getEntrNum(Chunk* c);

/*****************************************************************************
	initChunk :
	initiates a new chunk as a zeroed memory chunk. 
	The freeze state is set to INFANT.
*****************************************************************************/
void initChunk(Chunk* newChunk);

/*****************************************************************************
	allocateEntry :
	The allocation of an available entry is executed using the AllocateEntry
	method. An available entry contains NOT_DEFINED_KEY as a key 
	and zeros otherwise. 
	An available entry is allocated by assigning the key and data values in 
	the keyData word in a single atomic compare-and-swap
*****************************************************************************/
Entry* allocateEntry(Chunk* c, int key, Data data);

/*****************************************************************************
	combineFreezeState :
	receives a pointer mergeBuddy, and sets the 3 LSB of it as state.
*****************************************************************************/
Chunk* combineFreezeState(Chunk* mergeBuddy,FreezeState state);

/*****************************************************************************
	extractFreezeState :
	receives a pointer Buddy, and returns the 3 LSB of it as state.
*****************************************************************************/
FreezeState extractFreezeState(Chunk* buddy);

/*****************************************************************************
	clearFreezeState :
	receives a pointer Buddy, and returns the pointer without freeze state.
*****************************************************************************/
Chunk* clearFreezeState(Chunk* buddy);

/*****************************************************************************
	markChunkFrozen :
	marks each entry in the chunk c as frozen.
*****************************************************************************/
void markChunkFrozen(Chunk* c);

/*****************************************************************************
	freezeDecision :
	It computes the number of entries and returns the
	recovery code: R_SPLIT, R_MERGE, or R_COPY.
*****************************************************************************/
RecoveryType freezeDecision(Chunk* c);

/*****************************************************************************
	incCount :
	Increments the counter of entries in the chunk
*****************************************************************************/
void incCount(Chunk* c);

/*****************************************************************************
	decCount :
	decrements the counter of entries in the chunk
*****************************************************************************/
Bool decCount(Chunk* c);

/*****************************************************************************
	copyToOneChunk :
	Goes over all reachable entries in the old chunk and copies 
	them to the new chunk. It is assumed no other thread
	is modifying the new chunk,and that the old chunk is frozen,
	so it cannot be modifed as well.
*****************************************************************************/
void copyToOneChunk(Chunk* old, Chunk* new);

/*****************************************************************************
	mergeToTwoChunks :
	Goes over all reachable entries in the old1 and old2 chunks
	(which are sequential),finds the median key(which is returned) and copies 
	the bellow-median-value keys to new1 chunk and
	the above-median-value keys to the new2 chunk
	Median itself is inculded in new1.
	In addition it sets the new1 chunk pointer nextChunk to point to the
	new2 chunk. It is assumed that no other thread modifies the new1 and new2
	chunks,and that the old chunks are frozen and thus cannot be modifed 
	as well.
*****************************************************************************/
int mergeToTwoChunks(Chunk* old1, Chunk* old2, Chunk* new1, Chunk* new2);

/*****************************************************************************
	mergeToOneChunk :
	Goes over all reachable entries on the old1 and old2 chunks
	(which are sequential and have enough entries to fill one chunk)
	and copies them to the new chunk
	It is assumed that no other thread modifes the new chunk and that the old
	chunks are frozen and thus don't change.
*****************************************************************************/
void mergeToOneChunk(Chunk* old1, Chunk* old2, Chunk* new);

/*****************************************************************************
	splitIntoTwoChunks :
	Goes over all reachable entries on the old chunk
	finds the median key (which is returned) and copies the 
	bellow-median-value keys to new1 chunk and the above-median-value keys
	to the new2 chunk.
	Median itself is inculded in new1.
	In addition it sets the new1 chunk pointer nextChunk 
	to point at the new2 chunk.
	It is assumed that no other thread is modifying the new1 and new2 chunks,
	and that the old chunk is frozen and cannot be modifed.
*****************************************************************************/
int splitIntoTwoChunks(Chunk* old, Chunk* new1, Chunk* new2);

/*****************************************************************************
	moveEntryFromFirstToSecond :
	moves the last entry from c1 to c2
*****************************************************************************/
void moveEntryFromFirstToSecond(Chunk* c1, Chunk* c2);

/*****************************************************************************
	findMaxKeyInChunk :
	find the Max Key In the chunk received. 
*****************************************************************************/
int findMaxKeyInChunk(Chunk* c);


/*****************************************************************************
	entryIn:
	return an entry in chunk c by the index in word
*****************************************************************************/
Entry* entryIn(Chunk* c, uint64_t word);


/*****************************************************************************
	printChunk :
	prints the chunk
*****************************************************************************/
int printChunkSimple(Chunk* c);

/*****************************************************************************
	printChunkKeys :
	print the chunk keys ordered.
*****************************************************************************/
void printChunkKeys(Chunk* c);

/*****************************************************************************
	Debug functions
*****************************************************************************/
int noDeletedEntries(Chunk* c);

/*****************************************************************************
	validateChunk :
	returns 1 if chunk is ok and all keys are alligned and no deleted keys
*****************************************************************************/
int validateChunk(Chunk* c);



#endif //__CHUNK_H__

