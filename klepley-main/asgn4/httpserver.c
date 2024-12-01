#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <regex.h>
#include <signal.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "asgn2_helper_funcs.h"
#include "queue.h"
#include "rwlock.h"

/***********DEFS************/
#ifndef O_DIRECTORY
#define O_DIRECTORY 0
#endif
#define REQUEST_REGEX "^([a-zA-Z]{1,8}) /([a-zA-Z0-9.-]{1,63}) (HTTP/[0-9]\\.[0-9])\r\n"
#define HEADER_REGEX  "([a-zA-Z0-9.-]{1,128}): ([ -~]{1,128})\r\n"
#define BUFFER_SIZE   4096
#define PATH_MAX      4069

/*****************STRUCT DEFS************/
queue_t *request_queue;
pthread_mutex_t log_mutex;
rwlock_t *rw_lock;
typedef struct list list_t;

/***********ACTUAL STRUCTS************/
typedef struct thread_container {
    queue_t *queue;
    list_t *list;
} thread_container;

typedef struct user_req {
    char *target;
    char *http_version;
    char *body;
    int content_len;
    int id;
    char *command;
    int socket_fd;
    int remaining_len;
} user_req;

typedef struct list_node {
    struct list_node *first;
    struct list_node *last;
    rwlock_t *lock;
    char path[PATH_MAX];
} list_node;

typedef struct linked_list {
    list_node *head;
    list_node *tail;
    int size;
    pthread_mutex_t mutex;
} linked_list;

/*******LIST FUNCTION DEFS******************/
linked_list *create_list();
list_node *find_in_list(linked_list *list, char *path);

bool push_to_list(linked_list *list, char *path);
bool lock_and_push_to_list(linked_list *list, char *path);
bool lock_and_access_list(linked_list *list, char *path, bool write);
bool unlock_access_list(linked_list *list, char *path, bool write);

void delete_list(linked_list **list);

/*******MISC DEFS******************/
int server_port = 0;
int thread_count = 4;
volatile atomic_int server_shutdown = 0;
void parse_arguments(int count, char **values);
int parse_request(user_req *req, char *buffer, ssize_t buffer_len);
int handle_request(user_req *req, linked_list *list);
void handle_signal(int signo);
void log_entry(const char *operation, const char *path, int status, int id);
void *thread_worker();
void configure_signals();
int process_get(user_req *req);
int process_put(user_req *req);

/*****FUNCTIONS NEEDED FOR LIST FUNCTIONS TO WORK*******/

list_node *create_list_node(PRIORITY priority, char *path) {
    // Allocate memory for a new list node and initialize it to zero
    list_node *new_node = calloc(1, sizeof(list_node));
    // Initialize the previous and next pointers to NULL
    new_node->first = NULL;
    new_node->last = NULL;
    // Copy the provided path to the path field of the new node
    strcpy(new_node->path, path);
    // Create a new read-write lock with the given priority and some constant (4)
    new_node->lock = rwlock_new(priority, 4);
    // Return the newly created node
    return new_node;
}

thread_container *create_thread_container(queue_t *queue, list_t *list) {
    // Allocate memory for a new thread container and initialize it to zero
    thread_container *container = calloc(1, sizeof(thread_container));
    // Assign the provided queue and list to the container
    container->queue = queue;
    container->list = list;
    // Return the newly created thread container
    return container;
}

void clear_list(linked_list *list) {
    // Check if the list is not NULL
    if (list != NULL) {
        // Initialize a pointer to traverse the list starting from the head
        list_node *current = list->head;
        list_node *temp_node = NULL;
        // Loop through the list and free each node
        while (current != NULL) {
            // Store the next node
            temp_node = current->last;
            // Free the current node
            free(current);
            // Move to the next node
            current = temp_node;
        }
    }
}

/********LIST FUNCTIONS***************/

linked_list *create_list() {
    // Allocate memory for the new list and initialize it to zero
    linked_list *new_list = (linked_list *) calloc(1, sizeof(linked_list));
    // Initialize the list properties
    new_list->head = NULL;
    new_list->tail = NULL;
    new_list->size = 0;
    // Initialize the mutex associated with the list
    pthread_mutex_init(&(new_list->mutex), NULL);
    // Return the newly created list
    return new_list;
}

