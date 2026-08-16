// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/string.h>

#include "../active_execution_profile.h"
#include "../decode_aarch64.h"

struct orlix_tcti_active_execution_profile_artifact_entry {
	const char *feature;
	const char *state;
};

#define ORLIX_TCTI_A64_ACTIVE_EXECUTION_PROFILE_SOURCE(...)
#define ORLIX_TCTI_A64_ACTIVE_EXECUTION_PROFILE_SOURCE_MANIFEST(...)
#define ORLIX_TCTI_A64_ACTIVE_EXECUTION_PROFILE_COUNTS(...)
#define ORLIX_TCTI_A64_ACTIVE_EXECUTION_PROFILE_ENTRY(index, feature, state, manifest) \
	{ feature, #state },
static const struct orlix_tcti_active_execution_profile_artifact_entry
	orlix_tcti_active_execution_profile_artifact_entries[] = {
#include "../isa/target_active_execution_profile_artifact.def"
};
#undef ORLIX_TCTI_A64_ACTIVE_EXECUTION_PROFILE_ENTRY
#undef ORLIX_TCTI_A64_ACTIVE_EXECUTION_PROFILE_COUNTS
#undef ORLIX_TCTI_A64_ACTIVE_EXECUTION_PROFILE_SOURCE_MANIFEST
#undef ORLIX_TCTI_A64_ACTIVE_EXECUTION_PROFILE_SOURCE

static size_t orlix_tcti_active_execution_profile_artifact_matches(
	const char *feature, const char *state)
{
	size_t index;
	size_t matches = 0;

	for (index = 0;
	     index < ARRAY_SIZE(orlix_tcti_active_execution_profile_artifact_entries);
	     index++) {
		const struct orlix_tcti_active_execution_profile_artifact_entry *entry =
			&orlix_tcti_active_execution_profile_artifact_entries[index];

		if (!strcmp(entry->feature, feature) && !strcmp(entry->state, state))
			matches++;
	}
	return matches;
}

static void orlix_tcti_active_execution_profile_is_immutable_test(struct kunit *test)
{
	char mutable_name[] = "FEAT_SME";

	KUNIT_EXPECT_EQ(test,
		orlix_tcti_active_execution_profile_feature(mutable_name),
		ORLIX_TCTI_ACTIVE_EXECUTION_FEATURE_DISABLED);
	mutable_name[5] = 'X';
	KUNIT_EXPECT_EQ(test,
		orlix_tcti_active_execution_profile_feature("FEAT_SME"),
		ORLIX_TCTI_ACTIVE_EXECUTION_FEATURE_DISABLED);
	KUNIT_EXPECT_EQ(test,
		orlix_tcti_active_execution_profile_feature("FEAT_SVE"),
		ORLIX_TCTI_ACTIVE_EXECUTION_FEATURE_DISABLED);
	KUNIT_EXPECT_EQ(test,
		orlix_tcti_active_execution_profile_feature("FEAT_SVE_AES"),
		ORLIX_TCTI_ACTIVE_EXECUTION_FEATURE_DISABLED);
}

static void orlix_tcti_active_execution_profile_denies_invalid_test(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, orlix_tcti_active_execution_profile_feature(NULL),
		ORLIX_TCTI_ACTIVE_EXECUTION_FEATURE_INVALID);
	KUNIT_EXPECT_EQ(test, orlix_tcti_active_execution_profile_feature(""),
		ORLIX_TCTI_ACTIVE_EXECUTION_FEATURE_INVALID);
	KUNIT_EXPECT_EQ(test,
		orlix_tcti_active_execution_profile_feature("FEAT_NOT_REVIEWED"),
		ORLIX_TCTI_ACTIVE_EXECUTION_FEATURE_DISABLED);
}

