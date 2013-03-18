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
    uint32_t freeSlots;
    uint32_t slots;
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

/* Bit manipulation functions */
inline int bits_set(uint8_t num);
inline int first_unset(uint8_t num);

/* Superblock macros */
#define BLOCK_DIR(b) (((uint8_t *)(b)) + sizeof(block_t))
#define BLOCK_DATA(b, slot) (((char *)(b) + (b)->dataOffset) + ((b)->slotSize * slot))
#define BIN_ADD_HEAD(bin, block) do { \
    (block)->next = (bin); \
    (bin)->prev = (block); \
    (bin) = (block); \
} while(0)

/* Align pointer to closest page boundary downwards */
#define PAGE_ALIGN(p)    ((void *)(((unsigned long)(p) / pageSize) * pageSize))

/**
 *  Math Macros
 *  Source: http://en.wikipedia.org/wiki/Floor_and_ceiling_functions#Quotients
 */
#define CEIL(x, y) (((x) + (y) - 1) / (y))

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

/**
 * Hash thread id to a processor
 */
inline int hash_tid(int tid) {
    return tid % numProcessors;
}

inline uint32_t num_slots(uint32_t space, uint32_t slotSize) {
    /**
     * Calculate number of slots assuming they will be a multiple of 8
     * and integer division makes sure we never exceed allocated space.
     */
    int slots = (8 * space) / (8 * slotSize + 1);

    return slots;
}

void init_superblock(block_t *block, uint32_t tid, uint32_t processor, uint32_t slotSize) {
    /* Clear whole block */
    memset((char *)block, 0, pageSize);

    /* Set signature and other identification fields */
    block->sig = BLK_SIG;
    block->tid = tid;
    block->processor = processor;
    block->slotSize = slotSize;
    block->prev = NULL;
    block->next = NULL;

    /* Calculate number of slots and set up directory */
    block->slots = num_slots(pageSize - sizeof(block_t), slotSize);
    /**
     * TODO: Add data offset calculation to align on 8-byte boundary.
     */
    block->freeSlots = block->slots;
    block->dataOffset = sizeof(block_t) + CEIL(block->slots, 8);
}

/**
 * NOTE: To be only called while having lock for processor heap.
 */
block_t *alloc_superblock(uint32_t tid, uint32_t processor, uint32_t slotSize) {
    block_t *block = (block_t *) mem_sbrk(pageSize);
    assert(block != NULL);

    init_superblock(block, tid, processor, slotSize);

    return block;
}

