#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sched.h>
#include <errno.h>

/* Shared data structure */
struct shared_data {
    volatile uint64_t shared_value;
    volatile int turn_flag;
};

int main(int argc, char *argv[])
{
    int fd;
    void *map_base;
    struct shared_data *shared_region;
    int loop_count;
    char *filepath;
    off_t offset;
    long page_size;

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <loop_count> <filepath> <offset>\n", argv[0]);
        return EXIT_FAILURE;
    }

    loop_count = atoi(argv[1]);
    if (loop_count <= 0) {
        fprintf(stderr, "Loop count must be a positive integer\n");
        return EXIT_FAILURE;
    }

    filepath = argv[2];
    offset = strtoul(argv[3], NULL, 0);

    page_size = sysconf(_SC_PAGESIZE);
    if (offset % page_size != 0) {
        fprintf(stderr, "Offset must be a multiple of the page size (%ld)\n", page_size);
        return EXIT_FAILURE;
    }

    /* Open the shared memory file */
    fd = open(filepath, O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
        perror("open");
        return EXIT_FAILURE;
    }

    /* Set the size of the shared memory file */
    if (ftruncate(fd, sizeof(struct shared_data) + offset) == -1) {
        perror("ftruncate");
        close(fd);
        return EXIT_FAILURE;
    }

    /* Map the shared memory file */
    map_base = mmap(NULL, sizeof(struct shared_data), PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset);
    if (map_base == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return EXIT_FAILURE;
    }
    close(fd); // fd is no longer needed after mmap

    shared_region = (struct shared_data *)map_base;

    /* Initialize the value and turn */
    shared_region->shared_value = 0;
    shared_region->turn_flag = 1; /* 1 -> ping's turn, 2 -> pong's turn */

    printf("Ping starting. Initial value: 0, turn: 1\n");

    for (int i = 0; i < loop_count; i++) {
        /* busy-wait for our turn */
        while (shared_region->turn_flag != 1) { /* spin */
        }

        uint64_t v = shared_region->shared_value;
        printf("Ping received: %lu\n", v);

        v++;
        shared_region->shared_value = v;
        printf("Ping wrote:    %lu\n", v);

        /* hand off to pong */
        shared_region->turn_flag = 2;
    }

    /* Clean up */
    munmap(map_base, sizeof(struct shared_data));

    return EXIT_SUCCESS;
}