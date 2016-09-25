#include "chunk_test.h"
#include "entry_test.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

/*****************************************************************************/
void runAllChunkTests() {

	test_allocateEntry();
	test_markChunkFrozen();
	test_copyToOneChunk();
	test_mergeToOneChunk();
	test_mergeToTwoChunks(0);
	test_mergeToTwoChunks(1);
	test_splitIntoTwoChunks();
}


/*****************************************************************************/
void test_allocateEntry() {

	printf("==== test_allocateEntry ======\n");

	Chunk* chunk1 = (Chunk*)malloc(sizeof(Chunk));
	initChunk(chunk1);
	int key1=1 , key2=2, key3=3, key4=4;
	Data data1=116 , data2=117, data3=118, data4=119;
	Entry* entry1 = allocateEntry(chunk1,key1,data1);
	Entry* entry2 = allocateEntry(chunk1,key2,data2);
	Entry* entry3 = allocateEntry(chunk1,key3,data3);
	Entry* entry4 = allocateEntry(chunk1,key4,data4);

	

	assert( entry1 != NULL);
	assert( entry2 != NULL);
	assert( entry3 != NULL);
	assert( entry4 != NULL);
	
	assert( entry1->keyData == combineKeyData(key1,data1) );
	assert( entry2->keyData == combineKeyData(key2,data2) );
	assert( entry3->keyData == combineKeyData(key3,data3) );
	assert( entry4->keyData == combineKeyData(key4,data4) );

	assert( &(chunk1->entriesArray[0]) == entry1);
	assert( &(chunk1->entriesArray[1]) == entry2);
	assert( &(chunk1->entriesArray[2]) == entry3);
	assert( &(chunk1->entriesArray[3]) == entry4);
	
	assert( chunk1->entriesArray[0].keyData == combineKeyData(key1,data1) );
	assert( chunk1->entriesArray[1].keyData == combineKeyData(key2,data2) );
	assert( chunk1->entriesArray[2].keyData == combineKeyData(key3,data3) );
	assert( chunk1->entriesArray[3].keyData == combineKeyData(key4,data4) );
	
	assert( chunk1->entriesArray[4].keyData == combineKeyData(NOT_DEFINED_KEY,0));
	
	free(chunk1);
	printf("PASSED\n");
}
/*****************************************************************************/
void test_markChunkFrozen() {

	printf("==== test_markChunkFrozen ======\n");

	Chunk* chunk1 = (Chunk*)malloc(sizeof(Chunk));
	initChunk(chunk1);
	
	
	uint64_t i=0;
	Entry* iterator = NULL;
	
	assert(MIN_ENTRIES >= 2);
	chunk1->entryHead.nextEntry = setIndex( chunk1->entryHead.nextEntry, 1 );
	chunk1->entriesArray[1].nextEntry = setIndex( chunk1->entriesArray[1].nextEntry, 0 );
	chunk1->entriesArray[1].keyData = 10;
	chunk1->entriesArray[0].nextEntry =  setIndex( chunk1->entriesArray[0].nextEntry, END_INDEX );
	chunk1->entriesArray[0].keyData = 12;
	
	assert(isFrozen(chunk1->entryHead.keyData) == FALSE);
	assert(isFrozen(chunk1->entryHead.nextEntry) == FALSE);
	
	iterator = getNextEntry(&chunk1->entryHead, chunk1->entriesArray);
	while (iterator != NULL) {
		assert(!isFrozen(chunk1->entriesArray[i].keyData));
		assert(!isFrozen(chunk1->entriesArray[i].nextEntry));
		iterator = getNextEntry(iterator, chunk1->entriesArray);
		++i;
	}
	assert(i == 2);
		
	markChunkFrozen(chunk1);
	
	assert(isFrozen(chunk1->entryHead.keyData) == TRUE);
	assert(isFrozen(chunk1->entryHead.nextEntry) == TRUE);
	
	i=0;
	iterator = getNextEntry(&chunk1->entryHead, chunk1->entriesArray);
	while (iterator != NULL) {
		assert(isFrozen(chunk1->entriesArray[i].keyData) == TRUE);
		assert(isFrozen(chunk1->entriesArray[i].nextEntry) == TRUE);
		iterator = getNextEntry(iterator, chunk1->entriesArray);
		++i;
	}
	assert(i == 2);
	
	free(chunk1);
	printf("PASSED\n");
}
/*****************************************************************************/
static inline void buildTestChunk(Chunk* c, uint64_t size, uint64_t startIndex) {
	assert(size > 0);
	
	c->counter = size;
	c->entryHead.nextEntry = setIndex( c->entryHead.nextEntry, 0 );
	
	//freeze head
	c->entryHead.keyData = markFrozen(c->entryHead.keyData);
	c->entryHead.nextEntry = markFrozen(c->entryHead.nextEntry);
	
	uint64_t i = 0;
	for (i=0; i<size; ++i) {
		c->entriesArray[i].keyData = combineKeyData(startIndex+i+1,(startIndex+i+1)*10);
		c->entriesArray[i].nextEntry = setIndex( c->entriesArray[i].nextEntry, i+1 );
		//freeze entry for copy/split/merge test
		c->entriesArray[i].keyData = markFrozen(c->entriesArray[i].keyData);
		c->entriesArray[i].nextEntry = markFrozen(c->entriesArray[i].nextEntry);
	}	
	
	//set last entry to END (NULL/END_INDEX)
	c->entriesArray[size-1].nextEntry = setIndex(c->entriesArray[size-1].nextEntry, END_INDEX);
	c->entriesArray[size-1].nextEntry = markFrozen(c->entriesArray[size-1].nextEntry);
	assert(getNextEntry(&(c->entriesArray[size-1]), c->entriesArray) == NULL);
	
	//set chunk freeze state
	c->mergeBuddy = combineFreezeState(c->mergeBuddy, FREEZE);
}
/*****************************************************************************/
static inline int checkChunk(Chunk* c, uint64_t size, uint64_t startIndex) {
	assert(c->counter == size);
	assert(getNextEntry(&c->entryHead, c->entriesArray) == &c->entriesArray[0]);
	
	uint64_t i = 0;
	for (i=0; i<size; ++i) {
		assert(!isFrozen(c->entriesArray[i].keyData));
		assert(c->entriesArray[i].keyData == combineKeyData(startIndex+i+1,(startIndex+i+1)*10));
		assert(!isFrozen(c->entriesArray[i].nextEntry));
		if (i < size-1) {
			assert(getNextEntry(&c->entriesArray[i], c->entriesArray) == &c->entriesArray[i+1]);
		}
		else {
			assert(getNextEntry(&c->entriesArray[i], c->entriesArray) == NULL);
		}
	}
	assert(!isFrozen(c->entryHead.keyData));
	assert(!isFrozen(c->entryHead.nextEntry));
	return 1;
}
/*****************************************************************************/
void test_copyToOneChunk() {
	printf("==== test_copyToOneChunk ======\n");
	
	Chunk* old = (Chunk*)malloc(sizeof(Chunk));
	Chunk* new1 = (Chunk*)malloc(sizeof(Chunk));
	initChunk(old);
	initChunk(new1);
	
	buildTestChunk(old,MAX_ENTRIES,0);
	assert(new1->counter == 0);
	copyToOneChunk(old,new1);
	checkChunk(new1,MAX_ENTRIES,0);
	
	free(old);
	free(new1);
	printf("PASSED\n");
}
/*****************************************************************************/
void test_mergeToOneChunk() {
	printf("==== test_mergeToOneChunk ======\n");
	Chunk* old1 = (Chunk*)malloc(sizeof(Chunk));
	Chunk* old2 = (Chunk*)malloc(sizeof(Chunk));
	Chunk* new1 = (Chunk*)malloc(sizeof(Chunk));
	initChunk(old1);
	initChunk(old2);
	initChunk(new1);
	
	buildTestChunk(old1,MIN_ENTRIES,0);
	buildTestChunk(old2,MIN_ENTRIES+1,MIN_ENTRIES);
	old1->mergeBuddy = combineFreezeState(old1->mergeBuddy, MERGE);
	old2->mergeBuddy = combineFreezeState(old2->mergeBuddy, SLAVE_FREEZE);
	mergeToOneChunk(old1,old2,new1);
	checkChunk(new1,2*MIN_ENTRIES+1,0);

	free(old1);
	free(old2);
	free(new1);	
	printf("PASSED\n");
}
/*****************************************************************************/
void test_mergeToTwoChunks(int check) {
	printf("==== test_mergeToTwoChunks%d ======\n",check);
	Chunk* old1 = (Chunk*)malloc(sizeof(Chunk));
	Chunk* old2 = (Chunk*)malloc(sizeof(Chunk));
	Chunk* new1 = (Chunk*)malloc(sizeof(Chunk));
	Chunk* new2 = (Chunk*)malloc(sizeof(Chunk));
	initChunk(old1);
	initChunk(old2);
	initChunk(new1);
	initChunk(new2);
	
	//TEST for old1->counter > old2->counter
	if (check == 0) {
		old1->counter = MAX_ENTRIES;
		old2->counter = MIN_ENTRIES;
	}
	//TEST for old1->counter < old2->counter
	if (check == 1) {
		old1->counter = MIN_ENTRIES;
		old2->counter = MAX_ENTRIES;
	}
	
	buildTestChunk(old1,old1->counter,0);
	buildTestChunk(old2,old2->counter,old1->counter);
	old1->mergeBuddy = combineFreezeState(old1->mergeBuddy, MERGE);
	old2->mergeBuddy = combineFreezeState(old2->mergeBuddy, SLAVE_FREEZE);
	mergeToTwoChunks(old1,old2,new1,new2);
	
	uint64_t medianIndex = (old1->counter + old2->counter) / 2 ;
	checkChunk(new1,medianIndex+1,0);
	
	uint64_t chunk2_size = old1->counter + old2->counter - medianIndex - 1;
	checkChunk(new2,chunk2_size,medianIndex+1);
	
	assert(new2->counter == chunk2_size);
	assert((new1->counter + new2->counter) == (old1->counter + old2->counter));
	//assert(printChunkSimple(new1));
	//assert(printChunkSimple(new2));
	
	free(old1);
	free(old2);
	free(new1);
	free(new2);	
	printf("PASSED\n");
}
/*****************************************************************************/
void test_splitIntoTwoChunks() {
	printf("==== test_splitIntoTwoChunks ======\n");	
	Chunk* old1 = (Chunk*)malloc(sizeof(Chunk));
	Chunk* new1 = (Chunk*)malloc(sizeof(Chunk));
	Chunk* new2 = (Chunk*)malloc(sizeof(Chunk));
	initChunk(old1);
	initChunk(new1);
	initChunk(new2);

	buildTestChunk(old1,MAX_ENTRIES,0);
	splitIntoTwoChunks(old1,new1,new2);

	uint64_t medianIndex = old1->counter / 2 ;
	checkChunk(new1,medianIndex,0);

	uint64_t chunk2_size = old1->counter - medianIndex;
	checkChunk(new2,chunk2_size,medianIndex);
	
	assert(new2->counter == chunk2_size);
	assert((new1->counter + new2->counter) == old1->counter);
	//assert(printChunkSimple(new1));
	//assert(printChunkSimple(new2));
	
	free(old1);
	free(new1);
	free(new2);
	printf("PASSED\n");
}
/*****************************************************************************/

	



