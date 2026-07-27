/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_feature_field_domain_binding.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DOMAIN_HASH_OFFSET UINT64_C(1469598103934665603)
#define DOMAIN_HASH_PRIME UINT64_C(1099511628211)

static void binding_fail(
	struct orlix_tcti_feature_field_domain_binding_error *error,
	enum orlix_tcti_feature_field_domain_binding_error_code code,
	size_t offset, const char *format, ...)
{
	va_list arguments;

	if (!error)
		return;
	error->code = code;
	error->offset = offset;
	va_start(arguments, format);
	vsnprintf(error->message, sizeof(error->message), format, arguments);
	va_end(arguments);
}

static int same(const char *left, const char *right)
{
	return left && right && !strcmp(left, right);
}

static int optional_same(const char *left, const char *right)
{
	return (!left && !right) || (left && right && !strcmp(left, right));
}

static int same_field_identity(const struct orlix_tcti_feature_node *left,
	const struct orlix_tcti_feature_node *right)
{
	return left->kind == ORLIX_TCTI_FEATURE_FIELD &&
		right->kind == ORLIX_TCTI_FEATURE_FIELD &&
		same(left->field.state, right->field.state) &&
		same(left->field.register_name, right->field.register_name) &&
		same(left->field.selector, right->field.selector) &&
		left->field.instance.kind == right->field.instance.kind &&
		left->field.slices.kind == right->field.slices.kind;
}

static uint32_t identity_group(const struct orlix_tcti_feature_model *features,
	size_t node_index, uint32_t *occurrence_count)
{
	size_t prior, first, scan;
	uint32_t groups = 0;

	for (prior = 0; prior < node_index; prior++) {
		if (features->nodes[prior].kind != ORLIX_TCTI_FEATURE_FIELD)
			continue;
		for (first = 0; first < prior; first++)
			if (same_field_identity(&features->nodes[prior],
						&features->nodes[first]))
				break;
		if (first < prior)
			continue;
		if (same_field_identity(&features->nodes[node_index],
					&features->nodes[prior])) {
			*occurrence_count = 0;
			for (scan = 0; scan < features->node_count; scan++)
				if (same_field_identity(&features->nodes[node_index],
							&features->nodes[scan]))
					(*occurrence_count)++;
			return groups;
		}
		groups++;
	}
	*occurrence_count = 0;
	for (scan = 0; scan < features->node_count; scan++)
		if (same_field_identity(&features->nodes[node_index],
					&features->nodes[scan]))
			(*occurrence_count)++;
	return groups;
}

static uint64_t hash_byte(uint64_t hash, unsigned char byte)
{
	return (hash ^ byte) * DOMAIN_HASH_PRIME;
}

static uint64_t hash_u64(uint64_t hash, uint64_t value)
{
	unsigned int index;

	for (index = 0; index < 8U; index++)
		hash = hash_byte(hash, (unsigned char)(value >> (index * 8U)));
	return hash;
}

static uint64_t hash_string(uint64_t hash, const char *value)
{
	size_t index, length = value ? strlen(value) : 0U;

	hash = hash_u64(hash, value ? 1U : 0U);
	hash = hash_u64(hash, length);
	for (index = 0; index < length; index++)
		hash = hash_byte(hash, (unsigned char)value[index]);
	return hash;
}

static int span_contains(size_t outer_offset, size_t outer_length,
	size_t inner_offset, size_t inner_length)
{
	return outer_length && inner_length && inner_offset >= outer_offset &&
		inner_offset - outer_offset <= outer_length &&
		inner_length <= outer_length - (inner_offset - outer_offset);
}

static int expression_hash(const struct orlix_tcti_register_model *model,
	uint32_t expression_index, unsigned int depth, uint64_t *hash)
{
	const struct orlix_tcti_register_expression *expression;
	uint32_t child;

	if (expression_index == UINT32_MAX) {
		*hash = hash_u64(*hash, UINT32_MAX);
		return 0;
	}
	if (expression_index >= model->expression_count || depth > 128U)
		return -1;
	expression = &model->expressions[expression_index];
	if (expression->child_count &&
	    (expression->first_child == UINT32_MAX ||
	     expression->first_child > model->expression_child_count ||
	     expression->child_count >
		model->expression_child_count - expression->first_child))
		return -1;
	*hash = hash_string(*hash, expression->type);
	*hash = hash_string(*hash, expression->name);
	*hash = hash_string(*hash, expression->op);
	*hash = hash_string(*hash, expression->value);
	*hash = hash_string(*hash, expression->role);
	*hash = hash_string(*hash, expression->register_state);
	*hash = hash_string(*hash, expression->register_name);
	*hash = hash_string(*hash, expression->field_name);
	*hash = hash_u64(*hash, expression->scalar_kind);
	*hash = hash_u64(*hash, (uint64_t)expression->integer);
	*hash = hash_u64(*hash, expression->boolean);
	*hash = hash_u64(*hash, expression->child_count);
	for (child = 0; child < expression->child_count; child++)
		if (expression_hash(model,
			model->expression_children[expression->first_child + child],
			depth + 1U, hash))
			return -1;
	return 0;
}

static int expression_equal(const struct orlix_tcti_register_model *model,
	uint32_t left_index, uint32_t right_index, unsigned int depth)
{
	const struct orlix_tcti_register_expression *left, *right;
	uint32_t child;

	if (left_index == UINT32_MAX || right_index == UINT32_MAX)
		return left_index == right_index;
	if (left_index >= model->expression_count ||
	    right_index >= model->expression_count || depth > 128U)
		return 0;
	left = &model->expressions[left_index];
	right = &model->expressions[right_index];
	if (!optional_same(left->type, right->type) ||
	    !optional_same(left->name, right->name) ||
	    !optional_same(left->op, right->op) ||
	    !optional_same(left->value, right->value) ||
	    !optional_same(left->role, right->role) ||
	    !optional_same(left->register_state, right->register_state) ||
	    !optional_same(left->register_name, right->register_name) ||
	    !optional_same(left->field_name, right->field_name) ||
	    left->scalar_kind != right->scalar_kind ||
	    left->integer != right->integer || left->boolean != right->boolean ||
	    left->child_count != right->child_count)
		return 0;
	for (child = 0; child < left->child_count; child++)
		if (!expression_equal(model,
			model->expression_children[left->first_child + child],
			model->expression_children[right->first_child + child],
			depth + 1U))
			return 0;
	return 1;
}

static int domain_supported(const struct orlix_tcti_value_domain *domain)
{
	if (!domain->type || !domain->source_length)
		return 0;
	if (!strcmp(domain->type, "Values.Value"))
		return domain->value && domain->value[0];
	if (!strcmp(domain->type, "Values.ValueRange"))
		return domain->start && domain->start[0] && domain->end &&
			domain->end[0];
	if (!strcmp(domain->type, "Values.Link"))
		return domain->link && domain->link[0];
	if (!strcmp(domain->type, "Values.ConditionalValue"))
		return domain->condition_expression != UINT32_MAX &&
			domain->child_domain_count > 0U;
	return 0;
}

