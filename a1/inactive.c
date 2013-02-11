#include <stdio.h>
#include <stdint.h>

// http://www.cdf.toronto.edu/~csc469h/winter/assignments/a1/a1.shtml

static uint64_t start = 0;

void access_counter(unsigned *hi, unsigned *lo) {
    __asm__ volatile
        ( "rdtsc; movl %%edx, %0; movl %%eax, %1" /* format string */
        : "=r" (*hi), "=r" (*lo) /* output list */
        : /* no inputs */
        : "%edx", "%eax"); /* clobber list */
}

void start_counter() {
    unsigned hi, lo;
    access_counter(&hi, &lo);
    start = ((uint64_t)hi << 32) | lo;
}

uint64_t get_counter() {
    unsigned ncyc_hi, ncyc_lo;
    access_counter(&ncyc_hi, &ncyc_lo);
    return (((uint64_t)ncyc_hi << 32) | ncyc_lo) - start;
}

uint64_t inactive_periods(int num, uint64_t theshold, uint64_t *samples) {
    // TODO
}

int main(int argc, char const *argv[]) {
    start_counter();
    printf("%llu\n", get_counter());
}
