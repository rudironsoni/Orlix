/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_runtime_feature_condition_artifact_generator.h"

#include "target_feature_model.h"
#include "target_inventory_import.h"

#include <string.h>

#define ORLIX_TCTI_AARCHMRS_INSTRUCTIONS_SHA256 \
	"a1ad2c6538a47cd97d8762791ac5af88bce1d5f6aff096c9b77aef853e76acfe"
#define ORLIX_TCTI_AARCHMRS_FEATURES_SHA256 \
	"633259000ffd3da32900bd0c0c1beae4a9eea7095c278f74d62a00c846b41187"

int orlix_tcti_runtime_feature_condition_artifact_emit(const char *instructions,
	size_t instructions_length, const char *features, size_t features_length,
	FILE *output)
{
	struct orlix_tcti_target_inventory inventory = { 0 };
	struct orlix_tcti_feature_model model = { 0 };
	struct orlix_tcti_target_import_error instruction_error = { 0 };
	struct orlix_tcti_feature_error feature_error = { 0 };
	size_t index;
	int result = -1;

	if (!instructions || !features || !output ||
	    orlix_tcti_target_inventory_import(instructions, instructions_length,
						     &inventory, &instruction_error) ||
	    orlix_tcti_target_feature_model_import(features, features_length,
						      &model, &feature_error) ||
	    inventory.leaf_count != ORLIX_TCTI_A64_TARGET_LEAF_COUNT ||
	    model.parameter_count != ORLIX_TCTI_FEATURE_PARAMETER_COUNT)
		goto out;
	if (fprintf(output, "/* SPDX-License-Identifier: BSD-3-Clause */\n"
		"/* Generated from pinned AARCHMRS inputs. Do not edit. */\n"
		"ORLIX_TCTI_A64_RUNTIME_FEATURE_CONDITION_SOURCE(\"%s\", \"%s\", %uU, %uU)\n"
		"ORLIX_TCTI_A64_RUNTIME_FEATURE_CONDITION_CANDIDATE_COUNT(%uU)\n",
		ORLIX_TCTI_AARCHMRS_INSTRUCTIONS_SHA256,
		ORLIX_TCTI_AARCHMRS_FEATURES_SHA256,
		ORLIX_TCTI_A64_TARGET_LEAF_COUNT, ORLIX_TCTI_FEATURE_PARAMETER_COUNT,
		ORLIX_TCTI_A64_TARGET_LEAF_COUNT) < 0)
		goto out;
	for (index = 0; index < inventory.leaf_count; index++) {
		if (fprintf(output,
			"ORLIX_TCTI_A64_RUNTIME_FEATURE_CONDITION_CANDIDATE(%zuU)\n",
			index) < 0)
			goto out;
	}
	result = ferror(output) ? -1 : 0;
out:
	orlix_tcti_target_feature_model_destroy(&model);
	orlix_tcti_target_inventory_destroy(&inventory);
	return result;
}
