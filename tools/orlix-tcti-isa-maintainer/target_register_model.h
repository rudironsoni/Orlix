/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_REGISTER_MODEL_H
#define ORLIX_TCTI_TARGET_REGISTER_MODEL_H

#include <stddef.h>
#include <stdint.h>

#define ORLIX_TCTI_REGISTER_SOURCE_SHA256 "5bd76c3c3ce90322eb4fd179675dafe82df2fd1cb789beee516e5b29c471b874"
#define ORLIX_TCTI_REGISTER_RECORD_COUNT 2005U
#define ORLIX_TCTI_REGISTER_FIELDSET_COUNT 2370U
#define ORLIX_TCTI_REGISTER_FIELD_COUNT 15743U
#define ORLIX_TCTI_REGISTER_RANGE_COUNT 15863U
#define ORLIX_TCTI_REGISTER_VALUESET_COUNT 7884U
#define ORLIX_TCTI_REGISTER_DOMAIN_COUNT 12926U
#define ORLIX_TCTI_REGISTER_ACCESSOR_COUNT 3596U
#define ORLIX_TCTI_REGISTER_METADATA_COUNT 1801U
#define ORLIX_TCTI_REGISTER_MEMORY_ACCESS_COUNT 1862U

enum orlix_tcti_register_model_error_code {
	ORLIX_TCTI_REGISTER_MODEL_OK,
	ORLIX_TCTI_REGISTER_MODEL_INVALID_ARGUMENT,
	ORLIX_TCTI_REGISTER_MODEL_NO_MEMORY,
	ORLIX_TCTI_REGISTER_MODEL_INVALID_JSON,
	ORLIX_TCTI_REGISTER_MODEL_INVALID_SOURCE,
	ORLIX_TCTI_REGISTER_MODEL_UNSUPPORTED_TYPE,
	ORLIX_TCTI_REGISTER_MODEL_COUNT_MISMATCH,
	ORLIX_TCTI_REGISTER_MODEL_INPUT_LIMIT,
	ORLIX_TCTI_REGISTER_MODEL_DEPTH_LIMIT,
	ORLIX_TCTI_REGISTER_MODEL_HASH_MISMATCH,
};

struct orlix_tcti_register_model_error {
	enum orlix_tcti_register_model_error_code code;
	size_t offset;
	char message[160];
};

struct orlix_tcti_register_identity {
	char *name;
	char *type;
	char *state;
	char *index_variable;
	uint32_t parent_register;
	uint32_t metadata_owner;
	uint32_t metadata_index;
	uint32_t default_access_expression;
	uint32_t first_fieldset;
	uint32_t fieldset_count;
	size_t metadata_offset;
	size_t metadata_length;
	size_t source_offset;
	size_t source_length;
};

/* Pinned source provenance is typed, including Arm's license declaration. */
struct orlix_tcti_register_metadata {
	char *copyright;
	char *license_info;
	char *architecture;
	char *build;
	char *ref;
	char *schema;
	char *timestamp;
	size_t source_offset;
	size_t source_length;
};

enum orlix_tcti_register_memory_read_access {
	ORLIX_TCTI_REGISTER_MEMORY_READ_R,
	ORLIX_TCTI_REGISTER_MEMORY_READ_RAZ,
	ORLIX_TCTI_REGISTER_MEMORY_READ_RAO,
	ORLIX_TCTI_REGISTER_MEMORY_READ_RES0,
	ORLIX_TCTI_REGISTER_MEMORY_READ_RES1,
	ORLIX_TCTI_REGISTER_MEMORY_READ_UNKNOWN,
	ORLIX_TCTI_REGISTER_MEMORY_READ_IGNORE,
	ORLIX_TCTI_REGISTER_MEMORY_READ_RC,
	ORLIX_TCTI_REGISTER_MEMORY_READ_RS,
	ORLIX_TCTI_REGISTER_MEMORY_READ_RESERVED,
	ORLIX_TCTI_REGISTER_MEMORY_READ_ERROR,
};

