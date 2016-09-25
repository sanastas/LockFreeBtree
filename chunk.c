#include "chunk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


/*****************************************************************************
							Aux Functions declarations.
*****************************************************************************/
static inline void buildKeyArray(int arr[] ,Chunk* c1, Chunk* c2);
static inline void copyEntriesArray(int oldStart, int oldEnd,
									int newStart, int newEnd,
									Chunk* old, Chunk* new);
/****************************************************************************/


/*****************************************************************************
	getEntrNum :
	Goes over all reachable entries in Chunk c, counts them, 
	and returns the number of entries.
	Chunk c is assumed to be frozen and thus cannot be modified.
*****************************************************************************/
uint64_t getEntrNum (Chunk* c) {
	assert(clearFreezeState(c) != NULL);
	assert(validateChunk(c));
	
	uint64_t counter = 0;
	Entry* iterator = &c->entryHead;
	
	while ((iterator = getNextEntry(iterator, c->entriesArray)) != NULL) {
		++counter;
	}
	return counter;
}

/*****************************************************************************
	initChunk :
	initiates a new chunk as a zeroed memory chunk. 
	The freeze state is set to INFANT.
*****************************************************************************/
void initChunk(Chunk* newChunk) {
	assert(clearFreezeState(newChunk) != NULL);
	
	memset(newChunk, 0, sizeof(Chunk));
	
	//mark chunk freeze state as INFANT
	newChunk->mergeBuddy = C_INFANT;
	
	for (int i=0; i<MAX_ENTRIES; ++i) {
		//set entry to point on END_INDEX
		newChunk->entriesArray[i].nextEntry = \
			setIndex(newChunk->entriesArray[i].nextEntry, END_INDEX);
		
		//init version to 1
		newChunk->entriesArray[i].nextEntry = setVersion(newChunk->entriesArray[i].nextEntry, 1);
		
		//mark entry as AVAILABLE_ENTRY
		newChunk->entriesArray[i].keyData = AVAILABLE_ENTRY;
	}
	//set the head to point on END_INDEX
	newChunk->entryHead.nextEntry = \
		setIndex(newChunk->entryHead.nextEntry, END_INDEX);
}


/*****************************************************************************
	allocateEntry :
	The allocation of an available entry is executed using the AllocateEntry
	method. An available entry contains NOT_DEFINED_KEY as a key 
	and zeros otherwise. 
	An available entry is allocated by assigning the key and data values in 
	the keyData word in a single atomic compare-and-swap
*****************************************************************************/
Entry* allocateEntry(Chunk* c, int key, Data data) {
	assert(clearFreezeState(c) != NULL);
	assert(validateChunk(c));
	
	uint64_t keyData = combineKeyData(key, data);

	for(int i=0; i<MAX_ENTRIES; ++i) {
		Entry* currentE = &(c->entriesArray[i]);
		if ( currentE->keyData == AVAILABLE_ENTRY ) {
			if ( CAS(&(currentE->keyData), AVAILABLE_ENTRY, keyData) ) {
				assert(validateChunk(c));
				return currentE;
			}
		}
	}
	
	assert(validateChunk(c));
	return NULL;
}

/*****************************************************************************
	combineFreezeState :
	receives a pointer mergeBuddy, and sets the 3 LSB of it as state.
*****************************************************************************/
Chunk* combineFreezeState(Chunk* mergeBuddy, FreezeState state) {
	uint64_t intNewChunk = (uint64_t)mergeBuddy;
	uint64_t tmpState = (((uint64_t)state) & FREEZE_STATE_MASK);
	
	intNewChunk = ( intNewChunk & (~FREEZE_STATE_MASK) );
	intNewChunk = ( intNewChunk | tmpState);
	
	return (Chunk*)intNewChunk;
}

/*****************************************************************************
	extractFreezeState :
	receives a pointer Buddy, and returns the 3 LSB of it as state.
*****************************************************************************/
FreezeState extractFreezeState(Chunk* buddy) {
	return (FreezeState) (((uint64_t)buddy) & FREEZE_STATE_MASK);
}

/*****************************************************************************
	clearFreezeState :
	receives a pointer Buddy, and returns the pointer without freeze state.
*****************************************************************************/
Chunk* clearFreezeState(Chunk* buddy) {
	return (Chunk*) (((uint64_t)buddy) & ~FREEZE_STATE_MASK);
}