static int hash_domain(const struct orlix_tcti_register_model *model,
	const struct orlix_tcti_value_domain *domain, uint64_t *hash)
{
	if (!domain_supported(domain))
		return -1;
	*hash = hash_string(*hash, domain->type);
	*hash = hash_string(*hash, domain->value);
	*hash = hash_string(*hash, domain->start);
	*hash = hash_string(*hash, domain->end);
	*hash = hash_string(*hash, domain->link);
	*hash = hash_string(*hash, domain->meaning);
	*hash = hash_u64(*hash, domain->parent_domain != UINT32_MAX);
	*hash = hash_u64(*hash, domain->child_domain_count);
	*hash = hash_u64(*hash, domain->valueset_index != UINT32_MAX);
	*hash = hash_u64(*hash, domain->nested_valueset_index != UINT32_MAX);
	if (domain->valueset_index != UINT32_MAX) {
		if (domain->valueset_index >= model->valueset_count)
			return -1;
		*hash = hash_string(*hash,
			model->valuesets[domain->valueset_index].type);
	}
	if (domain->nested_valueset_index != UINT32_MAX) {
		if (domain->nested_valueset_index >= model->valueset_count)
			return -1;
		*hash = hash_string(*hash,
			model->valuesets[domain->nested_valueset_index].type);
	}
	if (expression_hash(model, domain->condition_expression, 0U, hash))
		return -1;
	return 0;
}

static int relation_valid(const struct orlix_tcti_register_model *model,
	uint32_t field_index,
	const struct orlix_tcti_register_field_value_relation *relation)
{
	if (relation->field_index != field_index)
		return 0;
	if (relation->value_candidate_count &&
	    (relation->first_value_candidate == UINT32_MAX ||
	     relation->first_value_candidate > model->field_value_candidate_count ||
	     relation->value_candidate_count > model->field_value_candidate_count -
		relation->first_value_candidate))
		return 0;
	if (relation->constraint_candidate_count &&
	    (relation->first_constraint_candidate == UINT32_MAX ||
	     relation->first_constraint_candidate >
		model->field_constraint_candidate_count ||
	     relation->constraint_candidate_count >
		model->field_constraint_candidate_count -
		relation->first_constraint_candidate))
		return 0;
	return 1;
}

static int normalize_field(const struct orlix_tcti_register_model *model,
	uint32_t field_index,
	struct orlix_tcti_feature_field_normalized_domain *domain,
	struct orlix_tcti_feature_field_domain_binding_error *error)
{
	const struct orlix_tcti_field_identity *field = &model->fields[field_index];
	const struct orlix_tcti_register_fieldset *fieldset;
	const struct orlix_tcti_register_field_value_relation *relation;
	uint64_t semantic = DOMAIN_HASH_OFFSET;
	uint32_t index;
	int saw_enum = 0, saw_range = 0;

	memset(domain, 0, sizeof(*domain));
	domain->field_condition_expression = UINT32_MAX;
	domain->wrapper_condition_expression = UINT32_MAX;
	domain->fieldset_condition_expression = UINT32_MAX;
	if (!field->name || !field->type ||
	    strcmp(field->type, "Fields.ConstantField") ||
	    field->fieldset_index >= model->fieldset_count ||
	    field_index >= model->field_value_relation_count) {
		binding_fail(error,
			ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_UNSUPPORTED_DOMAIN,
			field->source_offset, "unsupported or incomplete field domain");
		return -1;
	}
	fieldset = &model->fieldsets[field->fieldset_index];
	relation = &model->field_value_relations[field_index];
	if (!fieldset->width || field->width != fieldset->width ||
	    field->register_index != fieldset->register_index ||
	    !field->range_count || field->first_range == UINT32_MAX ||
	    field->first_range > model->range_count ||
	    field->range_count > model->range_count - field->first_range ||
	    !relation_valid(model, field_index, relation)) {
		binding_fail(error,
			ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_INVALID_MODEL,
			field->source_offset, "malformed fieldset, range, or relation");
		return -1;
	}
	if (fieldset->source_offset > UINT32_MAX ||
	    fieldset->source_length > UINT32_MAX ||
	    field->condition_offset > UINT32_MAX ||
	    field->condition_length > UINT32_MAX ||
	    field->wrapper_condition_offset > UINT32_MAX ||
	    field->wrapper_condition_length > UINT32_MAX ||
	    fieldset->condition_offset > UINT32_MAX ||
	    fieldset->condition_length > UINT32_MAX) {
		binding_fail(error,
			ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_INVALID_MODEL,
			field->source_offset, "normalized provenance exceeds 32 bits");
		return -1;
	}
	domain->fieldset_index = field->fieldset_index;
	domain->equivalent_field_count = 1U;
	domain->register_width = fieldset->width;
	domain->range_count = field->range_count;
	domain->relation_value_count = relation->value_candidate_count;
	domain->relation_constraint_count = relation->constraint_candidate_count;
	domain->field_condition_expression = field->condition_expression;
	domain->wrapper_condition_expression = field->wrapper_condition_expression;
	domain->fieldset_condition_expression = fieldset->condition_expression;
	domain->type = ORLIX_TCTI_FEATURE_FIELD_DOMAIN_TYPE_CONSTANT;
	domain->signedness = ORLIX_TCTI_FEATURE_FIELD_DOMAIN_UNSIGNED;
	domain->fieldset_source_offset = (uint32_t)fieldset->source_offset;
	domain->fieldset_source_length = (uint32_t)fieldset->source_length;
	domain->field_condition_offset = (uint32_t)field->condition_offset;
	domain->field_condition_length = (uint32_t)field->condition_length;
	domain->wrapper_condition_offset =
		(uint32_t)field->wrapper_condition_offset;
	domain->wrapper_condition_length =
		(uint32_t)field->wrapper_condition_length;
	domain->fieldset_condition_offset =
		(uint32_t)fieldset->condition_offset;
	domain->fieldset_condition_length =
		(uint32_t)fieldset->condition_length;
	semantic = hash_string(semantic, field->name);
	semantic = hash_string(semantic, field->type);
	semantic = hash_u64(semantic, field->width);
	semantic = hash_u64(semantic, fieldset->width);
	semantic = hash_u64(semantic, field->range_count);
	for (index = 0; index < field->range_count; index++) {
		const struct orlix_tcti_register_range *range =
			&model->ranges[field->first_range + index];
		uint32_t prior;

		if (range->field_index != field_index || !range->width ||
		    range->start >= fieldset->width ||
		    range->width > fieldset->width - range->start) {
			binding_fail(error,
				ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_INVALID_MODEL,
				range->source_offset, "field range is malformed");
			return -1;
		}
		for (prior = 0; prior < index; prior++) {
			const struct orlix_tcti_register_range *other =
				&model->ranges[field->first_range + prior];
			if (range->start < other->start + other->width &&
			    other->start < range->start + range->width) {
				binding_fail(error,
					ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_INVALID_MODEL,
					range->source_offset, "field ranges overlap");
				return -1;
			}
		}
		if (UINT32_MAX - domain->field_width < range->width)
			return -1;
		domain->field_width += range->width;
		semantic = hash_u64(semantic, range->start);
		semantic = hash_u64(semantic, range->width);
	}
	for (index = 0; index < model->domain_count; index++) {
		const struct orlix_tcti_value_domain *value = &model->domains[index];

		if (!span_contains(field->source_offset, field->source_length,
				   value->source_offset, value->source_length))
			continue;
		if (hash_domain(model, value, &semantic)) {
			binding_fail(error,
				ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_UNSUPPORTED_DOMAIN,
				value->source_offset, "unsupported value-domain grammar");
			return -1;
		}
		domain->value_member_count++;
		if (!strcmp(value->type, "Values.ValueRange")) {
			domain->range_member_count++;
			saw_range = 1;
		} else if (strcmp(value->type, "Values.ConditionalValue")) {
			saw_enum = 1;
		}
	}
	for (index = 0; index < model->constraint_count; index++) {
		const struct orlix_tcti_register_constraint *constraint =
			&model->constraints[index];

		if (!span_contains(field->source_offset, field->source_length,
				   constraint->source_offset, constraint->source_length))
			continue;
		if (constraint->field_index != UINT32_MAX &&
		    constraint->field_index != field_index) {
			binding_fail(error,
				ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_INVALID_MODEL,
				constraint->source_offset,
				"contained constraint belongs to another field");
			return -1;
		}
		if ((constraint->valueset_index != UINT32_MAX &&
		     constraint->valueset_index >= model->valueset_count) ||
		    (constraint->item_count &&
		    (constraint->first_item == UINT32_MAX ||
		     constraint->first_item > model->constraint_item_count ||
		     constraint->item_count > model->constraint_item_count -
			constraint->first_item))) {
			binding_fail(error,
				ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_INVALID_MODEL,
				constraint->source_offset,
				"contained constraint items are malformed");
			return -1;
		}
		domain->relation_constraint_count++;
		semantic = hash_u64(semantic, constraint->kind);
		semantic = hash_u64(semantic, constraint->item_count);
		semantic = hash_string(semantic,
			constraint->valueset_index == UINT32_MAX ? NULL :
			model->valuesets[constraint->valueset_index].type);
	}
	for (index = 0; index < relation->value_candidate_count; index++) {
		const struct orlix_tcti_register_field_value_candidate *candidate =
			&model->field_value_candidates[
				relation->first_value_candidate + index];

		if (candidate->field_index != field_index ||
		    (candidate->domain_count &&
		     (candidate->first_domain == UINT32_MAX ||
		      candidate->first_domain > model->domain_count ||
		      candidate->domain_count > model->domain_count -
			candidate->first_domain)) ||
		    (candidate->valueset_index != UINT32_MAX &&
		     candidate->valueset_index >= model->valueset_count)) {
			binding_fail(error,
				ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_INVALID_MODEL,
				candidate->source_offset,
				"field value relation candidate is malformed");
			return -1;
		}
		semantic = hash_u64(semantic, candidate->kind);
		semantic = hash_u64(semantic, candidate->domain_count);
		semantic = hash_string(semantic,
			candidate->valueset_index == UINT32_MAX ? NULL :
			model->valuesets[candidate->valueset_index].type);
	}
	for (index = 0; index < relation->constraint_candidate_count; index++) {
		const struct orlix_tcti_register_field_constraint_candidate *candidate =
			&model->field_constraint_candidates[
				relation->first_constraint_candidate + index];

		if (candidate->field_index != field_index ||
		    candidate->constraint_index >= model->constraint_count ||
		    (candidate->domain_count &&
		     (candidate->first_domain == UINT32_MAX ||
		      candidate->first_domain > model->domain_count ||
		      candidate->domain_count > model->domain_count -
			candidate->first_domain))) {
			binding_fail(error,
				ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_INVALID_MODEL,
				candidate->source_offset,
				"field constraint relation candidate is malformed");
			return -1;
		}
		semantic = hash_u64(semantic, candidate->kind);
		semantic = hash_u64(semantic, candidate->domain_count);
		semantic = hash_u64(semantic,
			model->constraints[candidate->constraint_index].kind);
	}
	/* No explicit Values node denotes the full unsigned bit-range. */
	if (!domain->value_member_count) {
		domain->range_member_count = 1U;
		saw_range = 1;
	}
	domain->member_kind = saw_enum && saw_range ?
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_MIXED_MEMBERS :
		(saw_range ? ORLIX_TCTI_FEATURE_FIELD_DOMAIN_RANGE_MEMBERS :
		 ORLIX_TCTI_FEATURE_FIELD_DOMAIN_ENUM_MEMBERS);
	semantic = hash_u64(semantic, domain->type);
	semantic = hash_u64(semantic, domain->signedness);
	semantic = hash_u64(semantic, domain->member_kind);
	semantic = hash_u64(semantic, domain->value_member_count);
	semantic = hash_u64(semantic, domain->range_member_count);
	semantic = hash_u64(semantic, relation->value_candidate_count);
	semantic = hash_u64(semantic, domain->relation_constraint_count);
	if (expression_hash(model, field->condition_expression, 0U, &semantic) ||
	    expression_hash(model, field->wrapper_condition_expression, 0U,
			    &semantic) ||
	    expression_hash(model, relation->field_condition_expression, 0U,
			    &semantic) ||
	    expression_hash(model, relation->wrapper_condition_expression, 0U,
			    &semantic)) {
		binding_fail(error,
			ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_INVALID_MODEL,
			field->source_offset, "field condition is malformed");
		return -1;
	}
	domain->semantic_identity = semantic;
	return 0;
}

