#!/bin/bash -x

TEST=$1

SCRIPTS=`dirname ${BASH_SOURCE[0]}`

#TESTETH=$SCRIPTS/../test/testeth
if [ -z "$TESTETH" ]
then
	echo "Please set TESTETH"
	exit(1)
fi


export ETHEREUM_TEST_PATH=$SCRIPTS/../test/jsontests

find . -name '*.gcda' -delete
$TESTETH -t $TEST
find . -name '*.gcda' | sed 's/.gcda/.o/g' | xargs gcov -n -d
