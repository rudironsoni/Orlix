/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_feature_field_domain_binding_artifact_generator.h"

#include <inttypes.h>
#include <stdint.h>
#include <string.h>

static uint64_t identity_byte(uint64_t identity, unsigned char byte)
{
	return (identity ^ byte) * UINT64_C(1099511628211);
}

static uint64_t identity_u64(uint64_t identity, uint64_t value)
{
	unsigned int index;

	for (index = 0; index < 8U; index++)
		identity = identity_byte(identity,
			(unsigned char)(value >> (index * 8U)));
	return identity;
}

static uint64_t identity_string(uint64_t identity, const char *value)
{
	size_t length = value ? strlen(value) : 0U;
	size_t index;

	identity = identity_u64(identity, length);
	for (index = 0; index < length; index++)
		identity = identity_byte(identity, (unsigned char)value[index]);
	return identity;
}

static uint64_t binding_identity(
	const struct orlix_tcti_feature_field_domain_bindings *bindings,
	const struct orlix_tcti_feature_model *features)
{
	uint64_t identity = UINT64_C(1469598103934665603);
	size_t index;

	identity = identity_u64(identity, bindings->count);
	for (index = 0; index < bindings->count; index++) {
		const struct orlix_tcti_feature_field_domain_binding *binding =
			&bindings->items[index];
		const struct orlix_tcti_feature_node *node;

		if (binding->feature_node_index >= features->node_count)
			return 0;
		node = &features->nodes[binding->feature_node_index];
		identity = identity_u64(identity, binding->feature_node_index);
		identity = identity_u64(identity, binding->identity_group_index);
		identity = identity_u64(identity, binding->occurrence_count);
		identity = identity_u64(identity, binding->register_index);
		identity = identity_u64(identity, binding->field_index);
		identity = identity_u64(identity, binding->value_relation_index);
		identity = identity_u64(identity, binding->disposition);
		identity = identity_string(identity, node->field.state);
		identity = identity_string(identity, node->field.register_name);
		identity = identity_string(identity, node->field.selector);
		identity = identity_u64(identity, node->field.instance.kind);
		identity = identity_u64(identity, node->field.instance.provenance.offset);
		identity = identity_u64(identity, node->field.instance.provenance.length);
		identity = identity_u64(identity, node->field.slices.kind);
		identity = identity_u64(identity, node->field.slices.provenance.offset);
		identity = identity_u64(identity, node->field.slices.provenance.length);
		identity = identity_u64(identity, binding->feature_provenance.offset);
		identity = identity_u64(identity, binding->feature_provenance.length);
		identity = identity_u64(identity, binding->register_source_offset);
		identity = identity_u64(identity, binding->register_source_length);
		identity = identity_u64(identity, binding->field_source_offset);
		identity = identity_u64(identity, binding->field_source_length);
		identity = identity_u64(identity, binding->value_relation_source_offset);
		identity = identity_u64(identity, binding->value_relation_source_length);
		identity = identity_u64(identity, binding->domain.fieldset_index);
		identity = identity_u64(identity, binding->domain.equivalent_field_count);
		identity = identity_u64(identity, binding->domain.register_width);
		identity = identity_u64(identity, binding->domain.field_width);
		identity = identity_u64(identity, binding->domain.range_count);
		identity = identity_u64(identity, binding->domain.value_member_count);
		identity = identity_u64(identity, binding->domain.range_member_count);
		identity = identity_u64(identity, binding->domain.relation_value_count);
		identity = identity_u64(identity,
			binding->domain.relation_constraint_count);
		identity = identity_u64(identity,
			binding->domain.field_condition_expression);
		identity = identity_u64(identity,
			binding->domain.wrapper_condition_expression);
		identity = identity_u64(identity,
			binding->domain.fieldset_condition_expression);
		identity = identity_u64(identity, binding->domain.type);
		identity = identity_u64(identity, binding->domain.signedness);
		identity = identity_u64(identity, binding->domain.member_kind);
		identity = identity_u64(identity, binding->domain.semantic_identity);
		identity = identity_u64(identity, binding->domain.resolution_identity);
		identity = identity_u64(identity, binding->domain.fieldset_source_offset);
		identity = identity_u64(identity, binding->domain.fieldset_source_length);
		identity = identity_u64(identity, binding->domain.field_condition_offset);
		identity = identity_u64(identity, binding->domain.field_condition_length);
		identity = identity_u64(identity,
			binding->domain.wrapper_condition_offset);
		identity = identity_u64(identity,
			binding->domain.wrapper_condition_length);
		identity = identity_u64(identity,
			binding->domain.fieldset_condition_offset);
		identity = identity_u64(identity,
			binding->domain.fieldset_condition_length);
	}
	return identity;
}

