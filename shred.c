#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 500

#include <err.h>
#include <errno.h>
#include <locale.h>
#include <stdbool.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#define DEF_BLOCK_SIZE 1024 // Must be a power of 2

size_t roundToNearestBlockSize(size_t, size_t);
ssize_t writeRandomBytes(int, size_t);
ssize_t getRandomBytes(const char*, void*, size_t);

extern char *program_invocation_short_name;
static const char* random_source = "/dev/urandom";
bool exact = false;

static int rand_fd = -1;

size_t roundToNearestBlockSize(size_t size, size_t block_size) {
    if(size <= block_size) return block_size;
    if(size && (size % block_size) == 0) return size;

    size_t div = (size / block_size) + 1;
    return div * block_size;
}

ssize_t readRandomBytes(const char* random_source, void *buff, size_t buff_size) {
    static bool opened = false;

    if(!opened) {
        rand_fd = open(random_source, O_RDONLY);

        if(rand_fd == -1) {
            perror("open");
            return -1;
        }

        opened = true;
    }

    ssize_t bytesr = read(rand_fd, buff, buff_size);
    if(bytesr == -1) {
        perror("read");
        return -1;
    }

    return bytesr;
}

ssize_t writeRandomBytes(int fd, size_t size) {
    ssize_t bytesw = 0;

    void *bytes_buffer = NULL;
    size_t buffer_size = DEF_BLOCK_SIZE;

    bytes_buffer = malloc(buffer_size);

    if(!bytes_buffer) {
        perror("malloc");
        return -1;
    }

    while(bytesw < size) {

        size_t bytes_left = size - bytesw;

        buffer_size = bytes_left > buffer_size ? DEF_BLOCK_SIZE : bytes_left;

        ssize_t br = readRandomBytes(random_source, bytes_buffer, buffer_size);

        if(br == -1) {
            fprintf(stderr, "%s: Failed to read random bytes from %s\n", program_invocation_short_name, random_source);
            exit(EXIT_FAILURE);
        }

        size_t bw = pwrite(fd, bytes_buffer, br, bytesw);

        if(bw == -1) {
            perror("write");
            return -1;
        }

        bytesw += bw;
    }

    free(bytes_buffer);

    return bytesw;
}

int main(int argc, char **argv) {
    (void) argc;
    (void) argv;

    if(!setlocale(LC_ALL, "C"))
        fprintf(stderr, "Failed to system set locale to C\n");

    const char *file_path = "./test.file";
    struct stat file_info;

    int fd = open(file_path, O_WRONLY | O_NOCTTY);

    if(fd == -1) {
        perror("open");
        return 1;
    }

    if(fstat(fd, &file_info) == -1) {
        perror("stat");
        return 1;
    }

    if ((S_ISCHR(file_info.st_mode) && isatty(fd)) || S_ISFIFO(file_info.st_mode) || S_ISSOCK(file_info.st_mode)) {
        fprintf(stderr, "%s: %s is not a valid file type\n", program_invocation_short_name, file_path);
        return 1;
    }

    printf("File size: %zu\n", file_info.st_size);
    printf("Block size: %zu\n", file_info.st_blksize);

    size_t target_bytes = 0;

    if(!exact) {
        target_bytes = roundToNearestBlockSize(file_info.st_size, file_info.st_blksize);
    } else target_bytes = file_info.st_size;

    size_t num_iter = 3;

    ssize_t total = 0;

    for(size_t iter = 0; iter < num_iter; iter++) {
        ssize_t bytesw = writeRandomBytes(fd, target_bytes);

        if(bytesw == -1) {
            fprintf(stderr, "%s: Failed to write random bytes to %s\n", program_invocation_short_name, file_path);
            return 1;
        }

        total += bytesw;
    }

    printf("Written bytes: %zu\n", total / num_iter);

    close(fd);
    if(rand_fd) close(rand_fd);

    return 0;
}
