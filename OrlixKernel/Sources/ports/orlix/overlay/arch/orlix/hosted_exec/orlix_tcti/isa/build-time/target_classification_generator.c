// SPDX-License-Identifier: GPL-2.0-only
/* Generate the immutable classification artifact from its reviewed C input. */
#define _POSIX_C_SOURCE 200809L
#include "target_classification_generator.h"

#include "../../tests/target_proof_registry.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ORLIX_TCTI_TARGET_CLASSIFICATION_ROWS 4350U

enum classification {
	CLASS_UNCLASSIFIED,
	CLASS_REQUIRED_EL0,
	CLASS_NON_EL0,
	CLASS_ARCH_UNDEFINED,
	CLASS_ALIAS_OR_DUPLICATE,
};

struct source_row {
	uint32_t ordinal;
	const char *name;
	const char *mnemonic;
	const char *operation;
	uint32_t mask;
	uint32_t pattern;
	const char *condition;
};

struct classification_row {
	const char *name;
	enum classification classification;
	int relation;
	const char *canonical;
	const char *evidence;
	const char *proof;
};

#define ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, name, mnemonic, operation, \
						     mask, pattern, condition, ...) \
	{ ordinal, name, mnemonic, operation, mask, pattern, condition },
static const struct source_row source_rows[] = {
#include "../generations/current/source_manifest.def"
};
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE

