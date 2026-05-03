/* IXLandSystem/kernel/ptrace.h
 * Private owner header for virtual ptrace supervision state.
 *
 * This is runtime behavior over IXLand tasks, not host process debugging.
 */

#ifndef KERNEL_PTRACE_H
#define KERNEL_PTRACE_H

#include <asm/posix_types.h>

#ifdef __cplusplus
extern "C" {
#endif

long ptrace_impl(long request, __kernel_pid_t pid, void *addr, void *data);
void ptrace_note_syscall_entry(long number, long arg0, long arg1, long arg2,
                               long arg3, long arg4, long arg5);
void ptrace_note_syscall_exit(long retval);

#ifdef __cplusplus
}
#endif

#endif /* KERNEL_PTRACE_H */
