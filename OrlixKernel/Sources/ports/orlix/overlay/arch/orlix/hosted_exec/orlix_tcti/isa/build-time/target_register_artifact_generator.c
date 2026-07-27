/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Convert the pinned Arm Registers.json typed model into a deterministic,
 * fixed-width C macro artifact. JSON parsing is confined to this explicit
 * refresh and audit tool.
 */
#include "target_register_artifact_generator.h"

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define INVALID_INDEX UINT32_MAX

static enum orlix_tcti_register_artifact_error model_error(
	const struct orlix_tcti_register_model_error *error)
{
	switch (error->code) {
	case ORLIX_TCTI_REGISTER_MODEL_OK:
		return ORLIX_TCTI_REGISTER_ARTIFACT_OK;
	case ORLIX_TCTI_REGISTER_MODEL_INVALID_ARGUMENT:
		return ORLIX_TCTI_REGISTER_ARTIFACT_INVALID_ARGUMENT;
	case ORLIX_TCTI_REGISTER_MODEL_INVALID_JSON:
	case ORLIX_TCTI_REGISTER_MODEL_INVALID_SOURCE:
		return ORLIX_TCTI_REGISTER_ARTIFACT_INVALID_SOURCE;
	case ORLIX_TCTI_REGISTER_MODEL_UNSUPPORTED_TYPE:
		return ORLIX_TCTI_REGISTER_ARTIFACT_UNSUPPORTED_GRAMMAR;
	case ORLIX_TCTI_REGISTER_MODEL_INPUT_LIMIT:
	case ORLIX_TCTI_REGISTER_MODEL_DEPTH_LIMIT:
		return ORLIX_TCTI_REGISTER_ARTIFACT_LIMIT;
	case ORLIX_TCTI_REGISTER_MODEL_NO_MEMORY:
		return ORLIX_TCTI_REGISTER_ARTIFACT_NO_MEMORY;
	case ORLIX_TCTI_REGISTER_MODEL_HASH_MISMATCH:
		return ORLIX_TCTI_REGISTER_ARTIFACT_PIN_MISMATCH;
	case ORLIX_TCTI_REGISTER_MODEL_COUNT_MISMATCH:
		return ORLIX_TCTI_REGISTER_ARTIFACT_COUNT_MISMATCH;
	}
	return ORLIX_TCTI_REGISTER_ARTIFACT_INVALID_SOURCE;
}

const char *orlix_tcti_target_register_artifact_error_name(
	enum orlix_tcti_register_artifact_error error)
{
	switch (error) {
	case ORLIX_TCTI_REGISTER_ARTIFACT_OK:
		return "success";
	case ORLIX_TCTI_REGISTER_ARTIFACT_INVALID_ARGUMENT:
		return "invalid argument";
	case ORLIX_TCTI_REGISTER_ARTIFACT_INVALID_SOURCE:
		return "invalid source";
	case ORLIX_TCTI_REGISTER_ARTIFACT_UNSUPPORTED_GRAMMAR:
		return "unsupported grammar";
	case ORLIX_TCTI_REGISTER_ARTIFACT_LIMIT:
		return "resource limit";
	case ORLIX_TCTI_REGISTER_ARTIFACT_NO_MEMORY:
		return "out of memory";
	case ORLIX_TCTI_REGISTER_ARTIFACT_PIN_MISMATCH:
		return "pin mismatch";
	case ORLIX_TCTI_REGISTER_ARTIFACT_COUNT_MISMATCH:
		return "count mismatch";
	case ORLIX_TCTI_REGISTER_ARTIFACT_UNRESOLVED_MODEL:
		return "unresolved typed model";
	case ORLIX_TCTI_REGISTER_ARTIFACT_IO:
		return "I/O failure";
	case ORLIX_TCTI_REGISTER_ARTIFACT_MISMATCH:
		return "artifact mismatch";
	}
	return "unknown failure";
}

static int span_valid(size_t offset, size_t length, size_t source_length)
{
	return offset <= source_length && length <= source_length - offset &&
		offset <= UINT32_MAX && length <= UINT32_MAX;
}

static int index_valid(uint32_t index, size_t count)
{
	return index == INVALID_INDEX || index < count;
}

static int range_valid(uint32_t first, uint32_t count, size_t total)
{
	if (!count)
		return first == INVALID_INDEX || first <= total;
	return first != INVALID_INDEX && first < total && count <= total - first;
}

static int text_is_one_of(const char *text, const char *const *values,
			  size_t value_count)
{
	size_t index;

	if (!text)
		return 0;
	for (index = 0; index < value_count; index++)
		if (!strcmp(text, values[index]))
			return 1;
	return 0;
}

static int register_kind_valid(const char *type)
{
	static const char *const types[] = {
		"Register", "RegisterArray", "RegisterBlock",
	};

	return text_is_one_of(type, types, sizeof(types) / sizeof(types[0]));
}

static int field_kind_valid(const char *type)
{
	static const char *const types[] = {
		"Fields.Array", "Fields.ConditionalField",
		"Fields.ConstantField", "Fields.Dynamic", "Fields.Field",
		"Fields.ImplementationDefined", "Fields.Reserved",
		"Fields.Vector",
	};

	return text_is_one_of(type, types, sizeof(types) / sizeof(types[0]));
}

static int valueset_kind_valid(const char *type)
{
	static const char *const types[] = {
		"Valuesets.Values", "Valuesets.ImplementationDefined",
	};

	return text_is_one_of(type, types, sizeof(types) / sizeof(types[0]));
}

static int domain_kind_valid(const char *type)
{
	static const char *const types[] = {
		"Values.Value", "Values.ValueRange", "Values.Link",
		"Values.ConditionalValue",
	};

	return text_is_one_of(type, types, sizeof(types) / sizeof(types[0]));
}

static int accessor_kind_valid(const char *type)
{
	static const char *const types[] = {
		"Accessors.SystemAccessor", "Accessors.SystemAccessorArray",
		"Accessors.MemoryMapped", "Accessors.BlockAccess",
		"Accessors.ExternalDebug", "Accessors.BlockAccessArray",
		"Accessors.ImplementationDefinedOffsetAccessorArray",
	};

	return text_is_one_of(type, types, sizeof(types) / sizeof(types[0]));
}

