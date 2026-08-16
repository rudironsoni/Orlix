/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_ORLIX_MMU_H
#define _ASM_ORLIX_MMU_H

#ifndef __ASSEMBLY__
#include <linux/atomic.h>
#include <linux/rwlock.h>
#include <linux/spinlock.h>

typedef struct {
	unsigned long end_brk;
#ifdef CONFIG_BINFMT_ELF_FDPIC
	unsigned long exec_fdpic_loadmap;
	unsigned long interp_fdpic_loadmap;
#endif
	unsigned long orlix_tcti_static_pie_base;
	atomic64_t orlix_tcti_mapping_sequence;
	rwlock_t orlix_tcti_mapping_lock;
} mm_context_t;
#endif

#endif /* _ASM_ORLIX_MMU_H */
