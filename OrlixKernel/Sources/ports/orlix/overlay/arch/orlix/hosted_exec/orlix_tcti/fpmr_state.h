/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_FPMR_STATE_H
#define ORLIX_TCTI_FPMR_STATE_H

#include <linux/types.h>

/*
 * The FPMR is per-task architectural state.  FP8 consumers use this narrow
 * interface rather than reaching into thread_struct or SystemAccessor.
 */
u64 orlix_tcti_fpmr_current(void);
void orlix_tcti_fpmr_write_current(u64 value);

#endif /* ORLIX_TCTI_FPMR_STATE_H */