static int emit_c_string(FILE *output, const char *string)
{
	const unsigned char *cursor = (const unsigned char *)(string ? string : "");

	if (fputc('"', output) == EOF)
		return -1;
	while (*cursor) {
		if (*cursor == '"' || *cursor == '\\') {
			if (fputc('\\', output) == EOF || fputc(*cursor, output) == EOF)
				return -1;
		} else if (*cursor >= 0x20U && *cursor <= 0x7eU) {
			if (fputc(*cursor, output) == EOF)
				return -1;
		} else if (fprintf(output, "\\%03o", (unsigned int)*cursor) < 0) {
			return -1;
		}
		cursor++;
	}
	return fputc('"', output) == EOF ? -1 : 0;
}

static int emit_optional_c_string(FILE *output, const char *string)
{
	return string ? emit_c_string(output, string) :
		(fputs("NULL", output) == EOF ? -1 : 0);
}

static int count_bindings(const struct orlix_tcti_feature_field_domain_bindings *bindings,
	size_t *groups, size_t *mapped, size_t *ambiguous_field)
{
	size_t index;

	*groups = 0;
	*mapped = 0;
	*ambiguous_field = 0;
	for (index = 0; index < bindings->count; index++) {
		const struct orlix_tcti_feature_field_domain_binding *binding =
			&bindings->items[index];

		if (binding->identity_group_index == *groups)
			(*groups)++;
		else if (binding->identity_group_index >= *groups)
			return -1;
		if (binding->disposition == ORLIX_TCTI_FEATURE_FIELD_DOMAIN_MAPPED)
			(*mapped)++;
		else if (binding->disposition ==
			 ORLIX_TCTI_FEATURE_FIELD_DOMAIN_AMBIGUOUS_FIELD)
			(*ambiguous_field)++;
		else
			return -1;
	}
	return 0;
}

static int span_valid(size_t offset, size_t length, size_t source_length)
{
	return length && offset < source_length && length <= source_length - offset;
}

static int exact_span(size_t actual_offset, size_t actual_length,
	size_t expected_offset, size_t expected_length)
{
	return actual_offset == expected_offset && actual_length == expected_length;
}

static int exact_binding(
	const struct orlix_tcti_feature_field_domain_binding *actual,
	const struct orlix_tcti_feature_field_domain_binding *expected)
{
	return actual->feature_node_index == expected->feature_node_index &&
		actual->identity_group_index == expected->identity_group_index &&
		actual->occurrence_count == expected->occurrence_count &&
		actual->register_index == expected->register_index &&
		actual->field_index == expected->field_index &&
		actual->value_relation_index == expected->value_relation_index &&
		actual->first_alternative == expected->first_alternative &&
		actual->alternative_count == expected->alternative_count &&
		actual->disposition == expected->disposition &&
		!memcmp(&actual->domain, &expected->domain, sizeof(actual->domain)) &&
		!memcmp(&actual->feature_provenance, &expected->feature_provenance,
			sizeof(actual->feature_provenance)) &&
		actual->register_source_offset == expected->register_source_offset &&
		actual->register_source_length == expected->register_source_length &&
		actual->field_source_offset == expected->field_source_offset &&
		actual->field_source_length == expected->field_source_length &&
		actual->value_relation_source_offset ==
			expected->value_relation_source_offset &&
		actual->value_relation_source_length ==
			expected->value_relation_source_length;
}

static int optional_string_equal(const char *left, const char *right)
{
	return (!left && !right) || (left && right && !strcmp(left, right));
}

static int exact_domain_node(
	const struct orlix_tcti_feature_field_domain_node *left,
	const struct orlix_tcti_feature_field_domain_node *right)
{
	return optional_string_equal(left->type, right->type) &&
		optional_string_equal(left->value, right->value) &&
		optional_string_equal(left->start, right->start) &&
		optional_string_equal(left->end, right->end) &&
		optional_string_equal(left->link, right->link) &&
		optional_string_equal(left->meaning, right->meaning) &&
		left->parent_domain == right->parent_domain &&
		left->condition_expression == right->condition_expression &&
		left->first_child_domain == right->first_child_domain &&
		left->child_domain_count == right->child_domain_count &&
		left->valueset_index == right->valueset_index &&
		left->nested_valueset_index == right->nested_valueset_index &&
		left->first_link == right->first_link &&
		left->link_count == right->link_count;
}

