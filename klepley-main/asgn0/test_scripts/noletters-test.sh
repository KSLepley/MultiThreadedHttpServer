#!/bin/bash

# Tests given output works on non-letters on txt files w/ only non-letters

./split + test_files/noletters.txt > /tmp/noletters.txt

difference=$(diff /tmp/noletters.txt test_files/noletterscorrect.txt)

if [[ -z "$difference" ]]; then
	exit 0
else
	exit 1
fi

