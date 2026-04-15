/*
 * IXLandSystem Time Subsystem - Darwin Bridge
 *
 * This file includes Darwin headers and provides the implementation
 * using Darwin's time functions. It is the ONLY file in the time
 * subsystem that includes Darwin headers like <sys/time.h> and <time.h>.
 * This is the private bridge - the public interface is in kernel/time.c
 *
 * NOTE: This file does NOT include the private time.h. It implements
 * the _impl() functions using Darwin's native types directly.
 */

#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

/* ============================================================================
 * TIME - Darwin implementation using host libc
 * ============================================================================ */

time_t time_impl(time_t *tloc) {
    time_t t = time(NULL);
    if (tloc) {
        *tloc = t;
    }
    return t;
}

/* ============================================================================
 * GETTIMEOFDAY - Darwin implementation
 * ============================================================================ */

int gettimeofday_impl(struct timeval *tv, struct timezone *tz) {
    return gettimeofday(tv, tz);
}

int settimeofday_impl(const struct timeval *tv, const struct timezone *tz) {
    /* iOS restriction: setting system time not allowed */
    (void)tv;
    (void)tz;
    errno = EPERM;
    return -1;
}

/* ============================================================================
 * CLOCK_GETTIME - Darwin implementation
 * Note: Darwin does not support all POSIX clocks; implement minimal set
 * ============================================================================ */

int clock_gettime_impl(clockid_t clk_id, struct timespec *tp) {
    (void)clk_id;
    (void)tp;
    /* iOS does not fully support POSIX clock_gettime */
    errno = ENOSYS;
    return -1;
}

int clock_getres_impl(clockid_t clk_id, struct timespec *res) {
    (void)clk_id;
    (void)res;
    errno = ENOSYS;
    return -1;
}

int clock_settime_impl(clockid_t clk_id, const struct timespec *tp) {
    /* iOS restriction: setting clocks not allowed */
    (void)clk_id;
    (void)tp;
    errno = EPERM;
    return -1;
}

/* ============================================================================
 * SLEEP FUNCTIONS - Darwin implementation
 * ============================================================================ */

unsigned int sleep_impl(unsigned int seconds) {
    return sleep(seconds);
}

int usleep_impl(useconds_t usec) {
    return usleep(usec);
}

int nanosleep_impl(const struct timespec *req, struct timespec *rem) {
    /* Use Darwin's nanosleep - same signature */
    return nanosleep(req, rem);
}

/* ============================================================================
 * ITIMER - Interval timers (iOS does not support - return ENOSYS)
 * ============================================================================ */

int setitimer_impl(int which, const struct itimerval *new_value, struct itimerval *old_value) {
    (void)which;
    (void)new_value;
    (void)old_value;
    errno = ENOSYS;
    return -1;
}

int getitimer_impl(int which, struct itimerval *curr_value) {
    (void)which;
    (void)curr_value;
    errno = ENOSYS;
    return -1;
}

unsigned int alarm_impl(unsigned int seconds) {
    (void)seconds;
    /* iOS does not support alarm() - return 0 */
    return 0;
}