static int exact_expression(
	const struct orlix_tcti_feature_field_domain_expression *left,
	const struct orlix_tcti_feature_field_domain_expression *right)
{
	return optional_string_equal(left->type, right->type) &&
		optional_string_equal(left->name, right->name) &&
		optional_string_equal(left->op, right->op) &&
		optional_string_equal(left->value, right->value) &&
		optional_string_equal(left->role, right->role) &&
		optional_string_equal(left->register_state, right->register_state) &&
		optional_string_equal(left->register_name, right->register_name) &&
		optional_string_equal(left->field_name, right->field_name) &&
		left->first_child == right->first_child &&
		left->child_count == right->child_count &&
		left->scalar_kind == right->scalar_kind &&
		left->integer == right->integer && left->boolean == right->boolean &&
		left->parent_expression == right->parent_expression &&
		left->instance_offset == right->instance_offset &&
		left->instance_length == right->instance_length &&
		left->slices_offset == right->slices_offset &&
		left->slices_length == right->slices_length &&
		left->source_offset == right->source_offset &&
		left->source_length == right->source_length;
}

static int exact_graph(
	const struct orlix_tcti_feature_field_domain_bindings *left,
	const struct orlix_tcti_feature_field_domain_bindings *right)
{
	size_t index;

	if (left->alternative_count != right->alternative_count ||
	    left->range_count != right->range_count ||
	    left->valueset_count != right->valueset_count ||
	    left->domain_count != right->domain_count ||
	    left->link_count != right->link_count ||
	    left->expression_count != right->expression_count ||
	    left->expression_child_count != right->expression_child_count ||
	    left->value_candidate_count != right->value_candidate_count ||
	    left->constraint_count != right->constraint_count ||
	    left->constraint_item_count != right->constraint_item_count ||
	    left->constraint_candidate_count != right->constraint_candidate_count)
		return 0;
	for (index = 0; index < left->valueset_count; index++)
		if (!optional_string_equal(left->valuesets[index].type,
				right->valuesets[index].type))
			return 0;
	for (index = 0; index < left->domain_count; index++)
		if (!exact_domain_node(&left->domains[index], &right->domains[index]))
			return 0;
	for (index = 0; index < left->link_count; index++)
		if (!optional_string_equal(left->links[index].key,
				right->links[index].key) ||
		    !optional_string_equal(left->links[index].value,
				right->links[index].value) ||
		    left->links[index].domain_index != right->links[index].domain_index)
			return 0;
	for (index = 0; index < left->expression_count; index++)
		if (!exact_expression(&left->expressions[index],
				      &right->expressions[index]))
			return 0;
	return (!left->alternative_count ||
		!memcmp(left->alternatives, right->alternatives,
			left->alternative_count * sizeof(*left->alternatives))) &&
		(!left->range_count ||
		 !memcmp(left->ranges, right->ranges,
			 left->range_count * sizeof(*left->ranges))) &&
		(!left->expression_child_count ||
		 !memcmp(left->expression_children, right->expression_children,
			 left->expression_child_count * sizeof(*left->expression_children))) &&
		(!left->value_candidate_count ||
		 !memcmp(left->value_candidates, right->value_candidates,
			 left->value_candidate_count * sizeof(*left->value_candidates))) &&
		(!left->constraint_count ||
		 !memcmp(left->constraints, right->constraints,
			 left->constraint_count * sizeof(*left->constraints))) &&
		(!left->constraint_item_count ||
		 !memcmp(left->constraint_items, right->constraint_items,
			 left->constraint_item_count * sizeof(*left->constraint_items))) &&
		(!left->constraint_candidate_count ||
		 !memcmp(left->constraint_candidates, right->constraint_candidates,
			 left->constraint_candidate_count *
			 sizeof(*left->constraint_candidates)));
}

static int graph_storage_valid(
	const struct orlix_tcti_feature_field_domain_bindings *bindings)
{
	return (!bindings->alternative_count || bindings->alternatives) &&
		(!bindings->range_count || bindings->ranges) &&
		(!bindings->valueset_count || bindings->valuesets) &&
		(!bindings->domain_count || bindings->domains) &&
		(!bindings->link_count || bindings->links) &&
		(!bindings->expression_count || bindings->expressions) &&
		(!bindings->expression_child_count || bindings->expression_children) &&
		(!bindings->value_candidate_count || bindings->value_candidates) &&
		(!bindings->constraint_count || bindings->constraints) &&
		(!bindings->constraint_item_count || bindings->constraint_items) &&
		(!bindings->constraint_candidate_count ||
		 bindings->constraint_candidates);
}

