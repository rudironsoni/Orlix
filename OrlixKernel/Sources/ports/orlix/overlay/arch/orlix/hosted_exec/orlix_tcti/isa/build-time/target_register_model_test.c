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

static int expect(const char *source, enum orlix_tcti_register_model_error_code code)
{
	struct orlix_tcti_register_model model = { 0 };
	struct orlix_tcti_register_model_error error = { 0 };

	CHECK(orlix_tcti_register_model_import(source, strlen(source), &model, &error) == -1);
	if (error.code != code)
		fprintf(stderr, "expected error %d, got %d at %zu: %s\n", code,
			error.code, error.offset, error.message);
	CHECK(error.code == code);
	orlix_tcti_register_model_destroy(&model);
	return 0;
}

static int malformed_json_rejected(void)
{
	return expect("[", ORLIX_TCTI_REGISTER_MODEL_INVALID_JSON);
}

static int synthetic_source_is_not_a_pin(void)
{
	return expect("[]", ORLIX_TCTI_REGISTER_MODEL_COUNT_MISMATCH);
}

static int range_overflow_rejected(void)
{
	return expect("[{\"_type\":\"Register\",\"name\":\"x\",\"fieldsets\":[{\"_type\":\"Fieldset\",\"width\":32,\"values\":[{\"_type\":\"Fields.Field\",\"name\":\"x\",\"rangeset\":[{\"_type\":\"Range\",\"start\":31,\"width\":2}]}]}]}]", ORLIX_TCTI_REGISTER_MODEL_INVALID_SOURCE);
}

static int unsupported_domain_rejected(void)
{
	return expect("[{\"_meta\":{\"license\":{\"copyright\":\"c\",\"info\":\"i\"},\"version\":{\"architecture\":\"vFATAp1-A\",\"build\":\"818\",\"ref\":\"2026-06_rel\",\"schema\":\"2.9.5\",\"timestamp\":\"2026-06-24 17:12:14\"}},\"_type\":\"Register\",\"name\":\"x\",\"fieldsets\":[{\"_type\":\"Fieldset\",\"width\":32,\"values\":[{\"_type\":\"Fields.Field\",\"name\":\"x\",\"rangeset\":[],\"values\":{\"_type\":\"Valuesets.Values\",\"values\":[{\"_type\":\"Values.Bad\"}]}}]}]}]", ORLIX_TCTI_REGISTER_MODEL_UNSUPPORTED_TYPE);
}

static int missing_metadata_rejected(void)
{
	return expect("[{\"_type\":\"Register\",\"name\":\"x\"}]",
		ORLIX_TCTI_REGISTER_MODEL_INVALID_SOURCE);
}

static int missing_fieldset_width_rejected(void)
{
	return expect("[{\"_meta\":{\"license\":{\"copyright\":\"c\",\"info\":\"i\"},\"version\":{\"architecture\":\"vFATAp1-A\",\"build\":\"818\",\"ref\":\"2026-06_rel\",\"schema\":\"2.9.5\",\"timestamp\":\"2026-06-24 17:12:14\"}},\"_type\":\"Register\",\"name\":\"x\",\"fieldsets\":[{\"_type\":\"Fieldset\",\"values\":[]}]}]",
		ORLIX_TCTI_REGISTER_MODEL_INVALID_SOURCE);
}

static int missing_conditional_wrapper_condition_rejected(void)
{
	return expect("[{\"_meta\":{\"license\":{\"copyright\":\"c\",\"info\":\"i\"},\"version\":{\"architecture\":\"vFATAp1-A\",\"build\":\"818\",\"ref\":\"2026-06_rel\",\"schema\":\"2.9.5\",\"timestamp\":\"2026-06-24 17:12:14\"}},\"_type\":\"Register\",\"name\":\"x\",\"fieldsets\":[{\"_type\":\"Fieldset\",\"width\":32,\"values\":[{\"_type\":\"Fields.ConditionalField\",\"rangeset\":[],\"fields\":[{\"field\":{\"_type\":\"Fields.Field\",\"rangeset\":[]}}]}]}]}]",
		ORLIX_TCTI_REGISTER_MODEL_INVALID_SOURCE);
}

static int missing_encoding_or_selector_value_rejected(void)
{
	static const char missing_asmvalue[] =
		"[{\"_meta\":{\"license\":{\"copyright\":\"c\",\"info\":\"i\"},\"version\":{\"architecture\":\"vFATAp1-A\",\"build\":\"818\",\"ref\":\"2026-06_rel\",\"schema\":\"2.9.5\",\"timestamp\":\"2026-06-24 17:12:14\"}},\"_type\":\"Register\",\"name\":\"x\",\"accessors\":[{\"_type\":\"Accessors.SystemAccessor\",\"name\":\"A64.MRS\",\"condition\":{\"_type\":\"AST.Bool\",\"value\":true},\"access\":null,\"encoding\":[{\"_type\":\"Encoding\",\"encodings\":{\"op0\":{\"_type\":\"Values.Value\",\"value\":\"'11'\"}}}]}]}]";
	static const char missing_selector_value[] =
		"[{\"_meta\":{\"license\":{\"copyright\":\"c\",\"info\":\"i\"},\"version\":{\"architecture\":\"vFATAp1-A\",\"build\":\"818\",\"ref\":\"2026-06_rel\",\"schema\":\"2.9.5\",\"timestamp\":\"2026-06-24 17:12:14\"}},\"_type\":\"Register\",\"name\":\"x\",\"accessors\":[{\"_type\":\"Accessors.SystemAccessor\",\"name\":\"A64.MRS\",\"condition\":{\"_type\":\"AST.Bool\",\"value\":true},\"access\":null,\"encoding\":[{\"_type\":\"Encoding\",\"asmvalue\":\"x\",\"encodings\":{\"op0\":{\"_type\":\"Values.Value\"}}}]}]}]";

	CHECK(!expect(missing_asmvalue, ORLIX_TCTI_REGISTER_MODEL_INVALID_SOURCE));
	CHECK(!expect(missing_selector_value, ORLIX_TCTI_REGISTER_MODEL_INVALID_SOURCE));
	return 0;
}

