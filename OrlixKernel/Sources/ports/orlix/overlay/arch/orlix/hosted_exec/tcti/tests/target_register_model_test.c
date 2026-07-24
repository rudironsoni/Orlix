// SPDX-License-Identifier: GPL-2.0-only
#include "target_register_model.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expression) do { \
	if (!(expression)) { \
		fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
		return -1; \
	} \
} while (0)

static int expect(const char *source, enum tcti_register_model_error_code code)
{
	struct tcti_register_model model = { 0 };
	struct tcti_register_model_error error = { 0 };

	CHECK(tcti_register_model_import(source, strlen(source), &model, &error) == -1);
	if (error.code != code)
		fprintf(stderr, "expected error %d, got %d at %zu: %s\n", code,
			error.code, error.offset, error.message);
	CHECK(error.code == code);
	tcti_register_model_destroy(&model);
	return 0;
}

static int malformed_json_rejected(void)
{
	return expect("[", TCTI_REGISTER_MODEL_INVALID_JSON);
}

static int synthetic_source_is_not_a_pin(void)
{
	return expect("[]", TCTI_REGISTER_MODEL_COUNT_MISMATCH);
}

static int range_overflow_rejected(void)
{
	return expect("[{\"_type\":\"Register\",\"name\":\"x\",\"fieldsets\":[{\"_type\":\"Fieldset\",\"width\":32,\"values\":[{\"_type\":\"Fields.Field\",\"name\":\"x\",\"rangeset\":[{\"_type\":\"Range\",\"start\":31,\"width\":2}]}]}]}]", TCTI_REGISTER_MODEL_INVALID_SOURCE);
}

static int unsupported_domain_rejected(void)
{
	return expect("[{\"_meta\":{\"version\":{\"architecture\":\"vFATAp1-A\",\"build\":\"818\",\"ref\":\"2026-06_rel\",\"schema\":\"2.9.5\",\"timestamp\":\"2026-06-24 17:12:14\"}},\"_type\":\"Register\",\"name\":\"x\",\"fieldsets\":[{\"_type\":\"Fieldset\",\"width\":32,\"values\":[{\"_type\":\"Fields.Field\",\"name\":\"x\",\"rangeset\":[],\"values\":{\"_type\":\"Valuesets.Values\",\"values\":[{\"_type\":\"Values.Bad\"}]}}]}]}]", TCTI_REGISTER_MODEL_UNSUPPORTED_TYPE);
}

static int missing_metadata_rejected(void)
{
	return expect("[{\"_type\":\"Register\",\"name\":\"x\"}]",
		TCTI_REGISTER_MODEL_INVALID_SOURCE);
}

static int missing_fieldset_width_rejected(void)
{
	return expect("[{\"_meta\":{\"version\":{\"architecture\":\"vFATAp1-A\",\"build\":\"818\",\"ref\":\"2026-06_rel\",\"schema\":\"2.9.5\",\"timestamp\":\"2026-06-24 17:12:14\"}},\"_type\":\"Register\",\"name\":\"x\",\"fieldsets\":[{\"_type\":\"Fieldset\",\"values\":[]}]}]",
		TCTI_REGISTER_MODEL_INVALID_SOURCE);
}

static int missing_conditional_wrapper_condition_rejected(void)
{
	return expect("[{\"_meta\":{\"version\":{\"architecture\":\"vFATAp1-A\",\"build\":\"818\",\"ref\":\"2026-06_rel\",\"schema\":\"2.9.5\",\"timestamp\":\"2026-06-24 17:12:14\"}},\"_type\":\"Register\",\"name\":\"x\",\"fieldsets\":[{\"_type\":\"Fieldset\",\"width\":32,\"values\":[{\"_type\":\"Fields.ConditionalField\",\"rangeset\":[],\"fields\":[{\"field\":{\"_type\":\"Fields.Field\",\"rangeset\":[]}}]}]}]}]",
		TCTI_REGISTER_MODEL_INVALID_SOURCE);
}

