#!/bin/bash

# Tests that return code runs on PDF files and 
# also ensures that lowercase and capital letters
#are treated as different delims

./split a test_files/beemovie.pdf > /tmp/pdfuni.txt

difference=$(diff /tmp/pdfuni.txt test_files/beemoviecorrect.txt)

if [[ -z "$difference" ]]; then
	exit 0
else
	exit 1
fi

