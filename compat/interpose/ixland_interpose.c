/* iOS Subsystem for Linux - Symbol Interposition Layer
 *
 * Provides standard Linux syscall names as strong symbols
 * These wrap the ixland_* public API
 */

#include <ixland/ixland_syscalls.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

/* ============================================================================
 * PROCESS MANAGEMENT (16 syscalls)
 * ============================================================================ */

__attribute__((visibility("default"))) pid_t fork(void) {
    return ixland_fork();
}

__attribute__((visibility("default"))) int vfork(void) {
    return ixland_vfork();
}

__attribute__((visibility("default"))) int execve(const char *p, char *const a[], char *const e[]) {
    return ixland_execve(p, a, e);
}

__attribute__((visibility("default"))) int execv(const char *p, char *const a[]) {
    return ixland_execv(p, a);
}

__attribute__((visibility("default"))) void exit(int s) {
    ixland_exit(s);
    __builtin_unreachable();
}

__attribute__((visibility("default"))) void _exit(int s) {
    ixland__exit(s);
    __builtin_unreachable();
}

__attribute__((visibility("default"))) pid_t getpid(void) {
    return ixland_getpid();
}

__attribute__((visibility("default"))) pid_t getppid(void) {
    return ixland_getppid();
}

__attribute__((visibility("default"))) pid_t getpgrp(void) {
    return ixland_getpgrp();
}

__attribute__((visibility("default"))) int setpgrp(void) {
    return ixland_setpgrp();
}

__attribute__((visibility("default"))) pid_t getpgid(pid_t p) {
    return ixland_getpgid(p);
}

__attribute__((visibility("default"))) int setpgid(pid_t p, pid_t g) {
    return ixland_setpgid(p, g);
}

__attribute__((visibility("default"))) pid_t wait(int *s) {
    return ixland_wait(s);
}

__attribute__((visibility("default"))) pid_t waitpid(pid_t p, int *s, int o) {
    return ixland_waitpid(p, s, o);
}

__attribute__((visibility("default"))) pid_t wait3(int *s, int o, struct rusage *r) {
    return ixland_wait3(s, o, r);
}

__attribute__((visibility("default"))) pid_t wait4(pid_t p, int *s, int o, struct rusage *r) {
    return ixland_wait4(p, s, o, r);
}

__attribute__((visibility("default"))) int system(const char *c) {
    return ixland_system(c);
}

/* ============================================================================
 * FILE OPERATIONS (20 syscalls)
 * ============================================================================ */

__attribute__((visibility("default"))) int open(const char *p, int f, ...) {
    mode_t m = 0;
    if (f & O_CREAT) {
        va_list a;
        va_start(a, f);
        m = va_arg(a, int); /* mode_t promoted to int */
        va_end(a);
    }
    return ixland_open(p, f, m);
}

__attribute__((visibility("default"))) int openat(int d, const char *p, int f, ...) {
    mode_t m = 0;
    if (f & O_CREAT) {
        va_list a;
        va_start(a, f);
        m = va_arg(a, int); /* mode_t promoted to int */
        va_end(a);
    }
    return ixland_openat(d, p, f, m);
}

__attribute__((visibility("default"))) int creat(const char *p, mode_t m) {
    return ixland_creat(p, m);
}

__attribute__((visibility("default"))) ssize_t read(int f, void *b, size_t c) {
    return ixland_read(f, b, c);
}

__attribute__((visibility("default"))) ssize_t write(int f, const void *b, size_t c) {
    return ixland_write(f, b, c);
}

__attribute__((visibility("default"))) int close(int f) {
    return ixland_close(f);
}

__attribute__((visibility("default"))) off_t lseek(int f, off_t o, int w) {
    return ixland_lseek(f, o, w);
}

__attribute__((visibility("default"))) ssize_t pread(int f, void *b, size_t c, off_t o) {
    return ixland_pread(f, b, c, o);
}

__attribute__((visibility("default"))) ssize_t pwrite(int f, const void *b, size_t c, off_t o) {
    return ixland_pwrite(f, b, c, o);
}

__attribute__((visibility("default"))) int dup(int o) {
    return ixland_dup(o);
}

__attribute__((visibility("default"))) int dup2(int o, int n) {
    return ixland_dup2(o, n);
}

__attribute__((visibility("default"))) int dup3(int o, int n, int f) {
    return ixland_dup3(o, n, f);
}

__attribute__((visibility("default"))) int fcntl(int f, int c, ...) {
    va_list a;
    va_start(a, c);
    int arg = va_arg(a, int);
    va_end(a);
    return ixland_fcntl(f, c, arg);
}

__attribute__((visibility("default"))) int ioctl(int f, unsigned long r, ...) {
    va_list a;
    va_start(a, r);
    void *arg = va_arg(a, void *);
    va_end(a);
    return ixland_ioctl(f, r, arg);
}

__attribute__((visibility("default"))) int access(const char *p, int m) {
    return ixland_access(p, m);
}

