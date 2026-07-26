// SPDX-License-Identifier: GPL-2.0-only
#include "inventory_contract.h"

#include <stdio.h>

#define EXPECT_EQ(expected, actual)                                            \
	do {                                                                    \
		if ((expected) != (actual)) {                                      \
			fprintf(stderr, "%s:%d: expected %s, got %s\n", __FILE__,    \
				__LINE__, #expected, #actual);                            \
			return -1;                                                  \
		}                                                               \
	} while (0)

#define TEST_LEAF_CAPACITY (ORLIX_TCTI_INVENTORY_AUTHORITATIVE_TARGET_LEAF_COUNT + 1)

static const char *const base_features[] = { "FEAT_BASE" };
static const char *const alternative_features[] = { "FEAT_ALT" };
static const struct orlix_tcti_inventory_condition conditions[] = {
	{ "base", "IsFeatureImplemented(FEAT_BASE)", base_features, 1 },
	{ "alternative", "IsFeatureImplemented(FEAT_ALT)",
	  alternative_features, 1 },
};
static const struct orlix_tcti_inventory_capability capabilities[] = {
	{ "FEAT_BASE", ORLIX_TCTI_INVENTORY_HWCAP, 1U << 0 },
	{ "FEAT_ALT", ORLIX_TCTI_INVENTORY_HWCAP2, 1U << 3 },
};
static struct orlix_tcti_inventory_leaf leaves[TEST_LEAF_CAPACITY];
static char names[TEST_LEAF_CAPACITY][20];

static void reset_leaves(size_t count)
{
	size_t index;

	for (index = 0; index < count; index++) {
		snprintf(names[index], sizeof(names[index]), "LEAF_%04zu", index);
		leaves[index] = (struct orlix_tcti_inventory_leaf) {
			.name = names[index],
			.classification = ORLIX_TCTI_INVENTORY_REQUIRED_EL0,
			.condition_id = "base",
			.proof = "leaf_kunit",
			.proved = true,
		};
	}
}

static int validate_target(struct orlix_tcti_inventory_result *result)
{
	return orlix_tcti_inventory_validate_target(
		leaves, ORLIX_TCTI_INVENTORY_AUTHORITATIVE_TARGET_LEAF_COUNT,
		conditions, 2, result);
}

static int target_count_is_fixed_and_runtime_independent(void)
{
	const struct orlix_tcti_inventory_runtime_profile profile = { .hwcap = 1U << 0 };
	struct orlix_tcti_inventory_result target_result;
	struct orlix_tcti_inventory_result runtime_result;

	reset_leaves(ORLIX_TCTI_INVENTORY_AUTHORITATIVE_TARGET_LEAF_COUNT);
	EXPECT_EQ(0, validate_target(&target_result));
	EXPECT_EQ(0, orlix_tcti_inventory_validate_runtime_projection(
		leaves, ORLIX_TCTI_INVENTORY_AUTHORITATIVE_TARGET_LEAF_COUNT,
		conditions, 2, capabilities, 2, &profile, &runtime_result));
	EXPECT_EQ(ORLIX_TCTI_INVENTORY_AUTHORITATIVE_TARGET_LEAF_COUNT,
		  target_result.target_count);
	EXPECT_EQ(1, runtime_result.advertised_capabilities);
	return 0;
}

static int wrong_target_counts_fail(void)
{
	struct orlix_tcti_inventory_result result;

	reset_leaves(ORLIX_TCTI_INVENTORY_AUTHORITATIVE_TARGET_LEAF_COUNT - 1);
	EXPECT_EQ(-1, orlix_tcti_inventory_validate_target(leaves,
		ORLIX_TCTI_INVENTORY_AUTHORITATIVE_TARGET_LEAF_COUNT - 1,
		conditions, 2, &result));
	EXPECT_EQ(ORLIX_TCTI_INVENTORY_ERROR_TARGET_COUNT, result.first_error);
	reset_leaves(ORLIX_TCTI_INVENTORY_AUTHORITATIVE_TARGET_LEAF_COUNT + 1);
	EXPECT_EQ(-1, orlix_tcti_inventory_validate_target(leaves,
		ORLIX_TCTI_INVENTORY_AUTHORITATIVE_TARGET_LEAF_COUNT + 1,
		conditions, 2, &result));
	EXPECT_EQ(ORLIX_TCTI_INVENTORY_ERROR_TARGET_COUNT, result.first_error);
	return 0;
}

static int unproved_unadvertised_leaf_fails(void)
{
	struct orlix_tcti_inventory_result result;

	reset_leaves(ORLIX_TCTI_INVENTORY_AUTHORITATIVE_TARGET_LEAF_COUNT);
	leaves[0].condition_id = "alternative";
	leaves[0].proved = false;
	EXPECT_EQ(-1, validate_target(&result));
	EXPECT_EQ(ORLIX_TCTI_INVENTORY_ERROR_UNPROVED_LEAF, result.first_error);
	return 0;
}

static int null_metadata_fails_safely(void)
{
	struct orlix_tcti_inventory_result result;

	EXPECT_EQ(-1, orlix_tcti_inventory_validate_target(NULL,
		ORLIX_TCTI_INVENTORY_AUTHORITATIVE_TARGET_LEAF_COUNT,
		conditions, 2, &result));
	EXPECT_EQ(ORLIX_TCTI_INVENTORY_ERROR_TARGET_COUNT, result.first_error);
	reset_leaves(ORLIX_TCTI_INVENTORY_AUTHORITATIVE_TARGET_LEAF_COUNT);
	leaves[0].proof = NULL;
	EXPECT_EQ(-1, validate_target(&result));
	EXPECT_EQ(ORLIX_TCTI_INVENTORY_ERROR_MISSING_PROOF, result.first_error);
	EXPECT_EQ(-1, validate_target(NULL));
	return 0;
}

static int alias_self_cycle_chain_and_missing_fail(void)
{
	struct orlix_tcti_inventory_result result;

	reset_leaves(ORLIX_TCTI_INVENTORY_AUTHORITATIVE_TARGET_LEAF_COUNT);
	leaves[0].classification = ORLIX_TCTI_INVENTORY_ALIAS_OR_DUPLICATE;
	leaves[0].alias_target = leaves[0].name;
	EXPECT_EQ(-1, validate_target(&result));
	EXPECT_EQ(ORLIX_TCTI_INVENTORY_ERROR_SELF_ALIAS, result.first_error);

	reset_leaves(ORLIX_TCTI_INVENTORY_AUTHORITATIVE_TARGET_LEAF_COUNT);
	leaves[0].classification = ORLIX_TCTI_INVENTORY_ALIAS_OR_DUPLICATE;
	leaves[1].classification = ORLIX_TCTI_INVENTORY_ALIAS_OR_DUPLICATE;
	leaves[0].alias_target = leaves[1].name;
	leaves[1].alias_target = leaves[0].name;
	EXPECT_EQ(-1, validate_target(&result));
	EXPECT_EQ(ORLIX_TCTI_INVENTORY_ERROR_ALIAS_TARGET_NOT_CANONICAL,
		  result.first_error);

	reset_leaves(ORLIX_TCTI_INVENTORY_AUTHORITATIVE_TARGET_LEAF_COUNT);
	leaves[0].classification = ORLIX_TCTI_INVENTORY_ALIAS_OR_DUPLICATE;
	leaves[1].classification = ORLIX_TCTI_INVENTORY_ALIAS_OR_DUPLICATE;
	leaves[0].alias_target = leaves[1].name;
	leaves[1].alias_target = leaves[2].name;
	EXPECT_EQ(-1, validate_target(&result));
	EXPECT_EQ(ORLIX_TCTI_INVENTORY_ERROR_ALIAS_TARGET_NOT_CANONICAL,
		  result.first_error);

	reset_leaves(ORLIX_TCTI_INVENTORY_AUTHORITATIVE_TARGET_LEAF_COUNT);
	leaves[0].classification = ORLIX_TCTI_INVENTORY_ALIAS_OR_DUPLICATE;
	leaves[0].alias_target = "MISSING";
	EXPECT_EQ(-1, validate_target(&result));
	EXPECT_EQ(ORLIX_TCTI_INVENTORY_ERROR_UNKNOWN_ALIAS_TARGET, result.first_error);
	return 0;
}

static int aliases_require_proof_compatible_and_proved_canonical(void)
{
	struct orlix_tcti_inventory_result result;

	reset_leaves(ORLIX_TCTI_INVENTORY_AUTHORITATIVE_TARGET_LEAF_COUNT);
	leaves[0].classification = ORLIX_TCTI_INVENTORY_ALIAS_OR_DUPLICATE;
	leaves[0].alias_target = leaves[1].name;
	leaves[0].proof = NULL;
	EXPECT_EQ(-1, validate_target(&result));
	EXPECT_EQ(ORLIX_TCTI_INVENTORY_ERROR_MISSING_PROOF, result.first_error);

	reset_leaves(ORLIX_TCTI_INVENTORY_AUTHORITATIVE_TARGET_LEAF_COUNT);
	leaves[0].classification = ORLIX_TCTI_INVENTORY_ALIAS_OR_DUPLICATE;
	leaves[0].alias_target = leaves[1].name;
	leaves[1].proved = false;
	EXPECT_EQ(-1, validate_target(&result));
	EXPECT_EQ(ORLIX_TCTI_INVENTORY_ERROR_UNPROVED_LEAF, result.first_error);

	reset_leaves(ORLIX_TCTI_INVENTORY_AUTHORITATIVE_TARGET_LEAF_COUNT);
	leaves[0].classification = ORLIX_TCTI_INVENTORY_ALIAS_OR_DUPLICATE;
	leaves[0].alias_target = leaves[1].name;
	leaves[0].condition_id = "alternative";
	EXPECT_EQ(-1, validate_target(&result));
	EXPECT_EQ(ORLIX_TCTI_INVENTORY_ERROR_ALIAS_CONDITION_MISMATCH,
		  result.first_error);
	return 0;
}

static int canonical_alias_passes(void)
{
	struct orlix_tcti_inventory_result result;

	reset_leaves(ORLIX_TCTI_INVENTORY_AUTHORITATIVE_TARGET_LEAF_COUNT);
	leaves[0].classification = ORLIX_TCTI_INVENTORY_ALIAS_OR_DUPLICATE;
	leaves[0].alias_target = leaves[1].name;
	EXPECT_EQ(0, validate_target(&result));
	EXPECT_EQ(1, result.alias_count);
	return 0;
}

static int empty_capability_feature_fails_safely(void)
{
	struct orlix_tcti_inventory_capability invalid = {
		.word = ORLIX_TCTI_INVENTORY_HWCAP,
		.mask = 1U,
	};
	const struct orlix_tcti_inventory_runtime_profile profile = { .hwcap = 1U };
	struct orlix_tcti_inventory_result result;

	reset_leaves(ORLIX_TCTI_INVENTORY_AUTHORITATIVE_TARGET_LEAF_COUNT);
	EXPECT_EQ(-1, orlix_tcti_inventory_validate_runtime_projection(leaves,
		ORLIX_TCTI_INVENTORY_AUTHORITATIVE_TARGET_LEAF_COUNT, conditions, 2,
		&invalid, 1, &profile, &result));
	EXPECT_EQ(ORLIX_TCTI_INVENTORY_ERROR_EMPTY_CAPABILITY_FEATURE,
		  result.first_error);
	return 0;
}

static int null_condition_metadata_fails_safely(void)
{
	const struct orlix_tcti_inventory_condition invalid[] = {
		{ NULL, "IsFeatureImplemented(FEAT_BASE)", base_features, 1 },
	};
	struct orlix_tcti_inventory_result result;

	reset_leaves(ORLIX_TCTI_INVENTORY_AUTHORITATIVE_TARGET_LEAF_COUNT);
	leaves[0].condition_id = NULL;
	EXPECT_EQ(-1, orlix_tcti_inventory_validate_target(leaves,
		ORLIX_TCTI_INVENTORY_AUTHORITATIVE_TARGET_LEAF_COUNT, invalid, 1,
		&result));
	EXPECT_EQ(ORLIX_TCTI_INVENTORY_ERROR_EMPTY_CONDITION_ID, result.first_error);
	return 0;
}

int main(void)
{
	static const struct {
		const char *name;
		int (*run)(void);
	} tests[] = {
		{ "target_count_is_fixed_and_runtime_independent", target_count_is_fixed_and_runtime_independent },
		{ "wrong_target_counts_fail", wrong_target_counts_fail },
		{ "unproved_unadvertised_leaf_fails", unproved_unadvertised_leaf_fails },
		{ "null_metadata_fails_safely", null_metadata_fails_safely },
		{ "alias_self_cycle_chain_and_missing_fail", alias_self_cycle_chain_and_missing_fail },
		{ "aliases_require_proof_compatible_and_proved_canonical", aliases_require_proof_compatible_and_proved_canonical },
		{ "canonical_alias_passes", canonical_alias_passes },
		{ "empty_capability_feature_fails_safely", empty_capability_feature_fails_safely },
		{ "null_condition_metadata_fails_safely", null_condition_metadata_fails_safely },
	};
	size_t index;

	for (index = 0; index < sizeof(tests) / sizeof(tests[0]); index++) {
		if (tests[index].run()) {
			fprintf(stderr, "FAIL %s\n", tests[index].name);
			return 1;
		}
		printf("PASS %s\n", tests[index].name);
	}
	return 0;
}
