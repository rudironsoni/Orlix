/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_SME_STATE_CONTRACT_H
#define ORLIX_TCTI_SME_STATE_CONTRACT_H

#include <linux/bitops.h>
#include <linux/types.h>

#include "decode_aarch64.h"

/*
 * SME is deliberately unavailable to hosted OrlixTCTI today.  This ledger records
 * the complete state and lifecycle work that must exist before an SME PSTATE
 * alias may execute.  It is a contract, not an emulated SME context.
 *
 * The alias encodings are the six architectural immediate forms currently
 * accepted by the decoder.  Their execution remains fail-closed until each
 * listed obligation has an architecture-conformant implementation and its
 * owning production-path KUnit proof. External DDI0602 provenance is tracked
 * separately and grants no implementation or proof credit.
 */
enum orlix_tcti_sme_state_obligation {
	/* PSTATE.SM and PSTATE.ZA state must be task-owned and atomically updated. */
	ORLIX_TCTI_SME_STATE_PSTATE = BIT(0),
	/* Normal VL and streaming SVL are distinct architectural vector lengths. */
	ORLIX_TCTI_SME_STATE_VL_AND_SVL = BIT(1),
	/* ZA's architecture-defined layout and validity must be task-owned. */
	ORLIX_TCTI_SME_STATE_ZA = BIT(2),
	/* ZT0's architecture-defined layout and validity must be task-owned. */
	ORLIX_TCTI_SME_STATE_ZT0 = BIT(3),
	/* Scheduler handoff must preserve every enabled SME state component. */
	ORLIX_TCTI_SME_STATE_CONTEXT_SWITCH = BIT(4),
	/* Signal, exec, fork, and exit paths need Linux-visible state handling. */
	ORLIX_TCTI_SME_STATE_TASK_LIFECYCLE = BIT(5),
	/* Semantic behavior, legal encoding, and exceptions require KUnit proof. */
	ORLIX_TCTI_SME_STATE_SEMANTICS_AND_KUNIT = BIT(6),
};

enum orlix_tcti_sme_semantic_provenance {
	ORLIX_TCTI_SME_PROVENANCE_EXTERNAL_DDI0602,
};

enum orlix_tcti_sme_implementation_status {
	ORLIX_TCTI_SME_IMPLEMENTATION_REQUIRED_UNIMPLEMENTED,
};

enum orlix_tcti_sme_proof_status {
	ORLIX_TCTI_SME_PROOF_REQUIRED_UNPROVEN,
};

#define ORLIX_TCTI_SME_STATE_EXECUTION_PREREQUISITES \
	(ORLIX_TCTI_SME_STATE_PSTATE | ORLIX_TCTI_SME_STATE_VL_AND_SVL | \
	 ORLIX_TCTI_SME_STATE_ZA | ORLIX_TCTI_SME_STATE_ZT0 | \
	 ORLIX_TCTI_SME_STATE_CONTEXT_SWITCH | ORLIX_TCTI_SME_STATE_TASK_LIFECYCLE | \
	 ORLIX_TCTI_SME_STATE_SEMANTICS_AND_KUNIT)

struct orlix_tcti_sme_pstate_alias_contract {
	u32 instruction;
	enum orlix_tcti_sme_pstate_operation operation;
	bool streaming_mode;
	bool za;
	unsigned long prerequisites;
	enum orlix_tcti_sme_semantic_provenance semantic_provenance;
	enum orlix_tcti_sme_implementation_status implementation_status;
	enum orlix_tcti_sme_proof_status proof_status;
};

static const struct orlix_tcti_sme_pstate_alias_contract
orlix_tcti_sme_pstate_alias_contracts[] = {
	{ 0xd503437fU, ORLIX_TCTI_SME_PSTATE_SMSTART, true, false,
	  ORLIX_TCTI_SME_STATE_EXECUTION_PREREQUISITES,
	  ORLIX_TCTI_SME_PROVENANCE_EXTERNAL_DDI0602,
	  ORLIX_TCTI_SME_IMPLEMENTATION_REQUIRED_UNIMPLEMENTED,
	  ORLIX_TCTI_SME_PROOF_REQUIRED_UNPROVEN },
	{ 0xd503457fU, ORLIX_TCTI_SME_PSTATE_SMSTART, false, true,
	  ORLIX_TCTI_SME_STATE_EXECUTION_PREREQUISITES,
	  ORLIX_TCTI_SME_PROVENANCE_EXTERNAL_DDI0602,
	  ORLIX_TCTI_SME_IMPLEMENTATION_REQUIRED_UNIMPLEMENTED,
	  ORLIX_TCTI_SME_PROOF_REQUIRED_UNPROVEN },
	{ 0xd503477fU, ORLIX_TCTI_SME_PSTATE_SMSTART, true, true,
	  ORLIX_TCTI_SME_STATE_EXECUTION_PREREQUISITES,
	  ORLIX_TCTI_SME_PROVENANCE_EXTERNAL_DDI0602,
	  ORLIX_TCTI_SME_IMPLEMENTATION_REQUIRED_UNIMPLEMENTED,
	  ORLIX_TCTI_SME_PROOF_REQUIRED_UNPROVEN },
	{ 0xd503427fU, ORLIX_TCTI_SME_PSTATE_SMSTOP, true, false,
	  ORLIX_TCTI_SME_STATE_EXECUTION_PREREQUISITES,
	  ORLIX_TCTI_SME_PROVENANCE_EXTERNAL_DDI0602,
	  ORLIX_TCTI_SME_IMPLEMENTATION_REQUIRED_UNIMPLEMENTED,
	  ORLIX_TCTI_SME_PROOF_REQUIRED_UNPROVEN },
	{ 0xd503447fU, ORLIX_TCTI_SME_PSTATE_SMSTOP, false, true,
	  ORLIX_TCTI_SME_STATE_EXECUTION_PREREQUISITES,
	  ORLIX_TCTI_SME_PROVENANCE_EXTERNAL_DDI0602,
	  ORLIX_TCTI_SME_IMPLEMENTATION_REQUIRED_UNIMPLEMENTED,
	  ORLIX_TCTI_SME_PROOF_REQUIRED_UNPROVEN },
	{ 0xd503467fU, ORLIX_TCTI_SME_PSTATE_SMSTOP, true, true,
	  ORLIX_TCTI_SME_STATE_EXECUTION_PREREQUISITES,
	  ORLIX_TCTI_SME_PROVENANCE_EXTERNAL_DDI0602,
	  ORLIX_TCTI_SME_IMPLEMENTATION_REQUIRED_UNIMPLEMENTED,
	  ORLIX_TCTI_SME_PROOF_REQUIRED_UNPROVEN },
};

#endif /* ORLIX_TCTI_SME_STATE_CONTRACT_H */
