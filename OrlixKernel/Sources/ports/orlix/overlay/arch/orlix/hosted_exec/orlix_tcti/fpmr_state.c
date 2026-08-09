// SPDX-License-Identifier: GPL-2.0-only
#include <linux/sched.h>

#include "fpmr_state.h"

u64 orlix_tcti_fpmr_current(void)
{
	return current->thread.user_fpmr;
}

void orlix_tcti_fpmr_write_current(u64 value)
{
	current->thread.user_fpmr = value;
}
