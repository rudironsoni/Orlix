/* iXland - Resource Limits and Usage
 *
 * Canonical owner for resource syscalls:
 * - getrlimit(), setrlimit(), getrlimit64(), setrlimit64()
 * - getrusage()
 * - prlimit(), prlimit64()
 *
 * Linux-shaped canonical owner - iOS mediation as implementation detail
 */

#include "resource.h"
#include "task.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>

/* ============================================================================
 * RLIMIT - Resource limits (private implementation)
 * ============================================================================ */

static int getrlimit_impl(int resource, struct rlimit *rlim) {
    struct task_struct *task = get_current();

    if (!rlim) {
        errno = EFAULT;
        return -1;
    }
    if (resource < 0 || resource >= 16) {
        errno = EINVAL;
        return -1;
    }
    if (!task) {
        errno = ESRCH;
        return -1;
    }
    rlim->rlim_cur = (__kernel_ulong_t)task->rlimits[resource].cur;
    rlim->rlim_max = (__kernel_ulong_t)task->rlimits[resource].max;
    return 0;
}

static int setrlimit_impl(int resource, const struct rlimit *rlim) {
    struct task_struct *task = get_current();

    if (!rlim) {
        errno = EFAULT;
        return -1;
    }
    if (resource < 0 || resource >= 16) {
        errno = EINVAL;
        return -1;
    }
    if (rlim->rlim_cur > rlim->rlim_max) {
        errno = EINVAL;
        return -1;
    }
    if (!task) {
        errno = ESRCH;
        return -1;
    }
    task->rlimits[resource].cur = (uint64_t)rlim->rlim_cur;
    task->rlimits[resource].max = (uint64_t)rlim->rlim_max;
    return 0;
}

/* ============================================================================
 * RUSAGE - Resource usage (private implementation)
 * ============================================================================ */

long times_impl(struct tms *buf) {
    if (buf) {
        memset(buf, 0, sizeof(*buf));
    }
    return 0;
}

int getrusage_impl(int who, struct rusage *usage) {
    if (who != 0 && who != -1 && who != 1) {
        errno = EINVAL;
        return -1;
    }
    if (!usage) {
        errno = EFAULT;
        return -1;
    }
    memset(usage, 0, sizeof(*usage));
    return 0;
}

/* ============================================================================
 * PRLIMIT - Process resource limits (private implementation)
 * ============================================================================ */

static int prlimit_impl(int32_t pid, int resource, const struct rlimit *new_limit,
                        struct rlimit *old_limit) {
    struct task_struct *task = get_current();

    if (pid != 0 && (!task || pid != task->pid)) {
        errno = ESRCH;
        return -1;
    }

    /* Get old values first */
    if (old_limit) {
        if (getrlimit_impl(resource, old_limit) < 0) {
            return -1;
        }
    }

    /* Set new values */
    if (new_limit) {
        return setrlimit_impl(resource, new_limit);
    }

    return 0;
}

/* ============================================================================
 * Public Canonical Syscalls
 * ============================================================================ */

__attribute__((visibility("default"))) int getrlimit(int resource, struct rlimit *rlim) {
    return getrlimit_impl(resource, rlim);
}

__attribute__((visibility("default"))) int setrlimit(int resource, const struct rlimit *rlim) {
    return setrlimit_impl(resource, rlim);
}

__attribute__((visibility("default"))) int getrlimit64(int resource, struct rlimit64 *rlim) {
    if (!rlim) {
        errno = EFAULT;
        return -1;
    }
    return getrlimit_impl(resource, (struct rlimit *)rlim);
}

__attribute__((visibility("default"))) int setrlimit64(int resource, const struct rlimit64 *rlim) {
    if (!rlim) {
        errno = EFAULT;
        return -1;
    }
    return setrlimit_impl(resource, (const struct rlimit *)rlim);
}

__attribute__((visibility("default"))) int getrusage(int who, struct rusage *usage) {
    return getrusage_impl(who, usage);
}

__attribute__((visibility("default"))) int prlimit(int32_t pid, int resource, const struct rlimit *new_limit,
                                                   struct rlimit *old_limit) {
    return prlimit_impl(pid, resource, new_limit, old_limit);
}
