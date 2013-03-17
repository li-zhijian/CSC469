#ifndef DEBUG_H_
#define DEBUG_H_

#ifdef DEBUG
#define TRACE(...) do { \
        printf(__VA_ARGS__); \
        printf("\n"); \
    } while (0)
#else
#define TRACE(...)
#endif

#endif /* DEBUG_H_ */