__attribute__((visibility("default"))) int faccessat(int d, const char *p, int m, int f) {
    return ixland_faccessat(d, p, m, f);
}

__attribute__((visibility("default"))) int chdir(const char *p) {
    return ixland_chdir(p);
}

__attribute__((visibility("default"))) int fchdir(int f) {
    return ixland_fchdir(f);
}

__attribute__((visibility("default"))) char *getcwd(char *b, size_t s) {
    return ixland_getcwd(b, s);
}

/* ============================================================================
 * ENVIRONMENT (5 syscalls)
 * ============================================================================ */

__attribute__((visibility("default"))) char *getenv(const char *n) {
    return ixland_getenv(n);
}

__attribute__((visibility("default"))) int setenv(const char *n, const char *v, int o) {
    return ixland_setenv(n, v, o);
}

__attribute__((visibility("default"))) int unsetenv(const char *n) {
    return ixland_unsetenv(n);
}

__attribute__((visibility("default"))) int clearenv(void) {
    return ixland_clearenv();
}

/* ============================================================================
 * MISC SYSCALLS - Stubs for unimplemented
 * ============================================================================ */

/* Filesystem stubs */
__attribute__((visibility("default"))) int stat(const char *p, struct stat *s) {
    return ixland_stat(p, s);
}

__attribute__((visibility("default"))) int fstat(int f, struct stat *s) {
    return ixland_fstat(f, s);
}

__attribute__((visibility("default"))) int lstat(const char *p, struct stat *s) {
    return ixland_lstat(p, s);
}

__attribute__((visibility("default"))) int mkdir(const char *p, mode_t m) {
    return ixland_mkdir(p, m);
}

__attribute__((visibility("default"))) int rmdir(const char *p) {
    return ixland_rmdir(p);
}

__attribute__((visibility("default"))) int unlink(const char *p) {
    return ixland_unlink(p);
}

__attribute__((visibility("default"))) int link(const char *o, const char *n) {
    return ixland_link(o, n);
}

__attribute__((visibility("default"))) int symlink(const char *t, const char *l) {
    return ixland_symlink(t, l);
}

__attribute__((visibility("default"))) ssize_t readlink(const char *p, char *b, size_t s) {
    return ixland_readlink(p, b, s);
}

__attribute__((visibility("default"))) int chmod(const char *p, mode_t m) {
    return ixland_chmod(p, m);
}

__attribute__((visibility("default"))) int fchmod(int f, mode_t m) {
    return ixland_fchmod(f, m);
}

__attribute__((visibility("default"))) int chown(const char *p, uid_t o, gid_t g) {
    return ixland_chown(p, o, g);
}

__attribute__((visibility("default"))) int fchown(int f, uid_t o, gid_t g) {
    return ixland_fchown(f, o, g);
}

__attribute__((visibility("default"))) int lchown(const char *p, uid_t o, gid_t g) {
    return ixland_lchown(p, o, g);
}

__attribute__((visibility("default"))) int chroot(const char *p) {
    return ixland_chroot(p);
}

/* Signal stubs */
__attribute__((visibility("default"))) sighandler_t signal(int s, sighandler_t h) {
    return ixland_signal(s, h);
}

__attribute__((visibility("default"))) int kill(pid_t p, int s) {
    return ixland_kill(p, s);
}

__attribute__((visibility("default"))) int sigaction(int s, const struct sigaction *a,
                                                     struct sigaction *o) {
    return ixland_sigaction(s, a, o);
}

unsigned __attribute__((visibility("default"))) int alarm(unsigned int s) {
    return ixland_alarm(s);
}

/* Time stubs */
unsigned __attribute__((visibility("default"))) int sleep(unsigned int s) {
    return ixland_sleep(s);
}

__attribute__((visibility("default"))) int usleep(useconds_t u) {
    return ixland_usleep(u);
}

__attribute__((visibility("default"))) int nanosleep(const struct timespec *r, struct timespec *o) {
    return ixland_nanosleep(r, o);
}

/* Memory stubs */
__attribute__((visibility("default"))) void *mmap(void *a, size_t l, int p, int f, int fd,
                                                  off_t o) {
    return ixland_mmap(a, l, p, f, fd, o);
}

__attribute__((visibility("default"))) int munmap(void *a, size_t l) {
    return ixland_munmap(a, l);
}

__attribute__((visibility("default"))) int mprotect(void *a, size_t l, int p) {
    return ixland_mprotect(a, l, p);
}

/* Pipe stubs */
__attribute__((visibility("default"))) int pipe(int p[2]) {
    return ixland_pipe(p);
}

/* TTY stubs */
__attribute__((visibility("default"))) int isatty(int f) {
    return ixland_isatty(f);
}

__attribute__((visibility("default"))) int tcgetattr(int f, struct termios *t) {
    return ixland_tcgetattr(f, t);
}

__attribute__((visibility("default"))) int tcsetattr(int f, int o, const struct termios *t) {
    return ixland_tcsetattr(f, o, t);
}
