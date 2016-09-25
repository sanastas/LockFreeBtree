#include "lockfreeBtree.h"
#include "tests/entry_test.h"
#include "tests/chunk_test.h"
#include "tests/chunkAlgorithm_test.h"
#include "tests/lockfreeBtree_test.h"
#include <assert.h>
#include <time.h>
#include <glib.h>
#include <stdio.h>


#ifndef UNITTEST
#define UNITTEST FALSE
#endif

int unitTests(int stage1_actions) {
	runAllEntryTests();
	runAllChunkTests();
	runAllChunkAlgorithmTests(stage1_actions);
}

int assertConfig() {
	//64bit architecture
	assert(sizeof(uint64_t) == 8);
	assert(sizeof(int) == 4);
	assert(BITS_IN_WORD == 64);
	//Entry index bounds for 64bit word
	assert(MAX_ENTRIES <= 1023);
	assert(MIN_ENTRIES >= 1);
	assert(MIN_ENTRIES < MAX_ENTRIES);
	assert(MAX_ENTRIES >= 2* MIN_ENTRIES +6); //algorithm requirement
	assert(MAX_ENTRIES >= 6); //algorithm requirement
	assert(MAX_ENTRIES % 2 == 0); //algorithm requirement
	assert(FALSE == 0);
	assert(TRUE == 1);
}


int main(int argc, char *argv[]) {
	assertConfig();
	
	// for gthread support
	g_thread_init(NULL);

	int stage1_actions = 1000;
	int stage2_actions = 10000;
	if (argc >= 3) {
		stage1_actions = atoi(argv[1]);
		stage2_actions = atoi(argv[2]);
	}
	
	//unit tests
	if (UNITTEST) {
		unitTests(stage1_actions);
		return 0;
	}

	float max_s1_insert = 0;
	float avg_s1_insert = 0;
	
	float max_s2_insert = 0;
	float max_s2_delete = 0;
	float max_s2_search = 0;
	
	float avg_s2_insert = 0;
	float avg_s2_delete = 0;
	float avg_s2_search = 0;	
	
	Btree* btree = initialize();
	
	float stage1_time = stage1(btree ,stage1_actions ,&max_s1_insert ,&avg_s1_insert); //run stage1	
	printf("Stage I finished\n");
	float stage2_time = stage2(btree, stage2_actions ,&max_s2_insert, &max_s2_delete,\
				&max_s2_search, &avg_s2_insert, &avg_s2_delete, &avg_s2_search); //run stage2

	freeBtree(btree);


	printf("stage1_actions: %d stage2_actions: %d Threads#: %d stage1: %f stage2: %f total: %f max_s1_insert: %f avg_s1_insert: %f max_s2_insert: %f max_s2_delete: %f max_s2_search: %f avg_s2_insert: %f avg_s2_delete: %f avg_s2_search: %f\n",\
		stage1_actions, stage2_actions, MAX_THREADS_NUM ,\
		stage1_time, stage2_time, stage1_time + stage2_time, \
		max_s1_insert, avg_s1_insert, max_s2_insert, max_s2_delete,	\
		max_s2_search, avg_s2_insert, avg_s2_delete, avg_s2_search	);


	return 0;
}

