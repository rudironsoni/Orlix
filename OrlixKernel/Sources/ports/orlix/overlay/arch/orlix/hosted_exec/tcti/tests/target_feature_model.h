/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_FEATURE_MODEL_H
#define ORLIX_TCTI_TARGET_FEATURE_MODEL_H

#include <stddef.h>
#include <stdint.h>

#define TCTI_FEATURE_PARAMETER_COUNT 409U
#define TCTI_FEATURE_CONSTRAINT_COUNT 1626U
#define TCTI_FEATURE_NODE_NONE UINT32_MAX

enum tcti_feature_node_kind {
	TCTI_FEATURE_BOOL, TCTI_FEATURE_IDENTIFIER, TCTI_FEATURE_INTEGER,
	TCTI_FEATURE_DOT_ATOM, TCTI_FEATURE_SET, TCTI_FEATURE_NOT,
	TCTI_FEATURE_AND, TCTI_FEATURE_OR, TCTI_FEATURE_EQ, TCTI_FEATURE_NE,
	TCTI_FEATURE_LT, TCTI_FEATURE_GT, TCTI_FEATURE_GE, TCTI_FEATURE_IN,
	TCTI_FEATURE_IMPLIES, TCTI_FEATURE_IFF, TCTI_FEATURE_UINT,
	TCTI_FEATURE_SINT, TCTI_FEATURE_FIELD, TCTI_FEATURE_VALUE,
};

struct tcti_feature_provenance { size_t offset; size_t length; };
struct tcti_feature_field {
	char *state;
	char *register_name;
	char *selector;
	struct tcti_feature_provenance provenance;
};
struct tcti_feature_node {
	enum tcti_feature_node_kind kind;
	uint32_t left, right, first_child, child_count;
	int64_t integer;
	char *text;
	struct tcti_feature_field field;
	struct tcti_feature_provenance provenance;
};
struct tcti_feature_parameter {
	char *name;
	uint32_t first_constraint, constraint_count;
	struct tcti_feature_provenance provenance;
};
struct tcti_feature_model {
	struct tcti_feature_parameter *parameters;
	size_t parameter_count, parameter_capacity;
	uint32_t *constraints;
	size_t constraint_count, constraint_capacity;
	struct tcti_feature_node *nodes;
	size_t node_count, node_capacity;
	uint32_t *children;
	size_t child_count, child_capacity;
};
enum tcti_feature_error_code {
	TCTI_FEATURE_OK, TCTI_FEATURE_INVALID_ARGUMENT, TCTI_FEATURE_INVALID_JSON,
	TCTI_FEATURE_INVALID_SOURCE, TCTI_FEATURE_UNSUPPORTED_GRAMMAR,
	TCTI_FEATURE_LIMIT, TCTI_FEATURE_NO_MEMORY, TCTI_FEATURE_PIN_MISMATCH,
	TCTI_FEATURE_COUNT_MISMATCH, TCTI_FEATURE_DUPLICATE_KEY,
};
struct tcti_feature_error { enum tcti_feature_error_code code; size_t offset; char message[160]; };

int tcti_target_feature_model_import(const char *json, size_t length,
	struct tcti_feature_model *model, struct tcti_feature_error *error);
void tcti_target_feature_model_destroy(struct tcti_feature_model *model);

#endif
