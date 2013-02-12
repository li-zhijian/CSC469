#define _POSIX_C_SOURCE 199309L

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#define SLEEP_NSEC      1E6L  /* 1 millisecond = 10^6 nanoseconds */
#define SLEEP_SAMPLES     5L
#define THRESHOLD_NSEC 1000L  /* (100 ns = Main Memory Reference) x 10  */

static uint64_t start = 0;

static inline void access_counter(unsigned *hi, unsigned *lo) {
    __asm__ volatile
        ( "rdtsc; movl %%edx, %0; movl %%eax, %1" /* format string */
        : "=r" (*hi), "=r" (*lo) /* output list */
        : /* no inputs */
        : "%edx", "%eax"); /* clobber list */
}

static inline void start_counter() {
    unsigned hi, lo;
    access_counter(&hi, &lo);
    start = ((uint64_t)hi << 32) | lo;
}

static inline uint64_t get_counter() {
    unsigned ncyc_hi, ncyc_lo;
    access_counter(&ncyc_hi, &ncyc_lo);
    return (((uint64_t)ncyc_hi << 32) | ncyc_lo) - start;
}

/*
 * Size of samples will be 2 * num
 */
uint64_t inactive_periods(int num, uint64_t threshold, uint64_t *samples) {
    uint64_t start, last, current;

    start_counter();
    last = current = start = get_counter();
    for(int i = 0; i < num;) {
        last = current;
        current = get_counter();
        if(current - last >= threshold) {
            samples[2*i] = last;
            samples[2*i + 1] = current;
            i++;
        }
    }

    return start;
}

int main(int argc, char const *argv[]) {
    if(argc != 2) {
        printf("Usage: trace num_inactive\n");
        return -1;
    }

    /* Parse arguments */
    uint64_t num_inactive = strtoull(argv[1], NULL, 10);

    /* Get clock resolution */
    struct timespec res;
    clock_getres(CLOCK_REALTIME, &res);

    /* Get CPU frequency by sleeping for SLEEP_SAMPLES times */
    struct timespec sleepTime = { 0, SLEEP_NSEC };
    uint64_t sleep_samples[SLEEP_SAMPLES];

    /**
     *  As long as we don't go over 10 seconds of combined sleep time
     *  this shouldn't overflow.
     */
    uint64_t cycles_per_msec = 0;

    for(uint64_t i = 0; i < SLEEP_SAMPLES;) {
        __asm__ volatile ( "cpuid;" );
        /* do some busy waiting */
        for(uint64_t j = 0; j < 1E7; j++);

        start_counter();
        if(nanosleep(&sleepTime, NULL) == 0) {
            sleep_samples[i] = get_counter();
            cycles_per_msec += sleep_samples[i++];
        }
    }
    cycles_per_msec = cycles_per_msec / SLEEP_SAMPLES;

    uint64_t threshold = cycles_per_msec / (1E6 /* 1 ms = 10^6 ns */ / THRESHOLD_NSEC);

    /* Collect samples of inactive periods */
    uint64_t *samples = malloc(sizeof(uint64_t) * num_inactive * 2);
    uint64_t active_start = inactive_periods(num_inactive, threshold, samples);

    printf("Clock Resolution: %lds %ldns\n", res.tv_sec, res.tv_nsec);
    printf("Clock Speed: %.2Lf GHz\n", cycles_per_msec / SLEEP_NSEC);
    printf("Threshold: %llu cycles\n", threshold);

    /* Print out inactive periods */
    for(int i = 0; i < num_inactive; i++) {
        printf("Active %d: start at %llu, duration %llu cycles (%.6Lf ms)\n",
                i, active_start, (samples[2*i] - active_start),
                (samples[2*i] - active_start)/(long double)cycles_per_msec);
        printf("Inactive %d: start at %llu, duration %llu cycles (%.6Lf ms)\n",
                i, samples[2*i], (samples[2*i+1] - samples[2*i]),
                (samples[2*i+1] - samples[2*i])/(long double)cycles_per_msec);
        active_start = samples[2*i+1];
    }

    /* Free up memory */
    free(samples);
}