void delete_list(linked_list **list) {
    // Check if the list pointer and the list itself are not NULL
    if (list != NULL && *list != NULL) {
        // Clear all nodes in the list
        clear_list(*list);
        // Destroy the mutex associated with the list
        pthread_mutex_destroy(&((*list)->mutex));
        // Free the memory allocated for the list
        free(*list);
        // Set the list pointer to NULL
        *list = NULL;
    }
}

bool push_to_list(linked_list *list, char *path) {
    // Return false if the list is NULL
    if (!list) {
        return false;
    }
    // Create a new list node with the given path
    list_node *new_node = create_list_node(N_WAY, path);
    // If the list is empty, set the head and tail to the new node
    if (list->size == 0) {
        list->head = new_node;
        list->tail = new_node;
    } else {
        // Otherwise, update the head of the list
        list_node *temp_list = list->head;
        list->head = new_node;
        new_node->last = temp_list;
        temp_list->first = new_node;
    }
    // Increment the size of the list
    list->size++;
    // Return true to indicate success
    return true;
}

bool lock_and_push_to_list(linked_list *list, char *path) {
    // Lock the mutex associated with the list
    pthread_mutex_lock(&(list->mutex));
    // Traverse the list to check for duplicate paths
    list_node *current = list->head;
    while (current) {
        if (strcmp(path, current->path) == 0) {
            // Unlock the mutex and return false if a duplicate is found
            pthread_mutex_unlock(&(list->mutex));
            return false;
        }
        current = current->last;
    }
    // Push the new node to the list
    push_to_list(list, path);
    // Unlock the mutex
    pthread_mutex_unlock(&(list->mutex));
    // Return true to indicate success
    return true;
}

// Combined lock function for read and write
bool lock_and_access_list(linked_list *list, char *path, bool write) {
    pthread_mutex_lock(&(list->mutex));
    list_node *node = find_in_list(list, path);
    if (!node) {
        pthread_mutex_unlock(&(list->mutex));
        return false;
    }
    pthread_mutex_unlock(&(list->mutex));
    if (write) {
        writer_lock(node->lock);
    } else {
        reader_lock(node->lock);
    }
    return true;
}

// Combined unlock function for read and write
bool unlock_access_list(linked_list *list, char *path, bool write) {
    pthread_mutex_lock(&(list->mutex));
    list_node *node = find_in_list(list, path);
    if (!node) {
        pthread_mutex_unlock(&(list->mutex));
        return false;
    }
    if (write) {
        writer_unlock(node->lock);
    } else {
        reader_unlock(node->lock);
    }
    pthread_mutex_unlock(&(list->mutex));
    return true;
}

// Use combined functions instead of separate lock/unlock functions
bool lock_and_read_list(linked_list *list, char *path) {
    return lock_and_access_list(list, path, false);
}

bool lock_and_write_list(linked_list *list, char *path) {
    return lock_and_access_list(list, path, true);
}

bool unlock_read_list(linked_list *list, char *path) {
    return unlock_access_list(list, path, false);
}

bool unlock_write_list(linked_list *list, char *path) {
    return unlock_access_list(list, path, true);
}

