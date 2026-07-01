// SPDX-License-Identifier: GPL-2.0-only
#include <linux/mm_types.h>

#include "../hosted_exec/tcti/block_cache.h"

void tcti_invalidate_mm(struct mm_struct *mm)
{
	if (mm)
		tcti_block_cache_invalidate_mm(mm);
}
