#include "entry_test.h"
#include <assert.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/*****************************************************************************
							entry.c tests.
*****************************************************************************/

/*****************************************************************************/
void test_isFreeze() {
	Entry e;

	printf("==== testFreeze1 ======\n");
	e.keyData = 0;
	e.nextEntry = 8;
	
	// e is not froozen
	assert( !isFrozen(e.nextEntry) );
	assert( isFrozen( markFrozen(e.nextEntry) ) );	
	assert( !isFrozen( clearFrozen(e.nextEntry)));
	assert( !isFrozen( clearFrozen( markFrozen(e.nextEntry) ) ) );
	
	printf("PASSED\n");
}
/*****************************************************************************/
void test_isDeleted() {
	Entry e;

	printf("==== test_isDeleted ======\n");
	e.keyData = 0;
	e.nextEntry = 8;
	
	// e is not deleted
	assert( !isDeleted(e.nextEntry) );
	assert( isDeleted( markDeleted(e.nextEntry) ) );	
	assert( !isDeleted( clearDeleted(e.nextEntry)));
	assert( !isDeleted( clearDeleted( markDeleted(e.nextEntry) ) ) );
	
	printf("PASSED\n");
}
/*****************************************************************************/
void test_combineKeyData(int times) {
	Data data = 0;
	int key = 0;
	uint64_t res = 0;
			
	int tKey = 0;
	Data tData = 0;

	printf("==== test_combineKeyData ======\n");
	
	srand(time(NULL));
	for (; times>0; --times) {
		
		key = ((int)rand()) % ((int)pow(2,30)) ;
		data = ((int)rand()) % ((int)pow(2,32)) ;

		res = combineKeyData(key, data);
		parseKeyData(res, &tKey, &tData);
		assert (tKey == key);
		assert (tData == data);
	}
	
	printf("PASSED\n");
}
/*****************************************************************************/
void test_getVersion() {

	printf("==== test_getVersion ======\n");
	Entry e;
	memset(&e, 0, sizeof(Entry));
	assert(getVersion(e.nextEntry) == 0);
	
	e.nextEntry = ((uint64_t)e.nextEntry | 0x00000000000000AC);
	assert(getVersion(e.nextEntry) == 43);
	
	memset(&e, 0, sizeof(Entry));
	e.nextEntry = ((uint64_t)e.nextEntry | 0x00000000000000A8);
	assert(getVersion(e.nextEntry) == 42);
	
	memset(&e, 0, sizeof(Entry));
	e.nextEntry = ((uint64_t)e.nextEntry | 0x0000000000000FF8);
	assert(getVersion(e.nextEntry) == 1022);
	
	printf("PASSED\n");
}
/*****************************************************************************/
void test_incVersion() {
	
	printf("==== test_incVersion ======\n");
	Entry e;
	memset(&e, 0, sizeof(Entry));
	assert(getVersion(e.nextEntry) == 0);
	
	e.nextEntry = ((uint64_t)e.nextEntry | 0x00000000000000AC); //43

	e.nextEntry = incVersion(e.nextEntry);
	assert(getVersion(e.nextEntry) == 44);
	
	e.nextEntry = incVersion(e.nextEntry);
	assert(getVersion(e.nextEntry) == 45);
	
	e.nextEntry = incVersion(e.nextEntry);
	assert(getVersion(e.nextEntry) == 46);
	
	memset(&e, 0, sizeof(Entry));
	e.nextEntry = ((uint64_t)e.nextEntry | 0x0000000000000FF8); //1022
	
	e.nextEntry = incVersion(e.nextEntry);
	assert(getVersion(e.nextEntry) == 1023);
	
	printf("PASSED\n");
}
/*****************************************************************************/
void test_setVersion() {
	
	printf("==== test_setVersion ======\n");
	
	Entry e;
	memset(&e, 0, sizeof(Entry));
	assert(getVersion(e.nextEntry) == 0);
	
	e.nextEntry = ((uint64_t)e.nextEntry | 0x0000000000000FF8); //1022
	
	e.nextEntry = setVersion(e.nextEntry, 43);
	assert(getVersion(e.nextEntry) == 43);
	
	e.nextEntry = setVersion(e.nextEntry, 44);
	assert(getVersion(e.nextEntry) == 44);
	
	e.nextEntry = setVersion(e.nextEntry, 45);
	assert(getVersion(e.nextEntry) == 45);
	
	e.nextEntry = setVersion(e.nextEntry, 0);
	assert(getVersion(e.nextEntry) == 0);
	
	e.nextEntry = setVersion(e.nextEntry, 1);
	assert(getVersion(e.nextEntry) == 1);
	
	printf("PASSED\n");
}
/*****************************************************************************/
void test_setIndex() {

	printf("==== test_setIndex ======\n");
	
	Entry e;
	memset(&e, 0, sizeof(Entry));
	
	e.nextEntry = setIndex(e.nextEntry, 0);
	assert( (((uint64_t)e.nextEntry >> 54) & LOW_32_MASK) == 0);
	
	e.nextEntry = setIndex(e.nextEntry, 1);
	assert( (((uint64_t)e.nextEntry >> 54) & LOW_32_MASK) == 1);
	
	e.nextEntry = setIndex(e.nextEntry, 2);
	assert( (((uint64_t)e.nextEntry >> 54) & LOW_32_MASK) == 2);
	
	e.nextEntry = setIndex(e.nextEntry, 3);
	assert( (((uint64_t)e.nextEntry >> 54) & LOW_32_MASK) == 3);
	
	e.nextEntry = setIndex(e.nextEntry, 4);
	assert( (((uint64_t)e.nextEntry >> 54) & LOW_32_MASK) == 4);
	
	printf("PASSED\n");
}
/*****************************************************************************/
void test_getNextEntry() {

	printf("==== test_getNextEntry ======\n");
	
	Entry array[END_INDEX];
	
	
	for (int i=END_INDEX-1; i>0; --i) {
		array[i].nextEntry = setIndex(array[i].nextEntry,i-1);
	}
	array[0].nextEntry = setIndex(array[0].nextEntry, END_INDEX);
	
	
	Entry* tst = &array[END_INDEX-1];
	for (int i=END_INDEX-1; i>=0; --i) {
		tst = getNextEntry(tst, array);
	}
	assert(tst == NULL); //reached end of list
	
	printf("PASSED\n");
}
/*****************************************************************************/
void test_entPtr() {
	printf("==== test_entPtr ======\n");
	
	Entry e;
	e.keyData = 999;
	e.nextEntry = (int)NULL;
	
	Entry* e1 = entPtr(&e.nextEntry);
	assert(e1->keyData == 999);
	
	printf("PASSED\n");
}
/*****************************************************************************/
void runAllEntryTests() {
	test_isFreeze();
	test_isDeleted();
	test_combineKeyData(10);
	test_getVersion();
	test_incVersion();
	test_setVersion();
	test_setIndex();
	test_getNextEntry();
	test_entPtr();
}
