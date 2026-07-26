/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_INVENTORY_CONTRACT_H
#define ORLIX_TCTI_INVENTORY_CONTRACT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum orlix_tcti_inventory_classification {
	ORLIX_TCTI_INVENTORY_UNCLASSIFIED = 0,
	ORLIX_TCTI_INVENTORY_REQUIRED_EL0,
	ORLIX_TCTI_INVENTORY_NON_EL0,
	ORLIX_TCTI_INVENTORY_ARCH_UNDEFINED_OR_UNALLOCATED,
	ORLIX_TCTI_INVENTORY_ALIAS_OR_DUPLICATE,
};

enum orlix_tcti_inventory_capability_word {
	ORLIX_TCTI_INVENTORY_HWCAP = 0,
	ORLIX_TCTI_INVENTORY_HWCAP2,
};

#define ORLIX_TCTI_INVENTORY_AUTHORITATIVE_TARGET_LEAF_COUNT 4350U

enum orlix_tcti_inventory_error {
	ORLIX_TCTI_INVENTORY_ERROR_NONE = 0,
	ORLIX_TCTI_INVENTORY_ERROR_TARGET_COUNT,
	ORLIX_TCTI_INVENTORY_ERROR_EMPTY_NAME,
	ORLIX_TCTI_INVENTORY_ERROR_DUPLICATE_NAME,
	ORLIX_TCTI_INVENTORY_ERROR_MISSING_CLASSIFICATION,
	ORLIX_TCTI_INVENTORY_ERROR_UNKNOWN_CLASSIFICATION,
	ORLIX_TCTI_INVENTORY_ERROR_UNKNOWN_CONDITION,
	ORLIX_TCTI_INVENTORY_ERROR_EMPTY_CONDITION_ID,
	ORLIX_TCTI_INVENTORY_ERROR_EMPTY_CONDITION_EXPRESSION,
	ORLIX_TCTI_INVENTORY_ERROR_DUPLICATE_CONDITION,
	ORLIX_TCTI_INVENTORY_ERROR_EMPTY_CONDITION_FEATURE,
	ORLIX_TCTI_INVENTORY_ERROR_MISSING_ALIAS_TARGET,
	ORLIX_TCTI_INVENTORY_ERROR_UNKNOWN_ALIAS_TARGET,
	ORLIX_TCTI_INVENTORY_ERROR_SELF_ALIAS,
	ORLIX_TCTI_INVENTORY_ERROR_UNEXPECTED_ALIAS_TARGET,
	ORLIX_TCTI_INVENTORY_ERROR_MISSING_PROOF,
	ORLIX_TCTI_INVENTORY_ERROR_UNPROVED_LEAF,
	ORLIX_TCTI_INVENTORY_ERROR_ALIAS_TARGET_NOT_CANONICAL,
	ORLIX_TCTI_INVENTORY_ERROR_ALIAS_CONDITION_MISMATCH,
	ORLIX_TCTI_INVENTORY_ERROR_EMPTY_CAPABILITY_FEATURE,
	ORLIX_TCTI_INVENTORY_ERROR_INVALID_CAPABILITY_WORD,
	ORLIX_TCTI_INVENTORY_ERROR_ZERO_CAPABILITY_MASK,
	ORLIX_TCTI_INVENTORY_ERROR_DUPLICATE_CAPABILITY,
	ORLIX_TCTI_INVENTORY_ERROR_UNKNOWN_ADVERTISED_CAPABILITY,
	ORLIX_TCTI_INVENTORY_ERROR_PARTIAL_CAPABILITY,
	ORLIX_TCTI_INVENTORY_ERROR_CAPABILITY_WITHOUT_LEAVES,
	ORLIX_TCTI_INVENTORY_ERROR_ADVERTISED_CAPABILITY_WITHOUT_PROOF,
};

struct orlix_tcti_inventory_leaf {
	const char *name;
	enum orlix_tcti_inventory_classification classification;
	const char *condition_id;
	const char *alias_target;
	const char *proof;
	bool proved;
};

struct orlix_tcti_inventory_condition {
	const char *id;
	const char *expression;
	const char *const *features;
	size_t feature_count;
};

struct orlix_tcti_inventory_capability {
	const char *feature;
	enum orlix_tcti_inventory_capability_word word;
	uint64_t mask;
};

struct orlix_tcti_inventory_runtime_profile {
	uint64_t hwcap;
	uint64_t hwcap2;
};

struct orlix_tcti_inventory_result {
	enum orlix_tcti_inventory_error first_error;
	size_t first_error_index;
	size_t errors;
	size_t target_count;
	size_t classified_count;
	size_t required_count;
	size_t non_el0_count;
	size_t undefined_count;
	size_t alias_count;
	size_t conditioned_count;
	size_t advertised_capabilities;
	size_t proved_advertised_capabilities;
};

int orlix_tcti_inventory_validate_target(
	const struct orlix_tcti_inventory_leaf *leaves,
	size_t leaf_count,
	const struct orlix_tcti_inventory_condition *conditions,
	size_t condition_count,
	struct orlix_tcti_inventory_result *result);

int orlix_tcti_inventory_validate_runtime_projection(
	const struct orlix_tcti_inventory_leaf *leaves,
	size_t leaf_count,
	const struct orlix_tcti_inventory_condition *conditions,
	size_t condition_count,
	const struct orlix_tcti_inventory_capability *capabilities,
	size_t capability_count,
	const struct orlix_tcti_inventory_runtime_profile *profile,
	struct orlix_tcti_inventory_result *result);

#endif /* ORLIX_TCTI_INVENTORY_CONTRACT_H */
