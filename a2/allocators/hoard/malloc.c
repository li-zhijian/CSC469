/**
 * CSC 469 - Assignment 2
 * Zeeshan Qureshi, Abhinav Gupta
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>

#include "mm_thread.h"
#include "memlib.h"
#include "malloc.h"

name_t myname = {
    /* Allocator Name */
    "simplified hoard",

    /* First Team Member */
    "Zeeshan Qureshi",
    "g0zee@cdf.toronto.edu",

    /* Second Team Member */
    "Abhinav Gupta",
    "g2abg@cdf.toronto.edu"
};

/* Output debug traces */
#ifdef _HOARD_DEBUG
#define TRACE(...) do { \
        printf("%s(%d): ", __FILE__, __LINE__); \
        printf(__VA_ARGS__); \
        printf("\n"); \
    } while (0)
#else
#define TRACE(...)
#endif

/* Output error and exit */
#define FATAL(...) do { \
    printf("%s(%d): ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); \
    printf(" Exiting! \n"); \
    exit(1); \
} while(0)

/* Allocator constants */
#define PAGE_SIZE       4 * 1024        /* Currently deal with fixed 4K page size */
#define BLK_SIG         0xCAFED00D      /* To verify block integrity */
#define HEAP_SIG        0xD00DCAFE      /* To verify heap integrity */
#define SLOT_MIN_POW    3               /* Minimum slot power 2^3 */
#define SLOT_MIN_SIZE   8               /* Minimum slot size 8 B */
#define SLOT_MAX_POW    20              /* Maximum slot power 2^20 */
#define SLOT_MAX_SIZE   1024 * 1024     /* Maximum slot size 1 MB */

/* Superblock */
typedef struct block_t {
    uint32_t sig;
    uint32_t processor;
    uint32_t tid;
    uint32_t slotSize;
    uint32_t dataOffset;
    struct block_t *prev, *next;
} block_t;

/* Processor heap */
typedef struct {
    uint32_t sig;
    pthread_mutex_t lock;
    block_t *bins[SLOT_MAX_POW];
} heap_t;

/* To be initialized by mm_init() */
char *meta_low = NULL, *meta_high = NULL;
int numProcessors = 0, pageSize = 0;

/**
 * Processor heaps will be at locations heaps[0] to heaps[numProcessors - 1]
 * and the global heap will be at heaps[numProcessors]
 */
heap_t *heaps = NULL;

int mm_init() {
    if(dseg_hi > dseg_lo) {
        /* Already initialized, don't do anything */
        TRACE("mm_init() already called before.");
        return -1;
    }

    /* Initialize allocated memory via memlib */
    mem_init();
    meta_low = mem_sbrk(mem_pagesize());
    meta_high = dseg_hi + 1;

    /* Allocated pointer should not be null */
    assert(meta_low != NULL);
    /* Since this is initial allocation, should match dseg_lo */
    assert(meta_low == dseg_lo);

    /* dseg_hi points to last byte in allocated space */
    assert(meta_low + mem_pagesize() == meta_high);

    /**
     * TODO: Deal with fixed 4K page size for now; make more generic
     * when correctness checked.
     */
    assert(mem_pagesize() == PAGE_SIZE);

    /* Initialize globals */
    numProcessors = getNumProcessors();
    pageSize = mem_pagesize();
    heaps = (heap_t *) meta_low;
    TRACE("# of Processors: %d", numProcessors);
    TRACE("Page Size: %d", pageSize);
    TRACE("sizeof(heap_t) = %d", sizeof(heap_t));
    TRACE("sizeof(block_t) = %d", sizeof(block_t));

    /* Initialize processor heaps */
    int i;
    for(i = 0; i <= numProcessors; i++) {
        heap_t *heap = heaps + i;
        assert((char *)heap < meta_high);

        /* Set signature */
        heap->sig = HEAP_SIG;

        /* initialize mutex */
        pthread_mutexattr_t attrs;
        pthread_mutexattr_init(&attrs);
        pthread_mutex_init(&(heap->lock), &attrs);

        /* Initialize superblock bins */
        int j;
        for(j = 0; j < SLOT_MAX_POW; j++) {
            heap->bins[j] = NULL;
        }
    }

    TRACE("Allocator Initialization Complete");
    return 0;
}

void *mm_malloc(size_t size) {
    return mem_sbrk(size);
}

void mm_free(void *ptr) {
}
