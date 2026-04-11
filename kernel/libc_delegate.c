/* iOS Subsystem for Linux - libc Delegate Layer
 *
 * Provides safe access to original libc functions when libixland interposes on them.
 * Uses dlsym(RTLD_NEXT, ...) to bypass the interposition layer and call real
 * system functions without triggering infinite recursion.
 *
 * Purpose: When we override open(), close(), etc., we still need to call the
 * real underlying libc functions. This file provides orig_* function pointers
 * that delegate directly to libc.
 */

#include <dlfcn.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "../include/ixland/ixland_signal.h"
#include "../internal/ixland_internal.h"

/* Define sighandler_t if needed */
#ifndef __sighandler_t
#define __sighandler_t sighandler_t
typedef void (*sighandler_t)(int);
#endif

/* Original libc function pointers for interposition safety */
static int (*libc_chdir)(const char *path) = NULL;
static int (*libc_fchdir)(int fd) = NULL;
static char *(*libc_getcwd)(char *buf, size_t size) = NULL;

/* Environment */
static char *(*libc_getenv)(const char *) = NULL;
static int (*libc_setenv)(const char *, const char *, int) = NULL;
static int (*libc_unsetenv)(const char *) = NULL;

/* Filesystem */
static int (*libc_stat)(const char *, struct stat *) = NULL;
static int (*libc_fstat)(int, struct stat *) = NULL;
static int (*libc_lstat)(const char *, struct stat *) = NULL;
static int (*libc_mkdir)(const char *, mode_t) = NULL;
static int (*libc_rmdir)(const char *) = NULL;
static int (*libc_unlink)(const char *) = NULL;
static int (*libc_chmod)(const char *, mode_t) = NULL;
static int (*libc_fchmod)(int, mode_t) = NULL;
static int (*libc_chown)(const char *, uid_t, gid_t) = NULL;
static int (*libc_fchown)(int, uid_t, gid_t) = NULL;
static int (*libc_lchown)(const char *, uid_t, gid_t) = NULL;

/* File operations - exported for use in other modules */
int (*libc_open)(const char *, int, mode_t) = NULL;
int (*libc_close)(int) = NULL;
ssize_t (*libc_read)(int, void *, size_t) = NULL;
ssize_t (*libc_write)(int, const void *, size_t) = NULL;
off_t (*libc_lseek)(int, off_t, int) = NULL;

/* Time */
static unsigned int (*libc_alarm)(unsigned int) = NULL;
static unsigned int (*libc_sleep)(unsigned int) = NULL;
static int (*libc_usleep)(useconds_t) = NULL;
static int (*libc_nanosleep)(const struct timespec *, struct timespec *) = NULL;

/* Memory */
static void *(*libc_mmap)(void *, size_t, int, int, int, off_t) = NULL;
static int (*libc_munmap)(void *, size_t) = NULL;
static int (*libc_mprotect)(void *, size_t, int) = NULL;