static int expression_kind_valid(const char *type)
{
	static const char *const types[] = {
		"Accessors.Permission.SystemAccess",
		"Accessors.Permission.MemoryAccess",
		"Accessors.Permission.AccessTypes.Memory.ReadWriteAccess",
		"Accessors.Permission.AccessTypes.Memory.ImplementationDefined",
		"AST.BinaryOp", "AST.Bool", "AST.Concat", "AST.DotAtom",
		"AST.Function", "AST.Identifier", "AST.Integer", "AST.Set",
		"AST.Slice", "AST.SquareOp", "AST.UnaryOp", "AST.Assignment",
		"AST.Return", "AST.Type", "AST.TypeAnnotation", "Types.Field",
		"Types.RegisterType", "Types.String", "Values.Value",
	};

	return text_is_one_of(type, types, sizeof(types) / sizeof(types[0]));
}

static int all_counts_fit(const struct orlix_tcti_register_model *model,
			  size_t source_length)
{
	const size_t counts[] = {
		model->register_count, model->metadata_count,
		model->memory_access_count,
		model->implementation_defined_permission_count,
		model->fieldset_count,
		model->field_count, model->range_count, model->field_branch_count,
		model->valueset_count, model->domain_count, model->constraint_count,
		model->constraint_item_count, model->link_count,
		model->accessor_count, model->accessor_offset_expression_count,
		model->system_encoding_count, model->system_selector_count,
		model->selector_literal_count, model->selector_equation_count,
		model->selector_slice_count, model->selector_group_count,
		model->selector_group_fragment_count, model->expression_count,
		model->expression_child_count,
	};
	size_t index;

	if (source_length > UINT32_MAX)
		return 0;
	for (index = 0; index < sizeof(counts) / sizeof(counts[0]); index++)
		if (counts[index] > UINT32_MAX)
			return 0;
	return 1;
}

static int required_tables_present(const struct orlix_tcti_register_model *model)
{
#define TABLE_PRESENT(member, count) (!(count) || (model->member != NULL))
	return TABLE_PRESENT(registers, model->register_count) &&
		TABLE_PRESENT(metadata, model->metadata_count) &&
		TABLE_PRESENT(memory_accesses, model->memory_access_count) &&
		TABLE_PRESENT(implementation_defined_permissions,
			      model->implementation_defined_permission_count) &&
		TABLE_PRESENT(fieldsets, model->fieldset_count) &&
		TABLE_PRESENT(fields, model->field_count) &&
		TABLE_PRESENT(ranges, model->range_count) &&
		TABLE_PRESENT(field_branches, model->field_branch_count) &&
		TABLE_PRESENT(valuesets, model->valueset_count) &&
		TABLE_PRESENT(domains, model->domain_count) &&
		TABLE_PRESENT(constraints, model->constraint_count) &&
		TABLE_PRESENT(constraint_items, model->constraint_item_count) &&
		TABLE_PRESENT(links, model->link_count) &&
		TABLE_PRESENT(accessors, model->accessor_count) &&
		TABLE_PRESENT(accessor_offset_expressions,
			      model->accessor_offset_expression_count) &&
		TABLE_PRESENT(system_encodings, model->system_encoding_count) &&
		TABLE_PRESENT(system_selectors, model->system_selector_count) &&
		TABLE_PRESENT(selector_literals, model->selector_literal_count) &&
		TABLE_PRESENT(selector_equations, model->selector_equation_count) &&
		TABLE_PRESENT(selector_slices, model->selector_slice_count) &&
		TABLE_PRESENT(selector_groups, model->selector_group_count) &&
		TABLE_PRESENT(selector_group_fragments,
			      model->selector_group_fragment_count) &&
		TABLE_PRESENT(expressions, model->expression_count) &&
		TABLE_PRESENT(expression_children, model->expression_child_count);
#undef TABLE_PRESENT
}

/*
 * This validates every relationship exported below. A model surface that has
 * no typed disposition is rejected before output rather than represented by a
 * fabricated default.
 */