static int typed_memory_permission_schema_rejected(void)
{
	static const char invalid_read[] =
		"[{\"_meta\":{\"license\":{\"copyright\":\"c\",\"info\":\"i\"},\"version\":{\"architecture\":\"vFATAp1-A\",\"build\":\"818\",\"ref\":\"2026-06_rel\",\"schema\":\"2.9.5\",\"timestamp\":\"2026-06-24 17:12:14\"}},\"_type\":\"Register\",\"name\":\"x\",\"accessors\":[{\"_type\":\"Accessors.SystemAccessor\",\"condition\":{\"_type\":\"AST.Bool\",\"value\":true},\"access\":{\"_type\":\"Accessors.Permission.AccessTypes.Memory.ReadWriteAccess\",\"read\":\"BAD\",\"write\":\"W\"}}]}]";
	static const char malformed_implementation_defined[] =
		"[{\"_meta\":{\"license\":{\"copyright\":\"c\",\"info\":\"i\"},\"version\":{\"architecture\":\"vFATAp1-A\",\"build\":\"818\",\"ref\":\"2026-06_rel\",\"schema\":\"2.9.5\",\"timestamp\":\"2026-06-24 17:12:14\"}},\"_type\":\"Register\",\"name\":\"x\",\"accessors\":[{\"_type\":\"Accessors.SystemAccessor\",\"condition\":{\"_type\":\"AST.Bool\",\"value\":true},\"access\":{\"_type\":\"Accessors.Permission.AccessTypes.Memory.ImplementationDefined\",\"constraints\":[{\"_type\":\"Accessors.Permission.AccessTypes.Memory.ReadWriteAccess\",\"read\":\"R\",\"write\":\"BAD\"}]}}]}]";

	CHECK(!expect(invalid_read, ORLIX_TCTI_REGISTER_MODEL_INVALID_SOURCE));
	CHECK(!expect(malformed_implementation_defined,
		      ORLIX_TCTI_REGISTER_MODEL_INVALID_SOURCE));
	return 0;
}

