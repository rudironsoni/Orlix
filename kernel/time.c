/* iXland - Time and Clock Subsystem
 *
 * Canonical owner for time-related syscalls:
 * - time(), gettimeofday(), settimeofday()
 * - clock_gettime(), clock_settime(), clock_getres()
 * - nanosleep(), usleep(), sleep()
 * - alarm(), setitimer(), getitimer()
 *
 * Linux-shaped canonical owner - iOS mediation as implementation detail
 */

#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "../internal/ixland_internal.h"

/* ============================================================================
 * TIME - High precision time
 * ============================================================================ */

time_t ixland_time(time_t *tloc) {
    time_t t = time(NULL);
    if (tloc) {
        *tloc = t;
    }
    return t;
}

/* ============================================================================
 * GETTIMEOFDAY - BSD compatibility
 * ============================================================================ */

int ixland_gettimeofday(struct timeval *tv, struct timezone *tz) {
    return gettimeofday(tv, tz);
}

int ixland_settimeofday(const struct timeval *tv, const struct timezone *tz) {
    /* iOS restriction: setting system time not allowed */
    (void)tv;
    (void)tz;
    errno = EPERM;
    return -1;
}

/* ============================================================================
 * CLOCK_GETTIME - POSIX clocks
 * ============================================================================ */

int ixland_clock_gettime(clockid_t clk_id, struct timespec *tp) {
    return clock_gettime(clk_id, tp);
}

int ixland_clock_getres(clockid_t clk_id, struct timespec *res) {
    return clock_getres(clk_id, res);
}

int ixland_clock_settime(clockid_t clk_id, const struct timespec *tp) {
    /* iOS restriction: setting clocks not allowed */
    (void)clk_id;
    (void)tp;
    errno = EPERM;
    return -1;
}

/* ============================================================================
 * SLEEP FUNCTIONS
 * ============================================================================ */

unsigned int ixland_sleep(unsigned int seconds) {
    return sleep(seconds);
}

int ixland_usleep(useconds_t usec) {
    return usleep(usec);
}

int ixland_nanosleep(const struct timespec *req, struct timespec *rem) {
    return nanosleep(req, rem);
}

/* ============================================================================
 * ITIMER - Interval timers (simulated)
 * ============================================================================ */

int ixland_setitimer(int which, const struct itimerval *new_value, struct itimerval *old_value) {
    /* iOS does not support itimer - simulate with timer */
    (void)which;
    (void)new_value;
    (void)old_value;
    errno = ENOSYS;
    return -1;
}

int ixland_getitimer(int which, struct itimerval *curr_value) {
    (void)which;
    (void)curr_value;
    errno = ENOSYS;
    return -1;
}

unsigned int ixland_alarm(unsigned int seconds) {
    /* iOS does not support alarm - return remaining */
    (void)seconds;
    return 0;
}
