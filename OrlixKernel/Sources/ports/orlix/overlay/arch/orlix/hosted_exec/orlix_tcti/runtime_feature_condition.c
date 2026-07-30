/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/mutex.h>

#include "runtime_feature_condition.h"
#include "target_feature_artifact.h"
#include "target_instruction_artifact.h"

#define ORLIX_TCTI_RUNTIME_FEATURE_CONDITION_NO_ORDINAL ((u32)~0U)

#define ORLIX_TCTI_A64_RUNTIME_FEATURE_CONDITION_SOURCE(instruction_sha, feature_sha, leaf_count, feature_count) \
	static const char production_instruction_sha[] = instruction_sha; \
	static const char production_feature_sha[] = feature_sha;
#define ORLIX_TCTI_A64_RUNTIME_FEATURE_CONDITION_CANDIDATE_COUNT(count) \
	static const u32 production_candidate_count = count;
#define ORLIX_TCTI_A64_RUNTIME_FEATURE_CONDITION_CANDIDATE(ordinal)
#include "isa/target_runtime_feature_condition_artifact.def"
#undef ORLIX_TCTI_A64_RUNTIME_FEATURE_CONDITION_SOURCE
#undef ORLIX_TCTI_A64_RUNTIME_FEATURE_CONDITION_CANDIDATE_COUNT
#undef ORLIX_TCTI_A64_RUNTIME_FEATURE_CONDITION_CANDIDATE

#define ORLIX_TCTI_A64_RUNTIME_FEATURE_CONDITION_SOURCE(...)
#define ORLIX_TCTI_A64_RUNTIME_FEATURE_CONDITION_CANDIDATE_COUNT(...)
#define ORLIX_TCTI_A64_RUNTIME_FEATURE_CONDITION_CANDIDATE(ordinal) ordinal,
static const u16 production_candidates[] = {
#include "isa/target_runtime_feature_condition_artifact.def"
};
#undef ORLIX_TCTI_A64_RUNTIME_FEATURE_CONDITION_SOURCE
#undef ORLIX_TCTI_A64_RUNTIME_FEATURE_CONDITION_CANDIDATE_COUNT
#undef ORLIX_TCTI_A64_RUNTIME_FEATURE_CONDITION_CANDIDATE

static orlix_tcti_feature_artifact_u8 production_feature_state[
	ORLIX_TCTI_FEATURE_ARTIFACT_NODE_COUNT];
static struct orlix_tcti_feature_artifact_frame production_feature_frames[
	ORLIX_TCTI_FEATURE_ARTIFACT_NODE_COUNT];
static orlix_tcti_feature_artifact_u8 production_feature_child_coverage[
	ORLIX_TCTI_FEATURE_ARTIFACT_CHILD_COUNT];
static DEFINE_MUTEX(production_feature_validation_lock);

int orlix_tcti_runtime_feature_condition_validate_artifacts(
	const struct orlix_tcti_target_instruction_artifact *instructions,
	const struct orlix_tcti_feature_artifact *features)
{
	struct orlix_tcti_target_instruction_artifact_validation_result instruction;
	struct orlix_tcti_feature_artifact_scratch scratch = {
		.state = production_feature_state,
		.state_count = ARRAY_SIZE(production_feature_state),
		.frames = production_feature_frames,
		.frame_count = ARRAY_SIZE(production_feature_frames),
		.child_coverage = production_feature_child_coverage,
		.child_coverage_count = ARRAY_SIZE(production_feature_child_coverage),
	};

	if (!instructions || !features || !instructions->source_sha256 ||
	    !features->source.sha256 ||
	    production_candidate_count != ARRAY_SIZE(production_candidates) ||
	    orlix_tcti_target_instruction_artifact_validate(instructions, &instruction))
		return -EINVAL;
	mutex_lock(&production_feature_validation_lock);
	if (orlix_tcti_feature_artifact_validate(features, &scratch, NULL) !=
	    ORLIX_TCTI_FEATURE_ARTIFACT_VALID) {
		mutex_unlock(&production_feature_validation_lock);
		return -EINVAL;
	}
	mutex_unlock(&production_feature_validation_lock);
	if (strcmp(production_instruction_sha, instructions->source_sha256) ||
	    strcmp(production_feature_sha, features->source.sha256))
		return -EINVAL;
	return 0;
}

static int production_profile_feature(void *context, const char *hex,
				      size_t offset, size_t length, u8 *enabled)
{
	(void)context;
	(void)offset;
	if (!hex || !enabled || !length)
		return -EINVAL;
	/* ADR 0029: #155 has no production feature projection. */
	return -ENOENT;
}

static const struct orlix_tcti_feature_domain_tcnd_environment complete_environment = {
	.context = (void *)0,
	.feature = production_profile_feature,
};

const struct orlix_tcti_el0_feature_profile *
orlix_tcti_production_el0_execution_profile(void)
{
	static struct orlix_tcti_el0_feature_profile profile = {
		.identity = "orlix-tcti-production-el0-unprojected-v1",
		.environment = &complete_environment,
	};

	return &profile;
}

struct runtime_environment {
	const struct orlix_tcti_feature_domain_tcnd_environment *profile;
	struct orlix_tcti_target_instruction_operand_assignment assignment;
};

