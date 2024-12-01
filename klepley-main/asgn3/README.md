## Assignment 3 directory
This directory contains source code and other files for Assignment 3.

# Main Program(s) (Queue and Lock)
The .c file(s) are part of our assignment in which we implement a thread-safe bounded buffer with FIFO properties, where elements can be added and removed in a first-in, first-out order. It includes functions to create and delete the queue, as well as to push and pop elements, ensuring thread safety with multiple concurrent producers and consumers. The rwlock.c file implements a reader-writer lock that allows multiple readers or a single writer to hold the lock, with functionalities to lock and unlock for both readers and writers. It supports different priority schemes to manage contention between readers and writers, preventing starvation and ensuring fairness.

# Makefile
The makefile simply makes the file. Run 'make' to make the queue.c and rwlock.c programs. Run 'make clean' to
remove all binaries and basically reset the file. Run 'format' to clang format the file. Run
'make all' do all the things mentioned above at once.

# README.md
This is simply the file you are reading now! (Awesome Sauce) 

# CODE INSPO / HELP
I basically live in OH, so any help I get on the assignment stems from the TA's
themselves as well as from Prof Veen. Also looked at C documentation and forums online. >:| And for the queue implementation, specifically, I used the professor's lecture slides as he basically gave us the code in that. Super cool, very helpful Veen!
