/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_feature_model.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int check(int ok, const char *what)
{
	if (!ok)
		fprintf(stderr, "FAIL: %s\n", what);
	return !ok;
}

static char *read_file(const char *path, size_t *length)
{
	FILE *f = fopen(path, "rb");
	long n;
	char *s;
	if (!f) return NULL;
	if (fseek(f, 0, SEEK_END) || (n = ftell(f)) < 0 || fseek(f, 0, SEEK_SET)) { fclose(f); return NULL; }
	s = malloc((size_t)n + 1);
	if (!s || fread(s, 1, (size_t)n, f) != (size_t)n) { free(s); fclose(f); return NULL; }
	fclose(f); s[n] = '\0'; *length = (size_t)n; return s;
}

static int expect(const char *source, size_t length, enum orlix_tcti_feature_error_code code, const char *what)
{
	struct orlix_tcti_feature_model model;
	struct orlix_tcti_feature_error error = { 0 };
	int result = orlix_tcti_target_feature_model_import(source, length, &model, &error);
	if (!result)
		orlix_tcti_target_feature_model_destroy(&model);
	return check(result && error.code == code, what);
}

static char *replace_once(const char *source, size_t length, const char *from, const char *to, size_t *out_length)
{
	const char *where = strstr(source, from);
	size_t before, from_length = strlen(from), to_length = strlen(to);
	char *copy;
	if (!where) return NULL;
	before = (size_t)(where - source);
	copy = malloc(length - from_length + to_length + 1);
	if (!copy) return NULL;
	memcpy(copy, source, before);
	memcpy(copy + before, to, to_length);
	memcpy(copy + before + to_length, where + from_length, length - before - from_length);
	*out_length = length - from_length + to_length;
	copy[*out_length] = '\0';
	return copy;
}