enum orlix_tcti_register_memory_write_access {
	ORLIX_TCTI_REGISTER_MEMORY_WRITE_W,
	ORLIX_TCTI_REGISTER_MEMORY_WRITE_WI,
	ORLIX_TCTI_REGISTER_MEMORY_WRITE_RES0,
	ORLIX_TCTI_REGISTER_MEMORY_WRITE_RES1,
	ORLIX_TCTI_REGISTER_MEMORY_WRITE_W1S,
	ORLIX_TCTI_REGISTER_MEMORY_WRITE_W1C,
	ORLIX_TCTI_REGISTER_MEMORY_WRITE_W1T,
	ORLIX_TCTI_REGISTER_MEMORY_WRITE_W0S,
	ORLIX_TCTI_REGISTER_MEMORY_WRITE_W0C,
	ORLIX_TCTI_REGISTER_MEMORY_WRITE_W0T,
	ORLIX_TCTI_REGISTER_MEMORY_WRITE_WS,
	ORLIX_TCTI_REGISTER_MEMORY_WRITE_WC,
	ORLIX_TCTI_REGISTER_MEMORY_WRITE_SBZ,
	ORLIX_TCTI_REGISTER_MEMORY_WRITE_SBO,
	ORLIX_TCTI_REGISTER_MEMORY_WRITE_SBZP,
	ORLIX_TCTI_REGISTER_MEMORY_WRITE_SBOP,
	ORLIX_TCTI_REGISTER_MEMORY_WRITE_RESERVED,
	ORLIX_TCTI_REGISTER_MEMORY_WRITE_ERROR,
};

enum orlix_tcti_register_memory_access_form {
	ORLIX_TCTI_REGISTER_MEMORY_ACCESS_EXPLICIT_OBJECT,
	ORLIX_TCTI_REGISTER_MEMORY_ACCESS_LEGACY_SENTINEL,
};

enum orlix_tcti_register_memory_access_origin {
	ORLIX_TCTI_REGISTER_MEMORY_ACCESS_EXPLICIT,
	ORLIX_TCTI_REGISTER_MEMORY_ACCESS_SCHEMA_DEFAULT,
};

/* The typed source object is owned by an expression node, never by a raw span. */
enum orlix_tcti_register_memory_access_owner_kind {
	ORLIX_TCTI_REGISTER_MEMORY_ACCESS_OWNER_EXPRESSION,
};

enum orlix_tcti_register_memory_access_legacy_sentinel {
	ORLIX_TCTI_REGISTER_MEMORY_ACCESS_LEGACY_NONE,
	ORLIX_TCTI_REGISTER_MEMORY_ACCESS_LEGACY_RW,
	ORLIX_TCTI_REGISTER_MEMORY_ACCESS_LEGACY_RO,
	ORLIX_TCTI_REGISTER_MEMORY_ACCESS_LEGACY_WO_RAZ,
	ORLIX_TCTI_REGISTER_MEMORY_ACCESS_LEGACY_RAZ_WI,
	ORLIX_TCTI_REGISTER_MEMORY_ACCESS_LEGACY_WO_RES0,
	ORLIX_TCTI_REGISTER_MEMORY_ACCESS_LEGACY_WO_RES1,
	ORLIX_TCTI_REGISTER_MEMORY_ACCESS_LEGACY_WO_UNKNOWN,
	ORLIX_TCTI_REGISTER_MEMORY_ACCESS_LEGACY_WO,
	ORLIX_TCTI_REGISTER_MEMORY_ACCESS_LEGACY_WO_ERROR,
	ORLIX_TCTI_REGISTER_MEMORY_ACCESS_LEGACY_RO_ERROR,
	ORLIX_TCTI_REGISTER_MEMORY_ACCESS_LEGACY_ERROR,
	ORLIX_TCTI_REGISTER_MEMORY_ACCESS_LEGACY_ERROR_RESERVED,
	ORLIX_TCTI_REGISTER_MEMORY_ACCESS_LEGACY_RESERVED_ERROR,
};

struct orlix_tcti_register_memory_access {
	enum orlix_tcti_register_memory_access_form form;
	enum orlix_tcti_register_memory_read_access read;
	enum orlix_tcti_register_memory_write_access write;
	enum orlix_tcti_register_memory_access_origin read_origin;
	enum orlix_tcti_register_memory_access_origin write_origin;
	enum orlix_tcti_register_memory_access_legacy_sentinel legacy_sentinel;
	enum orlix_tcti_register_memory_access_owner_kind owner_kind;
	uint32_t owner_index;
	size_t read_offset;
	size_t read_length;
	size_t write_offset;
	size_t write_length;
	size_t source_offset;
	size_t source_length;
};

