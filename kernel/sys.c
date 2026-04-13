/* IXLandSystem/kernel/sys.c
 * Internal kernel misc syscall owner
 *
 * Canonical internal implementations for process-group and session
 * primitives that have no dedicated owner file yet.
 * The exported Linux-facing syscall surface lives in IXLandLibC.
 */

#include <errno.h>

#include "task.h"

int do_setpgid(pid_t pid, pid_t pgid) {
    struct task_struct *task = get_current();

    if (!task) {
        errno = ESRCH;
        return -1;
    }

    if (pgid < 0) {
        errno = EINVAL;
        return -1;
    }

    if (pid == 0) {
        pid = task->pid;
    }

    if (pgid == 0) {
        pgid = task->pid;
    }

    task->pgid = pgid;
    return 0;
}