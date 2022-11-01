#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 500

#include <errno.h>
#include <libgen.h>
#include <assert.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <getopt.h>
#include <locale.h>
#include <stdbool.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#define DEF_BLOCK_SIZE 1024 // Must be a power of 2

static long int parseAndRangeCheckArg(const char*);
size_t roundToNearestBlockSize(size_t, size_t);
ssize_t writeRandomBytes(int, size_t);
ssize_t getRandomBytes(const char*, void*, size_t);
bool incname (char*, size_t);

static char const nameset[] =
"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_.";

typedef int (*rm_func_proto)(const char*);

int rm_func_unlink(const char*);
int rm_func_wipe(const char*);
int rm_func_wipesync(const char*);

rm_func_proto rm_func_table[] = {
    NULL, rm_func_unlink, rm_func_wipe, rm_func_wipesync
};

enum rm_method {
    rm_none = 0,
    rm_unlink,
    rm_wipe,
    rm_wipesync
};

static const char *rm_method_names[] = {
    NULL, "unlink", "wipe", "wipesync"
};

int rm_func_unlink(const char* path) {
    int retval = unlink(path);
    if(retval != 0) return -errno;
    return 1;
}

int rm_func_wipe(const char* path) {

    char *oldname = strdup(path);
    char *newname = strdup(path);

    char *base = basename(newname);
    size_t base_len = strlen(base);

    for(size_t len = base_len; len != 0; len--) {
        memset(base, nameset[0], len);
        base[len] = '\0';
        bool rename_ok;

        /*printf("%s -> %s\n", oldname, newname);*/

        while(!(rename_ok = (renameat2(AT_FDCWD, oldname, AT_FDCWD, newname, RENAME_NOREPLACE) == 0))
                        && errno == EEXIST && incname(base, len))
            continue;

        if(rename_ok) memcpy(oldname + (base - newname), base, len + 1);
        else {
            fprintf(stderr, "Failed to unlink\n");
            return -errno;
        }
    }

    int retval = unlink(newname);
    if(retval != 0) return -errno;

    free(newname);
    free(oldname);

    return 1;
}

int rm_func_wipesync(const char* path) {
    fprintf(stderr, "Not implemented\n");
    return -1;
}

static const size_t rm_method_names_size = sizeof(rm_method_names) / sizeof(rm_method_names[0]);

static const struct option long_options[] = {
    {"iterations", required_argument, NULL, 'n'},
    {"size", required_argument, NULL, 's'},
    {"remove", required_argument, NULL, 'r'},
    {"exact", no_argument, NULL, 'x'},
    {"zero", no_argument, NULL, 'z'},
    {"verbose", no_argument, NULL, 'v'},
    {"help", no_argument, NULL, 'h'},
};

struct flags {
    int iterations;
    size_t size;
    enum rm_method how_remove;
    bool exact;
    bool zero;
    bool verbose;
};

extern char *program_invocation_short_name;
static const char* random_source = "/dev/urandom";

static int rand_fd = -1;
static int def_iter = 3;
static enum rm_method def_remove = rm_wipesync;

bool incname(char *name, size_t len) {
    while(len--) {
        char const *p = strchr(nameset, name[len]);

        assert (p);

        if(p[1]) {
            name[len] = p[1];
            return true;
        }

        name[len] = nameset[0];
    }

    return false;
}

static long int parseAndRangeCheckArg(const char *arg) {
    char *end = NULL;

    intmax_t result = strtoimax(arg, &end, 10);
    if(*end != '\0') return -1;

    if(result < 0 || result == INTMAX_MAX || result == INTMAX_MIN) return -1;
    return (long) result;
}

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

        fsync(fd);

        bytesw += bw;
    }

    free(bytes_buffer);

    return bytesw;
}

