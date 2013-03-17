/**
 * CSC 469 - Assignment 2
 * Zeeshan Qureshi, Abhinav Gupta
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>

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

int mm_init() {
    mem_init();
    return 0;
}

void *mm_malloc(size_t size) {
    return mem_sbrk(size);
}

void mm_free(void *ptr) {
}
