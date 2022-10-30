#define _GNU_SOURCE

#include <err.h>
#include <errno.h>
#include <locale.h>
#include <stdbool.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#define DEF_BLOCK_SIZE 1024

size_t roundToNearestBlockSize(size_t, size_t);

static const char* random_source = "/dev/urandom";
extern char *program_invocation_short_name;

size_t roundToNearestBlockSize(size_t size, size_t block_size) {
    if(size && size <= block_size) return block_size;
    size_t div = size / block_size;
    return div * block_size;
}

int main(int argc, char **argv) {

    if(!setlocale(LC_ALL, "C"))
        fprintf(stderr, "Failed to system set locale to C\n");

    const char *file_path = "./test.file";
    struct stat file_info;

    bool round_to_nearest_blk_size = true;
    bool is_regular_file;

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

    uint8_t bytes_buffer[DEF_BLOCK_SIZE];

    printf("Size: %zu\n", file_info.st_size);
    printf("Block size: %zu\n", file_info.st_blksize);

    int rand_fd = open(random_source, O_RDONLY);

    if(rand_fd == -1) {
        perror("open");
        return 1;
    }

    size_t target_bytes = 0;

    if(round_to_nearest_blk_size) {
        target_bytes = roundToNearestBlockSize(file_info.st_size, file_info.st_blksize);
    } else target_bytes = file_info.st_size;

    ssize_t bytesr = 0;
    ssize_t bytesw = 0;

    size_t buffer_size = DEF_BLOCK_SIZE;

    printf("Bytes to write: %zu\n", target_bytes);
    while(bytesw < target_bytes) {

        size_t bytes_left = target_bytes - bytesw;

        buffer_size = bytes_left > buffer_size ? DEF_BLOCK_SIZE : bytes_left;

        size_t br = read(rand_fd, bytes_buffer, buffer_size);
        if(br == -1) {
            perror("read");
            return 1;
        }

        size_t bw = write(fd, bytes_buffer, br);
        if(bw == -1) {
            perror("write");
            return 1;
        }

        bytesw += bw;
    }
    printf("Total bytes written: %zu\n", bytesw);

    close(fd);
    close(rand_fd);

    return 0;
}