static int model_resolved(const struct orlix_tcti_register_model *model,
			  size_t source_length)
{
	size_t index;

	if (!model->source || model->source_length != source_length ||
	    model->register_count != ORLIX_TCTI_REGISTER_RECORD_COUNT ||
	    model->metadata_count != ORLIX_TCTI_REGISTER_METADATA_COUNT ||
	    model->memory_access_count != ORLIX_TCTI_REGISTER_MEMORY_ACCESS_COUNT ||
	    model->implementation_defined_permission_count != 17U ||
	    model->fieldset_count != ORLIX_TCTI_REGISTER_FIELDSET_COUNT ||
	    model->field_count != ORLIX_TCTI_REGISTER_FIELD_COUNT ||
	    model->range_count != ORLIX_TCTI_REGISTER_RANGE_COUNT ||
	    model->top_level_valueset_count != ORLIX_TCTI_REGISTER_VALUESET_COUNT ||
	    model->top_level_domain_count != ORLIX_TCTI_REGISTER_DOMAIN_COUNT ||
	    model->accessor_count != ORLIX_TCTI_REGISTER_ACCESSOR_COUNT ||
	    !all_counts_fit(model, source_length) ||
	    !required_tables_present(model))
		return 0;

	for (index = 0; index < model->metadata_count; index++) {
		const struct orlix_tcti_register_metadata *item = &model->metadata[index];

		if (!item->copyright || !item->license_info ||
		    !item->architecture || !item->build || !item->ref ||
		    !item->schema || !item->timestamp ||
		    !span_valid(item->source_offset, item->source_length,
				source_length))
			return 0;
	}
	for (index = 0; index < model->register_count; index++) {
		const struct orlix_tcti_register_identity *item = &model->registers[index];

		if (!item->name || !register_kind_valid(item->type) ||
		    !index_valid(item->parent_register, model->register_count) ||
		    item->metadata_owner >= model->register_count ||
		    item->metadata_index >= model->metadata_count ||
		    !range_valid(item->first_fieldset, item->fieldset_count,
				 model->fieldset_count) ||
		    !span_valid(item->metadata_offset, item->metadata_length,
				source_length) ||
		    !span_valid(item->source_offset, item->source_length,
				source_length))
			return 0;
	}
	for (index = 0; index < model->memory_access_count; index++) {
		const struct orlix_tcti_register_memory_access *item =
			&model->memory_accesses[index];

		if ((unsigned int)item->form >
		    ORLIX_TCTI_REGISTER_MEMORY_ACCESS_LEGACY_SENTINEL ||
		    (unsigned int)item->read > ORLIX_TCTI_REGISTER_MEMORY_READ_ERROR ||
		    (unsigned int)item->write > ORLIX_TCTI_REGISTER_MEMORY_WRITE_ERROR ||
		    (unsigned int)item->read_origin >
		    ORLIX_TCTI_REGISTER_MEMORY_ACCESS_SCHEMA_DEFAULT ||
		    (unsigned int)item->write_origin >
		    ORLIX_TCTI_REGISTER_MEMORY_ACCESS_SCHEMA_DEFAULT ||
		    (unsigned int)item->legacy_sentinel >
		    ORLIX_TCTI_REGISTER_MEMORY_ACCESS_LEGACY_RESERVED_ERROR ||
		    item->owner_kind !=
		    ORLIX_TCTI_REGISTER_MEMORY_ACCESS_OWNER_EXPRESSION ||
		    item->owner_index >= model->expression_count ||
		    !model->expressions[item->owner_index].type ||
		    strcmp(model->expressions[item->owner_index].type,
			   "Accessors.Permission.AccessTypes.Memory."
			   "ReadWriteAccess") ||
		    !span_valid(item->read_offset, item->read_length, source_length) ||
		    !span_valid(item->write_offset, item->write_length,
				source_length) ||
		    !span_valid(item->source_offset, item->source_length,
				source_length))
			return 0;
	}
	for (index = 0;
	     index < model->implementation_defined_permission_count; index++) {
		const struct orlix_tcti_register_implementation_defined_permission *item =
			&model->implementation_defined_permissions[index];
		size_t child;

		if ((unsigned int)item->kind >
		    ORLIX_TCTI_REGISTER_IMPLEMENTATION_DEFINED_CONSTRAINT_ARRAY ||
		    item->expression_index >= model->expression_count ||
		    !model->expressions[item->expression_index].type ||
		    strcmp(model->expressions[item->expression_index].type,
			   "Accessors.Permission.AccessTypes.Memory."
			   "ImplementationDefined") ||
		    !range_valid(item->first_memory_access,
				 item->memory_access_count,
				 model->memory_access_count) ||
		    !span_valid(item->source_offset, item->source_length,
				source_length))
			return 0;
		for (child = 0; child < item->memory_access_count; child++) {
			const struct orlix_tcti_register_memory_access *access =
				&model->memory_accesses[
					item->first_memory_access + child];
			const struct orlix_tcti_register_expression *owner;

			if (access->owner_kind !=
			    ORLIX_TCTI_REGISTER_MEMORY_ACCESS_OWNER_EXPRESSION ||
			    access->owner_index >= model->expression_count)
				return 0;
			owner = &model->expressions[access->owner_index];
			if (owner->parent_expression != item->expression_index)
				return 0;
		}
	}
	for (index = 0; index < model->fieldset_count; index++) {
		const struct orlix_tcti_register_fieldset *item = &model->fieldsets[index];

		if (item->register_index >= model->register_count ||
		    !index_valid(item->condition_expression,
				 model->expression_count) ||
		    !span_valid(item->condition_offset, item->condition_length,
				source_length) ||
		    !span_valid(item->source_offset, item->source_length,
				source_length))
			return 0;
	}
	for (index = 0; index < model->field_count; index++) {
		const struct orlix_tcti_field_identity *item = &model->fields[index];

		if (!field_kind_valid(item->type) ||
		    item->register_index >= model->register_count ||
		    !index_valid(item->parent_field, model->field_count) ||
		    item->fieldset_index >= model->fieldset_count ||
		    !range_valid(item->first_range, item->range_count,
				 model->range_count) ||
		    !index_valid(item->condition_expression,
				 model->expression_count) ||
		    !index_valid(item->wrapper_condition_expression,
				 model->expression_count) ||
		    !span_valid(item->condition_offset, item->condition_length,
				source_length) ||
		    !span_valid(item->wrapper_condition_offset,
				item->wrapper_condition_length, source_length) ||
		    !span_valid(item->branch_offset, item->branch_length,
				source_length) ||
		    !span_valid(item->source_offset, item->source_length,
				source_length))
			return 0;
	}
	for (index = 0; index < model->range_count; index++) {
		const struct orlix_tcti_register_range *item = &model->ranges[index];

		if (!item->width || item->field_index >= model->field_count ||
		    !span_valid(item->source_offset, item->source_length,
				source_length))
			return 0;
	}
	for (index = 0; index < model->field_branch_count; index++) {
		const struct orlix_tcti_conditional_field_branch *item =
			&model->field_branches[index];

		if (item->conditional_field_index >= model->field_count ||
		    item->field_index >= model->field_count ||
		    item->condition_expression >= model->expression_count ||
		    !span_valid(item->condition_offset, item->condition_length,
				source_length) ||
		    !span_valid(item->source_offset, item->source_length,
				source_length))
			return 0;
	}
	for (index = 0; index < model->valueset_count; index++) {
		const struct orlix_tcti_register_valueset *item = &model->valuesets[index];

		if (!valueset_kind_valid(item->type) ||
		    !index_valid(item->field_index, model->field_count) ||
		    !span_valid(item->source_offset, item->source_length,
				source_length))
			return 0;
	}
	for (index = 0; index < model->domain_count; index++) {
		const struct orlix_tcti_value_domain *item = &model->domains[index];

		if (!domain_kind_valid(item->type) ||
		    !index_valid(item->field_index, model->field_count) ||
		    !index_valid(item->parent_domain, model->domain_count) ||
		    !index_valid(item->condition_expression,
				 model->expression_count) ||
		    !range_valid(item->first_child_domain,
				 item->child_domain_count, model->domain_count) ||
		    item->valueset_index >= model->valueset_count ||
		    !index_valid(item->nested_valueset_index,
				 model->valueset_count) ||
		    !span_valid(item->condition_offset, item->condition_length,
				source_length) ||
		    !span_valid(item->branch_offset, item->branch_length,
				source_length) ||
		    !span_valid(item->source_offset, item->source_length,
				source_length))
			return 0;
	}
	for (index = 0; index < model->constraint_count; index++) {
		const struct orlix_tcti_register_constraint *item =
			&model->constraints[index];

		if ((unsigned int)item->kind > ORLIX_TCTI_REGISTER_CONSTRAINT_ARRAY ||
		    !index_valid(item->field_index, model->field_count) ||
		    !index_valid(item->valueset_index, model->valueset_count) ||
		    !range_valid(item->first_item, item->item_count,
				 model->constraint_item_count) ||
		    !span_valid(item->source_offset, item->source_length,
				source_length))
			return 0;
	}
	for (index = 0; index < model->constraint_item_count; index++) {
		const struct orlix_tcti_register_constraint_item *item =
			&model->constraint_items[index];

		if (item->constraint_index >= model->constraint_count ||
		    !index_valid(item->valueset_index, model->valueset_count) ||
		    !index_valid(item->domain_index, model->domain_count) ||
		    !span_valid(item->source_offset, item->source_length,
				source_length))
			return 0;
	}
	for (index = 0; index < model->link_count; index++) {
		const struct orlix_tcti_register_link *item = &model->links[index];

		if (!item->key || !item->value ||
		    item->domain_index >= model->domain_count ||
		    !span_valid(item->source_offset, item->source_length,
				source_length))
			return 0;
	}
	for (index = 0; index < model->accessor_count; index++) {
		const struct orlix_tcti_register_accessor *item = &model->accessors[index];

		if (!accessor_kind_valid(item->type) ||
		    item->register_index >= model->register_count ||
		    !range_valid(item->first_offset_expression,
				 item->offset_expression_count,
				 model->accessor_offset_expression_count) ||
		    !index_valid(item->references_expression,
				 model->expression_count) ||
		    !range_valid(item->first_system_encoding,
				 item->system_encoding_count,
				 model->system_encoding_count) ||
		    !index_valid(item->condition_expression,
				 model->expression_count) ||
		    !index_valid(item->access_expression,
				 model->expression_count) ||
		    !span_valid(item->condition_offset, item->condition_length,
				source_length) ||
		    !span_valid(item->access_offset, item->access_length,
				source_length) ||
		    !span_valid(item->encoding_offset, item->encoding_length,
				source_length) ||
		    !span_valid(item->source_offset, item->source_length,
				source_length))
			return 0;
	}
	for (index = 0; index < model->accessor_offset_expression_count; index++) {
		const struct orlix_tcti_register_accessor_offset_expression *item =
			&model->accessor_offset_expressions[index];

		if (item->accessor_index >= model->accessor_count ||
		    item->expression_index >= model->expression_count ||
		    !span_valid(item->source_offset, item->source_length,
				source_length))
			return 0;
	}
	for (index = 0; index < model->system_encoding_count; index++) {
		const struct orlix_tcti_register_system_encoding *item =
			&model->system_encodings[index];

		if (!item->type || strcmp(item->type, "Encoding") ||
		    !item->asmvalue ||
		    item->accessor_index >= model->accessor_count ||
		    item->asmvalue_is_null > 1U ||
		    !span_valid(item->source_offset, item->source_length,
				source_length))
			return 0;
	}
	for (index = 0; index < model->system_selector_count; index++) {
		const struct orlix_tcti_register_system_selector *item =
			&model->system_selectors[index];

		if (!item->name ||
		    (!strcmp(item->type ? item->type : "", "Values.Value") &&
		     item->value_kind != ORLIX_TCTI_REGISTER_SELECTOR_VALUE_LITERAL) ||
		    (!strcmp(item->type ? item->type : "",
			     "Values.EquationValue") &&
		     item->value_kind != ORLIX_TCTI_REGISTER_SELECTOR_VALUE_EQUATION) ||
		    (!strcmp(item->type ? item->type : "", "Values.Group") &&
		     item->value_kind != ORLIX_TCTI_REGISTER_SELECTOR_VALUE_GROUP) ||
		    (!item->type ||
		     (strcmp(item->type, "Values.Value") &&
		      strcmp(item->type, "Values.EquationValue") &&
		      strcmp(item->type, "Values.Group"))) ||
		    !item->value ||
		    item->value_kind > ORLIX_TCTI_REGISTER_SELECTOR_VALUE_GROUP ||
		    item->encoding_index >= model->system_encoding_count ||
		    !index_valid(item->value_expression, model->expression_count) ||
		    !span_valid(item->source_offset, item->source_length,
				source_length))
			return 0;
		switch (item->value_kind) {
		case ORLIX_TCTI_REGISTER_SELECTOR_VALUE_NONE:
			if (item->payload_index != INVALID_INDEX)
				return 0;
			break;
		case ORLIX_TCTI_REGISTER_SELECTOR_VALUE_LITERAL:
			if (item->payload_index >= model->selector_literal_count)
				return 0;
			break;
		case ORLIX_TCTI_REGISTER_SELECTOR_VALUE_EQUATION:
			if (item->payload_index >= model->selector_equation_count)
				return 0;
			break;
		case ORLIX_TCTI_REGISTER_SELECTOR_VALUE_GROUP:
			if (item->payload_index >= model->selector_group_count)
				return 0;
			break;
		}
	}
	for (index = 0; index < model->selector_literal_count; index++) {
		const struct orlix_tcti_register_selector_literal *item =
			&model->selector_literals[index];

		if (item->selector_index >= model->system_selector_count ||
		    !span_valid(item->source_offset, item->source_length,
				source_length))
			return 0;
	}
	for (index = 0; index < model->selector_equation_count; index++) {
		const struct orlix_tcti_register_selector_equation *item =
			&model->selector_equations[index];

		if (!item->identifier ||
		    item->selector_index >= model->system_selector_count ||
		    !range_valid(item->first_slice, item->slice_count,
				 model->selector_slice_count) ||
		    !span_valid(item->source_offset, item->source_length,
				source_length))
			return 0;
	}
	for (index = 0; index < model->selector_slice_count; index++) {
		const struct orlix_tcti_register_selector_slice *item =
			&model->selector_slices[index];

		if (item->equation_index >= model->selector_equation_count ||
		    !item->width ||
		    !span_valid(item->source_offset, item->source_length,
				source_length))
			return 0;
	}
	for (index = 0; index < model->selector_group_count; index++) {
		const struct orlix_tcti_register_selector_group *item =
			&model->selector_groups[index];

		if (item->selector_index >= model->system_selector_count ||
		    !range_valid(item->first_fragment, item->fragment_count,
				 model->selector_group_fragment_count) ||
		    !span_valid(item->source_offset, item->source_length,
				source_length))
			return 0;
	}
	for (index = 0; index < model->selector_group_fragment_count; index++) {
		const struct orlix_tcti_register_selector_group_fragment *item =
			&model->selector_group_fragments[index];

		if (item->kind < ORLIX_TCTI_REGISTER_SELECTOR_VALUE_LITERAL ||
		    item->kind > ORLIX_TCTI_REGISTER_SELECTOR_VALUE_GROUP ||
		    !span_valid(item->source_offset, item->source_length,
				source_length))
			return 0;
		if ((item->kind == ORLIX_TCTI_REGISTER_SELECTOR_VALUE_LITERAL &&
		     item->payload_index >= model->selector_literal_count) ||
		    (item->kind == ORLIX_TCTI_REGISTER_SELECTOR_VALUE_EQUATION &&
		     item->payload_index >= model->selector_equation_count) ||
		    (item->kind == ORLIX_TCTI_REGISTER_SELECTOR_VALUE_GROUP &&
		     item->payload_index >= model->selector_group_count))
			return 0;
	}
	for (index = 0; index < model->expression_count; index++) {
		const struct orlix_tcti_register_expression *item =
			&model->expressions[index];

		if (!expression_kind_valid(item->type) ||
		    !range_valid(item->first_child, item->child_count,
				 model->expression_child_count) ||
		    !index_valid(item->parent_expression, model->expression_count) ||
		    !span_valid(item->instance_offset, item->instance_length,
				source_length) ||
		    !span_valid(item->slices_offset, item->slices_length,
				source_length) ||
		    !span_valid(item->source_offset, item->source_length,
				source_length))
			return 0;
	}
	for (index = 0; index < model->expression_child_count; index++)
		if (model->expression_children[index] >= model->expression_count)
			return 0;
	return 1;
}

