HOWTO use the lockfreebtree as blackbox:


cleanup:
	make clean
	
unit tests:
	make UNIT_TEST=TRUE
	./test
	
full benchmark:
	1) make clean
	2) make MAX_THREADS=32 INSERT_PERCENT=20 DELETE_PERCENT=20 SEARCH_PERCENT=60 MIN_ENTRIES=200 MAX_ENTRIES=450
		
		or simply run "make", any parameter is optional where default values are:
		
			MAX_THREADS=32
			INSERT_PERCENT=20
			DELETE_PERCENT=20
			SEARCH_PERCENT=60
			MIN_ENTRIES=200
			MAX_ENTRIES=450
			
	3) ./test num_stage1_actions num_stage2_actions
		
		or simply run "./test" defaults are:
		
			num_stage1_actions=1000
			num_stage2_actions=10000
	
	

	
HOWTO use the lockfreebtree in code:	
	you can see some examples in lockfreeBtree_test.c and main_test.c
	
	use initialize() to get a btree object 
	use freeBtree(btree) to destroy the tree
	
parallel setup with <glib.h>:

	your entrance function is start_routine which should be passed to pthread_create
	your input will define the action of the tree
	
	full example below:
/*******************************************************/	
int main() {

	Btree* btree initialize();
	
	//create some insert threads
	GThread* insertThreads[MAX_THREADS_NUM];
	Input inputs[MAX_THREADS_NUM];
	
	//setup a barrier for parralel execution
	Barrier barrier;
	barrier.cond = g_cond_new();
	barrier.lock = g_mutex_new();
	barrier.waiting = 0;
	barrier.size = MAX_THREADS_NUM;
	
	//prepare the inputs for the threads
	for (int i=0; i<barrier.size; ++i) {
		inputs[i].action = INSERT_OP; 		// action can be {INSERT_OP, INSERT_OP, DELETE_OP}
		inputs[i].threadNum = i;
		inputs[i].pdata = NULL;
		inputs[i].barrier = &barrier; 		// barrier
		inputs[i].timer = g_timer_new();	// allocate timer.
		inputs[i].btree = btree;
		
		inputs[i].key = <YOUR_KEY>;
		inputs[i].data = <YOUR_DATA>;
		inputs[i].result = FALSE;
	}
		
	//run, all created threads will wait on barrier and then run simultaneously
	for (int i=0; i<barrier.size; ++i) {
		insertThreads[i] = g_thread_create(start_routine, \
					(gpointer) &inputs[i], 1, NULL);
	}

	//join all
	for (int i=0; i<barrier.size; ++i) {
		g_thread_join(insertThreads[i]);
	}
	
	//cleanup
	for (int i=0; i<MAX_THREADS_NUM; ++i) {
		g_timer_destroy(inputs[i].timer);
	}
	
	g_mutex_free(barrier.lock);
	g_cond_free(barrier.cond);
	
	freeBtree(btree);
	
	return 0;
}
/*******************************************************/
	
	