bool lock_and_delete_from_list(linked_list *list, char *path) {
    // Lock the mutex associated with the list
    pthread_mutex_lock(&(list->mutex));
    // Initialize the current node pointer to the head of the list
    list_node *current = list->head;
    int index = 0;
    // Traverse the list to find the node with the given path
    while (current != NULL) {
        if (strcmp(path, current->path) == 0) {
            // Node with the matching path found
            list_node *node_to_delete = current;
            if (list->size == 1) {
                // If the list has only one node, set head and tail to NULL
                list->head = NULL;
                list->tail = NULL;
            } else if (index == 0) {
                // If the node is the first node in the list
                list->head = current->last;
                current->last->first = NULL;
            } else if (index == list->size - 1) {
                // If the node is the last node in the list
                list->tail = current->first;
                current->first->last = NULL;
            } else {
                // If the node is in the middle of the list
                current->last->first = node_to_delete->first;
                current->first->last = node_to_delete->last;
            }
            // Delete the read-write lock associated with the node
            rwlock_delete(&(node_to_delete->lock));
            // Free the memory allocated for the node
            free(node_to_delete);
            // Decrement the size of the list
            list->size--;
            // Unlock the mutex
            pthread_mutex_unlock(&(list->mutex));
            // Return true to indicate success
            return true;
        }
        // Move to the next node and increment the index
        index++;
        current = current->last;
    }
    // Unlock the mutex if no matching node was found
    pthread_mutex_unlock(&(list->mutex));
    // Return false to indicate failure
    return false;
}

list_node *find_in_list(linked_list *list, char *path) {
    // Start traversing from the head of the list
    list_node *current = list->head;
    // Traverse the list to find the node with the matching path
    while (current) {
        // Check if the current node's path matches the target path
        if (strcmp(path, current->path) == 0) {
            // Return the matching node
            return current;
        }
        // Move to the next node in the list
        current = current->last;
    }
    // Return NULL if no matching node is found
    return NULL;
}

/************Other Helper Functions************/

/***********PARSING AND HANDLING**************/

void parse_arguments(int count, char **values) {
    // Initialize variables for option parsing
    int opt_char = 0;
    char *options = "t:";
    // Parse command-line options
    opt_char = getopt(count, values, options);
    while (opt_char != -1) {
        if (opt_char == 't') {
            // Set the thread count from the option argument
            thread_count = atoi(optarg);
        } else {
            // Exit if an unknown option is encountered
            exit(EXIT_FAILURE);
        }
        opt_char = getopt(count, values, options);
    }
    // Check if a server port is specified as a non-option argument
    if (optind < count) {
        server_port = atoi(values[optind]);
    } else {
        // Print an error message and exit if no port number is provided
        fputs("Port number is required\n", stderr);
        exit(EXIT_FAILURE);
    }
}

int parse_request(user_req *req, char *buffer, ssize_t buffer_len) {
    // Initialize variables for regex and offsets
    int offset = 0;
    regex_t request_regex;
    regmatch_t matches[4];
    // Compile the request regex pattern
    if (regcomp(&request_regex, REQUEST_REGEX, REG_EXTENDED) != 0) {
        return EXIT_FAILURE;
    }
    // Execute regex to match the request line
    if (regexec(&request_regex, buffer, 4, matches, 0) == 0) {
        // Extract and null-terminate the command, target, and HTTP version
        req->command = buffer;
        req->target = buffer + matches[2].rm_so;
        req->http_version = buffer + matches[3].rm_so;
        buffer[matches[1].rm_eo] = '\0';
        req->target[matches[2].rm_eo - matches[2].rm_so] = '\0';
        req->http_version[matches[3].rm_eo - matches[3].rm_so] = '\0';
        // Update buffer and offset to point past the request line
        buffer += matches[3].rm_eo + 2;
        offset += matches[3].rm_eo + 2;
    } else {
        // Handle bad request
        dprintf(req->socket_fd,
            "HTTP/1.1 400 Bad Request\r\nContent-Length: %d\r\n\r\nBad Request\n", 12);
        log_entry(req->command, req->target, 400, req->id);
        regfree(&request_regex);
        return EXIT_FAILURE;
    }
    // Initialize content length and request ID
    req->content_len = -1;
    req->id = 0;
    // Compile the header regex pattern
    if (regcomp(&request_regex, HEADER_REGEX, REG_EXTENDED) != 0) {
        return EXIT_FAILURE;
    }
    // Parse headers
    while (regexec(&request_regex, buffer, 3, matches, 0) == 0) {
        // Null-terminate the header field and value
        buffer[matches[1].rm_eo] = '\0';
        buffer[matches[2].rm_eo] = '\0';
        // Process specific headers
        if (strncmp(buffer, "Content-Length", 14) == 0) {
            errno = 0;
            int value = strtol(buffer + matches[2].rm_so, NULL, 10);
            if (errno == EINVAL) {
                // Handle bad request for invalid content length
                dprintf(req->socket_fd,
                    "HTTP/1.1 400 Bad Request\r\nContent-Length: %d\r\n\r\nBad Request\n", 12);
                log_entry(req->command, req->target, 400, req->id);
                return EXIT_FAILURE;
            }
            req->content_len = value;
        } else if (strncmp(buffer, "Request-Id", 10) == 0) {
            int id = strtol(buffer + matches[2].rm_so, NULL, 10);
            req->id = id;
        }
        // Update buffer and offset to point past the current header
        buffer += matches[2].rm_eo + 2;
        offset += matches[2].rm_eo + 2;
    }
    // Check for end of headers and start of body
    if (buffer[0] == '\r' && buffer[1] == '\n') {
        req->body = buffer + 2;
        offset += 2;
        req->remaining_len = buffer_len - offset;
    } else {
        // Handle bad request for malformed headers
        dprintf(req->socket_fd,
            "HTTP/1.1 400 Bad Request\r\nContent-Length: %d\r\n\r\nBad Request\n", 12);
        log_entry(req->command, req->target, 400, req->id);
        regfree(&request_regex);
        return EXIT_FAILURE;
    }
    // Free the compiled regex
    regfree(&request_regex);
    return EXIT_SUCCESS;
}

