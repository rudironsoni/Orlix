/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_FEATURE_FIELD_DOMAIN_BINDING_H
#define ORLIX_TCTI_TARGET_FEATURE_FIELD_DOMAIN_BINDING_H

#include <stddef.h>
#include <stdint.h>

#include "target_feature_model.h"
#include "target_register_model.h"

enum orlix_tcti_feature_field_domain_disposition {
	ORLIX_TCTI_FEATURE_FIELD_DOMAIN_MAPPED,
	ORLIX_TCTI_FEATURE_FIELD_DOMAIN_MISSING_REGISTER,
	ORLIX_TCTI_FEATURE_FIELD_DOMAIN_AMBIGUOUS_REGISTER,
	ORLIX_TCTI_FEATURE_FIELD_DOMAIN_MISSING_FIELD,
	ORLIX_TCTI_FEATURE_FIELD_DOMAIN_AMBIGUOUS_FIELD,
	ORLIX_TCTI_FEATURE_FIELD_DOMAIN_INVALID_MODEL,
};

enum orlix_tcti_feature_field_domain_type {
	ORLIX_TCTI_FEATURE_FIELD_DOMAIN_TYPE_CONSTANT,
	ORLIX_TCTI_FEATURE_FIELD_DOMAIN_TYPE_COUNT,
};

enum orlix_tcti_feature_field_domain_signedness {
	ORLIX_TCTI_FEATURE_FIELD_DOMAIN_UNSIGNED,
	ORLIX_TCTI_FEATURE_FIELD_DOMAIN_SIGNEDNESS_COUNT,
};

enum orlix_tcti_feature_field_domain_member_kind {
	ORLIX_TCTI_FEATURE_FIELD_DOMAIN_ENUM_MEMBERS,
	ORLIX_TCTI_FEATURE_FIELD_DOMAIN_RANGE_MEMBERS,
	ORLIX_TCTI_FEATURE_FIELD_DOMAIN_MIXED_MEMBERS,
	ORLIX_TCTI_FEATURE_FIELD_DOMAIN_MEMBER_KIND_COUNT,
};

/*
 * Fixed-width, source-independent normalization of one Arm register field.
 * The two identities cover the detailed range/value/relation trees.  The
 * semantic identity excludes source positions and fieldset applicability;
 * the resolution identity includes every equivalent declaration and its
 * condition provenance.
 */
struct orlix_tcti_feature_field_normalized_domain {
	uint32_t fieldset_index;
	uint32_t equivalent_field_count;
	uint32_t register_width;
	uint32_t field_width;
	uint32_t range_count;
	uint32_t value_member_count;
	uint32_t range_member_count;
	uint32_t relation_value_count;
	uint32_t relation_constraint_count;
	uint32_t field_condition_expression;
	uint32_t wrapper_condition_expression;
	uint32_t fieldset_condition_expression;
	uint32_t type;
	uint32_t signedness;
	uint32_t member_kind;
	uint64_t semantic_identity;
	uint64_t resolution_identity;
	uint32_t fieldset_source_offset;
	uint32_t fieldset_source_length;
	uint32_t field_condition_offset;
	uint32_t field_condition_length;
	uint32_t wrapper_condition_offset;
	uint32_t wrapper_condition_length;
	uint32_t fieldset_condition_offset;
	uint32_t fieldset_condition_length;
};

/* All indexes below are relative to the owning alternative's table slice. */
struct orlix_tcti_feature_field_domain_range {
	uint32_t start;
	uint32_t width;
};

struct orlix_tcti_feature_field_domain_valueset {
	const char *type;
};

struct orlix_tcti_feature_field_domain_expression {
	const char *type;
	const char *name;
	const char *op;
	const char *value;
	const char *role;
	const char *register_state;
	const char *register_name;
	const char *field_name;
	uint32_t first_child;
	uint32_t child_count;
	uint32_t scalar_kind;
	int64_t integer;
	uint32_t boolean;
	uint32_t parent_expression;
	size_t instance_offset;
	size_t instance_length;
	size_t slices_offset;
	size_t slices_length;
	size_t source_offset;
	size_t source_length;
};

struct orlix_tcti_feature_field_domain_node {
	const char *type;
	const char *value;
	const char *start;
	const char *end;
	const char *link;
	const char *meaning;
	uint32_t parent_domain;
	uint32_t condition_expression;
	uint32_t first_child_domain;
	uint32_t child_domain_count;
	uint32_t valueset_index;
	uint32_t nested_valueset_index;
	uint32_t first_link;
	uint32_t link_count;
};

struct orlix_tcti_feature_field_domain_link {
	const char *key;
	const char *value;
	uint32_t domain_index;
};

struct orlix_tcti_feature_field_domain_value_candidate {
	uint32_t kind;
	uint32_t valueset_index;
	uint32_t first_domain;
	uint32_t domain_count;
};