static int domains_equivalent(
	const struct orlix_tcti_feature_field_normalized_domain *left,
	const struct orlix_tcti_feature_field_normalized_domain *right)
{
	return left->register_width == right->register_width &&
		left->field_width == right->field_width &&
		left->range_count == right->range_count &&
		left->value_member_count == right->value_member_count &&
		left->range_member_count == right->range_member_count &&
		left->relation_value_count == right->relation_value_count &&
		left->relation_constraint_count == right->relation_constraint_count &&
		left->type == right->type && left->signedness == right->signedness &&
		left->member_kind == right->member_kind &&
		left->semantic_identity == right->semantic_identity;
}

static const char *valueset_type(const struct orlix_tcti_register_model *model,
	uint32_t index)
{
	return index == UINT32_MAX ? NULL : model->valuesets[index].type;
}

static int value_domains_equal(const struct orlix_tcti_register_model *model,
	const struct orlix_tcti_value_domain *left,
	const struct orlix_tcti_value_domain *right)
{
	return optional_same(left->type, right->type) &&
		optional_same(left->value, right->value) &&
		optional_same(left->start, right->start) &&
		optional_same(left->end, right->end) &&
		optional_same(left->link, right->link) &&
		optional_same(left->meaning, right->meaning) &&
		(left->parent_domain != UINT32_MAX) ==
			(right->parent_domain != UINT32_MAX) &&
		left->child_domain_count == right->child_domain_count &&
		optional_same(valueset_type(model, left->valueset_index),
			valueset_type(model, right->valueset_index)) &&
		optional_same(valueset_type(model, left->nested_valueset_index),
			valueset_type(model, right->nested_valueset_index)) &&
		expression_equal(model, left->condition_expression,
			right->condition_expression, 0U);
}

