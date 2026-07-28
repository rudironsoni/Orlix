// SPDX-License-Identifier: GPL-2.0-only
/* Fail-closed pinned target-ledger and feature-condition audit. */
#include "target_feature_model.h"
#include "target_feature_sat.h"
#include "target_feature_typed_ir.h"
#include "target_inventory_import.h"
#include "target_ledger.h"
#include "target_register_model.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define ORLIX_TCTI_AUDIT_INSTRUCTIONS_MAX_BYTES (128U * 1024U * 1024U)
#define ORLIX_TCTI_AUDIT_FEATURES_MAX_BYTES (8U * 1024U * 1024U)
#define ORLIX_TCTI_AUDIT_REGISTERS_MAX_BYTES (128U * 1024U * 1024U)

static char *read_file(const char *path, const char *label,
		       size_t maximum_length, size_t *length)
{
	FILE *file;
	char *data;
	long size;

	file = fopen(path, "rb");
	if (!file) {
		fprintf(stderr, "cannot read %s: %s\n", label, path);
		return NULL;
	}
	if (fseek(file, 0, SEEK_END) || (size = ftell(file)) < 0 ||
	    fseek(file, 0, SEEK_SET)) {
		fprintf(stderr, "cannot size %s: %s\n", label, path);
		goto fail;
	}
	if ((uintmax_t)size > (uintmax_t)maximum_length ||
	    (uintmax_t)size > (uintmax_t)(SIZE_MAX - 1)) {
		fprintf(stderr, "%s exceeds audit input limit of %zu bytes: %s\n",
			label, maximum_length, path);
		goto fail;
	}
	data = malloc((size_t)size + 1);
	if (!data || fread(data, 1, (size_t)size, file) != (size_t)size) {
		fprintf(stderr, "cannot load %s: %s\n", label, path);
		goto fail_data;
	}
	fclose(file);
	data[size] = '\0';
	*length = (size_t)size;
	return data;
fail_data:
	free(data);
fail:
	if (file)
		fclose(file);
	return NULL;
}

