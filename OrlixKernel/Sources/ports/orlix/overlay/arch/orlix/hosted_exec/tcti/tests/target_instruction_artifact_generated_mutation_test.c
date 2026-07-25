/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_instruction_artifact.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int failures;

#define EXPECT(expression) do { \
	if (!(expression)) { \
		fprintf(stderr, "%s:%d: expectation failed: %s\n", \
			__FILE__, __LINE__, #expression); \
		failures++; \
	} \
} while (0)

struct mutable_artifact {
	struct tcti_target_instruction_artifact artifact;
	struct tcti_target_instruction_artifact_leaf *leaves;
	struct tcti_target_instruction_artifact_operand *operands;
	struct tcti_target_instruction_artifact_fixed_operand *fixed_operands;
	u8 *strings;
	u8 *conditions;
};

static void destroy(struct mutable_artifact *mutable)
{
	free(mutable->conditions);
	free(mutable->strings);
	free(mutable->operands);
	free(mutable->fixed_operands);
	free(mutable->leaves);
	memset(mutable, 0, sizeof(*mutable));
}

static int initialize(struct mutable_artifact *mutable)
{
	const struct tcti_target_instruction_artifact *source =
		tcti_target_instruction_artifact_canonical();

	memset(mutable, 0, sizeof(*mutable));
	if (source->leaf_count > SIZE_MAX / sizeof(*mutable->leaves) ||
	    source->operand_count > SIZE_MAX / sizeof(*mutable->operands) ||
	    source->fixed_operand_count > SIZE_MAX / sizeof(*mutable->fixed_operands))
		return -1;
	mutable->leaves = malloc(source->leaf_count * sizeof(*mutable->leaves));
	mutable->operands = malloc(source->operand_count *
				   sizeof(*mutable->operands));
	mutable->fixed_operands = malloc(source->fixed_operand_count *
					 sizeof(*mutable->fixed_operands));
	mutable->strings = malloc(source->string_pool_size);
	mutable->conditions = malloc(source->condition_pool_size);
	if (!mutable->leaves || (source->operand_count && !mutable->operands) ||
	    (source->fixed_operand_count && !mutable->fixed_operands) ||
	    !mutable->strings || !mutable->conditions) {
		destroy(mutable);
		return -1;
	}
	memcpy(mutable->leaves, source->leaves,
	       source->leaf_count * sizeof(*mutable->leaves));
	memcpy(mutable->operands, source->operands,
	       source->operand_count * sizeof(*mutable->operands));
	memcpy(mutable->fixed_operands, source->fixed_operands,
	       source->fixed_operand_count * sizeof(*mutable->fixed_operands));
	memcpy(mutable->strings, source->string_pool, source->string_pool_size);
	memcpy(mutable->conditions, source->condition_pool,
	       source->condition_pool_size);
	mutable->artifact = *source;
	mutable->artifact.leaves = mutable->leaves;
	mutable->artifact.operands = mutable->operands;
	mutable->artifact.fixed_operands = mutable->fixed_operands;
	mutable->artifact.string_pool = mutable->strings;
	mutable->artifact.condition_pool = mutable->conditions;
	return 0;
}

static void expect_error(const struct mutable_artifact *mutable,
	enum tcti_target_instruction_artifact_validation_error expected)
{
	struct tcti_target_instruction_artifact_validation_result result;

	EXPECT(tcti_target_instruction_artifact_validate(&mutable->artifact,
							&result) == -1);
	EXPECT(result.error == expected);
}