static int exact_field_semantics_equal(
	const struct orlix_tcti_register_model *model, uint32_t left_index,
	uint32_t right_index)
{
	const struct orlix_tcti_field_identity *left = &model->fields[left_index];
	const struct orlix_tcti_field_identity *right = &model->fields[right_index];
	const struct orlix_tcti_register_field_value_relation *left_relation =
		&model->field_value_relations[left_index];
	const struct orlix_tcti_register_field_value_relation *right_relation =
		&model->field_value_relations[right_index];
	uint32_t item;
	size_t left_domain = 0, right_domain = 0;
	size_t left_constraint = 0, right_constraint = 0;

	if (!optional_same(left->name, right->name) ||
	    !optional_same(left->type, right->type) ||
	    left->width != right->width ||
	    model->fieldsets[left->fieldset_index].width !=
		model->fieldsets[right->fieldset_index].width ||
	    left->range_count != right->range_count ||
	    !expression_equal(model, left->condition_expression,
		right->condition_expression, 0U) ||
	    !expression_equal(model, left->wrapper_condition_expression,
		right->wrapper_condition_expression, 0U) ||
	    !expression_equal(model, left_relation->field_condition_expression,
		right_relation->field_condition_expression, 0U) ||
	    !expression_equal(model, left_relation->wrapper_condition_expression,
		right_relation->wrapper_condition_expression, 0U) ||
	    left_relation->value_candidate_count !=
		right_relation->value_candidate_count ||
	    left_relation->constraint_candidate_count !=
		right_relation->constraint_candidate_count)
		return 0;
	for (item = 0; item < left->range_count; item++) {
		const struct orlix_tcti_register_range *left_range =
			&model->ranges[left->first_range + item];
		const struct orlix_tcti_register_range *right_range =
			&model->ranges[right->first_range + item];

		if (left_range->start != right_range->start ||
		    left_range->width != right_range->width)
			return 0;
	}
	for (item = 0; item < left_relation->value_candidate_count; item++) {
		const struct orlix_tcti_register_field_value_candidate *left_item =
			&model->field_value_candidates[
				left_relation->first_value_candidate + item];
		const struct orlix_tcti_register_field_value_candidate *right_item =
			&model->field_value_candidates[
				right_relation->first_value_candidate + item];

		if (left_item->kind != right_item->kind ||
		    left_item->domain_count != right_item->domain_count ||
		    !optional_same(valueset_type(model, left_item->valueset_index),
			valueset_type(model, right_item->valueset_index)))
			return 0;
	}
	for (item = 0; item < left_relation->constraint_candidate_count; item++) {
		const struct orlix_tcti_register_field_constraint_candidate *left_item =
			&model->field_constraint_candidates[
				left_relation->first_constraint_candidate + item];
		const struct orlix_tcti_register_field_constraint_candidate *right_item =
			&model->field_constraint_candidates[
				right_relation->first_constraint_candidate + item];

		if (left_item->kind != right_item->kind ||
		    left_item->domain_count != right_item->domain_count ||
		    model->constraints[left_item->constraint_index].kind !=
			model->constraints[right_item->constraint_index].kind)
			return 0;
	}
	for (;;) {
		while (left_domain < model->domain_count &&
		       !span_contains(left->source_offset, left->source_length,
			model->domains[left_domain].source_offset,
			model->domains[left_domain].source_length))
			left_domain++;
		while (right_domain < model->domain_count &&
		       !span_contains(right->source_offset, right->source_length,
			model->domains[right_domain].source_offset,
			model->domains[right_domain].source_length))
			right_domain++;
		if (left_domain == model->domain_count ||
		    right_domain == model->domain_count) {
			if (left_domain != right_domain)
				return 0;
			break;
		}
		if (!value_domains_equal(model, &model->domains[left_domain],
					 &model->domains[right_domain]))
			return 0;
		left_domain++;
		right_domain++;
	}
	for (;;) {
		const struct orlix_tcti_register_constraint *left_item, *right_item;

		while (left_constraint < model->constraint_count &&
		       !span_contains(left->source_offset, left->source_length,
			model->constraints[left_constraint].source_offset,
			model->constraints[left_constraint].source_length))
			left_constraint++;
		while (right_constraint < model->constraint_count &&
		       !span_contains(right->source_offset, right->source_length,
			model->constraints[right_constraint].source_offset,
			model->constraints[right_constraint].source_length))
			right_constraint++;
		if (left_constraint == model->constraint_count ||
		    right_constraint == model->constraint_count)
			return left_constraint == right_constraint;
		left_item = &model->constraints[left_constraint];
		right_item = &model->constraints[right_constraint];
		if (left_item->kind != right_item->kind ||
		    left_item->item_count != right_item->item_count ||
		    !optional_same(valueset_type(model, left_item->valueset_index),
			valueset_type(model, right_item->valueset_index)))
			return 0;
		left_constraint++;
		right_constraint++;
	}
	return 1;
}

static void *append_table(void **table, size_t *count, size_t item_size)
{
	void *grown, *item;

	if (*count == SIZE_MAX || item_size > SIZE_MAX / (*count + 1U))
		return NULL;
	grown = realloc(*table, (*count + 1U) * item_size);
	if (!grown)
		return NULL;
	*table = grown;
	item = (unsigned char *)grown + *count * item_size;
	memset(item, 0, item_size);
	(*count)++;
	return item;
}

static uint32_t local_index(uint32_t raw, const uint32_t *map, size_t count)
{
	if (raw == UINT32_MAX)
		return UINT32_MAX;
	if (raw >= count)
		return UINT32_MAX;
	return map[raw];
}

struct alternative_build_context {
	const struct orlix_tcti_register_model *model;
	struct orlix_tcti_feature_field_domain_bindings *bindings;
	struct orlix_tcti_feature_field_domain_alternative *alternative;
	uint32_t *valueset_map;
	uint32_t *domain_map;
	uint32_t *constraint_map;
	uint32_t *expression_map;
	unsigned char *expression_state;
};

static int ensure_valueset(struct alternative_build_context *context,
	uint32_t raw_index, uint32_t *normalized)
{
	struct orlix_tcti_feature_field_domain_valueset *value;

	if (raw_index == UINT32_MAX) {
		*normalized = UINT32_MAX;
		return 0;
	}
	if (raw_index >= context->model->valueset_count)
		return -1;
	if (context->valueset_map[raw_index] != UINT32_MAX) {
		*normalized = context->valueset_map[raw_index];
		return 0;
	}
	value = append_table((void **)&context->bindings->valuesets,
		&context->bindings->valueset_count, sizeof(*value));
	if (!value)
		return -1;
	*normalized = (uint32_t)(context->bindings->valueset_count -
		context->alternative->first_valueset - 1U);
	context->valueset_map[raw_index] = *normalized;
	value->type = context->model->valuesets[raw_index].type;
	context->alternative->valueset_count++;
	return 0;
}

static int copy_expression(struct alternative_build_context *context,
	uint32_t raw_index, uint32_t parent, uint32_t *normalized)
{
	const struct orlix_tcti_register_expression *source;
	struct orlix_tcti_feature_field_domain_expression *target;
	uint32_t local, child;
	size_t child_base;

	if (raw_index == UINT32_MAX) {
		*normalized = UINT32_MAX;
		return 0;
	}
	if (raw_index >= context->model->expression_count ||
	    context->expression_state[raw_index] == 1U)
		return -1;
	if (context->expression_state[raw_index] == 2U) {
		local = context->expression_map[raw_index];
		if (context->bindings->expressions[
			context->alternative->first_expression + local].parent_expression != parent)
			return -1;
		*normalized = local;
		return 0;
	}
	source = &context->model->expressions[raw_index];
	if (source->child_count &&
	    (source->first_child == UINT32_MAX ||
	     source->first_child > context->model->expression_child_count ||
	     source->child_count > context->model->expression_child_count -
		source->first_child))
		return -1;
	target = append_table((void **)&context->bindings->expressions,
		&context->bindings->expression_count, sizeof(*target));
	if (!target)
		return -1;
	local = (uint32_t)(context->bindings->expression_count -
		context->alternative->first_expression - 1U);
	context->expression_map[raw_index] = local;
	context->expression_state[raw_index] = 1U;
	target->type = source->type;
	target->name = source->name;
	target->op = source->op;
	target->value = source->value;
	target->role = source->role;
	target->register_state = source->register_state;
	target->register_name = source->register_name;
	target->field_name = source->field_name;
	target->child_count = source->child_count;
	target->scalar_kind = source->scalar_kind;
	target->integer = source->integer;
	target->boolean = source->boolean;
	target->parent_expression = parent;
	target->instance_offset = source->instance_offset;
	target->instance_length = source->instance_length;
	target->slices_offset = source->slices_offset;
	target->slices_length = source->slices_length;
	target->source_offset = source->source_offset;
	target->source_length = source->source_length;
	child_base = context->bindings->expression_child_count;
	target->first_child = source->child_count ?
		(uint32_t)(child_base - context->alternative->first_expression_child) :
		UINT32_MAX;
	if (source->child_count) {
		uint32_t *grown;
		size_t total = child_base + source->child_count;

		if (total < child_base || total > SIZE_MAX / sizeof(*grown))
			return -1;
		grown = realloc(context->bindings->expression_children,
			total * sizeof(*grown));
		if (!grown)
			return -1;
		context->bindings->expression_children = grown;
		context->bindings->expression_child_count = total;
		context->alternative->expression_child_count += source->child_count;
	}
	for (child = 0; child < source->child_count; child++) {
		uint32_t child_raw = context->model->expression_children[
			source->first_child + child];
		uint32_t child_local;

		if (child_raw >= context->model->expression_count ||
		    context->model->expressions[child_raw].parent_expression != raw_index ||
		    copy_expression(context, child_raw, local, &child_local))
			return -1;
		context->bindings->expression_children[child_base + child] = child_local;
	}
	context->expression_state[raw_index] = 2U;
	context->alternative->expression_count++;
	*normalized = local;
	return 0;
}

