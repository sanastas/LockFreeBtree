#ifndef __LOCKFREELIST_TEST_H_FILE_
#define __LOCKFREELIST_TEST_H_FILE_


#include "../lockfreeBtree.h"
#include <stdlib.h>
#include <stdio.h>

//program stress test
float stage1(Btree* btree, uint64_t N, float *max_s1_insert, float *avg_s1_insert);

float stage2(Btree* btree, uint64_t N, float* max_s2_insert, float* max_s2_delete, float* max_s2_search,
	float* avg_s2_insert, float* avg_s2_delete, float* avg_s2_search );

#endif //__LOCKFREELIST_TEST_H_FILE_
