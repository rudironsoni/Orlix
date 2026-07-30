/* SPDX-License-Identifier: GPL-2.0-only */
#include <kunit/test.h>

#include "../runtime_feature_condition.h"
#include "../target_feature_artifact.h"
#include "../target_instruction_artifact.h"

struct runtime_feature_test_profile {
	u8 enabled;
};

static int runtime_feature_test_value(void *context, const char *hex,
	 size_t offset, size_t length, u8 *enabled)
{
	struct runtime_feature_test_profile *profile = context;

	if (!profile || !hex || !length || !enabled)
		return -EINVAL;
	*enabled = profile->enabled;
	return 0;
}

static void runtime_feature_condition_cssc_profiles(struct kunit *test)
{
	struct runtime_feature_test_profile enabled = { .enabled = 1U };
	struct runtime_feature_test_profile disabled = { .enabled = 0U };
	struct orlix_tcti_feature_domain_tcnd_environment enabled_environment = {
		.context = &enabled, .feature = runtime_feature_test_value,
	};
	struct orlix_tcti_feature_domain_tcnd_environment disabled_environment = {
		.context = &disabled, .feature = runtime_feature_test_value,
	};
	struct orlix_tcti_el0_feature_profile enabled_profile = {
		.identity = "kunit-cssc-on", .environment = &enabled_environment,
	};
	struct orlix_tcti_el0_feature_profile disabled_profile = {
		.identity = "kunit-cssc-off", .environment = &disabled_environment,
	};
	struct orlix_tcti_decoded_instruction decoded = { 0 };
	struct orlix_tcti_runtime_feature_condition_result result;

	KUNIT_ASSERT_EQ(test, orlix_tcti_runtime_feature_condition_authorize(
		0x11c00000U, &decoded, &enabled_profile, &result), 0);
	KUNIT_EXPECT_EQ(test, result.status,
		ORLIX_TCTI_RUNTIME_FEATURE_CONDITION_ALLOWED);
	KUNIT_EXPECT_TRUE(test, decoded.source_ordinal_valid);
	KUNIT_ASSERT_EQ(test, orlix_tcti_runtime_feature_condition_authorize(
		0x11c00000U, &decoded, &disabled_profile, &result), 0);
	KUNIT_EXPECT_EQ(test, result.status,
		ORLIX_TCTI_RUNTIME_FEATURE_CONDITION_FALSE);
	KUNIT_EXPECT_FALSE(test, decoded.source_ordinal_valid);
}

static void runtime_feature_condition_production_is_fail_closed(struct kunit *test)
{
	struct orlix_tcti_decoded_instruction decoded = { 0 };
	struct orlix_tcti_runtime_feature_condition_result result;

	KUNIT_ASSERT_EQ(test, orlix_tcti_runtime_feature_condition_authorize(
		0x11c00000U, &decoded,
		orlix_tcti_production_el0_execution_profile(), &result), 0);
	KUNIT_EXPECT_EQ(test, result.status,
		ORLIX_TCTI_RUNTIME_FEATURE_CONDITION_EVALUATION_ERROR);
	KUNIT_EXPECT_FALSE(test, decoded.source_ordinal_valid);
}

static void runtime_feature_condition_rejects_feature_provenance_drift(
	struct kunit *test)
{
	const struct orlix_tcti_target_instruction_artifact *instructions =
		orlix_tcti_target_instruction_artifact_canonical();
	const struct orlix_tcti_feature_artifact *canonical =
		orlix_tcti_feature_artifact_canonical();
	struct orlix_tcti_feature_artifact malformed = *canonical;

	malformed.source.sha256 = "0000000000000000000000000000000000000000000000000000000000000000";
	KUNIT_EXPECT_EQ(test, orlix_tcti_runtime_feature_condition_validate_artifacts(
		instructions, &malformed), -EINVAL);
}