static int span_selects_field(const struct orlix_tcti_field_identity *field,
	size_t offset, size_t length)
{
	return span_contains(field->source_offset, field->source_length,
		offset, length);
}

static int copy_alternative(
	const struct orlix_tcti_register_model *model, uint32_t field_index,
	struct orlix_tcti_feature_field_domain_bindings *bindings,
	struct orlix_tcti_feature_field_domain_binding_error *error)
{
	const struct orlix_tcti_field_identity *field = &model->fields[field_index];
	const struct orlix_tcti_register_field_value_relation *relation;
	const struct orlix_tcti_register_fieldset *fieldset;
	struct alternative_build_context context = { 0 };
	struct orlix_tcti_feature_field_domain_alternative *alternative;
	uint32_t index, local;
	int result = -1;

	if (field_index >= model->field_value_relation_count ||
	    field->fieldset_index >= model->fieldset_count)
		goto malformed;
	relation = &model->field_value_relations[field_index];
	fieldset = &model->fieldsets[field->fieldset_index];
	alternative = append_table((void **)&bindings->alternatives,
		&bindings->alternative_count, sizeof(*alternative));
	if (!alternative)
		goto no_memory;
	alternative->field_index = field_index;
	alternative->value_relation_index = field_index;
	alternative->fieldset_index = field->fieldset_index;
	alternative->first_range = (uint32_t)bindings->range_count;
	alternative->first_valueset = (uint32_t)bindings->valueset_count;
	alternative->first_domain = (uint32_t)bindings->domain_count;
	alternative->first_link = (uint32_t)bindings->link_count;
	alternative->first_expression = (uint32_t)bindings->expression_count;
	alternative->first_expression_child = (uint32_t)bindings->expression_child_count;
	alternative->first_value_candidate = (uint32_t)bindings->value_candidate_count;
	alternative->first_constraint = (uint32_t)bindings->constraint_count;
	alternative->first_constraint_item = (uint32_t)bindings->constraint_item_count;
	alternative->first_constraint_candidate =
		(uint32_t)bindings->constraint_candidate_count;
	alternative->field_source_offset = field->source_offset;
	alternative->field_source_length = field->source_length;
	alternative->value_relation_source_offset = relation->source_offset;
	alternative->value_relation_source_length = relation->source_length;
	alternative->fieldset_source_offset = fieldset->source_offset;
	alternative->fieldset_source_length = fieldset->source_length;
	alternative->field_condition_offset = field->condition_offset;
	alternative->field_condition_length = field->condition_length;
	alternative->wrapper_condition_offset = field->wrapper_condition_offset;
	alternative->wrapper_condition_length = field->wrapper_condition_length;
	alternative->fieldset_condition_offset = fieldset->condition_offset;
	alternative->fieldset_condition_length = fieldset->condition_length;
	context.model = model;
	context.bindings = bindings;
	context.alternative = alternative;
	context.valueset_map = malloc(model->valueset_count * sizeof(uint32_t));
	context.domain_map = malloc(model->domain_count * sizeof(uint32_t));
	context.constraint_map = malloc(model->constraint_count * sizeof(uint32_t));
	context.expression_map = malloc(model->expression_count * sizeof(uint32_t));
	context.expression_state = calloc(model->expression_count, 1U);
	if ((model->valueset_count && !context.valueset_map) ||
	    (model->domain_count && !context.domain_map) ||
	    (model->constraint_count && !context.constraint_map) ||
	    (model->expression_count &&
	     (!context.expression_map || !context.expression_state)))
		goto no_memory;
	for (index = 0; index < model->valueset_count; index++)
		context.valueset_map[index] = UINT32_MAX;
	for (index = 0; index < model->domain_count; index++)
		context.domain_map[index] = UINT32_MAX;
	for (index = 0; index < model->constraint_count; index++)
		context.constraint_map[index] = UINT32_MAX;
	for (index = 0; index < model->expression_count; index++)
		context.expression_map[index] = UINT32_MAX;

