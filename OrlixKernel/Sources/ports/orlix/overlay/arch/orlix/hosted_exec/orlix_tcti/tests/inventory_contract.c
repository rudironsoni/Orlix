// SPDX-License-Identifier: GPL-2.0-only
#include "inventory_contract.h"

#include <string.h>

static bool orlix_tcti_inventory_string_is_empty(const char *string)
{
	return !string || !string[0];
}

static bool orlix_tcti_inventory_conditions_match(const char *left,
					 const char *right)
{
	if (orlix_tcti_inventory_string_is_empty(left) ||
	    orlix_tcti_inventory_string_is_empty(right))
		return orlix_tcti_inventory_string_is_empty(left) &&
		       orlix_tcti_inventory_string_is_empty(right);
	return !strcmp(left, right);
}

static void orlix_tcti_inventory_record_error(
	struct orlix_tcti_inventory_result *result,
	enum orlix_tcti_inventory_error error,
	size_t index)
{
	if (!result->errors) {
		result->first_error = error;
		result->first_error_index = index;
	}
	result->errors++;
}

static size_t orlix_tcti_inventory_find_leaf(
	const struct orlix_tcti_inventory_leaf *leaves,
	size_t leaf_count,
	const char *name)
{
	size_t index;

	for (index = 0; index < leaf_count; index++)
		if (!orlix_tcti_inventory_string_is_empty(leaves[index].name) &&
		    !strcmp(leaves[index].name, name))
			return index;
	return leaf_count;
}

static void orlix_tcti_inventory_count_classification(
	const struct orlix_tcti_inventory_leaf *leaf,
	struct orlix_tcti_inventory_result *result)
{
	switch (leaf->classification) {
	case ORLIX_TCTI_INVENTORY_REQUIRED_EL0:
		result->required_count++;
		break;
	case ORLIX_TCTI_INVENTORY_NON_EL0:
		result->non_el0_count++;
		break;
	case ORLIX_TCTI_INVENTORY_ARCH_UNDEFINED_OR_UNALLOCATED:
		result->undefined_count++;
		break;
	case ORLIX_TCTI_INVENTORY_ALIAS_OR_DUPLICATE:
		result->alias_count++;
		break;
	case ORLIX_TCTI_INVENTORY_UNCLASSIFIED:
		break;
	default:
		break;
	}
}

static size_t orlix_tcti_inventory_find_condition(
	const struct orlix_tcti_inventory_condition *conditions,
	size_t condition_count,
	const char *id)
{
	size_t index;

	for (index = 0; index < condition_count; index++)
		if (!orlix_tcti_inventory_string_is_empty(conditions[index].id) &&
		    !strcmp(conditions[index].id, id))
			return index;
	return condition_count;
}

static void orlix_tcti_inventory_validate_conditions(
	const struct orlix_tcti_inventory_condition *conditions,
	size_t condition_count,
	struct orlix_tcti_inventory_result *result)
{
	size_t index;

	for (index = 0; index < condition_count; index++) {
		const struct orlix_tcti_inventory_condition *condition =
			&conditions[index];
		size_t feature;
		size_t previous;

		if (orlix_tcti_inventory_string_is_empty(condition->id))
			orlix_tcti_inventory_record_error(
				result, ORLIX_TCTI_INVENTORY_ERROR_EMPTY_CONDITION_ID,
				index);
		if (orlix_tcti_inventory_string_is_empty(condition->expression))
			orlix_tcti_inventory_record_error(
				result,
				ORLIX_TCTI_INVENTORY_ERROR_EMPTY_CONDITION_EXPRESSION,
				index);
		for (previous = 0; previous < index; previous++)
			if (!orlix_tcti_inventory_string_is_empty(condition->id) &&
			    !orlix_tcti_inventory_string_is_empty(
				    conditions[previous].id) &&
			    !strcmp(condition->id, conditions[previous].id)) {
				orlix_tcti_inventory_record_error(
					result,
					ORLIX_TCTI_INVENTORY_ERROR_DUPLICATE_CONDITION,
					index);
				break;
			}
		if (condition->feature_count && !condition->features) {
			orlix_tcti_inventory_record_error(
				result,
				ORLIX_TCTI_INVENTORY_ERROR_EMPTY_CONDITION_FEATURE,
				index);
			continue;
		}
		for (feature = 0; feature < condition->feature_count; feature++)
			if (orlix_tcti_inventory_string_is_empty(
				    condition->features[feature]))
				orlix_tcti_inventory_record_error(
					result,
					ORLIX_TCTI_INVENTORY_ERROR_EMPTY_CONDITION_FEATURE,
					index);
	}
}

