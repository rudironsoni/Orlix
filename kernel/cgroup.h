/* IXLandSystem/kernel/cgroup.h
 * Private owner header for IXLand virtual cgroup state.
 *
 * This is runtime state, not Linux UAPI.
 */

#ifndef KERNEL_CGROUP_H
#define KERNEL_CGROUP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cgroup;
struct task_struct;

int cgroup_init(void);
void cgroup_deinit(void);
struct cgroup *cgroup_get(struct cgroup *cgrp);
void cgroup_put(struct cgroup *cgrp);
struct cgroup *cgroup_root(void);
int task_attach_cgroup(struct task_struct *task, struct cgroup *cgrp);
void task_detach_cgroup(struct task_struct *task);
const char *task_cgroup_path(const struct task_struct *task);
unsigned int task_cgroup_member_count(const struct task_struct *task);
int task_cgroup_proc_content(const struct task_struct *task, char *buf, size_t buflen);
int cgroup_proc_task_content(int32_t pid, char *buf, size_t buflen);

#ifdef __cplusplus
}
#endif

#endif /* KERNEL_CGROUP_H */