void *mm_malloc(size_t size) {
    if(size > pageSize/2) {
        FATAL("Objects greater than PageSize/2 not supported yet.");
    }

    /* Fetch Thread ID and hash to processor */
    uint32_t tid = getTID();
    uint32_t processor = hash_tid(tid);
    heap_t *heap = heaps + processor;

    /* Calculate appropriate bin */
    uint32_t bin = SLOT_MIN_POW, slotSize = SLOT_MIN_SIZE;
    while(size > slotSize) {
        bin++;
        slotSize *= 2;
    }

    /* Acquire lock for heap */
    //TRACE("ACQ: TID: %d, Processor: %d, Size: %d, Slot Size: %d, Bin: %d", tid, processor, size, slotSize, bin);
    pthread_mutex_lock(&(heap->lock));

    block_t *block = heap->bins[bin];

    if(block == NULL) {
        /**
         * TODO: Make this into a separate function since this exact code
         * is copied below in the loop as well.
         *
         * Check for blocks in global heap
         */
        uint32_t globalUsed = 0;
        heap_t *global = heaps + numProcessors;

        pthread_mutex_lock(&(global->lock));
        if(global->bins[bin] != NULL) {
            TRACE("TID: %d, Slot Size: %d Fetching from global heap", tid, slotSize);
            globalUsed = 1;
            block = global->bins[bin];
            if(block->next != NULL) {
                block->next->prev = NULL;
            }
            global->bins[bin] = block->next;
            init_superblock(block, tid, processor, slotSize);
        }
        pthread_mutex_unlock(&(global->lock));

        if(globalUsed == 0) {
            /* Create first superblock in bin */
            block = alloc_superblock(tid, processor, slotSize);
        }

        heap->bins[bin] = block;
    } else {
        /* Traverse superblock list to find right block or allocate new */
        assert(block->sig == BLK_SIG);
        while(block->tid != tid || block->freeSlots == 0) {
            assert(block->sig == BLK_SIG);
            if(block->next == NULL) {
                /* Check for blocks in global heap */
                uint32_t globalUsed = 0;
                heap_t *global = heaps + numProcessors;

                pthread_mutex_lock(&(global->lock));
                if(global->bins[bin] != NULL) {
                    TRACE("TID: %d, Slot Size: %d Fetching from global heap", tid, slotSize);
                    globalUsed = 1;
                    block = global->bins[bin];
                    if(block->next != NULL) {
                        block->next->prev = NULL;
                    }
                    global->bins[bin] = block->next;
                    init_superblock(block, tid, processor, slotSize);
                }
                pthread_mutex_unlock(&(global->lock));

                if(globalUsed == 0) {
                    TRACE("TID: %d, Slot Size: %d Allocating new superblock", tid, slotSize);
                    block = alloc_superblock(tid, processor, slotSize);
                }

                /* Add block to front of bin */
                BIN_ADD_HEAD(heap->bins[bin], block);

                break;
            }

            block = block->next;
        }
    }

    /* We now have a valid block with free slots, choose one of the slots and return to user */
    uint8_t *directory = BLOCK_DIR(block), i;
    int32_t slot = -1;
    for(i = 0; 8*i < block->slots; i += 1) {
        if((slot = first_unset(directory[i])) != -1) {
            assert(8*i + slot < block->slots);

            /* Mark slot as used */
            directory[i] = directory[i] | 1 << (7 - slot);
            slot += 8*i;
            block->freeSlots -= 1;
            //TRACE("ALLOC: TID: %d, Slot: %d:%d, Free Slots: %d", tid, slotSize, slot, block->freeSlots);
            break;
        }
    }

    /* Release lock for heap */
    //TRACE("REL: TID: %d, Processor: %d, Size: %d, Slot Size: %d, Bin: %d", tid, processor, size, slotSize, bin);
    pthread_mutex_unlock(&(heap->lock));

    return slot != -1 ? BLOCK_DATA(block, slot) : NULL;
}

void mm_free(void *ptr) {
    /* Make sure pointer is within our boundaries */
    assert((char *)ptr >= dseg_lo);
    assert((char *)ptr <= dseg_hi);

    block_t *block = PAGE_ALIGN(ptr);
    assert(block->sig == BLK_SIG);

    uint32_t processor = block->processor;
    heap_t *heap = heaps + processor;

    /* Acquire lock for heap */
    pthread_mutex_lock(&(heap->lock));

    /* Make sure block hasn't been moved to different heap */
    assert(block->processor == processor);

    /* Calculate slot number */
    int slot = (char *)ptr - BLOCK_DATA(block, 0);
    assert(slot % block->slotSize == 0);
    slot = slot / block->slotSize;
    assert(slot <= block->slots);

    /* Mark slot as unused */
    uint8_t *directory = BLOCK_DIR(block);
    directory[slot/8] = directory[slot/8] & ~(1 << (7 - (slot % 8)));
    block->freeSlots += 1;

    /*TRACE("FREE: TID: %d, Processor: %d, Slot Size: %d, Slot: %d, Free Slots %d",
            block->tid, block->processor, block->slotSize, slot, block->freeSlots);*/

    if(block->freeSlots == block->slots) {
        TRACE("FREE: Block empty, move to global heap.");

        /* Acquire lock for global heap */
        heap_t *global = heaps + numProcessors;
        pthread_mutex_lock(&(global->lock));

        /* Calculate bin from slotSize */
        uint32_t bin = SLOT_MIN_POW, size = SLOT_MIN_SIZE;
        while(size != block->slotSize) {
            bin++;
            size *= 2;
        }

        /* Remove block from processor heap */
        if(heap->bins[bin] == block) {
            heap->bins[bin] = block->next;
        }

        if(block->prev != NULL) {
            block->prev->next = block->next;
        }

        if(block->next != NULL) {
            block->next->prev = block->prev;
        }

        /* Add block to global heap */
        if(global->bins[bin] == NULL) {
            global->bins[bin] = block;
        } else {
            BIN_ADD_HEAD(global->bins[bin], block);
        }

        /* Release lock for global heap */
        pthread_mutex_unlock(&(global->lock));
    }

    /* Release lock for heap */
    pthread_mutex_unlock(&(heap->lock));
}

/**
 * Count the number of bits set in an 8-bit number
 */