int orlix_tcti_inventory_validate_target(
	const struct orlix_tcti_inventory_leaf *leaves,
	size_t leaf_count,
	const struct orlix_tcti_inventory_condition *conditions,
	size_t condition_count,
	struct orlix_tcti_inventory_result *result)
{
	size_t index;

	if (!result)
		return -1;
	memset(result, 0, sizeof(*result));
	result->target_count = leaf_count;
	if (!leaves ||
	    leaf_count != ORLIX_TCTI_INVENTORY_AUTHORITATIVE_TARGET_LEAF_COUNT)
		orlix_tcti_inventory_record_error(result,
					    ORLIX_TCTI_INVENTORY_ERROR_TARGET_COUNT,
					    leaf_count);
	if (!leaves)
		return -1;
	if (!conditions && condition_count) {
		orlix_tcti_inventory_record_error(
			result, ORLIX_TCTI_INVENTORY_ERROR_UNKNOWN_CONDITION, 0);
		return -1;
	}
	orlix_tcti_inventory_validate_conditions(conditions, condition_count, result);

	for (index = 0; index < leaf_count; index++) {
		const struct orlix_tcti_inventory_leaf *leaf = &leaves[index];
		size_t previous;

		if (orlix_tcti_inventory_string_is_empty(leaf->name)) {
			orlix_tcti_inventory_record_error(
				result, ORLIX_TCTI_INVENTORY_ERROR_EMPTY_NAME, index);
		} else {
			for (previous = 0; previous < index; previous++)
				if (!orlix_tcti_inventory_string_is_empty(
					    leaves[previous].name) &&
				    !strcmp(leaves[previous].name, leaf->name)) {
					orlix_tcti_inventory_record_error(
						result,
						ORLIX_TCTI_INVENTORY_ERROR_DUPLICATE_NAME,
						index);
					break;
				}
		}

		if (leaf->classification == ORLIX_TCTI_INVENTORY_UNCLASSIFIED) {
			orlix_tcti_inventory_record_error(
				result,
				ORLIX_TCTI_INVENTORY_ERROR_MISSING_CLASSIFICATION,
				index);
			continue;
		}
		if (leaf->classification > ORLIX_TCTI_INVENTORY_ALIAS_OR_DUPLICATE) {
			orlix_tcti_inventory_record_error(
				result,
				ORLIX_TCTI_INVENTORY_ERROR_UNKNOWN_CLASSIFICATION,
				index);
			continue;
		}
		result->classified_count++;
		orlix_tcti_inventory_count_classification(leaf, result);

		if (!orlix_tcti_inventory_string_is_empty(leaf->condition_id)) {
			if (orlix_tcti_inventory_find_condition(
				    conditions, condition_count,
				    leaf->condition_id) == condition_count)
				orlix_tcti_inventory_record_error(
					result,
					ORLIX_TCTI_INVENTORY_ERROR_UNKNOWN_CONDITION,
					index);
			else
				result->conditioned_count++;
		}

		if (leaf->classification == ORLIX_TCTI_INVENTORY_ALIAS_OR_DUPLICATE) {
			size_t target;

			if (orlix_tcti_inventory_string_is_empty(leaf->alias_target)) {
				orlix_tcti_inventory_record_error(
					result,
					ORLIX_TCTI_INVENTORY_ERROR_MISSING_ALIAS_TARGET,
					index);
			} else if (!strcmp(leaf->name, leaf->alias_target)) {
				orlix_tcti_inventory_record_error(
					result, ORLIX_TCTI_INVENTORY_ERROR_SELF_ALIAS,
					index);
			} else {
				target = orlix_tcti_inventory_find_leaf(
					leaves, leaf_count, leaf->alias_target);
				if (target == leaf_count)
					orlix_tcti_inventory_record_error(
						result,
						ORLIX_TCTI_INVENTORY_ERROR_UNKNOWN_ALIAS_TARGET,
						index);
				else if (leaves[target].classification ==
					 ORLIX_TCTI_INVENTORY_ALIAS_OR_DUPLICATE)
					orlix_tcti_inventory_record_error(
						result,
						ORLIX_TCTI_INVENTORY_ERROR_ALIAS_TARGET_NOT_CANONICAL,
						index);
				else if (!orlix_tcti_inventory_conditions_match(
						 leaf->condition_id,
						 leaves[target].condition_id))
					orlix_tcti_inventory_record_error(
						result,
						ORLIX_TCTI_INVENTORY_ERROR_ALIAS_CONDITION_MISMATCH,
						index);
				else if (!leaves[target].proved)
					orlix_tcti_inventory_record_error(
						result,
						ORLIX_TCTI_INVENTORY_ERROR_UNPROVED_LEAF,
						index);
			}
		} else if (!orlix_tcti_inventory_string_is_empty(leaf->alias_target)) {
			orlix_tcti_inventory_record_error(
				result,
				ORLIX_TCTI_INVENTORY_ERROR_UNEXPECTED_ALIAS_TARGET,
				index);
		}

		if (orlix_tcti_inventory_string_is_empty(leaf->proof))
			orlix_tcti_inventory_record_error(
				result, ORLIX_TCTI_INVENTORY_ERROR_MISSING_PROOF,
				index);
		else if (leaf->classification != ORLIX_TCTI_INVENTORY_ALIAS_OR_DUPLICATE &&
			 !leaf->proved)
			orlix_tcti_inventory_record_error(
				result, ORLIX_TCTI_INVENTORY_ERROR_UNPROVED_LEAF, index);
	}