int handle_request(user_req *req, linked_list *list) {
    // Add the request target to the list with locking
    lock_and_push_to_list(list, req->target);
    // Initialize variables to track lock acquisition and request status
    int lock_acquired = 0;
    int status = EXIT_FAILURE;
    // Check the HTTP version
    if (strncmp(req->http_version, "HTTP/1.1", 8) != 0) {
        // Respond with 505 Version Not Supported
        dprintf(req->socket_fd,
            "HTTP/1.1 505 Version Not Supported\r\nContent-Length: %d\r\n\r\nVersion Not "
            "Supported\n",
            12);
        log_entry(req->command, req->target, 505, req->id);
    } else if (strncmp(req->command, "GET", 3) == 0) {
        // Handle GET request
        lock_and_access_list(list, req->target, false);
        lock_acquired = 1;
        status = process_get(req);
    } else if (strncmp(req->command, "PUT", 3) == 0) {
        // Handle PUT request
        lock_and_access_list(list, req->target, true);
        lock_acquired = 1;
        status = process_put(req);
    } else {
        // Respond with 501 Not Implemented
        dprintf(req->socket_fd,
            "HTTP/1.1 501 Not Implemented\r\nContent-Length: %d\r\n\r\nNot Implemented\n", 12);
        log_entry(req->command, req->target, 501, req->id);
    }
    // Release locks if they were acquired
    if (lock_acquired) {
        if (strncmp(req->command, "GET", 3) == 0) {
            unlock_access_list(list, req->target, false);
        } else if (strncmp(req->command, "PUT", 3) == 0) {
            unlock_access_list(list, req->target, true);
        }
    }
    // Return the status of the request handling
    return status;
}

void handle_signal(int signo) {
    // Check if the signal is SIGINT or SIGTERM
    if (signo == SIGINT || signo == SIGTERM) {
        // Set the server shutdown flag atomically
        atomic_store(&server_shutdown, 1);
    }
}

//*******ADDITIONAL FUNCS*********************//

void log_entry(const char *operation, const char *path, int status, int id) {
    // Lock the mutex for logging
    pthread_mutex_lock(&log_mutex);

    // Log the operation and path
    fprintf(stderr, "%s,/%s,%d,%d\n", operation, path, status, id);

    // Unlock the mutex after logging
    pthread_mutex_unlock(&log_mutex);
}

