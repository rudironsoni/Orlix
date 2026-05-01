/* IXLandSystem/runtime/aarch64/exec_context.h
 * Private aarch64 execution handoff substrate for virtual Linux tasks.
 */

#ifndef RUNTIME_AARCH64_EXEC_CONTEXT_H
#define RUNTIME_AARCH64_EXEC_CONTEXT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct task_struct;

struct aarch64_exec_context {
    uint64_t pc;
    uint64_t sp;
    struct task_struct *task;
    long (*read_memory)(struct task_struct *task, uint64_t addr, void *buf, size_t count);
    long (*write_memory)(struct task_struct *task, uint64_t addr, const void *buf, size_t count);
};

int aarch64_exec_context_from_task(struct task_struct *task, struct aarch64_exec_context *context);

#ifdef __cplusplus
}
#endif

#endif /* RUNTIME_AARCH64_EXEC_CONTEXT_H */