void init_orig_funcs(void) {
    static int initialized = 0;
    if (initialized)
        return;
    initialized = 1;

    /* Directory */
    libc_chdir = (int (*)(const char *))dlsym(RTLD_NEXT, "chdir");
    libc_fchdir = (int (*)(int))dlsym(RTLD_NEXT, "fchdir");
    libc_getcwd = (char *(*)(char *, size_t))dlsym(RTLD_NEXT, "getcwd");

    /* Filesystem */
    libc_stat = (int (*)(const char *, struct stat *))dlsym(RTLD_NEXT, "stat");
    libc_fstat = (int (*)(int, struct stat *))dlsym(RTLD_NEXT, "fstat");
    libc_lstat = (int (*)(const char *, struct stat *))dlsym(RTLD_NEXT, "lstat");
    libc_mkdir = (int (*)(const char *, mode_t))dlsym(RTLD_NEXT, "mkdir");
    libc_rmdir = (int (*)(const char *))dlsym(RTLD_NEXT, "rmdir");
    libc_unlink = (int (*)(const char *))dlsym(RTLD_NEXT, "unlink");
    libc_chmod = (int (*)(const char *, mode_t))dlsym(RTLD_NEXT, "chmod");
    libc_fchmod = (int (*)(int, mode_t))dlsym(RTLD_NEXT, "fchmod");
    libc_chown = (int (*)(const char *, uid_t, gid_t))dlsym(RTLD_NEXT, "chown");
    libc_fchown = (int (*)(int, uid_t, gid_t))dlsym(RTLD_NEXT, "fchown");
    libc_lchown = (int (*)(const char *, uid_t, gid_t))dlsym(RTLD_NEXT, "lchown");

    /* Time */
    libc_alarm = (unsigned int (*)(unsigned int))dlsym(RTLD_NEXT, "alarm");
    libc_sleep = (unsigned int (*)(unsigned int))dlsym(RTLD_NEXT, "sleep");
    libc_usleep = (int (*)(useconds_t))dlsym(RTLD_NEXT, "usleep");
    libc_nanosleep =
        (int (*)(const struct timespec *, struct timespec *))dlsym(RTLD_NEXT, "nanosleep");

    /* Memory */
    libc_mmap = (void *(*)(void *, size_t, int, int, int, off_t))dlsym(RTLD_NEXT, "mmap");
    libc_munmap = (int (*)(void *, size_t))dlsym(RTLD_NEXT, "munmap");
    libc_mprotect = (int (*)(void *, size_t, int))dlsym(RTLD_NEXT, "mprotect");

    /* File operations */
    libc_open = (int (*)(const char *, int, mode_t))dlsym(RTLD_NEXT, "open");
    libc_close = (int (*)(int))dlsym(RTLD_NEXT, "close");
    libc_read = (ssize_t (*)(int, void *, size_t))dlsym(RTLD_NEXT, "read");
    libc_write = (ssize_t (*)(int, const void *, size_t))dlsym(RTLD_NEXT, "write");
    libc_lseek = (off_t (*)(int, off_t, int))dlsym(RTLD_NEXT, "lseek");
}

/* ============================================================================
 * FILESYSTEM STUBS
 * ============================================================================ */

int __ixland_chmod_impl(const char *pathname, mode_t mode) {
    init_orig_funcs();
    return libc_chmod ? libc_chmod(pathname, mode) : -1;
}

int __ixland_fchmod_impl(int fd, mode_t mode) {
    init_orig_funcs();
    return libc_fchmod ? libc_fchmod(fd, mode) : -1;
}

int __ixland_chown_impl(const char *pathname, uid_t owner, gid_t group) {
    init_orig_funcs();
    return libc_chown ? libc_chown(pathname, owner, group) : -1;
}

int __ixland_fchown_impl(int fd, uid_t owner, gid_t group) {
    init_orig_funcs();
    return libc_fchown ? libc_fchown(fd, owner, group) : -1;
}

int __ixland_lchown_impl(const char *pathname, uid_t owner, gid_t group) {
    init_orig_funcs();
    return libc_lchown ? libc_lchown(pathname, owner, group) : -1;
}

/* ============================================================================
 * SIGNAL STUBS
 *
 * Note: __ixland_kill_impl, __ixland_signal_impl, __ixland_sigaction_impl
 * are implemented in ixland_process.c with full functionality
 * ============================================================================ */

int __ixland_sigprocmask_impl(int how, const sigset_t *set, sigset_t *oldset) {
    ixland_sigset_t *thread_mask = ixland_thread_sigmask();
    ixland_sigset_t ixland_set;

    if (set) {
        ixland_sigset_from_host(set, &ixland_set);
    }

    if (oldset) {
        ixland_sigset_to_host(thread_mask, oldset);
    }

    if (!set) {
        return 0;
    }

    switch (how) {
    case SIG_BLOCK:
        ixland_sigorset(thread_mask, thread_mask, &ixland_set);
        return 0;
    case SIG_UNBLOCK:
        ixland_signotset(&ixland_set, &ixland_set);
        ixland_sigandset(thread_mask, thread_mask, &ixland_set);
        return 0;
    case SIG_SETMASK:
        *thread_mask = ixland_set;
        return 0;
    default:
        errno = EINVAL;
        return -1;
    }
}

int __ixland_sigpending_impl(sigset_t *set) {
    if (!set) {
        errno = EINVAL;
        return -1;
    }
    memset(set, 0, sizeof(*set));
    return 0;
}

int __ixland_sigsuspend_impl(const sigset_t *mask) {
    if (mask) {
        ixland_sigset_t ixland_mask;
        ixland_sigset_from_host(mask, &ixland_mask);
        ixland_sigset_to_host(&ixland_mask, (sigset_t *)ixland_thread_sigmask());
    }
    errno = EINTR;
    return -1;
}

int __ixland_sigemptyset_impl(sigset_t *set) {
    if (!set) {
        errno = EINVAL;
        return -1;
    }
    memset(set, 0, sizeof(*set));
    return 0;
}

