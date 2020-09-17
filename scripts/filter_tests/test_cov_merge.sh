#!/bin/bash
while IFS= read -r line
do
	if [[ "$line" =~ ^File\ \'(.+)\'$ ]];
	then
		file="${BASH_REMATCH[1]}"
		IFS= read -r line2
		[[ "$line2" =~ ^Lines\ executed:(.+)%\ of\ (.+) ]]
		percent="${BASH_REMATCH[1]}"
		lines="${BASH_REMATCH[2]}"
		[[ "$percent" != "0.00" ]] && echo "$file" "$percent" "$lines"
	fi
done
