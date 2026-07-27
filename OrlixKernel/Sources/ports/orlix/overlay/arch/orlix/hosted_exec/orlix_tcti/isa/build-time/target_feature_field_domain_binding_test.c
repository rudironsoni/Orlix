/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_feature_field_domain_binding.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int check(int condition, const char *what)
{
	if (!condition)
		fprintf(stderr, "FAIL: %s\n", what);
	return !condition;
}

static char *read_file(const char *path, size_t *length)
{
	FILE *file = fopen(path, "rb");
	long size;
	char *source;
	if (!file)
		return NULL;
	if (fseek(file, 0, SEEK_END) || (size = ftell(file)) < 0 ||
	    fseek(file, 0, SEEK_SET)) {
		fclose(file);
		return NULL;
	}
	source = malloc((size_t)size + 1);
	if (!source || fread(source, 1, (size_t)size, file) != (size_t)size) {
		free(source);
		fclose(file);
		return NULL;
	}
	fclose(file);
	source[size] = '\0';
	*length = (size_t)size;
	return source;
}

static int synthetic_tests(void)
{
	struct orlix_tcti_feature_node feature_nodes[] = {
		{ .kind = ORLIX_TCTI_FEATURE_FIELD, .field = {
			.state = "AArch64", .register_name = "REG", .selector = "FIELD",
			.provenance = { 11, 19 },
		} },
		{ .kind = ORLIX_TCTI_FEATURE_FIELD, .field = {
			.state = "AArch64", .register_name = "REG", .selector = "FIELD",
			.provenance = { 41, 19 },
		} },
	};
	struct orlix_tcti_register_identity identities[] = {
		{ .name = "REG", .state = "AArch64", .source_offset = 101,
		  .source_length = 41 },
		{ .name = "REG", .state = "AArch64", .source_offset = 151,
		  .source_length = 42 },
	};
	struct orlix_tcti_field_identity fields[] = {
		{ .name = "FIELD", .type = "Fields.ConstantField", .width = 64,
		  .register_index = 0, .fieldset_index = 0, .first_range = 0,
		  .range_count = 1, .condition_expression = UINT32_MAX,
		  .wrapper_condition_expression = UINT32_MAX,
		  .source_offset = 201, .source_length = 31 },
		{ .name = "FIELD", .type = "Fields.ConstantField", .width = 64,
		  .register_index = 0, .fieldset_index = 1, .first_range = 1,
		  .range_count = 1, .condition_expression = UINT32_MAX,
		  .wrapper_condition_expression = UINT32_MAX,
		  .source_offset = 251, .source_length = 32 },
	};
	struct orlix_tcti_register_fieldset fieldsets[] = {
		{ .register_index = 0, .width = 64,
		  .condition_expression = UINT32_MAX,
		  .source_offset = 195, .source_length = 45 },
		{ .register_index = 0, .width = 64,
		  .condition_expression = UINT32_MAX,
		  .source_offset = 245, .source_length = 45 },
	};
	struct orlix_tcti_register_range ranges[] = {
		{ .start = 7, .width = 1, .field_index = 0,
		  .source_offset = 210, .source_length = 4 },
		{ .start = 7, .width = 1, .field_index = 1,
		  .source_offset = 260, .source_length = 4 },
	};
	struct orlix_tcti_value_domain domains[] = {
		{ .type = "Values.Value", .value = "'0'", .field_index = UINT32_MAX,
		  .parent_domain = UINT32_MAX, .condition_expression = UINT32_MAX,
		  .first_child_domain = UINT32_MAX,
		  .valueset_index = UINT32_MAX,
		  .nested_valueset_index = UINT32_MAX,
		  .source_offset = 215, .source_length = 4 },
		{ .type = "Values.Value", .value = "'0'", .field_index = UINT32_MAX,
		  .parent_domain = UINT32_MAX, .condition_expression = UINT32_MAX,
		  .first_child_domain = UINT32_MAX,
		  .valueset_index = UINT32_MAX,
		  .nested_valueset_index = UINT32_MAX,
		  .source_offset = 265, .source_length = 4 },
	};
	struct orlix_tcti_register_field_value_relation relations[] = {
		{ .field_index = 0, .first_value_candidate = UINT32_MAX,
		  .first_constraint_candidate = UINT32_MAX,
		  .field_condition_expression = UINT32_MAX,
		  .wrapper_condition_expression = UINT32_MAX,
		  .source_offset = 201, .source_length = 31 },
		{ .field_index = 1, .first_value_candidate = UINT32_MAX,
		  .first_constraint_candidate = UINT32_MAX,
		  .field_condition_expression = UINT32_MAX,
		  .wrapper_condition_expression = UINT32_MAX,
		  .source_offset = 251, .source_length = 32 },
	};
	struct orlix_tcti_feature_model feature_model = {
		.nodes = feature_nodes, .node_count = 2,
	};
	struct orlix_tcti_register_model register_model = {
		.registers = identities, .register_count = 1, .fields = fields,
		.field_count = 1, .field_value_relations = relations,
		.field_value_relation_count = 1, .fieldsets = fieldsets,
		.fieldset_count = 1, .ranges = ranges, .range_count = 1,
		.domains = domains, .domain_count = 1,
	};
	struct orlix_tcti_feature_field_domain_bindings bindings;
	struct orlix_tcti_feature_field_domain_binding_error error;
	int failed = 0;
	failed |= check(!orlix_tcti_target_feature_field_domain_bindings_build(
		&feature_model, &register_model, &bindings, &error), "synthetic map");
	failed |= check(bindings.count == 2, "two synthetic source occurrences");
	failed |= check(bindings.items[0].disposition ==
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_MAPPED, "synthetic mapped disposition");
	failed |= check(bindings.items[1].disposition ==
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_MAPPED, "duplicate synthetic mapped disposition");
	failed |= check(bindings.items[0].feature_node_index == 0 &&
		bindings.items[0].identity_group_index == 0 &&
		bindings.items[0].occurrence_count == 2 &&
		bindings.items[0].register_index == 0 &&
		bindings.items[0].field_index == 0 &&
		bindings.items[0].value_relation_index == 0, "synthetic indexes");
	failed |= check(bindings.items[1].feature_node_index == 1 &&
		bindings.items[1].identity_group_index == 0 &&
		bindings.items[1].occurrence_count == 2,
		"duplicate identity retains its own source obligation and group");
	failed |= check(bindings.items[0].feature_provenance.offset == 11 &&
		bindings.items[0].register_source_offset == 101 &&
		bindings.items[0].field_source_offset == 201 &&
		bindings.items[0].value_relation_source_offset == 201 &&
		bindings.items[0].domain.field_width == 1 &&
		bindings.items[0].domain.value_member_count == 1,
		"synthetic exact source spans");
	orlix_tcti_target_feature_field_domain_bindings_destroy(&bindings);

	feature_nodes[0].field.selector = "MISSING";
	feature_nodes[1].field.selector = "MISSING";
	failed |= check(!orlix_tcti_target_feature_field_domain_bindings_build(
		&feature_model, &register_model, &bindings, &error), "missing field binds");
	failed |= check(bindings.items[0].disposition ==
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_MISSING_FIELD, "missing field fails closed");
	orlix_tcti_target_feature_field_domain_bindings_destroy(&bindings);
	feature_nodes[0].field.selector = "FIELD";
	feature_nodes[1].field.selector = "FIELD";
	identities[0].name = "OTHER";
	failed |= check(!orlix_tcti_target_feature_field_domain_bindings_build(
		&feature_model, &register_model, &bindings, &error), "missing register binds");
	failed |= check(bindings.items[0].disposition ==
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_MISSING_REGISTER, "missing register fails closed");
	orlix_tcti_target_feature_field_domain_bindings_destroy(&bindings);
	identities[0].name = "REG";
	register_model.register_count = 2;
	failed |= check(!orlix_tcti_target_feature_field_domain_bindings_build(
		&feature_model, &register_model, &bindings, &error), "ambiguous register binds");
	failed |= check(bindings.items[0].disposition ==
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_AMBIGUOUS_REGISTER, "ambiguous register fails closed");
	orlix_tcti_target_feature_field_domain_bindings_destroy(&bindings);
	register_model.register_count = 1;
	register_model.field_count = 2;
	register_model.field_value_relation_count = 2;
	register_model.fieldset_count = 2;
	register_model.range_count = 2;
	register_model.domain_count = 2;
	failed |= check(!orlix_tcti_target_feature_field_domain_bindings_build(
		&feature_model, &register_model, &bindings, &error),
		"equivalent field declarations bind");
	failed |= check(bindings.items[0].disposition ==
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_MAPPED &&
		bindings.items[0].domain.equivalent_field_count == 2,
		"equivalent field declarations normalize without ambiguity");
	orlix_tcti_target_feature_field_domain_bindings_destroy(&bindings);
	domains[1].value = "'1'";
	failed |= check(!orlix_tcti_target_feature_field_domain_bindings_build(
		&feature_model, &register_model, &bindings, &error),
		"non-equivalent field declarations bind");
	failed |= check(bindings.items[0].disposition ==
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_AMBIGUOUS_FIELD,
		"non-equivalent field declarations remain ambiguous");
	orlix_tcti_target_feature_field_domain_bindings_destroy(&bindings);
	domains[1].value = "'0'";
	register_model.field_count = 1;
	register_model.fieldset_count = 0;
	failed |= check(orlix_tcti_target_feature_field_domain_bindings_build(
		&feature_model, &register_model, &bindings, &error),
		"missing fieldset fails hard");
	register_model.fieldset_count = 1;
	fields[0].type = "Fields.Dynamic";
	failed |= check(orlix_tcti_target_feature_field_domain_bindings_build(
		&feature_model, &register_model, &bindings, &error),
		"unsupported field type fails hard");
	fields[0].type = "Fields.ConstantField";
	fields[0].width = 32;
	failed |= check(orlix_tcti_target_feature_field_domain_bindings_build(
		&feature_model, &register_model, &bindings, &error) &&
		error.code == ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_INVALID_MODEL,
		"field and fieldset width mismatch fails hard");
	fields[0].width = 64;
	ranges[0].width = 0;
	failed |= check(orlix_tcti_target_feature_field_domain_bindings_build(
		&feature_model, &register_model, &bindings, &error) &&
		error.code == ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_INVALID_MODEL,
		"zero-width range fails hard");
	ranges[0].width = 1;
	domains[0].type = "Values.Unsupported";
	failed |= check(orlix_tcti_target_feature_field_domain_bindings_build(
		&feature_model, &register_model, &bindings, &error),
		"unsupported value grammar fails hard");
	domains[0].type = "Values.Value";
	relations[0].value_candidate_count = 1;
	failed |= check(orlix_tcti_target_feature_field_domain_bindings_build(
		&feature_model, &register_model, &bindings, &error) &&
		error.code == ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_INVALID_MODEL,
		"malformed relation fails hard");
	relations[0].value_candidate_count = 0;
	register_model.field_value_relation_count = 0;
	failed |= check(orlix_tcti_target_feature_field_domain_bindings_build(
		&feature_model, &register_model, &bindings, &error) &&
		error.code == ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_INVALID_MODEL,
		"missing relation index fails hard");
	return failed;
}