static int missing_encoding_or_selector_value_rejected(void)
{
	static const char missing_asmvalue[] =
		"[{\"_meta\":{\"version\":{\"architecture\":\"vFATAp1-A\",\"build\":\"818\",\"ref\":\"2026-06_rel\",\"schema\":\"2.9.5\",\"timestamp\":\"2026-06-24 17:12:14\"}},\"_type\":\"Register\",\"name\":\"x\",\"accessors\":[{\"_type\":\"Accessors.SystemAccessor\",\"name\":\"A64.MRS\",\"condition\":{\"_type\":\"AST.Bool\",\"value\":true},\"access\":null,\"encoding\":[{\"_type\":\"Encoding\",\"encodings\":{\"op0\":{\"_type\":\"Values.Value\",\"value\":\"'11'\"}}}]}]}]";
	static const char missing_selector_value[] =
		"[{\"_meta\":{\"version\":{\"architecture\":\"vFATAp1-A\",\"build\":\"818\",\"ref\":\"2026-06_rel\",\"schema\":\"2.9.5\",\"timestamp\":\"2026-06-24 17:12:14\"}},\"_type\":\"Register\",\"name\":\"x\",\"accessors\":[{\"_type\":\"Accessors.SystemAccessor\",\"name\":\"A64.MRS\",\"condition\":{\"_type\":\"AST.Bool\",\"value\":true},\"access\":null,\"encoding\":[{\"_type\":\"Encoding\",\"asmvalue\":\"x\",\"encodings\":{\"op0\":{\"_type\":\"Values.Value\"}}}]}]}]";

	CHECK(!expect(missing_asmvalue, TCTI_REGISTER_MODEL_INVALID_SOURCE));
	CHECK(!expect(missing_selector_value, TCTI_REGISTER_MODEL_INVALID_SOURCE));
	return 0;
}

static int input_and_depth_limits(void)
{
	struct tcti_register_model model = { 0 };
	struct tcti_register_model_error error = { 0 };
	char deep[515];
	size_t index;

	CHECK(tcti_register_model_import("[]", 128U * 1024U * 1024U + 1U,
					 &model, &error) == -1);
	CHECK(error.code == TCTI_REGISTER_MODEL_INPUT_LIMIT);
	for (index = 0; index < 257; index++)
		deep[index] = '[';
	for (index = 0; index < 257; index++)
		deep[257 + index] = ']';
	deep[514] = 0;
	memset(&error, 0, sizeof(error));
	CHECK(tcti_register_model_import(deep, 514, &model, &error) == -1);
	CHECK(error.code == TCTI_REGISTER_MODEL_DEPTH_LIMIT);
	tcti_register_model_destroy(&model);
	return 0;
}

static char *read_file(const char *path, size_t *length)
{
	FILE *file;
	char *source;
	long file_length;

	file = fopen(path, "rb");
	if (!file || fseek(file, 0, SEEK_END) || (file_length = ftell(file)) < 0 ||
		fseek(file, 0, SEEK_SET)) {
		if (file)
			fclose(file);
		return NULL;
	}
	source = malloc((size_t)file_length);
	if (!source || fread(source, 1, (size_t)file_length, file) != (size_t)file_length) {
		free(source);
		fclose(file);
		return NULL;
	}
	fclose(file);
	*length = (size_t)file_length;
	return source;
}

static char *find_bytes(char *haystack, size_t haystack_length,
			const char *needle, size_t needle_length)
{
	size_t offset;

	if (!needle_length || needle_length > haystack_length)
		return NULL;
	for (offset = 0; offset <= haystack_length - needle_length; offset++)
		if (!memcmp(haystack + offset, needle, needle_length))
			return haystack + offset;
	return NULL;
}

static int replace_once(char *source, size_t source_length, const char *before,
			const char *after)
{
	char *position;
	size_t length = strlen(before);

	if (length != strlen(after))
		return -1;
	position = find_bytes(source, source_length, before, length);
	if (!position)
		return -1;
	memcpy(position, after, length);
	return 0;
}

static int expect_pinned_mutation(const char *source, size_t length,
			 const char *before, const char *after,
			 enum tcti_register_model_error_code expected)
{
	struct tcti_register_model model = { 0 };
	struct tcti_register_model_error error = { 0 };
	char *copy = malloc(length);

	CHECK(copy != NULL);
	memcpy(copy, source, length);
	CHECK(!replace_once(copy, length, before, after));
	CHECK(tcti_register_model_import(copy, length, &model, &error) == -1);
	CHECK(error.code == expected);
	tcti_register_model_destroy(&model);
	free(copy);
	return 0;
}

static int expect_rollback(const char *source,
			 enum tcti_register_model_error_code expected)
{
	struct tcti_register_model model = { 0 };
	struct tcti_register_model_error error = { 0 };

	CHECK(tcti_register_model_import(source, strlen(source), &model, &error) == -1);
	CHECK(error.code == expected);
	CHECK(model.allocation_bytes == 0U);
	CHECK(model.allocation_high_water_bytes == 0U);
	CHECK(model.source == NULL);
	CHECK(model.register_count == 0U);
	tcti_register_model_destroy(&model);
	memset(&error, 0, sizeof(error));
	CHECK(tcti_register_model_import(source, strlen(source), &model, &error) == -1);
	CHECK(error.code == expected);
	CHECK(model.allocation_bytes == 0U);
	CHECK(model.allocation_high_water_bytes == 0U);
	CHECK(model.source == NULL);
	CHECK(model.register_count == 0U);
	tcti_register_model_destroy(&model);
	return 0;
}