int main(int argc, char **argv)
{
	const char *path = argc > 1 ? argv[1] : getenv("ORLIX_TCTI_FEATURES_JSON");
	struct orlix_tcti_feature_model model;
	struct orlix_tcti_feature_error error = { 0 };
	char *source, *mutation;
	char *bounded;
	size_t length, mutated_length;
	size_t i;
	int failed = 0;
	static const char short_source[] =
		"{\"_type\":\"Features\",\"_meta\":{\"version\":{\"architecture\":\"vFATAp1-A\","
		"\"build\":\"818\",\"ref\":\"2026-06_rel\",\"schema\":\"2.9.5\"}},"
		"\"parameters\":[],\"constraints\":[]}";
	static const char decoded_duplicate[] = "{\"x\":1,\"\\u0078\":2}";
	static const char unpaired_surrogate[] = "{\"x\":\"\\ud800\"}";
	failed |= expect(decoded_duplicate, sizeof(decoded_duplicate) - 1, ORLIX_TCTI_FEATURE_DUPLICATE_KEY, "decoded duplicate key");
	failed |= expect(unpaired_surrogate, sizeof(unpaired_surrogate) - 1, ORLIX_TCTI_FEATURE_INVALID_JSON, "unpaired JSON surrogate");
	failed |= expect(short_source, sizeof(short_source) - 1, ORLIX_TCTI_FEATURE_COUNT_MISMATCH, "count mutation");
	bounded = malloc(2 * 257 + 1);
	failed |= check(bounded != NULL, "allocate depth bound input");
	if (bounded) {
		for (i = 0; i < 257; i++) bounded[i] = '[';
		for (i = 0; i < 257; i++) bounded[257 + i] = ']';
		bounded[514] = '\0';
		failed |= expect(bounded, 514, ORLIX_TCTI_FEATURE_LIMIT, "JSON depth bound");
		free(bounded);
	}
	if (!path) {
		fprintf(stderr, "ORLIX_TCTI_FEATURES_JSON or a Features.json path is required\n");
		return 2;
	}
	source = read_file(path, &length);
	if (!source) { perror(path); return 1; }
	if (orlix_tcti_target_feature_model_import(source, length, &model, &error)) {
		failed |= check(0, error.message);
		free(source);
		return failed;
	}
	failed |= check(model.parameter_count == ORLIX_TCTI_FEATURE_PARAMETER_COUNT, "parameter count");
	failed |= check(model.constraint_count == ORLIX_TCTI_FEATURE_CONSTRAINT_COUNT, "constraint count");
	failed |= check(model.node_count > model.constraint_count, "symbolic nodes");
	{
		size_t nested_slices = 0;
		int slices_valid = 1;

		for (i = 0; i < model.node_count; i++) {
			const struct orlix_tcti_feature_node *parent = &model.nodes[i];
			uint32_t child;
			size_t prior_offset = 0;

			if (!parent->child_count)
				continue;
			if (parent->first_child > model.child_count ||
			    parent->child_count > model.child_count - parent->first_child) {
				slices_valid = 0;
				break;
			}
			for (child = 0; child < parent->child_count; child++) {
				uint32_t child_index =
					model.children[parent->first_child + child];
				const struct orlix_tcti_feature_node *direct;

				if (child_index >= model.node_count) {
					slices_valid = 0;
					break;
				}
				direct = &model.nodes[child_index];
				if (direct->provenance.offset < parent->provenance.offset ||
				    direct->provenance.length > parent->provenance.length ||
				    direct->provenance.offset - parent->provenance.offset >
					parent->provenance.length - direct->provenance.length ||
				    (child && direct->provenance.offset <= prior_offset)) {
					slices_valid = 0;
					break;
				}
				prior_offset = direct->provenance.offset;
				if (direct->child_count)
					nested_slices++;
			}
			if (!slices_valid)
				break;
		}
		failed |= check(slices_valid,
			"direct-child slices preserve bounds and source order");
		failed |= check(nested_slices != 0,
			"direct-child regression covers recursive child allocation");
	}
	{
		size_t field_count = 0;
		for (i = 0; i < model.node_count; i++)
			if (model.nodes[i].kind == ORLIX_TCTI_FEATURE_FIELD) {
				const struct orlix_tcti_feature_field *field =
					&model.nodes[i].field;
				field_count++;
				failed |= check(field->state && field->register_name &&
					field->selector,
					"complete field identity");
				failed |= check(field->instance.kind ==
					ORLIX_TCTI_FEATURE_FIELD_QUALIFIER_NULL &&
					field->slices.kind ==
					ORLIX_TCTI_FEATURE_FIELD_QUALIFIER_NULL,
					"explicit null field qualifiers retained");
				failed |= check(field->instance.provenance.length == 4 &&
					!memcmp(source + field->instance.provenance.offset,
						"null", 4) &&
					field->slices.provenance.length == 4 &&
					!memcmp(source + field->slices.provenance.offset,
						"null", 4),
					"field qualifier source provenance");
			}
		failed |= check(field_count != 0, "field references retained");
	}
	orlix_tcti_target_feature_model_destroy(&model);

	mutation = replace_once(source, length, "\"AST.Bool\"", "\"AST.Nope\"", &mutated_length);
	failed |= check(mutation != NULL, "find AST.Bool"); if (mutation) { failed |= expect(mutation, mutated_length, ORLIX_TCTI_FEATURE_UNSUPPORTED_GRAMMAR, "unknown AST type"); free(mutation); }
	mutation = replace_once(source, length, "\"&&\"", "\"@@\"", &mutated_length);
	failed |= check(mutation != NULL, "find binary operator"); if (mutation) { failed |= expect(mutation, mutated_length, ORLIX_TCTI_FEATURE_UNSUPPORTED_GRAMMAR, "unknown operator"); free(mutation); }
	mutation = replace_once(source, length, "\"UInt\"", "\"XInt\"", &mutated_length);
	failed |= check(mutation != NULL, "find function"); if (mutation) { failed |= expect(mutation, mutated_length, ORLIX_TCTI_FEATURE_UNSUPPORTED_GRAMMAR, "unknown function"); free(mutation); }
	mutation = replace_once(source, length, "\"instance\": null", "\"instance\": 0", &mutated_length);
	failed |= check(mutation != NULL, "find Types.Field instance"); if (mutation) { failed |= expect(mutation, mutated_length, ORLIX_TCTI_FEATURE_UNSUPPORTED_GRAMMAR, "non-null field instance"); free(mutation); }
	mutation = replace_once(source, length, "\"slices\": null", "\"slices\": []", &mutated_length);
	failed |= check(mutation != NULL, "find Types.Field slices"); if (mutation) { failed |= expect(mutation, mutated_length, ORLIX_TCTI_FEATURE_UNSUPPORTED_GRAMMAR, "non-null field slices"); free(mutation); }
	mutation = replace_once(source, length, "\"instance\": null", "\"instancE\": null", &mutated_length);
	failed |= check(mutation != NULL, "construct missing Types.Field instance"); if (mutation) { failed |= expect(mutation, mutated_length, ORLIX_TCTI_FEATURE_UNSUPPORTED_GRAMMAR, "missing field instance"); free(mutation); }
	mutation = replace_once(source, length, "\"_type\": \"AST.Bool\"", "\"_type\": \"AST.Bool\", \"\\u005ftype\": \"AST.Bool\"", &mutated_length);
	failed |= check(mutation != NULL, "construct duplicate key"); if (mutation) { failed |= expect(mutation, mutated_length, ORLIX_TCTI_FEATURE_DUPLICATE_KEY, "decoded duplicate key"); free(mutation); }
	mutation = replace_once(source, length, "true", "tru", &mutated_length);
	failed |= check(mutation != NULL, "find malformed string"); if (mutation) { failed |= expect(mutation, mutated_length, ORLIX_TCTI_FEATURE_INVALID_JSON, "malformed JSON"); free(mutation); }
	mutation = replace_once(source, length, "\"vFATAp1-A\"", "\"vFATAp1-B\"", &mutated_length);
	failed |= check(mutation != NULL, "find metadata pin"); if (mutation) { failed |= expect(mutation, mutated_length, ORLIX_TCTI_FEATURE_PIN_MISMATCH, "metadata pin"); free(mutation); }
	mutation = replace_once(source, length, "\"Features\"", "\"Features\" ", &mutated_length);
	failed |= check(mutation != NULL, "construct content pin mutation"); if (mutation) { failed |= expect(mutation, mutated_length, ORLIX_TCTI_FEATURE_PIN_MISMATCH, "SHA-256 pin"); free(mutation); }
	free(source);
	return failed;
}