inline int bits_set(uint8_t num) {
    char lookup_bits_set[256] = {
        0, /* 0 */
        1, /* 1 */
        1, /* 2 */
        2, /* 3 */
        1, /* 4 */
        2, /* 5 */
        2, /* 6 */
        3, /* 7 */
        1, /* 8 */
        2, /* 9 */
        2, /* 10 */
        3, /* 11 */
        2, /* 12 */
        3, /* 13 */
        3, /* 14 */
        4, /* 15 */
        1, /* 16 */
        2, /* 17 */
        2, /* 18 */
        3, /* 19 */
        2, /* 20 */
        3, /* 21 */
        3, /* 22 */
        4, /* 23 */
        2, /* 24 */
        3, /* 25 */
        3, /* 26 */
        4, /* 27 */
        3, /* 28 */
        4, /* 29 */
        4, /* 30 */
        5, /* 31 */
        1, /* 32 */
        2, /* 33 */
        2, /* 34 */
        3, /* 35 */
        2, /* 36 */
        3, /* 37 */
        3, /* 38 */
        4, /* 39 */
        2, /* 40 */
        3, /* 41 */
        3, /* 42 */
        4, /* 43 */
        3, /* 44 */
        4, /* 45 */
        4, /* 46 */
        5, /* 47 */
        2, /* 48 */
        3, /* 49 */
        3, /* 50 */
        4, /* 51 */
        3, /* 52 */
        4, /* 53 */
        4, /* 54 */
        5, /* 55 */
        3, /* 56 */
        4, /* 57 */
        4, /* 58 */
        5, /* 59 */
        4, /* 60 */
        5, /* 61 */
        5, /* 62 */
        6, /* 63 */
        1, /* 64 */
        2, /* 65 */
        2, /* 66 */
        3, /* 67 */
        2, /* 68 */
        3, /* 69 */
        3, /* 70 */
        4, /* 71 */
        2, /* 72 */
        3, /* 73 */
        3, /* 74 */
        4, /* 75 */
        3, /* 76 */
        4, /* 77 */
        4, /* 78 */
        5, /* 79 */
        2, /* 80 */
        3, /* 81 */
        3, /* 82 */
        4, /* 83 */
        3, /* 84 */
        4, /* 85 */
        4, /* 86 */
        5, /* 87 */
        3, /* 88 */
        4, /* 89 */
        4, /* 90 */
        5, /* 91 */
        4, /* 92 */
        5, /* 93 */
        5, /* 94 */
        6, /* 95 */
        2, /* 96 */
        3, /* 97 */
        3, /* 98 */
        4, /* 99 */
        3, /* 100 */
        4, /* 101 */
        4, /* 102 */
        5, /* 103 */
        3, /* 104 */
        4, /* 105 */
        4, /* 106 */
        5, /* 107 */
        4, /* 108 */
        5, /* 109 */
        5, /* 110 */
        6, /* 111 */
        3, /* 112 */
        4, /* 113 */
        4, /* 114 */
        5, /* 115 */
        4, /* 116 */
        5, /* 117 */
        5, /* 118 */
        6, /* 119 */
        4, /* 120 */
        5, /* 121 */
        5, /* 122 */
        6, /* 123 */
        5, /* 124 */
        6, /* 125 */
        6, /* 126 */
        7, /* 127 */
        1, /* 128 */
        2, /* 129 */
        2, /* 130 */
        3, /* 131 */
        2, /* 132 */
        3, /* 133 */
        3, /* 134 */
        4, /* 135 */
        2, /* 136 */
        3, /* 137 */
        3, /* 138 */
        4, /* 139 */
        3, /* 140 */
        4, /* 141 */
        4, /* 142 */
        5, /* 143 */
        2, /* 144 */
        3, /* 145 */
        3, /* 146 */
        4, /* 147 */
        3, /* 148 */
        4, /* 149 */
        4, /* 150 */
        5, /* 151 */
        3, /* 152 */
        4, /* 153 */
        4, /* 154 */
        5, /* 155 */
        4, /* 156 */
        5, /* 157 */
        5, /* 158 */
        6, /* 159 */
        2, /* 160 */
        3, /* 161 */
        3, /* 162 */
        4, /* 163 */
        3, /* 164 */
        4, /* 165 */
        4, /* 166 */
        5, /* 167 */
        3, /* 168 */
        4, /* 169 */
        4, /* 170 */
        5, /* 171 */
        4, /* 172 */
        5, /* 173 */
        5, /* 174 */
        6, /* 175 */
        3, /* 176 */
        4, /* 177 */
        4, /* 178 */
        5, /* 179 */
        4, /* 180 */
        5, /* 181 */
        5, /* 182 */
        6, /* 183 */
        4, /* 184 */
        5, /* 185 */
        5, /* 186 */
        6, /* 187 */
        5, /* 188 */
        6, /* 189 */
        6, /* 190 */
        7, /* 191 */
        2, /* 192 */
        3, /* 193 */
        3, /* 194 */
        4, /* 195 */
        3, /* 196 */
        4, /* 197 */
        4, /* 198 */
        5, /* 199 */
        3, /* 200 */
        4, /* 201 */
        4, /* 202 */
        5, /* 203 */
        4, /* 204 */
        5, /* 205 */
        5, /* 206 */
        6, /* 207 */
        3, /* 208 */
        4, /* 209 */
        4, /* 210 */
        5, /* 211 */
        4, /* 212 */
        5, /* 213 */
        5, /* 214 */
        6, /* 215 */
        4, /* 216 */
        5, /* 217 */
        5, /* 218 */
        6, /* 219 */
        5, /* 220 */
        6, /* 221 */
        6, /* 222 */
        7, /* 223 */
        3, /* 224 */
        4, /* 225 */
        4, /* 226 */
        5, /* 227 */
        4, /* 228 */
        5, /* 229 */
        5, /* 230 */
        6, /* 231 */
        4, /* 232 */
        5, /* 233 */
        5, /* 234 */
        6, /* 235 */
        5, /* 236 */
        6, /* 237 */
        6, /* 238 */
        7, /* 239 */
        4, /* 240 */
        5, /* 241 */
        5, /* 242 */
        6, /* 243 */
        5, /* 244 */
        6, /* 245 */
        6, /* 246 */
        7, /* 247 */
        5, /* 248 */
        6, /* 249 */
        6, /* 250 */
        7, /* 251 */
        6, /* 252 */
        7, /* 253 */
        7, /* 254 */
        8, /* 255 */
    };
    return lookup_bits_set[num];
}

