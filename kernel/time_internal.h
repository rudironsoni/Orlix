/* IXLandSystem/kernel/time_internal.h
 * Private internal header for time subsystem struct definitions
 * 
 * This is PRIVATE internal state - NOT Linux UAPI.
 * Shared between time.c (public wrappers) and time_darwin.c (implementations).
 */

#ifndef IXLAND_KERNEL_TIME_INTERNAL_H
#define IXLAND_KERNEL_TIME_INTERNAL_H

#include <sys/types.h>
#include <time.h>

#ifndef _IXLAND_TIMEVAL_DEFINED
#define _IXLAND_TIMEVAL_DEFINED
struct timeval {
    time_t tv_sec;
    suseconds_t tv_usec;
};
#endif

#ifndef _IXLAND_TIMEZONE_DEFINED
#define _IXLAND_TIMEZONE_DEFINED
struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};
#endif

#ifndef _IXLAND_ITIMERVAL_DEFINED
#define _IXLAND_ITIMERVAL_DEFINED
struct itimerval {
    struct timeval it_interval;
    struct timeval it_value;
};
#endif

#endif /* IXLAND_KERNEL_TIME_INTERNAL_H */
