#ifndef _LOCKFREE_BTREE_H_FILE__
#define _LOCKFREE_BTREE_H_FILE__

#include "entry.h"
#include "chunk.h"
#include "chunkAlgorithm.h"
#include "treeAlgorithm.h"
#include "globals.h"
#include <glib.h>


/*****************************************************************************
	Action : for Input use. operation that should be done by the thread.
*****************************************************************************/
typedef enum {SEARCH_OP, INSERT_OP, DELETE_OP} Action;


/*****************************************************************************
	Barrier : simple barrier
*****************************************************************************/
typedef struct Barrier_t {
	GCond* cond;
	GMutex* lock;
	int waiting;
	int size;
} Barrier;

/*****************************************************************************
	Input : for Input and Output use. 
	contains parameters which is needed to complete the operation.
*****************************************************************************/
typedef struct Input_t {
	Action action;
	int key;
	Data* pdata;	// for search
	Data data;		// for insert
	int threadNum;
	Bool result;
	Barrier* barrier;
	GTimer* timer;
	Btree* btree;
	int id;
	int operations_per_thread;
} Input;


/*****************************************************************************
	searchInTree : search for a key
	returns:
		TRUE - if found 
			data will contain value
		FALSE - the key doesn't exist
*****************************************************************************/
Bool searchInTree(Btree* btree, ThreadGlobals* tg, int key, Data *data);

/*****************************************************************************
	insertToTree : insert a new key
	returns:
		TRUE - if key was inserted succesfully
		FALSE - if the key already exists
*****************************************************************************/
Bool insertToTree(Btree* btree, ThreadGlobals* tg, int key, Data data);

/*****************************************************************************
	deleteFromTree : delete a key and associated value
	returns:
		TRUE - if the key was deleted
		FALSE - the key was not found
*****************************************************************************/
Bool deleteFromTree(Btree* btree, ThreadGlobals* tg, int key, Data data);

/*****************************************************************************
	initialize : initialize and return new tree.
*****************************************************************************/
Btree* initialize();

/*****************************************************************************
	start_routine :
	each operation will start in this function, Input argument will be passed
	to this function. Input has the Action should be done and the relevant
	parameters.
*****************************************************************************/
void* start_routine(void* arg);

/*****************************************************************************
	freeBtree :
	free all chunks and destroy tree
*****************************************************************************/
void freeBtree(Btree* btree);

#endif //_LOCKFREE_BTREE_H_FILE__