/* Octal escapes are fixed width, unlike hexadecimal C escapes. */
static int emit_c_string(FILE *output, const char *text)
{
	const unsigned char *cursor = (const unsigned char *)text;

	if (fputc('"', output) == EOF)
		return -1;
	while (*cursor) {
		if (*cursor == '"' || *cursor == '\\') {
			if (fputc('\\', output) == EOF ||
			    fputc((int)*cursor, output) == EOF)
				return -1;
		} else if (*cursor >= 0x20U && *cursor <= 0x7eU) {
			if (fputc((int)*cursor, output) == EOF)
				return -1;
		} else if (fprintf(output, "\\%03o",
				   (unsigned int)*cursor) < 0) {
			return -1;
		}
		cursor++;
	}
	return fputc('"', output) == EOF ? -1 : 0;
}

static int emit_optional(FILE *output, const char *text)
{
	if (fprintf(output, "%uU, ", text ? 1U : 0U) < 0)
		return -1;
	return emit_c_string(output, text ? text : "");
}

static int emit_span(FILE *output, size_t offset, size_t length)
{
	return fprintf(output, "%" PRIu32 "U, %" PRIu32 "U",
		       (uint32_t)offset, (uint32_t)length) < 0 ? -1 : 0;
}

static int comma_optional(FILE *output, const char *text)
{
	return fputs(", ", output) == EOF || emit_optional(output, text) ? -1 : 0;
}

