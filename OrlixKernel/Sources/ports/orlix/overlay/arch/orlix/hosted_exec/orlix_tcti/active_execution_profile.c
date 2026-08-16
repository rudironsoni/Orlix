/* SPDX-License-Identifier: GPL-2.0-only */
#include "active_execution_profile.h"
#include "decode_aarch64.h"

#include <linux/kernel.h>
#include <linux/string.h>

struct active_execution_profile_entry {
	const char *feature;
	unsigned int state;
	const char *manifest;
};
#define ORLIX_TCTI_A64_ACTIVE_EXECUTION_PROFILE_SOURCE(...)
#define ORLIX_TCTI_A64_ACTIVE_EXECUTION_PROFILE_SOURCE_MANIFEST(...)
#define ORLIX_TCTI_A64_ACTIVE_EXECUTION_PROFILE_COUNTS(...)
#define ORLIX_TCTI_A64_ACTIVE_EXECUTION_PROFILE_ENTRY(index, feature, state, manifest) \
	[index] = { feature, ORLIX_TCTI_ACTIVE_EXECUTION_FEATURE_##state, manifest },
static const struct active_execution_profile_entry active_execution_profile[] = {
#include "isa/target_active_execution_profile_artifact.def"
};
#undef ORLIX_TCTI_A64_ACTIVE_EXECUTION_PROFILE_ENTRY
#undef ORLIX_TCTI_A64_ACTIVE_EXECUTION_PROFILE_COUNTS
#undef ORLIX_TCTI_A64_ACTIVE_EXECUTION_PROFILE_SOURCE_MANIFEST
#undef ORLIX_TCTI_A64_ACTIVE_EXECUTION_PROFILE_SOURCE

/* The decoder currently carries family identity, not a feature expression
 * for each decoded leaf. A family is therefore admitted only when every
 * profile row that can affect that family is present exactly once and
 * ENABLED. Missing, duplicate, or DISABLED rows remain fail-closed through
 * orlix_tcti_active_execution_profile_feature(). */
static const char *const active_execution_profile_sme_features[] = {
	"FEAT_SME",
	"FEAT_SME2",
	"FEAT_SME_B16B16",
	"FEAT_SME_F16F16",
	"FEAT_SME_F64F64",
	"FEAT_SME_F8F16",
	"FEAT_SME_F8F32",
	"FEAT_SME_FA64",
	"FEAT_SME_I16I64",
	"FEAT_SME_LUTv2",
	"FEAT_SME_MOP4",
	"FEAT_SME_TMOP",
};

static const char *const active_execution_profile_sve_features[] = {
	"FEAT_SVE",
	"FEAT_SVE2",
	"FEAT_SVE_AES",
	"FEAT_SVE_AES2",
	"FEAT_SVE_SHA3",
	"FEAT_SVE_SM4",
	"FEAT_SVE_BitPerm",
	"FEAT_SVE_B16B16",
	"FEAT_SVE_B16MM",
	"FEAT_SVE_BFSCALE",
	"FEAT_SVE_F16F32MM",
	"FEAT_SVE_PMULL128",
};

static const char *const active_execution_profile_crc32_features[] = {
	"FEAT_CRC32",
};

static const char *const active_execution_profile_lse128_features[] = {
	"FEAT_LSE128",
};

enum orlix_tcti_active_execution_feature_state
orlix_tcti_active_execution_profile_feature(const char *feature_name)
{
	enum orlix_tcti_active_execution_feature_state state =
		ORLIX_TCTI_ACTIVE_EXECUTION_FEATURE_DISABLED;
	size_t i;
	size_t matches = 0;

	if (!feature_name || !*feature_name)
		return ORLIX_TCTI_ACTIVE_EXECUTION_FEATURE_INVALID;
	for (i = 0; i < ARRAY_SIZE(active_execution_profile); i++) {
		if (!active_execution_profile[i].feature ||
			strcmp(feature_name, active_execution_profile[i].feature))
			continue;
		matches++;
		state = active_execution_profile[i].state;
	}
	return matches == 1 ? state :
		ORLIX_TCTI_ACTIVE_EXECUTION_FEATURE_DISABLED;
}

/* Only a complete, unconditionally BASELINE inventory can use the baseline
 * identity. Mixed classes inspect their decoded leaf, and known optional
 * leaves use an exact identity below or remain UNKNOWN until one exists. */
static bool orlix_tcti_active_execution_profile_is_baseline_decoded(
	const struct orlix_tcti_decoded_instruction *decoded)
{
	if (!decoded)
		return false;

	switch (decoded->decode_class) {
	case ORLIX_TCTI_DECODE_SVC:
	case ORLIX_TCTI_DECODE_BRK:
	case ORLIX_TCTI_DECODE_HLT:
	case ORLIX_TCTI_DECODE_UNDEFINED:
	case ORLIX_TCTI_DECODE_CACHE_MAINTENANCE:
	case ORLIX_TCTI_DECODE_ADD_SUB_IMMEDIATE:
	case ORLIX_TCTI_DECODE_ADD_SUB_SHIFTED_REGISTER:
	case ORLIX_TCTI_DECODE_ADD_SUB_EXTENDED_REGISTER:
	case ORLIX_TCTI_DECODE_ADD_SUB_WITH_CARRY:
	case ORLIX_TCTI_DECODE_PC_RELATIVE_ADDRESS:
	case ORLIX_TCTI_DECODE_UNCONDITIONAL_BRANCH_IMMEDIATE:
	case ORLIX_TCTI_DECODE_UNCONDITIONAL_BRANCH_REGISTER:
	case ORLIX_TCTI_DECODE_COMPARE_BRANCH_IMMEDIATE:
	case ORLIX_TCTI_DECODE_COMPARE_BRANCH_EXTENSION:
	case ORLIX_TCTI_DECODE_TEST_BRANCH_IMMEDIATE:
	case ORLIX_TCTI_DECODE_CONDITIONAL_BRANCH_IMMEDIATE:
	case ORLIX_TCTI_DECODE_CONDITIONAL_COMPARE:
	case ORLIX_TCTI_DECODE_CONDITIONAL_SELECT:
	case ORLIX_TCTI_DECODE_LOGICAL_SHIFTED_REGISTER:
	case ORLIX_TCTI_DECODE_LOGICAL_IMMEDIATE:
	case ORLIX_TCTI_DECODE_BITFIELD:
	case ORLIX_TCTI_DECODE_EXTRACT:
	case ORLIX_TCTI_DECODE_MULTIPLY_ADD_SUB:
	case ORLIX_TCTI_DECODE_MOVE_WIDE_IMMEDIATE:
	case ORLIX_TCTI_DECODE_EXCLUSIVE_MONITOR_CLEAR:
	case ORLIX_TCTI_DECODE_LOAD_STORE_EXCLUSIVE:
		return true;
	case ORLIX_TCTI_DECODE_HINT:
		/* NOP, YIELD, WFE, WFI, SEV, and SEVL are baseline hints. */
		return decoded->hint_imm <= 5;
	case ORLIX_TCTI_DECODE_BARRIER:
		return !decoded->barrier_nxs;
	case ORLIX_TCTI_DECODE_LOAD_LITERAL:
	case ORLIX_TCTI_DECODE_LOAD_STORE_PAIR:
	case ORLIX_TCTI_DECODE_LOAD_STORE_UNSIGNED_IMMEDIATE:
	case ORLIX_TCTI_DECODE_LOAD_STORE_SIGNED_IMMEDIATE:
	case ORLIX_TCTI_DECODE_LOAD_STORE_REGISTER_OFFSET:
		return !decoded->simd_fp;
	case ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE:
		switch (decoded->dp1_op) {
		case ORLIX_TCTI_DP1_CLZ:
		case ORLIX_TCTI_DP1_RBIT:
		case ORLIX_TCTI_DP1_REV:
		case ORLIX_TCTI_DP1_REV16:
		case ORLIX_TCTI_DP1_REV32:
		case ORLIX_TCTI_DP1_CLS:
			return true;
		default:
			return false;
		}
	case ORLIX_TCTI_DECODE_DATA_PROCESSING_2SOURCE:
		switch (decoded->dp2_op) {
		case ORLIX_TCTI_DP2_UDIV:
		case ORLIX_TCTI_DP2_SDIV:
		case ORLIX_TCTI_DP2_LSLV:
		case ORLIX_TCTI_DP2_LSRV:
		case ORLIX_TCTI_DP2_ASRV:
		case ORLIX_TCTI_DP2_RORV:
			return true;
		default:
			return false;
		}
	default:
		return false;
	}
}

static enum orlix_tcti_decoded_feature_identity
orlix_tcti_active_execution_profile_expected_identity(
	const struct orlix_tcti_decoded_instruction *decoded)
{
	if (!decoded)
		return ORLIX_TCTI_DECODE_FEATURE_UNKNOWN;

	switch (decoded->decode_class) {
	case ORLIX_TCTI_DECODE_MIN_MAX_IMMEDIATE:
		return ORLIX_TCTI_DECODE_FEATURE_UNKNOWN;
	case ORLIX_TCTI_DECODE_BARRIER:
		return orlix_tcti_active_execution_profile_is_baseline_decoded(decoded) ?
			ORLIX_TCTI_DECODE_FEATURE_BASELINE :
			ORLIX_TCTI_DECODE_FEATURE_UNKNOWN;
	case ORLIX_TCTI_DECODE_LOAD_LITERAL:
	case ORLIX_TCTI_DECODE_LOAD_STORE_PAIR:
	case ORLIX_TCTI_DECODE_LOAD_STORE_UNSIGNED_IMMEDIATE:
	case ORLIX_TCTI_DECODE_LOAD_STORE_SIGNED_IMMEDIATE:
	case ORLIX_TCTI_DECODE_LOAD_STORE_REGISTER_OFFSET:
		return decoded->simd_fp ? ORLIX_TCTI_DECODE_FEATURE_UNKNOWN :
			ORLIX_TCTI_DECODE_FEATURE_BASELINE;
	case ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE:
		break;
	case ORLIX_TCTI_DECODE_DATA_PROCESSING_2SOURCE:
		if (decoded->dp2_op == ORLIX_TCTI_DP2_CRC32 ||
			decoded->dp2_op == ORLIX_TCTI_DP2_CRC32C)
			return ORLIX_TCTI_DECODE_FEATURE_CRC32;
		break;
	case ORLIX_TCTI_DECODE_LSE_ATOMIC:
		if (decoded->lse128)
			return ORLIX_TCTI_DECODE_FEATURE_LSE128;
		return ORLIX_TCTI_DECODE_FEATURE_UNKNOWN;
	case ORLIX_TCTI_DECODE_SME_PSTATE_IMMEDIATE:
		return ORLIX_TCTI_DECODE_FEATURE_SME;
	case ORLIX_TCTI_DECODE_SVE_PREDICATED_INTEGER_BINARY:
		return ORLIX_TCTI_DECODE_FEATURE_SVE;
	default:
		break;
	}
	return orlix_tcti_active_execution_profile_is_baseline_decoded(decoded) ?
		ORLIX_TCTI_DECODE_FEATURE_BASELINE : ORLIX_TCTI_DECODE_FEATURE_UNKNOWN;
}

static bool orlix_tcti_active_execution_profile_family_enabled(
	const char *const *features, size_t feature_count)
{
	size_t i;

	if (!features || !feature_count)
		return false;
	for (i = 0; i < feature_count; i++) {
		if (orlix_tcti_active_execution_profile_feature(features[i]) !=
			ORLIX_TCTI_ACTIVE_EXECUTION_FEATURE_ENABLED)
			return false;
	}
	return true;
}

static bool orlix_tcti_active_execution_profile_identity_enabled(
	enum orlix_tcti_decoded_feature_identity identity)
{
	switch (identity) {
	case ORLIX_TCTI_DECODE_FEATURE_CRC32:
		return orlix_tcti_active_execution_profile_family_enabled(
			active_execution_profile_crc32_features,
			ARRAY_SIZE(active_execution_profile_crc32_features));
	case ORLIX_TCTI_DECODE_FEATURE_LSE128:
		return orlix_tcti_active_execution_profile_family_enabled(
			active_execution_profile_lse128_features,
			ARRAY_SIZE(active_execution_profile_lse128_features));
	case ORLIX_TCTI_DECODE_FEATURE_SME:
		return orlix_tcti_active_execution_profile_family_enabled(
			active_execution_profile_sme_features,
			ARRAY_SIZE(active_execution_profile_sme_features));
	case ORLIX_TCTI_DECODE_FEATURE_SVE:
		return orlix_tcti_active_execution_profile_family_enabled(
			active_execution_profile_sve_features,
			ARRAY_SIZE(active_execution_profile_sve_features));
	default:
		return false;
	}
}

bool orlix_tcti_active_execution_profile_allows_decoded(
	const struct orlix_tcti_decoded_instruction *decoded)
{
	enum orlix_tcti_decoded_feature_identity expected;

	if (!decoded)
		return false;
	expected = orlix_tcti_active_execution_profile_expected_identity(decoded);
	if (expected == ORLIX_TCTI_DECODE_FEATURE_UNKNOWN ||
		decoded->feature_identity != expected)
		return false;
	if (expected == ORLIX_TCTI_DECODE_FEATURE_BASELINE)
		return true;
	return orlix_tcti_active_execution_profile_identity_enabled(expected);
}