enum orlix_tcti_feature_field_domain_binding_artifact_error
orlix_tcti_target_feature_field_domain_binding_artifact_validate(
	const struct orlix_tcti_feature_model *features, size_t feature_source_length,
	const struct orlix_tcti_register_model *registers, size_t register_source_length,
	const struct orlix_tcti_feature_field_domain_bindings *bindings
)
{
	size_t index;
	struct orlix_tcti_feature_field_domain_bindings expected = { 0 };
	struct orlix_tcti_feature_field_domain_binding_error error = { 0 };

	if (!features || !feature_source_length || feature_source_length > UINT32_MAX ||
	    !registers ||
	    !register_source_length || !bindings ||
	    register_source_length > UINT32_MAX ||
	    (bindings->count && !bindings->items))
		return ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_ARGUMENT;
	if (!graph_storage_valid(bindings))
		return ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_BINDING;
	if (orlix_tcti_target_feature_field_domain_bindings_build(features, registers,
							 &expected, &error))
		return ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_BINDING;
	if (expected.count != bindings->count) {
		orlix_tcti_target_feature_field_domain_bindings_destroy(&expected);
		return ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_BINDING;
	}
	if (!exact_graph(bindings, &expected)) {
		orlix_tcti_target_feature_field_domain_bindings_destroy(&expected);
		return ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_BINDING;
	}
	for (index = 0; index < bindings->count; index++)
		if (!exact_binding(&bindings->items[index], &expected.items[index])) {
			orlix_tcti_target_feature_field_domain_bindings_destroy(&expected);
			return ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_BINDING;
		}
	orlix_tcti_target_feature_field_domain_bindings_destroy(&expected);
	for (index = 0; index < bindings->count; index++) {
		const struct orlix_tcti_feature_field_domain_binding *binding =
			&bindings->items[index];
		const struct orlix_tcti_feature_node *node;

		if (binding->feature_node_index >= features->node_count)
			return ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_BINDING;
		node = &features->nodes[binding->feature_node_index];
		if (node->kind != ORLIX_TCTI_FEATURE_FIELD ||
		    !exact_span(binding->feature_provenance.offset,
			binding->feature_provenance.length,
			node->field.provenance.offset,
			node->field.provenance.length) ||
		    node->field.instance.kind > ORLIX_TCTI_FEATURE_FIELD_QUALIFIER_NULL ||
		    node->field.slices.kind > ORLIX_TCTI_FEATURE_FIELD_QUALIFIER_NULL)
			return ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_BINDING;
		if (!span_valid(binding->feature_provenance.offset,
				binding->feature_provenance.length, feature_source_length) ||
		    !span_valid(node->field.instance.provenance.offset,
				node->field.instance.provenance.length,
				feature_source_length) ||
		    !span_valid(node->field.slices.provenance.offset,
				node->field.slices.provenance.length,
				feature_source_length))
			return ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_SOURCE_SPAN;
		if (binding->disposition == ORLIX_TCTI_FEATURE_FIELD_DOMAIN_MAPPED) {
			const struct orlix_tcti_register_identity *reg;
			const struct orlix_tcti_field_identity *field;
			const struct orlix_tcti_register_field_value_relation *relation;

			if (binding->register_index >= registers->register_count ||
			    binding->field_index >= registers->field_count ||
			    binding->value_relation_index >=
				registers->field_value_relation_count)
				return ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_BINDING;
			reg = &registers->registers[binding->register_index];
			field = &registers->fields[binding->field_index];
			relation = &registers->field_value_relations[
				binding->value_relation_index];
			if (field->register_index != binding->register_index ||
			    relation->field_index != binding->field_index ||
			    !exact_span(binding->register_source_offset,
				binding->register_source_length, reg->source_offset,
				reg->source_length) ||
			    !exact_span(binding->field_source_offset,
				binding->field_source_length, field->source_offset,
				field->source_length) ||
			    !exact_span(binding->value_relation_source_offset,
				binding->value_relation_source_length,
				relation->source_offset, relation->source_length))
				return ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_BINDING;
			if (!span_valid(binding->register_source_offset,
					binding->register_source_length,
					register_source_length) ||
			    !span_valid(binding->field_source_offset,
					binding->field_source_length,
					register_source_length) ||
			    !span_valid(binding->value_relation_source_offset,
					binding->value_relation_source_length,
					register_source_length))
				return ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_SOURCE_SPAN;
		} else if (binding->disposition ==
			   ORLIX_TCTI_FEATURE_FIELD_DOMAIN_AMBIGUOUS_FIELD) {
			const struct orlix_tcti_register_identity *reg;

			if (binding->register_index >= registers->register_count ||
			    binding->field_index != UINT32_MAX ||
			    binding->value_relation_index != UINT32_MAX ||
			    binding->field_source_offset ||
			    binding->field_source_length ||
			    binding->value_relation_source_offset ||
			    binding->value_relation_source_length)
				return ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_BINDING;
			reg = &registers->registers[binding->register_index];
			if (!exact_span(binding->register_source_offset,
					binding->register_source_length,
					reg->source_offset, reg->source_length))
				return ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_BINDING;
			if (!span_valid(binding->register_source_offset,
					binding->register_source_length,
					register_source_length))
				return ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_SOURCE_SPAN;
		} else {
			return ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_BINDING;
		}
	}
	return ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_OK;
}