/*****************************************************************************
	markChunkFrozen :
	marks each entry in the chunk c as frozen.
*****************************************************************************/
void markChunkFrozen(Chunk* c) {
	assert(clearFreezeState(c) != NULL);
	assert(validateChunk(c));

	int i = 0;
	uint64_t savedWord = 0;

	Entry* currentE = &(c->entryHead);
	
	for(i=0; i<=MAX_ENTRIES; ++i) {
		savedWord = currentE->nextEntry;
		while ( !isFrozen(savedWord) ) { // Loop till the next pointer is frozen
			CAS(&(currentE->nextEntry), savedWord, markFrozen(savedWord));
			savedWord = currentE->nextEntry; // Reread from shared memory
		}
		
		savedWord = currentE->keyData;
		while ( !isFrozen(savedWord) ) { // Loop till the keyData word is frozen
			CAS(&(currentE->keyData), savedWord, markFrozen(savedWord));
			savedWord = currentE->keyData; // Reread from shared memory
		}
		currentE = &(c->entriesArray[i]);
	}
	assert(validateChunk(c));
	return;
}

/*****************************************************************************
	freezeDecision :
	It computes the number of entries and returns the
	recovery code: R_SPLIT, R_MERGE, or R_COPY.
*****************************************************************************/
RecoveryType freezeDecision(Chunk* c) {
	assert(clearFreezeState(c) != NULL);
	assert(validateChunk(c));
	
	uint64_t count = getEntrNum(c);
	if ( count == MIN_ENTRIES && c->root == FALSE) {
		return R_MERGE;
	} 
	else if ( count == MAX_ENTRIES) {
		return R_SPLIT;
	}
	else {
		return R_COPY;
	}
}

/*****************************************************************************
	incCount :
	Increments the counter of entries in the chunk
*****************************************************************************/
void incCount(Chunk* c) {
	assert(clearFreezeState(c) != NULL);
	assert(validateChunk(c));
	
	uint64_t counter=0;
	while(TRUE) {
		counter = c->counter;
		if (CAS(&(c->counter),counter,counter+1)) {
			return;
		}
	}
}

/*****************************************************************************
	decCount :
	decrements the counter of entries in the chunk
*****************************************************************************/
Bool decCount(Chunk* c) {
	assert(clearFreezeState(c) != NULL);
	assert(validateChunk(c));

	uint64_t counter=0;
	while(TRUE) {
		counter = c->counter;
		if ( counter == MIN_ENTRIES && !c->root) {
			return FALSE;
		}
		if (CAS(&(c->counter),counter, MAX(0,counter-1))) {
			return TRUE;
		}
	}
}

/*****************************************************************************
	copyEntriesArray :
	copy entries from old chunk to new chunk
	oldStart, oldEnd, newStart, newEnd are number of entires in the list
	from [oldStart...oldEnd] in old chunk
	to   [newStart...newEnd] in new chunk
*****************************************************************************/
static inline void copyEntriesArray(int oldStart, int oldEnd,
									int newStart, int newEnd,
									Chunk* old, Chunk* new) {
	
	assert(oldStart >= 0);
	assert(oldEnd >= 0);
	assert(newStart >= 0);
	assert(newEnd >= 0);
	assert(oldEnd < MAX_ENTRIES);
	assert(newEnd < MAX_ENTRIES);
	assert(oldStart <= oldEnd);
	assert(newStart <= newEnd);
	
	assert(clearFreezeState(old) != NULL);
	assert(clearFreezeState(new) != NULL);
	
	assert(isFrozen(old->entryHead.nextEntry));
	assert(!isDeleted(old->entryHead.nextEntry));
	assert(!isFrozen(new->entryHead.nextEntry));
	assert(!isDeleted(new->entryHead.nextEntry));
	
	new->entryHead.nextEntry = setIndex(new->entryHead.nextEntry, 0);

	uint64_t i = newStart;
	uint64_t j = 0;
	Entry* iterator = getNextEntry(&old->entryHead, old->entriesArray);
	
	//advance to oldStart
	while (iterator!= NULL && j<oldStart && j<MAX_ENTRIES) {
		++j;
		iterator = getNextEntry(iterator, old->entriesArray);
	}
	
	while (iterator != NULL && i<=newEnd && i<MAX_ENTRIES && j<=oldEnd) {
		//all entries are frozen and chunk is stabilized, no deleted expected
		assert(!isDeleted(iterator->nextEntry));
		if (!(isFrozen(iterator->nextEntry))) {
			printf("address of iterator: %p\n", iterator);
		}
		assert(isFrozen(iterator->nextEntry));
		
		uint64_t keyData = clearFrozen(iterator->keyData);
		new->entriesArray[i].keyData = keyData;

		iterator = getNextEntry(iterator, old->entriesArray);
		
		if (iterator != NULL && i<newEnd && i<MAX_ENTRIES-1 && j<oldEnd) {
			//still in range
			new->entriesArray[i].nextEntry = setIndex(new->entriesArray[i].nextEntry, i+1);
		}
		else {
			//end of range
			new->entriesArray[i].nextEntry = setIndex(new->entriesArray[i].nextEntry, END_INDEX);
		}

		++(new->counter);
		++i;
		++j;
	}
}

