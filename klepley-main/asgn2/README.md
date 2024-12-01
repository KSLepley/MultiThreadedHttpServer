## Assignment 2 directory
This directory contains source code and other files for Assignment 2.

# Main Thang (Server)
The httpserver.c file is part of our assignment in which we implement an HTTP server using C. The server is designed to handle incoming client connections persistently, processing HTTP GET and PUT requests. It establishes a listening socket, accepts connections, and processes each connection sequentially, handling HTTP requests by reading bytes from the client and sending responses back. The server ensures resilience against malformed or malicious inputs and is capable of handling errors gracefully without crashing. Additionally, it manages resources efficiently, avoiding memory leaks and ensuring timely responses.

# Makefile
The makefile simply makes the file. Run 'make' to make the httpserver.c program. Run 'make clean' to
remove all binaries and basically reset the file. Run 'format' to clang format the file. Run
'make all' do all the things mentioned above at once.

# README.md
This is simply the file you are reading now! (Awesome Sauce) 

# CODE INSPO / HELP
I basically live in OH, so any help I get on the assignment stems from the TA's
themselves as well as from Prof Veen. Also looked at C documentation and forums online. >:| 
