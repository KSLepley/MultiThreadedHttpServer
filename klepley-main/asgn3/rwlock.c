#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "rwlock.h"

typedef struct rwlock {
    int max_readers;
    int current_readers;
    int active_readers;
    int waiting_writers; // # of writers waiting for a lock
    int waiting_readers; // # of readers waiting for a lock
    int lock_holder; // Indicator of who holds the lock

    pthread_mutex_t mutex; // Mutex to protect access to the lock
    pthread_cond_t cond_priority; // Condition variable for priority
    pthread_cond_t cond_free; // Condition variable for general use
    PRIORITY priority;
} rwlock;

rwlock_t *rwlock_new(PRIORITY p, uint32_t n) {
    rwlock_t *lock = calloc(1, sizeof(rwlock_t));
    pthread_mutex_init(&(lock->mutex), NULL);
    pthread_cond_init(&(lock->cond_priority), NULL);
    pthread_cond_init(&(lock->cond_free), NULL); // Initialize the free condition variable
    lock->priority = p;
    lock->max_readers = n; // Set the maximum number of readers for N_WAY priority
    return lock;
}

void rwlock_delete(rwlock_t **lock) {
    if (lock && (*lock)) {
        pthread_mutex_destroy(&((*lock)->mutex)); // Destroy the mutex
        pthread_cond_destroy(&((*lock)->cond_priority)); // Destroy cond_prio
        pthread_cond_destroy(&((*lock)->cond_free)); // Destroy cond_free
        free(*lock); // Free the allocated memory
        *lock = NULL;
    }
}

void reader_lock(rwlock_t *lock) {
    pthread_mutex_lock(&(lock->mutex));
    lock->waiting_readers++; // Increment the number of waiting readers
    while (lock->lock_holder == 2 || (lock->priority == WRITERS && lock->waiting_writers > 0)
           || (lock->priority == N_WAY && lock->waiting_writers > 0
               && lock->current_readers >= lock->max_readers)) {
        pthread_cond_wait((lock->priority == READERS) ? &(lock->cond_priority) : &(lock->cond_free),
            &(lock->mutex)); // Wait if there are waiting writers (WRITERS priority) || // Wait if too many readers (N_WAY priority)
    }
    lock->active_readers++;
    if (lock->priority == N_WAY)
        lock->current_readers++; // Increment the number of active readers
    lock->waiting_readers--; // Decrement the number of waiting readers
    lock->lock_holder = 1; // Set the lock holder to reader
    pthread_mutex_unlock(&(lock->mutex));
}

void reader_unlock(rwlock_t *lock) {
    pthread_mutex_lock(&(lock->mutex));
    if (--lock->active_readers == 0) { // Decrement the number of active readers
        lock->lock_holder = 0; // Reset the lock holder if no more active readers
        pthread_cond_broadcast(&(lock->cond_priority)); //Wake up all threads
        pthread_cond_broadcast(&(lock->cond_free)); //Wake up all threads
    }
    pthread_mutex_unlock(&(lock->mutex));
}

// Acquire a write lock
//Lot of the logic is the same as reader_lock
void writer_lock(rwlock_t *lock) {
    pthread_mutex_lock(&(lock->mutex));
    lock->waiting_writers++;
    // Wait if the lock is held by someone else or conditions based on priority
    while (lock->lock_holder != 0
           || (lock->priority == N_WAY && lock->waiting_readers > 0
               && lock->current_readers < lock->max_readers)) {
        pthread_cond_wait((lock->priority == WRITERS) ? &(lock->cond_priority) : &(lock->cond_free),
            &(lock->mutex));
    }
    lock->waiting_writers--;
    if (lock->priority == N_WAY)
        lock->current_readers = 0;
    lock->lock_holder = 2;
    pthread_mutex_unlock(&(lock->mutex));
}

// Release a write lock
//Lot of the logic is the same as reader_unlock
void writer_unlock(rwlock_t *lock) {
    pthread_mutex_lock(&(lock->mutex));
    lock->lock_holder = 0;
    pthread_cond_broadcast(&(lock->cond_priority));
    pthread_cond_broadcast(&(lock->cond_free));
    pthread_mutex_unlock(&(lock->mutex));
}
