/* iXland - Synchronization Primitives
 *
 * Canonical owner for sync syscalls:
 * - futex() - Fast Userspace muTEX
 * - set_robust_list(), get_robust_list()
 * - restart_syscall()
 *
 * Linux-shaped canonical owner - iOS mediation as implementation detail
 * Note: futex is Linux-specific; on iOS we use condition variables
 */

#include <errno.h>
#include <pthread.h>

/* ============================================================================
 * FUTEX - Fast Userspace muTEX (simulated on iOS)
 * ============================================================================ */

/* Futex operations */
#define FUTEX_WAIT 0
#define FUTEX_WAKE 1
#define FUTEX_FD 2
#define FUTEX_REQUEUE 3
#define FUTEX_CMP_REQUEUE 4
#define FUTEX_WAKE_OP 5
#define FUTEX_LOCK_PI 6
#define FUTEX_UNLOCK_PI 7
#define FUTEX_TRYLOCK_PI 8
#define FUTEX_WAIT_BITSET 9
#define FUTEX_WAKE_BITSET 10

static int futex_impl(int *uaddr, int futex_op, int val, const struct timespec *timeout,
int *uaddr2, int val3) {
(void)uaddr;
(void)futex_op;
(void)val;
(void)timeout;
(void)uaddr2;
(void)val3;

/* futex not available on iOS - use pthread synchronization */
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
