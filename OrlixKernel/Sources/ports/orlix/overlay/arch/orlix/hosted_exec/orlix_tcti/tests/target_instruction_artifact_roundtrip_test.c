/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_instruction_artifact.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int expect_fixed_operand(
	const struct orlix_tcti_target_instruction_artifact *artifact, u32 leaf_index,
	const char *name, u8 start, u8 width, u32 fixed_value)
{
	const struct orlix_tcti_target_instruction_artifact_leaf *leaf;
	size_t index;
	char identity[96];
	int identity_length;

	if (leaf_index >= artifact->leaf_count)
		return -1;
	leaf = &artifact->leaves[leaf_index];
	for (index = leaf->fixed_operand_first;
	     index < (size_t)leaf->fixed_operand_first + leaf->fixed_operand_count;
	     index++) {
		const struct orlix_tcti_target_instruction_artifact_fixed_operand *operand =
			&artifact->fixed_operands[index];

		if (strcmp((const char *)artifact->string_pool + operand->name_offset,
			name))
			continue;
		identity_length = snprintf(identity, sizeof(identity),
			"a1ad2c6538a47cd97d8762791ac5af88bce1d5f6aff096c9b77aef853e76acfe:%u:%u",
			operand->source_offset, operand->source_length);
		return operand->leaf_index == leaf_index && operand->start == start &&
			operand->width == width && operand->fixed_mask ==
			((UINT32_C(3) << start)) && operand->fixed_value == fixed_value &&
			operand->source_length && identity_length > 0 &&
			(size_t)identity_length < sizeof(identity) &&
			operand->source_identity_offset < artifact->string_pool_size &&
			!strcmp((const char *)artifact->string_pool +
				operand->source_identity_offset, identity) ? 0 : -1;
	}
	return -1;
}

int main(void)
{
	const struct orlix_tcti_target_instruction_artifact *artifact =
		orlix_tcti_target_instruction_artifact_canonical();
	struct orlix_tcti_target_instruction_artifact_validation_result result;

	if (orlix_tcti_target_instruction_artifact_validate(
			artifact, &result)) {
		fprintf(stderr,
			"generated instruction artifact rejected: %s leaf=%u operand=%u\n",
			orlix_tcti_target_instruction_artifact_validation_error_name(result.error),
			result.leaf_index, result.operand_index);
		return EXIT_FAILURE;
	}
	if (expect_fixed_operand(artifact, 203U, "opc", 22U, 2U, 0U) ||
	    expect_fixed_operand(artifact, 204U, "opc", 22U, 2U, 0x00400000U) ||
	    expect_fixed_operand(artifact, 205U, "opc", 22U, 2U, 0x00800000U) ||
	    expect_fixed_operand(artifact, 2169U, "op21", 29U, 2U, 0U) ||
	    expect_fixed_operand(artifact, 2170U, "op21", 29U, 2U, 0U)) {
		fputs("generated instruction artifact lost fixed-field provenance\n",
			stderr);
		return EXIT_FAILURE;
	}
	puts("generated target instruction artifact: 1 passed");
	return EXIT_SUCCESS;
}