int main(int argc, char **argv) {
    if(!setlocale(LC_ALL, "C"))
        fprintf(stderr, "Failed to system set locale to C\n");


    int exit_status = 0;
    int opt;
    struct flags prog_flags = { 0, };

    while((opt = getopt_long(argc, argv, "n:s:r:vxhu", long_options, NULL)) != -1) {
        switch(opt) {
            case 'n': {
                int result = parseAndRangeCheckArg(optarg);

                if(result == -1) {
                    fprintf(stderr, "%s: Invalid number of itterations: %s\n", program_invocation_short_name, optarg);
                    return 1;
                }

                prog_flags.iterations = result;
                break;
            }

            case 's': {
                int result = parseAndRangeCheckArg(optarg);
                if(result == -1) {
                    fprintf(stderr, "%s: Invalid number size argument: %s\n", program_invocation_short_name, optarg);
                    return 1;
                }

                prog_flags.size = result;
                prog_flags.exact = false;
                break;
            }

            case 'r': {
                bool chosen = false;
                int idx = 1;
                for(; idx < rm_method_names_size; idx++) {
                    if(!strncmp(rm_method_names[idx], optarg, 10)) {
                        chosen = true;
                        break;
                    }
                }

                if(!chosen) {
                    fprintf(stderr, "%s: Invalid option name for remove flag: %s\n", program_invocation_short_name, optarg);
                    return 1;
                }

                prog_flags.how_remove = idx;
                break;
            }

            case 'u':
                prog_flags.how_remove = def_remove;
                break;

            case 'v':
                prog_flags.verbose = true;
                break;

            case 'x':
                prog_flags.exact = true;
                prog_flags.size = 0;
                break;

            case 'z':
                prog_flags.zero = true;
                break;

            case 'h':
                fprintf(stderr, "help me :(\n");
                return 1;

            default:
                printf("Unknown argument\n");
        }
    }

    prog_flags.iterations =  prog_flags.iterations == 0 ? def_iter : prog_flags.iterations;

    printf("Remove method %s [%i]\n", rm_method_names[prog_flags.how_remove], prog_flags.how_remove);
    struct stat file_info;

    if(argc == optind) {
        fprintf(stderr, "%s: No input files given\n", program_invocation_short_name);
        return 1;
    }

    for(int idx = optind; idx < argc; idx++) {
        const char *file_path = argv[idx];

        int fd = open(file_path, O_WRONLY | O_NOCTTY);

        if(fd == -1) {
            perror("open");
            exit_status &= 1;
            continue;
        }

        if(fstat(fd, &file_info) == -1) {
            perror("stat");
            exit_status &= 1;
            goto final;
        }

        if((S_ISCHR(file_info.st_mode) && isatty(fd)) || S_ISFIFO(file_info.st_mode) || S_ISSOCK(file_info.st_mode)) {
            fprintf(stderr, "%s: %s is not a valid file type\n", program_invocation_short_name, file_path);
            exit_status &= 1;
            goto final;
        }

        printf("File size: %zu\n", file_info.st_size);
        printf("Block size: %zu\n", file_info.st_blksize);

        size_t target_bytes = 0;

        if(prog_flags.exact) target_bytes = file_info.st_size;
        else if(prog_flags.size != 0) {
            if(!S_ISREG(file_info.st_mode) && prog_flags.size > file_info.st_size) target_bytes = file_info.st_size;
            target_bytes = prog_flags.size;
        } else target_bytes = roundToNearestBlockSize(file_info.st_size, file_info.st_blksize);

        ssize_t total = 0;

        for(size_t iter = 0; iter < prog_flags.iterations; iter++) {
            ssize_t bytesw = writeRandomBytes(fd, target_bytes);

            if(bytesw == -1) {
                fprintf(stderr, "%s: Failed to write random bytes to %s\n", program_invocation_short_name, file_path);
                goto final;
            }

            total += bytesw;
        }

        printf("Written bytes: %zu (%zu)\n", total, total / prog_flags.iterations);

        if(prog_flags.how_remove != 0) {
            printf("Remove method: %s\n", rm_method_names[prog_flags.how_remove]);
            int retval = rm_func_table[prog_flags.how_remove](file_path);
            if(retval < 0) {
                fprintf(stderr, "%s: Failed to remove %s: %s\n", program_invocation_short_name, file_path, strerror(-retval));
                goto final;
            }
        }

        final:
        close(fd);
    }

    if(rand_fd) close(rand_fd);

    return exit_status;
}
