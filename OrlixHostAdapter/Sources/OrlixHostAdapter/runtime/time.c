#include "OrlixHostAdapter/runtime/time.h"

#include <stdint.h>
#include <time.h>

__attribute__((visibility("hidden"))) unsigned long long orlix_host_time_monotonic_ns(void)
{
    struct timespec now = {
        .tv_sec = 0,
        .tv_nsec = 0,
    };

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0;
    }

    return ((uint64_t)now.tv_sec * 1000000000ULL) + (uint64_t)now.tv_nsec;
}
