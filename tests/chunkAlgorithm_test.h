#ifndef __CHUNL_ALGORITHM_TEST_H_FILE_
#define __CHUNL_ALGORITHM_TEST_H_FILE_

#include "../entry.h"
#include "../chunkAlgorithm.h"
#include "../chunk.h"
#include <stdlib.h>
#include <stdio.h>

void runAllChunkAlgorithmTests(int times);
float test_MultiThreaded_Insert(int numOfInserts, Bool randomize);
float test_MultiThreaded_Insert_Delete(int numOfInserts, Bool randomize);
void strong_Delete_test();

#endif //__CHUNL_ALGORITHM_TEST_H_FILE_
