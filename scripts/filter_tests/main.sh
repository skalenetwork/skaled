#!/bin/bash -x

# run it in a SUBDIR (i.e. txt) of a build directory (with codecov files)!

export TESTETH=../test/testeth

SCRIPTS=`dirname ${BASH_SOURCE[0]}`

$TESTETH --list-tests | $SCRIPTS/all_tests_cov.sh				# generates suite/subsuite/test.txt hierarchy
find . -type f | sed 's/\.\///g' | python3 $SCRIPTS/matrix.py >cpp2tests.txt		# makes inverted table cpp->(list of tests)

# make list of "recently" changed files
ORIGIN=`pwd`
cd ../../
git diff --name-only 6ca9f08ade13572513c088fbeb589cf7b31aabe2 develop | grep '\.h\|\.c' >$ORIGIN/recent_files.txt
cd libconsensus
git diff --name-only 01712dbf49d4ee097a0a73abf19990ded5f1cfdc develop | grep '\.h\|\.c' | sed 's/\(.*\)/libconsensus\/\1/g' >>$ORIGIN/recent_files.txt
cd $ORIGIN

# filter by only "recent" files
grep -F -f recent_files.txt cpp2tests.txt >cpp2tests.txt.filtered

# find unique tests
sed 's/ /\n/g' cpp2tests.txt.filtered | grep -v '\.h\|\.c' | sort | uniq
