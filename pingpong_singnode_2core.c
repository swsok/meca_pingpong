#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <pthread.h>
#include <sched.h>
#include <errno.h>

//#define TARGET_OFFSET 0x0UL
//#define DEVMEM "./mem"

#define TARGET_OFFSET 0x200000000UL
#define DEVMEM "/dev/mem"

/* Shared pointers into the mmap'd page */
static volatile uint64_t *shared_value;
static volatile int *turn_flag;
static int loop_count;

/* Thread 1: starts the ping–pong by writing 0, then alternately reads, increments, writes */
void *thread1_func(void *arg)
{
    /* Pin this thread to CPU 0 */
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset) !=
	0) {
	perror("pthread_setaffinity_np (thread1)");
    }

    /* Initialize the value and turn */
    shared_value[0] = 0;
    turn_flag[0] = 1;		/* 1 → thread1's turn, 2 → thread2's turn */

    for (int i = 0; i < loop_count; i++) {
	/* busy-wait for our turn */
	while (turn_flag[0] != 1) {	/* spin */
	}

	uint64_t v = shared_value[0];
	printf("Thread 1 received: %lu\n", v);

	v++;
	shared_value[0] = v;
	printf("Thread 1 wrote:    %lu\n", v);

	/* hand off to thread2 */
	turn_flag[0] = 2;
    }

    return NULL;
}

/* Thread 2: waits for thread1, then reads, increments, writes back, and hands off */
void *thread2_func(void *arg)
{
    /* Pin this thread to CPU 1 */
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(1, &cpuset);
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset) !=
	0) {
	perror("pthread_setaffinity_np (thread2)");
    }

    for (int i = 0; i < loop_count; i++) {
	/* busy-wait for our turn */
	while (turn_flag[0] != 2) {	/* spin */
	}

	uint64_t v = shared_value[0];
	printf("Thread 2 received: %lu\n", v);

	v++;
	shared_value[0] = v;
	printf("Thread 2 wrote:    %lu\n", v);

	/* hand back to thread1 */
	turn_flag[0] = 1;
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    int use_devmem = 0;
    int fd = -1;
    void * map_base = NULL;
    long page_size = 0;

    if (argc != 3) {
	fprintf(stderr, "Usage: %s <0|1: local|devmem> <loop_count>\n",
		argv[0]);
	return EXIT_FAILURE;
    }

    use_devmem = atoi(argv[1]);
    loop_count = atoi(argv[2]);
    if (loop_count <= 0) {
	fprintf(stderr, "Loop count must be a positive integer\n");
	return EXIT_FAILURE;
    }

    if (use_devmem) {
	/* Open /dev/mem (usually requires root) */
	fd = open(DEVMEM, O_RDWR | O_SYNC);
	if (fd < 0) {
	    perror("open(\"/dev/mem\")");
	    return EXIT_FAILURE;
	}

	/* mmap one page at the target physical offset */
	page_size = sysconf(_SC_PAGESIZE);
	map_base = mmap(NULL,
			      page_size,
			      PROT_READ | PROT_WRITE,
			      MAP_SHARED,
			      fd,
			      TARGET_OFFSET);
	if (map_base == MAP_FAILED) {
	    perror("mmap");
	    close(fd);
	    return EXIT_FAILURE;
	}
    } else {
	map_base = malloc(sizeof(uint64_t));
    }

    /* Set up pointers into that page:
       [0…7]    = uint64_t shared_value
       [8…11]   = int      turn_flag */
    shared_value = (volatile uint64_t *) map_base;
//    turn_flag    = (volatile int *)((char*)map_base + sizeof(uint64_t));
    turn_flag = (int *) malloc(sizeof(int));

    /* Launch the two threads */
    pthread_t t1, t2;
    if (pthread_create(&t1, NULL, thread1_func, NULL) != 0) {
	perror("pthread_create(thread1)");
	return EXIT_FAILURE;
    }
    if (pthread_create(&t2, NULL, thread2_func, NULL) != 0) {
	perror("pthread_create(thread2)");
	return EXIT_FAILURE;
    }

    /* Wait for them to finish */
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    /* Clean up */
    if (use_devmem) {
	    munmap(map_base, page_size);
    } else {
	    free(map_base);
    }
    free((void *) turn_flag);
    if (fd > 0) close(fd);

    return EXIT_SUCCESS;
}
