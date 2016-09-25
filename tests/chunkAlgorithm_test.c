#include "chunkAlgorithm_test.h"
#include "entry_test.h"
#include "../lockfreeBtree.h"
#include <assert.h>
#include <time.h>
#include <glib.h>

#define NUM MAX_THREADS_NUM


/*****************************************************************************/
void runAllChunkAlgorithmTests(int times) {
	float cleanTime1 = test_MultiThreaded_Insert(times, FALSE);
	printf("total INSERT_TEST , clean_time=%lf\n\n", cleanTime1);
	float cleanTime2 = test_MultiThreaded_Insert_Delete(times,TRUE);
	printf("total MERGE_TEST , clean_time=%lf\n", cleanTime2);
	float cleanTime3 = test_MultiThreaded_Insert_Delete(times,FALSE);
	printf("total MERGE_TEST , clean_time=%lf\n", cleanTime3);	
	strong_Delete_test();
}
/*****************************************************************************/
float test_MultiThreaded_Insert(int numOfInserts, Bool randomize) {
	
	printf("howMuch = %d\n", numOfInserts);
	
	assert((numOfInserts % NUM) == 0);
	
	if (randomize) {
		printf("====== test_MultiThreaded_Insert random keys ======\n");
	}
	else {
		printf("====== test_MultiThreaded_Insert ======\n");
	}
	
	
	//timer
	float totalTime = 0;
	float maxInThisIteration = 0;
	
	//threads
	GThread* threads[NUM];
	Input inputs[NUM];
	
	//barrier
	Barrier barrier;
	barrier.cond = g_cond_new();
	barrier.lock = g_mutex_new();
	barrier.waiting = 0;
	barrier.size = NUM;

	//used for main to operate on btree
	Data data;
	ThreadGlobals tg; 
	HPRecord threadRecord;
	tg.hp0 = &(threadRecord.hp0);
	tg.hp1 = &(threadRecord.hp1);
	
	//init tree
	Btree* btree =  initialize();
	
	srand(time(NULL));
	
	int runs = numOfInserts / NUM;
	int j=0;
	
	
	//prepare input for NUM threads		
	for (int i=0; i<NUM; ++i) {
		inputs[i].action = INSERT_OP;	
		inputs[i].pdata = NULL;
		inputs[i].result = FALSE;
		inputs[i].threadNum = i;
		inputs[i].barrier = &barrier; //barrier
		inputs[i].timer = g_timer_new();
		inputs[i].btree = btree;
	}
	
	int id = 0;
	
	while (j< runs) {
				
		//prepare input for NUM threads		
		for (int i=0; i<NUM; ++i) {
			if (randomize) {
				inputs[i].key = rand()%numOfInserts + 1;
			}
			else {
				inputs[i].key = (i + 1) + NUM*j;
			}
			inputs[i].data = inputs[i].key * 100;
			inputs[i].id = id++;
		}	
		
		//run
		for (int i=0; i<NUM; ++i) {
			threads[i] = g_thread_create(start_routine, \
						(gpointer) &inputs[i], 1, NULL);
		}
		
		//join all
		for (int i=0; i<NUM; ++i) {
			g_thread_join(threads[i]);
		}
		
		//validate inserts
		if (!randomize) {
			if ((j+1)*NUM != TreeSize(btree)) {
				printf("assert failure: tree size . expected=%d, size=%d\n",(j+1)*NUM,TreeSize(btree));
				printTree(btree);
				for (int i=0; i<NUM; ++i) {
					Bool res =  searchInTree(inputs->btree, &tg, inputs[i].key, &data);
					if (!res) {
						printf("INSERT FAILURE : %d\n",inputs[i].key);
					}
				}
			}
			assert((j+1)*NUM == TreeSize(btree));
		}
		
		//analysis
		maxInThisIteration = 0;
		for (int i=0; i<NUM; ++i) {
			//find the maximum time among the threads - that is the runtime for this iteration
			float runtime = g_timer_elapsed(inputs[i].timer, NULL);
			if (runtime > maxInThisIteration) {
				maxInThisIteration = runtime;
			}
		}
		totalTime += maxInThisIteration;

	
		//search
		for (int i=0; i<NUM; ++i) {
			Bool res =  searchInTree(inputs->btree, &tg, inputs[i].key, &data);
			if (!res) {
				printTree(btree);
				printf("inputs[i].result is: %d\n", inputs[i].result);
				printf("key is: %d\n", inputs[i].key);
				Chunk* expectedLocation = findLeaf(inputs->btree, &tg, inputs[i].key);
				printChunkSimple(expectedLocation);
				printf("max: %d\n",findMaxKeyInChunk(expectedLocation));
				printf("------------------\n");
				printChunkSimple(expectedLocation->newChunk);
				if (expectedLocation->newChunk->nextChunk) {
					printChunkSimple(expectedLocation->newChunk->nextChunk);
				}
				
			}
			assert(res);
			assert ( data == inputs[i].data );
		}

		
		++j;
	}
	
	
	
	g_mutex_free(barrier.lock);
	g_cond_free(barrier.cond);


	for (int i=0; i<NUM; ++i) {
		g_timer_destroy(inputs[i].timer);
	}
	
	printf("tree size = %d\n",TreeSize(btree));
	
	
	freeBtree(btree);
	printf("PASSED\n");
	
	return totalTime;
}
/*****************************************************************************/
float test_MultiThreaded_Insert_Delete(int numOfInserts, Bool randomize) {
	assert((numOfInserts % NUM) == 0);
	
	printf("howMuch = %d\n", numOfInserts);
	if (randomize) {
		printf("====== test_MultiThreaded_Insert_Delete random keys ======\n");
	}
	else {
		printf("====== test_MultiThreaded_Insert_Delete ======\n");
	}
	
	//timer
	float totalTime = 0;
	float maxInThisIteration = 0;
	
	//insert threads
	GThread* insertThreads[NUM/2];
	Input insertInputs1[NUM/2];
	Input insertInputs2[NUM/2];
	//delete threads
	GThread* deleteThreads[NUM/2];
	Input deleteInputs[NUM/2];
	
	//barrier
	Barrier barrier;
	barrier.cond = g_cond_new();
	barrier.lock = g_mutex_new();
	barrier.waiting = 0;
	barrier.size = NUM/2;
	
	//init tree
	Btree* btree =  initialize();
	
	srand(time(NULL));
	
	int runs = numOfInserts / (NUM/2);
	int j=0;
	
	for (int i=0; i<NUM/2; ++i) {
		insertInputs1[i].timer = g_timer_new();
		insertInputs2[i].timer = g_timer_new();
		deleteInputs[i].timer = g_timer_new();
	}

	//********************* first ******************************
	//insert some keys
	for (int i=0; i<NUM/2; ++i) {
		insertInputs1[i].action = INSERT_OP;
		if (randomize) {
			insertInputs1[i].key = rand()%numOfInserts + 1;
		}
		else {
			insertInputs1[i].key = (i + 1) + NUM*j;
		}
		insertInputs1[i].data = insertInputs1[i].key * 100;
		insertInputs1[i].pdata = NULL;
		insertInputs1[i].result = FALSE;
		insertInputs1[i].threadNum = i;
		insertInputs1[i].barrier = &barrier; //barrier
		insertInputs1[i].btree = btree;
	}
	//run
	for (int i=0; i<NUM/2; ++i) {
		insertThreads[i] = g_thread_create(start_routine, \
					(gpointer) &insertInputs1[i], 1, NULL);
	}

	//join all
	for (int i=0; i<NUM/2; ++i) {
		g_thread_join(insertThreads[i]);
	}
	
	//printf("the tree after just insertions :\n");
	//printTree(btree);
	//printf("press any key to end !\n");
	//getchar();
	
	
	//analysis
	maxInThisIteration = 0;
	for (int i=0; i<NUM/2; ++i) {
		//find the maximum time among the threads - that is the runtime for this iteration
		float runtime = g_timer_elapsed(insertInputs1[i].timer, NULL);
		if (runtime > maxInThisIteration) {
			maxInThisIteration = runtime;
		}
	}
	totalTime += maxInThisIteration;
	
	

	//********************* loop ******************************
	barrier.size = NUM/2 + NUM/2; //don't change to NUM due to oddity
	//search for previously inserted keys and insert new keys
	j=1;
	Input* prevInsertInput = NULL;
	Input* curInsertInput  = NULL;
	while (j< runs) {

	
		if (j%2 == 0) {
			prevInsertInput = insertInputs2;
			curInsertInput = insertInputs1;
		}
		else {
			prevInsertInput = insertInputs1;
			curInsertInput = insertInputs2;
		}
		
		//prepare delete threads, use values from previous insert run
		for (int i=0; i<NUM/2; ++i) {
			deleteInputs[i].action = DELETE_OP;
			deleteInputs[i].key = prevInsertInput[i].key;
			deleteInputs[i].data = prevInsertInput[i].data;
			deleteInputs[i].result = FALSE;
			deleteInputs[i].threadNum = i;
			deleteInputs[i].barrier = &barrier; //barrier
			deleteInputs[i].btree = btree;
		}
		
		srand(time(NULL));
		//prepare insert threads
		for (int i=0; i<NUM/2; ++i) {
			curInsertInput[i].action = INSERT_OP;
			if (randomize) {
				curInsertInput[i].key = rand()%numOfInserts + 1;
			}
			else {
				curInsertInput[i].key = (i + 1) + NUM*j;
			}
			curInsertInput[i].data = curInsertInput[i].key * 100;
			curInsertInput[i].pdata = NULL;
			curInsertInput[i].result = FALSE;
			curInsertInput[i].threadNum = i + NUM/2;
			curInsertInput[i].barrier = &barrier; //barrier
			curInsertInput[i].btree = btree;
		}
		
		//run
		for (int i=0; i<NUM/2; ++i) {
			deleteThreads[i] = g_thread_create(start_routine, \
						(gpointer) &deleteInputs[i], 1, NULL);
			
			insertThreads[i] = g_thread_create(start_routine, \
						(gpointer) &curInsertInput[i], 1, NULL);
			
		}

		//join all
		for (int i=0; i<NUM/2; ++i) {
			g_thread_join(deleteThreads[i]);
			g_thread_join(insertThreads[i]);
		}
		
		/*
		printf("iteration %d :\n",j);
		
		for (int i=0; i<NUM/2; ++i) {
			printf("DELETE: %d %d\n", deleteInputs[i].key , deleteInputs[i].result);
		}
		for (int i=0; i<NUM/2; ++i) {
			printf("INSERT: %d %d\n", curInsertInput[i].key , curInsertInput[i].result);
		}
		*/
		//printf("\n\n\n");
		//printTree(btree);
		//printf("---------------------------------------------------------------\n");
		//printf("press any key to next iteration !\n");
		//getchar();
		
		//analysis
		maxInThisIteration = 0;
		for (int i=0; i<NUM/2; ++i) {
			//find the maximum time among the threads - that is the runtime for this iteration
			float runtime1 = g_timer_elapsed(curInsertInput[i].timer, NULL);
			if (runtime1 > maxInThisIteration) {
				maxInThisIteration = runtime1;
			}
			float runtime2 = g_timer_elapsed(deleteInputs[i].timer, NULL);
			if (runtime2 > maxInThisIteration) {
				maxInThisIteration = runtime2;
			}
		}
		totalTime += maxInThisIteration;
		
		++j;
	}

	//********************* last ******************************
	barrier.size = NUM/2;
	prevInsertInput = curInsertInput;
	for (int i=0; i<NUM/2; ++i) {
		deleteInputs[i].action = DELETE_OP;
		deleteInputs[i].key = prevInsertInput[i].key;
		deleteInputs[i].data = prevInsertInput[i].data;
		deleteInputs[i].result = FALSE;
		deleteInputs[i].threadNum = i;
		deleteInputs[i].barrier = &barrier; //barrier
		deleteInputs[i].btree = btree;
	}
	//run
	for (int i=0; i<NUM/2; ++i) {
		deleteThreads[i] = g_thread_create(start_routine, \
					(gpointer) &deleteInputs[i], 1, NULL);
	}

	
	//join all
	for (int i=0; i<NUM/2; ++i) {
		g_thread_join(deleteThreads[i]);
	}
	
	//analysis
	maxInThisIteration = 0;
	for (int i=0; i<NUM/2; ++i) {
		//find the maximum time among the threads - that is the runtime for this iteration
		float runtime = g_timer_elapsed(deleteInputs[i].timer, NULL);
		if (runtime > maxInThisIteration) {
			maxInThisIteration = runtime;
		}
	}
	totalTime += maxInThisIteration;
	
	if (randomize == FALSE) {
		int treesize = TreeSize(btree);
		if (treesize != 0) {
			printf("ERROR: tree size is: %d\n", treesize);
			printTree(btree);
		}
		assert(treesize == 0);
	}
	
	
	for (int i=0; i<NUM/2; ++i) {
		g_timer_destroy(insertInputs1[i].timer);
		g_timer_destroy(insertInputs2[i].timer);
		g_timer_destroy(deleteInputs[i].timer);
	}
	
	
	g_mutex_free(barrier.lock);
	g_cond_free(barrier.cond);
	freeBtree(btree);
	printf("PASSED\n");
	
	return totalTime;
}