static void runtime_feature_condition_rejects_null_artifact_identities(
	struct kunit *test)
{
	const struct orlix_tcti_target_instruction_artifact *instructions =
		orlix_tcti_target_instruction_artifact_canonical();
	const struct orlix_tcti_feature_artifact *features =
		orlix_tcti_feature_artifact_canonical();
	struct orlix_tcti_target_instruction_artifact malformed_instructions =
		*instructions;
	struct orlix_tcti_feature_artifact malformed_features = *features;

	malformed_instructions.source_sha256 = NULL;
	KUNIT_EXPECT_EQ(test, orlix_tcti_runtime_feature_condition_validate_artifacts(
		&malformed_instructions, features), -EINVAL);
	malformed_features.source.sha256 = NULL;
	KUNIT_EXPECT_EQ(test, orlix_tcti_runtime_feature_condition_validate_artifacts(
		instructions, &malformed_features), -EINVAL);
}

static void runtime_feature_condition_ordinal_3297_operand_predicate(
	struct kunit *test)
{
	const struct orlix_tcti_target_instruction_artifact *artifact =
		orlix_tcti_target_instruction_artifact_canonical();
	const struct orlix_tcti_target_instruction_artifact_leaf *leaf =
		&artifact->leaves[3297U];
	const struct orlix_tcti_target_instruction_artifact_operand *option =
		&artifact->operands[leaf->operand_first + 1U];
	struct runtime_feature_test_profile enabled = { .enabled = 1U };
	struct orlix_tcti_feature_domain_tcnd_environment environment = {
		.context = &enabled, .feature = runtime_feature_test_value,
	};
	struct orlix_tcti_el0_feature_profile profile = {
		.identity = "kunit-operand", .environment = &environment,
	};
	struct orlix_tcti_decoded_instruction decoded = { 0 };
	struct orlix_tcti_runtime_feature_condition_result result;
	u32 matching = leaf->encoding_pattern | (2U << option->start);
	u32 nonmatching = leaf->encoding_pattern | (3U << option->start);

	KUNIT_ASSERT_EQ(test, leaf->operand_count, (u32)5U);
	KUNIT_ASSERT_EQ(test, option->start, (u8)13U);
	KUNIT_ASSERT_EQ(test, option->width, (u8)3U);
	KUNIT_ASSERT_EQ(test, orlix_tcti_runtime_feature_condition_authorize(
		matching, &decoded, &profile, &result), 0);
	KUNIT_EXPECT_EQ(test, result.status,
		ORLIX_TCTI_RUNTIME_FEATURE_CONDITION_ALLOWED);
	KUNIT_EXPECT_EQ(test, result.source_ordinal, (u32)3297U);
	KUNIT_EXPECT_TRUE(test, decoded.source_ordinal_valid);
	KUNIT_ASSERT_EQ(test, orlix_tcti_runtime_feature_condition_authorize(
		nonmatching, &decoded, &profile, &result), 0);
	KUNIT_EXPECT_EQ(test, result.status,
		ORLIX_TCTI_RUNTIME_FEATURE_CONDITION_FALSE);
	KUNIT_EXPECT_FALSE(test, decoded.source_ordinal_valid);
}

static struct kunit_case runtime_feature_condition_cases[] = {
	KUNIT_CASE(runtime_feature_condition_cssc_profiles),
	KUNIT_CASE(runtime_feature_condition_production_is_fail_closed),
	KUNIT_CASE(runtime_feature_condition_rejects_feature_provenance_drift),
	KUNIT_CASE(runtime_feature_condition_rejects_null_artifact_identities),
	KUNIT_CASE(runtime_feature_condition_ordinal_3297_operand_predicate),
	{}
};

static struct kunit_suite runtime_feature_condition_suite = {
	.name = "orlix-tcti-runtime-feature-condition",
	.test_cases = runtime_feature_condition_cases,
};

kunit_test_suite(runtime_feature_condition_suite);
