#!/bin/bash -x

SCRIPTS=`dirname ${BASH_SOURCE[0]}`

while IFS= read -r line
do
	mkdir -p `dirname $line`
	$SCRIPTS/test_cov.sh $line | $SCRIPTS/test_cov_merge.sh | grep -v 'test/\|SkaleDeps\|.hunter\|/usr/include\|deps/' >${line}.txt
done