/*****************************************************************************/
#define DELETE_NUM 100
void strong_Delete_test() {
	
	printf("howMuch = %d\n", DELETE_NUM);
	printf("====== strong_Delete_test (1) ======\n");
	
	//insert threads
	GThread* threads[DELETE_NUM];
	Input inputs[DELETE_NUM];
	Input dinputs[DELETE_NUM];
	
	//barrier
	Barrier barrier;
	barrier.cond = g_cond_new();
	barrier.lock = g_mutex_new();
	barrier.waiting = 0;
	barrier.size = DELETE_NUM;
	
	//init tree
	Btree* btree =  initialize();
	
	srand(time(NULL));
	
	int j=0;
	
	for (int i=0; i<DELETE_NUM; ++i) {
		inputs[i].timer = g_timer_new();
		dinputs[i].timer = g_timer_new();
	}

	//********************* first ******************************
	//insert some keys
	for (int i=0; i<DELETE_NUM; ++i) {
		inputs[i].action = INSERT_OP;
		//inputs[i].key = rand()%7 + 10*i + 1;
		inputs[i].key = i+1;
		inputs[i].data = inputs[i].key * 10;
		inputs[i].pdata = NULL;
		inputs[i].result = FALSE;
		inputs[i].threadNum = i;
		inputs[i].barrier = &barrier; //barrier
		inputs[i].btree = btree;
	}
	
	printf("====== strong_Delete_test (2) ======\n");
	
	//run
	for (int i=0; i<DELETE_NUM; ++i) {
		threads[i] = g_thread_create(start_routine, \
					(gpointer) &inputs[i], 1, NULL);
	}

	//join all
	for (int i=0; i<DELETE_NUM; ++i) {
		g_thread_join(threads[i]);
	}
	
	//printTree(btree);
	
	//printf("press any key to start deleting ...\n");
	//getchar();
	
	//check errors
	for (int i=0; i<DELETE_NUM; ++i) {
		if (inputs[i].result == FALSE) {
			printf("INSERT TEST ERROR !!\n");
			assert(FALSE);
		}
	}

	//prepare delete threads, use values from previous insert run
	for (int i=0; i<DELETE_NUM; ++i) {
		dinputs[i].action = DELETE_OP;
		dinputs[i].key = inputs[i].key;
		dinputs[i].data = inputs[i].data;
		dinputs[i].result = FALSE;
		dinputs[i].threadNum = i;
		dinputs[i].barrier = &barrier; //barrier
		dinputs[i].btree = btree;
		dinputs[i].id = i;
	}

	printf("====== strong_Delete_test (3) ======\n");
	
	//run
	for (int i=0; i<DELETE_NUM; ++i) {
		threads[i] = g_thread_create(start_routine, \
					(gpointer) &dinputs[i], 1, NULL);
	}

	//join all
	for (int i=0; i<DELETE_NUM; ++i) {
		g_thread_join(threads[i]);
	}

	printf("====== strong_Delete_test (4) : destroying tree ======\n");
	
	for (int i=0; i<DELETE_NUM; ++i) {
		g_timer_destroy(inputs[i].timer);
		g_timer_destroy(dinputs[i].timer);
	}
	
	//check errors
	Bool success = TRUE;
	for (int i=0; i<DELETE_NUM; ++i) {
		if (dinputs[i].result == FALSE) {
			printf("DELETE TEST ERROR, %d !!\n", dinputs[i].key);
			success = FALSE;
		}
	}
	
	if (!success) {
		printTree(btree);
	}
	
	//printf("press any key to end !\n");
	//getchar();
	
	if (TreeSize(btree) != 0) printTree(btree);
	assert(TreeSize(btree) == 0);
	
	
	g_mutex_free(barrier.lock);
	g_cond_free(barrier.cond);
	freeBtree(btree);
	printf("PASSED\n");
}







































