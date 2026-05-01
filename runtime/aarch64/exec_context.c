/* IXLandSystem/runtime/aarch64/exec_context.c
 * Virtual aarch64 execution context setup from task exec handoff state.
 */

#include "exec_context.h"

#include <errno.h>

#include "../../kernel/task.h"

int aarch64_exec_context_from_task(struct task_struct *task, struct aarch64_exec_context *context) {
    const struct task_exec_handoff *handoff;

    if (!task || !context) {
        errno = EFAULT;
        return -1;
    }

    handoff = task_get_exec_handoff_impl(task);
    if (!handoff ||
        handoff->aarch64_pc == 0 ||
        handoff->aarch64_sp == 0 ||
        !handoff->read_memory ||
        !handoff->write_memory) {
        errno = ENOEXEC;
        return -1;
    }

    context->pc = handoff->aarch64_pc;
    context->sp = handoff->aarch64_sp;
    context->task = task;
    context->read_memory = handoff->read_memory;
    context->write_memory = handoff->write_memory;
    return 0;
}
