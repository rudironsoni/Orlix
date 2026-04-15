/* Time and Clock Subsystem
 *
 * Canonical owner for time-related syscalls:
 * - time(), gettimeofday(), settimeofday()
 * - clock_gettime(), clock_settime(), clock_getres()
 * - nanosleep(), usleep(), sleep()
 * - alarm(), setitimer(), getitimer()
 *
 * Linux-shaped canonical owner - iOS mediation via time_darwin.c
 *
 * This file does NOT include Darwin headers.
 * It includes only the private time.h which uses Linux-shaped types.
 */

#include "time.h"

#include <errno.h>

/* ============================================================================
 * PUBLIC SYSCALL WRAPPERS
 * These export the canonical Linux/POSIX interface
 * ============================================================================ */

__attribute__((visibility("default"))) time_t time(time_t *tloc) {
    return time_impl(tloc);
}

__attribute__((visibility("default"))) int gettimeofday(struct timeval *tv, struct timezone *tz) {
    return gettimeofday_impl(tv, tz);
}

__attribute__((visibility("default"))) int settimeofday(const struct timeval *tv, const struct timezone *tz) {
    return settimeofday_impl(tv, tz);
}

__attribute__((visibility("default"))) int clock_gettime(clockid_t clk_id, struct timespec *tp) {
    return clock_gettime_impl(clk_id, tp);
}

__attribute__((visibility("default"))) int clock_getres(clockid_t clk_id, struct timespec *res) {
    return clock_getres_impl(clk_id, res);
}

__attribute__((visibility("default"))) int clock_settime(clockid_t clk_id, const struct timespec *tp) {
    return clock_settime_impl(clk_id, tp);
}

__attribute__((visibility("default"))) unsigned int sleep(unsigned int seconds) {
    return sleep_impl(seconds);
}

__attribute__((visibility("default"))) int usleep(useconds_t usec) {
    return usleep_impl(usec);
}

__attribute__((visibility("default"))) int nanosleep(const struct timespec *req, struct timespec *rem) {
    return nanosleep_impl(req, rem);
}

__attribute__((visibility("default"))) int setitimer(int which, const struct itimerval *new_value, struct itimerval *old_value) {
    return setitimer_impl(which, new_value, old_value);
}

__attribute__((visibility("default"))) int getitimer(int which, struct itimerval *curr_value) {
    return getitimer_impl(which, curr_value);
}

__attribute__((visibility("default"))) unsigned int alarm(unsigned int seconds) {
    return alarm_impl(seconds);
}