int main(int argc, char **argv)
{
	const struct orlix_tcti_target_ledger_source_row *source;
	const struct orlix_tcti_target_ledger_classification_row *classification;
	const struct orlix_tcti_target_proof_registry_entry *proof_registry;
	struct orlix_tcti_target_inventory target = { 0 };
	struct orlix_tcti_target_import_error import_error = { 0 };
	struct orlix_tcti_feature_model feature_model = { 0 };
	struct orlix_tcti_feature_error feature_error = { 0 };
	struct orlix_tcti_typed_expression typed_expression = { 0 };
	struct orlix_tcti_feature_typed_result typed_result = { 0 };
	struct orlix_tcti_register_model register_model = { 0 };
	struct orlix_tcti_register_model_error register_error = { 0 };
	struct orlix_tcti_target_feature_sat_audit feature_audit = { 0 };
	struct orlix_tcti_target_feature_sat_error feature_sat_error = { 0 };
	struct orlix_tcti_target_ledger_result ledger = { 0 };
	char *instructions_json = NULL;
	char *features_json = NULL;
	char *registers_json = NULL;
	size_t instructions_length = 0;
	size_t features_length = 0;
	size_t registers_length = 0;
	size_t source_count;
	size_t classification_count;
	size_t proof_registry_count;
	size_t applicable_count = 0;
	size_t impossible_count = 0;
	size_t unresolved_count = 0;
	size_t index;
	int status = EXIT_FAILURE;

	if (argc != 4) {
		fprintf(stderr,
			"usage: %s Instructions.json Features.json Registers.json\n",
			argv[0]);
		return EXIT_FAILURE;
	}
	instructions_json = read_file(argv[1], "Instructions.json",
				      ORLIX_TCTI_AUDIT_INSTRUCTIONS_MAX_BYTES,
				      &instructions_length);
	if (!instructions_json)
		goto out;
	features_json = read_file(argv[2], "Features.json",
				  ORLIX_TCTI_AUDIT_FEATURES_MAX_BYTES,
				  &features_length);
	if (!features_json)
		goto out;
	registers_json = read_file(argv[3], "Registers.json",
				   ORLIX_TCTI_AUDIT_REGISTERS_MAX_BYTES,
				   &registers_length);
	if (!registers_json)
		goto out;
	if (orlix_tcti_target_inventory_import(instructions_json, instructions_length,
					 &target, &import_error)) {
		fprintf(stderr, "Instructions.json:%zu: %s\n", import_error.offset,
			import_error.message[0] ? import_error.message : "target import failed");
		goto out;
	}
	if (orlix_tcti_target_feature_model_import(features_json, features_length,
					      &feature_model, &feature_error)) {
		fprintf(stderr, "Features.json:%zu: %s\n", feature_error.offset,
			feature_error.message[0] ? feature_error.message :
			"feature-model import failed");
		goto out;
	}
	if (orlix_tcti_register_model_import(registers_json, registers_length,
				       &register_model, &register_error)) {
		fprintf(stderr, "Registers.json:%zu: %s\n", register_error.offset,
			register_error.message[0] ? register_error.message :
			"register-model import failed");
		goto out;
	}
	if (orlix_tcti_feature_typed_lower(&feature_model, &typed_expression,
				     &typed_result) ||
	    typed_result.diagnostic_count ||
	    typed_result.parameter_count != feature_model.parameter_count ||
	    typed_result.constraint_count != feature_model.constraint_count) {
		if (typed_result.diagnostic_count) {
			const struct orlix_tcti_feature_typed_diagnostic *diagnostic =
				&typed_result.diagnostics[0];

			fprintf(stderr,
				"Features.json typed-IR audit failed: diagnostics=%zu "
				"first=%d kind=%d offset=%zu\n",
				typed_result.diagnostic_count, diagnostic->code,
				diagnostic->source_kind,
				diagnostic->provenance.offset);
		} else {
			fprintf(stderr,
				"Features.json typed-IR audit failed: "
				"parameters=%zu/%zu constraints=%zu/%zu\n",
				typed_result.parameter_count,
				feature_model.parameter_count,
				typed_result.constraint_count,
				feature_model.constraint_count);
		}
		goto out;
	}
	if (orlix_tcti_target_feature_sat_audit(
		&feature_model, &target,
		ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_BRANCH_LIMIT,
		ORLIX_TCTI_TARGET_FEATURE_SAT_DEFAULT_ALLOCATION_LIMIT, NULL,
		&feature_audit, &feature_sat_error) ||
	    feature_audit.leaf_count != ORLIX_TCTI_A64_TARGET_LEAF_COUNT) {
		fprintf(stderr,
			"target-feature SAT audit failed: code=%d leaf=%zu offset=%zu\n",
			feature_sat_error.code, feature_sat_error.leaf_index,
			feature_sat_error.provenance.offset);
		goto out;
	}
	for (index = 0; index < feature_audit.leaf_count; index++) {
		switch (feature_audit.leaves[index]) {
		case ORLIX_TCTI_TARGET_FEATURE_SAT_APPLICABLE:
			applicable_count++;
			break;
		case ORLIX_TCTI_TARGET_FEATURE_SAT_IMPOSSIBLE:
			impossible_count++;
			break;
		default:
			unresolved_count++;
			break;
		}
	}
	if (unresolved_count ||
	    applicable_count + impossible_count != feature_audit.leaf_count) {
		fprintf(stderr,
			"target-feature Boolean SAT census unresolved: "
			"applicable=%zu impossible=%zu unresolved=%zu total=%zu\n",
			applicable_count, impossible_count, unresolved_count,
			feature_audit.leaf_count);
		goto out;
	}
	source = orlix_tcti_target_ledger_source(&source_count);
	classification = orlix_tcti_target_ledger_classification(&classification_count);
	proof_registry = orlix_tcti_target_proof_registry_entries(&proof_registry_count);
	if (source_count != ORLIX_TCTI_A64_TARGET_LEAF_COUNT || classification_count != source_count ||
	    orlix_tcti_target_ledger_validate(source, source_count, classification,
					classification_count,
					ORLIX_TCTI_A64_TARGET_LEAF_COUNT,
					proof_registry, proof_registry_count,
					&ledger) ||
	    orlix_tcti_target_ledger_verify_import(source, source_count, &target, &ledger)) {
		fprintf(stderr, "target-ledger audit failed: source=%zu classification=%zu "
			"unclassified=%zu proof-gaps=%zu errors=%zu first=%d@%zu\n",
			source_count, classification_count, ledger.unclassified_rows,
			ledger.proof_gaps, ledger.errors, ledger.first_error,
			ledger.first_error_index);
		goto out;
	}
	printf("pinned imports and Boolean feature SAT completed: "
	       "source=%zu classification=%zu applicable=%zu impossible=%zu "
	       "feature-parameters=%zu register-records-imported=%zu\n",
	       source_count, classification_count, applicable_count,
	       impossible_count, feature_model.parameter_count,
	       register_model.register_count);
	status = EXIT_SUCCESS;
out:
	orlix_tcti_target_feature_sat_audit_destroy(&feature_audit);
	orlix_tcti_register_model_destroy(&register_model);
	orlix_tcti_feature_typed_result_destroy(&typed_result);
	orlix_tcti_typed_destroy(&typed_expression);
	orlix_tcti_target_feature_model_destroy(&feature_model);
	orlix_tcti_target_inventory_destroy(&target);
	free(registers_json);
	free(features_json);
	free(instructions_json);
	return status;
}