static int object_span(const struct tcti_register_model *model, size_t offset,
			   size_t length)
{
	CHECK(offset <= model->source_length);
	CHECK(length <= model->source_length - offset);
	CHECK(length > 1U);
	CHECK(model->source[offset] == '{');
	CHECK(model->source[offset + length - 1U] == '}');
	return 0;
}

static int scalar_span(const struct tcti_register_model *model, size_t offset,
			   size_t length)
{
	CHECK(offset <= model->source_length);
	CHECK(length > 0U);
	CHECK(length <= model->source_length - offset);
	return 0;
}

struct expression_span {
	size_t offset;
	size_t length;
	const char *type;
};

static int compare_expression_spans(const void *left, const void *right)
{
	const struct expression_span *a = left;
	const struct expression_span *b = right;

	if (a->offset != b->offset)
		return a->offset < b->offset ? -1 : 1;
	if (a->length != b->length)
		return a->length < b->length ? -1 : 1;
	return strcmp(a->type, b->type);
}

static int find_register(const struct tcti_register_model *model, const char *name,
			 uint32_t *register_index)
{
	size_t index;

	for (index = 0; index < model->register_count; index++) {
		if (!strcmp(model->registers[index].name, name) &&
		    !strcmp(model->registers[index].type, "Register") &&
		    !strcmp(model->registers[index].state, "AArch64")) {
			*register_index = (uint32_t)index;
			return 0;
		}
	}
	return -1;
}

static size_t count_register_type(const struct tcti_register_model *model,
				  const char *type)
{
	size_t count = 0;
	size_t index;

	for (index = 0; index < model->register_count; index++)
		if (!strcmp(model->registers[index].type, type))
			count++;
	return count;
}

static size_t count_field_type(const struct tcti_register_model *model,
			       const char *type)
{
	size_t count = 0;
	size_t index;

	for (index = 0; index < model->field_count; index++)
		if (!strcmp(model->fields[index].type, type))
			count++;
	return count;
}

static size_t count_domain_type(const struct tcti_register_model *model,
				const char *type)
{
	size_t count = 0;
	size_t index;

	for (index = 0; index < model->domain_count; index++)
		if (!strcmp(model->domains[index].type, type))
			count++;
	return count;
}

static size_t count_accessor_type(const struct tcti_register_model *model,
				  const char *type)
{
	size_t count = 0;
	size_t index;

	for (index = 0; index < model->accessor_count; index++)
		if (!strcmp(model->accessors[index].type, type))
			count++;
	return count;
}

static int pinned_type_histograms(const struct tcti_register_model *model)
{
	size_t index;
	size_t fieldset_32 = 0;
	size_t fieldset_64 = 0;
	size_t fieldset_128 = 0;

	CHECK(count_register_type(model, "Register") == 1802U);
	CHECK(count_register_type(model, "RegisterArray") == 195U);
	CHECK(count_register_type(model, "RegisterBlock") == 8U);
	CHECK(count_field_type(model, "Fields.Array") == 300U);
	CHECK(count_field_type(model, "Fields.ConditionalField") == 3028U);
	CHECK(count_field_type(model, "Fields.ConstantField") == 1625U);
	CHECK(count_field_type(model, "Fields.Dynamic") == 36U);
	CHECK(count_field_type(model, "Fields.Field") == 7566U);
	CHECK(count_field_type(model, "Fields.ImplementationDefined") == 166U);
	CHECK(count_field_type(model, "Fields.Reserved") == 3004U);
	CHECK(count_field_type(model, "Fields.Vector") == 18U);
	CHECK(count_domain_type(model, "Values.Value") == 15675U);
	CHECK(count_domain_type(model, "Values.ValueRange") == 90U);
	CHECK(count_domain_type(model, "Values.Link") == 193U);
	CHECK(count_domain_type(model, "Values.ConditionalValue") == 621U);
	CHECK(count_accessor_type(model, "Accessors.SystemAccessor") == 2363U);
	CHECK(count_accessor_type(model, "Accessors.SystemAccessorArray") == 195U);
	CHECK(count_accessor_type(model, "Accessors.MemoryMapped") == 598U);
	CHECK(count_accessor_type(model, "Accessors.BlockAccess") == 213U);
	CHECK(count_accessor_type(model, "Accessors.ExternalDebug") == 201U);
	CHECK(count_accessor_type(model, "Accessors.BlockAccessArray") == 23U);
	CHECK(count_accessor_type(model,
		"Accessors.ImplementationDefinedOffsetAccessorArray") == 3U);
	for (index = 0; index < model->fieldset_count; index++) {
		fieldset_32 += model->fieldsets[index].width == 32U;
		fieldset_64 += model->fieldsets[index].width == 64U;
		fieldset_128 += model->fieldsets[index].width == 128U;
	}
	CHECK(fieldset_32 == 932U);
	CHECK(fieldset_64 == 1164U);
	CHECK(fieldset_128 == 73U);
	return 0;
}

