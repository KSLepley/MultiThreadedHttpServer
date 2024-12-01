#!/bin/bash

# Tests given output for if the file has mixed contents

./split a test_files/mixed.txt > /tmp/mixedtmp.txt

difference=$(diff /tmp/mixedtmp.txt test_files/mixedcorrect.txt)

if [[ -z "$difference" ]]; then
	exit 0
else
	exit 1
fi