	for (index = 0; index < field->range_count; index++) {
		const struct orlix_tcti_register_range *source;
		struct orlix_tcti_feature_field_domain_range *target;

		if (field->first_range == UINT32_MAX ||
		    field->first_range + index >= model->range_count)
			goto malformed;
		source = &model->ranges[field->first_range + index];
		if (source->field_index != field_index)
			goto malformed;
		target = append_table((void **)&bindings->ranges,
			&bindings->range_count, sizeof(*target));
		if (!target)
			goto no_memory;
		target->start = source->start;
		target->width = source->width;
		alternative->range_count++;
	}
	for (index = 0; index < model->valueset_count; index++)
		if ((model->valuesets[index].field_index == field_index ||
		     span_selects_field(field, model->valuesets[index].source_offset,
				model->valuesets[index].source_length)) &&
		    ensure_valueset(&context, index, &local))
			goto no_memory;
	for (index = 0; index < model->domain_count; index++) {
		struct orlix_tcti_feature_field_domain_node *target;

		if (model->domains[index].field_index != field_index &&
		    !span_selects_field(field, model->domains[index].source_offset,
			model->domains[index].source_length))
			continue;
		target = append_table((void **)&bindings->domains,
			&bindings->domain_count, sizeof(*target));
		if (!target)
			goto no_memory;
		context.domain_map[index] = (uint32_t)(bindings->domain_count -
			alternative->first_domain - 1U);
		alternative->domain_count++;
	}
	for (index = 0; index < model->domain_count; index++) {
		const struct orlix_tcti_value_domain *source = &model->domains[index];
		struct orlix_tcti_feature_field_domain_node *target;
		uint32_t child;

		if (context.domain_map[index] == UINT32_MAX)
			continue;
		target = &bindings->domains[alternative->first_domain +
			context.domain_map[index]];
		target->type = source->type;
		target->value = source->value;
		target->start = source->start;
		target->end = source->end;
		target->link = source->link;
		target->meaning = source->meaning;
		target->parent_domain = local_index(source->parent_domain,
			context.domain_map, model->domain_count);
		if (source->parent_domain != UINT32_MAX &&
		    target->parent_domain == UINT32_MAX)
			goto malformed;
		{
			uint32_t ancestor = source->parent_domain;
			size_t depth = 0;
			while (ancestor != UINT32_MAX) {
				if (ancestor >= model->domain_count ||
				    context.domain_map[ancestor] == UINT32_MAX ||
				    ancestor == index || depth++ >= alternative->domain_count)
					goto malformed;
				ancestor = model->domains[ancestor].parent_domain;
			}
		}
		target->child_domain_count = source->child_domain_count;
		target->first_child_domain = source->child_domain_count ?
			local_index(source->first_child_domain, context.domain_map,
				model->domain_count) : UINT32_MAX;
		if (source->child_domain_count &&
		    (target->first_child_domain == UINT32_MAX ||
		     source->first_child_domain > model->domain_count ||
		     source->child_domain_count > model->domain_count -
			source->first_child_domain))
			goto malformed;
		for (child = 0; child < source->child_domain_count; child++) {
			uint32_t child_raw = source->first_child_domain + child;
			if (context.domain_map[child_raw] !=
			    target->first_child_domain + child ||
			    model->domains[child_raw].parent_domain != index)
				goto malformed;
		}
		if (ensure_valueset(&context, source->valueset_index,
				    &target->valueset_index) ||
		    ensure_valueset(&context, source->nested_valueset_index,
				    &target->nested_valueset_index) ||
		    copy_expression(&context, source->condition_expression,
				    UINT32_MAX, &target->condition_expression))
			goto malformed;
		target->first_link = (uint32_t)(bindings->link_count -
			alternative->first_link);
		for (child = 0; child < model->link_count; child++) {
			const struct orlix_tcti_register_link *source_link =
				&model->links[child];
			struct orlix_tcti_feature_field_domain_link *target_link;
			if (source_link->domain_index != index)
				continue;
			target_link = append_table((void **)&bindings->links,
				&bindings->link_count, sizeof(*target_link));
			if (!target_link)
				goto no_memory;
			target_link->key = source_link->key;
			target_link->value = source_link->value;
			target_link->domain_index = context.domain_map[index];
			target->link_count++;
			alternative->link_count++;
		}
	}
	if (copy_expression(&context, field->condition_expression, UINT32_MAX,
			    &alternative->field_condition_expression) ||
	    copy_expression(&context, field->wrapper_condition_expression, UINT32_MAX,
			    &alternative->wrapper_condition_expression) ||
	    copy_expression(&context, fieldset->condition_expression, UINT32_MAX,
			    &alternative->fieldset_condition_expression) ||
	    copy_expression(&context, relation->field_condition_expression, UINT32_MAX,
			    &alternative->relation_field_condition_expression) ||
	    copy_expression(&context, relation->wrapper_condition_expression, UINT32_MAX,
			    &alternative->relation_wrapper_condition_expression))
		goto malformed;

	for (index = 0; index < model->constraint_count; index++) {
		const struct orlix_tcti_register_constraint *source =
			&model->constraints[index];
		struct orlix_tcti_feature_field_domain_constraint *target;
		uint32_t item;

		if (source->field_index != field_index &&
		    !span_selects_field(field, source->source_offset, source->source_length))
			continue;
		target = append_table((void **)&bindings->constraints,
			&bindings->constraint_count, sizeof(*target));
		if (!target)
			goto no_memory;
		context.constraint_map[index] = (uint32_t)(bindings->constraint_count -
			alternative->first_constraint - 1U);
		target->kind = source->kind;
		if (ensure_valueset(&context, source->valueset_index,
				    &target->valueset_index))
			goto malformed;
		target->first_item = (uint32_t)(bindings->constraint_item_count -
			alternative->first_constraint_item);
		target->item_count = source->item_count;
		if (source->item_count &&
		    (source->first_item == UINT32_MAX ||
		     source->first_item > model->constraint_item_count ||
		     source->item_count > model->constraint_item_count - source->first_item))
			goto malformed;
		for (item = 0; item < source->item_count; item++) {
			const struct orlix_tcti_register_constraint_item *source_item =
				&model->constraint_items[source->first_item + item];
			struct orlix_tcti_feature_field_domain_constraint_item *target_item;
			if (source_item->constraint_index != index)
				goto malformed;
			target_item = append_table((void **)&bindings->constraint_items,
				&bindings->constraint_item_count, sizeof(*target_item));
			if (!target_item)
				goto no_memory;
			target_item->constraint_index = context.constraint_map[index];
			if (ensure_valueset(&context, source_item->valueset_index,
					    &target_item->valueset_index))
				goto malformed;
			target_item->domain_index = local_index(source_item->domain_index,
				context.domain_map, model->domain_count);
			if (source_item->domain_index != UINT32_MAX &&
			    target_item->domain_index == UINT32_MAX)
				goto malformed;
			alternative->constraint_item_count++;
		}
		alternative->constraint_count++;
	}
	for (index = 0; index < relation->value_candidate_count; index++) {
		const struct orlix_tcti_register_field_value_candidate *source;
		struct orlix_tcti_feature_field_domain_value_candidate *target;
		uint32_t end;
		if (relation->first_value_candidate == UINT32_MAX ||
		    relation->first_value_candidate + index >=
			model->field_value_candidate_count)
			goto malformed;
		source = &model->field_value_candidates[
			relation->first_value_candidate + index];
		if (source->field_index != field_index)
			goto malformed;
		target = append_table((void **)&bindings->value_candidates,
			&bindings->value_candidate_count, sizeof(*target));
		if (!target)
			goto no_memory;
		target->kind = source->kind;
		if (ensure_valueset(&context, source->valueset_index,
				    &target->valueset_index))
			goto malformed;
		target->domain_count = source->domain_count;
		target->first_domain = source->domain_count ?
			local_index(source->first_domain, context.domain_map,
				model->domain_count) : UINT32_MAX;
		end = target->first_domain + target->domain_count;
		if (source->domain_count &&
		    (target->first_domain == UINT32_MAX || end < target->first_domain ||
		     end > alternative->domain_count))
			goto malformed;
		alternative->value_candidate_count++;
	}
	for (index = 0; index < relation->constraint_candidate_count; index++) {
		const struct orlix_tcti_register_field_constraint_candidate *source;
		struct orlix_tcti_feature_field_domain_constraint_candidate *target;
		uint32_t end;
		if (relation->first_constraint_candidate == UINT32_MAX ||
		    relation->first_constraint_candidate + index >=
			model->field_constraint_candidate_count)
			goto malformed;
		source = &model->field_constraint_candidates[
			relation->first_constraint_candidate + index];
		if (source->field_index != field_index ||
		    source->constraint_index >= model->constraint_count)
			goto malformed;
		target = append_table((void **)&bindings->constraint_candidates,
			&bindings->constraint_candidate_count, sizeof(*target));
		if (!target)
			goto no_memory;
		target->constraint_index = context.constraint_map[source->constraint_index];
		if (target->constraint_index == UINT32_MAX)
			goto malformed;
		target->kind = source->kind;
		target->domain_count = source->domain_count;
		target->first_domain = source->domain_count ?
			local_index(source->first_domain, context.domain_map,
				model->domain_count) : UINT32_MAX;
		end = target->first_domain + target->domain_count;
		if (source->domain_count &&
		    (target->first_domain == UINT32_MAX || end < target->first_domain ||
		     end > alternative->domain_count))
			goto malformed;
		alternative->constraint_candidate_count++;
	}
	result = 0;
	goto out;
no_memory:
	binding_fail(error, ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_NO_MEMORY,
		field->source_offset, "cannot retain normalized field-domain graph");
	goto out;
malformed:
	binding_fail(error, ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_INVALID_MODEL,
		field->source_offset, "malformed field-domain graph");
out:
	free(context.valueset_map);
	free(context.domain_map);
	free(context.constraint_map);
	free(context.expression_map);
	free(context.expression_state);
	return result;
}