static int selector_payloads(const struct tcti_register_model *model)
{
	size_t index;
	size_t literals = 0;
	size_t equations = 0;
	size_t groups = 0;

	for (index = 0; index < model->system_selector_count; index++) {
		const struct tcti_register_system_selector *selector =
			&model->system_selectors[index];

		if (!strcmp(selector->type, "Values.Value")) {
			const struct tcti_register_selector_literal *literal;

			CHECK(selector->value_kind == TCTI_REGISTER_SELECTOR_VALUE_LITERAL);
			CHECK(selector->payload_index < model->selector_literal_count);
			CHECK(selector->value_expression == UINT32_MAX);
			literal = &model->selector_literals[selector->payload_index];
			CHECK(literal->selector_index == index);
			CHECK(!scalar_span(model, literal->source_offset, literal->source_length));
			literals++;
		} else if (!strcmp(selector->type, "Values.EquationValue")) {
			const struct tcti_register_selector_equation *equation;
			size_t slice_index;

			CHECK(selector->value_kind == TCTI_REGISTER_SELECTOR_VALUE_EQUATION);
			CHECK(selector->payload_index < model->selector_equation_count);
			equation = &model->selector_equations[selector->payload_index];
			CHECK(equation->selector_index == index);
			CHECK(equation->identifier != NULL);
			CHECK(equation->slice_count > 0U);
			CHECK(equation->first_slice < model->selector_slice_count);
			CHECK(equation->slice_count <= model->selector_slice_count -
			      equation->first_slice);
			for (slice_index = 0; slice_index < equation->slice_count; slice_index++) {
				const struct tcti_register_selector_slice *slice =
					&model->selector_slices[equation->first_slice + slice_index];

				CHECK(slice->equation_index == selector->payload_index);
				CHECK(slice->width > 0U);
				CHECK(!object_span(model, slice->source_offset, slice->source_length));
			}
			equations++;
		} else if (!strcmp(selector->type, "Values.Group")) {
			const struct tcti_register_selector_group *group;
			size_t fragment_index;

			CHECK(selector->value_kind == TCTI_REGISTER_SELECTOR_VALUE_GROUP);
			CHECK(selector->payload_index < model->selector_group_count);
			group = &model->selector_groups[selector->payload_index];
			CHECK(group->selector_index == index);
			CHECK(group->fragment_count > 0U);
			CHECK(group->first_fragment < model->selector_group_fragment_count);
			CHECK(group->fragment_count <= model->selector_group_fragment_count -
			      group->first_fragment);
			for (fragment_index = 0; fragment_index < group->fragment_count;
			     fragment_index++) {
				const struct tcti_register_selector_group_fragment *fragment =
					&model->selector_group_fragments[group->first_fragment +
						fragment_index];

				CHECK(fragment->kind == TCTI_REGISTER_SELECTOR_VALUE_LITERAL ||
				      fragment->kind == TCTI_REGISTER_SELECTOR_VALUE_EQUATION ||
				      fragment->kind == TCTI_REGISTER_SELECTOR_VALUE_GROUP);
				if (fragment->kind == TCTI_REGISTER_SELECTOR_VALUE_LITERAL)
					CHECK(fragment->payload_index < model->selector_literal_count);
				else if (fragment->kind == TCTI_REGISTER_SELECTOR_VALUE_EQUATION)
					CHECK(fragment->payload_index < model->selector_equation_count);
				else
					CHECK(fragment->payload_index < model->selector_group_count);
				CHECK(fragment->source_offset <= model->source_length);
				CHECK(fragment->source_length <= model->source_length -
				      fragment->source_offset);
			}
			groups++;
		} else {
			CHECK(selector->value_kind == TCTI_REGISTER_SELECTOR_VALUE_NONE);
			CHECK(selector->payload_index == UINT32_MAX);
			CHECK(selector->value_expression == UINT32_MAX);
		}
	}
	CHECK(literals > 0U);
	CHECK(equations > 0U);
	CHECK(groups > 0U);
	return 0;
}

static int unique_expression_spans(const struct tcti_register_model *model)
{
	struct expression_span *spans;
	size_t index;

	CHECK(model->expression_count <= SIZE_MAX / sizeof(*spans));
	spans = calloc(model->expression_count, sizeof(*spans));
	CHECK(spans != NULL);
	for (index = 0; index < model->expression_count; index++) {
		spans[index].offset = model->expressions[index].source_offset;
		spans[index].length = model->expressions[index].source_length;
		spans[index].type = model->expressions[index].type;
	}
	qsort(spans, model->expression_count, sizeof(*spans), compare_expression_spans);
	for (index = 1; index < model->expression_count; index++)
		CHECK(compare_expression_spans(&spans[index - 1U], &spans[index]) != 0);
	free(spans);
	return 0;
}