void *thread_worker(void *list_ptr) {
    linked_list *list = (linked_list *) list_ptr;
    // Continue processing while the server is not shut down
    while (!atomic_load(&server_shutdown)) {
        uintptr_t client_socket;
        // Pop a request from the request queue
        if (!queue_pop(request_queue, (void **) &client_socket)) {
            continue;
        }
        // Initialize request buffer and user request structure
        char buffer[BUFFER_SIZE + 1] = { '\0' };
        user_req req;
        req.socket_fd = client_socket;
        // Read the request from the client socket
        ssize_t bytes_read = read_until(client_socket, buffer, BUFFER_SIZE, "\r\n\r\n");
        if (bytes_read == -1) {
            // Handle bad request
            dprintf(req.socket_fd,
                "HTTP/1.1 400 Bad Request\r\nContent-Length: %d\r\n\r\nBad Request\n", 12);
            log_entry(req.command, req.target, 400, req.id);
            close(client_socket);
            continue;
        }
        // Parse the request and handle it if parsing is successful
        if (parse_request(&req, buffer, bytes_read) != EXIT_FAILURE) {
            handle_request(&req, list);
        }
        // Clear the buffer and close the client socket
        memset(buffer, '\0', sizeof(buffer));
        close(client_socket);
    }
    return NULL;
}

void configure_signals() {
    // Configure signal handlers for SIGINT and SIGTERM
    if (signal(SIGINT, handle_signal) == SIG_ERR || signal(SIGTERM, handle_signal) == SIG_ERR) {
        perror("signal");
        exit(EXIT_FAILURE);
    }
}

/***********HANDLING GETS AND PUTS****************/
int process_get(user_req *req) {
    struct stat stat_buf;
    int file_fd = open(req->target, O_RDONLY | O_DIRECTORY);
    // Check for invalid request content length or remaining length
    if ((req->content_len != -1) || (req->remaining_len > 0)) {
        dprintf(req->socket_fd,
            "HTTP/1.1 400 Bad Request\r\nContent-Length: %d\r\n\r\nBad Request\n", 12);
        log_entry(req->command, req->target, 400, req->id);
        return EXIT_FAILURE;
    }
    // Check if the target is a directory
    if (file_fd != -1) {
        dprintf(
            req->socket_fd, "HTTP/1.1 403 Forbidden\r\nContent-Length: %d\r\n\r\nForbidden\n", 10);
        log_entry(req->command, req->target, 403, req->id);
        return EXIT_FAILURE;
    }
    // Open the target file
    file_fd = open(req->target, O_RDONLY);
    if (file_fd == -1) {
        int err_code;
        const char *message;
        // Determine error code and message based on errno
        if (errno == ENOENT) {
            err_code = 404;
            message = "HTTP/1.1 404 Not Found\r\nContent-Length: %d\r\n\r\nNot Found\n";
        } else if (errno == EACCES) {
            err_code = 403;
            message = "HTTP/1.1 403 Forbidden\r\nContent-Length: %d\r\n\r\nForbidden\n";
        } else {
            err_code = 500;
            message = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: %d\r\n\r\nInternal "
                      "Server Error\n";
        }
        // Respond with the appropriate error message and log the entry
        dprintf(req->socket_fd, message, 10);
        log_entry(req->command, req->target, err_code, req->id);
        return EXIT_FAILURE;
    }
    // Get the file size and send the response header
    fstat(file_fd, &stat_buf);
    off_t size = stat_buf.st_size;
    dprintf(req->socket_fd, "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\n\r\n", size);
    log_entry(req->command, req->target, 200, req->id);
    // Send the file content
    if (pass_n_bytes(file_fd, req->socket_fd, size) == -1) {
        dprintf(req->socket_fd,
            "HTTP/1.1 500 Internal Server Error\r\nContent-Length: %d\r\n\r\nInternal Server "
            "Error\n",
            22);
        log_entry(req->command, req->target, 500, req->id);
        close(file_fd);
        return EXIT_FAILURE;
    }
    close(file_fd);
    return EXIT_SUCCESS;
}

