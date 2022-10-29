#define _GNU_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <errno.h>
#include <err.h>
#include <locale.h>

#define DEF_BLOCK_SIZE 1024

size_t roundToNearestBlockSize(size_t);
static const char* random_source = "/dev/urandom";

extern char *program_invocation_short_name;

int main(int argc, char **argv) {

    if(!setlocale(LC_ALL, "C"))
        fprintf(stderr, "Failed to system set locale to C\n");

    const char *file_path = "./test.file";
    struct stat file_info;

    bool round_to_nearest_blk_size = false;
    bool is_regular_file;

    if(stat(file_path, &file_info) == -1)
        perror("stat");

    is_regular_file = S_ISREG(file_info.st_mode);

    if(!is_regular_file) {
        fprintf(stderr, "%s: %s is not a regular file\n",
                program_invocation_short_name, file_path);
        return 1;
    }

    uint8_t bytes_buffer[DEF_BLOCK_SIZE];

    printf("Size: %zu\n", file_info.st_size);
    printf("Block size: %zu\n", file_info.st_blksize);

    return 0;
}
