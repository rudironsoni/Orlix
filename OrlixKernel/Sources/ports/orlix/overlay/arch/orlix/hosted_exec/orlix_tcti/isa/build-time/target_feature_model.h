/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_FEATURE_MODEL_H
#define ORLIX_TCTI_TARGET_FEATURE_MODEL_H

#include <stddef.h>
#include <stdint.h>

#define ORLIX_TCTI_FEATURE_PARAMETER_COUNT 409U
#define ORLIX_TCTI_FEATURE_CONSTRAINT_COUNT 1626U
#define ORLIX_TCTI_FEATURE_NODE_NONE UINT32_MAX

enum orlix_tcti_feature_node_kind {
	ORLIX_TCTI_FEATURE_BOOL, ORLIX_TCTI_FEATURE_IDENTIFIER, ORLIX_TCTI_FEATURE_INTEGER,
	ORLIX_TCTI_FEATURE_DOT_ATOM, ORLIX_TCTI_FEATURE_SET, ORLIX_TCTI_FEATURE_NOT,
	ORLIX_TCTI_FEATURE_AND, ORLIX_TCTI_FEATURE_OR, ORLIX_TCTI_FEATURE_EQ, ORLIX_TCTI_FEATURE_NE,
	ORLIX_TCTI_FEATURE_LT, ORLIX_TCTI_FEATURE_GT, ORLIX_TCTI_FEATURE_GE, ORLIX_TCTI_FEATURE_IN,
	ORLIX_TCTI_FEATURE_IMPLIES, ORLIX_TCTI_FEATURE_IFF, ORLIX_TCTI_FEATURE_UINT,
	ORLIX_TCTI_FEATURE_SINT, ORLIX_TCTI_FEATURE_FIELD, ORLIX_TCTI_FEATURE_VALUE,
};

struct orlix_tcti_feature_provenance { size_t offset; size_t length; };
/*
 * AARCHMRS 2026-06 represents Types.Field instance and slices explicitly as
 * JSON null. Keep that source fact and its exact span instead of silently
 * treating either qualifier as absent. A future source form needs an explicit
 * model extension before it can enter the target inventory.
 */
enum orlix_tcti_feature_field_qualifier_kind {
	ORLIX_TCTI_FEATURE_FIELD_QUALIFIER_NONE,
	ORLIX_TCTI_FEATURE_FIELD_QUALIFIER_NULL,
};
struct orlix_tcti_feature_field_qualifier {
	enum orlix_tcti_feature_field_qualifier_kind kind;
	struct orlix_tcti_feature_provenance provenance;
};
struct orlix_tcti_feature_field {
	char *state;
	char *register_name;
	char *selector;
	struct orlix_tcti_feature_field_qualifier instance;
	struct orlix_tcti_feature_field_qualifier slices;
	struct orlix_tcti_feature_provenance provenance;
};
struct orlix_tcti_feature_node {
	enum orlix_tcti_feature_node_kind kind;
	uint32_t left, right, first_child, child_count;
	int64_t integer;
	char *text;
	struct orlix_tcti_feature_field field;
	struct orlix_tcti_feature_provenance provenance;
};
struct orlix_tcti_feature_parameter {
	char *name;
	uint32_t first_constraint, constraint_count;
	struct orlix_tcti_feature_provenance provenance;
};
struct orlix_tcti_feature_model {
	struct orlix_tcti_feature_parameter *parameters;
	size_t parameter_count, parameter_capacity;
	uint32_t *constraints;
	size_t constraint_count, constraint_capacity;
	struct orlix_tcti_feature_node *nodes;
	size_t node_count, node_capacity;
	uint32_t *children;
	size_t child_count, child_capacity;
};
enum orlix_tcti_feature_error_code {
	ORLIX_TCTI_FEATURE_OK, ORLIX_TCTI_FEATURE_INVALID_ARGUMENT, ORLIX_TCTI_FEATURE_INVALID_JSON,
	ORLIX_TCTI_FEATURE_INVALID_SOURCE, ORLIX_TCTI_FEATURE_UNSUPPORTED_GRAMMAR,
	ORLIX_TCTI_FEATURE_LIMIT, ORLIX_TCTI_FEATURE_NO_MEMORY, ORLIX_TCTI_FEATURE_PIN_MISMATCH,
	ORLIX_TCTI_FEATURE_COUNT_MISMATCH, ORLIX_TCTI_FEATURE_DUPLICATE_KEY,
};
struct orlix_tcti_feature_error { enum orlix_tcti_feature_error_code code; size_t offset; char message[160]; };

int orlix_tcti_target_feature_model_import(const char *json, size_t length,
	struct orlix_tcti_feature_model *model, struct orlix_tcti_feature_error *error);
void orlix_tcti_target_feature_model_destroy(struct orlix_tcti_feature_model *model);

#endif
