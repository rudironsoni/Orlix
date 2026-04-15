/* Time and Clock Subsystem
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

/* ============================================================================
 * TIME - High precision time
 * ============================================================================ */

static time_t time_impl(time_t *tloc) {
    time_t t = time(NULL);
    if (tloc) {
        *tloc = t;
    }
    return t;
}

/* ============================================================================
 * GETTIMEOFDAY - BSD compatibility
 * ============================================================================ */

static int gettimeofday_impl(struct timeval *tv, struct timezone *tz) {
    return gettimeofday(tv, tz);
}

static int settimeofday_impl(const struct timeval *tv, const struct timezone *tz) {
    /* iOS restriction: setting system time not allowed */
    (void)tv;
    (void)tz;
    errno = EPERM;
    return -1;
}

/* ============================================================================
 * CLOCK_GETTIME - POSIX clocks
 * ============================================================================ */

static int clock_gettime_impl(clockid_t clk_id, struct timespec *tp) {
    (void)clk_id;
    (void)tp;
    errno = ENOSYS;
    return -1;
}

static int clock_getres_impl(clockid_t clk_id, struct timespec *res) {
    (void)clk_id;
    (void)res;
    errno = ENOSYS;
    return -1;
}

static int clock_settime_impl(clockid_t clk_id, const struct timespec *tp) {
    /* iOS restriction: setting clocks not allowed */
    (void)clk_id;
    (void)tp;
    errno = EPERM;
    return -1;
}

/* ============================================================================
 * SLEEP FUNCTIONS
 * ============================================================================ */

static unsigned int sleep_impl(unsigned int seconds) {
    return sleep(seconds);
}

static int usleep_impl(useconds_t usec) {
    return usleep(usec);
}

static int nanosleep_impl(const struct timespec *req, struct timespec *rem) {
    return nanosleep(req, rem);
}

/* ============================================================================
 * ITIMER - Interval timers (simulated)
 * ============================================================================ */

static int setitimer_impl(int which, const struct itimerval *new_value, struct itimerval *old_value) {
    /* iOS does not support itimer - simulate with timer */
    (void)which;
    (void)new_value;
    (void)old_value;
    errno = ENOSYS;
    return -1;
}

static int getitimer_impl(int which, struct itimerval *curr_value) {
    (void)which;
    (void)curr_value;
    errno = ENOSYS;
    return -1;
}

static unsigned int alarm_impl(unsigned int seconds) {
    /* iOS does not support alarm - return remaining */
    (void)seconds;
    return 0;
}

/* ============================================================================
 * PUBLIC SYSCALL WRAPPERS
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
