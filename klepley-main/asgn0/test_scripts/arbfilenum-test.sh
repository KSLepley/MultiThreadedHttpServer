#!/bin/bash

# Tests that return code can be ran on arbit (a lot) of files

./split a test_files/alla.txt test_files/mixed.txt test_files/nod.txt test_files/noletters.txt > /tmp/arbfiles.txt

#cat /tmp/allfiles.txt

difference=$(diff /tmp/arbfiles.txt test_files/allfilescorrect.txt)

if [[ -z "$difference" ]]; then
	exit 0
else
	exit 1
fi