	return result->errors ? -1 : 0;
}

static uint64_t orlix_tcti_inventory_profile_word(
	const struct orlix_tcti_inventory_runtime_profile *profile,
	enum orlix_tcti_inventory_capability_word word)
{
	return word == ORLIX_TCTI_INVENTORY_HWCAP ? profile->hwcap : profile->hwcap2;
}

static bool orlix_tcti_inventory_feature_has_leaves(
	const struct orlix_tcti_inventory_leaf *leaves,
	size_t leaf_count,
	const struct orlix_tcti_inventory_condition *conditions,
	size_t condition_count,
	const char *feature,
	bool *proved)
{
	bool found = false;
	size_t index;

	*proved = true;
	for (index = 0; index < leaf_count; index++) {
		size_t condition_index;
		size_t feature_index;

		if (orlix_tcti_inventory_string_is_empty(leaves[index].condition_id))
			continue;
		condition_index = orlix_tcti_inventory_find_condition(
			conditions, condition_count, leaves[index].condition_id);
		if (condition_index == condition_count)
			continue;
		if (!conditions[condition_index].features)
			continue;
		for (feature_index = 0;
		     feature_index < conditions[condition_index].feature_count;
		     feature_index++) {
			if (orlix_tcti_inventory_string_is_empty(
				    conditions[condition_index].features[feature_index]) ||
			    strcmp(conditions[condition_index].features[feature_index],
				   feature))
				continue;
			found = true;
			if (!leaves[index].proved)
				*proved = false;
			break;
		}
	}
	return found;
}

