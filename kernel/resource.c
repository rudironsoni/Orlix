/* iXland - Resource Limits and Usage
 *
 * Canonical owner for resource syscalls:
 * - getrlimit(), setrlimit(), getrlimit64(), setrlimit64()
 * - getrusage()
 * - prlimit(), prlimit64()
 *
 * Linux-shaped canonical owner - iOS mediation as implementation detail
 */

#include <errno.h>
#include <sys/resource.h>
#include <unistd.h>

#include "../src/ixland/internal/ixland_internal.h"

/* ============================================================================
 * RLIMIT - Resource limits
 * ============================================================================ */

int ixland_getrlimit(int resource, struct rlimit *rlim) {
    return getrlimit(resource, rlim);
}

int ixland_setrlimit(int resource, const struct rlimit *rlim) {
    /* iOS restriction: some limits cannot be changed */
    return setrlimit(resource, rlim);
}

#ifdef __LP64__
int ixland_getrlimit64(int resource, struct rlimit64 *rlim) {
    return ixland_getrlimit(resource, (struct rlimit *)rlim);
}

int ixland_setrlimit64(int resource, const struct rlimit64 *rlim) {
    return ixland_setrlimit(resource, (const struct rlimit *)rlim);
}
#endif

/* ============================================================================
 * RUSAGE - Resource usage
 * ============================================================================ */

int ixland_getrusage(int who, struct rusage *usage) {
    return getrusage(who, usage);
}

/* ============================================================================
 * PRlimit - Process resource limits
 * ============================================================================ */

int ixland_prlimit(pid_t pid, int resource, const struct rlimit *new_limit,
                   struct rlimit *old_limit) {
    (void)pid;

    /* Get old values first */
    if (old_limit) {
        if (ixland_getrlimit(resource, old_limit) < 0) {
            return -1;
        }
    }

    /* Set new values */
    if (new_limit) {
        return ixland_setrlimit(resource, new_limit);
    }

    return 0;
}