static int comma_span(FILE *output, size_t offset, size_t length)
{
	return fputs(", ", output) == EOF || emit_span(output, offset, length) ?
		-1 : 0;
}

static int emit_artifact(FILE *output,
			 const struct orlix_tcti_register_model *model,
			 size_t source_length)
{
	size_t index;

	if (fputs("/* SPDX-License-Identifier: BSD-3-Clause */\n"
		  "/* Generated by target_register_artifact_generator.c. Do not edit. */\n"
		  "/* TREG tags are the lossless, versioned Arm register source tables. */\n"
		  "/* META metadata, REG register, MEM memory permission, IMPL implementation-defined. */\n"
		  "/* FS/F/RANGE/FB fields, VS/DOM/CON/CI/LINK value topology. */\n"
		  "/* ACC/AO accessors, ENC/SEL/LIT/EQ/SLICE/GRP/GF system selectors. */\n"
		  "/* E/EC typed expressions and their ordered child edges. */\n"
		  "TREG_SRC("
		  "\"vFATAp1-A\", \"818\", \"2026-06_rel\", \"2.9.5\", "
		  "\"2026-06-24 17:12:14\", "
		  "\"5bd76c3c3ce90322eb4fd179675dafe82df2fd1cb789beee516e5b29c471b874\", ",
		  output) == EOF ||
	    fprintf(output, "%zuU)\n", source_length) < 0 ||
	    fprintf(output,
		    "TREG_COUNTS("
		    "%zuU, %zuU, %zuU, %zuU, %zuU, %zuU, %zuU, %zuU, "
		    "%zuU, %zuU, %zuU, %zuU, %zuU, %zuU, %zuU, %zuU, "
		    "%zuU, %zuU, %zuU, %zuU, %zuU, %zuU, %zuU, %zuU)\n",
		    model->register_count, model->metadata_count,
		    model->memory_access_count,
		    model->implementation_defined_permission_count,
		    model->fieldset_count,
		    model->field_count, model->range_count,
		    model->field_branch_count, model->valueset_count,
		    model->domain_count, model->constraint_count,
		    model->constraint_item_count, model->link_count,
		    model->accessor_count,
		    model->accessor_offset_expression_count,
		    model->system_encoding_count, model->system_selector_count,
		    model->selector_literal_count, model->selector_equation_count,
		    model->selector_slice_count, model->selector_group_count,
		    model->selector_group_fragment_count, model->expression_count,
		    model->expression_child_count) < 0)
		return -1;

	for (index = 0; index < model->metadata_count; index++) {
		const struct orlix_tcti_register_metadata *item = &model->metadata[index];

		if (fprintf(output, "TREG_META(%zuU, ", index) < 0 ||
		    emit_c_string(output, item->copyright) ||
		    fputs(", ", output) == EOF ||
		    emit_c_string(output, item->license_info) ||
		    fputs(", ", output) == EOF ||
		    emit_c_string(output, item->architecture) ||
		    fputs(", ", output) == EOF ||
		    emit_c_string(output, item->build) ||
		    fputs(", ", output) == EOF ||
		    emit_c_string(output, item->ref) ||
		    fputs(", ", output) == EOF ||
		    emit_c_string(output, item->schema) ||
		    fputs(", ", output) == EOF ||
		    emit_c_string(output, item->timestamp) ||
		    comma_span(output, item->source_offset, item->source_length) ||
		    fputs(")\n", output) == EOF)
			return -1;
	}
	for (index = 0; index < model->register_count; index++) {
		const struct orlix_tcti_register_identity *item = &model->registers[index];

		if (fprintf(output, "TREG_REG(%zuU, ", index) < 0 ||
		    emit_c_string(output, item->name) ||
		    fputs(", ", output) == EOF ||
		    emit_c_string(output, item->type) ||
		    comma_optional(output, item->state) ||
		    comma_optional(output, item->index_variable) ||
		    fprintf(output, ", %" PRIu32 "U, %" PRIu32 "U, %" PRIu32
			    "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, ",
			    item->parent_register, item->metadata_owner,
			    item->metadata_index, item->default_access_expression,
			    item->first_fieldset,
			    item->fieldset_count) < 0 ||
		    emit_span(output, item->metadata_offset, item->metadata_length) ||
		    comma_span(output, item->source_offset, item->source_length) ||
		    fputs(")\n", output) == EOF)
			return -1;
	}
	for (index = 0; index < model->memory_access_count; index++) {
		const struct orlix_tcti_register_memory_access *item =
			&model->memory_accesses[index];

		if (fprintf(output,
			    "TREG_MEM(%zuU, %uU, %uU, "
			    "%uU, %uU, %uU, %uU, %uU, %" PRIu32 "U, ",
			    index, (unsigned int)item->form,
			    (unsigned int)item->read, (unsigned int)item->write,
			    (unsigned int)item->read_origin,
			    (unsigned int)item->write_origin,
			    (unsigned int)item->legacy_sentinel,
			    (unsigned int)item->owner_kind,
			    item->owner_index) < 0 ||
		    emit_span(output, item->read_offset, item->read_length) ||
		    comma_span(output, item->write_offset, item->write_length) ||
		    comma_span(output, item->source_offset, item->source_length) ||
		    fputs(")\n", output) == EOF)
			return -1;
	}
	for (index = 0;
	     index < model->implementation_defined_permission_count; index++) {
		const struct orlix_tcti_register_implementation_defined_permission *item =
			&model->implementation_defined_permissions[index];

		if (fprintf(output,
			    "TREG_IMPL("
			    "%zuU, %uU, %" PRIu32 "U, %" PRIu32 "U, %"
			    PRIu32 "U, ",
			    index, (unsigned int)item->kind,
			    item->expression_index, item->first_memory_access,
			    item->memory_access_count) < 0 ||
		    emit_span(output, item->source_offset, item->source_length) ||
		    fputs(")\n", output) == EOF)
			return -1;
	}
	for (index = 0; index < model->fieldset_count; index++) {
		const struct orlix_tcti_register_fieldset *item = &model->fieldsets[index];

		if (fprintf(output,
			    "TREG_FS(%zuU, %" PRIu32
			    "U, %" PRIu32 "U, ", index, item->register_index,
			    item->width) < 0 ||
		    emit_optional(output, item->name) ||
		    comma_optional(output, item->display) ||
		    fprintf(output, ", %" PRIu32 "U, ",
			    item->condition_expression) < 0 ||
		    emit_span(output, item->condition_offset,
			      item->condition_length) ||
		    comma_span(output, item->source_offset, item->source_length) ||
		    fputs(")\n", output) == EOF)
			return -1;
	}
	for (index = 0; index < model->field_count; index++) {
		const struct orlix_tcti_field_identity *item = &model->fields[index];

		if (fprintf(output, "TREG_F(%zuU, ", index) < 0 ||
		    emit_optional(output, item->name) ||
		    fputs(", ", output) == EOF ||
		    emit_c_string(output, item->type) ||
		    fprintf(output,
			    ", %" PRIu32 "U, %" PRIu32 "U, %" PRIu32
			    "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32
			    "U, %" PRIu32 "U, %" PRIu32 "U, ",
			    item->width, item->register_index, item->parent_field,
			    item->fieldset_index, item->first_range,
			    item->range_count, item->condition_expression,
			    item->wrapper_condition_expression) < 0 ||
		    emit_span(output, item->condition_offset,
			      item->condition_length) ||
		    comma_span(output, item->wrapper_condition_offset,
			       item->wrapper_condition_length) ||
		    comma_span(output, item->branch_offset, item->branch_length) ||
		    comma_span(output, item->source_offset, item->source_length) ||
		    fputs(")\n", output) == EOF)
			return -1;
	}
	for (index = 0; index < model->range_count; index++) {
		const struct orlix_tcti_register_range *item = &model->ranges[index];

		if (fprintf(output,
			    "TREG_RANGE(%zuU, %" PRIu32
			    "U, %" PRIu32 "U, %" PRIu32 "U, ",
			    index, item->start, item->width, item->field_index) < 0 ||
		    emit_span(output, item->source_offset, item->source_length) ||
		    fputs(")\n", output) == EOF)
			return -1;
	}
	for (index = 0; index < model->field_branch_count; index++) {
		const struct orlix_tcti_conditional_field_branch *item =
			&model->field_branches[index];

		if (fprintf(output,
			    "TREG_FB(%zuU, %" PRIu32
			    "U, %" PRIu32 "U, %" PRIu32 "U, ",
			    index, item->conditional_field_index, item->field_index,
			    item->condition_expression) < 0 ||
		    emit_span(output, item->condition_offset,
			      item->condition_length) ||
		    comma_span(output, item->source_offset, item->source_length) ||
		    fputs(")\n", output) == EOF)
			return -1;
	}
	for (index = 0; index < model->valueset_count; index++) {
		const struct orlix_tcti_register_valueset *item = &model->valuesets[index];

		if (fprintf(output, "TREG_VS(%zuU, ", index) < 0 ||
		    emit_c_string(output, item->type) ||
		    fprintf(output, ", %" PRIu32 "U, ", item->field_index) < 0 ||
		    emit_span(output, item->source_offset, item->source_length) ||
		    fputs(")\n", output) == EOF)
			return -1;
	}
	for (index = 0; index < model->domain_count; index++) {
		const struct orlix_tcti_value_domain *item = &model->domains[index];

		if (fprintf(output, "TREG_DOM(%zuU, ", index) < 0 ||
		    emit_c_string(output, item->type) ||
		    comma_optional(output, item->value) ||
		    comma_optional(output, item->start) ||
		    comma_optional(output, item->end) ||
		    comma_optional(output, item->link) ||
		    comma_optional(output, item->meaning) ||
		    fprintf(output,
			    ", %" PRIu32 "U, %" PRIu32 "U, %" PRIu32
			    "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32
			    "U, %" PRIu32 "U, ",
			    item->field_index, item->parent_domain,
			    item->condition_expression, item->first_child_domain,
			    item->child_domain_count, item->valueset_index,
			    item->nested_valueset_index) < 0 ||
		    emit_span(output, item->condition_offset,
			      item->condition_length) ||
		    comma_span(output, item->branch_offset, item->branch_length) ||
		    comma_span(output, item->source_offset, item->source_length) ||
		    fputs(")\n", output) == EOF)
			return -1;
	}
	for (index = 0; index < model->constraint_count; index++) {
		const struct orlix_tcti_register_constraint *item =
			&model->constraints[index];

		if (fprintf(output,
			    "TREG_CON(%zuU, %uU, %" PRIu32
			    "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, ",
			    index, (unsigned int)item->kind, item->field_index,
			    item->valueset_index, item->first_item,
			    item->item_count) < 0 ||
		    emit_span(output, item->source_offset, item->source_length) ||
		    fputs(")\n", output) == EOF)
			return -1;
	}
	for (index = 0; index < model->constraint_item_count; index++) {
		const struct orlix_tcti_register_constraint_item *item =
			&model->constraint_items[index];

		if (fprintf(output,
			    "TREG_CI(%zuU, %" PRIu32
			    "U, %" PRIu32 "U, %" PRIu32 "U, ",
			    index, item->constraint_index, item->valueset_index,
			    item->domain_index) < 0 ||
		    emit_span(output, item->source_offset, item->source_length) ||
		    fputs(")\n", output) == EOF)
			return -1;
	}
	for (index = 0; index < model->link_count; index++) {
		const struct orlix_tcti_register_link *item = &model->links[index];

		if (fprintf(output, "TREG_LINK(%zuU, ", index) < 0 ||
		    emit_c_string(output, item->key) ||
		    fputs(", ", output) == EOF ||
		    emit_c_string(output, item->value) ||
		    fprintf(output, ", %" PRIu32 "U, ", item->domain_index) < 0 ||
		    emit_span(output, item->source_offset, item->source_length) ||
		    fputs(")\n", output) == EOF)
			return -1;
	}
	for (index = 0; index < model->accessor_count; index++) {
		const struct orlix_tcti_register_accessor *item = &model->accessors[index];

		if (fprintf(output, "TREG_ACC(%zuU, ", index) < 0 ||
		    emit_c_string(output, item->type) ||
		    comma_optional(output, item->name) ||
		    comma_optional(output, item->index_variable) ||
		    comma_optional(output, item->component) ||
		    comma_optional(output, item->frame) ||
		    comma_optional(output, item->instance) ||
		    comma_optional(output, item->power_domain) ||
		    fprintf(output,
			    ", %" PRIu32 "U, %" PRIu32 "U, %" PRIu32
			    "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32
			    "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32
			    "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32
			    "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32
			    "U, %" PRIu32 "U, %" PRIu32 "U, ",
			    item->register_index, item->index_start,
			    item->index_width, item->range_start,
			    item->range_width, item->range_is_null,
			    item->component_is_null, item->frame_is_null,
			    item->instance_is_null, item->power_domain_is_null,
			    item->first_offset_expression,
			    item->offset_expression_count,
			    item->references_expression,
			    item->first_system_encoding,
			    item->system_encoding_count,
			    item->condition_expression,
			    item->access_expression) < 0 ||
		    emit_span(output, item->condition_offset,
			      item->condition_length) ||
		    comma_span(output, item->access_offset, item->access_length) ||
		    comma_span(output, item->encoding_offset, item->encoding_length) ||
		    comma_span(output, item->source_offset, item->source_length) ||
		    fputs(")\n", output) == EOF)
			return -1;
	}
	for (index = 0; index < model->accessor_offset_expression_count; index++) {
		const struct orlix_tcti_register_accessor_offset_expression *item =
			&model->accessor_offset_expressions[index];

		if (fprintf(output,
			    "TREG_AO(%zuU, %" PRIu32
			    "U, %" PRIu32 "U, ", index, item->accessor_index,
			    item->expression_index) < 0 ||
		    emit_span(output, item->source_offset, item->source_length) ||
		    fputs(")\n", output) == EOF)
			return -1;
	}
	for (index = 0; index < model->system_encoding_count; index++) {
		const struct orlix_tcti_register_system_encoding *item =
			&model->system_encodings[index];

		if (fprintf(output,
			    "TREG_ENC(%zuU, ", index) < 0 ||
		    emit_c_string(output, item->type) ||
		    fputs(", ", output) == EOF ||
		    emit_c_string(output, item->asmvalue) ||
		    fprintf(output, ", %" PRIu32 "U, %" PRIu32 "U, ",
			    item->asmvalue_is_null, item->accessor_index) < 0 ||
		    emit_span(output, item->source_offset, item->source_length) ||
		    fputs(")\n", output) == EOF)
			return -1;
	}
	for (index = 0; index < model->system_selector_count; index++) {
		const struct orlix_tcti_register_system_selector *item =
			&model->system_selectors[index];

		if (fprintf(output, "TREG_SEL(%zuU, ",
			    index) < 0 ||
		    emit_c_string(output, item->name) ||
		    fputs(", ", output) == EOF ||
		    emit_c_string(output, item->type) ||
		    fputs(", ", output) == EOF ||
		    emit_c_string(output, item->value) ||
		    comma_optional(output, item->meaning) ||
		    fprintf(output,
			    ", %" PRIu32 "U, %" PRIu32 "U, %" PRIu32
			    "U, %" PRIu32 "U, ",
			    item->value_kind, item->payload_index,
			    item->value_expression, item->encoding_index) < 0 ||
		    emit_span(output, item->source_offset, item->source_length) ||
		    fputs(")\n", output) == EOF)
			return -1;
	}
	for (index = 0; index < model->selector_literal_count; index++) {
		const struct orlix_tcti_register_selector_literal *item =
			&model->selector_literals[index];

		if (fprintf(output,
			    "TREG_LIT(%zuU, %" PRIu32
			    "U, ", index, item->selector_index) < 0 ||
		    emit_span(output, item->source_offset, item->source_length) ||
		    fputs(")\n", output) == EOF)
			return -1;
	}
	for (index = 0; index < model->selector_equation_count; index++) {
		const struct orlix_tcti_register_selector_equation *item =
			&model->selector_equations[index];

		if (fprintf(output,
			    "TREG_EQ(%zuU, %" PRIu32
			    "U, ", index, item->selector_index) < 0 ||
		    emit_c_string(output, item->identifier) ||
		    fprintf(output, ", %" PRIu32 "U, %" PRIu32 "U, ",
			    item->first_slice, item->slice_count) < 0 ||
		    emit_span(output, item->source_offset, item->source_length) ||
		    fputs(")\n", output) == EOF)
			return -1;
	}
	for (index = 0; index < model->selector_slice_count; index++) {
		const struct orlix_tcti_register_selector_slice *item =
			&model->selector_slices[index];

		if (fprintf(output,
			    "TREG_SLICE(%zuU, %" PRIu32
			    "U, %" PRIu32 "U, %" PRIu32 "U, ",
			    index, item->equation_index, item->start,
			    item->width) < 0 ||
		    emit_span(output, item->source_offset, item->source_length) ||
		    fputs(")\n", output) == EOF)
			return -1;
	}
	for (index = 0; index < model->selector_group_count; index++) {
		const struct orlix_tcti_register_selector_group *item =
			&model->selector_groups[index];

		if (fprintf(output,
			    "TREG_GRP(%zuU, %" PRIu32
			    "U, %" PRIu32 "U, %" PRIu32 "U, ",
			    index, item->selector_index, item->first_fragment,
			    item->fragment_count) < 0 ||
		    emit_span(output, item->source_offset, item->source_length) ||
		    fputs(")\n", output) == EOF)
			return -1;
	}
	for (index = 0; index < model->selector_group_fragment_count; index++) {
		const struct orlix_tcti_register_selector_group_fragment *item =
			&model->selector_group_fragments[index];

		if (fprintf(output,
			    "TREG_GF(%zuU, "
			    "%" PRIu32 "U, %" PRIu32 "U, ",
			    index, item->kind, item->payload_index) < 0 ||
		    emit_span(output, item->source_offset, item->source_length) ||
		    fputs(")\n", output) == EOF)
			return -1;
	}
	for (index = 0; index < model->expression_count; index++) {
		const struct orlix_tcti_register_expression *item =
			&model->expressions[index];

		if (fprintf(output, "TREG_E(%zuU, ", index) < 0 ||
		    emit_c_string(output, item->type) ||
		    comma_optional(output, item->name) ||
		    comma_optional(output, item->op) ||
		    comma_optional(output, item->value) ||
		    comma_optional(output, item->role) ||
		    comma_optional(output, item->register_state) ||
		    comma_optional(output, item->register_name) ||
		    comma_optional(output, item->field_name) ||
		    fprintf(output,
			    ", %" PRIu32 "U, %" PRIu32 "U, %" PRIu32
			    "U, %" PRId64 ", %" PRIu32 "U, %" PRIu32 "U, ",
			    item->first_child, item->child_count,
			    item->scalar_kind, item->integer, item->boolean,
			    item->parent_expression) < 0 ||
		    emit_span(output, item->instance_offset,
			      item->instance_length) ||
		    comma_span(output, item->slices_offset, item->slices_length) ||
		    comma_span(output, item->source_offset, item->source_length) ||
		    fputs(")\n", output) == EOF)
			return -1;
	}
	for (index = 0; index < model->expression_child_count; index++)
		if (fprintf(output,
			    "TREG_EC(%zuU, %" PRIu32
			    "U)\n", index, model->expression_children[index]) < 0)
			return -1;
	return ferror(output) ? -1 : 0;
}

