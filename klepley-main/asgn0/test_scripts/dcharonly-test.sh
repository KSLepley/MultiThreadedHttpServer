#!/bin/bash

# Tests given output for if the file ONLY contains the delimiter

./split a test_files/alla.txt > /tmp/allatmp.txt

difference=$(diff /tmp/allatmp.txt test_files/allacorrect.txt)

if [[ -z "$difference" ]]; then
	exit 0
else
	exit 1
fi
