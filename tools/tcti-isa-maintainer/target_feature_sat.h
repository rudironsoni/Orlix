/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_FEATURE_SAT_H
#define ORLIX_TCTI_TARGET_FEATURE_SAT_H

#include <stddef.h>

#include "target_feature_model.h"
#include "target_inventory_import.h"

/* Host-only exact applicability audit. Resource limits fail the whole audit. */
#define TCTI_TARGET_FEATURE_SAT_DEFAULT_BRANCH_LIMIT 1000000U
#define TCTI_TARGET_FEATURE_SAT_DEFAULT_ALLOCATION_LIMIT (128U * 1024U * 1024U)

enum tcti_target_feature_sat_result {
	TCTI_TARGET_FEATURE_SAT_APPLICABLE,
	TCTI_TARGET_FEATURE_SAT_IMPOSSIBLE,
};

enum tcti_target_feature_sat_error_code {
	TCTI_TARGET_FEATURE_SAT_OK,
	TCTI_TARGET_FEATURE_SAT_INVALID_ARGUMENT,
	TCTI_TARGET_FEATURE_SAT_MALFORMED_MODEL,
	TCTI_TARGET_FEATURE_SAT_UNSUPPORTED_SEMANTICS,
	TCTI_TARGET_FEATURE_SAT_UNSAT_BASE,
	TCTI_TARGET_FEATURE_SAT_RESOURCE_LIMIT,
	TCTI_TARGET_FEATURE_SAT_NO_MEMORY,
};

struct tcti_target_feature_sat_error {
	enum tcti_target_feature_sat_error_code code;
	size_t leaf_index;
	struct tcti_feature_provenance provenance;
};

struct tcti_target_feature_sat_audit {
	enum tcti_target_feature_sat_result *leaves;
	size_t leaf_count;
};

/*
 * Computes SAT(Features.json constraints && leaf condition) for every leaf.
 * The grammar is type checked before solving. Any unsupported semantic form,
 * unsatisfiable base model, malformed graph, or resource exhaustion returns
 * nonzero and leaves no usable audit result. Both limits are cumulative across
 * the complete base-model and leaf audit, rather than per solver invocation.
 */
int tcti_target_feature_sat_audit(const struct tcti_feature_model *model,
	const struct tcti_target_inventory *inventory, size_t branch_limit,
	size_t allocation_limit,
	struct tcti_target_feature_sat_audit *audit,
	struct tcti_target_feature_sat_error *error);

void tcti_target_feature_sat_audit_destroy(
	struct tcti_target_feature_sat_audit *audit);

#endif /* ORLIX_TCTI_TARGET_FEATURE_SAT_H */
