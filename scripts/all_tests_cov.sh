#!/bin/bash -x

while IFS= read -r line
do
	mkdir -p `dirname $line`
	../scripts/test_cov.sh $line | ../scripts/test_cov_merge.sh | grep -v 'test/\|SkaleDeps\|.hunter\|/usr/include\|deps/' >${line}.txt
done