int process_put(user_req *req) {
    // Check if Content-Length header is present
    if (req->content_len == -1) {
        dprintf(req->socket_fd,
            "HTTP/1.1 400 Bad Request\r\nContent-Length: %d\r\n\r\nBad Request\n", 12);
        log_entry(req->command, req->target, 400, req->id);
        return EXIT_FAILURE;
    }
    int file_fd = open(req->target, O_WRONLY | O_CREAT | O_EXCL, 0666);
    int status_code = 0;
    // Check if the file already exists
    if (file_fd == -1) {
        if (errno == EEXIST) {
            file_fd = open(req->target, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            status_code = 200;
        } else {
            if (errno == EACCES) {
                dprintf(req->socket_fd,
                    "HTTP/1.1 403 Forbidden\r\nContent-Length: %d\r\n\r\nForbidden\n", 10);
                log_entry(req->command, req->target, 403, req->id);
            } else {
                dprintf(req->socket_fd,
                    "HTTP/1.1 500 Internal Server Error\r\nContent-Length: %d\r\n\r\nInternal "
                    "Server Error\n",
                    22);
                log_entry(req->command, req->target, 500, req->id);
            }
            return EXIT_FAILURE;
        }
    } else {
        status_code = 201;
    }
    ssize_t bytes_written = 0;
    ssize_t total_len = req->content_len;
    // Write the remaining content length from the request body
    if (req->remaining_len > 0) {
        bytes_written = write_n_bytes(file_fd, req->body, req->remaining_len);
        if (bytes_written == -1) {
            dprintf(req->socket_fd,
                "HTTP/1.1 500 Internal Server Error\r\nContent-Length: %d\r\n\r\nInternal Server "
                "Error\n",
                22);
            log_entry(req->command, req->target, 500, req->id);
            close(file_fd);
            return EXIT_FAILURE;
        }
        total_len -= bytes_written;
    }
    // Write any remaining content from the socket to the file
    if (total_len > 0) {
        bytes_written = pass_n_bytes(req->socket_fd, file_fd, total_len);
        if (bytes_written == -1) {
            dprintf(req->socket_fd,
                "HTTP/1.1 500 Internal Server Error\r\nContent-Length: %d\r\n\r\nInternal Server "
                "Error\n",
                22);
            log_entry(req->command, req->target, 500, req->id);
            close(file_fd);
            return EXIT_FAILURE;
        }
    }
    // Respond with the appropriate status code and log the entry
    if (status_code == 201) {
        dprintf(req->socket_fd, "HTTP/1.1 201 Created\r\nContent-Length: %d\r\n\r\nCreated\n", 8);
        log_entry(req->command, req->target, 201, req->id);
    } else {
        dprintf(req->socket_fd, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\nOK\n", 3);
        log_entry(req->command, req->target, 200, req->id);
    }

    close(file_fd);
    return EXIT_SUCCESS;
}

/*****MAIN CODE*********/

int main(int argc, char **argv) {
    // Create the linked list
    linked_list *list = create_list();
    // Parse command-line arguments and configure signal handlers
    parse_arguments(argc, argv);
    configure_signals();
    // Initialize the server listener socket
    Listener_Socket server_socket;
    int socket_fd = listener_init(&server_socket, server_port);
    if (socket_fd == -1) {
        fprintf(stderr, "Failed to initialize server socket\n");
        exit(EXIT_FAILURE);
    }
    // Initialize the request queue and other resources
    request_queue = queue_new(thread_count);
    pthread_mutex_init(&log_mutex, NULL);
    rw_lock = rwlock_new(N_WAY, 1);
    // Create worker threads
    pthread_t *threads = malloc(thread_count * sizeof(pthread_t));
    for (int i = 0; i < thread_count; i++) {
        pthread_create(&threads[i], NULL, thread_worker, (void *) list);
    }
    // Accept incoming client connections
    while (!atomic_load(&server_shutdown)) {
        uintptr_t client_socket = listener_accept(&server_socket);
        if (client_socket == (uintptr_t) -1) {
            if (atomic_load(&server_shutdown)) {
                break;
            }
            continue;
        }
        queue_push(request_queue, (void *) client_socket);
    }
    // Join worker threads
    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }
    // Clean up resources
    free(threads);
    delete_list(&list);
    queue_delete(&request_queue);
    pthread_mutex_destroy(&log_mutex);
    rwlock_delete(&rw_lock);
    close(socket_fd);

    return 0;
}