int main(void)
{
	struct mutable_artifact mutable;
	struct tcti_target_instruction_artifact_validation_result result;
	u32 original_u32;
	u8 original_u8;
	size_t index;

	if (initialize(&mutable)) {
		fputs("cannot copy generated instruction artifact\n", stderr);
		return EXIT_FAILURE;
	}
	EXPECT(tcti_target_instruction_artifact_validate(&mutable.artifact,
							&result) == 0);
	EXPECT(result.error == TCTI_TARGET_INSTRUCTION_ARTIFACT_VALID);

	mutable.artifact.version++;
	expect_error(&mutable,
		TCTI_TARGET_INSTRUCTION_ARTIFACT_VERSION_MISMATCH);
	mutable.artifact.version--;

	original_u32 = mutable.leaves[0].name_offset;
	mutable.leaves[0].name_offset = UINT32_MAX;
	expect_error(&mutable, TCTI_TARGET_INSTRUCTION_ARTIFACT_STRING_INVALID);
	mutable.leaves[0].name_offset = original_u32;

	original_u32 = mutable.leaves[0].condition_length;
	mutable.leaves[0].condition_length = 0;
	expect_error(&mutable,
		TCTI_TARGET_INSTRUCTION_ARTIFACT_CONDITION_INVALID);
	mutable.leaves[0].condition_length = original_u32;
	if (mutable.leaves[0].condition_length <= 9U) {
		fputs("generated instruction condition is too short\n", stderr);
		failures++;
	} else {
		original_u8 = mutable.conditions[
			mutable.leaves[0].condition_offset + 5U];
		mutable.conditions[mutable.leaves[0].condition_offset + 5U] = 0xff;
		expect_error(&mutable,
			TCTI_TARGET_INSTRUCTION_ARTIFACT_CONDITION_INVALID);
		mutable.conditions[mutable.leaves[0].condition_offset + 5U] =
			original_u8;

		original_u8 = mutable.conditions[
			mutable.leaves[0].condition_offset + 6U];
		mutable.conditions[mutable.leaves[0].condition_offset + 6U] = 0xff;
		expect_error(&mutable,
			TCTI_TARGET_INSTRUCTION_ARTIFACT_CONDITION_INVALID);
		mutable.conditions[mutable.leaves[0].condition_offset + 6U] =
			original_u8;
	}

	original_u32 = mutable.leaves[0].operand_first;
	mutable.leaves[0].operand_first = UINT32_MAX;
	expect_error(&mutable, TCTI_TARGET_INSTRUCTION_ARTIFACT_SPAN_INVALID);
	mutable.leaves[0].operand_first = original_u32;

	if (!mutable.artifact.operand_count) {
		fputs("generated instruction artifact has no operands\n", stderr);
		failures++;
	} else {
		original_u32 = mutable.operands[0].leaf_index;
		mutable.operands[0].leaf_index = UINT32_MAX;
		expect_error(&mutable,
			TCTI_TARGET_INSTRUCTION_ARTIFACT_OPERAND_INVALID);
		mutable.operands[0].leaf_index = original_u32;

		original_u8 = mutable.operands[0].width;
		mutable.operands[0].width = 0;
		expect_error(&mutable,
			TCTI_TARGET_INSTRUCTION_ARTIFACT_OPERAND_INVALID);
		mutable.operands[0].width = original_u8;

		for (index = 0; index < mutable.artifact.leaf_count; index++)
			if (mutable.leaves[index].operand_count >= 2U)
				break;
		if (index == mutable.artifact.leaf_count) {
			fputs("generated instruction artifact has no sibling operands\n",
			      stderr);
			failures++;
		} else {
			const struct tcti_target_instruction_artifact_leaf *leaf =
				&mutable.leaves[index];
			struct tcti_target_instruction_artifact_operand *first =
				&mutable.operands[leaf->operand_first];
			struct tcti_target_instruction_artifact_operand *second =
				&mutable.operands[leaf->operand_first + 1U];
			struct tcti_target_instruction_artifact_operand saved = *second;

			second->start = first->start;
			second->width = first->width;
			second->variable_mask = first->variable_mask;
			expect_error(&mutable,
				TCTI_TARGET_INSTRUCTION_ARTIFACT_OPERAND_INVALID);
			*second = saved;
			second->name_offset = first->name_offset;
			expect_error(&mutable,
				TCTI_TARGET_INSTRUCTION_ARTIFACT_OPERAND_INVALID);
			*second = saved;
		}

		if (mutable.artifact.string_pool[
			mutable.leaves[0].name_offset + 1U] == '\0') {
			fputs("generated first instruction name is too short\n", stderr);
			failures++;
		} else {
			original_u32 = mutable.leaves[0].name_offset;
			mutable.leaves[0].name_offset++;
			expect_error(&mutable,
				TCTI_TARGET_INSTRUCTION_ARTIFACT_STRING_INVALID);
			mutable.leaves[0].name_offset = original_u32;
		}

		{
			u32 owner = mutable.operands[0].leaf_index;

			for (index = 0; index < mutable.artifact.leaf_count; index++)
				if (mutable.leaves[index].condition_offset !=
					    mutable.leaves[owner].condition_offset ||
				    mutable.leaves[index].condition_length !=
					    mutable.leaves[owner].condition_length)
					break;
			if (index == mutable.artifact.leaf_count) {
				fputs("generated artifact has no distinct conditions\n", stderr);
				failures++;
			} else {
				u32 saved_offset = mutable.operands[0].condition_offset;
				u32 saved_length = mutable.operands[0].condition_length;

				mutable.operands[0].condition_offset =
					mutable.leaves[index].condition_offset;
				mutable.operands[0].condition_length =
					mutable.leaves[index].condition_length;
				expect_error(&mutable,
					TCTI_TARGET_INSTRUCTION_ARTIFACT_OPERAND_INVALID);
				mutable.operands[0].condition_offset = saved_offset;
				mutable.operands[0].condition_length = saved_length;
			}
		}
	}
	if (!mutable.artifact.fixed_operand_count) {
		fputs("generated instruction artifact has no fixed operands\n", stderr);
		failures++;
	} else {
		original_u32 = mutable.fixed_operands[0].fixed_mask;
		mutable.fixed_operands[0].fixed_mask = 0;
		expect_error(&mutable,
			TCTI_TARGET_INSTRUCTION_ARTIFACT_FIXED_OPERAND_INVALID);
		mutable.fixed_operands[0].fixed_mask = original_u32;

		original_u32 = mutable.fixed_operands[0].source_length;
		mutable.fixed_operands[0].source_length = 0;
		expect_error(&mutable,
			TCTI_TARGET_INSTRUCTION_ARTIFACT_FIXED_OPERAND_INVALID);
		mutable.fixed_operands[0].source_length = original_u32;

		original_u32 = mutable.fixed_operands[0].leaf_index;
		mutable.fixed_operands[0].leaf_index = UINT32_MAX;
		expect_error(&mutable,
			TCTI_TARGET_INSTRUCTION_ARTIFACT_FIXED_OPERAND_INVALID);
		mutable.fixed_operands[0].leaf_index = original_u32;

		original_u32 = mutable.fixed_operands[0].fixed_value;
		mutable.fixed_operands[0].fixed_value ^= mutable.fixed_operands[0].fixed_mask;
		expect_error(&mutable,
			TCTI_TARGET_INSTRUCTION_ARTIFACT_FIXED_OPERAND_INVALID);
		mutable.fixed_operands[0].fixed_value = original_u32;

		original_u8 = mutable.fixed_operands[0].start;
		mutable.fixed_operands[0].start++;
		expect_error(&mutable,
			TCTI_TARGET_INSTRUCTION_ARTIFACT_FIXED_OPERAND_INVALID);
		mutable.fixed_operands[0].start = original_u8;

		original_u8 = mutable.fixed_operands[0].width;
		mutable.fixed_operands[0].width = 0;
		expect_error(&mutable,
			TCTI_TARGET_INSTRUCTION_ARTIFACT_FIXED_OPERAND_INVALID);
		mutable.fixed_operands[0].width = original_u8;

		original_u32 = mutable.fixed_operands[0].condition_offset;
		mutable.fixed_operands[0].condition_offset = UINT32_MAX;
		expect_error(&mutable,
			TCTI_TARGET_INSTRUCTION_ARTIFACT_FIXED_OPERAND_INVALID);
		mutable.fixed_operands[0].condition_offset = original_u32;

		original_u32 = mutable.fixed_operands[0].condition_length;
		mutable.fixed_operands[0].condition_length = 0;
		expect_error(&mutable,
			TCTI_TARGET_INSTRUCTION_ARTIFACT_FIXED_OPERAND_INVALID);
		mutable.fixed_operands[0].condition_length = original_u32;

		original_u32 = mutable.fixed_operands[0].source_offset;
		mutable.fixed_operands[0].source_offset++;
		expect_error(&mutable,
			TCTI_TARGET_INSTRUCTION_ARTIFACT_FIXED_OPERAND_INVALID);
		mutable.fixed_operands[0].source_offset = original_u32;

		original_u32 = mutable.fixed_operands[0].source_identity_offset;
		mutable.fixed_operands[0].source_identity_offset = UINT32_MAX;
		expect_error(&mutable,
			TCTI_TARGET_INSTRUCTION_ARTIFACT_FIXED_OPERAND_INVALID);
		mutable.fixed_operands[0].source_identity_offset = original_u32;

		original_u32 = mutable.leaves[0].fixed_operand_first;
		mutable.leaves[0].fixed_operand_first = UINT32_MAX;
		expect_error(&mutable, TCTI_TARGET_INSTRUCTION_ARTIFACT_SPAN_INVALID);
		mutable.leaves[0].fixed_operand_first = original_u32;

		original_u32 = mutable.leaves[0].fixed_operand_count;
		mutable.leaves[0].fixed_operand_count = UINT32_MAX;
		expect_error(&mutable, TCTI_TARGET_INSTRUCTION_ARTIFACT_SPAN_INVALID);
		mutable.leaves[0].fixed_operand_count = original_u32;

		for (index = 0; index < mutable.artifact.leaf_count; index++)
			if (mutable.leaves[index].fixed_operand_count >= 2U)
				break;
		if (index != mutable.artifact.leaf_count) {
			const struct tcti_target_instruction_artifact_leaf *leaf =
				&mutable.leaves[index];
			struct tcti_target_instruction_artifact_fixed_operand *first =
				&mutable.fixed_operands[leaf->fixed_operand_first];
			struct tcti_target_instruction_artifact_fixed_operand *second =
				&mutable.fixed_operands[leaf->fixed_operand_first + 1U];
			struct tcti_target_instruction_artifact_fixed_operand saved = *second;

			second->name_offset = first->name_offset;
			expect_error(&mutable,
				TCTI_TARGET_INSTRUCTION_ARTIFACT_FIXED_OPERAND_INVALID);
			*second = saved;
			second->start = first->start;
			second->width = first->width;
			second->fixed_mask = first->fixed_mask;
			second->fixed_value = first->fixed_value;
			expect_error(&mutable,
				TCTI_TARGET_INSTRUCTION_ARTIFACT_FIXED_OPERAND_INVALID);
			*second = saved;
		}
		for (index = 0; index < mutable.artifact.leaf_count; index++)
			if (mutable.leaves[index].fixed_operand_count &&
			    mutable.leaves[index].operand_count)
				break;
		if (index != mutable.artifact.leaf_count) {
			const struct tcti_target_instruction_artifact_leaf *leaf =
				&mutable.leaves[index];
			struct tcti_target_instruction_artifact_fixed_operand *fixed =
				&mutable.fixed_operands[leaf->fixed_operand_first];
			struct tcti_target_instruction_artifact_operand *variable =
				&mutable.operands[leaf->operand_first];
			struct tcti_target_instruction_artifact_operand saved = *variable;

			/* A variable mask may never occupy a leaf-fixed field. */
			variable->start = fixed->start;
			variable->width = fixed->width;
			variable->variable_mask = fixed->fixed_mask;
			expect_error(&mutable,
				TCTI_TARGET_INSTRUCTION_ARTIFACT_OPERAND_INVALID);
			*variable = saved;
		}
	}
	destroy(&mutable);
	if (failures) {
		fprintf(stderr,
			"%u generated instruction artifact mutation test(s) failed\n",
			failures);
		return EXIT_FAILURE;
	}
	puts("generated target instruction artifact mutations: 1 passed");
	return EXIT_SUCCESS;
}
