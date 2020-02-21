#!/bin/bash -x

TEST=$1

TESTETH=./test/testeth
export ETHEREUM_TEST_PATH=../test/jsontests

find . -name '*.gcda' -delete
$TESTETH -t $TEST
find . -name '*.gcda' | sed 's/.gcda/.o/g' | xargs gcov -n -d