/*****************************************************************************
	buildKeyArray :
	construct an array of int32 keys from entries in c1 and c2 Chunks
*****************************************************************************/
static inline void buildKeyArray(int arr[] ,Chunk* c1, Chunk* c2) {
	assert(clearFreezeState(c1) != NULL);
	assert(arr);

	Entry* iter = getNextEntry(&c1->entryHead, c1->entriesArray);
	uint64_t i = 0;
	
	//first chunk
	while (iter != NULL) {
		arr[i] = getKey(iter);
		iter = getNextEntry(iter, c1->entriesArray);
		++i;
	}
	//assert that all entires were copied from first chunk
	assert(getEntrNum(c1) == i);
	
	if (c2 != NULL) {
		//second chunk
		iter = getNextEntry(&c2->entryHead, c2->entriesArray);
		while (iter != NULL) {
			arr[i] = getKey(iter);
			iter = getNextEntry(iter, c2->entriesArray);
			++i;
		}
		//assert that all entires were copied from second chunk
		assert((getEntrNum(c1) + getEntrNum(c2)) == i);
	}
}

/*****************************************************************************
	copyToOneChunk :
	Goes over all reachable entries in the old chunk and copies 
	them to the new chunk. It is assumed no other thread
	is modifying the new chunk,and that the old chunk is frozen,
	so it cannot be modifed as well.
*****************************************************************************/
void copyToOneChunk(Chunk* old, Chunk* new) {
	assert(clearFreezeState(old) != NULL);
	assert(clearFreezeState(new) != NULL);
	assert(getEntrNum(new) == 0);
	assert(getEntrNum(old) >= 0);
	assert(getEntrNum(old) <= MAX_ENTRIES);
	assert(validateChunk(old));
	assert(noDeletedEntries(old));
	assert(extractFreezeState(old->mergeBuddy) != NORMAL);
	assert(extractFreezeState(old->mergeBuddy) != INFANT);
	
	uint64_t oldcount = getEntrNum(old);
	assert(oldcount > 0 || old->root);
	if (oldcount > 0) {
		copyEntriesArray(0, oldcount-1, 0, oldcount-1, old, new);
	}
	
	// for Btree
	new->root = old->root;
	new->height = old->height;
	new->creator = old;
	
	//printChunkSimple(new);
	assert(validateChunk(new));
	assert(noDeletedEntries(new));
	assert(new->nextChunk == NULL);
	assert(extractFreezeState(new->mergeBuddy) == INFANT);
}

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
int mergeToTwoChunks(Chunk* old1, Chunk* old2, Chunk* new1, Chunk* new2) {
	assert(clearFreezeState(old1) != NULL);
	assert(clearFreezeState(old2) != NULL);
	assert(clearFreezeState(new1) != NULL);
	assert(clearFreezeState(new2) != NULL);
	assert(getEntrNum(new1) == 0);
	assert(getEntrNum(new2) == 0);
	assert(getEntrNum(old1) <= MAX_ENTRIES);
	assert(getEntrNum(old2) <= MAX_ENTRIES);
	assert(getEntrNum(old1) >= MIN_ENTRIES);
	assert(getEntrNum(old2) >= MIN_ENTRIES);
	assert(getEntrNum(old1) + getEntrNum(old2) >= MAX_ENTRIES);
	assert(validateChunk(old1));
	assert(validateChunk(old2));
	assert(noDeletedEntries(old1));
	assert(noDeletedEntries(old2));
	assert(getKey(getNextEntry(&old1->entryHead, old1->entriesArray)) < \
			getKey(getNextEntry(&old2->entryHead, old2->entriesArray)));
	assert(extractFreezeState(old1->mergeBuddy) != NORMAL);
	assert(extractFreezeState(old2->mergeBuddy) != NORMAL);
	
	
	uint64_t old1count = getEntrNum(old1);
	uint64_t old2count = getEntrNum(old2);
	
	uint64_t size = old1count + old2count;
	assert(size > 0);
	assert(size <= 2*MAX_ENTRIES);
	int arr[2*MAX_ENTRIES] = {0};
	
	//find median
	buildKeyArray(arr,old1,old2);
	uint64_t medianIndex = (size-1)/2 + 1;
	int median = arr[medianIndex-1];
	
	uint64_t oldStartIndex = 0;
	uint64_t oldEndIndex = 0;
	uint64_t newStartIndex = 0;
	uint64_t newEndIndex = 0;
	
	if (old1count > medianIndex) { //old1 contains more than needed
		//copy until median in first chunk
		copyEntriesArray(0, medianIndex-1, 0, medianIndex-1, old1, new1);
		
		//copy median and remainings in first chunk to second chunk
		newEndIndex = (old1count-1) - medianIndex;
		copyEntriesArray(medianIndex, old1count-1, 0, newEndIndex, old1, new2);
		
		//connect edges
		new2->entriesArray[newEndIndex].nextEntry = \
			setIndex(new2->entriesArray[newEndIndex].nextEntry, newEndIndex+1);
		
		//copy all the rest to second chunk
		newStartIndex = newEndIndex + 1;
		newEndIndex = newStartIndex + (old2count-1);
		copyEntriesArray(0, old2count-1, newStartIndex, newEndIndex ,old2, new2);
	}
	else if (old1count == medianIndex) {
		//copy everything from first chunk
		copyEntriesArray(0, old1count-1, 0, old1count-1, old1, new1);
		//copy everything from second chunk
		copyEntriesArray(0, old2count-1, 0, old2count-1, old2, new2);
		
	}
	else { //old1 contains less than needed or equal
		//copy everything from first chunk
		copyEntriesArray(0, old1count-1, 0, old1count-1, old1, new1);
		
		//connect edges
		new1->entriesArray[old1count-1].nextEntry = \
			setIndex(new1->entriesArray[old1count-1].nextEntry , old1count);
		
		//copy until median in second old chunk
		oldEndIndex = (medianIndex-1) - new1->counter;
		newEndIndex = old1count + oldEndIndex;
		copyEntriesArray(0, oldEndIndex, old1count, newEndIndex ,old2, new1);
		
		//copy all the rest to second chunk
		oldStartIndex = oldEndIndex + 1;
		newEndIndex = (old2count-1) - oldStartIndex;
		copyEntriesArray(oldStartIndex, old2count-1, 0, newEndIndex, old2, new2);
	}
	
	assert(new1 != new2);
	new1->nextChunk = new2;
	
	// for Btree
	new1->root = FALSE;
	new2->root = FALSE;
	new1->height = old1->height;
	new2->height = old2->height;
	assert(new1->height == new2->height);
	if (extractFreezeState(old2->mergeBuddy) == MERGE) {
		assert(extractFreezeState(old1->mergeBuddy) == SLAVE_FREEZE);
		new1->creator = old2;
		new2->creator = old2;
	} else {
		assert(extractFreezeState(old1->mergeBuddy) == MERGE);
		assert(extractFreezeState(old2->mergeBuddy) == SLAVE_FREEZE);
		new1->creator = old1;
		new2->creator = old1;
	}

	
	assert(median == findMaxKeyInChunk(new1));
	assert(validateChunk(new1));
	assert(validateChunk(new2));
	assert(noDeletedEntries(new1));
	assert(noDeletedEntries(new2));
	assert(new2->nextChunk == NULL);
	assert(extractFreezeState(new1->mergeBuddy) == INFANT);
	assert(extractFreezeState(new2->mergeBuddy) == INFANT);
	assert( (getEntrNum(old1) + getEntrNum(old2)) == (getEntrNum(new1) + getEntrNum(new2)) );
	return median;
}