static int normalized_expression_equal(
	const struct orlix_tcti_feature_field_domain_bindings *bindings,
	const struct orlix_tcti_feature_field_domain_alternative *left_alternative,
	uint32_t left_index,
	const struct orlix_tcti_feature_field_domain_alternative *right_alternative,
	uint32_t right_index, unsigned int depth)
{
	const struct orlix_tcti_feature_field_domain_expression *left, *right;
	uint32_t child;

	if (left_index == UINT32_MAX || right_index == UINT32_MAX)
		return left_index == right_index;
	if (left_index >= left_alternative->expression_count ||
	    right_index >= right_alternative->expression_count || depth > 128U)
		return 0;
	left = &bindings->expressions[left_alternative->first_expression + left_index];
	right = &bindings->expressions[right_alternative->first_expression + right_index];
	if (!optional_same(left->type, right->type) ||
	    !optional_same(left->name, right->name) ||
	    !optional_same(left->op, right->op) ||
	    !optional_same(left->value, right->value) ||
	    !optional_same(left->role, right->role) ||
	    !optional_same(left->register_state, right->register_state) ||
	    !optional_same(left->register_name, right->register_name) ||
	    !optional_same(left->field_name, right->field_name) ||
	    left->scalar_kind != right->scalar_kind ||
	    left->integer != right->integer || left->boolean != right->boolean ||
	    left->child_count != right->child_count)
		return 0;
	if (left->child_count &&
	    (left->first_child > left_alternative->expression_child_count ||
	     left->child_count > left_alternative->expression_child_count -
		left->first_child ||
	     right->first_child > right_alternative->expression_child_count ||
	     right->child_count > right_alternative->expression_child_count -
		right->first_child))
		return 0;
	for (child = 0; child < left->child_count; child++)
		if (!normalized_expression_equal(bindings, left_alternative,
			bindings->expression_children[
				left_alternative->first_expression_child +
				left->first_child + child],
			right_alternative,
			bindings->expression_children[
				right_alternative->first_expression_child +
				right->first_child + child], depth + 1U))
			return 0;
	return 1;
}

static int normalized_alternative_domains_equal(
	const struct orlix_tcti_feature_field_domain_bindings *bindings,
	uint32_t left_index, uint32_t right_index)
{
	const struct orlix_tcti_feature_field_domain_alternative *left =
		&bindings->alternatives[left_index];
	const struct orlix_tcti_feature_field_domain_alternative *right =
		&bindings->alternatives[right_index];
	uint32_t index;

	if (left->range_count != right->range_count ||
	    left->valueset_count != right->valueset_count ||
	    left->domain_count != right->domain_count ||
	    left->link_count != right->link_count ||
	    left->value_candidate_count != right->value_candidate_count ||
	    left->constraint_count != right->constraint_count ||
	    left->constraint_item_count != right->constraint_item_count ||
	    left->constraint_candidate_count != right->constraint_candidate_count)
		return 0;
	for (index = 0; index < left->range_count; index++) {
		const struct orlix_tcti_feature_field_domain_range *left_item =
			&bindings->ranges[left->first_range + index];
		const struct orlix_tcti_feature_field_domain_range *right_item =
			&bindings->ranges[right->first_range + index];
		if (left_item->start != right_item->start ||
		    left_item->width != right_item->width)
			return 0;
	}
	for (index = 0; index < left->valueset_count; index++)
		if (!optional_same(bindings->valuesets[left->first_valueset + index].type,
				   bindings->valuesets[right->first_valueset + index].type))
			return 0;
	for (index = 0; index < left->domain_count; index++) {
		const struct orlix_tcti_feature_field_domain_node *left_item =
			&bindings->domains[left->first_domain + index];
		const struct orlix_tcti_feature_field_domain_node *right_item =
			&bindings->domains[right->first_domain + index];
		if (!optional_same(left_item->type, right_item->type) ||
		    !optional_same(left_item->value, right_item->value) ||
		    !optional_same(left_item->start, right_item->start) ||
		    !optional_same(left_item->end, right_item->end) ||
		    !optional_same(left_item->link, right_item->link) ||
		    !optional_same(left_item->meaning, right_item->meaning) ||
		    left_item->parent_domain != right_item->parent_domain ||
		    left_item->first_child_domain != right_item->first_child_domain ||
		    left_item->child_domain_count != right_item->child_domain_count ||
		    left_item->valueset_index != right_item->valueset_index ||
		    left_item->nested_valueset_index != right_item->nested_valueset_index ||
		    left_item->first_link != right_item->first_link ||
		    left_item->link_count != right_item->link_count ||
		    !normalized_expression_equal(bindings, left,
			left_item->condition_expression, right,
			right_item->condition_expression, 0U))
			return 0;
	}
	for (index = 0; index < left->link_count; index++) {
		const struct orlix_tcti_feature_field_domain_link *left_item =
			&bindings->links[left->first_link + index];
		const struct orlix_tcti_feature_field_domain_link *right_item =
			&bindings->links[right->first_link + index];
		if (!optional_same(left_item->key, right_item->key) ||
		    !optional_same(left_item->value, right_item->value) ||
		    left_item->domain_index != right_item->domain_index)
			return 0;
	}
#define SAME_NUMERIC_SLICE(member, count_member, first_member) \
	(!left->count_member || !memcmp( \
		&bindings->member[left->first_member], \
		&bindings->member[right->first_member], \
		left->count_member * sizeof(bindings->member[0])))
	if (!SAME_NUMERIC_SLICE(value_candidates, value_candidate_count,
				first_value_candidate) ||
	    !SAME_NUMERIC_SLICE(constraints, constraint_count, first_constraint) ||
	    !SAME_NUMERIC_SLICE(constraint_items, constraint_item_count,
				first_constraint_item) ||
	    !SAME_NUMERIC_SLICE(constraint_candidates, constraint_candidate_count,
				first_constraint_candidate)) {
#undef SAME_NUMERIC_SLICE
		return 0;
	}
#undef SAME_NUMERIC_SLICE
	return 1;
}

static int model_is_valid(const struct orlix_tcti_feature_model *features,
	const struct orlix_tcti_register_model *registers,
	struct orlix_tcti_feature_field_domain_binding_error *error)
{
	if ((features->node_count && !features->nodes) ||
	    (registers->register_count && !registers->registers) ||
	    (registers->field_count && !registers->fields) ||
	    (registers->fieldset_count && !registers->fieldsets) ||
	    (registers->range_count && !registers->ranges) ||
	    (registers->domain_count && !registers->domains) ||
	    (registers->valueset_count && !registers->valuesets) ||
	    (registers->constraint_count && !registers->constraints) ||
	    (registers->constraint_item_count && !registers->constraint_items) ||
	    registers->field_value_relation_count != registers->field_count ||
	    (registers->field_value_relation_count &&
	     !registers->field_value_relations) ||
	    (registers->field_value_candidate_count &&
	     !registers->field_value_candidates) ||
	    (registers->field_constraint_candidate_count &&
	     !registers->field_constraint_candidates) ||
	    (registers->link_count && !registers->links) ||
	    (registers->expression_count && !registers->expressions) ||
	    (registers->expression_child_count &&
	     !registers->expression_children)) {
		binding_fail(error,
			ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_INVALID_MODEL, 0,
			"feature or register model lacks normalized domain inputs");
		return 0;
	}
	return 1;
}

