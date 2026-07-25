/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_SME_STATE_CONTRACT_H
#define ORLIX_TCTI_SME_STATE_CONTRACT_H

#include <linux/bitops.h>
#include <linux/types.h>

#include "decode_aarch64.h"

/*
 * SME is deliberately unavailable to hosted TCTI today.  This ledger records
 * the complete state and lifecycle work that must exist before an SME PSTATE
 * alias may execute.  It is a contract, not an emulated SME context.
 *
 * The alias encodings are the six architectural immediate forms currently
 * accepted by the decoder.  Their execution remains fail-closed until each
 * listed obligation has an authoritative ASL-backed implementation and its
 * owning production-path KUnit proof.
 */
enum tcti_sme_state_obligation {
	/* PSTATE.SM and PSTATE.ZA state must be task-owned and atomically updated. */
	TCTI_SME_STATE_PSTATE = BIT(0),
	/* Normal VL and streaming SVL are distinct architectural vector lengths. */
	TCTI_SME_STATE_VL_AND_SVL = BIT(1),
	/* ZA's ASL-defined layout and validity must be task-owned. */
	TCTI_SME_STATE_ZA = BIT(2),
	/* ZT0's ASL-defined layout and validity must be task-owned when applicable. */
	TCTI_SME_STATE_ZT0 = BIT(3),
	/* Scheduler handoff must preserve every enabled SME state component. */
	TCTI_SME_STATE_CONTEXT_SWITCH = BIT(4),
	/* Signal, exec, fork, and exit paths need Linux-visible state handling. */
	TCTI_SME_STATE_TASK_LIFECYCLE = BIT(5),
	/* ASL semantic entry, legal encoding, and exception behavior require proof. */
	TCTI_SME_STATE_ASL_AND_KUNIT = BIT(6),
};

#define TCTI_SME_STATE_EXECUTION_PREREQUISITES \
	(TCTI_SME_STATE_PSTATE | TCTI_SME_STATE_VL_AND_SVL | \
	 TCTI_SME_STATE_ZA | TCTI_SME_STATE_ZT0 | \
	 TCTI_SME_STATE_CONTEXT_SWITCH | TCTI_SME_STATE_TASK_LIFECYCLE | \
	 TCTI_SME_STATE_ASL_AND_KUNIT)

struct tcti_sme_pstate_alias_contract {
	u32 instruction;
	enum tcti_sme_pstate_operation operation;
	bool streaming_mode;
	bool za;
	unsigned long prerequisites;
};

static const struct tcti_sme_pstate_alias_contract
tcti_sme_pstate_alias_contracts[] = {
	{ 0xd503437fU, TCTI_SME_PSTATE_SMSTART, true, false,
	  TCTI_SME_STATE_EXECUTION_PREREQUISITES },
	{ 0xd503457fU, TCTI_SME_PSTATE_SMSTART, false, true,
	  TCTI_SME_STATE_EXECUTION_PREREQUISITES },
	{ 0xd503477fU, TCTI_SME_PSTATE_SMSTART, true, true,
	  TCTI_SME_STATE_EXECUTION_PREREQUISITES },
	{ 0xd503427fU, TCTI_SME_PSTATE_SMSTOP, true, false,
	  TCTI_SME_STATE_EXECUTION_PREREQUISITES },
	{ 0xd503447fU, TCTI_SME_PSTATE_SMSTOP, false, true,
	  TCTI_SME_STATE_EXECUTION_PREREQUISITES },
	{ 0xd503467fU, TCTI_SME_PSTATE_SMSTOP, true, true,
	  TCTI_SME_STATE_EXECUTION_PREREQUISITES },
};

#endif /* ORLIX_TCTI_SME_STATE_CONTRACT_H */
