#include <sys/types.h>

/* Target function */
int __ixland_open_impl(const char *pathname, int flags, int mode) {
    (void)pathname;
    (void)flags;
    (void)mode;
    return 42;
}

/* Test 1: weak alias */
pid_t fork(void) __attribute__((weak, alias("__ixland_open_impl")));

/* Test 2: visibility default with alias */
int open(const char *pathname, int flags, int mode)
    __attribute__((visibility("default"), alias("__ixland_open_impl")));

/* Test 3: Simple visible function */
int close(int fd) __attribute__((visibility("default")));

int close(int fd) {
    return -fd;
}