static int input_and_depth_limits(void)
{
	struct orlix_tcti_register_model model = { 0 };
	struct orlix_tcti_register_model_error error = { 0 };
	char deep[515];
	size_t index;

	CHECK(orlix_tcti_register_model_import("[]", 128U * 1024U * 1024U + 1U,
					 &model, &error) == -1);
	CHECK(error.code == ORLIX_TCTI_REGISTER_MODEL_INPUT_LIMIT);
	for (index = 0; index < 257; index++)
		deep[index] = '[';
	for (index = 0; index < 257; index++)
		deep[257 + index] = ']';
	deep[514] = 0;
	memset(&error, 0, sizeof(error));
	CHECK(orlix_tcti_register_model_import(deep, 514, &model, &error) == -1);
	CHECK(error.code == ORLIX_TCTI_REGISTER_MODEL_DEPTH_LIMIT);
	orlix_tcti_register_model_destroy(&model);
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

static int expect_rollback(const char *source,
			 enum orlix_tcti_register_model_error_code expected)
{
	struct orlix_tcti_register_model model = { 0 };
	struct orlix_tcti_register_model_error error = { 0 };

	CHECK(orlix_tcti_register_model_import(source, strlen(source), &model, &error) == -1);
	CHECK(error.code == expected);
	CHECK(model.allocation_bytes == 0U);
	CHECK(model.allocation_high_water_bytes == 0U);
	CHECK(model.source == NULL);
	CHECK(model.register_count == 0U);
	orlix_tcti_register_model_destroy(&model);
	memset(&error, 0, sizeof(error));
	CHECK(orlix_tcti_register_model_import(source, strlen(source), &model, &error) == -1);
	CHECK(error.code == expected);
	CHECK(model.allocation_bytes == 0U);
	CHECK(model.allocation_high_water_bytes == 0U);
	CHECK(model.source == NULL);
	CHECK(model.register_count == 0U);
	orlix_tcti_register_model_destroy(&model);
	return 0;
}

/*
 * These deliberately stop before the pinned-source count and digest checks.
 * They exercise the typed accessor validator directly and, more importantly,
 * prove that a rejected accessor cannot leave partial ownership in the model.
 */
#define NON_SYSTEM_REGISTER_PREFIX \
	"[{\"_meta\":{\"license\":{\"copyright\":\"c\",\"info\":\"i\"},\"version\":{\"architecture\":\"vFATAp1-A\",\"build\":\"818\",\"ref\":\"2026-06_rel\",\"schema\":\"2.9.5\",\"timestamp\":\"2026-06-24 17:12:14\"}},\"_type\":\"Register\",\"name\":\"x\",\"accessors\":["
#define NON_SYSTEM_ACCESS_CONDITION \
	"\"access\":{\"_type\":\"AST.Bool\",\"value\":true},\"condition\":{\"_type\":\"AST.Bool\",\"value\":true},"
#define NON_SYSTEM_REGISTER_SUFFIX "]}]"

static int non_system_accessor_rejection_and_rollback(void)
{
	/* Every supported non-system subtype must reject a missing subtype key. */
	CHECK(!expect_rollback(NON_SYSTEM_REGISTER_PREFIX
		"{\"_type\":\"Accessors.BlockAccess\"," NON_SYSTEM_ACCESS_CONDITION
		"\"offset\":[]}" NON_SYSTEM_REGISTER_SUFFIX,
		ORLIX_TCTI_REGISTER_MODEL_INVALID_SOURCE));
	CHECK(!expect_rollback(NON_SYSTEM_REGISTER_PREFIX
		"{\"_type\":\"Accessors.BlockAccessArray\"," NON_SYSTEM_ACCESS_CONDITION
		"\"index_variable\":\"n\",\"offset\":[],\"references\":{\"_type\":\"AST.Bool\",\"value\":true}}"
		NON_SYSTEM_REGISTER_SUFFIX, ORLIX_TCTI_REGISTER_MODEL_INVALID_SOURCE));
	CHECK(!expect_rollback(NON_SYSTEM_REGISTER_PREFIX
		"{\"_type\":\"Accessors.ImplementationDefinedOffsetAccessorArray\"," NON_SYSTEM_ACCESS_CONDITION
		"\"index_variable\":\"n\",\"offset\":[]}" NON_SYSTEM_REGISTER_SUFFIX,
		ORLIX_TCTI_REGISTER_MODEL_INVALID_SOURCE));
	CHECK(!expect_rollback(NON_SYSTEM_REGISTER_PREFIX
		"{\"_type\":\"Accessors.ExternalDebug\"," NON_SYSTEM_ACCESS_CONDITION
		"\"component\":\"c\",\"instance\":\"i\",\"offset\":{\"_type\":\"AST.Bool\",\"value\":true},\"power_domain\":null}"
		NON_SYSTEM_REGISTER_SUFFIX, ORLIX_TCTI_REGISTER_MODEL_INVALID_SOURCE));
	CHECK(!expect_rollback(NON_SYSTEM_REGISTER_PREFIX
		"{\"_type\":\"Accessors.MemoryMapped\"," NON_SYSTEM_ACCESS_CONDITION
		"\"component\":\"c\",\"frame\":null,\"instance\":null,\"offset\":{\"_type\":\"AST.Bool\",\"value\":true},\"power_domain\":null}"
		NON_SYSTEM_REGISTER_SUFFIX, ORLIX_TCTI_REGISTER_MODEL_INVALID_SOURCE));

	/* A present key with the wrong typed value must also roll the model back. */
	CHECK(!expect_rollback(NON_SYSTEM_REGISTER_PREFIX
		"{\"_type\":\"Accessors.BlockAccess\"," NON_SYSTEM_ACCESS_CONDITION
		"\"offset\":[],\"references\":{\"_type\":\"AST.Bool\",\"value\":true}}"
		NON_SYSTEM_REGISTER_SUFFIX, ORLIX_TCTI_REGISTER_MODEL_INVALID_SOURCE));
	CHECK(!expect_rollback(NON_SYSTEM_REGISTER_PREFIX
		"{\"_type\":\"Accessors.BlockAccessArray\"," NON_SYSTEM_ACCESS_CONDITION
		"\"index_variable\":\"n\",\"indexes\":[{\"_type\":\"Range\",\"start\":0,\"width\":0}],\"offset\":[],\"references\":{\"_type\":\"AST.Bool\",\"value\":true}}"
		NON_SYSTEM_REGISTER_SUFFIX, ORLIX_TCTI_REGISTER_MODEL_INVALID_SOURCE));
	CHECK(!expect_rollback(NON_SYSTEM_REGISTER_PREFIX
		"{\"_type\":\"Accessors.ImplementationDefinedOffsetAccessorArray\"," NON_SYSTEM_ACCESS_CONDITION
		"\"index_variable\":\"n\",\"indexes\":[{\"_type\":\"Range\",\"start\":0,\"width\":0}],\"offset\":[]}"
		NON_SYSTEM_REGISTER_SUFFIX, ORLIX_TCTI_REGISTER_MODEL_INVALID_SOURCE));
	CHECK(!expect_rollback(NON_SYSTEM_REGISTER_PREFIX
		"{\"_type\":\"Accessors.ExternalDebug\"," NON_SYSTEM_ACCESS_CONDITION
		"\"component\":\"c\",\"instance\":\"i\",\"offset\":{\"_type\":\"AST.Bool\",\"value\":true},\"power_domain\":null,\"range\":{\"_type\":\"Range\",\"start\":0,\"width\":0}}"
		NON_SYSTEM_REGISTER_SUFFIX, ORLIX_TCTI_REGISTER_MODEL_INVALID_SOURCE));
	CHECK(!expect_rollback(NON_SYSTEM_REGISTER_PREFIX
		"{\"_type\":\"Accessors.MemoryMapped\"," NON_SYSTEM_ACCESS_CONDITION
		"\"component\":\"c\",\"frame\":null,\"instance\":null,\"offset\":{\"_type\":\"AST.Bool\",\"value\":true},\"power_domain\":null,\"range\":{\"_type\":\"Range\",\"start\":0,\"width\":0}}"
		NON_SYSTEM_REGISTER_SUFFIX, ORLIX_TCTI_REGISTER_MODEL_INVALID_SOURCE));
	return 0;
}

#undef NON_SYSTEM_REGISTER_SUFFIX
#undef NON_SYSTEM_ACCESS_CONDITION
#undef NON_SYSTEM_REGISTER_PREFIX

static int object_span(const struct orlix_tcti_register_model *model, size_t offset,
			   size_t length)
{
	CHECK(offset <= model->source_length);
	CHECK(length <= model->source_length - offset);
	CHECK(length > 1U);
	CHECK(model->source[offset] == '{');
	CHECK(model->source[offset + length - 1U] == '}');
	return 0;
}

static int scalar_span(const struct orlix_tcti_register_model *model, size_t offset,
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

static int find_register(const struct orlix_tcti_register_model *model, const char *name,
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

static size_t count_register_type(const struct orlix_tcti_register_model *model,
				  const char *type)
{
	size_t count = 0;
	size_t index;

	for (index = 0; index < model->register_count; index++)
		if (!strcmp(model->registers[index].type, type))
			count++;
	return count;
}

static size_t count_field_type(const struct orlix_tcti_register_model *model,
			       const char *type)
{
	size_t count = 0;
	size_t index;

	for (index = 0; index < model->field_count; index++)
		if (!strcmp(model->fields[index].type, type))
			count++;
	return count;
}

static size_t count_domain_type(const struct orlix_tcti_register_model *model,
				const char *type)
{
	size_t count = 0;
	size_t index;

	for (index = 0; index < model->domain_count; index++)
		if (!strcmp(model->domains[index].type, type))
			count++;
	return count;
}

static size_t count_accessor_type(const struct orlix_tcti_register_model *model,
				  const char *type)
{
	size_t count = 0;
	size_t index;

	for (index = 0; index < model->accessor_count; index++)
		if (!strcmp(model->accessors[index].type, type))
			count++;
	return count;
}

static int pinned_type_histograms(const struct orlix_tcti_register_model *model)
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

static int selector_payloads(const struct orlix_tcti_register_model *model)
{
	size_t index;
	size_t literals = 0;
	size_t equations = 0;
	size_t groups = 0;

	for (index = 0; index < model->system_selector_count; index++) {
		const struct orlix_tcti_register_system_selector *selector =
			&model->system_selectors[index];

		if (!strcmp(selector->type, "Values.Value")) {
			const struct orlix_tcti_register_selector_literal *literal;

			CHECK(selector->value_kind == ORLIX_TCTI_REGISTER_SELECTOR_VALUE_LITERAL);
			CHECK(selector->payload_index < model->selector_literal_count);
			CHECK(selector->value_expression == UINT32_MAX);
			literal = &model->selector_literals[selector->payload_index];
			CHECK(literal->selector_index == index);
			CHECK(!scalar_span(model, literal->source_offset, literal->source_length));
			literals++;
		} else if (!strcmp(selector->type, "Values.EquationValue")) {
			const struct orlix_tcti_register_selector_equation *equation;
			size_t slice_index;

			CHECK(selector->value_kind == ORLIX_TCTI_REGISTER_SELECTOR_VALUE_EQUATION);
			CHECK(selector->payload_index < model->selector_equation_count);
			equation = &model->selector_equations[selector->payload_index];
			CHECK(equation->selector_index == index);
			CHECK(equation->identifier != NULL);
			CHECK(equation->slice_count > 0U);
			CHECK(equation->first_slice < model->selector_slice_count);
			CHECK(equation->slice_count <= model->selector_slice_count -
			      equation->first_slice);
			for (slice_index = 0; slice_index < equation->slice_count; slice_index++) {
				const struct orlix_tcti_register_selector_slice *slice =
					&model->selector_slices[equation->first_slice + slice_index];

				CHECK(slice->equation_index == selector->payload_index);
				CHECK(slice->width > 0U);
				CHECK(!object_span(model, slice->source_offset, slice->source_length));
			}
			equations++;
		} else if (!strcmp(selector->type, "Values.Group")) {
			const struct orlix_tcti_register_selector_group *group;
			size_t fragment_index;

			CHECK(selector->value_kind == ORLIX_TCTI_REGISTER_SELECTOR_VALUE_GROUP);
			CHECK(selector->payload_index < model->selector_group_count);
			group = &model->selector_groups[selector->payload_index];
			CHECK(group->selector_index == index);
			CHECK(group->fragment_count > 0U);
			CHECK(group->first_fragment < model->selector_group_fragment_count);
			CHECK(group->fragment_count <= model->selector_group_fragment_count -
			      group->first_fragment);
			for (fragment_index = 0; fragment_index < group->fragment_count;
			     fragment_index++) {
				const struct orlix_tcti_register_selector_group_fragment *fragment =
					&model->selector_group_fragments[group->first_fragment +
						fragment_index];

				CHECK(fragment->kind == ORLIX_TCTI_REGISTER_SELECTOR_VALUE_LITERAL ||
				      fragment->kind == ORLIX_TCTI_REGISTER_SELECTOR_VALUE_EQUATION ||
				      fragment->kind == ORLIX_TCTI_REGISTER_SELECTOR_VALUE_GROUP);
				if (fragment->kind == ORLIX_TCTI_REGISTER_SELECTOR_VALUE_LITERAL)
					CHECK(fragment->payload_index < model->selector_literal_count);
				else if (fragment->kind == ORLIX_TCTI_REGISTER_SELECTOR_VALUE_EQUATION)
					CHECK(fragment->payload_index < model->selector_equation_count);
				else
					CHECK(fragment->payload_index < model->selector_group_count);
				CHECK(fragment->source_offset <= model->source_length);
				CHECK(fragment->source_length <= model->source_length -
				      fragment->source_offset);
			}
			groups++;
		} else {
			CHECK(selector->value_kind == ORLIX_TCTI_REGISTER_SELECTOR_VALUE_NONE);
			CHECK(selector->payload_index == UINT32_MAX);
			CHECK(selector->value_expression == UINT32_MAX);
		}
	}
	CHECK(literals > 0U);
	CHECK(equations > 0U);
	CHECK(groups > 0U);
	return 0;
}

static int unique_expression_spans(const struct orlix_tcti_register_model *model)
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

static int selector_value(const struct orlix_tcti_register_model *model,
			  uint32_t encoding_index, const char *name, const char *value)
{
	size_t index;

	for (index = 0; index < model->system_selector_count; index++) {
		const struct orlix_tcti_register_system_selector *selector =
			&model->system_selectors[index];

		if (selector->encoding_index == encoding_index &&
		    !strcmp(selector->name, name) && !strcmp(selector->value, value))
			return 0;
	}
	return -1;
}

static int rndr_tuple(const struct orlix_tcti_register_model *model, const char *name,
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
		const struct orlix_tcti_field_identity *field = &model->fields[index];

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
		const struct orlix_tcti_register_accessor *accessor = &model->accessors[index];
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

static int source_spans_and_relationships(const struct orlix_tcti_register_model *model)
{
	size_t index;

	for (index = 0; index < model->register_count; index++) {
		const struct orlix_tcti_register_identity *register_identity = &model->registers[index];
		const struct orlix_tcti_register_identity *metadata_owner;

		CHECK(!object_span(model, register_identity->source_offset,
			register_identity->source_length));
		CHECK(register_identity->parent_register == UINT32_MAX ||
		      register_identity->parent_register < index);
		CHECK(register_identity->metadata_owner < model->register_count);
		CHECK(register_identity->metadata_index < model->metadata_count);
		metadata_owner = &model->registers[register_identity->metadata_owner];
		CHECK(model->metadata[register_identity->metadata_index].source_offset ==
		      metadata_owner->metadata_offset);
		CHECK(model->metadata[register_identity->metadata_index].source_length ==
		      metadata_owner->metadata_length);
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
		if (register_identity->parent_register != UINT32_MAX)
			CHECK(register_identity->metadata_index ==
			      model->registers[register_identity->parent_register].metadata_index);
		if (!strcmp(register_identity->type, "RegisterBlock")) {
			CHECK(register_identity->default_access_expression <
			      model->expression_count);
			CHECK(!strcmp(model->expressions[
			      register_identity->default_access_expression].type,
			      "Accessors.Permission.AccessTypes.Memory.ReadWriteAccess"));
		} else {
			CHECK(register_identity->default_access_expression == UINT32_MAX);
		}
	}
	for (index = 0; index < model->field_count; index++) {
		const struct orlix_tcti_field_identity *field = &model->fields[index];

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
	CHECK(model->field_value_relation_count == model->field_count);
	for (index = 0; index < model->field_value_relation_count; index++) {
		const struct orlix_tcti_register_field_value_relation *relation =
			&model->field_value_relations[index];
		const struct orlix_tcti_field_identity *field = &model->fields[index];
		size_t candidate;

		CHECK(relation->field_index == index);
		CHECK(relation->source_offset == field->source_offset);
		CHECK(relation->source_length == field->source_length);
		CHECK(relation->field_condition_expression ==
		      field->condition_expression);
		CHECK(relation->wrapper_condition_expression ==
		      field->wrapper_condition_expression);
		if (!relation->value_candidate_count)
			CHECK(relation->first_value_candidate == UINT32_MAX);
		else {
			CHECK(relation->first_value_candidate <
			      model->field_value_candidate_count);
			CHECK(relation->value_candidate_count <=
			      model->field_value_candidate_count -
			      relation->first_value_candidate);
			for (candidate = 0; candidate < relation->value_candidate_count;
			     candidate++) {
				const struct orlix_tcti_register_field_value_candidate *entry =
					&model->field_value_candidates[
						relation->first_value_candidate + candidate];

				CHECK(entry->field_index == index);
				CHECK(entry->kind == ORLIX_TCTI_REGISTER_FIELD_VALUE_SOURCE_VALUES ||
				      entry->kind == ORLIX_TCTI_REGISTER_FIELD_VALUE_SOURCE_IMPLEMENTATION_DEFINED);
				CHECK(entry->valueset_index < model->valueset_count);
				CHECK(model->valuesets[entry->valueset_index].field_index == index);
				CHECK(!object_span(model, entry->source_offset, entry->source_length));
			}
		}
		if (!relation->constraint_candidate_count)
			CHECK(relation->first_constraint_candidate == UINT32_MAX);
		else {
			CHECK(relation->first_constraint_candidate <
			      model->field_constraint_candidate_count);
			CHECK(relation->constraint_candidate_count <=
			      model->field_constraint_candidate_count -
			      relation->first_constraint_candidate);
			for (candidate = 0; candidate < relation->constraint_candidate_count;
			     candidate++) {
				const struct orlix_tcti_register_field_constraint_candidate *entry =
					&model->field_constraint_candidates[
						relation->first_constraint_candidate + candidate];

				CHECK(entry->field_index == index);
				CHECK(entry->constraint_index < model->constraint_count);
				CHECK(model->constraints[entry->constraint_index].field_index == index);
				CHECK(entry->kind == model->constraints[entry->constraint_index].kind);
			}
		}
	}
	for (index = 0; index < model->fieldset_count; index++) {
		const struct orlix_tcti_register_fieldset *fieldset = &model->fieldsets[index];

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
		const struct orlix_tcti_conditional_field_branch *branch =
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
		const struct orlix_tcti_register_range *range = &model->ranges[index];

		CHECK(range->field_index < model->field_count);
		CHECK(!object_span(model, range->source_offset, range->source_length));
	}
	for (index = 0; index < model->valueset_count; index++) {
		const struct orlix_tcti_register_valueset *valueset = &model->valuesets[index];

		CHECK(valueset->field_index == UINT32_MAX ||
		      valueset->field_index < model->field_count);
		CHECK(!object_span(model, valueset->source_offset, valueset->source_length));
	}
	for (index = 0; index < model->domain_count; index++) {
		const struct orlix_tcti_value_domain *domain = &model->domains[index];

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
		const struct orlix_tcti_register_accessor *accessor = &model->accessors[index];

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
		const struct orlix_tcti_register_system_encoding *encoding =
			&model->system_encodings[index];

		CHECK(encoding->accessor_index < model->accessor_count);
		CHECK(!strcmp(encoding->type, "Encoding"));
		CHECK(!object_span(model, encoding->source_offset, encoding->source_length));
	}
	for (index = 0; index < model->system_selector_count; index++) {
		const struct orlix_tcti_register_system_selector *selector =
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
		const struct orlix_tcti_register_constraint *constraint = &model->constraints[index];
		size_t item;

		CHECK(constraint->kind == ORLIX_TCTI_REGISTER_CONSTRAINT_NULL ||
		      constraint->kind == ORLIX_TCTI_REGISTER_CONSTRAINT_VALUESET ||
		      constraint->kind == ORLIX_TCTI_REGISTER_CONSTRAINT_ARRAY);
		CHECK(constraint->field_index == UINT32_MAX ||
		      constraint->field_index < model->field_count);
		CHECK(constraint->source_offset <= model->source_length);
		CHECK(constraint->source_length <= model->source_length - constraint->source_offset);
		if (constraint->kind == ORLIX_TCTI_REGISTER_CONSTRAINT_NULL) {
			CHECK(constraint->source_length == 4U);
			CHECK(!memcmp(model->source + constraint->source_offset, "null", 4U));
		} else if (constraint->kind == ORLIX_TCTI_REGISTER_CONSTRAINT_ARRAY) {
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
		if (constraint->kind == ORLIX_TCTI_REGISTER_CONSTRAINT_NULL) {
			CHECK(constraint->valueset_index == UINT32_MAX);
			CHECK(constraint->item_count == 0U);
		}
		if (constraint->kind == ORLIX_TCTI_REGISTER_CONSTRAINT_VALUESET)
			CHECK(constraint->valueset_index < model->valueset_count);
		if (constraint->kind == ORLIX_TCTI_REGISTER_CONSTRAINT_ARRAY)
			CHECK(constraint->item_count > 0U);
		for (item = 0; item < constraint->item_count; item++) {
			const struct orlix_tcti_register_constraint_item *entry =
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
		const struct orlix_tcti_register_link *link = &model->links[index];

		CHECK(link->domain_index < model->domain_count);
		CHECK(link->key != NULL);
		CHECK(link->value != NULL);
		CHECK(link->source_offset <= model->source_length);
		CHECK(link->source_length <= model->source_length - link->source_offset);
	}
	for (index = 0; index < model->expression_count; index++) {
		const struct orlix_tcti_register_expression *expression = &model->expressions[index];
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
			const struct orlix_tcti_register_expression *nested;

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

static int non_system_accessor_census(const struct orlix_tcti_register_model *model)
{
	size_t index;
	size_t next_offset_expression = 0;
	size_t block = 0;
	size_t block_array = 0;
	size_t external_debug = 0;
	size_t implementation_defined_array = 0;
	size_t memory_mapped = 0;
	size_t external_power_domain_null = 0;
	size_t external_range_null = 0;
	size_t memory_frame_null = 0;
	size_t memory_instance_null = 0;
	size_t memory_power_domain_null = 0;
	size_t memory_range_null = 0;

	for (index = 0; index < model->accessor_count; index++) {
		const struct orlix_tcti_register_accessor *accessor = &model->accessors[index];
		int is_block = !strcmp(accessor->type, "Accessors.BlockAccess");
		int is_block_array = !strcmp(accessor->type,
			"Accessors.BlockAccessArray");
		int is_implementation_defined_array = !strcmp(accessor->type,
			"Accessors.ImplementationDefinedOffsetAccessorArray");
		int is_external_debug = !strcmp(accessor->type,
			"Accessors.ExternalDebug");
		int is_memory_mapped = !strcmp(accessor->type,
			"Accessors.MemoryMapped");
		int is_non_system = is_block || is_block_array ||
			is_implementation_defined_array || is_external_debug ||
			is_memory_mapped;
		size_t offset;

		if (!is_non_system)
			continue;
		CHECK(accessor->register_index < model->register_count);
		CHECK(accessor->condition_expression < model->expression_count);
		CHECK(accessor->access_expression < model->expression_count);
		CHECK(model->expressions[accessor->condition_expression].parent_expression ==
		      UINT32_MAX);
		CHECK(model->expressions[accessor->access_expression].parent_expression ==
		      UINT32_MAX);
		CHECK(accessor->first_system_encoding == UINT32_MAX);
		CHECK(accessor->system_encoding_count == 0U);
		CHECK(accessor->offset_expression_count > 0U);
		CHECK(accessor->first_offset_expression == next_offset_expression);
		CHECK(accessor->offset_expression_count <=
		      model->accessor_offset_expression_count - next_offset_expression);
		for (offset = 0; offset < accessor->offset_expression_count; offset++) {
			const struct orlix_tcti_register_accessor_offset_expression *entry =
				&model->accessor_offset_expressions[next_offset_expression + offset];

			CHECK(entry->accessor_index == index);
			CHECK(entry->expression_index < model->expression_count);
			CHECK(model->expressions[entry->expression_index].parent_expression ==
			      UINT32_MAX);
			CHECK(!object_span(model, entry->source_offset, entry->source_length));
		}
		next_offset_expression += accessor->offset_expression_count;

		if (is_block || is_block_array) {
			CHECK(accessor->references_expression < model->expression_count);
			CHECK(model->expressions[accessor->references_expression].parent_expression ==
			      UINT32_MAX);
		} else {
			CHECK(accessor->references_expression == UINT32_MAX);
		}
		if (is_block_array || is_implementation_defined_array) {
			CHECK(accessor->index_variable != NULL);
			CHECK(accessor->index_start != UINT32_MAX);
			CHECK(accessor->index_width > 0U);
		} else {
			CHECK(accessor->index_variable == NULL);
			CHECK(accessor->index_start == UINT32_MAX);
			CHECK(accessor->index_width == 0U);
		}
		if (is_external_debug || is_memory_mapped) {
			CHECK(accessor->component != NULL);
			CHECK(accessor->component_is_null == 0U);
			if (accessor->instance_is_null)
				CHECK(accessor->instance == NULL);
			else
				CHECK(accessor->instance != NULL);
			if (accessor->power_domain_is_null)
				CHECK(accessor->power_domain == NULL);
			else
				CHECK(accessor->power_domain != NULL);
			if (accessor->range_is_null) {
				CHECK(accessor->range_start == UINT32_MAX);
				CHECK(accessor->range_width == 0U);
			} else {
				CHECK(accessor->range_start != UINT32_MAX);
				CHECK(accessor->range_width > 0U);
			}
		}
		if (is_block)
			block++;
		else if (is_block_array)
			block_array++;
		else if (is_implementation_defined_array)
			implementation_defined_array++;
		else if (is_external_debug) {
			external_debug++;
			CHECK(accessor->instance_is_null == 0U);
			CHECK(accessor->frame == NULL);
			external_power_domain_null += accessor->power_domain_is_null;
			external_range_null += accessor->range_is_null;
		} else {
			memory_mapped++;
			memory_frame_null += accessor->frame_is_null;
			memory_instance_null += accessor->instance_is_null;
			memory_power_domain_null += accessor->power_domain_is_null;
			memory_range_null += accessor->range_is_null;
			if (accessor->frame_is_null)
				CHECK(accessor->frame == NULL);
			else
				CHECK(accessor->frame != NULL);
		}
	}
	CHECK(next_offset_expression == model->accessor_offset_expression_count);
	CHECK(block == 213U);
	CHECK(block_array == 23U);
	CHECK(implementation_defined_array == 3U);
	CHECK(external_debug == 201U);
	CHECK(memory_mapped == 598U);
	CHECK(external_power_domain_null == 201U);
	CHECK(external_range_null == 197U);
	CHECK(memory_frame_null == 180U);
	CHECK(memory_instance_null == 88U);
	CHECK(memory_power_domain_null == 598U);
	CHECK(memory_range_null == 576U);
	return 0;
}

static int memory_permission_census(const struct orlix_tcti_register_model *model)
{
	size_t index;
	size_t implementation_defined_null = 0;
	size_t implementation_defined_array = 0;
	size_t constrained_entries = 0;
	size_t read_r = 0, read_raz = 0, read_res0 = 0, read_reserved = 0;
	size_t read_unknown = 0, read_error = 0;
	size_t write_w = 0, write_wi = 0, write_res0 = 0;
	size_t write_reserved = 0, write_error = 0;

	CHECK(model->memory_access_count == ORLIX_TCTI_REGISTER_MEMORY_ACCESS_COUNT);
	CHECK(model->implementation_defined_permission_count == 17U);
	for (index = 0; index < model->memory_access_count; index++) {
		const struct orlix_tcti_register_memory_access *access =
			&model->memory_accesses[index];

		CHECK(access->form == ORLIX_TCTI_REGISTER_MEMORY_ACCESS_EXPLICIT_OBJECT);
		CHECK(access->owner_kind ==
		      ORLIX_TCTI_REGISTER_MEMORY_ACCESS_OWNER_EXPRESSION);
		CHECK(access->owner_index < model->expression_count);
		CHECK(!strcmp(model->expressions[access->owner_index].type,
		      "Accessors.Permission.AccessTypes.Memory.ReadWriteAccess"));
		switch (access->read) {
		case ORLIX_TCTI_REGISTER_MEMORY_READ_R: read_r++; break;
		case ORLIX_TCTI_REGISTER_MEMORY_READ_RAZ: read_raz++; break;
		case ORLIX_TCTI_REGISTER_MEMORY_READ_RES0: read_res0++; break;
		case ORLIX_TCTI_REGISTER_MEMORY_READ_RESERVED: read_reserved++; break;
		case ORLIX_TCTI_REGISTER_MEMORY_READ_UNKNOWN: read_unknown++; break;
		case ORLIX_TCTI_REGISTER_MEMORY_READ_ERROR: read_error++; break;
		default: CHECK(0);
		}
		switch (access->write) {
		case ORLIX_TCTI_REGISTER_MEMORY_WRITE_W: write_w++; break;
		case ORLIX_TCTI_REGISTER_MEMORY_WRITE_WI: write_wi++; break;
		case ORLIX_TCTI_REGISTER_MEMORY_WRITE_RES0: write_res0++; break;
		case ORLIX_TCTI_REGISTER_MEMORY_WRITE_RESERVED: write_reserved++; break;
		case ORLIX_TCTI_REGISTER_MEMORY_WRITE_ERROR: write_error++; break;
		default: CHECK(0);
		}
		CHECK(access->read_origin == ORLIX_TCTI_REGISTER_MEMORY_ACCESS_EXPLICIT);
		CHECK(access->write_origin == ORLIX_TCTI_REGISTER_MEMORY_ACCESS_EXPLICIT);
		CHECK(access->legacy_sentinel == ORLIX_TCTI_REGISTER_MEMORY_ACCESS_LEGACY_NONE);
		CHECK(!object_span(model, access->source_offset, access->source_length));
		CHECK(!scalar_span(model, access->read_offset, access->read_length));
		CHECK(!scalar_span(model, access->write_offset, access->write_length));
	}
	CHECK(read_r == 1162U);
	CHECK(read_raz == 251U);
	CHECK(read_res0 == 19U);
	CHECK(read_reserved == 102U);
	CHECK(read_unknown == 25U);
	CHECK(read_error == 303U);
	CHECK(write_w == 682U);
	CHECK(write_wi == 291U);
	CHECK(write_res0 == 19U);
	CHECK(write_reserved == 567U);
	CHECK(write_error == 303U);
	for (index = 0; index < model->implementation_defined_permission_count;
	     index++) {
		const struct orlix_tcti_register_implementation_defined_permission *permission =
			&model->implementation_defined_permissions[index];
		size_t member;

		CHECK(permission->expression_index < model->expression_count);
		CHECK(!strcmp(model->expressions[permission->expression_index].type,
		      "Accessors.Permission.AccessTypes.Memory.ImplementationDefined"));
		CHECK(!object_span(model, permission->source_offset,
			permission->source_length));
		if (permission->kind ==
		    ORLIX_TCTI_REGISTER_IMPLEMENTATION_DEFINED_CONSTRAINT_NULL) {
			CHECK(permission->first_memory_access == UINT32_MAX);
			CHECK(permission->memory_access_count == 0U);
			implementation_defined_null++;
			continue;
		}
		CHECK(permission->kind ==
		      ORLIX_TCTI_REGISTER_IMPLEMENTATION_DEFINED_CONSTRAINT_ARRAY);
		CHECK(permission->first_memory_access < model->memory_access_count);
		CHECK(permission->memory_access_count <= model->memory_access_count -
		      permission->first_memory_access);
		for (member = 0; member < permission->memory_access_count; member++) {
			const struct orlix_tcti_register_memory_access *access =
				&model->memory_accesses[permission->first_memory_access + member];

			CHECK(model->expressions[access->owner_index].parent_expression ==
			      permission->expression_index);
		}
		constrained_entries += permission->memory_access_count;
		implementation_defined_array++;
	}
	CHECK(implementation_defined_null == 14U);
	CHECK(implementation_defined_array == 3U);
	CHECK(constrained_entries == 6U);
	return 0;
}

static int pinned_model(const char *path)
{
	struct orlix_tcti_register_model model = { 0 };
	struct orlix_tcti_register_model_error error = { 0 };
	char *source;
	size_t source_length;
	int result = -1;

	source = read_file(path, &source_length);
	CHECK(source != NULL);
	if (orlix_tcti_register_model_import(source, source_length, &model, &error)) {
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
	CHECK(model.register_count == ORLIX_TCTI_REGISTER_RECORD_COUNT);
	CHECK(model.metadata_count == ORLIX_TCTI_REGISTER_METADATA_COUNT);
	CHECK(model.memory_access_count == ORLIX_TCTI_REGISTER_MEMORY_ACCESS_COUNT);
	CHECK(model.implementation_defined_permission_count == 17U);
	CHECK(model.fieldset_count == ORLIX_TCTI_REGISTER_FIELDSET_COUNT);
	CHECK(model.field_count == ORLIX_TCTI_REGISTER_FIELD_COUNT);
	CHECK(model.range_count == ORLIX_TCTI_REGISTER_RANGE_COUNT);
	CHECK(model.top_level_valueset_count == ORLIX_TCTI_REGISTER_VALUESET_COUNT);
	CHECK(model.top_level_domain_count == ORLIX_TCTI_REGISTER_DOMAIN_COUNT);
	CHECK(model.accessor_count == ORLIX_TCTI_REGISTER_ACCESSOR_COUNT);
	CHECK(model.field_branch_count == 3314U);
	CHECK(model.constraint_count == 1656U);
	CHECK(model.constraint_domain_count == 2958U);
	CHECK(model.system_encoding_count == 2558U);
	CHECK(model.system_selector_count > 0U);
	CHECK(model.expression_count > 0U);
	CHECK(!pinned_type_histograms(&model));
	CHECK(!memory_permission_census(&model));
	CHECK(!non_system_accessor_census(&model));
	CHECK(!rndr_tuple(&model, "RNDR", "'000'"));
	CHECK(!rndr_tuple(&model, "RNDRRS", "'001'"));
	CHECK(!selector_payloads(&model));
	CHECK(!source_spans_and_relationships(&model));
	CHECK(!unique_expression_spans(&model));

	/* A populated model is never overwritten, leaked, or silently reset. */
	memset(&error, 0, sizeof(error));
	CHECK(orlix_tcti_register_model_import(source, source_length, &model, &error) == -1);
	CHECK(error.code == ORLIX_TCTI_REGISTER_MODEL_INVALID_ARGUMENT);
	CHECK(model.source != NULL);
	CHECK(model.register_count == ORLIX_TCTI_REGISTER_RECORD_COUNT);

	/*
	 * Keep exactly one full pinned import. The successful import above validates
	 * the exact source digest, metadata, and complete typed census. Malformed
	 * grammar and rollback cases are intentionally exercised by compact fixtures
	 * so this host test remains a bounded ordinary build-time check.
	 */
	result = 0;
out:
	orlix_tcti_register_model_destroy(&model);
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
	CHECK(!typed_memory_permission_schema_rejected());
	CHECK(!expect_rollback("[{\"_type\":\"Register\",\"name\":\"x\"}]",
		ORLIX_TCTI_REGISTER_MODEL_INVALID_SOURCE));
	CHECK(!non_system_accessor_rejection_and_rollback());
	CHECK(!input_and_depth_limits());
	CHECK(!pinned_model(argv[1]));
	puts("target register model tests: passed");
	return 0;
}
