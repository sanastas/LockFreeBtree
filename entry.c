#include "entry.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stddef.h>



/*****************************************************************************
	isFrozen :
	Checks if the frozen bit (LSB) is set
*****************************************************************************/
Bool isFrozen(uint64_t word) {
	if (word & FROZEN_FLAG) {
		return TRUE;
	}
	return FALSE;
}

/*****************************************************************************
	markFrozen :
	Returns the value of word with the frozen bit set
	it doesn’t matter if in initial word this bit was set or not.
*****************************************************************************/
uint64_t markFrozen(uint64_t word) {
	return (word | FROZEN_FLAG);
}

/*****************************************************************************
	clearFrozen :
	Returns the value of a word with the frozen bit reset to zero;
	it doesn’t matter if in initial word this bit was set or not.
*****************************************************************************/
uint64_t clearFrozen(uint64_t word) {
	return (word & ~FROZEN_FLAG);
}

/*****************************************************************************
	isDeleted :
	Checks if deleted bit (2nd LSB) is set
*****************************************************************************/
Bool isDeleted(uint64_t word) {
	if (word & DELETED_FLAG) {
		return TRUE;
	}
	return FALSE;
}

/*****************************************************************************
	markDeleted :
	Returns the value of word with the deleted bit set
	it doesn’t matter if in initial word this bit was set or not.
*****************************************************************************/
uint64_t markDeleted(uint64_t word) {
	return (word | DELETED_FLAG);
}

/*****************************************************************************
	clearDeleted :
	Returns the value of a word with the deleted bit reset to zero;
	it doesn’t matter if in initial word this bit was set or not.
*****************************************************************************/
uint64_t clearDeleted(uint64_t word) {
	return (word & ~DELETED_FLAG);
}


/*****************************************************************************
	combineKeyData :
	receives a key, and pointer to Data, and combines them to one word.
*****************************************************************************/
uint64_t combineKeyData(int key, Data data) {
	assert(key <= MAX_KEY || key == NOT_DEFINED_KEY);
	uint64_t tmp = 0;
	uint64_t tmpKey = 0;
	tmp = (uint64_t)data & LOW_32_MASK;
	tmp = tmp << SHIFT_DATA;
	key = key << 1;
	tmpKey = (uint64_t)key & LOW_32_MASK;
	tmp = tmp | tmpKey;
	return tmp;
}

/*****************************************************************************
	parseKeyData :
	receives a word, analyzes it, and returns the key and the data each in his
	own. (pare the combined word.)
*****************************************************************************/
void parseKeyData(uint64_t word, int* key, Data* data) {
	
	uint64_t tmpData = ( word & HIGH_32_MASK );
	uint64_t tmpKey = ( word & LOW_32_MASK );

	if (key) {
		*key = (int) (tmpKey >> 1);
	}
	if (data) {
		*data = (int) (tmpData >> SHIFT_DATA);
	}
	
	return;
}

/*****************************************************************************
	getKey :
	receives an entry, and returns it's key.
*****************************************************************************/
int getKey(Entry* entry) {
	assert(entry != NULL);
	return ( (entry->keyData & LOW_32_MASK) >> 1 );
}

/*****************************************************************************
	getData :
	receives an entry, and returns it's data.
*****************************************************************************/
Data getData(Entry* entry) {
	assert(entry != NULL);
	return ( (entry->keyData & HIGH_32_MASK) >> SHIFT_DATA );
}

/*****************************************************************************
	getNextEntry :
	receives an entry, and returns a pointer to the next entry.
*****************************************************************************/
Entry* getNextEntry(Entry* curEntry, Entry* relativeStartLocation) {
	assert(curEntry);
	
	uint64_t index = ( ( (curEntry->nextEntry) & INDEX_MASK ) >> SHIFT_INDEX ) & LOW_32_MASK;
	assert(index >=0);
	
	if (index == END_INDEX) {
		return NULL;
	}
	return &(relativeStartLocation[index]);
}

/*****************************************************************************
	getVersion :
	return version stored in a word
*****************************************************************************/
int getVersion(uint64_t word) {
	return ((word & VERSION_MASK) >> SHIFT_DELETE_FREEZE);
}

/*****************************************************************************
	incVersion :
	return word with version+1
*****************************************************************************/
uint64_t incVersion(uint64_t word) {
	return (word + INC_VERSION);	
}

/*****************************************************************************
	setVersion :
	returns word with updated version
*****************************************************************************/
uint64_t setVersion(uint64_t word, int version) {
	assert(version <= MAX_VERSION);
	return ( ((uint64_t)version << SHIFT_DELETE_FREEZE) | (word & CLEARED_VERSION) );
}

/*****************************************************************************
	setIndex :
	returns word with updated index
*****************************************************************************/
uint64_t setIndex(uint64_t word, int index) {
	assert(index <= END_INDEX);
	return ( ((uint64_t)index << SHIFT_INDEX) | (word & CLEARED_INDEX) );
}

/*****************************************************************************
	getIndex :
	returns the index stored in the word (High 10 bits)
*****************************************************************************/
int getIndex(uint64_t word) {
	return ((word >> SHIFT_INDEX) & LOW_32_MASK);
}

/*****************************************************************************
	getPosition :
	curEntry - the entry we check
	relativeStartLocation - an array of entries
	returns the position of current entry in the received array
	(relative address ).
*****************************************************************************/
int getPosition(Entry* curEntry, Entry* relativeStartLocation) {
	if (curEntry == NULL) {
		return END_INDEX;
	}
	return (curEntry - relativeStartLocation);	
}

/*****************************************************************************
	entPtr :
	returns start of Entry by the nextEntry word (address manipulation)
*****************************************************************************/
Entry* entPtr(uint64_t* word) {
	return (Entry*) ((char*)(word) - (unsigned long)(&((Entry*)0)->nextEntry));
}



