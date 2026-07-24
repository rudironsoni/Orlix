/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_INVENTORY_CONTRACT_H
#define ORLIX_TCTI_INVENTORY_CONTRACT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum tcti_inventory_classification {
	TCTI_INVENTORY_UNCLASSIFIED = 0,
	TCTI_INVENTORY_REQUIRED_EL0,
	TCTI_INVENTORY_NON_EL0,
	TCTI_INVENTORY_ARCH_UNDEFINED_OR_UNALLOCATED,
	TCTI_INVENTORY_ALIAS_OR_DUPLICATE,
};

enum tcti_inventory_capability_word {
	TCTI_INVENTORY_HWCAP = 0,
	TCTI_INVENTORY_HWCAP2,
};

#define TCTI_INVENTORY_AUTHORITATIVE_TARGET_LEAF_COUNT 4350U

enum tcti_inventory_error {
	TCTI_INVENTORY_ERROR_NONE = 0,
	TCTI_INVENTORY_ERROR_TARGET_COUNT,
	TCTI_INVENTORY_ERROR_EMPTY_NAME,
	TCTI_INVENTORY_ERROR_DUPLICATE_NAME,
	TCTI_INVENTORY_ERROR_MISSING_CLASSIFICATION,
	TCTI_INVENTORY_ERROR_UNKNOWN_CLASSIFICATION,
	TCTI_INVENTORY_ERROR_UNKNOWN_CONDITION,
	TCTI_INVENTORY_ERROR_EMPTY_CONDITION_ID,
	TCTI_INVENTORY_ERROR_EMPTY_CONDITION_EXPRESSION,
	TCTI_INVENTORY_ERROR_DUPLICATE_CONDITION,
	TCTI_INVENTORY_ERROR_EMPTY_CONDITION_FEATURE,
	TCTI_INVENTORY_ERROR_MISSING_ALIAS_TARGET,
	TCTI_INVENTORY_ERROR_UNKNOWN_ALIAS_TARGET,
	TCTI_INVENTORY_ERROR_SELF_ALIAS,
	TCTI_INVENTORY_ERROR_UNEXPECTED_ALIAS_TARGET,
	TCTI_INVENTORY_ERROR_MISSING_PROOF,
	TCTI_INVENTORY_ERROR_UNPROVED_LEAF,
	TCTI_INVENTORY_ERROR_ALIAS_TARGET_NOT_CANONICAL,
	TCTI_INVENTORY_ERROR_ALIAS_CONDITION_MISMATCH,
	TCTI_INVENTORY_ERROR_EMPTY_CAPABILITY_FEATURE,
	TCTI_INVENTORY_ERROR_INVALID_CAPABILITY_WORD,
	TCTI_INVENTORY_ERROR_ZERO_CAPABILITY_MASK,
	TCTI_INVENTORY_ERROR_DUPLICATE_CAPABILITY,
	TCTI_INVENTORY_ERROR_UNKNOWN_ADVERTISED_CAPABILITY,
	TCTI_INVENTORY_ERROR_PARTIAL_CAPABILITY,
	TCTI_INVENTORY_ERROR_CAPABILITY_WITHOUT_LEAVES,
	TCTI_INVENTORY_ERROR_ADVERTISED_CAPABILITY_WITHOUT_PROOF,
};

struct tcti_inventory_leaf {
	const char *name;
	enum tcti_inventory_classification classification;
	const char *condition_id;
	const char *alias_target;
	const char *proof;
	bool proved;
};

struct tcti_inventory_condition {
	const char *id;
	const char *expression;
	const char *const *features;
	size_t feature_count;
};

struct tcti_inventory_capability {
	const char *feature;
	enum tcti_inventory_capability_word word;
	uint64_t mask;
};

struct tcti_inventory_runtime_profile {
	uint64_t hwcap;
	uint64_t hwcap2;
};

struct tcti_inventory_result {
	enum tcti_inventory_error first_error;
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

int tcti_inventory_validate_target(
	const struct tcti_inventory_leaf *leaves,
	size_t leaf_count,
	const struct tcti_inventory_condition *conditions,
	size_t condition_count,
	struct tcti_inventory_result *result);

int tcti_inventory_validate_runtime_projection(
	const struct tcti_inventory_leaf *leaves,
	size_t leaf_count,
	const struct tcti_inventory_condition *conditions,
	size_t condition_count,
	const struct tcti_inventory_capability *capabilities,
	size_t capability_count,
	const struct tcti_inventory_runtime_profile *profile,
	struct tcti_inventory_result *result);

#endif /* ORLIX_TCTI_INVENTORY_CONTRACT_H */