/*
 * ImplementationDefined is a separate Arm source construct. Its constraints
 * are either null or an ordered array of ReadWriteAccess objects. Retaining
 * this relation prevents a generator from mistaking its child permissions for
 * unowned generic AST nodes.
 */
enum orlix_tcti_register_implementation_defined_constraint_kind {
	ORLIX_TCTI_REGISTER_IMPLEMENTATION_DEFINED_CONSTRAINT_NULL,
	ORLIX_TCTI_REGISTER_IMPLEMENTATION_DEFINED_CONSTRAINT_ARRAY,
};

struct orlix_tcti_register_implementation_defined_permission {
	enum orlix_tcti_register_implementation_defined_constraint_kind kind;
	uint32_t expression_index;
	uint32_t first_memory_access;
	uint32_t memory_access_count;
	size_t source_offset;
	size_t source_length;
};

struct orlix_tcti_field_identity {
	char *name;
	char *type;
	uint32_t width;
	uint32_t register_index;
	uint32_t parent_field;
	uint32_t fieldset_index;
	uint32_t first_range;
	uint32_t range_count;
	uint32_t condition_expression;
	uint32_t wrapper_condition_expression;
	size_t condition_offset;
	size_t condition_length;
	size_t wrapper_condition_offset;
	size_t wrapper_condition_length;
	size_t branch_offset;
	size_t branch_length;
	size_t source_offset;
	size_t source_length;
};

struct orlix_tcti_register_fieldset {
	uint32_t register_index;
	uint32_t width;
	char *name;
	char *display;
	uint32_t condition_expression;
	size_t condition_offset;
	size_t condition_length;
	size_t source_offset;
	size_t source_length;
};

struct orlix_tcti_register_range {
	uint32_t start;
	uint32_t width;
	uint32_t field_index;
	size_t source_offset;
	size_t source_length;
};

struct orlix_tcti_conditional_field_branch {
	uint32_t conditional_field_index;
	uint32_t field_index;
	uint32_t condition_expression;
	size_t condition_offset;
	size_t condition_length;
	size_t source_offset;
	size_t source_length;
};

struct orlix_tcti_value_domain {
	char *type;
	char *value;
	char *end;
	char *link;
	char *meaning;
	uint32_t field_index;
	uint32_t parent_domain;
	uint32_t condition_expression;
	uint32_t first_child_domain;
	uint32_t child_domain_count;
	size_t condition_offset;
	size_t condition_length;
	size_t branch_offset;
	size_t branch_length;
	size_t source_offset;
	size_t source_length;
	char *start;
	uint32_t valueset_index;
	uint32_t nested_valueset_index;
};

/* Raw source spans retain the exact Arm-defined access and encoding objects. */
struct orlix_tcti_register_accessor {
	char *type;
	char *name;
	char *index_variable;
	char *component;
	char *frame;
	char *instance;
	char *power_domain;
	uint32_t register_index;
	uint32_t index_start;
	uint32_t index_width;
	uint32_t range_start;
	uint32_t range_width;
	uint32_t range_is_null;
	uint32_t component_is_null;
	uint32_t frame_is_null;
	uint32_t instance_is_null;
	uint32_t power_domain_is_null;
	uint32_t first_offset_expression;
	uint32_t offset_expression_count;
	uint32_t references_expression;
	uint32_t first_system_encoding;
	uint32_t system_encoding_count;
	uint32_t condition_expression;
	uint32_t access_expression;
	size_t condition_offset;
	size_t condition_length;
	size_t access_offset;
	size_t access_length;
	size_t encoding_offset;
	size_t encoding_length;
	size_t source_offset;
	size_t source_length;
};

/* Ordered roots for an accessor's offset expression or expression array. */
struct orlix_tcti_register_accessor_offset_expression {
	uint32_t accessor_index;
	uint32_t expression_index;
	size_t source_offset;
	size_t source_length;
};

struct orlix_tcti_register_system_encoding {
	char *type;
	char *asmvalue;
	uint32_t asmvalue_is_null;
	uint32_t accessor_index;
	size_t source_offset;
	size_t source_length;
};

struct orlix_tcti_register_system_selector {
	char *name;
	char *type;
	char *value;
	char *meaning;
	uint32_t value_kind;
	uint32_t payload_index;
	uint32_t value_expression;
	uint32_t encoding_index;
	size_t source_offset;
	size_t source_length;
};

