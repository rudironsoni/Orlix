/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_feature_applicability_artifact.h"
#include "target_feature_artifact.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXPECT(condition) do { if (!(condition)) { \
	fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
	return 1; } } while (0)

static int expect_error(
	const struct orlix_tcti_target_feature_applicability_artifact *artifact,
	enum orlix_tcti_target_feature_applicability_validation_error error)
{
	struct orlix_tcti_target_feature_applicability_validation_result result;
	return orlix_tcti_target_feature_applicability_artifact_validate(
		artifact, &result) == -1 && result.error == error ? 0 : -1;
}

static int expect_semantic_error(
	const struct orlix_tcti_target_feature_applicability_artifact *artifact,
	struct orlix_tcti_target_feature_applicability_semantic_scratch *scratch)
{
	struct orlix_tcti_target_feature_applicability_validation_result result;
	return orlix_tcti_target_feature_applicability_artifact_validate_semantics(
		artifact, scratch, &result) == -1 && result.error ==
		ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_SEMANTIC_MISMATCH ? 0 : -1;
}

int main(void)
{
	const struct orlix_tcti_target_feature_applicability_artifact *canonical =
		orlix_tcti_target_feature_applicability_artifact();
	struct orlix_tcti_target_feature_applicability_validation_result result;
	struct orlix_tcti_target_feature_applicability_artifact mutated;
	struct orlix_tcti_target_feature_applicability_provenance provenance;
	struct orlix_tcti_target_feature_applicability_row *rows;
	struct orlix_tcti_target_feature_applicability_certificate_value *symbols;
	struct orlix_tcti_target_feature_applicability_certificate_value *operand_values;
	char *condition;
	signed char witness[ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_PARAMETERS];
	size_t config_index = SIZE_MAX;
	size_t boolean_index = SIZE_MAX;
	size_t operand_row = SIZE_MAX;
	size_t mutation_parameter = SIZE_MAX;
	size_t index;
	int semantic_status;
	static unsigned char active[8955];
	static unsigned char seen[ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_MAX_COMMON_VALUES];
	static size_t parameter_order[ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_PARAMETERS];
	static size_t binding_order[605];
	static size_t field_common[362];
	struct orlix_tcti_target_feature_applicability_semantic_scratch scratch = {
		active, sizeof(active), seen, sizeof(seen),
		parameter_order, sizeof(parameter_order) / sizeof(parameter_order[0]),
		binding_order, sizeof(binding_order) / sizeof(binding_order[0]),
		field_common, sizeof(field_common) / sizeof(field_common[0]),
	};

	EXPECT(canonical);
	EXPECT(!orlix_tcti_target_feature_applicability_artifact_validate(
		canonical, &result));
	EXPECT(result.applicable_count == ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_ROWS);
	semantic_status = orlix_tcti_target_feature_applicability_artifact_validate_semantics(
		canonical, &scratch, &result);
	if (semantic_status)
		fprintf(stderr, "semantic validation failed: error=%u row=%zu\n",
			(unsigned int)result.error, result.row);
	EXPECT(!semantic_status);

	mutated = *canonical;
	provenance = *canonical->provenance;
	provenance.registers_sha256 =
		"4bd76c3c3ce90322eb4fd179675dafe82df2fd1cb789beee516e5b29c471b874";
	mutated.provenance = &provenance;
	EXPECT(!expect_error(&mutated,
		ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_PROVENANCE));

	rows = malloc(canonical->row_count * sizeof(*rows));
	EXPECT(rows);
	memcpy(rows, canonical->rows, canonical->row_count * sizeof(*rows));
	mutated = *canonical;
	mutated.rows = rows;
	rows[0].condition_index++;
	EXPECT(!expect_error(&mutated,
		ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_ROW));
	rows[0] = canonical->rows[0];
	condition = malloc(strlen(rows[0].condition_tcnd_hex) + 1U);
	EXPECT(condition);
	strcpy(condition, rows[0].condition_tcnd_hex);
	condition[0] = condition[0] == '0' ? '1' : '0';
	rows[0].condition_tcnd_hex = condition;
	EXPECT(!expect_error(&mutated,
		ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_ROW));
	free(condition);
	rows[0] = canonical->rows[0];
	memcpy(witness, rows[0].witness, sizeof(witness));
	witness[0] = 0;
	rows[0].witness = witness;
	EXPECT(!expect_error(&mutated,
		ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_WITNESS));
	rows[0] = canonical->rows[0];
	rows[0].status = ORLIX_TCTI_TARGET_FEATURE_IMPOSSIBLE;
	EXPECT(!expect_error(&mutated,
		ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_ROW));
	free(rows);

	for (index = 0; index < canonical->common_symbol_count; index++)
		if (canonical->common_symbols[index].kind ==
		    ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_CONFIGURATION) {
			config_index = index;
			break;
		}
	EXPECT(config_index != SIZE_MAX);
	symbols = malloc(canonical->common_symbol_count * sizeof(*symbols));
	EXPECT(symbols);
	memcpy(symbols, canonical->common_symbols,
		canonical->common_symbol_count * sizeof(*symbols));
	mutated = *canonical;
	mutated.common_symbols = symbols;
	symbols[config_index].feature_node_index =
		(orlix_tcti_feature_applicability_u32)-1;
	EXPECT(!expect_error(&mutated,
		ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_CERTIFICATE));
	free(symbols);
	for (index = 0; index < canonical->common_symbol_count; index++)
		if (canonical->common_symbols[index].kind ==
		    ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_BOOLEAN_IDENTIFIER) {
			boolean_index = index;
			break;
		}
	EXPECT(boolean_index != SIZE_MAX);
	symbols = malloc(canonical->common_symbol_count * sizeof(*symbols));
	EXPECT(symbols);
	memcpy(symbols, canonical->common_symbols,
		canonical->common_symbol_count * sizeof(*symbols));
	mutated = *canonical;
	mutated.common_symbols = symbols;
	symbols[boolean_index].name = "FEAT_NOT_IN_CERTIFICATE";
	EXPECT(!expect_error(&mutated,
		ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_CERTIFICATE));
	free(symbols);

	mutated = *canonical;
	mutated.common_symbol_count--;
	EXPECT(!expect_error(&mutated,
		ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_COUNT));

	mutated = *canonical;
	mutated.operand_value_count--;
	EXPECT(!expect_error(&mutated,
		ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_COUNT));
	for (index = 0; index < canonical->row_count; index++)
		if (canonical->rows[index].operand_value_count > 1U) {
			operand_row = index;
			break;
		}
	EXPECT(operand_row != SIZE_MAX);
	operand_values = malloc(canonical->operand_value_count * sizeof(*operand_values));
	EXPECT(operand_values);
	memcpy(operand_values, canonical->operand_values,
		canonical->operand_value_count * sizeof(*operand_values));
	operand_values[canonical->rows[operand_row].first_operand_value + 1U].name =
		operand_values[canonical->rows[operand_row].first_operand_value].name;
	mutated = *canonical;
	mutated.operand_values = operand_values;
	EXPECT(!expect_error(&mutated,
		ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_CERTIFICATE));
	free(operand_values);

	EXPECT(strstr(canonical->rows[0].condition_tcnd_hex,
		"464541545f535645")); /* FEAT_SVE in the source-bound TCND. */
	for (index = 0; index < ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_PARAMETERS;
	     index++)
		if (canonical->rows[0].witness[index] > 0 &&
		    !strcmp(orlix_tcti_feature_artifact_canonical()->parameters[index].name,
			"FEAT_SVE")) {
			mutation_parameter = index;
			break;
		}
	EXPECT(mutation_parameter != SIZE_MAX);
	rows = malloc(canonical->row_count * sizeof(*rows));
	EXPECT(rows);
	memcpy(rows, canonical->rows, canonical->row_count * sizeof(*rows));
	memcpy(witness, rows[0].witness, sizeof(witness));
	witness[mutation_parameter] = -witness[mutation_parameter];
	rows[0].witness = witness;
	mutated = *canonical;
	mutated.rows = rows;
	EXPECT(!orlix_tcti_target_feature_applicability_artifact_validate(
		&mutated, &result));
	EXPECT(!expect_semantic_error(&mutated, &scratch));
	free(rows);

	puts("PASS target feature applicability artifact validation");
	return 0;
}
