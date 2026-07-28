// SPDX-License-Identifier: GPL-2.0-only
#include <linux/errno.h>
#include <linux/kconfig.h>
#include <asm/orlix_tcti.h>

#ifndef CONFIG_ORLIX_TCTI_RCWMASK_EL1_LO
#define CONFIG_ORLIX_TCTI_RCWMASK_EL1_LO 0
#endif
#ifndef CONFIG_ORLIX_TCTI_RCWMASK_EL1_HI
#define CONFIG_ORLIX_TCTI_RCWMASK_EL1_HI 0
#endif
#ifndef CONFIG_ORLIX_TCTI_RCWSMASK_EL1_LO
#define CONFIG_ORLIX_TCTI_RCWSMASK_EL1_LO 0
#endif
#ifndef CONFIG_ORLIX_TCTI_RCWSMASK_EL1_HI
#define CONFIG_ORLIX_TCTI_RCWSMASK_EL1_HI 0
#endif

/*
 * This is architectural configuration, not mutable test state.  Linux owns
 * whether an EL0 feature is advertised; the interpreter consumes the matching
 * EL1 register domain only after the runtime feature gate has admitted it.
 */
static const struct orlix_tcti_rcw_el1_state orlix_tcti_rcw_el1 = {
	.rcwmask_el1 = {
		(u64)CONFIG_ORLIX_TCTI_RCWMASK_EL1_LO,
		(u64)CONFIG_ORLIX_TCTI_RCWMASK_EL1_HI,
	},
	.rcwsmask_el1 = {
		(u64)CONFIG_ORLIX_TCTI_RCWSMASK_EL1_LO,
		(u64)CONFIG_ORLIX_TCTI_RCWSMASK_EL1_HI,
	},
	.feat_the = IS_ENABLED(CONFIG_ORLIX_TCTI_FEAT_THE),
	.feat_d128 = IS_ENABLED(CONFIG_ORLIX_TCTI_FEAT_D128),
	.tcr2_el1_enabled = IS_ENABLED(CONFIG_ORLIX_TCTI_TCR2_EL1_ENABLED),
	.tcr2_el1_pnch = IS_ENABLED(CONFIG_ORLIX_TCTI_TCR2_EL1_PNCH),
};

int orlix_tcti_rcw_el1_state_read(struct orlix_tcti_rcw_el1_state *state)
{
	if (!state)
		return -EINVAL;
	*state = orlix_tcti_rcw_el1;
	return 0;
}