static enum orlix_tcti_feature_field_domain_binding_artifact_error emit_bindings(
	const struct orlix_tcti_feature_model *features,
	size_t feature_source_length, const struct orlix_tcti_register_model *registers,
	size_t register_source_length, FILE *output)
{
	struct orlix_tcti_feature_field_domain_bindings bindings = { 0 };
	struct orlix_tcti_feature_field_domain_binding_error binding_error = { 0 };
	size_t groups, mapped, ambiguous_field, index;
	uint64_t identity;
	enum orlix_tcti_feature_field_domain_binding_artifact_error validation;

	if (orlix_tcti_target_feature_field_domain_bindings_build(features, registers,
							      &bindings, &binding_error))
		return ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_BINDING;
	if (count_bindings(&bindings, &groups, &mapped, &ambiguous_field) ||
	    bindings.count != ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_EXPECTED_OCCURRENCES ||
	    groups != ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_EXPECTED_IDENTITY_GROUPS ||
	    mapped != ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_EXPECTED_MAPPED ||
	    ambiguous_field !=
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_EXPECTED_AMBIGUOUS_FIELD ||
	    bindings.alternative_count !=
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_EXPECTED_ALTERNATIVES) {
		orlix_tcti_target_feature_field_domain_bindings_destroy(&bindings);
		return ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_COUNT_MISMATCH;
	}
	validation = orlix_tcti_target_feature_field_domain_binding_artifact_validate(
		features, feature_source_length, registers, register_source_length,
		&bindings);
	if (validation != ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_OK) {
		orlix_tcti_target_feature_field_domain_bindings_destroy(&bindings);
		return validation;
	}
	identity = binding_identity(&bindings, features);
	if (!identity) {
		orlix_tcti_target_feature_field_domain_bindings_destroy(&bindings);
		return ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_BINDING;
	}
	if (fputs("/* SPDX-License-Identifier: BSD-3-Clause */\n"
		  "/* Generated by target_feature_field_domain_binding_artifact_generator.c. Do not edit. */\n"
		  "ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_SOURCE(\"vFATAp1-A\", \"818\", "
		  "\"2026-06_rel\", \"2.9.5\", "
		  "\"633259000ffd3da32900bd0c0c1beae4a9eea7095c278f74d62a00c846b41187\", ",
		  output) == EOF ||
	    fprintf(output, "%zuU, \"5bd76c3c3ce90322eb4fd179675dafe82df2fd1cb789beee516e5b29c471b874\", %zuU)\n",
		    feature_source_length, register_source_length) < 0 ||
		    fprintf(output, "ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_COUNTS_V3(%zuU, %zuU, %zuU, %zuU, %zuU, %zuU, %zuU, %zuU, %zuU, %zuU, %zuU, %zuU, %zuU, %zuU, %zuU)\n",
			    bindings.count, groups, mapped, ambiguous_field,
			    bindings.alternative_count, bindings.range_count,
			    bindings.valueset_count, bindings.domain_count,
			    bindings.link_count, bindings.expression_count,
			    bindings.expression_child_count,
			    bindings.value_candidate_count, bindings.constraint_count,
			    bindings.constraint_item_count,
			    bindings.constraint_candidate_count) < 0 ||
	    fprintf(output, "ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_IDENTITY(UINT64_C(0x%016" PRIx64 "))\n",
		    identity) < 0)
		goto io;
	for (index = 0; index < bindings.range_count; index++)
		if (fprintf(output,
			"ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_RANGE_V3(%zuU, %" PRIu32 "U, %" PRIu32 "U)\n",
			index, bindings.ranges[index].start,
			bindings.ranges[index].width) < 0)
			goto io;
	for (index = 0; index < bindings.valueset_count; index++) {
		if (fprintf(output,
			"ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_VALUESET_V3(%zuU, ",
			index) < 0 ||
		    emit_optional_c_string(output, bindings.valuesets[index].type) ||
		    fputs(")\n", output) == EOF)
			goto io;
	}
	for (index = 0; index < bindings.domain_count; index++) {
		const struct orlix_tcti_feature_field_domain_node *domain =
			&bindings.domains[index];
		if (fprintf(output,
			"ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_DOMAIN_V3(%zuU, ", index) < 0 ||
		    emit_optional_c_string(output, domain->type) ||
		    fputs(", ", output) == EOF ||
		    emit_optional_c_string(output, domain->value) ||
		    fputs(", ", output) == EOF ||
		    emit_optional_c_string(output, domain->start) ||
		    fputs(", ", output) == EOF ||
		    emit_optional_c_string(output, domain->end) ||
		    fputs(", ", output) == EOF ||
		    emit_optional_c_string(output, domain->link) ||
		    fputs(", ", output) == EOF ||
		    emit_optional_c_string(output, domain->meaning) ||
		    fprintf(output,
			", %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32
			"U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U)\n",
			domain->parent_domain, domain->condition_expression,
			domain->first_child_domain, domain->child_domain_count,
			domain->valueset_index, domain->nested_valueset_index,
			domain->first_link, domain->link_count) < 0)
			goto io;
	}
	for (index = 0; index < bindings.link_count; index++) {
		const struct orlix_tcti_feature_field_domain_link *link =
			&bindings.links[index];
		if (fprintf(output,
			"ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_LINK_V3(%zuU, ", index) < 0 ||
		    emit_optional_c_string(output, link->key) ||
		    fputs(", ", output) == EOF ||
		    emit_optional_c_string(output, link->value) ||
		    fprintf(output, ", %" PRIu32 "U)\n", link->domain_index) < 0)
			goto io;
	}
	for (index = 0; index < bindings.expression_count; index++) {
		const struct orlix_tcti_feature_field_domain_expression *expression =
			&bindings.expressions[index];
		if (fprintf(output,
			"ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_EXPRESSION_V3(%zuU, ", index) < 0 ||
		    emit_optional_c_string(output, expression->type) ||
		    fputs(", ", output) == EOF ||
		    emit_optional_c_string(output, expression->name) ||
		    fputs(", ", output) == EOF ||
		    emit_optional_c_string(output, expression->op) ||
		    fputs(", ", output) == EOF ||
		    emit_optional_c_string(output, expression->value) ||
		    fputs(", ", output) == EOF ||
		    emit_optional_c_string(output, expression->role) ||
		    fputs(", ", output) == EOF ||
		    emit_optional_c_string(output, expression->register_state) ||
		    fputs(", ", output) == EOF ||
		    emit_optional_c_string(output, expression->register_name) ||
		    fputs(", ", output) == EOF ||
		    emit_optional_c_string(output, expression->field_name) ||
		    fprintf(output,
			", %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRId64
			", %" PRIu32 "U, %" PRIu32 "U, %zuU, %zuU, %zuU, %zuU, %zuU, %zuU)\n",
			expression->first_child, expression->child_count,
			expression->scalar_kind, expression->integer, expression->boolean,
			expression->parent_expression, expression->instance_offset,
			expression->instance_length, expression->slices_offset,
			expression->slices_length, expression->source_offset,
			expression->source_length) < 0)
			goto io;
	}
	for (index = 0; index < bindings.expression_child_count; index++)
		if (fprintf(output,
			"ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_EXPRESSION_CHILD_V3(%zuU, %" PRIu32 "U)\n",
			index, bindings.expression_children[index]) < 0)
			goto io;
	for (index = 0; index < bindings.value_candidate_count; index++) {
		const struct orlix_tcti_feature_field_domain_value_candidate *candidate =
			&bindings.value_candidates[index];
		if (fprintf(output,
			"ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_VALUE_CANDIDATE_V3(%zuU, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U)\n",
			index, candidate->kind, candidate->valueset_index,
			candidate->first_domain, candidate->domain_count) < 0)
			goto io;
	}
	for (index = 0; index < bindings.constraint_count; index++) {
		const struct orlix_tcti_feature_field_domain_constraint *constraint =
			&bindings.constraints[index];
		if (fprintf(output,
			"ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_CONSTRAINT_V3(%zuU, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U)\n",
			index, constraint->kind, constraint->valueset_index,
			constraint->first_item, constraint->item_count) < 0)
			goto io;
	}
	for (index = 0; index < bindings.constraint_item_count; index++) {
		const struct orlix_tcti_feature_field_domain_constraint_item *item =
			&bindings.constraint_items[index];
		if (fprintf(output,
			"ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_CONSTRAINT_ITEM_V3(%zuU, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U)\n",
			index, item->constraint_index, item->valueset_index,
			item->domain_index) < 0)
			goto io;
	}
	for (index = 0; index < bindings.constraint_candidate_count; index++) {
		const struct orlix_tcti_feature_field_domain_constraint_candidate *candidate =
			&bindings.constraint_candidates[index];
		if (fprintf(output,
			"ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_CONSTRAINT_CANDIDATE_V3(%zuU, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U)\n",
			index, candidate->constraint_index, candidate->kind,
			candidate->first_domain, candidate->domain_count) < 0)
			goto io;
	}
	for (index = 0; index < bindings.alternative_count; index++) {
		const struct orlix_tcti_feature_field_domain_alternative *alternative =
			&bindings.alternatives[index];
		if (fprintf(output,
			"ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_ALTERNATIVE_V3(%zuU, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %zuU, %zuU, %zuU, %zuU, %zuU, %zuU, %zuU, %zuU, %zuU, %zuU, %zuU, %zuU)\n",
			index, alternative->field_index,
			alternative->value_relation_index, alternative->fieldset_index,
			alternative->field_condition_expression,
			alternative->wrapper_condition_expression,
			alternative->fieldset_condition_expression,
			alternative->relation_field_condition_expression,
			alternative->relation_wrapper_condition_expression,
			alternative->first_range, alternative->range_count,
			alternative->first_valueset, alternative->valueset_count,
			alternative->first_domain, alternative->domain_count,
			alternative->first_link, alternative->link_count,
			alternative->first_expression, alternative->expression_count,
			alternative->first_expression_child,
			alternative->expression_child_count,
			alternative->first_value_candidate,
			alternative->value_candidate_count,
			alternative->first_constraint, alternative->constraint_count,
			alternative->first_constraint_item,
			alternative->constraint_item_count,
			alternative->first_constraint_candidate,
			alternative->constraint_candidate_count,
			alternative->field_source_offset,
			alternative->field_source_length,
			alternative->value_relation_source_offset,
			alternative->value_relation_source_length,
			alternative->fieldset_source_offset,
			alternative->fieldset_source_length,
			alternative->field_condition_offset,
			alternative->field_condition_length,
			alternative->wrapper_condition_offset,
			alternative->wrapper_condition_length,
			alternative->fieldset_condition_offset,
			alternative->fieldset_condition_length) < 0)
			goto io;
	}
	for (index = 0; index < bindings.count; index++) {
		const struct orlix_tcti_feature_field_domain_binding *binding =
			&bindings.items[index];
		const struct orlix_tcti_feature_node *node =
			&features->nodes[binding->feature_node_index];

		if (fprintf(output, "ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_OCCURRENCE_V3("
			    "%zuU, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, "
			    "%" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %uU, ", index,
			    binding->feature_node_index, binding->identity_group_index,
			    binding->occurrence_count, binding->register_index,
			    binding->field_index, binding->value_relation_index,
			    (unsigned int)binding->disposition) < 0 ||
		    emit_c_string(output, node->field.state) || fputs(", ", output) == EOF ||
		    emit_c_string(output, node->field.register_name) ||
		    fputs(", ", output) == EOF ||
		    emit_c_string(output, node->field.selector) ||
		    fprintf(output, ", %uU, %zuU, %zuU, %uU, %zuU, %zuU, "
					     "%zuU, %zuU, %zuU, %zuU, %zuU, %zuU, %zuU, %zuU, "
					     "%" PRIu32 "U, %" PRIu32 "U, "
				     "%" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, "
				     "%" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, "
				     "%" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, "
				     "%" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, "
				     "%uU, %uU, %uU, UINT64_C(0x%016" PRIx64 "), "
				     "UINT64_C(0x%016" PRIx64 "), "
				     "%" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, "
				     "%" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, "
				     "%" PRIu32 "U, %" PRIu32 "U)\n",
			    (unsigned int)node->field.instance.kind,
			    node->field.instance.provenance.offset,
			    node->field.instance.provenance.length,
			    (unsigned int)node->field.slices.kind,
			    node->field.slices.provenance.offset,
			    node->field.slices.provenance.length,
			    binding->feature_provenance.offset,
			    binding->feature_provenance.length,
			    binding->register_source_offset,
			    binding->register_source_length,
			    binding->field_source_offset,
				     binding->field_source_length,
					     binding->value_relation_source_offset,
					     binding->value_relation_source_length,
					     binding->first_alternative,
					     binding->alternative_count,
				     binding->domain.fieldset_index,
				     binding->domain.equivalent_field_count,
				     binding->domain.register_width,
				     binding->domain.field_width,
				     binding->domain.range_count,
				     binding->domain.value_member_count,
				     binding->domain.range_member_count,
				     binding->domain.relation_value_count,
				     binding->domain.relation_constraint_count,
				     binding->domain.field_condition_expression,
				     binding->domain.wrapper_condition_expression,
				     binding->domain.fieldset_condition_expression,
				     (unsigned int)binding->domain.type,
				     (unsigned int)binding->domain.signedness,
				     (unsigned int)binding->domain.member_kind,
				     binding->domain.semantic_identity,
				     binding->domain.resolution_identity,
				     binding->domain.fieldset_source_offset,
				     binding->domain.fieldset_source_length,
				     binding->domain.field_condition_offset,
				     binding->domain.field_condition_length,
				     binding->domain.wrapper_condition_offset,
				     binding->domain.wrapper_condition_length,
				     binding->domain.fieldset_condition_offset,
				     binding->domain.fieldset_condition_length) < 0)
			goto io;
	}
	orlix_tcti_target_feature_field_domain_bindings_destroy(&bindings);
	return ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_OK;
io:
	orlix_tcti_target_feature_field_domain_bindings_destroy(&bindings);
	return ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_IO;
}