enum orlix_tcti_register_selector_value_kind {
	ORLIX_TCTI_REGISTER_SELECTOR_VALUE_NONE,
	ORLIX_TCTI_REGISTER_SELECTOR_VALUE_LITERAL,
	ORLIX_TCTI_REGISTER_SELECTOR_VALUE_EQUATION,
	ORLIX_TCTI_REGISTER_SELECTOR_VALUE_GROUP,
};

struct orlix_tcti_register_selector_literal {
	uint32_t selector_index;
	size_t source_offset;
	size_t source_length;
};

struct orlix_tcti_register_selector_slice {
	uint32_t equation_index;
	uint32_t start;
	uint32_t width;
	size_t source_offset;
	size_t source_length;
};

struct orlix_tcti_register_selector_equation {
	uint32_t selector_index;
	char *identifier;
	uint32_t first_slice;
	uint32_t slice_count;
	size_t source_offset;
	size_t source_length;
};

struct orlix_tcti_register_selector_group {
	uint32_t selector_index;
	uint32_t first_fragment;
	uint32_t fragment_count;
	size_t source_offset;
	size_t source_length;
};

struct orlix_tcti_register_selector_group_fragment {
	uint32_t kind;
	uint32_t payload_index;
	size_t source_offset;
	size_t source_length;
};

struct orlix_tcti_register_expression {
	char *type;
	char *name;
	char *op;
	char *value;
	char *role;
	char *register_state;
	char *register_name;
	char *field_name;
	uint32_t first_child;
	uint32_t child_count;
	uint32_t scalar_kind;
	int64_t integer;
	uint32_t boolean;
	size_t instance_offset;
	size_t instance_length;
	size_t slices_offset;
	size_t slices_length;
	uint32_t parent_expression;
	size_t source_offset;
	size_t source_length;
};

struct orlix_tcti_register_valueset {
	char *type;
	uint32_t field_index;
	size_t source_offset;
	size_t source_length;
};

enum orlix_tcti_register_constraint_kind {
	ORLIX_TCTI_REGISTER_CONSTRAINT_NULL,
	ORLIX_TCTI_REGISTER_CONSTRAINT_VALUESET,
	ORLIX_TCTI_REGISTER_CONSTRAINT_ARRAY,
};

struct orlix_tcti_register_constraint {
	enum orlix_tcti_register_constraint_kind kind;
	uint32_t field_index;
	uint32_t valueset_index;
	uint32_t first_item;
	uint32_t item_count;
	size_t source_offset;
	size_t source_length;
};

struct orlix_tcti_register_constraint_item {
	uint32_t constraint_index;
	uint32_t valueset_index;
	uint32_t domain_index;
	size_t source_offset;
	size_t source_length;
};

/*
 * Direct, source-preserving bridge from a field to the declarations that
 * constrain its representable values.  The arrays retained elsewhere remain
 * the authoritative detailed trees.  This relation exists so a consumer does
 * not have to infer field ownership by scanning every Valueset, domain, and
 * constraint node.  It never selects a value or projects runtime HWCAP.
 */
enum orlix_tcti_register_field_value_source_kind {
	ORLIX_TCTI_REGISTER_FIELD_VALUE_SOURCE_NONE,
	ORLIX_TCTI_REGISTER_FIELD_VALUE_SOURCE_VALUES,
	ORLIX_TCTI_REGISTER_FIELD_VALUE_SOURCE_IMPLEMENTATION_DEFINED,
};

struct orlix_tcti_register_field_value_relation {
	uint32_t field_index;
	uint32_t first_value_candidate;
	uint32_t value_candidate_count;
	uint32_t first_constraint_candidate;
	uint32_t constraint_candidate_count;
	uint32_t field_condition_expression;
	uint32_t wrapper_condition_expression;
	size_t source_offset;
	size_t source_length;
};

struct orlix_tcti_register_field_value_candidate {
	uint32_t field_index;
	enum orlix_tcti_register_field_value_source_kind kind;
	uint32_t valueset_index;
	uint32_t first_domain;
	uint32_t domain_count;
	size_t source_offset;
	size_t source_length;
};

struct orlix_tcti_register_field_constraint_candidate {
	uint32_t field_index;
	uint32_t constraint_index;
	enum orlix_tcti_register_constraint_kind kind;
	uint32_t first_domain;
	uint32_t domain_count;
	size_t source_offset;
	size_t source_length;
};