static int selector_value(const struct tcti_register_model *model,
			  uint32_t encoding_index, const char *name, const char *value)
{
	size_t index;

	for (index = 0; index < model->system_selector_count; index++) {
		const struct tcti_register_system_selector *selector =
			&model->system_selectors[index];

		if (selector->encoding_index == encoding_index &&
		    !strcmp(selector->name, name) && !strcmp(selector->value, value))
			return 0;
	}
	return -1;
}

static int rndr_tuple(const struct tcti_register_model *model, const char *name,
		      const char *op2)
{
	uint32_t register_index;
	size_t index;
	unsigned int encodings = 0;
	unsigned int fields = 0;

	CHECK(!find_register(model, name, &register_index));
	CHECK(model->registers[register_index].fieldset_count == 1U);
	CHECK(model->registers[register_index].first_fieldset < model->fieldset_count);
	CHECK(model->fieldsets[model->registers[register_index].first_fieldset].width == 64U);
	for (index = 0; index < model->field_count; index++) {
		const struct tcti_field_identity *field = &model->fields[index];

		if (field->register_index != register_index)
			continue;
		CHECK(!strcmp(field->name, name));
		CHECK(field->range_count == 1U);
		CHECK(field->first_range < model->range_count);
		CHECK(model->ranges[field->first_range].start == 0U);
		CHECK(model->ranges[field->first_range].width == 64U);
		fields++;
	}
	CHECK(fields == 1U);
	for (index = 0; index < model->accessor_count; index++) {
		const struct tcti_register_accessor *accessor = &model->accessors[index];
		uint32_t encoding_index;

		if (accessor->register_index != register_index ||
		    strcmp(accessor->type, "Accessors.SystemAccessor") ||
		    strcmp(accessor->name, "A64.MRS"))
			continue;
		CHECK(accessor->system_encoding_count == 1U);
		CHECK(accessor->first_system_encoding < model->system_encoding_count);
		encoding_index = accessor->first_system_encoding;
		CHECK(!strcmp(model->system_encodings[encoding_index].asmvalue, name));
		CHECK(!selector_value(model, encoding_index, "op0", "'11'"));
		CHECK(!selector_value(model, encoding_index, "op1", "'011'"));
		CHECK(!selector_value(model, encoding_index, "CRn", "'0010'"));
		CHECK(!selector_value(model, encoding_index, "CRm", "'0100'"));
		CHECK(!selector_value(model, encoding_index, "op2", op2));
		encodings++;
	}
	CHECK(encodings == 1U);
	return 0;
}

