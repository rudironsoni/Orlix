#ifndef INTERNAL_TIMEKEEPING_H
#define INTERNAL_TIMEKEEPING_H

#include <linux/time_types.h>

#ifdef __cplusplus
extern "C" {
#endif

int kernel_sleep_ms(int timeout_ms);
int kernel_clock_gettime(int clock_id, struct __kernel_timespec *tp);

#ifdef __cplusplus
}
#endif

#endif
