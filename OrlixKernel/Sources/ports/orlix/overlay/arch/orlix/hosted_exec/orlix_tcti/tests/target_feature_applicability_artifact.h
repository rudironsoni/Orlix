/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_ARTIFACT_H
#define ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_ARTIFACT_H

#ifdef __KERNEL__
#include <linux/stddef.h>
#include <linux/types.h>
typedef u32 orlix_tcti_feature_applicability_u32;
typedef u64 orlix_tcti_feature_applicability_u64;
#else
#include <stddef.h>
#include <stdint.h>
typedef uint32_t orlix_tcti_feature_applicability_u32;
typedef uint64_t orlix_tcti_feature_applicability_u64;
#endif

#define ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_ROWS 4350U
#define ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_PARAMETERS 409U
#define ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_CONFIGURATION_VALUES 10U
#define ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_BOOLEAN_VALUES 5U
#define ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_FIELD_VALUES 362U
#define ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_COMMON_VALUES 377U
#define ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_MAX_COMMON_VALUES 1024U
#define ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_MAX_OPERAND_VALUES 64U
#define ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_OPERAND_VALUES 364U
#define ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_COMMON_CHUNK_HEX_LENGTH 512U
#define ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_COMMON_CHUNK_COUNT 64U

enum orlix_tcti_target_feature_applicability_status {
	ORLIX_TCTI_TARGET_FEATURE_IMPOSSIBLE = 0,
	ORLIX_TCTI_TARGET_FEATURE_APPLICABLE = 1,
};

enum orlix_tcti_target_feature_applicability_symbol_kind {
	ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_CONFIGURATION,
	ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_FIELD,
	ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_TARGET_OPERAND,
	ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_BOOLEAN_IDENTIFIER,
};

enum orlix_tcti_target_feature_applicability_numeric_kind {
	ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_UNSIGNED,
	ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_SIGNED,
};

struct orlix_tcti_target_feature_applicability_certificate_value {
	enum orlix_tcti_target_feature_applicability_symbol_kind kind;
	enum orlix_tcti_target_feature_applicability_numeric_kind numeric_kind;
	orlix_tcti_feature_applicability_u32 feature_node_index;
	orlix_tcti_feature_applicability_u32 identity_group_index;
	orlix_tcti_feature_applicability_u32 leaf_index;
	const char *name;
	orlix_tcti_feature_applicability_u32 width;
	orlix_tcti_feature_applicability_u64 low;
	orlix_tcti_feature_applicability_u64 high;
};

struct orlix_tcti_target_feature_applicability_provenance {
	const char *architecture;
	const char *release;
	const char *instructions_sha256;
	const char *features_sha256;
	const char *registers_sha256;
	const char *reconciliation_identity;
	size_t row_count;
	size_t parameter_count;
};

struct orlix_tcti_target_feature_applicability_row {
	orlix_tcti_feature_applicability_u32 ordinal;
	const char *name;
	const char *mnemonic;
	const char *operation_id;
	orlix_tcti_feature_applicability_u32 condition_index;
	orlix_tcti_feature_applicability_u32 condition_length;
	enum orlix_tcti_target_feature_applicability_status status;
	const char *condition_tcnd_hex;
	const signed char *witness;
	size_t witness_count;
	size_t witness_storage_length;
	size_t first_operand_value;
	size_t operand_value_count;
	orlix_tcti_feature_applicability_u64 formula_identity;
};

struct orlix_tcti_target_feature_applicability_common_values {
	const char *chunks[
		ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_COMMON_CHUNK_COUNT];
};

struct orlix_tcti_target_feature_applicability_artifact {
	const struct orlix_tcti_target_feature_applicability_provenance *provenance;
	const struct orlix_tcti_target_feature_applicability_row *rows;
	size_t row_count;
	const struct orlix_tcti_target_feature_applicability_certificate_value
		*common_symbols;
	size_t common_symbol_count;
	const struct orlix_tcti_target_feature_applicability_common_values
		*common_values;
	size_t common_value_row_count;
	const struct orlix_tcti_target_feature_applicability_certificate_value
		*operand_values;
	size_t operand_value_count;
	size_t common_value_bytes_per_row;
};

enum orlix_tcti_target_feature_applicability_validation_error {
	ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_VALID = 0,
	ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_ARGUMENT,
	ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_PROVENANCE,
	ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_COUNT,
	ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_ROW,
	ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_CONDITION_BINDING,
	ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_WITNESS,
	ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_CERTIFICATE,
	ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_SEMANTIC_MISMATCH,
	ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_UNSAT_CERTIFICATE_MISSING,
};

struct orlix_tcti_target_feature_applicability_validation_result {
	enum orlix_tcti_target_feature_applicability_validation_error error;
	size_t row;
	size_t applicable_count;
	size_t impossible_count;
};

struct orlix_tcti_target_feature_applicability_semantic_scratch {
	unsigned char *active_feature_nodes;
	size_t active_feature_node_count;
	unsigned char *common_values_seen;
	size_t common_values_seen_count;
	size_t *parameter_order;
	size_t parameter_order_count;
	size_t *binding_order;
	size_t binding_order_count;
	size_t *field_common_index;
	size_t field_common_index_count;
};

int orlix_tcti_target_feature_applicability_artifact_validate(
	const struct orlix_tcti_target_feature_applicability_artifact *artifact,
	struct orlix_tcti_target_feature_applicability_validation_result *result);

int orlix_tcti_target_feature_applicability_artifact_validate_semantics(
	const struct orlix_tcti_target_feature_applicability_artifact *artifact,
	struct orlix_tcti_target_feature_applicability_semantic_scratch *scratch,
	struct orlix_tcti_target_feature_applicability_validation_result *result);

const struct orlix_tcti_target_feature_applicability_artifact *
orlix_tcti_target_feature_applicability_artifact(void);

#endif /* ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_ARTIFACT_H */