struct orlix_tcti_feature_field_domain_constraint {
	uint32_t kind;
	uint32_t valueset_index;
	uint32_t first_item;
	uint32_t item_count;
};

struct orlix_tcti_feature_field_domain_constraint_item {
	uint32_t constraint_index;
	uint32_t valueset_index;
	uint32_t domain_index;
};

struct orlix_tcti_feature_field_domain_constraint_candidate {
	uint32_t constraint_index;
	uint32_t kind;
	uint32_t first_domain;
	uint32_t domain_count;
};

struct orlix_tcti_feature_field_domain_alternative {
	uint32_t field_index;
	uint32_t value_relation_index;
	uint32_t fieldset_index;
	uint32_t field_condition_expression;
	uint32_t wrapper_condition_expression;
	uint32_t fieldset_condition_expression;
	uint32_t relation_field_condition_expression;
	uint32_t relation_wrapper_condition_expression;
	uint32_t first_range;
	uint32_t range_count;
	uint32_t first_valueset;
	uint32_t valueset_count;
	uint32_t first_domain;
	uint32_t domain_count;
	uint32_t first_link;
	uint32_t link_count;
	uint32_t first_expression;
	uint32_t expression_count;
	uint32_t first_expression_child;
	uint32_t expression_child_count;
	uint32_t first_value_candidate;
	uint32_t value_candidate_count;
	uint32_t first_constraint;
	uint32_t constraint_count;
	uint32_t first_constraint_item;
	uint32_t constraint_item_count;
	uint32_t first_constraint_candidate;
	uint32_t constraint_candidate_count;
	size_t field_source_offset;
	size_t field_source_length;
	size_t value_relation_source_offset;
	size_t value_relation_source_length;
	size_t fieldset_source_offset;
	size_t fieldset_source_length;
	size_t field_condition_offset;
	size_t field_condition_length;
	size_t wrapper_condition_offset;
	size_t wrapper_condition_length;
	size_t fieldset_condition_offset;
	size_t fieldset_condition_length;
};

struct orlix_tcti_feature_field_domain_binding {
	/* Every source AST.Field row. Equal identities share one group. */
	uint32_t feature_node_index;
	uint32_t identity_group_index;
	uint32_t occurrence_count;
	uint32_t register_index;
	uint32_t field_index;
	uint32_t value_relation_index;
	uint32_t first_alternative;
	uint32_t alternative_count;
	enum orlix_tcti_feature_field_domain_disposition disposition;
	struct orlix_tcti_feature_field_normalized_domain domain;
	struct orlix_tcti_feature_provenance feature_provenance;
	size_t register_source_offset;
	size_t register_source_length;
	size_t field_source_offset;
	size_t field_source_length;
	size_t value_relation_source_offset;
	size_t value_relation_source_length;
};

struct orlix_tcti_feature_field_domain_bindings {
	struct orlix_tcti_feature_field_domain_binding *items;
	size_t count;
	struct orlix_tcti_feature_field_domain_alternative *alternatives;
	size_t alternative_count;
	struct orlix_tcti_feature_field_domain_range *ranges;
	size_t range_count;
	struct orlix_tcti_feature_field_domain_valueset *valuesets;
	size_t valueset_count;
	struct orlix_tcti_feature_field_domain_node *domains;
	size_t domain_count;
	struct orlix_tcti_feature_field_domain_link *links;
	size_t link_count;
	struct orlix_tcti_feature_field_domain_expression *expressions;
	size_t expression_count;
	uint32_t *expression_children;
	size_t expression_child_count;
	struct orlix_tcti_feature_field_domain_value_candidate *value_candidates;
	size_t value_candidate_count;
	struct orlix_tcti_feature_field_domain_constraint *constraints;
	size_t constraint_count;
	struct orlix_tcti_feature_field_domain_constraint_item *constraint_items;
	size_t constraint_item_count;
	struct orlix_tcti_feature_field_domain_constraint_candidate *constraint_candidates;
	size_t constraint_candidate_count;
};

enum orlix_tcti_feature_field_domain_binding_error_code {
	ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_OK,
	ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_INVALID_ARGUMENT,
	ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_NO_MEMORY,
	ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_INVALID_MODEL,
	ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_UNSUPPORTED_DOMAIN,
};

struct orlix_tcti_feature_field_domain_binding_error {
	enum orlix_tcti_feature_field_domain_binding_error_code code;
	size_t offset;
	char message[160];
};

int orlix_tcti_target_feature_field_domain_bindings_build(
	const struct orlix_tcti_feature_model *features,
	const struct orlix_tcti_register_model *registers,
	struct orlix_tcti_feature_field_domain_bindings *bindings,
	struct orlix_tcti_feature_field_domain_binding_error *error);
void orlix_tcti_target_feature_field_domain_bindings_destroy(
	struct orlix_tcti_feature_field_domain_bindings *bindings);

#endif
