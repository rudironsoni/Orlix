/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_feature_field_domain_binding.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void binding_fail(struct orlix_tcti_feature_field_domain_binding_error *error,
	enum orlix_tcti_feature_field_domain_binding_error_code code, size_t offset,
	const char *format, ...)
{
	va_list ap;
	if (!error)
		return;
	error->code = code;
	error->offset = offset;
	va_start(ap, format);
	vsnprintf(error->message, sizeof(error->message), format, ap);
	va_end(ap);
}

static int same(const char *left, const char *right)
{
	return left && right && !strcmp(left, right);
}

static int same_field_identity(const struct orlix_tcti_feature_node *left,
	const struct orlix_tcti_feature_node *right)
{
	return left->kind == ORLIX_TCTI_FEATURE_FIELD &&
		right->kind == ORLIX_TCTI_FEATURE_FIELD &&
		same(left->field.state, right->field.state) &&
		same(left->field.register_name, right->field.register_name) &&
		same(left->field.selector, right->field.selector);
}

static uint32_t field_identity_group(const struct orlix_tcti_feature_model *features,
	size_t node_index, uint32_t *occurrence_count)
{
	const struct orlix_tcti_feature_node *node = &features->nodes[node_index];
	size_t prior, scan;
	uint32_t groups = 0;
	for (prior = 0; prior < node_index; prior++) {
		int first = 1;
		if (features->nodes[prior].kind != ORLIX_TCTI_FEATURE_FIELD)
			continue;
		for (scan = 0; scan < prior; scan++)
			if (same_field_identity(&features->nodes[prior],
				&features->nodes[scan])) {
				first = 0;
				break;
			}
		if (!first)
			continue;
		if (same_field_identity(node, &features->nodes[prior]))
			break;
		groups++;
	}
	*occurrence_count = 0;
	for (scan = 0; scan < features->node_count; scan++)
		if (same_field_identity(node, &features->nodes[scan]))
			(*occurrence_count)++;
	return groups;
}

static int model_is_valid(const struct orlix_tcti_feature_model *features,
	const struct orlix_tcti_register_model *registers,
	struct orlix_tcti_feature_field_domain_binding_error *error)
{
	if ((features->node_count && !features->nodes) ||
		(registers->register_count && !registers->registers) ||
		(registers->field_count && !registers->fields) ||
		registers->field_value_relation_count != registers->field_count ||
		(registers->field_value_relation_count &&
		 !registers->field_value_relations)) {
		binding_fail(error, ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_INVALID_MODEL, 0,
			"feature or register model has no index-preserving field relation");
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
		binding_fail(error, ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_INVALID_ARGUMENT,
			0, "features, registers, and output bindings are required");
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
		binding_fail(error, ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_NO_MEMORY, 0,
			"feature field binding allocation overflows");
		return -1;
	}
	bindings->items = calloc(field_count, sizeof(*bindings->items));
	if (!bindings->items) {
		binding_fail(error, ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_NO_MEMORY, 0,
			"cannot allocate feature field bindings");
		return -1;
	}
	bindings->count = field_count;
	for (node_index = 0; node_index < features->node_count; node_index++) {
		const struct orlix_tcti_feature_node *node = &features->nodes[node_index];
		struct orlix_tcti_feature_field_domain_binding *binding;
		size_t register_index, register_matches = 0, matched_register = 0;
		size_t field_index, field_matches = 0, matched_field = 0;
		if (node->kind != ORLIX_TCTI_FEATURE_FIELD)
			continue;
		binding = &bindings->items[result_index++];
		binding->feature_node_index = (uint32_t)node_index;
		binding->identity_group_index = field_identity_group(features, node_index,
			&binding->occurrence_count);
		binding->register_index = UINT32_MAX;
		binding->field_index = UINT32_MAX;
		binding->value_relation_index = UINT32_MAX;
		binding->feature_provenance = node->field.provenance;
		if (!node->field.state || !node->field.register_name ||
			!node->field.selector) {
			binding->disposition = ORLIX_TCTI_FEATURE_FIELD_DOMAIN_INVALID_MODEL;
			continue;
		}
		for (register_index = 0; register_index < registers->register_count;
		     register_index++) {
			const struct orlix_tcti_register_identity *identity =
				&registers->registers[register_index];
			if (same(node->field.state, identity->state) &&
			    same(node->field.register_name, identity->name)) {
				register_matches++;
				matched_register = register_index;
			}
		}
		if (!register_matches) {
			binding->disposition = ORLIX_TCTI_FEATURE_FIELD_DOMAIN_MISSING_REGISTER;
			continue;
		}
		if (register_matches != 1) {
			binding->disposition = ORLIX_TCTI_FEATURE_FIELD_DOMAIN_AMBIGUOUS_REGISTER;
			continue;
		}
		binding->register_index = (uint32_t)matched_register;
		binding->register_source_offset =
			registers->registers[matched_register].source_offset;
		binding->register_source_length =
			registers->registers[matched_register].source_length;
		for (field_index = 0; field_index < registers->field_count;
		     field_index++) {
			const struct orlix_tcti_field_identity *field =
				&registers->fields[field_index];
			if (field->register_index == matched_register &&
			    same(node->field.selector, field->name)) {
				field_matches++;
				matched_field = field_index;
			}
		}
		if (!field_matches) {
			binding->disposition = ORLIX_TCTI_FEATURE_FIELD_DOMAIN_MISSING_FIELD;
			continue;
		}
		if (field_matches != 1) {
			binding->disposition = ORLIX_TCTI_FEATURE_FIELD_DOMAIN_AMBIGUOUS_FIELD;
			continue;
		}
		binding->field_index = (uint32_t)matched_field;
		binding->field_source_offset = registers->fields[matched_field].source_offset;
		binding->field_source_length = registers->fields[matched_field].source_length;
		binding->value_relation_index = (uint32_t)matched_field;
		binding->value_relation_source_offset =
			registers->field_value_relations[matched_field].source_offset;
		binding->value_relation_source_length =
			registers->field_value_relations[matched_field].source_length;
		binding->disposition = ORLIX_TCTI_FEATURE_FIELD_DOMAIN_MAPPED;
	}
	return 0;
}

void orlix_tcti_target_feature_field_domain_bindings_destroy(
	struct orlix_tcti_feature_field_domain_bindings *bindings)
{
	if (!bindings)
		return;
	free(bindings->items);
	memset(bindings, 0, sizeof(*bindings));
}