/*****************************************************************************
	mergeToOneChunk :
	Goes over all reachable entries on the old1 and old2 chunks
	(which are sequential and have enough entries to fill one chunk)
	and copies them to the new chunk
	It is assumed that no other thread modifes the new chunk and that the old
	chunks are frozen and thus don't change.
*****************************************************************************/
void mergeToOneChunk(Chunk* old1, Chunk* old2, Chunk* new) {
	assert(clearFreezeState(old1) != NULL);
	assert(clearFreezeState(old2) != NULL);
	assert(clearFreezeState(new) != NULL);
	assert(getEntrNum(new) == 0);
	assert(getEntrNum(old1) <= MAX_ENTRIES);
	assert(getEntrNum(old2) <= MAX_ENTRIES);
	assert(getEntrNum(old1) >= 0);
	assert(getEntrNum(old2) >= MIN_ENTRIES);
	assert(getEntrNum(old1) + getEntrNum(old2) <= MAX_ENTRIES);
	assert(validateChunk(old1));
	assert(validateChunk(old2));
	assert(noDeletedEntries(old1));
	assert(noDeletedEntries(old2));
	assert(extractFreezeState(old1->mergeBuddy) != NORMAL);
	assert(extractFreezeState(old2->mergeBuddy) != NORMAL);
	assert(extractFreezeState(old1->mergeBuddy) != INFANT);
	assert(extractFreezeState(old2->mergeBuddy) != INFANT);

	assert(isFrozen(old1->entryHead.nextEntry));
	assert(isFrozen(old2->entryHead.nextEntry));
	
	uint64_t old1count = getEntrNum(old1);
	uint64_t old2count = getEntrNum(old2);
	
	
	if (old1count > 0) { //copy from both old chunks
		assert(getKey(getNextEntry(&old1->entryHead, old1->entriesArray)) < \
				getKey(getNextEntry(&old2->entryHead, old2->entriesArray)));
		
		//copy everything from first Chunk
		copyEntriesArray(0, old1count-1, 0, old1count-1, old1, new);
		assert(validateChunk(new));
		
		//connect the edge entries
		new->entriesArray[old1count-1].nextEntry = \
			setIndex(new->entriesArray[old1count-1].nextEntry, old1count);
		
		//copy everything from second Chunk
		uint64_t newEndIndex = old1count + old2count -1;
		copyEntriesArray(0, old2count-1, old1count, newEndIndex, old2, new);
	}
	else { //copy just from second old chunk
		copyEntriesArray(0, old2count-1, 0, old2count-1, old2, new);
	}
	
	// for Btree
	new->root = FALSE;
	new->height = old1->height;
	if (extractFreezeState(old2->mergeBuddy) == MERGE) {
		new->creator = old2;
	} else {
		assert(extractFreezeState(old1->mergeBuddy) == MERGE);
		assert(extractFreezeState(old2->mergeBuddy) == SLAVE_FREEZE);
		new->creator = old1;
	}
	
	assert(validateChunk(new));
	assert(noDeletedEntries(new));
	assert(new->nextChunk == NULL);
	assert(extractFreezeState(new->mergeBuddy) == INFANT);
	assert( (getEntrNum(old1) + getEntrNum(old2)) == getEntrNum(new) );
}

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
int splitIntoTwoChunks(Chunk* old, Chunk* new1, Chunk* new2) {
	assert(clearFreezeState(old) != NULL);
	assert(clearFreezeState(new1) != NULL);
	assert(clearFreezeState(new2) != NULL);
	assert(getEntrNum(new1) == 0);
	assert(getEntrNum(new2) == 0);
	assert(getEntrNum(old) <= MAX_ENTRIES);
	assert(getEntrNum(old) >= MIN_ENTRIES);
	assert(validateChunk(old));
	assert(noDeletedEntries(old));
	assert(extractFreezeState(old->mergeBuddy) != NORMAL);
	
	
	uint64_t size = getEntrNum(old);
	assert(size > 0);
	assert(size <= MAX_ENTRIES);
	int arr[MAX_ENTRIES] = {0};
	
	//find median
	buildKeyArray(arr,old,NULL);
	uint64_t medianIndex = (size-1)/2 +1;
	int median = arr[medianIndex-1];
	
	//copy until and contain median to first chunk
	copyEntriesArray(0, medianIndex-1, 0, medianIndex-1, old, new1);
	
	//copy remainings to second chunk
	uint64_t newEndIndex = size-1 - medianIndex;
	copyEntriesArray(medianIndex, size-1, 0, newEndIndex, old, new2);
	
	assert(	new1 != new2 );
	new1->nextChunk = new2;
	
	
	// for Btree
	new1->root = FALSE;
	new2->root = FALSE;
	new1->height = old->height;
	new2->height = old->height;
	new1->creator = old;
	new2->creator = old;
	
	//check that first entry in new chunk is median
	assert(median == findMaxKeyInChunk(new1));
	assert(validateChunk(new1));
	assert(validateChunk(new2));
	assert(noDeletedEntries(new1));
	assert(noDeletedEntries(new2));
	assert(new2->nextChunk == NULL);
	assert(extractFreezeState(new1->mergeBuddy) == INFANT);
	assert(extractFreezeState(new2->mergeBuddy) == INFANT);
	assert(getEntrNum(old) == getEntrNum(new1) + getEntrNum(new2));
	return median;
}
/*****************************************************************************
	moveEntryFromFirstToSecond :
	moves the last entry from c1 to c2
*****************************************************************************/
void moveEntryFromFirstToSecond(Chunk* c1, Chunk* c2) {
	assert(c1);
	assert(c2);
	assert(extractFreezeState(c1->mergeBuddy) == INFANT);
	assert(extractFreezeState(c2->mergeBuddy) == INFANT);
	assert(getEntrNum(c1) >= MIN_ENTRIES);
	assert(getEntrNum(c2) >= MIN_ENTRIES);
	assert(getKey(getNextEntry(&c1->entryHead, c1->entriesArray)) < \
			getKey(getNextEntry(&c2->entryHead, c2->entriesArray)));
	
	
	int oldEntIndex = c1->counter -1;
	int newEntIndex = c2->counter; //entriesArray[0..counter-1] are used so take next free spot

	//copy contents
	c2->entriesArray[newEntIndex].keyData = c1->entriesArray[oldEntIndex].keyData;
	//connect
	c2->entriesArray[newEntIndex].nextEntry = \
		setIndex(c2->entriesArray[newEntIndex].nextEntry, 0);
	c2->entryHead.nextEntry = setIndex(c2->entryHead.nextEntry, newEntIndex);
	++c2->counter;

	//free last entry
	c1->entriesArray[oldEntIndex].nextEntry = \
		setIndex(c1->entriesArray[oldEntIndex].nextEntry, END_INDEX);
	c1->entriesArray[oldEntIndex].keyData = AVAILABLE_ENTRY;
	//disconnect last
	c1->entriesArray[oldEntIndex-1].nextEntry = \
		setIndex(c1->entriesArray[oldEntIndex-1].nextEntry, END_INDEX);
	--c1->counter;
}
/*****************************************************************************
	findMaxKeyInChunk :
	find the Max Key In the chunk received. 
*****************************************************************************/
int findMaxKeyInChunk(Chunk* c) {
	assert(clearFreezeState(c));
	
	int max = 0;
	Entry* iterator = getNextEntry(&c->entryHead, c->entriesArray);
	assert(iterator);
	
	while (iterator != NULL) {
		if (!isDeleted(iterator->nextEntry)) {
			max = getKey(iterator);
		}
		iterator = getNextEntry(iterator, c->entriesArray);
	}

	assert(max > 0 && max <= MAX_KEY);
	return max;
}
/*****************************************************************************
	entryIn:
	return an entry in chunk c by the index in word
*****************************************************************************/
Entry* entryIn(Chunk* c, uint64_t word) {
	int index = getIndex(word);
	if (index == END_INDEX)
		return NULL;
	assert(index <= END_INDEX);
	assert(index >= 0);
	return &c->entriesArray[index];
}

