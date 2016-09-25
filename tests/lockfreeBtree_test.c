#include "lockfreeBtree_test.h"
#include <assert.h>
#include <time.h>
#include <glib.h>

#ifndef INSERT_PERCENT
#define INSERT_PERCENT 20
#endif 

#ifndef DELETE_PERCENT
#define DELETE_PERCENT 20
#endif 

#ifndef SEARCH_PERCENT
#define SEARCH_PERCENT 60
#endif 
	
/*****************************************************************************/
float stage1(Btree* btree, uint64_t N, float *max_s1_insert, float *avg_s1_insert) {

	float totalTime = 0;	
	assert(max_s1_insert);
	assert(avg_s1_insert);
	
	*max_s1_insert = 0;
	*avg_s1_insert = 0;
	
	//insert threads
	GThread* insertThreads[MAX_THREADS_NUM];
	int operations_per_thread = N/MAX_THREADS_NUM + 1;
	Input* insertInputs[MAX_THREADS_NUM];
	
	for (int i = 0; i < MAX_THREADS_NUM; ++i) {
		insertInputs[i] = (Input*)malloc(sizeof(Input) * operations_per_thread);
	}
	
	//barrier
	Barrier barrier;
	barrier.cond = g_cond_new();
	barrier.lock = g_mutex_new();
	barrier.waiting = 0;
	barrier.size = MAX_THREADS_NUM;

	srand(time(NULL));
	GTimer* t = g_timer_new();

	//prepare insert threads
	for (int i=0; i < MAX_THREADS_NUM; ++i) {
		for (int j=0; j < operations_per_thread; ++j) {
			insertInputs[i][j].action = INSERT_OP;
			insertInputs[i][j].threadNum = i;
			insertInputs[i][j].pdata = NULL;
			insertInputs[i][j].barrier = &barrier; //barrier
			insertInputs[i][j].timer = g_timer_new();	// allocate timer.
			insertInputs[i][j].btree = btree;
			insertInputs[i][j].key = (rand() % 262144)+1; //2^18 
			insertInputs[i][j].data = 1000 + insertInputs[i][j].key; 
			insertInputs[i][j].result = FALSE;
			if (i == MAX_THREADS_NUM-1) {
				insertInputs[i][j].operations_per_thread = N%operations_per_thread;
			}
			else {
				insertInputs[i][j].operations_per_thread = operations_per_thread;
			}
		}
	}
	
	g_timer_start(t);
	
	//run
	for (int i=0; i<barrier.size; ++i) {
		insertThreads[i] = g_thread_create(start_routine, \
					(gpointer)insertInputs[i], 1, NULL);
	}

	printf("Stage I: All threads were dispatched\n");
	
	//join all
	for (int i=0; i<barrier.size; ++i) {
		g_thread_join(insertThreads[i]);
	}

	g_timer_stop(t);
	
	for (int i=0; i < MAX_THREADS_NUM; ++i) {
		for (int j=0; j < insertInputs[i][0].operations_per_thread; ++j) {

			float elapsed = g_timer_elapsed(insertInputs[i][j].timer, NULL);
			*avg_s1_insert += elapsed;
			if (elapsed > *max_s1_insert) {
				*max_s1_insert = elapsed;
			}
 			g_timer_destroy(insertInputs[i][j].timer);
		}
	}
	
	g_mutex_free(barrier.lock);
	g_cond_free(barrier.cond);

	*avg_s1_insert = *avg_s1_insert / N;

	totalTime = g_timer_elapsed(t,NULL);
	g_timer_destroy(t);

	for (int i = 0; i < MAX_THREADS_NUM; ++i) {
		free(insertInputs[i]);
	}
	return totalTime;
}

/*****************************************************************************/

