#ifndef PRIVATE_KERNEL_PTRACE_STATE_H
#define PRIVATE_KERNEL_PTRACE_STATE_H

#include "kernel/ptrace.h"

#ifdef __cplusplus
extern "C" {
#endif

int ptrace_may_access_task_impl(const struct task *tracer, const struct task *target);

#ifdef __cplusplus
}
#endif

#endif /* PRIVATE_KERNEL_PTRACE_STATE_H */
