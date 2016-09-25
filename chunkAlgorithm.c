#include "chunkAlgorithm.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/*****************************************************************************
							ALGORITHM FUNCTIONS
*****************************************************************************/
Bool searchInChunk(Btree* btree, ThreadGlobals* tg, Chunk* chunk, int key, Data *data) {
	
	assert(clearFreezeState(chunk) != NULL);
	assert(clearFreezeState(chunk) == chunk);
	assert(key > 0);
	assert(data != NULL);
	assert(validateChunk(chunk));

	Bool result = FALSE;
	if (find(btree, tg, chunk, key)) {
		Entry* CUR = entryIn(chunk, tg->cur);
		assert(CUR != NULL);
		*data = getData(CUR);
		result = TRUE; 
	} 
	
	*(tg->hp0) = NULL;
	*(tg->hp1) = NULL;
	return result;
}
/*****************************************************************************/
Bool insertToChunk(Btree* btree, ThreadGlobals* tg, Chunk* chunk, int key, Data data) {

	assert(clearFreezeState(chunk) != NULL);
	assert(clearFreezeState(chunk) == chunk);
	assert(key > 0);
	assert(validateChunk(chunk));

	Bool result = FALSE;
   
	//Find an available entry
	Entry* current = allocateEntry(chunk,key,data);
	
	while (current == NULL) {
		// No available entry.Freeze and try again
		uint64_t data64bit = LOW_32_MASK & data;
		
		chunk = freeze(btree, tg, chunk, key, 0, data64bit, INSERT, &result);
		if (chunk == NULL) {
		   // Freeze completed the insertion.
			return result;
		}
		
		//help infant chunk
		if (extractFreezeState(chunk->mergeBuddy) == INFANT) {
			helpInfant(btree,tg,chunk);
		}
		
		// Otherwise, retry allocation
		current = allocateEntry(chunk,key,data);
	}
   
	ReturnCode returnCode = insertEntry(btree, tg, chunk, current, key);
   
	switch (returnCode) {
	   case SUCCESS_THIS:
		   // Increments the counter of entries in the chunk
		   incCount(chunk);
		   result = TRUE;
		   break;
		   
	   case SUCCESS_OTHER:
		   // Entry was inserted by other thread due to help in freeze
		   result = TRUE;
		   break;
		   
	   case SUCCESS_DELAYED_INSERT:
		   // Entry was inserted by other thread and then deleted (delayed insert)
		   result = TRUE;
		   break;
		   
	   case EXISTED:
		   // This key exists - attempt to clear the entry
		   if (clearEntry(btree, tg, chunk, current)) {
			   result = FALSE;
		   }
		   else {
			   // Failure to clear the entry implies 
			   // a freeze thread that eventually inserts the entry
			   result = TRUE;
		   }
		   break;
		   
	} //end of switch

	// Clear all hazard pointers and return
	*(tg->hp0) = NULL;
	*(tg->hp1) = NULL;

	return result;
}
/*****************************************************************************/
Bool deleteInChunk(Btree* btree, ThreadGlobals* tg, Chunk* chunk,int key, Data expectedData) {

	assert(clearFreezeState(chunk) != NULL);
	assert(clearFreezeState(chunk) == chunk);
	assert(key > 0);
	assert(validateChunk(chunk));

	Bool result = FALSE;

restart_deleteInChunk:

	while (!decCount(chunk)) {

		assert(extractFreezeState(chunk->mergeBuddy) != INFANT);
		
		// If too few entries in chunk; call freeze
		chunk = freeze(btree, tg, chunk, key, 0, expectedData, DELETE, &result);
		if (chunk == NULL ) {
			// If Freeze succeeded to proceed with deletion, return
			return result;
		}
	} // end of decrement counter while

	while (TRUE) {
		
		if (!find(btree, tg, chunk, key)) {
			incCount(chunk);
			*(tg->hp0) = *(tg->hp1) = NULL;
			return FALSE;
		}
		
		Entry* CUR = entryIn(chunk, tg->cur);
		assert(CUR != NULL);
		
		// Mark entry as deleted, assume entry is not deleted or frozen
		uint64_t clearedNext = clearFrozen(clearDeleted(CUR->nextEntry));

		//deal with delayed merge threads
		//only applied to non leafs
		if (chunk->height != 0) {
			assert(CUR != NULL);
			Data index = getData(CUR);
			if (index != expectedData) {
				return TRUE; //someone else deleted the entry
			}
		}

		
		if (!CAS(&(CUR->nextEntry), clearedNext, incVersion(markDeleted(clearedNext))) ) {
		
			if (isFrozen(CUR->nextEntry)) {
				incCount(chunk);
				chunk = freeze(btree, tg, chunk, key, 0, expectedData, DELETE, &result);
				if (chunk == NULL) {
					return result;
				}
				goto restart_deleteInChunk;
			} else {
				continue;
			}
		}

		uint64_t new1 = setIndex ( incVersion( tg->cur ) , getIndex(tg->next) );
		//if prev was frozen keep it frozen
		if (isFrozen(tg->cur)) {
			new1 = markFrozen(new1); //new1 will replace prev
		}
		
		if ( CAS(tg->prev, tg->cur, new1)) {
			retireEntry(btree, tg, CUR);
		} else {
			find(btree, tg, chunk, key);
		}

		*(tg->hp0) = *(tg->hp1) = NULL;
		return TRUE;
	}

}
/*****************************************************************************/
ReturnCode insertEntry(Btree* btree, ThreadGlobals* tg, Chunk* chunk, Entry* entry, int key) {
	
	assert(clearFreezeState(chunk) != NULL);
	assert(clearFreezeState(chunk) == chunk);
	assert(key > 0);
	assert(entry != NULL);
	assert(validateChunk(chunk));
	
	Entry* CUR = NULL;
	while (TRUE) {
		uint64_t savedNext = entry->nextEntry;
		//should never insert deleted entry
		assert(isDeleted(savedNext) == FALSE);
		// Find insert location and pointers to previous and current entries
		if (find(btree, tg, chunk, key)) {
			CUR = entryIn(chunk, tg->cur);
			assert(CUR != NULL);
			//This key existed in the list, cur is global initiated by Find
			if (entry == CUR) {
				return SUCCESS_OTHER;
			} else {
				return EXISTED;
			}
		}
		CUR = entryIn(chunk, tg->cur);
		
		// Attempt setting next field	(forward link)
		uint64_t new1 = setIndex ( incVersion( savedNext ) , getIndex(tg->cur));
		if (!CAS(&(entry->nextEntry), savedNext, new1)) {
			continue;
		}
		
		
		// don't add frozen chunk to Btree.	(happens with delayed insert)
		if (chunk->height != 0) { //only for non leaf
			assert(entry != NULL);
			Data chunkIndex = getData(entry);
			assert(chunkIndex>=0 && chunkIndex<MAX_CHUNKS);
		
			FreezeState fs = extractFreezeState(btree->memoryManager->memoryPool[chunkIndex].mergeBuddy);
			if ( (fs != INFANT) && (fs != NORMAL)) {			
				return SUCCESS_DELAYED_INSERT; //someone else completed the insert
			}
			
			assert(fs == INFANT || fs ==NORMAL); //allow only infant node to be inserted
		}
		
		
		// Attempt linking into the list	(backward link)
		int entryPos = getPosition(entry, chunk->entriesArray);
		uint64_t new2 = setIndex ( incVersion( tg->cur ) , entryPos );
		
		if (!CAS(tg->prev, tg->cur ,new2)) {
			continue;
		}
		return SUCCESS_THIS;	// both CASes were successful
	}
	
	assert(FALSE); //unreachable
}
/*****************************************************************************/
Bool find(Btree* btree, ThreadGlobals* tg, Chunk* chunk, int key) {

	assert(clearFreezeState(chunk) != NULL);
	assert(clearFreezeState(chunk) == chunk);
	assert(tg);
	assert(key > 0);
	assert(validateChunk(chunk));
	
	Entry* CUR = NULL;
	
	uint64_t new1 = 0;
	
restart_find:

	tg->prev = &(chunk->entryHead.nextEntry);	// Restart pointer
	tg->cur = *tg->prev;
	CUR = entryIn(chunk, tg->cur);
	assert(CUR || chunk->root);
	
	if ( *tg->prev != tg->cur ) {
		goto restart_find;
	}
	
	while (CUR != NULL) {
		// Progress to an unprotected entry
		*(tg->hp0) = CUR;

		if ( *tg->prev != tg->cur ) {
			goto restart_find; // Validate progress after protecting
		}

		tg->next = CUR->nextEntry;
		
		if (isDeleted(tg->next)) { // Current entry is marked deleted
			// Disconnect current: prev gets the value of next with the delete bit cleared
			new1 = clearDeleted( setIndex(incVersion(tg->cur), getIndex(tg->next)) );
			
			if (isFrozen(tg->cur)) {
				//new1 replaces prev_nextEntry; save freeze bit
				new1 = markFrozen(new1);
			}
			
			if (!CAS(tg->prev, tg->cur, new1)) {
				goto restart_find;
			}
			
			retireEntry(btree, tg, CUR); //CAS succeeded - try to reclaim
			
			tg->cur = clearDeleted(tg->next);
			CUR = entryIn(chunk, tg->cur);
			
		} else {
			int ckey = getKey(CUR);
			
			if ( *tg->prev != tg->cur ) {
				goto restart_find; // Validate progress after protecting
			}
			
			if (ckey >= key) {
				return (ckey == key);
			}
						
			tg->prev = &(CUR->nextEntry);
			
			// All private. hp0,hp1 are ptrs to hazard ptrs
			Entry* tmp;
			tmp = *(tg->hp0);
			*(tg->hp0) = *(tg->hp1);
			*(tg->hp1) = tmp;

			tg->cur = tg->next;
			CUR = entryIn(chunk, tg->cur);
			
		}//end of else

	}//end of while
	
	return FALSE;
}
/*****************************************************************************/
Bool replaceInChunk(Btree* btree, ThreadGlobals* tg, Chunk* chunk, \
			int key, uint64_t expected, uint64_t new) {
				
	assert(clearFreezeState(chunk) != NULL);
	assert(clearFreezeState(chunk) == chunk);
	assert(isFrozen(expected) == FALSE);
	assert(key == ((expected & LOW_32_MASK) >> 1));
	
	Entry* CUR = NULL;
	
	while(TRUE) {
		
		if( !find(btree, tg, chunk, key) ) {
			*(tg->hp0) = *(tg->hp1) = NULL;
			return FALSE;
		}
		CUR = entryIn(chunk, tg->cur);
		assert(CUR != NULL);
		Bool casResult = FALSE;
		
		if (!CAS(&(CUR->keyData), expected, new)) {
			
			// assume no freeze bit set in exp - no swap on frozen entry
			if ( isFrozen(CUR->keyData) ) {
				// if CAS failed due to freeze help in freeze and try again
				Bool result = FALSE;
				
				chunk = freeze(btree, tg, chunk, key, expected, new, SWAP, &result);
				if ( chunk == NULL ) {
					*(tg->hp0) = *(tg->hp1) = NULL;
					return result; 
				}
				continue;
			}
			*(tg->hp0) = *(tg->hp1) = NULL;
			return FALSE; // CAS failed due to not expected data
			
		}
		
		*(tg->hp0) = *(tg->hp1) = NULL;
		return TRUE;
	}
}
/*****************************************************************************/
Chunk* freeze(Btree* btree, ThreadGlobals* tg, Chunk* chunk, uint64_t key, 
uint64_t expected, uint64_t data, FreezeTriggerType trigger, Bool* result) {
	assert(clearFreezeState(chunk) != NULL);
	assert(clearFreezeState(chunk) == chunk);
	assert(result != NULL);
	assert(key >= 0);
	assert(validateChunk(chunk));
	assert(extractFreezeState(chunk->mergeBuddy) != INFANT);

	Chunk* mergePartner = NULL;
	RecoveryType decision = -1;
	
	CAS(&(chunk->mergeBuddy), C_NORMAL, C_FREEZE);
	//At this point,the freeze state must NOT be NORMAL or INFANT

	switch (extractFreezeState(chunk->mergeBuddy)) {
		
		case COPY:
			decision = R_COPY;
			break;
			
		case SPLIT:
			decision = R_SPLIT;
			break;
			
		case MERGE: //mergePartner is already set
			decision = R_MERGE;
			mergePartner = clearFreezeState(chunk->mergeBuddy);
			//master expected to be in MERGE, slave in SLAVE_FREEZE and pointers correctly set
			assert(extractFreezeState(mergePartner->mergeBuddy) == SLAVE_FREEZE);
			assert(clearFreezeState(chunk->mergeBuddy) == mergePartner);
			assert(clearFreezeState(mergePartner->mergeBuddy) == chunk);
			break;

		case REQUEST_SLAVE: //mergePartner unknown
			decision = R_MERGE;
			mergePartner = findMergeSlave(btree, tg, chunk);
			if (mergePartner != NULL) {
				//pointers correctly set
				assert(clearFreezeState(chunk->mergeBuddy) == mergePartner);
				assert(clearFreezeState(mergePartner->mergeBuddy) == chunk);
				break;
			}
			//if partner is NULL chuck was turned to SLAVE_FREEZE, continue
			assert(extractFreezeState(chunk->mergeBuddy) == SLAVE_FREEZE);
			//continue to case SLAVE_FREEZE, no break
			
		case SLAVE_FREEZE: //mergePartner is already set
			decision = R_MERGE;
			mergePartner = clearFreezeState(chunk->mergeBuddy);
			assert(mergePartner != NULL);
			
			markChunkFrozen(chunk);
			stabilizeChunk(btree,tg,chunk);
			
			//slave is set, change master to MERGE and recover
			CAS(&mergePartner->mergeBuddy, combineFreezeState(chunk, REQUEST_SLAVE),\
											combineFreezeState(chunk, MERGE));		
			//master expected to be in MERGE, slave in SLAVE_FREEZE and pointers correctly set
			
			if (extractFreezeState(mergePartner->mergeBuddy) != MERGE) {
				printf("### chunk=%p mergePartner=%p mergePartner->mergeBuddy=%p\n",chunk,mergePartner,mergePartner->mergeBuddy);
			}
			
			assert(extractFreezeState(mergePartner->mergeBuddy) == MERGE);
			assert(clearFreezeState(chunk->mergeBuddy) == mergePartner);
			assert(clearFreezeState(mergePartner->mergeBuddy) == chunk);
			break;
			
		case FREEZE: //decision unknown
			markChunkFrozen(chunk);
			stabilizeChunk(btree, tg, chunk);
			decision = freezeDecision(chunk);
			switch (decision) {
				case R_COPY:
					assert(clearFreezeState(chunk->mergeBuddy) == NULL); //no mergeBuddy for COPY
					CAS(&chunk->mergeBuddy, C_FREEZE, C_COPY);
					break;
				case R_SPLIT:
					assert(clearFreezeState(chunk->mergeBuddy) == NULL); //no mergeBuddy for SPLIT
					CAS(&chunk->mergeBuddy, C_FREEZE, C_SPLIT);
					break;
				case R_MERGE:
					mergePartner = findMergeSlave(btree, tg, chunk);
					if (mergePartner == NULL) {
						//this chunk is slave, merge partner is master
						assert(extractFreezeState(chunk->mergeBuddy) == SLAVE_FREEZE);
						mergePartner = clearFreezeState(chunk->mergeBuddy);
						assert(mergePartner != NULL);
						CAS(&mergePartner->mergeBuddy, combineFreezeState(chunk, REQUEST_SLAVE),\
											combineFreezeState(chunk, MERGE));
						assert(extractFreezeState(mergePartner->mergeBuddy) == MERGE); // master expected to be in MERGE
					}
					//pointers correctly set
					assert(clearFreezeState(chunk->mergeBuddy) == mergePartner);
					assert(clearFreezeState(mergePartner->mergeBuddy) == chunk);
					break;
			} // end of switch on decision
			break; //end of case FREEZE

	} //end of switch on freeze state
	
	return freezeRecovery(btree, tg, chunk, key, expected, data, decision,\
							mergePartner, trigger, result);
}
/*****************************************************************************/
static inline int shouldMove(Chunk* leftleft, Chunk* left, Chunk* right) {
	Chunk* leftleft_partner = clearFreezeState(leftleft->mergeBuddy);
	Chunk* left_partner = clearFreezeState(left->mergeBuddy);
	Chunk* right_partner = clearFreezeState(right->mergeBuddy);

	if (right_partner == left) {
		if (left_partner == right) {
			return 1;
		}
		if (left_partner == NULL) {
			if (leftleft_partner == left) {
				return 2;
			}
			return 1;
		}
	}
	if (left_partner == right) {
		return 1;
	}
	return 0;
}
/*****************************************************************************/
Chunk* freezeRecovery(Btree* btree, ThreadGlobals* tg, Chunk* oldChunk, 
			uint64_t key, uint64_t expected, uint64_t input, RecoveryType recovType,
			Chunk* mergeChunk, FreezeTriggerType trigger, Bool* result) {
				
	assert(clearFreezeState(oldChunk) != NULL);
	assert(clearFreezeState(oldChunk) == oldChunk);
	assert(clearFreezeState(mergeChunk) == mergeChunk);
	assert(result != NULL);
	assert(key >= 0);
	
	int sepKey = MAX_KEY;
	Chunk* newChunk2 = NULL;
	Chunk* newChunk1 = &btree->memoryManager->memoryPool[allocateChunk(btree->memoryManager)];	//Allocate a new chunk
		
	Chunk* lowChunk = NULL;
	Chunk* highChunk = NULL;
	Chunk* master = NULL;
	
	assert(validateChunk(oldChunk));
	assert(validateChunk(mergeChunk));
	
	uint64_t oldChunkKey = 0;
	uint64_t partnerKey = 0;
	
	switch (recovType) {
		
		case R_COPY:
			assert(extractFreezeState(oldChunk->mergeBuddy) == COPY);
			copyToOneChunk(oldChunk, newChunk1);
			break;

		case R_MERGE:
			assert(mergeChunk != NULL);
			//correctly pointing merge partners
			assert(clearFreezeState(oldChunk->mergeBuddy) == mergeChunk);
			assert(clearFreezeState(mergeChunk->mergeBuddy) == oldChunk);
			//one chunk is in MERGE and its partner in SLAVE_FREEZE
			assert((extractFreezeState(oldChunk->mergeBuddy) == MERGE && extractFreezeState(mergeChunk->mergeBuddy) == SLAVE_FREEZE) ||
					(extractFreezeState(oldChunk->mergeBuddy) == SLAVE_FREEZE && extractFreezeState(mergeChunk->mergeBuddy) == MERGE));
			
			oldChunkKey = getKey(getNextEntry(&oldChunk->entryHead,oldChunk->entriesArray));
			partnerKey = getKey(getNextEntry(&mergeChunk->entryHead,mergeChunk->entriesArray));
			if (oldChunkKey < partnerKey) {
				lowChunk = oldChunk;
				highChunk = mergeChunk;
			} else {
				lowChunk = mergeChunk;
				highChunk = oldChunk;
			}

			if ((getEntrNum(oldChunk)+getEntrNum(mergeChunk)) >= MAX_ENTRIES ) {
				newChunk2 = &btree->memoryManager->memoryPool[allocateChunk(btree->memoryManager)];
				assert(newChunk1 != newChunk2);
				sepKey = mergeToTwoChunks(lowChunk, highChunk, newChunk1, newChunk2);
			} else {
				mergeToOneChunk(lowChunk, highChunk, newChunk1);
			}
			break;

		case R_SPLIT:
			assert(extractFreezeState(oldChunk->mergeBuddy) == SPLIT);
			newChunk2 = &btree->memoryManager->memoryPool[allocateChunk(btree->memoryManager)];
			assert(newChunk1 != newChunk2);
			sepKey = splitIntoTwoChunks(oldChunk, newChunk1, newChunk2);
			break;
			
		default:
			assert(FALSE); //recovType must be known at this point
			break;
	}

	if (newChunk2 != NULL && newChunk2->height != 0) {
		//if there are two new non leaf chunks - 
		//do not seperate between master and slave entries
		int last = newChunk1->counter -1;
		assert(last >= 1);
		Chunk* sepChunk0 = getSon(btree->memoryManager, &newChunk1->entriesArray[last-1]);
		Chunk* sepChunk1 = getSon(btree->memoryManager, &newChunk1->entriesArray[last]);
		Chunk* sepChunk2 = getSon(btree->memoryManager, &newChunk2->entriesArray[0]);
		int numToMove = shouldMove(sepChunk0, sepChunk1, sepChunk2);
		for (int i=0; i<numToMove; ++i) {
			moveEntryFromFirstToSecond(newChunk1,newChunk2);
		}
		if (numToMove != 0) {
			sepKey = findMaxKeyInChunk(newChunk1);
		}
	}

	assert(validateChunk(newChunk1));
	assert(validateChunk(newChunk2));

	*result = FALSE;
	switch (trigger) {
		
		case DELETE:
			*result = deleteInChunk(btree, tg, newChunk1,key, input);
			if (newChunk2 != NULL ) {
				*result = *result || deleteInChunk(btree, tg, newChunk2, key, input);
			}
			break;

		case INSERT:
			if ( ( newChunk2 != NULL) && (key > sepKey) ) {
				*result = insertToChunk(btree, tg, newChunk2, key, input);
			} else {
				*result = insertToChunk(btree, tg, newChunk1, key, input);
			}
			break;

		case SWAP:
			if (key <= sepKey) {
				*result = replaceInChunk(btree, tg, newChunk1, key, expected, input);
			} else {
				*result = replaceInChunk(btree, tg, newChunk2, key, expected, input);	
			}
			break;
		
		case ENSLAVE:
			// input should be interpreted as pointer to master trying to enslave, only in case of COPY
			if (recovType == R_COPY) {
				master = (Chunk*)input;
				newChunk1->mergeBuddy = combineFreezeState(master,SLAVE_FREEZE);
				markChunkFrozen(newChunk1);
			}
			break;
				
		case NONE:
			break;
	}

	//updateTarget set as following:
	//for SPLIT & COPY - oldChunk
	//for MERGE - master chunk
	Chunk* updateTarget = oldChunk;
	if (recovType == R_MERGE) {
		assert(mergeChunk);
		if (extractFreezeState(oldChunk->mergeBuddy) == SLAVE_FREEZE) {
			assert(extractFreezeState(mergeChunk->mergeBuddy) == MERGE);
			updateTarget = mergeChunk;
		}
	}
	
	Chunk* retChunk = NULL;
	if ( !CAS(&(updateTarget->newChunk), NULL, newChunk1 )) {
		//determine in which of the new chunks the destination key is now located
		if (key <= sepKey) {
			retChunk = updateTarget->newChunk;
		} else {
			retChunk = updateTarget->newChunk->nextChunk;
		}
	}

	//if the new chunk is in SLAVE_FREEZE, update the master to point back to him
	if (extractFreezeState(updateTarget->newChunk->mergeBuddy) == SLAVE_FREEZE) {
		master = clearFreezeState(updateTarget->newChunk->mergeBuddy);
		CAS(&master->mergeBuddy, combineFreezeState(updateTarget,REQUEST_SLAVE), \
					combineFreezeState(updateTarget->newChunk,REQUEST_SLAVE));
	}

	callForUpdate(btree, tg, recovType, updateTarget, sepKey);
	
	return retChunk;	// NULL if succeeded
	
}
/*****************************************************************************/
void stabilizeChunk(Btree* btree, ThreadGlobals* tg, Chunk* chunk) {

	assert(clearFreezeState(chunk) != NULL);
	assert(clearFreezeState(chunk) == chunk);
	assert(validateChunk(chunk));

	int maxKey = MAX_KEY;
	uint64_t eNext = 0;
	int key = 0;
	
	//Implicitly remove deleted entries
	find(btree, tg, chunk, maxKey);
	
	for(int i=0; i<MAX_ENTRIES; ++i) {
		Entry* currentE = &(chunk->entriesArray[i]);
		key = getKey(currentE);
		eNext = currentE->nextEntry;
		if ((key != NOT_DEFINED_KEY) && (!isDeleted(eNext))) {
			//This entry is allocated and not deleted
			if (!find(btree, tg, chunk, key)) {
				//This key is not yet in the tree
				insertEntry(btree, tg, chunk, currentE, key);
			}
		}
	}	//end of foreach
	assert(noDeletedEntries(chunk));
}
/*****************************************************************************/
void retireEntry(Btree* btree, ThreadGlobals* tg, Entry* entry) {
	
	assert(entry != NULL);
	//Add the entry to the(local)list of to-be-retired entries
	addToRetList(tg, entry);	
	//Scan the list and reclaim the entries if possible
	handleReclamationBuffer(btree, tg);

}
/*****************************************************************************/
void handleReclamationBuffer(Btree* btree, ThreadGlobals* tg) {

	// Local list for recording current hazard pointers
	EntryL plist;
	INIT_LIST_HEAD(&plist.list1);

	//Obtain head of hazard pointers array(HPA)
	HPRecord* hprec = btree->memoryManager->HPArray;

	// Stage1 : Save current hazard pointers in plist(locally)
	for(int i=0; i<MAX_THREADS_NUM; ++i) {
		Entry* hptr0 = hprec[i].hp0;
		Entry* hptr1 = hprec[i].hp1;
		
		if (hptr0 != NULL ) {
			EntryL* newEntry = (EntryL*) malloc(sizeof(EntryL));
			newEntry->entry = hptr0;
			list_add_tail(&newEntry->list1, &plist.list1);
		}
		if (hptr1 != NULL ) {
			EntryL* newEntry = (EntryL*) malloc(sizeof(EntryL));
			newEntry->entry = hptr1;
			list_add_tail(&newEntry->list1, &plist.list1);
		}
	}

	//Stage2: Reclaim to-be-retired entries that are not protected by a HP
	EntryL tmplist;
	EntryL* entry = NULL;
	list_t* tmpEntry = NULL;
	//Copy all local to-be-retired entires and clear RetList
	popAllRetList(tg, &tmplist);
	
	tmpEntry = list_pop(&tmplist.list1);
	if (!tmpEntry ) {
		entry = list_entry(tmpEntry, EntryL, list1);
	}

	while ( (entry != NULL) && (tmpEntry != NULL) ) {
		if ( lookUpEntryLsList(&plist,entry) ) {
			addEntryLToRetList(tg, entry); //entry is protected
		}
		else if (!isFrozen(entry->entry->nextEntry)) {
			clearEntry(btree, tg , NULL, entry->entry); //attempt to reclaim
		}
		//advance to next entry
		tmpEntry = list_pop(&tmplist.list1);
		if (!tmpEntry ) {
			entry = list_entry(tmpEntry, EntryL, list1);
		}
	}
	freeEntryLsList(&plist);	// normal free. just deallocate all the entries.

}
/*****************************************************************************/
Bool clearEntry(Btree* btree, ThreadGlobals* tg, Chunk* chunk, Entry* entry) {
	
	assert(entry != NULL);
	
	uint64_t savedKeyData = clearFrozen(entry->keyData);
	uint64_t savedNext = clearFrozen(entry->nextEntry);
	Bool result = FALSE;

	Entry* CUR = NULL;
	
	if ( CAS(&(entry->nextEntry), savedNext, setIndex(0, END_INDEX)) ) {
		if (CAS(&(entry->keyData), savedKeyData, AVAILABLE_ENTRY)) {
			return TRUE;	// Both CASes were successful
		}
	}

	assert(clearFreezeState(chunk) != NULL);
	assert(clearFreezeState(chunk) == chunk);
	assert(validateChunk(chunk));
	
	//A CAS failure indicates a freeze. Help freeze before proceeding.
	int key = getKey(getNextEntry(&chunk->entryHead, chunk->entriesArray));
	freeze(btree, tg, chunk, key, 0, 0, NONE, &result);	
	if (find(btree, tg, chunk, getKey(entry))) {
		CUR = entryIn(chunk, tg->cur);
		//Check whether the entry to be reclaimed was linked back by the freeze
		if ( entry == CUR ) {	
			return FALSE;// cur is global initiated by Find
		}
	}
	return TRUE;
	
}

/*****************************************************************************
	disabled Functions.
*****************************************************************************/


/*****************************************************************************/
void retireChunk(Btree* btree,ThreadGlobals* tg, Chunk* chunk) {}
/*****************************************************************************/
void handleChunkReclamationBuffer(Btree* btree, ThreadGlobals* tg) {}


