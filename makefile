MAX_THREADS := 32
INSERT_PERCENT := 20
DELETE_PERCENT := 20
SEARCH_PERCENT := 60

UNIT_TEST := FALSE

ifeq (${UNIT_TEST}, TRUE)
MAX_ENTRIES := 12
MIN_ENTRIES := 3
CC_FLAGS = -g -m64 -std=c99 -pedantic-errors -DMAX_THREADS_NUM=${MAX_THREADS} -DMAX_ENTRIES=${MAX_ENTRIES} -DMIN_ENTRIES=${MIN_ENTRIES} -DUNITTEST=${UNIT_TEST}
else
MAX_ENTRIES := 450
MIN_ENTRIES := 200
CC_FLAGS = -g -m64 -std=c99 -pedantic-errors -DMAX_THREADS_NUM=${MAX_THREADS} -DMAX_ENTRIES=${MAX_ENTRIES} -DMIN_ENTRIES=${MIN_ENTRIES} -DINSERT_PERCENT=${INSERT_PERCENT} -DDELETE_PERCENT=${DELETE_PERCENT} -DSEARCH_PERCENT=${SEARCH_PERCENT} -DNDEBUG
endif


CC = gcc
CC_CAS = `pkg-config --cflags glib-2.0`
CC_GTHREAD = `pkg-config --libs gthread-2.0`

OBJECTS = entry.o chunk.o globals.o chunkAlgorithm.o treeAlgorithm.o lockfreeBtree.o
TEST_OBJS =  entry_test.o chunk_test.o chunkAlgorithm_test.o lockfreeBtree_test.o main_test.o 

#######################TEST######################

test: ${OBJECTS} ${TEST_OBJS}
	${CC} ${CC_FLAGS} ${CC_GTHREAD} \
	${OBJECTS} ${TEST_OBJS} -o $@ `pkg-config --cflags glib-2.0` `pkg-config --libs gthread-2.0`


######################OBJECTS######################

entry.o: entry.c entry.h
	${CC} ${CC_FLAGS} -c $*.c -o $@

chunk.o: chunk.c chunk.h entry.h
	${CC} ${CC_FLAGS} ${CC_CAS} -c $*.c -o $@

globals.o: globals.c globals.h list.h entry.h chunk.h
	${CC} ${CC_FLAGS} ${CC_CAS} -c $*.c -o $@
	
chunkAlgorithm.o: chunkAlgorithm.c chunkAlgorithm.h chunk.h \
  entry.h globals.h list.h
	${CC} ${CC_FLAGS} ${CC_CAS} -c $*.c -o $@

treeAlgorithm.o: treeAlgorithm.c treeAlgorithm.h \
	chunkAlgorithm.c chunkAlgorithm.h chunk.h \
	entry.h globals.h list.h
	${CC} ${CC_FLAGS} ${CC_CAS} -c $*.c -o $@

lockfreeBtree.o: lockfreeBtree.c treeAlgorithm.h chunkAlgorithm.h \
	chunk.h entry.h globals.h list.h lockfreeBtree.h
	${CC} ${CC_FLAGS} ${CC_GTHREAD} ${CC_CAS} -c $*.c -o $@
	
######################TEST_OBJS######################

entry_test.o: tests/entry_test.c tests/entry_test.h entry.h
	${CC} ${CC_FLAGS} -c tests/$*.c -o $@

chunk_test.o: tests/chunk_test.c tests/chunk_test.h chunk.h entry.h \
  tests/entry_test.h entry.h
	${CC} ${CC_FLAGS} ${CC_CAS} -c tests/$*.c -o $@
	
chunkAlgorithm_test.o: tests/chunkAlgorithm_test.c tests/chunkAlgorithm_test.h \
  entry.h chunkAlgorithm.h chunk.h entry.h \
  globals.h list.h chunk.h tests/entry_test.h \
  lockfreeBtree.h chunkAlgorithm.h
	${CC} ${CC_FLAGS} ${CC_CAS} -c tests/$*.c -o $@
	
lockfreeBtree_test.o: entry.h chunkAlgorithm.h chunk.h entry.h globals.h list.h chunk.h \
	lockfreeBtree.h chunkAlgorithm.h tests/lockfreeBtree_test.h
	${CC} ${CC_FLAGS} ${CC_GTHREAD} ${CC_CAS} -c tests/$*.c -o $@	
 
	
main_test.o: main_test.c
	${CC} ${CC_FLAGS} ${CC_CAS} -c $*.c -o $@

######################Clean######################

clean:
	rm test *.o

