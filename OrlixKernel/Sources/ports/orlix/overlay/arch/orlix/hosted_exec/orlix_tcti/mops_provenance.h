/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_MOPS_PROVENANCE_H
#define ORLIX_TCTI_MOPS_PROVENANCE_H

#include <linux/types.h>

/*
 * MOPS semantic provenance, implementation state, and proof state are
 * orthogonal. This ledger is deliberately not an executor and cannot be used
 * to advertise FEAT_MOPS or FEAT_MOPS_GO.
 */
enum orlix_tcti_mops_semantic_provenance {
	ORLIX_TCTI_MOPS_PROVENANCE_EXTERNAL_DDI0602 = 0,
	ORLIX_TCTI_MOPS_PROVENANCE_OFFICIAL_NOT_SPECIFIED,
};

enum orlix_tcti_mops_implementation_status {
	ORLIX_TCTI_MOPS_IMPLEMENTATION_REQUIRED_UNIMPLEMENTED = 0,
	ORLIX_TCTI_MOPS_IMPLEMENTATION_PRODUCTION,
};

enum orlix_tcti_mops_proof_status {
	ORLIX_TCTI_MOPS_PROOF_REQUIRED_UNPROVEN = 0,
	ORLIX_TCTI_MOPS_PROOF_KUNIT_OWNER,
};

enum orlix_tcti_mops_phase {
	ORLIX_TCTI_MOPS_PHASE_SET_GO,
	ORLIX_TCTI_MOPS_PHASE_COPY_FORWARD_PROLOGUE,
	ORLIX_TCTI_MOPS_PHASE_COPY_FORWARD_MAIN,
	ORLIX_TCTI_MOPS_PHASE_COPY_FORWARD_EPILOGUE,
	ORLIX_TCTI_MOPS_PHASE_SET_PROLOGUE,
	ORLIX_TCTI_MOPS_PHASE_SET_MAIN,
	ORLIX_TCTI_MOPS_PHASE_SET_EPILOGUE,
	ORLIX_TCTI_MOPS_PHASE_COPY_BACKWARD_PROLOGUE,
	ORLIX_TCTI_MOPS_PHASE_COPY_BACKWARD_MAIN,
	ORLIX_TCTI_MOPS_PHASE_COPY_BACKWARD_EPILOGUE,
	ORLIX_TCTI_MOPS_PHASE_SET_TAGGED_PROLOGUE,
	ORLIX_TCTI_MOPS_PHASE_SET_TAGGED_MAIN,
	ORLIX_TCTI_MOPS_PHASE_SET_TAGGED_EPILOGUE,
};

struct orlix_tcti_mops_leaf_provenance {
	u32 source_ordinal;
	const char *feature;
	const char *operation;
	const char *asl_operation;
	const char *ddi0602_locator;
	const char *ddi0602_archive_sha256;
	const char *production_owner;
	const char *kunit_suite;
	const char *linux_proof_disposition;
	enum orlix_tcti_mops_phase phase;
	enum orlix_tcti_mops_semantic_provenance semantic_provenance;
	enum orlix_tcti_mops_implementation_status implementation_status;
	enum orlix_tcti_mops_proof_status proof_status;
};

#define ORLIX_TCTI_MOPS_GO_LEAF_COUNT	12U
#define ORLIX_TCTI_MOPS_LEAF_COUNT		120U
#define ORLIX_TCTI_MOPS_TOTAL_LEAF_COUNT	(ORLIX_TCTI_MOPS_GO_LEAF_COUNT + \
					 ORLIX_TCTI_MOPS_LEAF_COUNT)

bool orlix_tcti_mops_leaf_provenance(u32 source_ordinal,
			       struct orlix_tcti_mops_leaf_provenance *provenance);

#endif /* ORLIX_TCTI_MOPS_PROVENANCE_H */