struct orlix_tcti_register_link {
	char *key;
	char *value;
	uint32_t domain_index;
	size_t source_offset;
	size_t source_length;
};

struct orlix_tcti_register_model {
	struct orlix_tcti_register_identity *registers;
	size_t register_count;
	size_t register_capacity;
	struct orlix_tcti_register_metadata *metadata;
	size_t metadata_count;
	size_t metadata_capacity;
	struct orlix_tcti_register_memory_access *memory_accesses;
	size_t memory_access_count;
	size_t memory_access_capacity;
	struct orlix_tcti_register_implementation_defined_permission *implementation_defined_permissions;
	size_t implementation_defined_permission_count;
	size_t implementation_defined_permission_capacity;
	struct orlix_tcti_field_identity *fields;
	size_t field_count;
	size_t field_capacity;
	struct orlix_tcti_register_range *ranges;
	size_t range_capacity;
	struct orlix_tcti_conditional_field_branch *field_branches;
	size_t field_branch_count;
	size_t field_branch_capacity;
	struct orlix_tcti_register_fieldset *fieldsets;
	size_t fieldset_capacity;
	struct orlix_tcti_value_domain *domains;
	size_t domain_count;
	size_t top_level_domain_count;
	size_t domain_capacity;
	struct orlix_tcti_register_valueset *valuesets;
	size_t valueset_count;
	size_t top_level_valueset_count;
	size_t valueset_capacity;
	struct orlix_tcti_register_constraint *constraints;
	size_t constraint_count;
	size_t constraint_capacity;
	size_t constraint_domain_count;
	struct orlix_tcti_register_constraint_item *constraint_items;
	size_t constraint_item_count;
	size_t constraint_item_capacity;
	struct orlix_tcti_register_field_value_relation *field_value_relations;
	size_t field_value_relation_count;
	size_t field_value_relation_capacity;
	struct orlix_tcti_register_field_value_candidate *field_value_candidates;
	size_t field_value_candidate_count;
	size_t field_value_candidate_capacity;
	struct orlix_tcti_register_field_constraint_candidate *field_constraint_candidates;
	size_t field_constraint_candidate_count;
	size_t field_constraint_candidate_capacity;
	struct orlix_tcti_register_link *links;
	size_t link_count;
	size_t link_capacity;
	struct orlix_tcti_register_accessor *accessors;
	size_t accessor_count;
	size_t accessor_capacity;
	struct orlix_tcti_register_accessor_offset_expression *accessor_offset_expressions;
	size_t accessor_offset_expression_count;
	size_t accessor_offset_expression_capacity;
	struct orlix_tcti_register_system_encoding *system_encodings;
	size_t system_encoding_count;
	size_t system_encoding_capacity;
	struct orlix_tcti_register_system_selector *system_selectors;
	size_t system_selector_count;
	size_t system_selector_capacity;
	struct orlix_tcti_register_selector_literal *selector_literals;
	size_t selector_literal_count;
	size_t selector_literal_capacity;
	struct orlix_tcti_register_selector_equation *selector_equations;
	size_t selector_equation_count;
	size_t selector_equation_capacity;
	struct orlix_tcti_register_selector_slice *selector_slices;
	size_t selector_slice_count;
	size_t selector_slice_capacity;
	struct orlix_tcti_register_selector_group *selector_groups;
	size_t selector_group_count;
	size_t selector_group_capacity;
	struct orlix_tcti_register_selector_group_fragment *selector_group_fragments;
	size_t selector_group_fragment_count;
	size_t selector_group_fragment_capacity;
	struct orlix_tcti_register_expression *expressions;
	size_t expression_count;
	size_t expression_capacity;
	uint32_t *expression_children;
	size_t expression_child_count;
	size_t expression_child_capacity;
	char *source;
	size_t source_length;
	size_t fieldset_count;
	size_t range_count;
	size_t allocation_bytes;
	size_t allocation_high_water_bytes;
	size_t allocation_limit_bytes;
};

int orlix_tcti_register_model_import(const char *json, size_t length,
			       struct orlix_tcti_register_model *model,
			       struct orlix_tcti_register_model_error *error);
void orlix_tcti_register_model_destroy(struct orlix_tcti_register_model *model);

#endif