enum orlix_tcti_register_artifact_error
orlix_tcti_target_register_artifact_emit_model(
	const struct orlix_tcti_register_model *model, size_t source_length, FILE *output)
{
	if (!model || !output || !source_length)
		return ORLIX_TCTI_REGISTER_ARTIFACT_INVALID_ARGUMENT;
	if (!model_resolved(model, source_length))
		return ORLIX_TCTI_REGISTER_ARTIFACT_UNRESOLVED_MODEL;
	return emit_artifact(output, model, source_length) ?
		ORLIX_TCTI_REGISTER_ARTIFACT_IO : ORLIX_TCTI_REGISTER_ARTIFACT_OK;
}

enum orlix_tcti_register_artifact_error orlix_tcti_target_register_artifact_emit(
	const char *source, size_t length, FILE *output)
{
	struct orlix_tcti_register_model model = { 0 };
	struct orlix_tcti_register_model_error error = { 0 };
	enum orlix_tcti_register_artifact_error result;

	if (!source || !length || !output)
		return ORLIX_TCTI_REGISTER_ARTIFACT_INVALID_ARGUMENT;
	if (length > ORLIX_TCTI_REGISTER_ARTIFACT_MAX_INPUT)
		return ORLIX_TCTI_REGISTER_ARTIFACT_LIMIT;
	if (orlix_tcti_register_model_import(source, length, &model, &error))
		return model_error(&error);
	result = orlix_tcti_target_register_artifact_emit_model(&model, length, output);
	orlix_tcti_register_model_destroy(&model);
	return result;
}