static void orlix_tcti_active_execution_profile_covers_pending_conditions_test(struct kunit *test)
{
	static const char *const features[] = {
		"FEAT_SME", "FEAT_SME2", "FEAT_SME_B16B16", "FEAT_SME_F16F16",
		"FEAT_SME_F64F64", "FEAT_SME_F8F16", "FEAT_SME_F8F32",
		"FEAT_SME_FA64", "FEAT_SME_I16I64", "FEAT_SME_LUTv2",
		"FEAT_SME_MOP4", "FEAT_SME_TMOP",
		"FEAT_SVE", "FEAT_SVE2", "FEAT_SVE_AES", "FEAT_SVE_AES2",
		"FEAT_SVE_SHA3", "FEAT_SVE_SM4", "FEAT_SVE_BitPerm",
		"FEAT_SVE_B16B16", "FEAT_SVE_B16MM", "FEAT_SVE_BFSCALE",
		"FEAT_SVE_F16F32MM", "FEAT_SVE_PMULL128", "FEAT_FP8",
		"FEAT_FP8DOT2", "FEAT_FP8DOT4", "FEAT_FP8FMA",
	};
	size_t index;

	/* This reads the selected generated artifact directly. A lookup cannot
	 * establish closure because an absent row is also fail-closed DISABLED. */
	KUNIT_ASSERT_EQ(test, ARRAY_SIZE(features),
		ARRAY_SIZE(orlix_tcti_active_execution_profile_artifact_entries));
	for (index = 0; index < ARRAY_SIZE(features); index++)
		KUNIT_EXPECT_EQ(test, 1U,
			orlix_tcti_active_execution_profile_artifact_matches(
				features[index], "DISABLED"));
	for (index = 0; index < ARRAY_SIZE(features); index++)
		KUNIT_EXPECT_EQ(test,
			orlix_tcti_active_execution_profile_feature(features[index]),
			ORLIX_TCTI_ACTIVE_EXECUTION_FEATURE_DISABLED);
}

static void orlix_tcti_active_execution_profile_gates_decoded_extensions_test(struct kunit *test)
{
	struct orlix_tcti_decoded_instruction decoded = {
		.decode_class = ORLIX_TCTI_DECODE_SME_PSTATE_IMMEDIATE,
		.feature_identity = ORLIX_TCTI_DECODE_FEATURE_SME,
	};
	struct orlix_tcti_decoded_instruction before = decoded;

	KUNIT_EXPECT_FALSE(test,
		orlix_tcti_active_execution_profile_allows_decoded(&decoded));
	KUNIT_EXPECT_MEMEQ(test, &decoded, &before, sizeof(decoded));
	decoded.feature_identity = ORLIX_TCTI_DECODE_FEATURE_UNKNOWN;
	KUNIT_EXPECT_FALSE(test,
		orlix_tcti_active_execution_profile_allows_decoded(&decoded));
	decoded.feature_identity = ORLIX_TCTI_DECODE_FEATURE_SVE;
	KUNIT_EXPECT_FALSE(test,
		orlix_tcti_active_execution_profile_allows_decoded(&decoded));
	decoded.decode_class = ORLIX_TCTI_DECODE_SVE_PREDICATED_INTEGER_BINARY;
	decoded.feature_identity = ORLIX_TCTI_DECODE_FEATURE_SVE;
	KUNIT_EXPECT_FALSE(test,
		orlix_tcti_active_execution_profile_allows_decoded(&decoded));
	/* A feature-family class cannot fall back to the baseline identity. */
	decoded.feature_identity = ORLIX_TCTI_DECODE_FEATURE_BASELINE;
	KUNIT_EXPECT_FALSE(test,
		orlix_tcti_active_execution_profile_allows_decoded(&decoded));
	decoded.decode_class = ORLIX_TCTI_DECODE_ADD_SUB_IMMEDIATE;
	decoded.feature_identity = ORLIX_TCTI_DECODE_FEATURE_BASELINE;
	KUNIT_EXPECT_TRUE(test,
		orlix_tcti_active_execution_profile_allows_decoded(&decoded));
	decoded.feature_identity = ORLIX_TCTI_DECODE_FEATURE_UNKNOWN;
	KUNIT_EXPECT_FALSE(test,
		orlix_tcti_active_execution_profile_allows_decoded(&decoded));
	decoded.feature_identity = ORLIX_TCTI_DECODE_FEATURE_SME;
	KUNIT_EXPECT_FALSE(test,
		orlix_tcti_active_execution_profile_allows_decoded(&decoded));
	decoded.decode_class = ORLIX_TCTI_DECODE_UNSUPPORTED;
	decoded.feature_identity = ORLIX_TCTI_DECODE_FEATURE_BASELINE;
	KUNIT_EXPECT_FALSE(test,
		orlix_tcti_active_execution_profile_allows_decoded(&decoded));
}

static void orlix_tcti_active_execution_profile_gates_disabled_optional_leaves_test(
	struct kunit *test)
{
	struct orlix_tcti_decoded_instruction crc32 =
		orlix_tcti_decode_aarch64(0x1ac24020U);
	struct orlix_tcti_decoded_instruction lse128 =
		orlix_tcti_decode_aarch64(0x19201000U | (6U << 16) |
			(7U << 5) | 8U);

	KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DECODE_DATA_PROCESSING_2SOURCE,
		crc32.decode_class);
	KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DP2_CRC32, crc32.dp2_op);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_FEATURE_CRC32,
		crc32.feature_identity);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACTIVE_EXECUTION_FEATURE_DISABLED,
		orlix_tcti_active_execution_profile_feature("FEAT_CRC32"));
	KUNIT_EXPECT_FALSE(test,
		orlix_tcti_active_execution_profile_allows_decoded(&crc32));
	crc32.feature_identity = ORLIX_TCTI_DECODE_FEATURE_BASELINE;
	KUNIT_EXPECT_FALSE(test,
		orlix_tcti_active_execution_profile_allows_decoded(&crc32));

	KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DECODE_LSE_ATOMIC,
		lse128.decode_class);
	KUNIT_ASSERT_TRUE(test, lse128.lse128);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_FEATURE_LSE128,
		lse128.feature_identity);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACTIVE_EXECUTION_FEATURE_DISABLED,
		orlix_tcti_active_execution_profile_feature("FEAT_LSE128"));
	KUNIT_EXPECT_FALSE(test,
		orlix_tcti_active_execution_profile_allows_decoded(&lse128));
	lse128.feature_identity = ORLIX_TCTI_DECODE_FEATURE_BASELINE;
	KUNIT_EXPECT_FALSE(test,
		orlix_tcti_active_execution_profile_allows_decoded(&lse128));
}

static void orlix_tcti_active_execution_profile_rejects_optional_class_baseline_identity_test(
	struct kunit *test)
{
	struct orlix_tcti_decoded_instruction decoded = {
		.decode_class = ORLIX_TCTI_DECODE_SYSTEM_REGISTER,
		.feature_identity = ORLIX_TCTI_DECODE_FEATURE_BASELINE,
	};

	/* A broad class value must not authorize an optional leaf as BASELINE. */
	KUNIT_EXPECT_FALSE(test,
		orlix_tcti_active_execution_profile_allows_decoded(&decoded));
	decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
	KUNIT_EXPECT_FALSE(test,
		orlix_tcti_active_execution_profile_allows_decoded(&decoded));
	decoded.decode_class = ORLIX_TCTI_DECODE_FP_SCALAR_1SOURCE;
	KUNIT_EXPECT_FALSE(test,
		orlix_tcti_active_execution_profile_allows_decoded(&decoded));
	decoded.decode_class = ORLIX_TCTI_DECODE_MIN_MAX_IMMEDIATE;
	KUNIT_EXPECT_FALSE(test,
		orlix_tcti_active_execution_profile_allows_decoded(&decoded));
	decoded.decode_class = ORLIX_TCTI_DECODE_BARRIER;
	decoded.barrier_nxs = true;
	KUNIT_EXPECT_FALSE(test,
		orlix_tcti_active_execution_profile_allows_decoded(&decoded));
}

static void orlix_tcti_active_execution_profile_decoder_identity_test(struct kunit *test)
{
	struct orlix_tcti_decoded_instruction decoded;

	decoded = orlix_tcti_decode_aarch64(0xd503437fU);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_SME_PSTATE_IMMEDIATE,
		decoded.decode_class);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_FEATURE_SME,
		decoded.feature_identity);
	decoded = orlix_tcti_decode_aarch64(AARCH64_SVE_PREDICATED_INTEGER_BINARY);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_SVE_PREDICATED_INTEGER_BINARY,
		decoded.decode_class);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_FEATURE_SVE,
		decoded.feature_identity);
	decoded = orlix_tcti_decode_aarch64(0x91000000U);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_ADD_SUB_IMMEDIATE,
		decoded.decode_class);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_FEATURE_BASELINE,
		decoded.feature_identity);
}

static struct kunit_case orlix_tcti_active_execution_profile_cases[] = {
	KUNIT_CASE(orlix_tcti_active_execution_profile_is_immutable_test),
	KUNIT_CASE(orlix_tcti_active_execution_profile_denies_invalid_test),
	KUNIT_CASE(orlix_tcti_active_execution_profile_covers_pending_conditions_test),
	KUNIT_CASE(orlix_tcti_active_execution_profile_gates_decoded_extensions_test),
	KUNIT_CASE(orlix_tcti_active_execution_profile_gates_disabled_optional_leaves_test),
	KUNIT_CASE(orlix_tcti_active_execution_profile_rejects_optional_class_baseline_identity_test),
	KUNIT_CASE(orlix_tcti_active_execution_profile_decoder_identity_test),
	{}
};

static struct kunit_suite orlix_tcti_active_execution_profile_suite = {
	.name = "orlix_tcti_active_execution_profile",
	.test_cases = orlix_tcti_active_execution_profile_cases,
};
kunit_test_suite(orlix_tcti_active_execution_profile_suite);
