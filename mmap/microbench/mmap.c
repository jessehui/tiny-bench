#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <sys/time.h>
#include <stdbool.h>
#include <pthread.h>

// ============================================================================
// Helper macros
// ============================================================================

#define KB                      (1024UL)
#define MB                      (1024 * 1024UL)
#define PAGE_SIZE               (4 * KB)

#define ALIGN_DOWN(x, a)        ((x) & ~(a-1)) // a must be a power of two
#define ALIGN_UP(x, a)          ALIGN_DOWN((x+(a-1)), (a))
#define ARRAY_SIZE(array)   (sizeof(array)/sizeof(array[0]))

#define MAX_BUF_NUM             65000

// ============================================================================
// Helper functions
// ============================================================================
static int check_bytes_in_buf(char *buf, size_t len, int expected_byte_val) {
    for (size_t bi = 0; bi < len; bi += 4*KB) {
        if (buf[bi] != (char)expected_byte_val) {
            printf("check_bytes_in_buf: expect %02X, but found %02X, at offset %lu\n",
                   (unsigned char)expected_byte_val, (unsigned char)buf[bi], bi);
            return -1;
        }
    }
    return 0;
}

// ============================================================================
// Main functions
// ============================================================================
static bool     TOUCH_PAGES = false;
static int      THREAD_NUM = 1;
static size_t   MAX_MMAP_SIZE = 4 * MB;
static volatile unsigned long g_mmap_time = 0, g_munmap_time = 0;
const int prot = PROT_READ | PROT_WRITE;
const int flags = MAP_PRIVATE | MAP_ANONYMOUS;

void print_usage() {
    fprintf(stderr, "Usage:\n ./mmap <-t (touch pages) > <-p thread_num> <-s rand_max_mmap_size> \n\n");
    // fprintf(stderr, " Now support testcase: <sigmask, sigdef>\n");
}

static void *run_main() {
    void *bufs[MAX_BUF_NUM] = {0};
    size_t lens[MAX_BUF_NUM];
    size_t num_bufs = 0;
    size_t used_memory = 0;
    size_t peak_mem_bufs = 0;

    struct timeval time_begin, time_end1, time_end2;
    unsigned long local_mmap_time = 0, local_munmap_time = 0;

    // Phrase 1: do mmap with random sizes until no more buffers or memory
    gettimeofday(&time_begin, NULL);
    for (num_bufs = 0;
            num_bufs < ARRAY_SIZE(bufs);
            num_bufs++) {
        // Choose the mmap size randomly but no bigger than 128 KB because if the size is
        // too big, the mmap time will be very small.
        size_t len = rand() % (MAX_MMAP_SIZE) + 1;
        len = ALIGN_UP(len, PAGE_SIZE);

        // Do mmap
        void *buf = mmap(NULL, len, prot, flags, -1, 0);
        if (buf == MAP_FAILED) {
            printf("mmap failed\n");
            printf("used_memory = %ld, mmap time = %ld\n", used_memory, num_bufs);
            exit(-1);
        }
        bufs[num_bufs] = buf;
        lens[num_bufs] = len;

        // Update memory usage
        used_memory += len;
        if (TOUCH_PAGES) {
            check_bytes_in_buf(buf, len, 0);
        }
    }
    gettimeofday(&time_end1, NULL);
    peak_mem_bufs = num_bufs;

    // Phrase 2: do munmap to free all memory mapped memory
    for (int bi = 0; bi < num_bufs; bi++) {
        void *buf = bufs[bi];
        size_t len = lens[bi];
        int ret = munmap(buf, len);
        if (ret < 0) {
            printf("munmap failed");
            exit(-1);
        }

        bufs[bi] = NULL;
        lens[bi] = 0;
    }
    gettimeofday(&time_end2, NULL);
    local_mmap_time = (time_end1.tv_sec - time_begin.tv_sec) * 1000000
                            + (time_end1.tv_usec - time_begin.tv_usec);
    local_munmap_time = (time_end2.tv_sec - time_end1.tv_sec) * 1000000
                            + (time_end2.tv_usec - time_end1.tv_usec);

    __atomic_fetch_add(&g_mmap_time, local_mmap_time, __ATOMIC_SEQ_CST);
    __atomic_fetch_add(&g_munmap_time, local_munmap_time, __ATOMIC_SEQ_CST);

    // printf("mmap time = %lu us, munmap time = %lu us\n", mmap_time, munmap_time);
    printf("buf number = %ld, memory used: %ld MB\n", peak_mem_bufs, used_memory / MB);
    return NULL;
}

int main(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "tp:s:")) != -1) {
        switch (opt) {
            case 't':{
                TOUCH_PAGES = true; break;
            }
            case 'p': {
                THREAD_NUM = atoi(optarg); break;
            }
            case 's': {
                MAX_MMAP_SIZE = atol(optarg); break;
            }
            default:
                print_usage();
                exit(-1);
        }
    }
    char *s = TOUCH_PAGES? "True":"False";

    printf("Options:\n"
            "touch pages:      %s\n"
            "thread_num:       %d\n"
            "max_mmap_size:    %ld\n\n", s, THREAD_NUM, MAX_MMAP_SIZE);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 2 * MB);

    pthread_t threads[THREAD_NUM];
    for (int ti = 0; ti < THREAD_NUM; ti++) {
        if (pthread_create(&threads[ti], &attr, run_main, NULL) < 0) {
            printf("ERROR: pthread_create failed (ti = %d)\n", ti);
            return -1;
        }
    }

    /*
     * Wait for the threads to finish
     */
    for (int ti = 0; ti < THREAD_NUM; ti++) {
        if (pthread_join(threads[ti], NULL) < 0) {
            printf("ERROR: pthread_join failed (ti = %d)\n", ti);
            return -1;
        }
    }

    printf("mmap time = %lu us, munmap time = %lu us\n", g_mmap_time / THREAD_NUM, g_munmap_time / THREAD_NUM);
    return 0;
}