#ifndef TARGET_REGISTER_ARTIFACT_GENERATOR_NO_MAIN
struct source_bytes {
	char *data;
	size_t length;
};

static int read_source(const char *path, struct source_bytes *source)
{
	FILE *file;
	long length;
	size_t read_count;
	int close_result;

	file = fopen(path, "rb");
	if (!file)
		return -1;
	if (fseek(file, 0, SEEK_END) || (length = ftell(file)) < 0 ||
	    fseek(file, 0, SEEK_SET) || (uintmax_t)length > SIZE_MAX - 1) {
		fclose(file);
		return -1;
	}
	if ((uintmax_t)length > ORLIX_TCTI_REGISTER_ARTIFACT_MAX_INPUT) {
		fclose(file);
		errno = EFBIG;
		return -1;
	}
	source->data = malloc((size_t)length + 1);
	if (!source->data) {
		fclose(file);
		return -1;
	}
	read_count = fread(source->data, 1, (size_t)length, file);
	close_result = fclose(file);
	if (read_count != (size_t)length || close_result) {
		free(source->data);
		*source = (struct source_bytes){ 0 };
		return -1;
	}
	source->data[length] = '\0';
	source->length = (size_t)length;
	return 0;
}

static int files_match(FILE *left, FILE *right)
{
	unsigned char left_bytes[4096];
	unsigned char right_bytes[4096];
	size_t left_count;
	size_t right_count;

	for (;;) {
		left_count = fread(left_bytes, 1, sizeof(left_bytes), left);
		right_count = fread(right_bytes, 1, sizeof(right_bytes), right);
		if (left_count != right_count ||
		    memcmp(left_bytes, right_bytes, left_count))
			return -1;
		if (!left_count)
			return ferror(left) || ferror(right) ? -1 : 0;
	}
}