int __ixland_sigfillset_impl(sigset_t *set) {
    if (!set) {
        errno = EINVAL;
        return -1;
    }
    memset(set, 0xff, sizeof(*set));
    return 0;
}

int __ixland_sigaddset_impl(sigset_t *set, int signum) {
    if (!set) {
        errno = EINVAL;
        return -1;
    }
    sigaddset(set, signum);
    return 0;
}

int __ixland_sigdelset_impl(sigset_t *set, int signum) {
    if (!set) {
        errno = EINVAL;
        return -1;
    }
    sigdelset(set, signum);
    return 0;
}

int __ixland_sigismember_impl(const sigset_t *set, int signum) {
    if (!set) {
        errno = EINVAL;
        return -1;
    }
    return sigismember(set, signum);
}

unsigned int __ixland_alarm_impl(unsigned int seconds) {
    init_orig_funcs();
    return libc_alarm ? libc_alarm(seconds) : 0;
}

int __ixland_setitimer_impl(int which, const struct itimerval *new_value,
                            struct itimerval *old_value) {
    return setitimer(which, new_value, old_value);
}

int __ixland_getitimer_impl(int which, struct itimerval *curr_value) {
    return getitimer(which, curr_value);
}

int __ixland_pause_impl(void) {
    return pause();
}

/* ============================================================================
 * TIME STUBS
 * ============================================================================ */

unsigned int __ixland_sleep_impl(unsigned int seconds) {
    init_orig_funcs();
    return libc_sleep ? libc_sleep(seconds) : 0;
}

int __ixland_usleep_impl(useconds_t usec) {
    init_orig_funcs();
    return libc_usleep ? libc_usleep(usec) : -1;
}

int __ixland_nanosleep_impl(const struct timespec *req, struct timespec *rem) {
    init_orig_funcs();
    return libc_nanosleep ? libc_nanosleep(req, rem) : -1;
}

int __ixland_gettimeofday_impl(struct timeval *tv, struct timezone *tz) {
    return gettimeofday(tv, tz);
}

int __ixland_settimeofday_impl(const struct timeval *tv, const struct timezone *tz) {
    return settimeofday(tv, tz);
}

int __ixland_clock_gettime_impl(clockid_t clk_id, struct timespec *tp) {
    return clock_gettime(clk_id, tp);
}

time_t __ixland_time_impl(time_t *tloc) {
    return time(tloc);
}

/* ============================================================================
 * ENVIRONMENT STUBS
 * ============================================================================ */

/* Environment function pointers declared with others at top of file */

char *__ixland_getenv_impl(const char *name) {
    init_orig_funcs();
    return libc_getenv ? libc_getenv(name) : NULL;
}

int __ixland_setenv_impl(const char *name, const char *value, int overwrite) {
    init_orig_funcs();
    return libc_setenv ? libc_setenv(name, value, overwrite) : -1;
}

int __ixland_unsetenv_impl(const char *name) {
    init_orig_funcs();
    return libc_unsetenv ? libc_unsetenv(name) : -1;
}

int __ixland_clearenv_impl(void) {
    /* Not standard, just clear environ */
    extern char **environ;
    environ = NULL;
    return 0;
}

/* ============================================================================
 * MEMORY STUBS
 * ============================================================================ */

void *__ixland_mmap_impl(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    init_orig_funcs();
    return libc_mmap ? libc_mmap(addr, length, prot, flags, fd, offset) : MAP_FAILED;
}

int __ixland_munmap_impl(void *addr, size_t length) {
    init_orig_funcs();
    return libc_munmap ? libc_munmap(addr, length) : -1;
}

int __ixland_mprotect_impl(void *addr, size_t len, int prot) {
    init_orig_funcs();
    return libc_mprotect ? libc_mprotect(addr, len, prot) : -1;
}

/* ============================================================================
 * PIPE STUBS
 * ============================================================================ */

int __ixland_pipe_impl(int pipefd[2]) {
    return pipe(pipefd);
}

/* ============================================================================
 * TTY STUBS
 * ============================================================================ */

int __ixland_isatty_impl(int fd) {
    return isatty(fd);
}

int __ixland_tcgetattr_impl(int fd, struct termios *termios_p) {
    return tcgetattr(fd, termios_p);
}

int __ixland_tcsetattr_impl(int fd, int optional_actions, const struct termios *termios_p) {
    return tcsetattr(fd, optional_actions, termios_p);
}

