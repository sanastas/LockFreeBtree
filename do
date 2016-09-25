#!/bin/bash
make clean
make UNIT_TEST=TRUE && ./test