float stage2(Btree* btree, uint64_t N, float* max_s2_insert, float* max_s2_delete, float* max_s2_search,
	float* avg_s2_insert, float* avg_s2_delete, float* avg_s2_search ) {

	float totalTime = 0;	
	int countInsert = 0;
	int countDelete = 0;
	int countSearch = 0;

	assert(max_s2_insert);
	assert(avg_s2_insert);
	assert(max_s2_delete);
	assert(avg_s2_delete);
	assert(max_s2_search);
	assert(avg_s2_search);
	assert((INSERT_PERCENT + DELETE_PERCENT + SEARCH_PERCENT) == 100);
	assert(INSERT_PERCENT >= 0);
	assert(DELETE_PERCENT >= 0);
	assert(SEARCH_PERCENT >= 0);

	*max_s2_insert = 0;
	*avg_s2_insert = 0;
	*max_s2_delete = 0;
	*avg_s2_delete = 0;
	*max_s2_search = 0;
	*avg_s2_search = 0;

	//threads
	GThread* threads[MAX_THREADS_NUM];
	Data searchResults[MAX_THREADS_NUM];

	int operations_per_thread = N/MAX_THREADS_NUM + 1;
	Input* inputs[MAX_THREADS_NUM];

	for (int i = 0; i < MAX_THREADS_NUM; ++i) {
		inputs[i] = (Input*)malloc(sizeof(Input) * operations_per_thread);
	}

	
	//barrier
	Barrier barrier;
	barrier.cond = g_cond_new();
	barrier.lock = g_mutex_new();
	barrier.waiting = 0;
	barrier.size = MAX_THREADS_NUM;

	srand(time(NULL));
	GTimer* t = g_timer_new();

	//prepare threads
	for (int i=0; i < MAX_THREADS_NUM; ++i) {
		for (int j=0; j < operations_per_thread; ++j) {

			int actionRand = rand() % 100 + 1;
			if ((actionRand >= 1) && (actionRand <= INSERT_PERCENT)) {
				inputs[i][j].action = INSERT_OP;
			}
			else if ((actionRand > INSERT_PERCENT) && (actionRand <= (INSERT_PERCENT + DELETE_PERCENT))) {
				inputs[i][j].action = DELETE_OP;
			}
			else if ((actionRand > (INSERT_PERCENT + DELETE_PERCENT)) && (actionRand <= 100)) {
				inputs[i][j].action = SEARCH_OP;
			}

			inputs[i][j].threadNum = i;
			inputs[i][j].pdata = &searchResults[i];
			inputs[i][j].barrier = &barrier; //barrier
			inputs[i][j].timer = g_timer_new(); // allocate timer.
			inputs[i][j].btree = btree;
			inputs[i][j].key = (rand() % 262144)+1; //2^18 
			inputs[i][j].data = 1000 + inputs[i][j].key; 
			inputs[i][j].result = FALSE;
			if (i == MAX_THREADS_NUM-1) {
				inputs[i][j].operations_per_thread = N%operations_per_thread;
			}
			else {
				inputs[i][j].operations_per_thread = operations_per_thread;
			}
		}
	}
	
	g_timer_start(t);
	
	//run
	for (int i=0; i<barrier.size; ++i) {
		threads[i] = g_thread_create(start_routine, \
					(gpointer)inputs[i], 1, NULL);
	}

		printf("Stage II: All threads were dispatched\n");
	
	//join all
	for (int i=0; i<barrier.size; ++i) {
		g_thread_join(threads[i]);
	}

	g_timer_stop(t);
	
	for (int i=0; i < MAX_THREADS_NUM; ++i) {
		for (int j=0; j < operations_per_thread; ++j) {
			float elapsed = g_timer_elapsed(inputs[i][j].timer, NULL);

			switch (inputs[i][j].action) {

				case INSERT_OP:
					++countInsert;
					*avg_s2_insert += elapsed;
					if (elapsed > *max_s2_insert) {
						*max_s2_insert = elapsed;
					}
					break;

				case DELETE_OP:
					++countDelete;
					*avg_s2_delete += elapsed;
					if (elapsed > *max_s2_delete) {
						*max_s2_delete = elapsed;
					}
					break;

				case SEARCH_OP:
					++countSearch;
					*avg_s2_search += elapsed;
					if (elapsed > *max_s2_search) {
						*max_s2_search = elapsed;
					}
					break;
			}
			
 			g_timer_destroy(inputs[i][j].timer);
		}
	}

	
	g_mutex_free(barrier.lock);
	g_cond_free(barrier.cond);

	*avg_s2_insert = *avg_s2_insert / countInsert;
	*avg_s2_delete = *avg_s2_delete / countDelete;
	*avg_s2_search = *avg_s2_search / countSearch;

	totalTime = g_timer_elapsed(t,NULL);
	g_timer_destroy(t);

	for (int i = 0; i < MAX_THREADS_NUM; ++i) {
		free(inputs[i]);
	}

	return totalTime;
}

