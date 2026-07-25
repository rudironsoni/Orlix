/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_MOPS_PROVENANCE_H
#define ORLIX_TCTI_MOPS_PROVENANCE_H

#include <linux/types.h>

/*
 * MOPS must remain visible while the pinned AARCHMRS package lacks its shared
 * ASL bodies.  This is provenance only.  It is deliberately not an executor
 * and cannot be used to advertise FEAT_MOPS or FEAT_MOPS_GO.
 */
enum tcti_mops_semantics_status {
	TCTI_MOPS_SEMANTICS_SHARED_ASL_ABSENT_BLOCKING = 0,
};

enum tcti_mops_phase {
	TCTI_MOPS_PHASE_SET_GO,
	TCTI_MOPS_PHASE_COPY_FORWARD_PROLOGUE,
	TCTI_MOPS_PHASE_COPY_FORWARD_MAIN,
	TCTI_MOPS_PHASE_COPY_FORWARD_EPILOGUE,
	TCTI_MOPS_PHASE_SET_PROLOGUE,
	TCTI_MOPS_PHASE_SET_MAIN,
	TCTI_MOPS_PHASE_SET_EPILOGUE,
	TCTI_MOPS_PHASE_COPY_BACKWARD_PROLOGUE,
	TCTI_MOPS_PHASE_COPY_BACKWARD_MAIN,
	TCTI_MOPS_PHASE_COPY_BACKWARD_EPILOGUE,
	TCTI_MOPS_PHASE_SET_TAGGED_PROLOGUE,
	TCTI_MOPS_PHASE_SET_TAGGED_MAIN,
	TCTI_MOPS_PHASE_SET_TAGGED_EPILOGUE,
};

struct tcti_mops_leaf_provenance {
	u32 source_ordinal;
	const char *feature;
	const char *operation;
	const char *asl_operation;
	enum tcti_mops_phase phase;
	enum tcti_mops_semantics_status semantics_status;
};

#define TCTI_MOPS_GO_LEAF_COUNT	12U
#define TCTI_MOPS_LEAF_COUNT		120U
#define TCTI_MOPS_TOTAL_LEAF_COUNT	(TCTI_MOPS_GO_LEAF_COUNT + \
					 TCTI_MOPS_LEAF_COUNT)

bool tcti_mops_leaf_provenance(u32 source_ordinal,
			       struct tcti_mops_leaf_provenance *provenance);

#endif /* ORLIX_TCTI_MOPS_PROVENANCE_H */
