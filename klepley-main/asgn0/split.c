#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <delimiter> <file1> [file2...]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *delimiter = argv[1];
    if (strlen(delimiter) != 1) {
        fprintf(stderr, "%s length is not 1\n", delimiter);
        exit(EXIT_FAILURE);
    }

    int error_flag = 0;
    int invalid_file_error_flag = 0; // Track error in invalid files
    int valid_file_count = 0;

    for (int i = 2; i < argc; ++i) {
        char *filename = argv[i];

        int fd = open(filename, O_RDONLY);
        if (fd == -1) {
            perror("open");
            error_flag = 1;
            invalid_file_error_flag = 1; // Set the flag for invalid file
            continue;
        }

        char buffer[BUFFER_SIZE];

        ssize_t bytes_read;
        while ((bytes_read = read(fd, buffer, BUFFER_SIZE)) > 0) {
            for (ssize_t j = 0; j < bytes_read; j += 1) {
                if (*delimiter == *(buffer + j)) {
                    *(buffer + j) = '\n';
                }
            }
            write(STDOUT_FILENO, buffer, bytes_read);
        }

        if (bytes_read == -1) {
            perror("read");
            error_flag = 1;
        }

        close(fd);

        valid_file_count++;
    }

    // If there were no valid files encountered
    if (valid_file_count == 0) {
        fprintf(stderr, "No valid files provided.\n");
        exit(EXIT_FAILURE);
    }

    // Return 0 if no errors or if the last file encountered an error but not the one before it
    if (!error_flag || (invalid_file_error_flag && valid_file_count > 1)) {
        return EXIT_SUCCESS;
    } else {
        return EXIT_FAILURE;
    }
}
