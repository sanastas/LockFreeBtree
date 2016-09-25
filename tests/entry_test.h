#ifndef __ENTRY_TEST_H_
#define __ENTRY_TEST_H_

#include "../entry.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

void runAllEntryTests();
void test_isFreeze();
void test_isDeleted();
void test_combineKeyData();
void test(Bool exp);

#endif //__ENTRY_TEST_H_