int main(int argc, char **argv)
{
	struct source_bytes source = { 0 };
	FILE *output = stdout;
	FILE *expected = NULL;
	enum orlix_tcti_register_artifact_error error;
	int status = EXIT_FAILURE;

	if (argc == 2) {
		/* Output remains stdout. */
	} else if (argc == 4 && !strcmp(argv[1], "--check")) {
		expected = fopen(argv[3], "rb");
		output = tmpfile();
		if (!expected || !output) {
			fprintf(stderr,
				"target register artifact generator: I/O failure\n");
			goto out;
		}
	} else {
		fprintf(stderr, "usage: %s Registers.json\n"
			"       %s --check Registers.json artifact.def\n",
			argv[0], argv[0]);
		goto out;
	}
	if (read_source(argc == 2 ? argv[1] : argv[2], &source)) {
		fprintf(stderr, "target register artifact generator: %s\n",
			strerror(errno));
		goto out;
	}
	error = orlix_tcti_target_register_artifact_emit(source.data, source.length,
						  output);
	if (error != ORLIX_TCTI_REGISTER_ARTIFACT_OK) {
		fprintf(stderr, "target register artifact generator: %s\n",
			orlix_tcti_target_register_artifact_error_name(error));
		goto out;
	}
	if (expected && (fflush(output) || fseek(output, 0, SEEK_SET) ||
			 files_match(expected, output))) {
		fprintf(stderr,
			"target register artifact generator: artifact mismatch\n");
		goto out;
	}
	status = EXIT_SUCCESS;
out:
	if (expected)
		fclose(expected);
	if (output != stdout)
		fclose(output);
	free(source.data);
	return status;
}
#endif
