/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_instruction_artifact.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
	struct tcti_target_instruction_artifact_validation_result result;

	if (tcti_target_instruction_artifact_validate(
			tcti_target_instruction_artifact_canonical(), &result)) {
		fprintf(stderr,
			"generated instruction artifact rejected: %s leaf=%u operand=%u\n",
			tcti_target_instruction_artifact_validation_error_name(result.error),
			result.leaf_index, result.operand_index);
		return EXIT_FAILURE;
	}
	puts("generated target instruction artifact: 1 passed");
	return EXIT_SUCCESS;
}