int main(int argc, char **argv)
{
	const char *features_path;
	const char *registers_path;
	struct orlix_tcti_feature_model features = { 0 };
	struct orlix_tcti_register_model registers = { 0 };
	struct orlix_tcti_feature_error feature_error = { 0 };
	struct orlix_tcti_register_model_error register_error = { 0 };
	struct orlix_tcti_feature_field_domain_bindings bindings;
	struct orlix_tcti_feature_field_domain_binding_error binding_error;
	char *feature_source, *register_source;
	size_t feature_length, register_length, index;
	size_t fields = 0, unique_groups = 0, mapped = 0, ambiguous = 0, mpam = 0;
	int failed = synthetic_tests();
	if (argc != 3) {
		fprintf(stderr, "usage: %s Features.json Registers.json\n", argv[0]);
		return 2;
	}
	features_path = argv[1];
	registers_path = argv[2];
	feature_source = read_file(features_path, &feature_length);
	register_source = read_file(registers_path, &register_length);
	if (!feature_source || !register_source) {
		perror(!feature_source ? features_path : registers_path);
		free(feature_source);
		free(register_source);
		return 1;
	}
	if (orlix_tcti_target_feature_model_import(feature_source, feature_length,
		&features, &feature_error)) {
		fprintf(stderr, "Features.json:%zu: %s\n", feature_error.offset,
			feature_error.message);
		free(feature_source);
		free(register_source);
		return 1;
	}
	if (orlix_tcti_register_model_import(register_source, register_length, &registers,
		&register_error)) {
		fprintf(stderr, "Registers.json:%zu: %s\n", register_error.offset,
			register_error.message);
		orlix_tcti_target_feature_model_destroy(&features);
		free(feature_source);
		free(register_source);
		return 1;
	}
	failed |= check(!orlix_tcti_target_feature_field_domain_bindings_build(&features,
		&registers, &bindings, &binding_error), "pinned field-domain binding");
	if (!bindings.count && binding_error.code)
		fprintf(stderr, "binding error %d at %zu: %s\n", binding_error.code,
			binding_error.offset, binding_error.message);
	if (bindings.count != 605)
		fprintf(stderr, "pinned Types.Field count: got %zu, expected 605\n",
			bindings.count);
	failed |= check(bindings.count == 605, "pinned Types.Field count");
	failed |= check(bindings.alternative_count == 606,
		"pinned source declaration alternative count");
	for (index = 0; index < bindings.count; index++) {
		const struct orlix_tcti_feature_field_domain_binding *binding =
			&bindings.items[index];
		fields++;
		failed |= check(binding->alternative_count > 0U &&
			binding->first_alternative <= bindings.alternative_count &&
			binding->alternative_count <= bindings.alternative_count -
				binding->first_alternative,
			"binding alternative slice");
		if ((size_t)binding->identity_group_index + 1 > unique_groups)
			unique_groups = (size_t)binding->identity_group_index + 1;
		if (binding->disposition == ORLIX_TCTI_FEATURE_FIELD_DOMAIN_MAPPED)
			mapped++;
		if (binding->disposition == ORLIX_TCTI_FEATURE_FIELD_DOMAIN_AMBIGUOUS_FIELD)
			ambiguous++;
		if (index == 292U) {
			mpam++;
			failed |= check(binding->alternative_count == 2U,
				"MPAM retains two source declarations");
			failed |= check(binding->feature_node_index == 4838U &&
				binding->identity_group_index == 205U &&
				binding->field_index == 13637U &&
				binding->domain.fieldset_index == 1981U &&
				binding->domain.equivalent_field_count == 2U &&
				binding->domain.register_width == 64U &&
				binding->domain.field_width == 1U &&
				binding->domain.value_member_count == 2U &&
				binding->domain.relation_constraint_count == 1U &&
				binding->domain.type ==
					ORLIX_TCTI_FEATURE_FIELD_DOMAIN_TYPE_CONSTANT &&
				binding->domain.signedness ==
					ORLIX_TCTI_FEATURE_FIELD_DOMAIN_UNSIGNED,
				"MPAM HAS_BW_CTRL resolves by semantic equivalence");
		}
		failed |= check(binding->feature_provenance.length &&
			binding->feature_provenance.offset + binding->feature_provenance.length <=
			feature_length, "pinned feature span in source");
		failed |= check(binding->disposition == ORLIX_TCTI_FEATURE_FIELD_DOMAIN_MAPPED ||
			binding->disposition == ORLIX_TCTI_FEATURE_FIELD_DOMAIN_AMBIGUOUS_FIELD,
			"pinned field is mapped or blocks on a typed ambiguity");
		if (binding->disposition == ORLIX_TCTI_FEATURE_FIELD_DOMAIN_MAPPED) {
			failed |= check(binding->register_source_length &&
				binding->register_source_offset + binding->register_source_length <=
				register_length && binding->field_source_length &&
				binding->field_source_offset + binding->field_source_length <=
				register_length && binding->value_relation_source_length,
				"pinned register and field spans in source");
		}
	}
	if (!(fields == 605 && unique_groups == 362 && mapped == 605 &&
	      ambiguous == 0 && mpam == 1))
		fprintf(stderr, "pinned totals: fields=%zu groups=%zu mapped=%zu ambiguous=%zu\n",
			fields, unique_groups, mapped, ambiguous);
	failed |= check(fields == 605 && unique_groups == 362 && mapped == 605 &&
		ambiguous == 0 && mpam == 1,
		"pinned field obligations resolve without conflation");
	orlix_tcti_target_feature_field_domain_bindings_destroy(&bindings);
	orlix_tcti_register_model_destroy(&registers);
	orlix_tcti_target_feature_model_destroy(&features);
	free(feature_source);
	free(register_source);
	return failed;
}
