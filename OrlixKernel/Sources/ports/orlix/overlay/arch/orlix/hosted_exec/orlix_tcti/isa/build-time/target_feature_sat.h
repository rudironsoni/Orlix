/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_FEATURE_SAT_H
#define ORLIX_TCTI_TARGET_FEATURE_SAT_H

#include <stddef.h>
#include <stdint.h>

#include "target_feature_model.h"
#include "target_feature_field_domain_binding.h"
#include "target_inventory_import.h"

/* Host-only exact applicability audit. Resource limits fail the whole audit. */
#define ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_BRANCH_LIMIT 10000000U
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

enum orlix_tcti_target_feature_sat_symbol_kind {
	ORLIX_TCTI_TARGET_FEATURE_SAT_CONFIGURATION,
	ORLIX_TCTI_TARGET_FEATURE_SAT_FIELD,
	ORLIX_TCTI_TARGET_FEATURE_SAT_TARGET_OPERAND,
	ORLIX_TCTI_TARGET_FEATURE_SAT_BOOLEAN_IDENTIFIER,
};

enum orlix_tcti_target_feature_sat_numeric_kind {
	ORLIX_TCTI_TARGET_FEATURE_SAT_UNSIGNED,
	ORLIX_TCTI_TARGET_FEATURE_SAT_SIGNED,
};

struct orlix_tcti_target_feature_sat_value {
	enum orlix_tcti_target_feature_sat_symbol_kind kind;
	enum orlix_tcti_target_feature_sat_numeric_kind numeric_kind;
	uint32_t identity_group_index;
	uint32_t leaf_index;
	uint32_t feature_node_index;
	const char *name;
	uint64_t low;
	uint64_t high;
	uint8_t width;
};

struct orlix_tcti_target_feature_sat_certificate {
	uint32_t condition_index;
	uint64_t formula_identity;
	size_t first_value;
	size_t value_count;
};

struct orlix_tcti_target_feature_sat_audit {
	enum orlix_tcti_target_feature_sat_result *leaves;
	/*
	 * Canonical SAT assignments, stored leaf-major. Each applicable leaf owns
	 * parameter_count signed values: -1 false, +1 true. Impossible leaves are
	 * all zero. Parameter ordering is exactly Features.json parameter order.
	 */
	signed char *witnesses;
	struct orlix_tcti_target_feature_sat_certificate *certificates;
	struct orlix_tcti_target_feature_sat_value *certificate_values;
	size_t certificate_value_count;
	size_t certificate_value_capacity;
	size_t leaf_count, parameter_count;
};

/* Returns an applicable leaf's canonical parameter assignment, or NULL. */
const signed char *orlix_tcti_target_feature_sat_witness(
	const struct orlix_tcti_target_feature_sat_audit *audit, size_t leaf_index);

const struct orlix_tcti_target_feature_sat_certificate *
orlix_tcti_target_feature_sat_certificate(
	const struct orlix_tcti_target_feature_sat_audit *audit, size_t leaf_index);

const struct orlix_tcti_target_feature_sat_value *
orlix_tcti_target_feature_sat_certificate_values(
	const struct orlix_tcti_target_feature_sat_audit *audit,
	size_t leaf_index, size_t *value_count);

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
	const struct orlix_tcti_feature_field_domain_bindings *field_domains,
	struct orlix_tcti_target_feature_sat_audit *audit,
	struct orlix_tcti_target_feature_sat_error *error);

void orlix_tcti_target_feature_sat_audit_destroy(
	struct orlix_tcti_target_feature_sat_audit *audit);

#endif /* ORLIX_TCTI_TARGET_FEATURE_SAT_H */