/**
 * Return first unset bit in an 8-bit number
 */
inline int first_unset(uint8_t num) {
    char lookup_first_unset[256] = {
        0, /* 0 */
        0, /* 1 */
        0, /* 2 */
        0, /* 3 */
        0, /* 4 */
        0, /* 5 */
        0, /* 6 */
        0, /* 7 */
        0, /* 8 */
        0, /* 9 */
        0, /* 10 */
        0, /* 11 */
        0, /* 12 */
        0, /* 13 */
        0, /* 14 */
        0, /* 15 */
        0, /* 16 */
        0, /* 17 */
        0, /* 18 */
        0, /* 19 */
        0, /* 20 */
        0, /* 21 */
        0, /* 22 */
        0, /* 23 */
        0, /* 24 */
        0, /* 25 */
        0, /* 26 */
        0, /* 27 */
        0, /* 28 */
        0, /* 29 */
        0, /* 30 */
        0, /* 31 */
        0, /* 32 */
        0, /* 33 */
        0, /* 34 */
        0, /* 35 */
        0, /* 36 */
        0, /* 37 */
        0, /* 38 */
        0, /* 39 */
        0, /* 40 */
        0, /* 41 */
        0, /* 42 */
        0, /* 43 */
        0, /* 44 */
        0, /* 45 */
        0, /* 46 */
        0, /* 47 */
        0, /* 48 */
        0, /* 49 */
        0, /* 50 */
        0, /* 51 */
        0, /* 52 */
        0, /* 53 */
        0, /* 54 */
        0, /* 55 */
        0, /* 56 */
        0, /* 57 */
        0, /* 58 */
        0, /* 59 */
        0, /* 60 */
        0, /* 61 */
        0, /* 62 */
        0, /* 63 */
        0, /* 64 */
        0, /* 65 */
        0, /* 66 */
        0, /* 67 */
        0, /* 68 */
        0, /* 69 */
        0, /* 70 */
        0, /* 71 */
        0, /* 72 */
        0, /* 73 */
        0, /* 74 */
        0, /* 75 */
        0, /* 76 */
        0, /* 77 */
        0, /* 78 */
        0, /* 79 */
        0, /* 80 */
        0, /* 81 */
        0, /* 82 */
        0, /* 83 */
        0, /* 84 */
        0, /* 85 */
        0, /* 86 */
        0, /* 87 */
        0, /* 88 */
        0, /* 89 */
        0, /* 90 */
        0, /* 91 */
        0, /* 92 */
        0, /* 93 */
        0, /* 94 */
        0, /* 95 */
        0, /* 96 */
        0, /* 97 */
        0, /* 98 */
        0, /* 99 */
        0, /* 100 */
        0, /* 101 */
        0, /* 102 */
        0, /* 103 */
        0, /* 104 */
        0, /* 105 */
        0, /* 106 */
        0, /* 107 */
        0, /* 108 */
        0, /* 109 */
        0, /* 110 */
        0, /* 111 */
        0, /* 112 */
        0, /* 113 */
        0, /* 114 */
        0, /* 115 */
        0, /* 116 */
        0, /* 117 */
        0, /* 118 */
        0, /* 119 */
        0, /* 120 */
        0, /* 121 */
        0, /* 122 */
        0, /* 123 */
        0, /* 124 */
        0, /* 125 */
        0, /* 126 */
        0, /* 127 */
        1, /* 128 */
        1, /* 129 */
        1, /* 130 */
        1, /* 131 */
        1, /* 132 */
        1, /* 133 */
        1, /* 134 */
        1, /* 135 */
        1, /* 136 */
        1, /* 137 */
        1, /* 138 */
        1, /* 139 */
        1, /* 140 */
        1, /* 141 */
        1, /* 142 */
        1, /* 143 */
        1, /* 144 */
        1, /* 145 */
        1, /* 146 */
        1, /* 147 */
        1, /* 148 */
        1, /* 149 */
        1, /* 150 */
        1, /* 151 */
        1, /* 152 */
        1, /* 153 */
        1, /* 154 */
        1, /* 155 */
        1, /* 156 */
        1, /* 157 */
        1, /* 158 */
        1, /* 159 */
        1, /* 160 */
        1, /* 161 */
        1, /* 162 */
        1, /* 163 */
        1, /* 164 */
        1, /* 165 */
        1, /* 166 */
        1, /* 167 */
        1, /* 168 */
        1, /* 169 */
        1, /* 170 */
        1, /* 171 */
        1, /* 172 */
        1, /* 173 */
        1, /* 174 */
        1, /* 175 */
        1, /* 176 */
        1, /* 177 */
        1, /* 178 */
        1, /* 179 */
        1, /* 180 */
        1, /* 181 */
        1, /* 182 */
        1, /* 183 */
        1, /* 184 */
        1, /* 185 */
        1, /* 186 */
        1, /* 187 */
        1, /* 188 */
        1, /* 189 */
        1, /* 190 */
        1, /* 191 */
        2, /* 192 */
        2, /* 193 */
        2, /* 194 */
        2, /* 195 */
        2, /* 196 */
        2, /* 197 */
        2, /* 198 */
        2, /* 199 */
        2, /* 200 */
        2, /* 201 */
        2, /* 202 */
        2, /* 203 */
        2, /* 204 */
        2, /* 205 */
        2, /* 206 */
        2, /* 207 */
        2, /* 208 */
        2, /* 209 */
        2, /* 210 */
        2, /* 211 */
        2, /* 212 */
        2, /* 213 */
        2, /* 214 */
        2, /* 215 */
        2, /* 216 */
        2, /* 217 */
        2, /* 218 */
        2, /* 219 */
        2, /* 220 */
        2, /* 221 */
        2, /* 222 */
        2, /* 223 */
        3, /* 224 */
        3, /* 225 */
        3, /* 226 */
        3, /* 227 */
        3, /* 228 */
        3, /* 229 */
        3, /* 230 */
        3, /* 231 */
        3, /* 232 */
        3, /* 233 */
        3, /* 234 */
        3, /* 235 */
        3, /* 236 */
        3, /* 237 */
        3, /* 238 */
        3, /* 239 */
        4, /* 240 */
        4, /* 241 */
        4, /* 242 */
        4, /* 243 */
        4, /* 244 */
        4, /* 245 */
        4, /* 246 */
        4, /* 247 */
        5, /* 248 */
        5, /* 249 */
        5, /* 250 */
        5, /* 251 */
        6, /* 252 */
        6, /* 253 */
        7, /* 254 */
        -1, /* 255 */
    };
    return lookup_first_unset[num];
}
