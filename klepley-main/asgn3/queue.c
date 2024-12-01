#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include "queue.h"

// Define the structure for the queue
typedef struct queue {
    void **buffer; // Buffer to store queue elements

    sem_t full_count; // Sem to count full slots
    sem_t empty_count; // Sema to count empty slots
    sem_t mutex; // Sem for mutual exclusion

    int capacity; // Capacity of the q
    int head; // Index for the head of the q
    int tail; // Index for the tail of the q
} queue_t;

// Create a new queue with the given capacity
queue_t *queue_new(int capacity) {
    queue_t *q = (queue_t *) malloc(sizeof(queue_t)); // Allocate memory for the queue structure
    if (!q) {
        return NULL; // Return NULL if memory allocation fails
    }

    q->capacity = capacity; // Set the capacity of the queue
    q->head = 0; // Initialize the head index
    q->tail = 0;
    q->buffer = (void **) malloc(capacity * sizeof(void *)); // Allocate memory for the buffer
    if (!q->buffer) {
        free(q);
        return NULL; // Return NULL if buffer memory allocation fails
    }

    // Initialize the semaphores
    int result = sem_init(&(q->full_count), 0, capacity);
    assert(result == 0);
    result = sem_init(&(q->empty_count), 0, 0);
    assert(result == 0);
    result = sem_init(&(q->mutex), 0, 1);
    assert(result == 0);

    return q;
}

// Delete the queue and free its resources
void queue_delete(queue_t **q_ptr) {
    if (q_ptr && *q_ptr) { // Check if the queue pointer and the queue itself are not NULL
        queue_t *q = *q_ptr;

        if (q->buffer) {
            // Destroy the semaphores
            int res = sem_destroy(&(q->full_count));
            assert(res == 0);
            res = sem_destroy(&(q->empty_count));
            assert(res == 0);
            res = sem_destroy(&(q->mutex));
            assert(res == 0);

            free(q->buffer); // Free the buffer memory
        }

        free(q); // Free the queue structure memory
        *q_ptr = NULL; // Set the queue pointer to NULL
    }
}

// Push an element onto the queue
bool queue_push(queue_t *q, void *elem) {
    if (!q) {
        return false; // Return false if the queue is NULL
    }

    sem_wait(&(q->full_count)); // Wait if the q is full
    sem_wait(&(q->mutex)); // Lock the queue for exclusive access

    q->buffer[q->head] = elem; // Place the element in the queue
    q->head = (q->head + 1) % q->capacity; // Update head index

    sem_post(&(q->mutex)); // Unlock the queue
    sem_post(&(q->empty_count)); // Signal that there is a new element in the queue

    return true;
}

// Pop an element from the queue
bool queue_pop(queue_t *q, void **elem) {
    if (!q) {
        return false; // Return false if the queue is NULL
    }

    sem_wait(&(q->empty_count)); // Wait if the queue is empty
    sem_wait(&(q->mutex));

    *elem = q->buffer[q->tail]; // Retrieve the element from the queue
    q->tail = (q->tail + 1) % q->capacity; // Update the tail index

    sem_post(&(q->mutex)); // Unlock the queue
    sem_post(&(q->full_count)); // Signal that there is a new empty slot in the queue

    return true;
}
