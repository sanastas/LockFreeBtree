#ifndef __ENTRY__H_
#define __ENTRY__H_


#include <stdint.h>
//typedef signed long long int uint64_t;

/*****************************************************************************
	Entry Struct :
	overview:
	|keyData word | nextEntry word |
	detailed:
	keyData word  -  32 bit data, 31 bit key, 1 bit freeze
	nextEntry word - 10 bit index, 52 bit version, 1 bit delete, 1 bit freeze
*****************************************************************************/
typedef struct Entry_t {
	uint64_t keyData;
	uint64_t nextEntry;
} Entry;


#ifndef MAX_ENTRIES 
#define MAX_ENTRIES 450
#endif

#ifndef MIN_ENTRIES 
#define MIN_ENTRIES 200
#endif

//entry availability
#define NOT_DEFINED_KEY 2147483647
#define MAX_KEY 2147483646
#define AVAILABLE_ENTRY ((uint64_t)(NOT_DEFINED_KEY << 1) & LOW_32_MASK)
//frozen, deleted
#define FROZEN_FLAG 0x0000000000000001
#define DELETED_FLAG 0x0000000000000002
//bit masks
#define HIGH_32_MASK 0xFFFFFFFF00000000
#define LOW_32_MASK 0x00000000FFFFFFFF
//index
#define INDEX_MASK 0xFFC0000000000000
#define CLEARED_INDEX 0x003FFFFFFFFFFFFF
#define END_INDEX MAX_ENTRIES
//version
#define VERSION_MASK 0x003FFFFFFFFFFFC
#define CLEARED_VERSION 0xFFC0000000000003
#define MAX_VERSION  4503599627370495
//arithmetic operations
#define SHIFT_INDEX 54
#define SHIFT_DELETE_FREEZE 2
#define SHIFT_DATA 32
#define INC_VERSION 4


typedef enum {FALSE, TRUE} Bool;
typedef int Data;


/*****************************************************************************
	isFrozen :
	Checks if the frozen bit (LSB) is set
*****************************************************************************/
Bool isFrozen(uint64_t word);

/*****************************************************************************
	markFrozen :
	Returns the value of word with the frozen bit set
	it doesn’t matter if in initial word this bit was set or not.
*****************************************************************************/
uint64_t markFrozen(uint64_t word);

/*****************************************************************************
	clearFrozen :
	Returns the value of a word with the frozen bit reset to zero;
	it doesn’t matter if in initial word this bit was set or not.
*****************************************************************************/
uint64_t clearFrozen(uint64_t word);

/*****************************************************************************
	isDeleted :
	Checks if deleted bit (2nd LSB) is set
*****************************************************************************/
Bool isDeleted(uint64_t word);

/*****************************************************************************
	markDeleted :
	Returns the value of word with the deleted bit set
	it doesn’t matter if in initial word this bit was set or not.
*****************************************************************************/
uint64_t markDeleted(uint64_t word);

/*****************************************************************************
	clearDeleted :
	Returns the value of a word with the deleted bit reset to zero;
	it doesn’t matter if in initial word this bit was set or not.
*****************************************************************************/
uint64_t clearDeleted(uint64_t word);

/*****************************************************************************
	combineKeyData :
	receives a key, and pointer to Data, and combines them to one word.
*****************************************************************************/
uint64_t combineKeyData(int key, Data data);

/*****************************************************************************
	parseKeyData :
	receives a word, analyzes it, and returns the key and the data each in his
	own. (pare the combined word.)
*****************************************************************************/
void parseKeyData(uint64_t word, int* key, Data* data);

/*****************************************************************************
	getKey :
	receives an entry, and returns it's key.
*****************************************************************************/
int getKey(Entry* entry);

/*****************************************************************************
	getData :
	receives an entry, and returns it's data.
*****************************************************************************/
Data getData(Entry* entry);

/*****************************************************************************
	getNextEntry :
	receives an entry and relative start
	returns a pointer to the next entry by extracting the next entry index.
*****************************************************************************/
Entry* getNextEntry(Entry* curEntry, Entry* relativeStartLocation);

/*****************************************************************************
	getVersion :
	return version stored in a word
*****************************************************************************/
int getVersion(uint64_t word);

/*****************************************************************************
	incVersion :
	return word with version+1
*****************************************************************************/
uint64_t incVersion(uint64_t word);

/*****************************************************************************
	setVersion :
	returns word with updated version
*****************************************************************************/
uint64_t setVersion(uint64_t word, int version);

/*****************************************************************************
	setIndex :
	returns word with updated index
*****************************************************************************/
uint64_t setIndex(uint64_t word, int index);

/*****************************************************************************
	getIndex :
	returns the index stored in the word (High 10 bits)
*****************************************************************************/
int getIndex(uint64_t word);

/*****************************************************************************
	getPosition :
	curEntry - the entry we check
	relativeStartLocation - an array of entries
	returns the position of current entry in the received array
	(relative address ).
*****************************************************************************/
int getPosition(Entry* curEntry, Entry* relativeStartLocation);

/*****************************************************************************
	entPtr :
	returns start of Entry by the nextEntry word (address manipulation)
*****************************************************************************/
Entry* entPtr(uint64_t* word);

#endif // __ENTRY__H_