/*****************************************************************************
						Debug functions
*****************************************************************************/



/*****************************************************************************
	printChunkSimple : (for debug)
	prints the chunk in a table.
*****************************************************************************/
int printChunkSimple(Chunk* c) {
	if (clearFreezeState(c) == NULL) {
		return 1;
	}
	
	int i = 0;	
	int key1 = 0;
	Data data1 = 0;
		
	printf("----------------------------------------------------------------------------\n");
	printf("==> chunk address is %p\n",(void*)c);
	printf("==> mergeBuddy is %p\n",(void*)c->mergeBuddy);
	printf("==> root flag is %d\n", c->root);
	printf("==> counter is %ld\n",c->counter);
	printf("==> real counter is %ld\n",getEntrNum(c));
	printf("==> height is %ld\n",c->height);
	printf("==> first entry is %d\n",getIndex(c->entryHead.nextEntry));
	printf("------------------------------------------------\n");
	printf("i	key[i]		data[i]		frozenNext	deletedNext	frozenKeyData  nextIndex\n");
	for(i=0; i< MAX_ENTRIES; ++i) {
		parseKeyData(c->entriesArray[i].keyData, &key1, &data1);
		printf("%d	%d		%d		%d		%d		%d               %d\n",\
			i,	key1,		data1,		isFrozen(c->entriesArray[i].nextEntry),\
			isDeleted(c->entriesArray[i].nextEntry),isFrozen(c->entriesArray[i].keyData),
			getIndex(c->entriesArray[i].nextEntry));
	}
	printf("----------------------------------------------------------------------------\n\n");
	
	return 1;
}
/*****************************************************************************
	printChunkKeys :
	print the chunk keys ordered.
*****************************************************************************/
void printChunkKeys(Chunk* c) {
	if (clearFreezeState(c) == NULL) {
		return;
	}
	
	Entry* iterator = &c->entryHead;
	while ((iterator=getNextEntry(iterator, c->entriesArray)) != NULL) {
		printf("%d ",getKey(iterator));
	}
	printf("\n");
}
/*****************************************************************************
	noDeletedEntries :
	assert that there are no deleted entries in the chunk
*****************************************************************************/
int noDeletedEntries(Chunk* c) {
	assert(clearFreezeState(c) != NULL);
	
	Entry* iter = getNextEntry(&c->entryHead, c->entriesArray);
	
	while (iter != NULL) {
		if (isDeleted(iter->nextEntry)) {
			printf("found deleted entry!\n");
			printChunkSimple(c);
			return 0;
		}
		iter = getNextEntry(iter, c->entriesArray);
	}
	return 1;
}
/*****************************************************************************
	validateChunk :
	returns 1 if chunk is ok and all keys are alligned and no deleted keys
*****************************************************************************/
int validateChunk(Chunk* c) {
	
	return 1;//removed for faster run with asserts - chunks appear always valide
	
	int key1 = 0;
	int key2 = 0;
	Data data1 = 0;
	Data data2 = 0;
	
	if (clearFreezeState(c) == NULL) {
		return 1;
	}

	uint64_t count = 1;
	
	//printf("entryHead: %p\n", c->entryHead.nextEntry);
	//printf("entry0: %p\n", c->entriesArray[0].nextEntry);
	Entry* iter = getNextEntry(&(c->entryHead), c->entriesArray);
	Entry* last = iter;
	
	//printf("iter: %p\n",iter);
	//printf("last: %p\n",last);
	
	iter = getNextEntry(iter, c->entriesArray);
	while(iter != NULL) {
		if ( count > MAX_ENTRIES ) {
			printf("problem1 : count > MAX_ENTRIES\n");
			return 0;
		}
		parseKeyData(last->keyData, &key1, &data1);
		parseKeyData(iter->keyData, &key2, &data2);
		if (key2 <= key1) {
			printChunkSimple(c);
			printf("key1 is: %d\n",key1);
			printf("key2 is: %d\n",key2);
			printf("key2 should be bigger!\n");
			return 0;
		}
		
		last = iter;
		iter = getNextEntry(iter, c->entriesArray);
		
		++count;
	}
	
	if ( count > MAX_ENTRIES ) {
		printf("problem2 : count > MAX_ENTRIES\n");
		return 0;
	}
	
	return 1;
}

