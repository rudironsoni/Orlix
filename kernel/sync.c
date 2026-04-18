/* iXland - Synchronization Primitives
 *
 * Canonical owner for sync syscalls:
 * - futex() - Fast Userspace muTEX
 * - set_robust_list(), get_robust_list()
 * - restart_syscall()
 *
 * Linux-shaped canonical owner - iOS mediation as implementation detail
 * Note: futex is Linux-specific; unsupported operations currently reject with ENOSYS
 */

#include <errno.h>
#include <time.h>
#include <linux/futex.h>

/* ============================================================================
 * FUTEX - Fast Userspace muTEX
 * ============================================================================ */

/* ABI truth comes from vendored Linux UAPI: <linux/futex.h> */

static int futex_impl(int *uaddr, int futex_op, int val, const struct timespec *timeout,
int *uaddr2, int val3) {
(void)uaddr;
(void)futex_op;
(void)val;
(void)timeout;
(void)uaddr2;
(void)val3;

/* Unsupported on current iOS substrate: Linux-facing rejection stays here */
errno = ENOSYS;
return -1;
}

__attribute__((visibility("default"))) int futex(int *uaddr, int futex_op, int val,
const struct timespec *timeout, int *uaddr2, int val3) {
return futex_impl(uaddr, futex_op, val, timeout, uaddr2, val3);
}

/*
* NOTE: syscall() was removed because it conflicts with system unistd.h headers.
* The original ixland_syscall() has been dropped.
* Use specific syscall wrappers (futex, set_robust_list, etc.) instead.
*/
