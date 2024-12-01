## Assignment 3 directory
This directory contains source code and other files for Assignment 4.

# Main Program: httpserver.c (Multi-Threaded HttpServer)
The httpserver.c file implements a multi-threaded HTTP server designed to handle multiple client requests concurrently using synchronization mechanisms like thread-safe queues and reader-writer locks. The main function initializes the server, creates worker threads, and assigns incoming connections to these threads via a dispatcher. Worker threads process HTTP GET and PUT requests, logging each request in an atomic and coherent manner. Helper functions manage socket connections, thread synchronization, and audit logging to ensure efficient and reliable server operation.

# Makefile
The makefile simply makes the file. Run 'make' to make the queue.c and rwlock.c programs. Run 'make clean' to
remove all binaries and basically reset the file. Run 'format' to clang format the file. Run
'make all' do all the things mentioned above at once.

# README.md
This is simply the file you are reading now! (Awesome Sauce Part2 ) 

# CODE INSPO / HELP
I basically live in OH, so any help I get on the assignment stems from the TA's
themselves as well as from Prof Veen. Also looked at C documentation and forums online. >:| I also gained access to some of Quinn's lectures from the latest quarter he taught and it really helped with the linked list implementation. Also shoutout Mitchell!
