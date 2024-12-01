#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define BUFFER_SIZE 1024

void exit_with_error(const char *msg, int code) {
    fprintf(stderr, "%s\n", msg);
    exit(code);
}

int secure_read_line(int file_descriptor, char *dest, int buffer_limit, int must_have_newline) {
    int index = 0, bytes_read;
    char character;
    int found_newline = 0;

    while (index < buffer_limit - 1) {
        bytes_read = read(file_descriptor, &character, 1);
        if (bytes_read == 1) {
            if (character == '\n') {
                found_newline = 1;
                break;
            }
            dest[index++] = character;
        } else if (bytes_read == 0) {
            break;
        } else {
            close(file_descriptor);
            exit_with_error("Operation Failed", 1);
        }
    }
    dest[index] = '\0';

    if (must_have_newline && !found_newline && index > 0) {
        return -1;
    }
    return index;
}

int main(void) {
    char command[BUFFER_SIZE], target[BUFFER_SIZE];
    int fd, bytes_read;
    char read_buffer[BUFFER_SIZE], extra_input[BUFFER_SIZE];
    struct stat file_stats;

    if (secure_read_line(STDIN_FILENO, command, sizeof(command), 1) <= 0) {

        exit_with_error("Invalid Command", 1);
    }

    if (!strcmp(command, "get")) {
        if (secure_read_line(STDIN_FILENO, target, sizeof(target), 1) <= 0) {
            exit_with_error("Invalid Command", 1);
        }

        if (stat(target, &file_stats) != 0) {
            exit_with_error("Invalid Command", 1);
        }

        if (!S_ISREG(file_stats.st_mode)) {
            exit_with_error("Operation Failed", 1);
        }

        if (file_stats.st_size == 0) {
            exit_with_error("Invalid Command", 1);
        }

        if (secure_read_line(STDIN_FILENO, extra_input, sizeof(extra_input), 0) > 0) {
            exit_with_error("Invalid Command", 1);
        }

        fd = open(target, O_RDONLY);
        if (fd == -1) {
            close(fd);
            exit_with_error("Invalid Command", 1);
        }

        while ((bytes_read = read(fd, read_buffer, BUFFER_SIZE)) > 0) {
            if (write(STDOUT_FILENO, read_buffer, bytes_read) != bytes_read) {
                close(fd);
                exit_with_error("Write Error", 1);
            }
        }
        close(fd);
    } else if (!strcmp(command, "set")) {
        if (secure_read_line(STDIN_FILENO, target, sizeof(target), 1) <= 0) {
            exit_with_error("Invalid Command", 1);
        }

        if (secure_read_line(STDIN_FILENO, read_buffer, sizeof(read_buffer), 1) <= 0) {
            exit_with_error("Invalid Command", 1);
        }

        int content_length = atoi(read_buffer);
        if (content_length < 0) {
            exit_with_error("Invalid Command", 1); // Check for negative content length
        }

        fd = open(target, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) {
            close(fd);
            exit_with_error("Operation Failed", 1);
        }

        int total_written = 0;
        // If content length is zero, check for immediate newline
        if (content_length == 0) {
            bytes_read = read(STDIN_FILENO, read_buffer, 1); // Check if there is any more input
            if (bytes_read == 1 && read_buffer[0] != '\n') {
                close(fd);
                exit_with_error("Invalid Command", 1); // Extra data when none should be present
            }
            printf("OK\n");
            close(fd);
            return 0;
        }

        while (total_written < content_length) {
            int to_read = (BUFFER_SIZE < content_length - total_written)
                              ? BUFFER_SIZE
                              : content_length - total_written;
            bytes_read = read(STDIN_FILENO, read_buffer, to_read);
            if (bytes_read < 0) {
                close(fd);
                exit_with_error("Read Error", 1);
            }
            if (bytes_read == 0) { // Handle EOF
                // Check if we have written enough bytes
                if (total_written == 0
                    && content_length > 0) { // No content was provided but was expected
                    close(fd);
                    exit_with_error("Invalid Command", 1); // Not enough data provided
                }
                break;
            }
            if (write(fd, read_buffer, bytes_read) != bytes_read) {
                close(fd);
                exit_with_error("Write Error", 1);
            }
            total_written += bytes_read;
        }

        close(fd);
        printf("OK\n");
    } else {
        exit_with_error("Invalid Command", 1);
    }
    return 0;
}