static int runtime_profile_feature(void *context, const char *hex,
				   size_t offset, size_t length, u8 *enabled)
{
	struct runtime_environment *environment = context;

	if (!environment || !environment->profile || !environment->profile->feature)
		return -EINVAL;
	return environment->profile->feature(environment->profile->context, hex, offset,
					     length, enabled);
}

static int runtime_operand(void *context, const char *hex,
			   size_t offset, size_t length, const char **value,
			   size_t *value_length)
{
	struct runtime_environment *environment = context;

	if (!environment)
		return -EINVAL;
	return orlix_tcti_target_instruction_operand_assignment(&environment->assignment,
							 hex, offset, length, value, value_length);
}

static char *condition_hex(const struct orlix_tcti_target_instruction_artifact *artifact,
			   const struct orlix_tcti_target_instruction_artifact_leaf *leaf)
{
	static const char digits[] = "0123456789abcdef";
	char *hex;
	size_t index;

	if (!artifact || !leaf || !leaf->condition_length ||
	    leaf->condition_offset > artifact->condition_pool_size ||
	    leaf->condition_length > artifact->condition_pool_size - leaf->condition_offset ||
	    leaf->condition_length > (SIZE_MAX - 1U) / 2U)
		return NULL;
	hex = kmalloc(leaf->condition_length * 2U + 1U, GFP_KERNEL);
	if (!hex)
		return NULL;
	for (index = 0; index < leaf->condition_length; index++) {
		u8 value = artifact->condition_pool[leaf->condition_offset + index];

		hex[index * 2U] = digits[value >> 4];
		hex[index * 2U + 1U] = digits[value & 0xfU];
	}
	hex[leaf->condition_length * 2U] = '\0';
	return hex;
}

int orlix_tcti_runtime_feature_condition_authorize(
	u32 instruction, struct orlix_tcti_decoded_instruction *decoded,
	const struct orlix_tcti_el0_feature_profile *profile,
	struct orlix_tcti_runtime_feature_condition_result *result)
{
	const struct orlix_tcti_target_instruction_artifact *instructions;
	const struct orlix_tcti_feature_artifact *features;
	u32 selected = ORLIX_TCTI_RUNTIME_FEATURE_CONDITION_NO_ORDINAL;
	size_t ordinal;

	if (!decoded || !profile || !profile->identity || !profile->environment || !result)
		return -EINVAL;
	*result = (struct orlix_tcti_runtime_feature_condition_result) {
		.status = ORLIX_TCTI_RUNTIME_FEATURE_CONDITION_INVALID_ARTIFACT,
		.source_ordinal = ORLIX_TCTI_RUNTIME_FEATURE_CONDITION_NO_ORDINAL,
	};
	decoded->source_ordinal_valid = false;
	instructions = orlix_tcti_target_instruction_artifact_canonical();
	features = orlix_tcti_feature_artifact_canonical();
	if (orlix_tcti_runtime_feature_condition_validate_artifacts(instructions,
		features))
		return 0;
	for (ordinal = 0; ordinal < production_candidate_count; ordinal++) {
		u32 candidate_ordinal = production_candidates[ordinal];
		const struct orlix_tcti_target_instruction_artifact_leaf *leaf =
			candidate_ordinal < instructions->leaf_count ?
			&instructions->leaves[candidate_ordinal] : NULL;
		struct runtime_environment runtime_environment = {
			.profile = profile->environment,
			.assignment = {
				.artifact = instructions,
				.leaf_index = candidate_ordinal,
				.instruction = instruction,
			},
		};
		struct orlix_tcti_feature_domain_tcnd_environment environment = {
			.context = &runtime_environment,
			.feature = runtime_profile_feature,
			.operand = runtime_operand,
		};
		char *hex;
		u8 satisfied;

		if (!leaf) {
			result->status = ORLIX_TCTI_RUNTIME_FEATURE_CONDITION_INVALID_ARTIFACT;
			return 0;
		}
		if ((instruction & leaf->encoding_mask) != leaf->encoding_pattern)
			continue;
		result->matching_candidates++;
		hex = condition_hex(instructions, leaf);
		if (!hex) {
			result->status = ORLIX_TCTI_RUNTIME_FEATURE_CONDITION_INVALID_ARTIFACT;
			return 0;
		}
		if (orlix_tcti_feature_domain_evaluate_tcnd(features, hex, &environment,
						     &satisfied, &result->diagnostic)) {
			kfree(hex);
			result->status = ORLIX_TCTI_RUNTIME_FEATURE_CONDITION_EVALUATION_ERROR;
			return 0;
		}
		kfree(hex);
		if (!satisfied)
			continue;
		if (selected != ORLIX_TCTI_RUNTIME_FEATURE_CONDITION_NO_ORDINAL) {
			result->status = ORLIX_TCTI_RUNTIME_FEATURE_CONDITION_AMBIGUOUS;
			return 0;
		}
		selected = candidate_ordinal;
	}
	if (!result->matching_candidates)
		result->status = ORLIX_TCTI_RUNTIME_FEATURE_CONDITION_NO_CANDIDATE;
	else if (selected == ORLIX_TCTI_RUNTIME_FEATURE_CONDITION_NO_ORDINAL)
		result->status = ORLIX_TCTI_RUNTIME_FEATURE_CONDITION_FALSE;
	else {
		result->status = ORLIX_TCTI_RUNTIME_FEATURE_CONDITION_ALLOWED;
		result->source_ordinal = selected;
		decoded->source_ordinal = selected;
		decoded->source_ordinal_valid = true;
	}
	return 0;
}
