#ifndef __CHUNK_TEST_H_
#define __CHUNK_TEST_H_

#include "../chunk.h"
#include <stdlib.h>
#include <stdio.h>

void runAllChunkTests();
void test_allocateChunk();
void test_isSwapped();
void test_allocateEntry();
void test_combineBuddyFreezeState();
void test_markChunkFrozen();
void test_incCount_and_decCount(int times);
void test_copyToOneChunk();
void test_mergeToOneChunk();
void test_mergeToTwoChunks(int check);
void test_splitIntoTwoChunks();


#endif //__CHUNK_TEST_H_