static int source_spans_and_relationships(const struct tcti_register_model *model)
{
	size_t index;

	for (index = 0; index < model->register_count; index++) {
		const struct tcti_register_identity *register_identity = &model->registers[index];
		const struct tcti_register_identity *metadata_owner;

		CHECK(!object_span(model, register_identity->source_offset,
			register_identity->source_length));
		CHECK(register_identity->parent_register == UINT32_MAX ||
		      register_identity->parent_register < index);
		CHECK(register_identity->metadata_owner < model->register_count);
		metadata_owner = &model->registers[register_identity->metadata_owner];
		CHECK(metadata_owner->metadata_length > 1U);
		CHECK(!object_span(model, metadata_owner->metadata_offset,
			metadata_owner->metadata_length));
		if (register_identity->metadata_length) {
			CHECK(register_identity->metadata_owner == index);
			CHECK(!object_span(model, register_identity->metadata_offset,
				register_identity->metadata_length));
		}
		if (register_identity->parent_register != UINT32_MAX)
			CHECK(register_identity->metadata_owner ==
			      model->registers[register_identity->parent_register].metadata_owner);
	}
	for (index = 0; index < model->field_count; index++) {
		const struct tcti_field_identity *field = &model->fields[index];

		CHECK(field->register_index < model->register_count);
		CHECK(field->fieldset_index < model->fieldset_count);
		CHECK(field->parent_field == UINT32_MAX || field->parent_field < index);
		CHECK(field->first_range <= model->range_count);
		CHECK(field->range_count <= model->range_count - field->first_range);
		CHECK(!object_span(model, field->source_offset, field->source_length));
		if (!field->condition_length)
			CHECK(field->condition_expression == UINT32_MAX);
		else {
			CHECK(field->condition_expression < model->expression_count);
			CHECK(model->expressions[field->condition_expression].source_offset ==
			      field->condition_offset);
			CHECK(model->expressions[field->condition_expression].source_length ==
			      field->condition_length);
		}
		if (!field->wrapper_condition_length)
			CHECK(field->wrapper_condition_expression == UINT32_MAX);
		else {
			CHECK(field->wrapper_condition_expression < model->expression_count);
			CHECK(model->expressions[field->wrapper_condition_expression].source_offset ==
			      field->wrapper_condition_offset);
			CHECK(model->expressions[field->wrapper_condition_expression].source_length ==
			      field->wrapper_condition_length);
		}
	}
	for (index = 0; index < model->fieldset_count; index++) {
		const struct tcti_register_fieldset *fieldset = &model->fieldsets[index];

		CHECK(fieldset->register_index < model->register_count);
		CHECK(!object_span(model, fieldset->source_offset, fieldset->source_length));
		if (!fieldset->condition_length)
			CHECK(fieldset->condition_expression == UINT32_MAX);
		else {
			CHECK(fieldset->condition_expression < model->expression_count);
			CHECK(model->expressions[fieldset->condition_expression].source_offset ==
			      fieldset->condition_offset);
			CHECK(model->expressions[fieldset->condition_expression].source_length ==
			      fieldset->condition_length);
		}
	}
	for (index = 0; index < model->field_branch_count; index++) {
		const struct tcti_conditional_field_branch *branch =
			&model->field_branches[index];

		CHECK(branch->conditional_field_index < model->field_count);
		CHECK(branch->field_index < model->field_count);
		CHECK(!object_span(model, branch->source_offset, branch->source_length));
		CHECK(branch->condition_expression < model->expression_count);
		CHECK(model->expressions[branch->condition_expression].source_offset ==
		      branch->condition_offset);
		CHECK(model->expressions[branch->condition_expression].source_length ==
		      branch->condition_length);
	}
	for (index = 0; index < model->range_count; index++) {
		const struct tcti_register_range *range = &model->ranges[index];

		CHECK(range->field_index < model->field_count);
		CHECK(!object_span(model, range->source_offset, range->source_length));
	}
	for (index = 0; index < model->valueset_count; index++) {
		const struct tcti_register_valueset *valueset = &model->valuesets[index];

		CHECK(valueset->field_index == UINT32_MAX ||
		      valueset->field_index < model->field_count);
		CHECK(!object_span(model, valueset->source_offset, valueset->source_length));
	}
	for (index = 0; index < model->domain_count; index++) {
		const struct tcti_value_domain *domain = &model->domains[index];

		CHECK(domain->field_index == UINT32_MAX || domain->field_index < model->field_count);
		CHECK(domain->valueset_index < model->valueset_count);
		CHECK(domain->parent_domain == UINT32_MAX || domain->parent_domain < index);
		CHECK(!object_span(model, domain->source_offset, domain->source_length));
		if (!domain->condition_length)
			CHECK(domain->condition_expression == UINT32_MAX);
		else {
			CHECK(domain->condition_expression < model->expression_count);
			CHECK(model->expressions[domain->condition_expression].source_offset ==
			      domain->condition_offset);
			CHECK(model->expressions[domain->condition_expression].source_length ==
			      domain->condition_length);
		}
		if (!domain->child_domain_count)
			CHECK(domain->first_child_domain == UINT32_MAX);
		else {
			CHECK(domain->first_child_domain < model->domain_count);
			CHECK(domain->child_domain_count <= model->domain_count -
			      domain->first_child_domain);
			for (size_t child = 0; child < domain->child_domain_count; child++)
				CHECK(model->domains[domain->first_child_domain + child].parent_domain ==
				      index);
		}
	}
	for (index = 0; index < model->accessor_count; index++) {
		const struct tcti_register_accessor *accessor = &model->accessors[index];

		CHECK(accessor->register_index < model->register_count);
		CHECK(!object_span(model, accessor->source_offset, accessor->source_length));
		CHECK(accessor->first_system_encoding == UINT32_MAX ||
		      accessor->first_system_encoding <= model->system_encoding_count);
		CHECK(accessor->system_encoding_count <= model->system_encoding_count -
		      (accessor->first_system_encoding == UINT32_MAX ?
		       model->system_encoding_count : accessor->first_system_encoding));
		CHECK(accessor->condition_expression == UINT32_MAX ||
		      accessor->condition_expression < model->expression_count);
		CHECK(accessor->access_expression == UINT32_MAX ||
		      accessor->access_expression < model->expression_count);
		if (!accessor->condition_length)
			CHECK(accessor->condition_expression == UINT32_MAX);
		else {
			CHECK(accessor->condition_expression < model->expression_count);
			CHECK(model->expressions[accessor->condition_expression].source_offset ==
			      accessor->condition_offset);
			CHECK(model->expressions[accessor->condition_expression].source_length ==
			      accessor->condition_length);
		}
		if (!accessor->access_length)
			CHECK(accessor->access_expression == UINT32_MAX);
		else if (accessor->access_expression != UINT32_MAX) {
			CHECK(model->expressions[accessor->access_expression].source_offset ==
			      accessor->access_offset);
			CHECK(model->expressions[accessor->access_expression].source_length ==
			      accessor->access_length);
		}
	}
	for (index = 0; index < model->system_encoding_count; index++) {
		const struct tcti_register_system_encoding *encoding =
			&model->system_encodings[index];

		CHECK(encoding->accessor_index < model->accessor_count);
		CHECK(!strcmp(encoding->type, "Encoding"));
		CHECK(!object_span(model, encoding->source_offset, encoding->source_length));
	}
	for (index = 0; index < model->system_selector_count; index++) {
		const struct tcti_register_system_selector *selector =
			&model->system_selectors[index];

		CHECK(selector->encoding_index < model->system_encoding_count);
		CHECK(!strcmp(selector->type, "Values.Value") ||
		      !strcmp(selector->type, "Values.EquationValue") ||
		      !strcmp(selector->type, "Values.Group"));
		CHECK(selector->name != NULL);
		CHECK(selector->value != NULL);
		CHECK(selector->value_expression == UINT32_MAX ||
		      selector->value_expression < model->expression_count);
		CHECK(!object_span(model, selector->source_offset, selector->source_length));
	}
	for (index = 0; index < model->constraint_count; index++) {
		const struct tcti_register_constraint *constraint = &model->constraints[index];
		size_t item;

		CHECK(constraint->kind == TCTI_REGISTER_CONSTRAINT_NULL ||
		      constraint->kind == TCTI_REGISTER_CONSTRAINT_VALUESET ||
		      constraint->kind == TCTI_REGISTER_CONSTRAINT_ARRAY);
		CHECK(constraint->field_index == UINT32_MAX ||
		      constraint->field_index < model->field_count);
		CHECK(constraint->source_offset <= model->source_length);
		CHECK(constraint->source_length <= model->source_length - constraint->source_offset);
		if (constraint->kind == TCTI_REGISTER_CONSTRAINT_NULL) {
			CHECK(constraint->source_length == 4U);
			CHECK(!memcmp(model->source + constraint->source_offset, "null", 4U));
		} else if (constraint->kind == TCTI_REGISTER_CONSTRAINT_ARRAY) {
			CHECK(constraint->source_length >= 2U);
			CHECK(model->source[constraint->source_offset] == '[');
			CHECK(model->source[constraint->source_offset +
				constraint->source_length - 1U] == ']');
		} else {
			CHECK(!object_span(model, constraint->source_offset,
				constraint->source_length));
		}
		if (!constraint->item_count)
			CHECK(constraint->first_item == UINT32_MAX);
		else {
			CHECK(constraint->first_item < model->constraint_item_count);
			CHECK(constraint->item_count <= model->constraint_item_count -
			      constraint->first_item);
		}
		if (constraint->kind == TCTI_REGISTER_CONSTRAINT_NULL) {
			CHECK(constraint->valueset_index == UINT32_MAX);
			CHECK(constraint->item_count == 0U);
		}
		if (constraint->kind == TCTI_REGISTER_CONSTRAINT_VALUESET)
			CHECK(constraint->valueset_index < model->valueset_count);
		if (constraint->kind == TCTI_REGISTER_CONSTRAINT_ARRAY)
			CHECK(constraint->item_count > 0U);
		for (item = 0; item < constraint->item_count; item++) {
			const struct tcti_register_constraint_item *entry =
				&model->constraint_items[constraint->first_item + item];

			CHECK(entry->constraint_index == index);
			CHECK(entry->valueset_index == UINT32_MAX ||
			      entry->valueset_index < model->valueset_count);
			CHECK(entry->domain_index == UINT32_MAX ||
			      entry->domain_index < model->domain_count);
			CHECK(entry->source_offset <= model->source_length);
			CHECK(entry->source_length <= model->source_length - entry->source_offset);
		}
	}
	for (index = 0; index < model->link_count; index++) {
		const struct tcti_register_link *link = &model->links[index];

		CHECK(link->domain_index < model->domain_count);
		CHECK(link->key != NULL);
		CHECK(link->value != NULL);
		CHECK(link->source_offset <= model->source_length);
		CHECK(link->source_length <= model->source_length - link->source_offset);
	}
	for (index = 0; index < model->expression_count; index++) {
		const struct tcti_register_expression *expression = &model->expressions[index];
		size_t child;

		CHECK(expression->parent_expression == UINT32_MAX ||
		      expression->parent_expression < index);
		CHECK(expression->source_offset <= model->source_length);
		CHECK(expression->source_length <= model->source_length - expression->source_offset);
		CHECK(!object_span(model, expression->source_offset, expression->source_length));
		CHECK(expression->role == NULL || expression->parent_expression != UINT32_MAX);
		if (!expression->child_count)
			CHECK(expression->first_child == UINT32_MAX);
		else {
			CHECK(expression->first_child < model->expression_child_count);
			CHECK(expression->child_count <= model->expression_child_count -
			      expression->first_child);
		}
		for (child = 0; child < expression->child_count; child++) {
			uint32_t child_index =
				model->expression_children[expression->first_child + child];
			const struct tcti_register_expression *nested;

			CHECK(child_index < model->expression_count);
			CHECK(child_index > index);
			nested = &model->expressions[child_index];
			CHECK(nested->parent_expression == index);
			CHECK(nested->source_offset >= expression->source_offset);
			CHECK(nested->source_length <= expression->source_length -
			      (nested->source_offset - expression->source_offset));
		}
	}
	return 0;
}

