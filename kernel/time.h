/*
 * IXLandSystem Time Subsystem - Private Internal API
 *
 * Linux-shaped internal interface. NO Darwin headers.
 */

#ifndef TIME_H
#define TIME_H

#include <stddef.h>

/* Forward declare types - actual definition comes from Linux-shaped owner */
typedef long time_t;
typedef long clockid_t;
typedef unsigned int useconds_t;

/* Linux-shaped timeval - NOT Darwin timeval */
struct timeval {
    time_t tv_sec;
    long tv_usec;
};

/* Linux-shaped timespec */
struct timespec {
    time_t tv_sec;
    long tv_nsec;
};

/* Linux-shaped timezone */
struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};

/* Linux-shaped itimerval */
struct itimerval {
    struct timeval it_interval;
    struct timeval it_value;
};

/* ============================================================================
 * INTERNAL TIME IMPLEMENTATION
 * Called by public wrappers, separated from Darwin mediation
 * ============================================================================ */

time_t time_impl(time_t *tloc);
int gettimeofday_impl(struct timeval *tv, struct timezone *tz);
int settimeofday_impl(const struct timeval *tv, const struct timezone *tz);
int clock_gettime_impl(clockid_t clk_id, struct timespec *tp);
int clock_getres_impl(clockid_t clk_id, struct timespec *res);
int clock_settime_impl(clockid_t clk_id, const struct timespec *tp);
unsigned int sleep_impl(unsigned int seconds);
int usleep_impl(useconds_t usec);
int nanosleep_impl(const struct timespec *req, struct timespec *rem);
int setitimer_impl(int which, const struct itimerval *new_value, struct itimerval *old_value);
int getitimer_impl(int which, struct itimerval *curr_value);
unsigned int alarm_impl(unsigned int seconds);

#endif /* TIME_H */