enum orlix_tcti_feature_field_domain_binding_artifact_error
orlix_tcti_target_feature_field_domain_binding_artifact_emit_model(
	const struct orlix_tcti_feature_model *features,
	size_t feature_source_length, const struct orlix_tcti_register_model *registers,
	size_t register_source_length, FILE *output)
{
	if (!features || !feature_source_length || !registers ||
	    !register_source_length || !output)
		return ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_ARGUMENT;
	return emit_bindings(features, feature_source_length, registers,
		register_source_length, output);
}

enum orlix_tcti_feature_field_domain_binding_artifact_error
orlix_tcti_target_feature_field_domain_binding_artifact_emit(
	const char *feature_source, size_t feature_length,
	const char *register_source, size_t register_length, FILE *output)
{
	struct orlix_tcti_feature_model features = { 0 };
	struct orlix_tcti_register_model registers = { 0 };
	struct orlix_tcti_feature_error feature_error = { 0 };
	struct orlix_tcti_register_model_error register_error = { 0 };
	enum orlix_tcti_feature_field_domain_binding_artifact_error result;

	if (!feature_source || !feature_length || !register_source ||
	    !register_length || !output)
		return ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_ARGUMENT;
	if (orlix_tcti_target_feature_model_import(feature_source, feature_length,
					     &features, &feature_error))
		return ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_FEATURE_IMPORT;
	if (orlix_tcti_register_model_import(register_source, register_length,
				       &registers, &register_error)) {
		orlix_tcti_target_feature_model_destroy(&features);
		return ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_REGISTER_IMPORT;
	}
	result = emit_bindings(&features, feature_length, &registers,
		register_length, output);
	orlix_tcti_register_model_destroy(&registers);
	orlix_tcti_target_feature_model_destroy(&features);
	return result;
}

const char *orlix_tcti_target_feature_field_domain_binding_artifact_error_name(
	enum orlix_tcti_feature_field_domain_binding_artifact_error error)
{
	switch (error) {
	case ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_OK: return "ok";
	case ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_ARGUMENT: return "invalid argument";
	case ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_FEATURE_IMPORT: return "feature import failed";
	case ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_REGISTER_IMPORT: return "register import failed";
	case ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_BINDING: return "field-domain binding failed";
	case ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_BINDING: return "field-domain binding violates artifact invariants";
	case ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_SOURCE_SPAN: return "source span is outside supplied source length";
	case ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_COUNT_MISMATCH: return "pinned field-domain census mismatch";
	case ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_IO: return "I/O failure";
	}
	return "unknown error";
}
