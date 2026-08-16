/* SPDX-License-Identifier: GPL-2.0-only */
#include "active_execution_profile.h"
#include "decode_aarch64.h"

#include <linux/kernel.h>
#include <linux/string.h>

struct active_execution_profile_entry { const char *feature; unsigned int state; const char *manifest; };
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
		if (strcmp(feature_name, active_execution_profile[i].feature))
			continue;
		matches++;
		state = active_execution_profile[i].state;
	}
	return matches == 1 ? state :
		ORLIX_TCTI_ACTIVE_EXECUTION_FEATURE_DISABLED;
}

static bool orlix_tcti_active_execution_profile_is_baseline_class(
	enum orlix_tcti_decode_class decode_class)
{
	switch (decode_class) {
	case ORLIX_TCTI_DECODE_SVC:
	case ORLIX_TCTI_DECODE_BRK:
	case ORLIX_TCTI_DECODE_HLT:
	case ORLIX_TCTI_DECODE_UNDEFINED:
	case ORLIX_TCTI_DECODE_HINT:
	case ORLIX_TCTI_DECODE_BARRIER:
	case ORLIX_TCTI_DECODE_CACHE_MAINTENANCE:
	case ORLIX_TCTI_DECODE_MIN_MAX_IMMEDIATE:
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
	case ORLIX_TCTI_DECODE_LOAD_LITERAL:
	case ORLIX_TCTI_DECODE_LOAD_STORE_PAIR:
	case ORLIX_TCTI_DECODE_LOAD_STORE_UNSIGNED_IMMEDIATE:
	case ORLIX_TCTI_DECODE_LOAD_STORE_SIGNED_IMMEDIATE:
	case ORLIX_TCTI_DECODE_LOAD_STORE_REGISTER_OFFSET:
	case ORLIX_TCTI_DECODE_LOGICAL_SHIFTED_REGISTER:
	case ORLIX_TCTI_DECODE_LOGICAL_IMMEDIATE:
	case ORLIX_TCTI_DECODE_BITFIELD:
	case ORLIX_TCTI_DECODE_EXTRACT:
	case ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE:
	case ORLIX_TCTI_DECODE_DATA_PROCESSING_2SOURCE:
	case ORLIX_TCTI_DECODE_MULTIPLY_ADD_SUB:
	case ORLIX_TCTI_DECODE_MOVE_WIDE_IMMEDIATE:
	case ORLIX_TCTI_DECODE_SYSTEM_REGISTER:
	case ORLIX_TCTI_DECODE_EXCLUSIVE_MONITOR_CLEAR:
	case ORLIX_TCTI_DECODE_LOAD_STORE_EXCLUSIVE:
	case ORLIX_TCTI_DECODE_LSE_ATOMIC:
	case ORLIX_TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE:
	case ORLIX_TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE:
	case ORLIX_TCTI_DECODE_SIMD_TABLE_LOOKUP:
	case ORLIX_TCTI_DECODE_SIMD_VECTOR_LOGICAL:
	case ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC:
	case ORLIX_TCTI_DECODE_SIMD_VECTOR_COMPARE:
	case ORLIX_TCTI_DECODE_SIMD_VECTOR_REDUCTION:
	case ORLIX_TCTI_DECODE_SIMD_LOAD_STORE_SINGLE_STRUCTURE:
	case ORLIX_TCTI_DECODE_SIMD_LOAD_REPLICATE:
	case ORLIX_TCTI_DECODE_SIMD_LOAD_STORE_MULTIPLE_STRUCTURE:
	case ORLIX_TCTI_DECODE_FP_SCALAR_IMMEDIATE:
	case ORLIX_TCTI_DECODE_FP_SCALAR_MOVE:
	case ORLIX_TCTI_DECODE_FP_SCALAR_1SOURCE:
	case ORLIX_TCTI_DECODE_FP_SCALAR_2SOURCE:
	case ORLIX_TCTI_DECODE_FP_SCALAR_3SOURCE:
	case ORLIX_TCTI_DECODE_FP_SCALAR_COMPARE:
	case ORLIX_TCTI_DECODE_FP_CONDITIONAL_SELECT:
	case ORLIX_TCTI_DECODE_FP_INT_CONVERT:
		return true;
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
	case ORLIX_TCTI_DECODE_SME_PSTATE_IMMEDIATE:
		return ORLIX_TCTI_DECODE_FEATURE_SME;
	case ORLIX_TCTI_DECODE_SVE_PREDICATED_INTEGER_BINARY:
		return ORLIX_TCTI_DECODE_FEATURE_SVE;
	default:
		return orlix_tcti_active_execution_profile_is_baseline_class(
			decoded->decode_class) ? ORLIX_TCTI_DECODE_FEATURE_BASELINE :
			ORLIX_TCTI_DECODE_FEATURE_UNKNOWN;
	}
}

static const char *orlix_tcti_active_execution_profile_feature_name(
	enum orlix_tcti_decoded_feature_identity identity)
{
	switch (identity) {
	case ORLIX_TCTI_DECODE_FEATURE_SME:
		return "FEAT_SME";
	case ORLIX_TCTI_DECODE_FEATURE_SVE:
		return "FEAT_SVE";
	default:
		return NULL;
	}
}

bool orlix_tcti_active_execution_profile_allows_decoded(
	const struct orlix_tcti_decoded_instruction *decoded)
{
	enum orlix_tcti_decoded_feature_identity expected;
	const char *feature;

	if (!decoded)
		return false;
	expected = orlix_tcti_active_execution_profile_expected_identity(decoded);
	if (expected == ORLIX_TCTI_DECODE_FEATURE_UNKNOWN ||
		decoded->feature_identity != expected)
		return false;
	if (expected == ORLIX_TCTI_DECODE_FEATURE_BASELINE)
		return true;
	feature = orlix_tcti_active_execution_profile_feature_name(expected);
	return feature &&
		orlix_tcti_active_execution_profile_feature(feature) ==
			ORLIX_TCTI_ACTIVE_EXECUTION_FEATURE_ENABLED;
}
