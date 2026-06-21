/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __ASM_ORLIX_VDSO_GETTIMEOFDAY_H
#define __ASM_ORLIX_VDSO_GETTIMEOFDAY_H

#ifndef __ASSEMBLY__

#include <linux/types.h>

struct vdso_data;

static __always_inline u64 __arch_get_hw_counter(s32 clock_mode,
						 const struct vdso_data *vd)
{
	return 0;
}

static __always_inline const struct vdso_data *__arch_get_vdso_data(void)
{
	return NULL;
}

#endif /* __ASSEMBLY__ */

#endif /* __ASM_ORLIX_VDSO_GETTIMEOFDAY_H */