static int pinned_model(const char *path)
{
	struct tcti_register_model model = { 0 };
	struct tcti_register_model_error error = { 0 };
	char *source;
	size_t source_length;
	int result = -1;

	source = read_file(path, &source_length);
	CHECK(source != NULL);
	if (tcti_register_model_import(source, source_length, &model, &error)) {
		fprintf(stderr, "pinned error %d at %zu: %s\n", error.code,
			error.offset, error.message);
		goto out;
	}
	CHECK(model.source != NULL);
	CHECK(model.source_length == source_length);
	CHECK(model.allocation_limit_bytes == 512U * 1024U * 1024U);
	CHECK(model.allocation_high_water_bytes > 0U);
	CHECK(model.allocation_high_water_bytes <= model.allocation_limit_bytes);
	CHECK(model.allocation_bytes <= model.allocation_high_water_bytes);
	CHECK(model.register_count == TCTI_REGISTER_RECORD_COUNT);
	CHECK(model.fieldset_count == TCTI_REGISTER_FIELDSET_COUNT);
	CHECK(model.field_count == TCTI_REGISTER_FIELD_COUNT);
	CHECK(model.range_count == TCTI_REGISTER_RANGE_COUNT);
	CHECK(model.top_level_valueset_count == TCTI_REGISTER_VALUESET_COUNT);
	CHECK(model.top_level_domain_count == TCTI_REGISTER_DOMAIN_COUNT);
	CHECK(model.accessor_count == TCTI_REGISTER_ACCESSOR_COUNT);
	CHECK(model.field_branch_count == 3314U);
	CHECK(model.constraint_count == 1656U);
	CHECK(model.constraint_domain_count == 2958U);
	CHECK(model.system_encoding_count == 2558U);
	CHECK(model.system_selector_count > 0U);
	CHECK(model.expression_count > 0U);
	CHECK(!pinned_type_histograms(&model));
	CHECK(!rndr_tuple(&model, "RNDR", "'000'"));
	CHECK(!rndr_tuple(&model, "RNDRRS", "'001'"));
	CHECK(!selector_payloads(&model));
	CHECK(!source_spans_and_relationships(&model));
	CHECK(!unique_expression_spans(&model));

	/* A populated model is never overwritten, leaked, or silently reset. */
	memset(&error, 0, sizeof(error));
	CHECK(tcti_register_model_import(source, source_length, &model, &error) == -1);
	CHECK(error.code == TCTI_REGISTER_MODEL_INVALID_ARGUMENT);
	CHECK(model.source != NULL);
	CHECK(model.register_count == TCTI_REGISTER_RECORD_COUNT);

	/* These edits keep JSON valid, and must fail before a changed source can be used. */
	CHECK(!expect_pinned_mutation(source, source_length,
		"\"architecture\": \"vFATAp1-A\"", "\"architecture\": \"vFATAp1-B\"",
		TCTI_REGISTER_MODEL_INVALID_SOURCE));
	CHECK(!expect_pinned_mutation(source, source_length, "\"rangeset\"", "\"rangesex\"",
		TCTI_REGISTER_MODEL_INVALID_SOURCE));
	CHECK(!expect_pinned_mutation(source, source_length, "\"op\": \"<=\"", "\"op\": \"??\"",
		TCTI_REGISTER_MODEL_UNSUPPORTED_TYPE));
	CHECK(!expect_pinned_mutation(source, source_length, "ACTLR2", "BCTLR2",
		TCTI_REGISTER_MODEL_HASH_MISMATCH));
	result = 0;
out:
	tcti_register_model_destroy(&model);
	free(source);
	return result;
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s /path/to/pinned/Registers.json\n", argv[0]);
		return 2;
	}
	CHECK(!malformed_json_rejected());
	CHECK(!synthetic_source_is_not_a_pin());
	CHECK(!range_overflow_rejected());
	CHECK(!unsupported_domain_rejected());
	CHECK(!missing_metadata_rejected());
	CHECK(!missing_fieldset_width_rejected());
	CHECK(!missing_conditional_wrapper_condition_rejected());
	CHECK(!missing_encoding_or_selector_value_rejected());
	CHECK(!expect_rollback("[{\"_type\":\"Register\",\"name\":\"x\"}]",
		TCTI_REGISTER_MODEL_INVALID_SOURCE));
	CHECK(!input_and_depth_limits());
	CHECK(!pinned_model(argv[1]));
	puts("target register model tests: passed");
	return 0;
}