#define UNCLASSIFIED CLASS_UNCLASSIFIED
#define REQUIRED CLASS_REQUIRED_EL0
#define REQUIRED_EL0 CLASS_REQUIRED_EL0
#define NON_EL0 CLASS_NON_EL0
#define ARCHITECTURALLY_UNDEFINED CLASS_ARCH_UNDEFINED
#define ARCH_UNDEFINED_OR_UNALLOCATED CLASS_ARCH_UNDEFINED
#define ALIAS_OR_DUPLICATE CLASS_ALIAS_OR_DUPLICATE
#define ORLIX_TCTI_A64_TARGET_RELATION_NONE 0
#define ORLIX_TCTI_A64_TARGET_RELATION_ALIAS 1
#define ORLIX_TCTI_A64_TARGET_RELATION_DUPLICATE 2
#define ORLIX_TCTI_A64_TARGET_CLASSIFICATION(name, classification, relation, canonical, \
						 evidence, proof) \
	{ #name, classification, relation, canonical, evidence, proof },
static const struct classification_row classification_rows[] = {
#include "../target_classification_input.def"
};
#undef ORLIX_TCTI_A64_TARGET_CLASSIFICATION
#undef ORLIX_TCTI_A64_TARGET_RELATION_DUPLICATE
#undef ORLIX_TCTI_A64_TARGET_RELATION_ALIAS
#undef ORLIX_TCTI_A64_TARGET_RELATION_NONE
#undef ALIAS_OR_DUPLICATE
#undef ARCH_UNDEFINED_OR_UNALLOCATED
#undef ARCHITECTURALLY_UNDEFINED
#undef NON_EL0
#undef REQUIRED_EL0
#undef REQUIRED
#undef UNCLASSIFIED

static int empty(const char *text)
{
	return !text || !text[0];
}

static size_t find_classification(const char *name)
{
	size_t index;

	for (index = 0; index < sizeof(classification_rows) / sizeof(classification_rows[0]); index++)
		if (!strcmp(classification_rows[index].name, name))
			return index;
	return sizeof(classification_rows) / sizeof(classification_rows[0]);
}

static enum orlix_tcti_target_classification_error validate_input(void)
{
	const struct orlix_tcti_target_proof_registry_entry *registry;
	size_t registry_count;
	size_t index;

	if (sizeof(source_rows) / sizeof(source_rows[0]) != ORLIX_TCTI_TARGET_CLASSIFICATION_ROWS ||
	    sizeof(classification_rows) / sizeof(classification_rows[0]) !=
		ORLIX_TCTI_TARGET_CLASSIFICATION_ROWS) {
		return ORLIX_TCTI_TARGET_CLASSIFICATION_ROW_COUNT;
	}
	registry = orlix_tcti_target_proof_registry_entries(&registry_count);
	if (!registry) {
		return ORLIX_TCTI_TARGET_CLASSIFICATION_PROOF_REGISTRY;
	}
	for (index = 0; index < ORLIX_TCTI_TARGET_CLASSIFICATION_ROWS; index++) {
		const struct classification_row *row = &classification_rows[index];
		const struct source_row *source = &source_rows[index];
		size_t proof_source = index;
		size_t prior;

		if (source->ordinal != index || empty(source->name) || empty(row->name) ||
		    strcmp(source->name, row->name)) {
			return ORLIX_TCTI_TARGET_CLASSIFICATION_SOURCE_ROW;
		}
		for (prior = 0; prior < index; prior++)
			if (!strcmp(row->name, classification_rows[prior].name)) {
				return ORLIX_TCTI_TARGET_CLASSIFICATION_DUPLICATE_NAME;
			}
		if (row->classification > CLASS_ALIAS_OR_DUPLICATE ||
		    empty(row->evidence) != empty(row->proof)) {
			return ORLIX_TCTI_TARGET_CLASSIFICATION_INVALID_ENTRY;
		}
		if (row->classification == CLASS_ALIAS_OR_DUPLICATE) {
			proof_source = find_classification(row->canonical);
			if (proof_source == ORLIX_TCTI_TARGET_CLASSIFICATION_ROWS ||
			    classification_rows[proof_source].classification == CLASS_ALIAS_OR_DUPLICATE) {
				return ORLIX_TCTI_TARGET_CLASSIFICATION_INVALID_CANONICAL;
			}
		}
		/* Blank evidence and proof form an explicit audit blocker. Provenance
		 * is checked only after the reviewed ledger supplies both fields. */
		if (empty(row->proof))
			continue;
		if (orlix_tcti_target_proof_registry_lookup(registry, registry_count,
				&(const struct orlix_tcti_target_proof_reference) {
					.id = row->proof,
					.leaf_name = source_rows[proof_source].name,
					.mnemonic = source_rows[proof_source].mnemonic,
					.operation_id = source_rows[proof_source].operation,
					.encoding_mask = source_rows[proof_source].mask,
					.encoding_pattern = source_rows[proof_source].pattern,
					.condition_tcnd_hex = source_rows[proof_source].condition,
					.classification = classification_rows[proof_source].classification,
				}) != ORLIX_TCTI_TARGET_PROOF_REGISTRY_OK) {
			return ORLIX_TCTI_TARGET_CLASSIFICATION_PROOF_REFERENCE;
		}
	}
	return ORLIX_TCTI_TARGET_CLASSIFICATION_OK;
}

const char *orlix_tcti_target_classification_error_name(
	enum orlix_tcti_target_classification_error error)
{
	switch (error) {
	case ORLIX_TCTI_TARGET_CLASSIFICATION_OK: return "success";
	case ORLIX_TCTI_TARGET_CLASSIFICATION_INVALID_ARGUMENT: return "invalid argument";
	case ORLIX_TCTI_TARGET_CLASSIFICATION_ROW_COUNT: return "row count mismatch";
	case ORLIX_TCTI_TARGET_CLASSIFICATION_PROOF_REGISTRY: return "proof registry unavailable";
	case ORLIX_TCTI_TARGET_CLASSIFICATION_SOURCE_ROW: return "source row mismatch";
	case ORLIX_TCTI_TARGET_CLASSIFICATION_DUPLICATE_NAME: return "duplicate classification name";
	case ORLIX_TCTI_TARGET_CLASSIFICATION_INVALID_ENTRY: return "invalid classified entry";
	case ORLIX_TCTI_TARGET_CLASSIFICATION_INVALID_CANONICAL: return "invalid alias canonical";
	case ORLIX_TCTI_TARGET_CLASSIFICATION_PROOF_REFERENCE: return "proof reference mismatch";
	case ORLIX_TCTI_TARGET_CLASSIFICATION_OUTPUT: return "output generation failed";
	}
	return "unknown classification failure";
}

int orlix_tcti_target_classification_generate(int canonical_root_fd,
						      struct orlix_tcti_target_classification_output *output)
{
	FILE *stream;
	size_t index;

	if (!output)
		return -1;
	*output = (struct orlix_tcti_target_classification_output) { 0 };
	if (canonical_root_fd < 0) {
		output->error = ORLIX_TCTI_TARGET_CLASSIFICATION_INVALID_ARGUMENT;
		return -1;
	}
	output->error = validate_input();
	if (output->error) {
		return -1;
	}
	stream = open_memstream(&output->data, &output->length);
	if (!stream)
		return -1;
	if (fputs("/* SPDX-License-Identifier: GPL-2.0-only */\n"
		  "/*\n"
		  " * Generated by target_classification_generator.c. Do not edit.\n"
		  " * Arm AARCHMRS vFATAp1-A build 818, 2026-06_rel, 4,350 leaves.\n"
		  " * Blank evidence and proof_id fields are deliberate audit blockers.\n"
		  " */\n", stream) == EOF)
		goto fail;
	for (index = 0; index < ORLIX_TCTI_TARGET_CLASSIFICATION_ROWS; index++) {
		const struct classification_row *row = &classification_rows[index];
		static const char *const classifications[] = {
			"UNCLASSIFIED", "REQUIRED", "NON_EL0",
			"ARCHITECTURALLY_UNDEFINED", "ALIAS_OR_DUPLICATE",
		};
		static const char *const relations[] = {
			"ORLIX_TCTI_A64_TARGET_RELATION_NONE",
			"ORLIX_TCTI_A64_TARGET_RELATION_ALIAS",
			"ORLIX_TCTI_A64_TARGET_RELATION_DUPLICATE",
		};

		if (row->relation < 0 || row->relation >= (int)(sizeof(relations) / sizeof(relations[0])) ||
		    fprintf(stream, "ORLIX_TCTI_A64_TARGET_CLASSIFICATION(%s, %s, %s, \"%s\", \"%s\", \"%s\")\n",
			row->name, classifications[row->classification], relations[row->relation],
			row->canonical, row->evidence, row->proof) < 0)
			goto fail;
	}
	if (!fclose(stream))
		return 0;
fail:
	fclose(stream);
	free(output->data);
	output->data = NULL;
	output->length = 0;
	output->error = ORLIX_TCTI_TARGET_CLASSIFICATION_OUTPUT;
	return -1;
}

void orlix_tcti_target_classification_destroy(
	struct orlix_tcti_target_classification_output *output)
{
	if (!output)
		return;
	free(output->data);
	*output = (struct orlix_tcti_target_classification_output) { 0 };
}

int orlix_tcti_target_classification_matches(int canonical_root_fd,
	const void *data, size_t length)
{
	struct orlix_tcti_target_classification_output expected;
	int result;

	if (!data || orlix_tcti_target_classification_generate(canonical_root_fd,
								   &expected))
		return -1;
	result = length == expected.length && !memcmp(data, expected.data, length) ?
		0 : -1;
	orlix_tcti_target_classification_destroy(&expected);
	return result;
}

#ifndef TARGET_CLASSIFICATION_GENERATOR_NO_MAIN
int main(void)
{
	struct orlix_tcti_target_classification_output output;

	if (orlix_tcti_target_classification_generate(AT_FDCWD, &output))
		return EXIT_FAILURE;
	if (fwrite(output.data, 1, output.length, stdout) != output.length) {
		orlix_tcti_target_classification_destroy(&output);
		return EXIT_FAILURE;
	}
	orlix_tcti_target_classification_destroy(&output);
	return EXIT_SUCCESS;
}
#endif