int orlix_tcti_target_feature_field_domain_bindings_build(
	const struct orlix_tcti_feature_model *features,
	const struct orlix_tcti_register_model *registers,
	struct orlix_tcti_feature_field_domain_bindings *bindings,
	struct orlix_tcti_feature_field_domain_binding_error *error)
{
	size_t field_count = 0, node_index, result_index = 0;

	if (!features || !registers || !bindings) {
		binding_fail(error,
			ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_INVALID_ARGUMENT, 0,
			"features, registers, and output bindings are required");
		return -1;
	}
	memset(bindings, 0, sizeof(*bindings));
	if (error)
		memset(error, 0, sizeof(*error));
	if (!model_is_valid(features, registers, error))
		return -1;
	for (node_index = 0; node_index < features->node_count; node_index++)
		if (features->nodes[node_index].kind == ORLIX_TCTI_FEATURE_FIELD)
			field_count++;
	if (!field_count)
		return 0;
	if (field_count > SIZE_MAX / sizeof(*bindings->items)) {
		binding_fail(error,
			ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_NO_MEMORY, 0,
			"field binding allocation overflows");
		return -1;
	}
	bindings->items = calloc(field_count, sizeof(*bindings->items));
	if (!bindings->items) {
		binding_fail(error,
			ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_NO_MEMORY, 0,
			"cannot allocate field bindings");
		return -1;
	}
	bindings->count = field_count;
	for (node_index = 0; node_index < features->node_count; node_index++) {
		const struct orlix_tcti_feature_node *node =
			&features->nodes[node_index];
		struct orlix_tcti_feature_field_domain_binding *binding;
		size_t register_index, matched_register = SIZE_MAX;
		size_t matched_fields = 0, field_index, canonical_field = SIZE_MAX;
		int ambiguous_semantics = 0;
		uint64_t resolution = DOMAIN_HASH_OFFSET;

		if (node->kind != ORLIX_TCTI_FEATURE_FIELD)
			continue;
		binding = &bindings->items[result_index++];
		binding->feature_node_index = (uint32_t)node_index;
		binding->identity_group_index = identity_group(features, node_index,
			&binding->occurrence_count);
		binding->register_index = UINT32_MAX;
		binding->field_index = UINT32_MAX;
		binding->value_relation_index = UINT32_MAX;
		binding->first_alternative = (uint32_t)bindings->alternative_count;
		binding->feature_provenance = node->field.provenance;
		if (!node->field.state || !node->field.register_name ||
		    !node->field.selector) {
			binding->disposition = ORLIX_TCTI_FEATURE_FIELD_DOMAIN_INVALID_MODEL;
			continue;
		}
		for (register_index = 0;
		     register_index < registers->register_count; register_index++) {
			const struct orlix_tcti_register_identity *reg =
				&registers->registers[register_index];

			if (!same(node->field.state, reg->state) ||
			    !same(node->field.register_name, reg->name))
				continue;
			if (matched_register != SIZE_MAX) {
				binding->disposition =
					ORLIX_TCTI_FEATURE_FIELD_DOMAIN_AMBIGUOUS_REGISTER;
				matched_register = SIZE_MAX;
				break;
			}
			matched_register = register_index;
		}
		if (matched_register == SIZE_MAX) {
			if (binding->disposition !=
			    ORLIX_TCTI_FEATURE_FIELD_DOMAIN_AMBIGUOUS_REGISTER)
				binding->disposition =
					ORLIX_TCTI_FEATURE_FIELD_DOMAIN_MISSING_REGISTER;
			continue;
		}
		binding->register_index = (uint32_t)matched_register;
		binding->register_source_offset =
			registers->registers[matched_register].source_offset;
		binding->register_source_length =
			registers->registers[matched_register].source_length;
		for (field_index = 0; field_index < registers->field_count;
		     field_index++) {
			struct orlix_tcti_feature_field_normalized_domain candidate;
			const struct orlix_tcti_field_identity *field =
				&registers->fields[field_index];

			if (field->register_index != matched_register ||
			    !same(node->field.selector, field->name))
				continue;
			if (normalize_field(registers, (uint32_t)field_index, &candidate,
					    error))
				goto fail;
			if (copy_alternative(registers, (uint32_t)field_index,
					     bindings, error))
				goto fail;
			binding->alternative_count++;
			if (!matched_fields) {
				/* This becomes canonical only if every later match is equal. */
				binding->domain = candidate;
				canonical_field = field_index;
			} else if (!domains_equivalent(&binding->domain, &candidate) ||
				   !normalized_alternative_domains_equal(bindings,
					binding->first_alternative,
					(uint32_t)bindings->alternative_count - 1U) ||
				   !exact_field_semantics_equal(registers,
					(uint32_t)canonical_field,
					(uint32_t)field_index)) {
				ambiguous_semantics = 1;
			}
			matched_fields++;
			resolution = hash_u64(resolution, candidate.semantic_identity);
			resolution = hash_u64(resolution, field_index);
			resolution = hash_u64(resolution, candidate.fieldset_index);
			resolution = hash_u64(resolution, field->source_offset);
			resolution = hash_u64(resolution, field->source_length);
			resolution = hash_u64(resolution,
				candidate.fieldset_source_offset);
			resolution = hash_u64(resolution,
				candidate.fieldset_source_length);
			resolution = hash_u64(resolution,
				candidate.fieldset_condition_offset);
			resolution = hash_u64(resolution,
				candidate.fieldset_condition_length);
			if (expression_hash(registers,
					candidate.fieldset_condition_expression, 0U,
					&resolution)) {
				binding_fail(error,
					ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_INVALID_MODEL,
					candidate.fieldset_condition_offset,
					"fieldset condition is malformed");
				goto fail;
			}
		}
		if (!matched_fields) {
			binding->disposition =
				ORLIX_TCTI_FEATURE_FIELD_DOMAIN_MISSING_FIELD;
			continue;
		}
		if (ambiguous_semantics) {
			binding->disposition =
				ORLIX_TCTI_FEATURE_FIELD_DOMAIN_AMBIGUOUS_FIELD;
			continue;
		}
		binding->field_index = (uint32_t)canonical_field;
		binding->value_relation_index = (uint32_t)canonical_field;
		binding->domain.equivalent_field_count = (uint32_t)matched_fields;
		binding->domain.resolution_identity =
			hash_u64(resolution, matched_fields);
		binding->field_source_offset =
			registers->fields[canonical_field].source_offset;
		binding->field_source_length =
			registers->fields[canonical_field].source_length;
		binding->value_relation_source_offset =
			registers->field_value_relations[canonical_field].source_offset;
		binding->value_relation_source_length =
			registers->field_value_relations[canonical_field].source_length;
		binding->disposition = ORLIX_TCTI_FEATURE_FIELD_DOMAIN_MAPPED;
	}
	return 0;
fail:
	orlix_tcti_target_feature_field_domain_bindings_destroy(bindings);
	return -1;
}

void orlix_tcti_target_feature_field_domain_bindings_destroy(
	struct orlix_tcti_feature_field_domain_bindings *bindings)
{
	if (!bindings)
		return;
	free(bindings->items);
	free(bindings->alternatives);
	free(bindings->ranges);
	free(bindings->valuesets);
	free(bindings->domains);
	free(bindings->links);
	free(bindings->expressions);
	free(bindings->expression_children);
	free(bindings->value_candidates);
	free(bindings->constraints);
	free(bindings->constraint_items);
	free(bindings->constraint_candidates);
	memset(bindings, 0, sizeof(*bindings));
}
