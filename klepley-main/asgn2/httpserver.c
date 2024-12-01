#include <arpa/inet.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "asgn2_helper_funcs.h"

#define BUFF_SIZE 4096

/* -----------------------------------------------------------------------------
 * Listener Socket Functions
 * These are identical to the functions in the asgn2 helper functions .a file
 * You can reference them by calling the same function names as listed in the .h
 * file and do not need to use the source code below.
 */
// typedef struct {
//     int fd;
// } Listener_Socket;

/*-----------------HELPER FUNCTIONS DEFINITIONS-------------------------------*/
int check_headers(const char *buffer);
void handle_request_based_on_method(
    int client_fd, const char *method, const char *uri, const char *version, const char *buffer);
void handle_get_request(int client_fd, const char *uri);
void handle_put_request(int client_fd, const char *uri, int content_length);
ssize_t transmit_data(int source_fd, int destination_fd, size_t size);
ssize_t read_to_limit(int fd, char buffer[], size_t limit);
/*-----------------------------------------------------------------------------*/

int listener_init(Listener_Socket *sock, int port) {
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if ((sock->fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return -1;
    }

    if (bind(sock->fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        return -1;
    }

    if (listen(sock->fd, 128) < 0) {
        return -1;
    }

    return 0;
}

int listener_accept(Listener_Socket *sock) {
    int connfd = accept(sock->fd, NULL, NULL);
    return connfd;
}
/* ------------------------------MY HELPER FUNCTION
 * CODE---------------------------------*/
int check_headers(const char *buffer) {
    const char *ptr = buffer;
    while ((ptr = strstr(ptr, "\r\n")) != NULL) {
        ptr += 2; // Move past the "\r\n"
        if (strncmp(ptr, "\r\n", 2) == 0)
            break; // End of headers

        const char *colon = strchr(ptr, ':');
        fprintf(stderr, "%s", colon);
        // if (colon == NULL || strchr(ptr, ';') != NULL) {
        if (colon == NULL) {
            return -1; // Malformed header found
        }
    }
    return 0;
}

void handle_request_based_on_method(
    int client_fd, const char *method, const char *uri, const char *version, const char *buffer) {
    // Validating HTTP version first
    if (strcmp(version, "HTTP/1.1") != 0) {
        // Respond with 400 if the version is malformed or not properly formatted
        if (strcmp(version, "HTTP/1.10") == 0 || strcmp(version, "HTTP/1.0") == 0) {
            // fprintf(stderr, "here loser 4\n");
            write_n_bytes(client_fd,
                "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n", 60);
        } else {
            // For other non-1.1 versions that are not malformed but unsupported
            write_n_bytes(client_fd,
                "HTTP/1.1 505 Version Not Supported\r\nContent-Length: "
                "22\r\n\r\nVersion Not Supported\n",
                80);
        }
        return;
    }

    // Now check for valid methods
    if (strcmp(method, "GET") == 0 || strcmp(method, "PUT") == 0) {
        if (strcmp(method, "GET") == 0) {
            handle_get_request(client_fd, uri);
        } else { // Method is PUT
            char *content_length_str = strstr(buffer, "Content-Length: ");
            if (!content_length_str) {
                // fprintf(stderr, "here loser 1\n");
                write_n_bytes(client_fd,
                    "HTTP/1.1 400 Bad Request\r\nContent-Length: "
                    "12\r\n\r\nBad Request\n",
                    60);
                return;
            }
            int content_length = 0;
            sscanf(content_length_str, "Content-Length: %d", &content_length);
            // char *body = strstr(buffer, "\r\n\r\n") + 4;  // Start of the body
            // printf("%d", content_length);
            handle_put_request(client_fd, uri, content_length);
        }
    } else {
        // If the method is known but not implemented (e.g., "DELETE", "PATCH")
        if (strstr(method, "GET")) {
            write_n_bytes(client_fd,
                "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n", 60);
            return;
        }
        write_n_bytes(client_fd,
            "HTTP/1.1 501 Not Implemented\r\nContent-Length: "
            "16\r\n\r\nNot Implemented\n",
            68);
    }
}

void handle_get_request(int client_fd, const char *uri) {
    char filepath[256];
    strcpy(filepath, uri + 1); // Removing the leading '/'

    struct stat path_stat;
    if (stat(filepath, &path_stat) < 0) {
        // If stat fails, send a 404 Not Found or handle the error appropriately
        write_n_bytes(
            client_fd, "HTTP/1.1 404 Not Found\r\nContent-Length: 10\r\n\r\nNot Found\n", 62);
        return;
    }

    if (S_ISDIR(path_stat.st_mode)) {
        // If the path is a directory, return 403 Forbidden
        write_n_bytes(
            client_fd, "HTTP/1.1 403 Forbidden\r\nContent-Length: 10\r\n\r\nForbidden\n", 62);
        return;
    }

    int file_fd = open(filepath, O_RDONLY);
    if (file_fd < 0) {
        write_n_bytes(
            client_fd, "HTTP/1.1 404 Not Found\r\nContent-Length: 10\r\n\r\nNot Found\n", 62);
        return;
    }

    ssize_t length = path_stat.st_size;
    char header[256];
    sprintf(header, "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\n\r\n", length);
    write_n_bytes(client_fd, header, strlen(header));
    pass_n_bytes(file_fd, client_fd, length);
    close(file_fd);
}

void handle_put_request(int client_fd, const char *uri, int content_length) {
    if (content_length == -1) {
        // fprintf(stderr, "here loser 3\n");
        write_n_bytes(
            client_fd, "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n", 60);
        return;
    }

    char filepath[256];
    strcpy(filepath,
        uri + 1); // Remove the first '/' from the URI to get the filename

    struct stat path_stat;
    stat(filepath, &path_stat);
    if (S_ISDIR(path_stat.st_mode)) {
        write_n_bytes(
            client_fd, "HTTP/1.1 403 Forbidden\r\nContent-Length: 10\r\n\r\nForbidden\n", 64);
        return;
    }

    // Handling file creation or writing to an existing file
    int file_exists = 0;
    if (access(filepath, F_OK) == 0) {
        file_exists = 1;
    }
    errno = 0;
    int fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    // if (errno == EEXIST) {
    //     fprintf(stderr, "DONEEEEEEEEE\n");
    // }
    if (fd == -1) {
        // if (errno == EEXIST) {
        //     fd = open(filepath, O_WRONLY | O_TRUNC, 0666);
        // }
        if (fd == -1) {
            write_n_bytes(client_fd,
                "HTTP/1.1 500 Internal Server Error\r\nContent-Length: "
                "22\r\n\r\nInternal Server Error\n",
                86);
            return;
        }
    }
    // printf("%c", body[0]);
    int bytes_written = transmit_data(client_fd, fd, content_length);
    // write_n_bytes(fd, "\n", 1);
    if (bytes_written != content_length) {
        close(fd);
        // fprintf(stderr, "this is the problem\n");
        write_n_bytes(client_fd,
            "HTTP/1.1 500 Internal Server Error\r\nContent-Length: "
            "22\r\n\r\nInternal Server Error\n",
            86);
        return;
    }

    if (file_exists == 1) {
        // exit(1);
        char err[] = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nOK\n";
        write_n_bytes(client_fd, err, strlen(err));
        // fprintf(stderr, "exit\n");
        //  exit(1);
        close(fd);

    } else {
        // if i switch to 201 its wrong
        // sometimes fails bc no new line
        //  write_n_bytes(fd, "\n", 1);
        // fprintf(stderr, "before exit 201\n");
        //  exit(1);
        char err[] = "HTTP/1.1 201 Created\r\nContent-Length: 7\r\n\r\nCreated\n";
        write_n_bytes(client_fd, err, strlen(err));
        // fprintf(stderr, "exit\n");
        // exit(1);
        close(fd);
    }
}

ssize_t transmit_data(int source_fd, int destination_fd, size_t size) {
    char data_buffer[BUFF_SIZE];
    // Loop until all bytes are passed or an error occurs
    for (size_t bytes_left = size; bytes_left > 0;) {
        // Read a chunk of data into the buffer
        ssize_t bytes_read
            = read(source_fd, data_buffer, bytes_left < BUFF_SIZE ? bytes_left : BUFF_SIZE);
        if (bytes_read <= 0) {
            return bytes_read; // Return on error or end of file
        }
        // Write the read data to the destination
        if (write_n_bytes(destination_fd, data_buffer, bytes_read) < 0) {
            return -1; // Return on error
        }
        bytes_left -= bytes_read; // Update remaining bytes
    }
    return size; // Return total size passed
}

ssize_t read_to_limit(int fd, char buffer[], size_t limit) {
    size_t total_read = 0; // Track total bytes read
    char *buf_ptr = buffer; // Pointer to the current position in buffer

    while (total_read < limit) {
        ssize_t bytes_read = read(fd, buf_ptr, 1); // Read one character
        if (bytes_read == -1) {
            if (errno == EINTR) {
                bytes_read = 0;
            } else {
                return -1; // Return -1 for other errors
            }
        } else if (bytes_read == 0) {
            break; // EOF
        } else {
            total_read += bytes_read; // Update total bytes read
            buf_ptr += bytes_read; // Move buffer pointer
            // Checking for end of headers: '\r\n\r\n', '\n\n', or '\r\r'
            if (total_read >= 4
                && (memcmp(buf_ptr - 4, "\r\n\r\n", 4) == 0 || memcmp(buf_ptr - 2, "\n\n", 2) == 0
                    || memcmp(buf_ptr - 2, "\r\r", 2) == 0)) {
                break;
            }
        }
    }
    if (total_read < limit) {
        *buf_ptr = '\0';
    }
    return total_read; // Return total bytes read
}

/*------------------END OF MY HELPER FUNCTIONS--------------------------------*/

void handle_connection(int connfd) {
    /* Handle connection */

    char buf[BUFF_SIZE + 1];
    write_n_bytes(connfd, buf, BUFF_SIZE);
    buf[BUFF_SIZE] = '\0';
    printf("%s", buf);

    close(connfd);
    return;
}

ssize_t find_length(const char *path) {
    struct stat file;
    stat(path, &file);
    // sprintf(length_str, "%ld", file.st_size);
    return file.st_size;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: ./httpserver <port>\n");
        return 1;
    }

    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid Port\n");
        return 1;
    }

    Listener_Socket listener;
    if (listener_init(&listener, port) != 0) {
        fprintf(stderr, "Invalid Port\n");
        return 1;
    }

    while (1) {
        int client_fd = listener_accept(&listener);
        if (client_fd < 0) {
            perror("Failed to accept connection");
            continue;
        }

        char buffer[BUFF_SIZE];
        ssize_t bytes_read = read_to_limit(client_fd, buffer, BUFF_SIZE);
        if (bytes_read <= 0) {
            perror("Failed to read from socket");
            close(client_fd);
            continue;
        }

        buffer[bytes_read] = '\0'; // Null-terminate the buffer to make it a valid
            // string for parsing

        if (check_headers(buffer) != 0) {
            // fprintf(stderr, "here loser 2\n");
            write_n_bytes(client_fd,
                "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n", 60);
            close(client_fd);
            continue;
        }

        char method[10], uri[256], version[10];
        sscanf(buffer, "%s %s %s", method, uri, version); // Parse the request line

        // Use the improved method checking
        handle_request_based_on_method(client_fd, method, uri, version, buffer);

        close(client_fd);
        // fprintf(stderr, "closed\n");
    }

    return 0;
}