int orlix_tcti_inventory_validate_runtime_projection(
	const struct orlix_tcti_inventory_leaf *leaves,
	size_t leaf_count,
	const struct orlix_tcti_inventory_condition *conditions,
	size_t condition_count,
	const struct orlix_tcti_inventory_capability *capabilities,
	size_t capability_count,
	const struct orlix_tcti_inventory_runtime_profile *profile,
	struct orlix_tcti_inventory_result *result)
{
	uint64_t known_hwcap = 0;
	uint64_t known_hwcap2 = 0;
	size_t index;

	if (!result)
		return -1;
	memset(result, 0, sizeof(*result));
	result->target_count = leaf_count;
	if (!leaves || (!conditions && condition_count) ||
	    (!capabilities && capability_count) || !profile) {
		orlix_tcti_inventory_record_error(
			result, ORLIX_TCTI_INVENTORY_ERROR_TARGET_COUNT, 0);
		return -1;
	}
	orlix_tcti_inventory_validate_conditions(conditions, condition_count, result);

	for (index = 0; index < capability_count; index++) {
		const struct orlix_tcti_inventory_capability *capability =
			&capabilities[index];
		uint64_t *known;
		uint64_t advertised;
		bool leaves_proved;
		bool has_leaves;

		if (orlix_tcti_inventory_string_is_empty(capability->feature)) {
			orlix_tcti_inventory_record_error(
				result,
				ORLIX_TCTI_INVENTORY_ERROR_EMPTY_CAPABILITY_FEATURE,
				index);
			continue;
		}
		if (capability->word > ORLIX_TCTI_INVENTORY_HWCAP2) {
			orlix_tcti_inventory_record_error(
				result,
				ORLIX_TCTI_INVENTORY_ERROR_INVALID_CAPABILITY_WORD,
				index);
			continue;
		}
		if (!capability->mask) {
			orlix_tcti_inventory_record_error(
				result, ORLIX_TCTI_INVENTORY_ERROR_ZERO_CAPABILITY_MASK,
				index);
			continue;
		}
		known = capability->word == ORLIX_TCTI_INVENTORY_HWCAP ?
			&known_hwcap : &known_hwcap2;
		if (*known & capability->mask)
			orlix_tcti_inventory_record_error(
				result,
				ORLIX_TCTI_INVENTORY_ERROR_DUPLICATE_CAPABILITY,
				index);
		*known |= capability->mask;

		advertised = orlix_tcti_inventory_profile_word(profile,
							 capability->word);
		if (!(advertised & capability->mask))
			continue;
		result->advertised_capabilities++;
		if ((advertised & capability->mask) != capability->mask) {
			orlix_tcti_inventory_record_error(
				result, ORLIX_TCTI_INVENTORY_ERROR_PARTIAL_CAPABILITY,
				index);
			continue;
		}
		has_leaves = orlix_tcti_inventory_feature_has_leaves(
			leaves, leaf_count, conditions, condition_count,
			capability->feature, &leaves_proved);
		if (!has_leaves) {
			orlix_tcti_inventory_record_error(
				result,
				ORLIX_TCTI_INVENTORY_ERROR_CAPABILITY_WITHOUT_LEAVES,
				index);
		} else if (!leaves_proved) {
			orlix_tcti_inventory_record_error(
				result,
				ORLIX_TCTI_INVENTORY_ERROR_ADVERTISED_CAPABILITY_WITHOUT_PROOF,
				index);
		} else {
			result->proved_advertised_capabilities++;
		}
	}

	if (profile->hwcap & ~known_hwcap)
		orlix_tcti_inventory_record_error(
			result,
			ORLIX_TCTI_INVENTORY_ERROR_UNKNOWN_ADVERTISED_CAPABILITY,
			ORLIX_TCTI_INVENTORY_HWCAP);
	if (profile->hwcap2 & ~known_hwcap2)
		orlix_tcti_inventory_record_error(
			result,
			ORLIX_TCTI_INVENTORY_ERROR_UNKNOWN_ADVERTISED_CAPABILITY,
			ORLIX_TCTI_INVENTORY_HWCAP2);

	return result->errors ? -1 : 0;
}
