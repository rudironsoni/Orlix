// SPDX-License-Identifier: GPL-2.0-only

#include <linux/sched.h>
#include <linux/time_namespace.h>
#include <vdso/datapage.h>

struct vdso_data *arch_get_vdso_data(void *vvar_page)
{
	return vvar_page;
}

#ifdef CONFIG_TIME_NS
int vdso_join_timens(struct task_struct *task, struct time_namespace *ns)
{
	return 0;
}
#endif
