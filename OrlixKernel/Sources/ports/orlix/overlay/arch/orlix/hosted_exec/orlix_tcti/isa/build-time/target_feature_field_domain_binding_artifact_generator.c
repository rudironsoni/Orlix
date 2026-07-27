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

enum orlix_tcti_feature_field_domain_binding_artifact_error
orlix_tcti_target_feature_field_domain_binding_artifact_validate(
	const struct orlix_tcti_feature_model *features, size_t feature_source_length,
	const struct orlix_tcti_register_model *registers, size_t register_source_length,
	const struct orlix_tcti_feature_field_domain_bindings *bindings
)
{
	size_t index;

	if (!features || !feature_source_length || !registers ||
	    !register_source_length || !bindings ||
	    (bindings->count && !bindings->items))
		return ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_ARGUMENT;
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
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_EXPECTED_AMBIGUOUS_FIELD) {
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
	    fprintf(output, "ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_COUNTS(%zuU, %zuU, %zuU, %zuU)\n",
		    bindings.count, groups, mapped, ambiguous_field) < 0 ||
	    fprintf(output, "ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_IDENTITY(UINT64_C(0x%016" PRIx64 "))\n",
		    identity) < 0)
		goto io;
	for (index = 0; index < bindings.count; index++) {
		const struct orlix_tcti_feature_field_domain_binding *binding =
			&bindings.items[index];
		const struct orlix_tcti_feature_node *node =
			&features->nodes[binding->feature_node_index];

		if (fprintf(output, "ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_OCCURRENCE("
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
			    "%zuU, %zuU, %zuU, %zuU, %zuU, %zuU, %zuU, %zuU)\n",
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
			    binding->value_relation_source_length) < 0)
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
