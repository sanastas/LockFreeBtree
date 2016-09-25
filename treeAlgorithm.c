#include "treeAlgorithm.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

/*****************************************************************************
	printTree :
	print the chunk keys ordered.
*****************************************************************************/
void printTree1(Btree* btree, Chunk* head, const char* tabs) {
	if (clearFreezeState(head) == NULL) {
		return;
	}
	head = clearFreezeState(head);
	Entry* iterator = &head->entryHead;
	if (head->newChunk) {
		if (!head->newChunk->nextChunk) {
			printf("%s%p - h=%d : newChunk=%p\n",tabs,head,head->height,head->newChunk);
		} else {
			printf("%s%p - h=%d : newChunks=%p,%p\n",tabs,head,head->height,head->newChunk,head->newChunk->nextChunk);
		}
	} else {
		printf("%s%p - h=%d\n",tabs,head,head->height);
	}
	
	while ((iterator=getNextEntry(iterator, head->entriesArray)) != NULL) {
		int key = 0;
		int data = 0;
		parseKeyData(iterator->keyData, &key, &data);
		if (head->height > 0) {
			if (isDeleted(iterator->nextEntry)) {
				printf("%s%d (D)\n",tabs,getKey(iterator));
			} else {
				printf("%s%d\n",tabs,getKey(iterator));
			}
			char* tabss = (char*)malloc(strlen(tabs)+3);
			strcpy(tabss, "\t");
			strcat(tabss,tabs);
			Chunk* son = getSon(btree->memoryManager, iterator);
			printTree1(btree, son, tabss);
		} else {
			if (isDeleted(iterator->nextEntry)) {
				printf("%s%d - %d (D)\n",tabs,getKey(iterator),data);
			} else {
				printf("%s%d - %d\n",tabs,getKey(iterator),data);
			}
		}
	}
	printf("\n");
}
void printTree(Btree* btree) { printTree1(btree, btree->root, ""); }


