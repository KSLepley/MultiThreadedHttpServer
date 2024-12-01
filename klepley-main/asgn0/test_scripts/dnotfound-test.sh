#!/bin/bash

# Tests given output for when file does not have the delimiter

./split a test_files/nod.txt > /tmp/nodtmp.txt

difference=$(diff /tmp/nodtmp.txt test_files/nodcorrect.txt)

if [[ -z "$difference" ]]; then
	exit 0
else
	exit 1
fi