/* ============================================================================
 * PUBLIC API - STUBS
 * ============================================================================ */

/* Filesystem */
int ixland_chmod(const char *pathname, mode_t mode) {
    return __ixland_chmod_impl(pathname, mode);
}
int ixland_fchmod(int fd, mode_t mode) {
    return __ixland_fchmod_impl(fd, mode);
}
int ixland_chown(const char *pathname, uid_t owner, gid_t group) {
    return __ixland_chown_impl(pathname, owner, group);
}
int ixland_fchown(int fd, uid_t owner, gid_t group) {
    return __ixland_fchown_impl(fd, owner, group);
}
int ixland_lchown(const char *pathname, uid_t owner, gid_t group) {
    return __ixland_lchown_impl(pathname, owner, group);
}

/* Signal wrappers - main implementations in ixland_process.c and signal_mask.c */
/* __sighandler_t ixland_signal(int signum, __sighandler_t handler) - in ixland_process.c */
/* int ixland_kill(pid_t pid, int sig) - in ixland_process.c */
/* int ixland_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) - in
 * ixland_process.c */
/* ixland_sigprocmask, sigpending, sigsuspend, sigemptyset, sigfillset, sigaddset, sigdelset,
 * sigismember - implemented in signal_mask.c for ixland_sigset_t type */
unsigned int ixland_alarm(unsigned int seconds) {
    return __ixland_alarm_impl(seconds);
}
int ixland_setitimer(int which, const struct itimerval *new_value, struct itimerval *old_value) {
    return __ixland_setitimer_impl(which, new_value, old_value);
}
int ixland_getitimer(int which, struct itimerval *curr_value) {
    return __ixland_getitimer_impl(which, curr_value);
}
int ixland_pause(void) {
    return __ixland_pause_impl();
}

/* Time */
unsigned int ixland_sleep(unsigned int seconds) {
    return __ixland_sleep_impl(seconds);
}
int ixland_usleep(useconds_t usec) {
    return __ixland_usleep_impl(usec);
}
int ixland_nanosleep(const struct timespec *req, struct timespec *rem) {
    return __ixland_nanosleep_impl(req, rem);
}
int ixland_gettimeofday(struct timeval *tv, struct timezone *tz) {
    return __ixland_gettimeofday_impl(tv, tz);
}
int ixland_settimeofday(const struct timeval *tv, const struct timezone *tz) {
    return __ixland_settimeofday_impl(tv, tz);
}
int ixland_clock_gettime(clockid_t clk_id, struct timespec *tp) {
    return __ixland_clock_gettime_impl(clk_id, tp);
}
time_t ixland_time(time_t *tloc) {
    return __ixland_time_impl(tloc);
}

/* Environment */
char *ixland_getenv(const char *name) {
    return __ixland_getenv_impl(name);
}
int ixland_setenv(const char *name, const char *value, int overwrite) {
    return __ixland_setenv_impl(name, value, overwrite);
}
int ixland_unsetenv(const char *name) {
    return __ixland_unsetenv_impl(name);
}
int ixland_clearenv(void) {
    return __ixland_clearenv_impl();
}

/* Memory */
void *ixland_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    return __ixland_mmap_impl(addr, length, prot, flags, fd, offset);
}
int ixland_munmap(void *addr, size_t length) {
    return __ixland_munmap_impl(addr, length);
}
int ixland_mprotect(void *addr, size_t len, int prot) {
    return __ixland_mprotect_impl(addr, len, prot);
}

/* Pipe */
int ixland_pipe(int pipefd[2]) {
    return __ixland_pipe_impl(pipefd);
}

/* TTY */
int ixland_isatty(int fd) {
    return __ixland_isatty_impl(fd);
}
int ixland_tcgetattr(int fd, struct termios *termios_p) {
    return __ixland_tcgetattr_impl(fd, termios_p);
}
int ixland_tcsetattr(int fd, int optional_actions, const struct termios *termios_p) {
    return __ixland_tcsetattr_impl(fd, optional_actions, termios_p);
}

/* ============================================================================
 * DIRECTORY STUBS
 * ============================================================================ */

/* ============================================================================
 * IDENTITY SYSCALLS
 *
 * Note: ixland_getuid(), ixland_geteuid(), ixland_getgid(), ixland_getegid(),
 * ixland_setuid(), ixland_setgid() are implemented in ixland_identity.c
 * ============================================================================ */