void TreeSize1(Btree* btree, Chunk* head, int* count) {
	if (clearFreezeState(head) == NULL) {
		return;
	}
	head = clearFreezeState(head);
	Entry* iterator = &head->entryHead;
	while ((iterator=getNextEntry(iterator, head->entriesArray)) != NULL) {
		// ADDED: 03/04 by Ali.
		if (isDeleted(iterator->nextEntry)) {
			continue;
		}
		if (head->height > 0) {
			Chunk* son = getSon(btree->memoryManager, iterator);
			TreeSize1(btree, son, count);
		} else {
			*count = *count + 1;
		}
	}
}
int TreeSize(Btree* btree) { 
	int c = 0;
	TreeSize1(btree, btree->root, &c);
	return c;
}
/*****************************************************************************
	searching
*****************************************************************************/
Chunk* findLeaf(Btree* btree, ThreadGlobals* tg, int key) {
	assert(key > 0 && key <= MAX_KEY);
	Entry* CUR = NULL;
	
	Chunk* chunk = btree->root;
	while ( chunk->height != 0 ) {
		find(btree, tg, chunk, key);
		CUR = entryIn(chunk, tg->cur);
		assert(CUR != NULL);
		
		chunk = getSon(btree->memoryManager, CUR);
	}
	return chunk;
}
/*****************************************************************************/
Chunk* findParent(Btree* btree, ThreadGlobals* tg, int key, Chunk* sonChunk,
	Entry** parentEnt, Entry** neighborEnt) {
		
	assert(clearFreezeState(sonChunk) != NULL);
	assert(clearFreezeState(sonChunk) == sonChunk);
	assert(key > 0 && key <= MAX_KEY);
	
	Entry* CUR = NULL;
	Chunk* chunk = btree->root;
	Chunk* pChunk = NULL;
	
	if (sonChunk->root) {
		return NULL; //chunk is root and has no parent
	}

	while( chunk->height != 0 ) {
	
		find(btree, tg, chunk, key);
		CUR = entryIn(chunk, tg->cur);
		
		if (CUR == NULL) {
			return NULL;
		}
		
		if (sonChunk == getSon(btree->memoryManager, CUR)) {
			*parentEnt = CUR;
			
			// required for merge (need neighbor).
			if ( neighborEnt != NULL) {
				if ( tg->prev == &chunk->entryHead.nextEntry ) {
					//special case, chunk is first in the list, return right neighbor
					*neighborEnt = entryIn(chunk, tg->next);
					assert(*neighborEnt);
				}
				else {
					//regular case, return left neighbor
					*neighborEnt = entPtr(tg->prev);
					assert(*neighborEnt);
				}
			}
			
			assert(chunk); //parent found, must not be NULL
			if (extractFreezeState(chunk->mergeBuddy) == INFANT) {
				helpInfant(btree,tg,chunk);
				assert(extractFreezeState(chunk->mergeBuddy) != INFANT);
			}
			return chunk;
		}

		chunk = getSon(btree->memoryManager, CUR);
	}
	
	return NULL;	// current chunk is leaf, no parent found.
}
/*****************************************************************************
	enslave
*****************************************************************************/
Chunk* findMergeSlave(Btree* btree, ThreadGlobals* tg, Chunk* master) {
	assert(clearFreezeState(master) != NULL);
	assert(clearFreezeState(master) == master);
	assert(extractFreezeState(master->mergeBuddy) != INFANT);
	
	//should not call this function on root
	assert(master != btree->root );
	assert(master->root == FALSE);
	//make sure master is frozen when calling
	assert(isFrozen(master->entriesArray[MAX_ENTRIES-1].nextEntry));
	
	Chunk* slave = NULL;
	Chunk* oldSlave = NULL;
	Chunk* parent = NULL;
	Entry* masterEnt = NULL;
	Entry* slaveEnt = NULL;
	int masterKey = 0;
	int slaveKey = 0;
	Chunk* expState = NULL;
	
findMergeSlave_start:
	
	masterKey = getKey(getNextEntry(&master->entryHead, master->entriesArray));
	assert(masterKey > 0 && masterKey <= MAX_KEY);
	
	if ((parent = findParent(btree,tg,masterKey,master,&masterEnt,&slaveEnt)) == NULL) {
		//master is not in the btree, its merge is accomplished and its slave
		//is written in the mergeBuddy pointer
		slave = clearFreezeState(master->mergeBuddy);
		assert(slave != NULL);
		assert(clearFreezeState(master->mergeBuddy) == slave); //master points to slave
		assert(clearFreezeState(slave->mergeBuddy) == master); //slave points back to master
		assert((extractFreezeState(master->mergeBuddy) == MERGE && extractFreezeState(slave->mergeBuddy) == SLAVE_FREEZE) ||
    		(extractFreezeState(master->mergeBuddy) == SLAVE_FREEZE && extractFreezeState(slave->mergeBuddy) == MERGE));
		return slave;
	}

	slave = getSon(btree->memoryManager, slaveEnt);
	assert(slave);
	
	//set the master freeze state from <FREEZE,NULL> or from <REQUEST_SLAVE,oldSlave> 
	//(in case oldSlave was frozen) to <REQUEST_SLAVE,slave>
	if (oldSlave == NULL) {
		expState = (Chunk*)C_FREEZE;
	} else {
		expState = combineFreezeState(oldSlave, REQUEST_SLAVE);
	}
		
	if ( !CAS(&master->mergeBuddy, expState, combineFreezeState(slave, REQUEST_SLAVE)) ) {
		//master freeze state can be only REQUEST_SLAVE or MERGE
		if (extractFreezeState(master->mergeBuddy) == MERGE) {
			slave = clearFreezeState(master->mergeBuddy);
			assert(slave != NULL);
			assert(extractFreezeState(slave->mergeBuddy) == SLAVE_FREEZE); //slave expected to be in SLAVE_FREEZE
			assert(clearFreezeState(master->mergeBuddy) == slave); //master points to slave
			assert(clearFreezeState(slave->mergeBuddy) == master); //slave points back to master
			return slave;
		}
	}
	
	slave = clearFreezeState(master->mergeBuddy); // Current slave is the one pointed by mergeBuddy
	assert(slave);
	
	//check that the parent is not in frozen state and help frozen parent if needed
	if (extractFreezeState(parent->mergeBuddy) != NORMAL && oldSlave == NULL) {
		Bool result = FALSE;
		freeze(btree,tg,parent,0,0,0,NONE,&result);
		oldSlave = slave;
		goto findMergeSlave_start;
	}
	
	//set the slave freeze state from <NORMAL,NULL> to <SLAVE_FREEZE,master>
	slaveKey = getKey(getNextEntry(&slave->entryHead, slave->entriesArray));
	assert(slaveKey > 0 && slaveKey <= MAX_KEY);
	if ( !setSlave(btree, tg, master, slave, masterKey, slaveKey) ) {
		oldSlave = slave;
		goto findMergeSlave_start;
	}

	//succeded to get the slave update master
	//the following CAS may fail in special case with leftmost chunks
	CAS(&master->mergeBuddy, combineFreezeState(slave,REQUEST_SLAVE),\
							combineFreezeState(slave,MERGE));

	if (extractFreezeState(master->mergeBuddy) == MERGE) {
		assert(slave != NULL);
		assert(extractFreezeState(slave->mergeBuddy) == SLAVE_FREEZE); //slave expected to be in SLAVE_FREEZE
		assert(clearFreezeState(master->mergeBuddy) == slave); //master points to slave
		assert(clearFreezeState(slave->mergeBuddy) == master); //slave points back to master
		return slave;
	} else {
		//master became slave and expected to be in SLAVE_FREEZE
		assert(extractFreezeState(master->mergeBuddy) == SLAVE_FREEZE);
		return NULL;
	}
}
/*****************************************************************************/
Bool setSlave(Btree* btree, ThreadGlobals* tg, Chunk* master, Chunk* slave,\
int masterKey,int slaveKey) {
	
	assert(clearFreezeState(master) != NULL);
	assert(clearFreezeState(master) == master);
	assert(clearFreezeState(slave) != NULL);
	assert(clearFreezeState(slave) == slave);
	assert(masterKey > 0 && masterKey <= MAX_KEY);
	assert(slaveKey > 0 && slaveKey <= MAX_KEY);
	Bool result = FALSE;
	
	//set slave freeze state from <NORMAL,NULL> to <SLAVE_FREEZE,master>
	while ( !CAS(&slave->mergeBuddy, C_NORMAL, combineFreezeState(master, SLAVE_FREEZE)) ) {
		
		//help slave, different helps for frozen slave and infant slave
		if (extractFreezeState(slave->mergeBuddy) == INFANT) {
			helpInfant(btree,tg,slave);
			continue;
		}
		
		else if (slave->mergeBuddy == combineFreezeState(master,SLAVE_FREEZE)) {
			break; //already correctly set
		}
		
		else { //the slave is under some kind of freeze, help and look for new slave			
			//special case: two leftmost chunks try to enslave each other
			if (slave->mergeBuddy == combineFreezeState(master,REQUEST_SLAVE)) {
				if (masterKey < slaveKey) {
					// Current master node is left sibling and should become a slave
					return CAS(&master->mergeBuddy, combineFreezeState(slave,REQUEST_SLAVE), \
													combineFreezeState(slave,SLAVE_FREEZE));
				} else {
					// Current master node is right sibling and the other node should become a slave
					return CAS(&slave->mergeBuddy, combineFreezeState(master,REQUEST_SLAVE), \
													combineFreezeState(master,SLAVE_FREEZE));
				}
			}// end case of two leftmost nodes trying to enslave each other

			freeze(btree,tg,slave,0,0,(uint64_t)master,ENSLAVE,&result);
			return FALSE;
		} //end of investigating the enslaving failure
	}

	//succeded to get the slave - freeze it
	markChunkFrozen(slave);
	stabilizeChunk(btree,tg,slave);
	assert(extractFreezeState(slave->mergeBuddy) == SLAVE_FREEZE);
	return TRUE;
}
/*****************************************************************************
	balancing
*****************************************************************************/
void split(Btree* btree, ThreadGlobals* tg, int sepKey, Chunk* chunk) {
	assert(clearFreezeState(chunk) != NULL);
	assert(clearFreezeState(chunk) == chunk);
	assert(sepKey > 0 && sepKey <= MAX_KEY);
	
	Entry* chunkEnt = NULL;
	Chunk* newLow = chunk->newChunk;
	Chunk* newHigh = chunk->newChunk->nextChunk;
	Chunk* parent = NULL;
	
	assert(newLow != NULL);
	assert(newHigh != NULL);
	
	
	if (( parent = findParent(btree,tg,sepKey,chunk,&chunkEnt,NULL) ) != NULL) {
		// Can only fail if someone else completes it before we do
		insertToChunk(btree,tg,parent,sepKey,getChunkIndex(btree->memoryManager, newLow));
	}

	chunkEnt = NULL;
	int maxKey = findMaxKeyInChunk(chunk);
	if (( parent = findParent(btree,tg,maxKey,chunk,&chunkEnt,NULL) ) != NULL) {

		int entKey = getKey(chunkEnt);
		assert(entKey > 0 && entKey <= MAX_KEY);
		
		// Can only fail if someone else completes it before we do
		replaceInChunk(btree, tg, parent, entKey, \
			combineKeyData(entKey, getChunkIndex(btree->memoryManager,chunk)), \
			combineKeyData(entKey, getChunkIndex(btree->memoryManager,newHigh)));
	}
	
	//insertToChunk and replaceInChunk can only fail if someone else completes the task
	//before we do, which is fine. Now update the states of the new chunks from 
	// INFANT to NORMAL
	CAS(&newLow->mergeBuddy, C_INFANT, C_NORMAL);
	CAS(&newHigh->mergeBuddy, C_INFANT, C_NORMAL);
}
/*****************************************************************************/
void merge(Btree* btree, ThreadGlobals* tg, Chunk* master) {
	
	assert(clearFreezeState(master) != NULL);
	assert(clearFreezeState(master) == master);
	
	Chunk* newChunk = master->newChunk;
	Chunk* slave = clearFreezeState(master->mergeBuddy);
	assert(clearFreezeState(slave->mergeBuddy) == master);
	assert(master->root == FALSE);
	assert(slave->root == FALSE);
	
	// master freezeState is MERGE, slave freezeState is SLAVE_FREEZE
	assert((extractFreezeState(master->mergeBuddy) == MERGE && \
			extractFreezeState(slave->mergeBuddy) == SLAVE_FREEZE));
	
	int maxMasterKey = findMaxKeyInChunk(master);
	int maxSlaveKey = findMaxKeyInChunk(slave);
	
	int highKey = 0;
	int lowKey = 0;
	
	Chunk* highChunk = NULL;
	Chunk* lowChunk = NULL;

	Chunk* highParent = NULL;
	Chunk* lowParent = NULL;
	
	// find low and high keys among master and slave
	if (maxSlaveKey < maxMasterKey) { 
		highKey = maxMasterKey;
		highChunk = master;
		lowKey = maxSlaveKey;
		lowChunk = slave;
	} 
	else { 
		highKey = maxSlaveKey;
		highChunk = slave;
		lowKey = maxMasterKey;
		lowChunk = master; 
	}
	
	Entry* highEnt = NULL;
	if ((highParent = findParent(btree, tg, highKey, highChunk, &highEnt, NULL)) != NULL) {		
		int highEntKey = getKey(highEnt);
		// swap the highest key entry to point on newChunk
		// If replacing fails, the parent chunk was updated by a helper
		replaceInChunk(btree, tg, highParent, highEntKey,
			combineKeyData(highEntKey, getChunkIndex(btree->memoryManager, highChunk)),
			combineKeyData(highEntKey, getChunkIndex(btree->memoryManager, newChunk)));
	}
	
	Entry* lowEnt = NULL;
	if ((lowParent = findParent(btree, tg, lowKey, lowChunk, &lowEnt, NULL)) != NULL) {
		int lowEntKey = getKey(lowEnt);
		int lowChunkIndex = getChunkIndex(btree->memoryManager, lowChunk);
		if (lowParent->root) {
			//possibleNewRoot is master->newChunk
			Chunk* possibleNewRoot = master->newChunk;
			mergeRoot(btree, tg, lowParent, possibleNewRoot, lowEntKey, lowChunkIndex, master );
		}
		else {
			deleteInChunk(btree, tg, lowParent, lowEntKey, lowChunkIndex);
			// if already deleted, continue anyway
		}
	}
	// if lowChunk can no longer be found on the tree,
	// then the merge was completed (by someone else).
	
	// try to update the news on state to NORMAL from INFANT
	CAS(&(newChunk->mergeBuddy), C_INFANT, C_NORMAL);
}
/*****************************************************************************/
void borrow(Btree* btree, ThreadGlobals* tg, Chunk* master, int sepKey) {

	assert(clearFreezeState(master) != NULL);
	assert(clearFreezeState(master) == master);
	assert(sepKey > 0 && sepKey <= MAX_KEY);

	Chunk* newLow = master->newChunk;
	assert(newLow != NULL);
	Chunk* newHigh = master->newChunk->nextChunk;
	
	Chunk* slave = clearFreezeState(master->mergeBuddy);
	assert(clearFreezeState(slave->mergeBuddy) == master);
	assert(master->root == FALSE);
	assert(slave->root == FALSE);
	
	// master freezeState is MERGE, slave freezeState is SLAVE_FREEZE
	assert((extractFreezeState(master->mergeBuddy) == MERGE && \
			extractFreezeState(slave->mergeBuddy) == SLAVE_FREEZE));
	
	int maxMasterKey = findMaxKeyInChunk(master);
	int maxSlaveKey = findMaxKeyInChunk(slave);
	
	int highKey = 0;
	int lowKey = 0;
	
	int highEntKey = 0;
	int lowEntKey = 0;
	
	Chunk* highChunk = NULL;
	Chunk* lowChunk = NULL;
	Entry* highEnt = NULL;	
	Entry* lowEnt = NULL;
	
	Chunk* sonForInsert = NULL;
	Chunk* insertParent = NULL;
	Chunk* highParent = NULL;
	Chunk* lowParent = NULL;
	Entry* ent = NULL;
	
	// find low and high keys among master and slave
	if (maxSlaveKey < maxMasterKey) { 
		highKey = maxMasterKey;
		highChunk = master;
		lowKey = maxSlaveKey;
		lowChunk = slave;
	} 
	else { 
		highKey = maxSlaveKey;
		highChunk = slave;
		lowKey = maxMasterKey;
		lowChunk = master; 
	}

	if (lowKey < sepKey) { //sepKey located on the higher old chunk
		sonForInsert = highChunk;
	} else { //sepKey located on the lower old chunk
		sonForInsert = lowChunk;
	}

	if ((insertParent = findParent(btree,tg,sepKey,sonForInsert,&ent,NULL)) != NULL) {
		insertToChunk(btree,tg,insertParent,sepKey,getChunkIndex(btree->memoryManager, newLow));
	} //end of dealing with new low chunk insert

	if ((highParent = findParent(btree,tg,highKey,highChunk,&highEnt,NULL)) != NULL) {
		assert(highEnt);
		highEntKey = getKey(highEnt);
		replaceInChunk(btree, tg, highParent, highEntKey,
			combineKeyData(highEntKey, getChunkIndex(btree->memoryManager, highChunk)),
			combineKeyData(highEntKey, getChunkIndex(btree->memoryManager, newHigh)));
	} //end of dealing with new high chunk pointer replace
	
	if ((lowParent = findParent(btree,tg,lowKey,lowChunk,&lowEnt,NULL)) != NULL) {
		assert(lowEnt);
		lowEntKey = getKey(lowEnt);
		deleteInChunk(btree,tg,lowParent,lowEntKey,getChunkIndex(btree->memoryManager, lowChunk));
	} //end of dealing with old low chunk deletion

	// try to update the new sons states to NORMAL from INFANT
	CAS( &newLow->mergeBuddy, C_INFANT, C_NORMAL);
	CAS( &newHigh->mergeBuddy, C_INFANT, C_NORMAL);
}
/*****************************************************************************/
void splitRoot(Btree* btree, ThreadGlobals* tg, Chunk* root, int sepKey) {
	assert(clearFreezeState(root) != NULL);
	assert(clearFreezeState(root) == root);
	assert(sepKey > 0 && sepKey <= MAX_KEY);
	
	Chunk* newLow = root->newChunk;
	Chunk* newHigh = newLow->nextChunk;
	Chunk* newRoot = &btree->memoryManager->memoryPool[allocateChunk(btree->memoryManager)];
	
	assert(extractFreezeState(newRoot->mergeBuddy) == INFANT);
	
	//update newRoot fields
	newRoot->mergeBuddy = (Chunk*)C_NORMAL;
	newRoot->root = TRUE;
	newRoot->height = root->height +1;
	
	//Construct new root with old root's new sons.
	//Son with higher keys is pointed with INF key entry
	addRootSons(btree, newRoot, sepKey, newLow, MAX_KEY, newHigh);
	
	//try to swap the old root pointer to the new
	CAS(&btree->root, root, newRoot);
	
	//if CAS is unsuccessful, then old root's sons were inserted under other new root
	//this new root will be freed by Garabage collector.
	//Try to update the new sons's freeze state to NORMAL from INFANT
	CAS(&newLow->mergeBuddy, C_INFANT, C_NORMAL);
	CAS(&newHigh->mergeBuddy, C_INFANT, C_NORMAL);
}
/*****************************************************************************/
void mergeRoot(Btree* btree, ThreadGlobals* tg, Chunk* root, Chunk* possibleNewRoot, int key, Data data, Chunk* master) {
	assert(clearFreezeState(root) != NULL);
	assert(clearFreezeState(root) == root);
	assert(clearFreezeState(possibleNewRoot) != NULL);
	assert(clearFreezeState(possibleNewRoot) == possibleNewRoot);
	assert(key > 0 && key <= MAX_KEY);
	
	
	uint64_t rootEntNum = getEntrNum(root);
	if (rootEntNum > 2) {
		//no need to update root
		deleteInChunk(btree, tg, root, key, data);
		return;
	}
	
	// rootEntNum is 2 here, 1st entry points to the frozen chunk
	// 2nd on infant new root

	Chunk* creator = possibleNewRoot->creator;
	
	assert(rootEntNum == 2);

	Entry* entry1 = getNextEntry(&root->entryHead, root->entriesArray);
	Entry* entry2 = getNextEntry(entry1, root->entriesArray);
	
	assert(entry1);
	assert(entry2);
	
	Chunk* son1 = getSon(btree->memoryManager, entry1);
	Chunk* son2 = getSon(btree->memoryManager, entry2);
	
	assert(son1);
	assert(son2);
	
	//check that root is not trying to merge with one of its sons
	if (!( (master == son1 && possibleNewRoot == son2) || (master == son2 && possibleNewRoot == son1)) ) {
		Chunk* slave = clearFreezeState(master->mergeBuddy);
		if (!( (slave == son1 && possibleNewRoot == son2) || (slave == son2 && possibleNewRoot == son1)) ) {
			return;
		}
	}
	possibleNewRoot->root = TRUE;
	CAS(&(btree->root), root, possibleNewRoot);
	// if CAS is unsuccessful, then old root was changed by someone else
	assert(btree->root != root);
}
/*****************************************************************************
	synchronizing main structure
*****************************************************************************/
void callForUpdate(Btree* btree, ThreadGlobals* tg, RecoveryType recovType, Chunk* chunk, int sepKey) {
	assert(clearFreezeState(chunk) != NULL);
	assert(clearFreezeState(chunk) == chunk);
	assert(sepKey > 0 && sepKey <= MAX_KEY);
	
	Chunk* newLow = chunk->newChunk;
	assert(newLow != NULL);
	assert(chunk->newChunk != NULL);
	Chunk* newHigh = chunk->newChunk->nextChunk;
	
	Entry* chunkEnt = NULL;
	Chunk* parent = NULL;
	
	switch ( recovType ) {
		case R_COPY:
			assert(extractFreezeState(chunk->mergeBuddy) == COPY);
			assert(clearFreezeState(chunk->mergeBuddy) == NULL); //no mergeBuddy for COPY
			
			if (chunk->root) {
				assert(newLow->root); //make sure root flag set
				CAS(&btree->root, chunk, newLow);
			}
			else {
				if (( parent = findParent(btree,tg,findMaxKeyInChunk(chunk),chunk,&chunkEnt,NULL) ) != NULL) {
					assert(chunkEnt != NULL);
					int chunkEntKey = getKey(chunkEnt);
					replaceInChunk(btree, tg, parent, chunkEntKey, \
						combineKeyData(chunkEntKey ,getChunkIndex(btree->memoryManager, chunk)), \
						combineKeyData(chunkEntKey ,getChunkIndex(btree->memoryManager, newLow)));
						
				}
			}
			
			CAS(&newLow->mergeBuddy, C_INFANT, C_NORMAL);
			return;
		
		case R_SPLIT:
			assert(extractFreezeState(chunk->mergeBuddy) == SPLIT);
			assert(clearFreezeState(chunk->mergeBuddy) == NULL); //no mergeBuddy for SPLIT
			
			if (chunk->root) {
				splitRoot(btree, tg, chunk, sepKey);
			}
			else {
				split(btree, tg, sepKey, chunk);
			}
			return;
			
		case R_MERGE:
			assert(extractFreezeState(chunk->mergeBuddy) == MERGE);
			assert(clearFreezeState(chunk->mergeBuddy) != NULL); //should be mergeBuddy for MERGE
			
			if ( newHigh == NULL ) {
				assert(chunk->root == FALSE);
				merge( btree, tg, chunk);
			}
			else {
				assert(chunk->root == FALSE);
				borrow( btree, tg, chunk, sepKey );
			}
			
			return;
		
		default:
			assert(FALSE); //recovType must be known at this point
			break;
	}
}
/*****************************************************************************/
void helpInfant(Btree* btree, ThreadGlobals* tg, Chunk* chunk) {
	assert(clearFreezeState(chunk) != NULL);
	assert(clearFreezeState(chunk) == chunk);

	Chunk* creator = chunk->creator;
	assert(creator != NULL); //should have creator
	FreezeState creatorFrSt = extractFreezeState(creator->mergeBuddy);
	
	Chunk* newLow = creator->newChunk;
	Chunk* newHigh = newLow->nextChunk;
	assert(newLow);
	
	
	Entry* crEnt = NULL;
	Chunk* parent = NULL;
	int crEntkey = 0;
	int sepKey = 0;
	
	switch ( creatorFrSt ) {
	
		case COPY:
			CAS(&chunk->mergeBuddy , C_INFANT, C_NORMAL);
			assert(extractFreezeState(chunk->mergeBuddy) != INFANT);
			return;
		
		case SPLIT:
			parent = findParent(btree, tg, findMaxKeyInChunk(creator), creator, &crEnt, NULL);
			if (parent != NULL) {
				crEntkey = getKey(crEnt);
				replaceInChunk( btree, tg, parent, crEntkey,  \
					combineKeyData( crEntkey, getChunkIndex(btree->memoryManager, creator)), \
					combineKeyData( crEntkey, getChunkIndex(btree->memoryManager, newHigh)));
			}
						
			CAS(&newLow->mergeBuddy, C_INFANT, C_NORMAL);
			CAS(&newHigh->mergeBuddy, C_INFANT, C_NORMAL);
			assert(extractFreezeState(chunk->mergeBuddy) != INFANT);
			return;
			
		case MERGE:
			if (newHigh == NULL) {
				assert(creator->root == FALSE);
				merge(btree, tg, creator);
			}
			else {
				if (extractFreezeState(newLow->mergeBuddy) != INFANT) {
					CAS(&newHigh->mergeBuddy, C_INFANT, C_NORMAL);
				} 
				else {
					sepKey = findMaxKeyInChunk(newLow);
					assert(creator->root == FALSE);
					borrow(btree, tg, creator, sepKey);
				}
			}
			assert(extractFreezeState(chunk->mergeBuddy) != INFANT);
			return;
			
		default:
			// we will never get here.
			// since creatorFrSt can be only COPY, SPLIT or MERGE
			printf("creatorFrSt: %d\n", creatorFrSt);
			assert(FALSE);
			break;
	}

}
/*****************************************************************************
	addRootSons :
	Construct new root with old root's new sons.
	Son with higher keys is pointed with INF key entry
*****************************************************************************/
void addRootSons(Btree* btree, Chunk* newRoot, int sepKey1, Chunk* newLow, int sepKey2, Chunk* newHigh) {
	assert(clearFreezeState(newRoot) != NULL);
	assert(clearFreezeState(newRoot) == newRoot);
	assert(clearFreezeState(newLow) != NULL);
	assert(clearFreezeState(newLow) == newLow);
	assert(clearFreezeState(newHigh) != NULL);
	assert(clearFreezeState(newHigh) == newHigh);
	assert(sepKey1 > 0 && sepKey1 <= MAX_KEY);
	assert(sepKey2 > 0 && sepKey2 <= MAX_KEY);
	
	allocateEntry(newRoot, sepKey1, getChunkIndex(btree->memoryManager, newLow));
	allocateEntry(newRoot, sepKey2, getChunkIndex(btree->memoryManager, newHigh));

	newRoot->entryHead.nextEntry = \
		setIndex(newRoot->entryHead.nextEntry, 0);
	newRoot->entriesArray[0].nextEntry = \
		setIndex(newRoot->entriesArray[0].nextEntry, 1);
	newRoot->counter = 2;
	assert(getEntrNum(newRoot) == 2);
}
