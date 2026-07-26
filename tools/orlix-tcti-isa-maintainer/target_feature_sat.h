/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_FEATURE_SAT_H
#define ORLIX_TCTI_TARGET_FEATURE_SAT_H

#include <stddef.h>

#include "target_feature_model.h"
#include "target_inventory_import.h"

/* Host-only exact applicability audit. Resource limits fail the whole audit. */
#define ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_BRANCH_LIMIT 1000000U
#define ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_ALLOCATION_LIMIT (128U * 1024U * 1024U)

enum orlix_tcti_target_feature_sat_result {
	ORLIX_TCTI_TARGET_FEATURE_SAT_APPLICABLE,
	ORLIX_TCTI_TARGET_FEATURE_SAT_IMPOSSIBLE,
};

enum orlix_tcti_target_feature_sat_error_code {
	ORLIX_TCTI_TARGET_FEATURE_SAT_OK,
	ORLIX_TCTI_TARGET_FEATURE_SAT_INVALID_ARGUMENT,
	ORLIX_TCTI_TARGET_FEATURE_SAT_MALFORMED_MODEL,
	ORLIX_TCTI_TARGET_FEATURE_SAT_UNSUPPORTED_SEMANTICS,
	ORLIX_TCTI_TARGET_FEATURE_SAT_UNSAT_BASE,
	ORLIX_TCTI_TARGET_FEATURE_SAT_RESOURCE_LIMIT,
	ORLIX_TCTI_TARGET_FEATURE_SAT_NO_MEMORY,
};

struct orlix_tcti_target_feature_sat_error {
	enum orlix_tcti_target_feature_sat_error_code code;
	size_t leaf_index;
	struct orlix_tcti_feature_provenance provenance;
};

struct orlix_tcti_target_feature_sat_audit {
	enum orlix_tcti_target_feature_sat_result *leaves;
	/*
	 * Canonical SAT assignments, stored leaf-major. Each applicable leaf owns
	 * parameter_count signed values: -1 false, +1 true. Impossible leaves are
	 * all zero. Parameter ordering is exactly Features.json parameter order.
	 */
	signed char *witnesses;
	size_t leaf_count, parameter_count;
};

/* Returns an applicable leaf's canonical parameter assignment, or NULL. */
const signed char *orlix_tcti_target_feature_sat_witness(
	const struct orlix_tcti_target_feature_sat_audit *audit, size_t leaf_index);

/*
 * Computes SAT(Features.json constraints && leaf condition) for every leaf.
 * The grammar is type checked before solving. Any unsupported semantic form,
 * unsatisfiable base model, malformed graph, or resource exhaustion returns
 * nonzero and leaves no usable audit result. Both limits are cumulative across
 * the complete base-model and leaf audit, rather than per solver invocation.
 * For every applicable Boolean leaf, the audit also returns the first
 * deterministic assignment produced by the fixed variable and branch order.
 */
int orlix_tcti_target_feature_sat_audit(const struct orlix_tcti_feature_model *model,
	const struct orlix_tcti_target_inventory *inventory, size_t branch_limit,
	size_t allocation_limit,
	struct orlix_tcti_target_feature_sat_audit *audit,
	struct orlix_tcti_target_feature_sat_error *error);

void orlix_tcti_target_feature_sat_audit_destroy(
	struct orlix_tcti_target_feature_sat_audit *audit);

#endif /* ORLIX_TCTI_TARGET_FEATURE_SAT_H */
