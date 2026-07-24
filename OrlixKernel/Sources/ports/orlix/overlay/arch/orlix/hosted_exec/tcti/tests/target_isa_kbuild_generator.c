/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Kbuild host generator for the canonical AArch64 target ledgers.
 *
 * This program consumes C .def inputs compiled into the host tool. It has no
 * JSON parser, source path, runtime profile, or HWCAP input.
 */
#include "target_isa_kbuild_generator.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

struct source_string_offsets {
	uint32_t name;
	uint32_t mnemonic;
	uint32_t operation;
	uint32_t condition;
};

struct classification_string_offsets {
	uint32_t name;
	uint32_t canonical;
	uint32_t evidence;
	uint32_t proof;
};

struct generated_offsets {
	uint32_t metadata[5];
	struct source_string_offsets *source;
	struct classification_string_offsets *classification;
	uint32_t pool_size;
};

static int fixed_text(const char *actual, const char *expected)
{
	return actual && !strcmp(actual, expected);
}

static int valid_sha256(const char *text)
{
	size_t index;

	if (!text || strlen(text) != 64U)
		return 0;
	for (index = 0; index < 64U; index++)
		if (!((text[index] >= '0' && text[index] <= '9') ||
		      (text[index] >= 'a' && text[index] <= 'f')))
			return 0;
	return 1;
}

static int valid_condition(const char *text)
{
	size_t index;
	size_t length;

	if (!text)
		return 0;
	length = strlen(text);
	if (length < 10U || (length & 1U) ||
	    strncmp(text, "54434e44", 8U))
		return 0;
	for (index = 0; index < length; index++)
		if (!((text[index] >= '0' && text[index] <= '9') ||
		      (text[index] >= 'a' && text[index] <= 'f')))
			return 0;
	return 1;
}

static enum tcti_a64_kbuild_generator_error validate_inputs(
	const struct tcti_a64_kbuild_metadata *metadata,
	const struct tcti_a64_kbuild_source_row *source,
	size_t source_count,
	const struct tcti_a64_kbuild_classification_row *classification,
	size_t classification_count)
{
	size_t index;
	size_t prior;

	if (!metadata || !source || !classification)
		return TCTI_A64_KBUILD_GENERATOR_INVALID_ARGUMENT;
	if (!fixed_text(metadata->architecture,
			TCTI_A64_KBUILD_ARCHITECTURE) ||
	    !fixed_text(metadata->build, TCTI_A64_KBUILD_BUILD) ||
	    !fixed_text(metadata->reference, TCTI_A64_KBUILD_REFERENCE) ||
	    !fixed_text(metadata->schema, TCTI_A64_KBUILD_SCHEMA) ||
	    !fixed_text(metadata->instructions_sha256,
			TCTI_A64_KBUILD_INSTRUCTIONS_SHA256) ||
	    !valid_sha256(metadata->instructions_sha256) ||
	    metadata->source_count != TCTI_A64_KBUILD_SOURCE_COUNT)
		return TCTI_A64_KBUILD_GENERATOR_METADATA_MISMATCH;
	if (source_count != TCTI_A64_KBUILD_SOURCE_COUNT ||
	    classification_count != TCTI_A64_KBUILD_SOURCE_COUNT)
		return TCTI_A64_KBUILD_GENERATOR_COUNT_MISMATCH;
	for (index = 0; index < source_count; index++) {
		const struct tcti_a64_kbuild_source_row *source_row =
			&source[index];
		const struct tcti_a64_kbuild_classification_row *class_row =
			&classification[index];

		if (source_row->ordinal != index)
			return TCTI_A64_KBUILD_GENERATOR_BAD_ORDINAL;
		if (!source_row->name || !source_row->name[0] ||
		    !source_row->mnemonic || !source_row->mnemonic[0] ||
		    !source_row->operation || !source_row->operation[0] ||
		    !valid_condition(source_row->condition_tcnd_hex) ||
		    (source_row->pattern & ~source_row->mask))
			return TCTI_A64_KBUILD_GENERATOR_BAD_SOURCE_ROW;
		if (!class_row->name ||
		    strcmp(source_row->name, class_row->name))
			return TCTI_A64_KBUILD_GENERATOR_CLASSIFICATION_MISMATCH;
		if (class_row->classification < TCTI_A64_KBUILD_UNCLASSIFIED ||
		    class_row->classification >
		    TCTI_A64_KBUILD_ALIAS_OR_DUPLICATE ||
		    class_row->relation < TCTI_A64_KBUILD_RELATION_NONE ||
		    class_row->relation > TCTI_A64_KBUILD_RELATION_DUPLICATE ||
		    !class_row->canonical || !class_row->evidence ||
		    !class_row->proof)
			return TCTI_A64_KBUILD_GENERATOR_BAD_CLASSIFICATION;
		if (class_row->relation == TCTI_A64_KBUILD_RELATION_NONE &&
		    class_row->canonical[0])
			return TCTI_A64_KBUILD_GENERATOR_BAD_CLASSIFICATION;
		if (class_row->relation != TCTI_A64_KBUILD_RELATION_NONE &&
		    !class_row->canonical[0])
			return TCTI_A64_KBUILD_GENERATOR_BAD_CLASSIFICATION;
		if ((class_row->classification ==
		     TCTI_A64_KBUILD_ALIAS_OR_DUPLICATE) !=
		    (class_row->relation != TCTI_A64_KBUILD_RELATION_NONE))
			return TCTI_A64_KBUILD_GENERATOR_BAD_CLASSIFICATION;
		if (class_row->relation != TCTI_A64_KBUILD_RELATION_NONE) {
			int found = 0;

			if (!strcmp(class_row->name, class_row->canonical))
				return TCTI_A64_KBUILD_GENERATOR_BAD_CLASSIFICATION;
			for (prior = 0; prior < source_count; prior++)
				if (!strcmp(source[prior].name,
					    class_row->canonical)) {
					found = 1;
					break;
				}
			if (!found)
				return TCTI_A64_KBUILD_GENERATOR_BAD_CLASSIFICATION;
		}
		if (class_row->classification == TCTI_A64_KBUILD_UNCLASSIFIED &&
		    (class_row->relation != TCTI_A64_KBUILD_RELATION_NONE ||
		     class_row->canonical[0] || class_row->evidence[0] ||
		     class_row->proof[0]))
			return TCTI_A64_KBUILD_GENERATOR_BAD_CLASSIFICATION;
		for (prior = 0; prior < index; prior++)
			if (!strcmp(source[prior].name, source_row->name))
				return TCTI_A64_KBUILD_GENERATOR_DUPLICATE_SOURCE;
	}
	return TCTI_A64_KBUILD_GENERATOR_OK;
}

static int add_string_offset(uint64_t *pool_size, const char *text,
			     uint32_t *offset)
{
	size_t length = strlen(text) + 1U;

	if (*pool_size > UINT32_MAX ||
	    length > UINT32_MAX - *pool_size)
		return -1;
	*offset = (uint32_t)*pool_size;
	*pool_size += length;
	return 0;
}

static int build_offsets(
	const struct tcti_a64_kbuild_metadata *metadata,
	const struct tcti_a64_kbuild_source_row *source,
	size_t source_count,
	const struct tcti_a64_kbuild_classification_row *classification,
	struct generated_offsets *offsets)
{
	const char *const metadata_text[] = {
		metadata->architecture, metadata->build, metadata->reference,
		metadata->schema, metadata->instructions_sha256,
	};
	uint64_t pool_size = 0;
	size_t index;

	offsets->source = calloc(source_count, sizeof(*offsets->source));
	offsets->classification =
		calloc(source_count, sizeof(*offsets->classification));
	if (!offsets->source || !offsets->classification)
		return -1;
	for (index = 0; index < sizeof(metadata_text) / sizeof(metadata_text[0]);
	     index++)
		if (add_string_offset(&pool_size, metadata_text[index],
				      &offsets->metadata[index]))
			return -1;
	for (index = 0; index < source_count; index++)
		if (add_string_offset(&pool_size, source[index].name,
				      &offsets->source[index].name) ||
		    add_string_offset(&pool_size, source[index].mnemonic,
				      &offsets->source[index].mnemonic) ||
		    add_string_offset(&pool_size, source[index].operation,
				      &offsets->source[index].operation) ||
		    add_string_offset(&pool_size, source[index].condition_tcnd_hex,
				      &offsets->source[index].condition))
			return -1;
	for (index = 0; index < source_count; index++)
		if (add_string_offset(&pool_size, classification[index].name,
				      &offsets->classification[index].name) ||
		    add_string_offset(&pool_size, classification[index].canonical,
				      &offsets->classification[index].canonical) ||
		    add_string_offset(&pool_size, classification[index].evidence,
				      &offsets->classification[index].evidence) ||
		    add_string_offset(&pool_size, classification[index].proof,
				      &offsets->classification[index].proof))
			return -1;
	offsets->pool_size = (uint32_t)pool_size;
	return 0;
}

static int emit_pool_byte(FILE *output, unsigned int byte, size_t *column)
{
	if (!*column && fputs("\t", output) == EOF)
		return -1;
	if (fprintf(output, "0x%02xU,", byte) < 0)
		return -1;
	(*column)++;
	if (*column == 12U) {
		if (fputc('\n', output) == EOF)
			return -1;
		*column = 0;
	} else if (fputc(' ', output) == EOF) {
		return -1;
	}
	return 0;
}

static int emit_pool_text(FILE *output, const char *text, size_t *column)
{
	const unsigned char *cursor = (const unsigned char *)text;

	do {
		if (emit_pool_byte(output, *cursor, column))
			return -1;
	} while (*cursor++);
	return 0;
}

static int emit_header(
	const struct tcti_a64_kbuild_metadata *metadata,
	const struct tcti_a64_kbuild_source_row *source,
	size_t source_count,
	const struct tcti_a64_kbuild_classification_row *classification,
	const struct generated_offsets *offsets,
	FILE *output)
{
	const char *const metadata_text[] = {
		metadata->architecture, metadata->build, metadata->reference,
		metadata->schema, metadata->instructions_sha256,
	};
	size_t unclassified_count = 0;
	size_t index;
	size_t column = 0;

	for (index = 0; index < source_count; index++)
		if (classification[index].classification ==
		    TCTI_A64_KBUILD_UNCLASSIFIED)
			unclassified_count++;
	if (fputs("/* SPDX-License-Identifier: BSD-3-Clause */\n"
		  "/* Generated by target_isa_kbuild_generator.c. Do not edit. */\n"
		  "#ifndef ORLIX_TCTI_A64_TARGET_GENERATED_H\n"
		  "#define ORLIX_TCTI_A64_TARGET_GENERATED_H\n"
		  "#include <linux/types.h>\n"
		  "#define TCTI_A64_GENERATED_ARCHITECTURE \"vFATAp1-A\"\n"
		  "#define TCTI_A64_GENERATED_BUILD \"818\"\n"
		  "#define TCTI_A64_GENERATED_REFERENCE \"2026-06_rel\"\n"
		  "#define TCTI_A64_GENERATED_SCHEMA \"2.9.5\"\n"
		  "#define TCTI_A64_GENERATED_INSTRUCTIONS_SHA256 "
		  "\"a1ad2c6538a47cd97d8762791ac5af88bce1d5f6aff096c9b77aef853e76acfe\"\n"
		  "#define TCTI_A64_GENERATED_SOURCE_COUNT 4350U\n",
		  output) == EOF ||
	    fprintf(output,
		    "#define TCTI_A64_GENERATED_UNCLASSIFIED_COUNT %zuU\n"
		    "#define TCTI_A64_GENERATED_CLASSIFICATION_COMPLETE %uU\n"
		    "/* ISA completion requires separate source-bound semantic proof. */\n"
		    "#define TCTI_A64_GENERATED_ISA_COMPLETE 0U\n"
		    "#define TCTI_A64_GENERATED_STRING_POOL_SIZE %" PRIu32 "U\n",
		    unclassified_count, unclassified_count ? 0U : 1U,
		    offsets->pool_size) < 0 ||
	    fputs("struct tcti_a64_generated_metadata {\n"
		  "\tu32 architecture; u32 build; u32 reference; u32 schema;\n"
		  "\tu32 instructions_sha256; u32 source_count;\n"
		  "};\n"
		  "struct tcti_a64_generated_row {\n"
		  "\tu32 ordinal; u32 name; u32 mnemonic; u32 operation;\n"
		  "\tu32 mask; u32 pattern; u32 condition;\n"
		  "\tu32 classification_name; u32 canonical; u32 evidence; u32 proof;\n"
		  "\tu8 classification; u8 relation; u8 reserved[2];\n"
		  "};\n"
		  "static const u8 tcti_a64_generated_strings[] = {\n",
		  output) == EOF)
		return -1;
	for (index = 0; index < sizeof(metadata_text) / sizeof(metadata_text[0]);
	     index++)
		if (emit_pool_text(output, metadata_text[index], &column))
			return -1;
	for (index = 0; index < source_count; index++)
		if (emit_pool_text(output, source[index].name, &column) ||
		    emit_pool_text(output, source[index].mnemonic, &column) ||
		    emit_pool_text(output, source[index].operation, &column) ||
		    emit_pool_text(output, source[index].condition_tcnd_hex, &column))
			return -1;
	for (index = 0; index < source_count; index++)
		if (emit_pool_text(output, classification[index].name, &column) ||
		    emit_pool_text(output, classification[index].canonical, &column) ||
		    emit_pool_text(output, classification[index].evidence, &column) ||
		    emit_pool_text(output, classification[index].proof, &column))
			return -1;
	if (column && fputc('\n', output) == EOF)
		return -1;
	if (fprintf(output,
		    "};\n"
		    "static const struct tcti_a64_generated_metadata "
		    "tcti_a64_generated_metadata = {\n"
		    "\t%" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %"
		    PRIu32 "U, %" PRIu32 "U, 4350U,\n};\n"
		    "static const struct tcti_a64_generated_row "
		    "tcti_a64_generated_rows[4350] = {\n",
		    offsets->metadata[0], offsets->metadata[1],
		    offsets->metadata[2], offsets->metadata[3],
		    offsets->metadata[4]) < 0)
		return -1;
	for (index = 0; index < source_count; index++) {
		const struct source_string_offsets *source_strings =
			&offsets->source[index];
		const struct classification_string_offsets *class_strings =
			&offsets->classification[index];

		if (fprintf(output,
			    "\t{ %" PRIu32 "U, %" PRIu32 "U, %" PRIu32
			    "U, %" PRIu32 "U, 0x%08" PRIx32
			    "U, 0x%08" PRIx32 "U, %" PRIu32 "U, %"
			    PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %"
			    PRIu32 "U, %uU, %uU, { 0U, 0U } },\n",
			    source[index].ordinal, source_strings->name,
			    source_strings->mnemonic, source_strings->operation,
			    source[index].mask, source[index].pattern,
			    source_strings->condition, class_strings->name,
			    class_strings->canonical, class_strings->evidence,
			    class_strings->proof,
			    (unsigned int)classification[index].classification,
			    (unsigned int)classification[index].relation) < 0)
			return -1;
	}
	return fputs("};\n"
		     "#endif /* ORLIX_TCTI_A64_TARGET_GENERATED_H */\n",
		     output) == EOF || ferror(output) ? -1 : 0;
}

enum tcti_a64_kbuild_generator_error tcti_a64_kbuild_generate_header(
	const struct tcti_a64_kbuild_metadata *metadata,
	const struct tcti_a64_kbuild_source_row *source,
	size_t source_count,
	const struct tcti_a64_kbuild_classification_row *classification,
	size_t classification_count,
	FILE *output)
{
	struct generated_offsets offsets = { 0 };
	enum tcti_a64_kbuild_generator_error error;

	if (!output)
		return TCTI_A64_KBUILD_GENERATOR_INVALID_ARGUMENT;
	error = validate_inputs(metadata, source, source_count, classification,
				classification_count);
	if (error != TCTI_A64_KBUILD_GENERATOR_OK)
		return error;
	if (build_offsets(metadata, source, source_count, classification,
			  &offsets)) {
		free(offsets.source);
		free(offsets.classification);
		return TCTI_A64_KBUILD_GENERATOR_NO_MEMORY;
	}
	error = emit_header(metadata, source, source_count, classification,
			    &offsets, output) ?
		TCTI_A64_KBUILD_GENERATOR_IO : TCTI_A64_KBUILD_GENERATOR_OK;
	free(offsets.source);
	free(offsets.classification);
	return error;
}

const char *tcti_a64_kbuild_generator_error_name(
	enum tcti_a64_kbuild_generator_error error)
{
	switch (error) {
	case TCTI_A64_KBUILD_GENERATOR_OK:
		return "success";
	case TCTI_A64_KBUILD_GENERATOR_INVALID_ARGUMENT:
		return "invalid argument";
	case TCTI_A64_KBUILD_GENERATOR_METADATA_MISMATCH:
		return "metadata mismatch";
	case TCTI_A64_KBUILD_GENERATOR_COUNT_MISMATCH:
		return "count mismatch";
	case TCTI_A64_KBUILD_GENERATOR_BAD_ORDINAL:
		return "bad ordinal";
	case TCTI_A64_KBUILD_GENERATOR_BAD_SOURCE_ROW:
		return "bad source row";
	case TCTI_A64_KBUILD_GENERATOR_DUPLICATE_SOURCE:
		return "duplicate source row";
	case TCTI_A64_KBUILD_GENERATOR_CLASSIFICATION_MISMATCH:
		return "classification identity mismatch";
	case TCTI_A64_KBUILD_GENERATOR_BAD_CLASSIFICATION:
		return "bad classification";
	case TCTI_A64_KBUILD_GENERATOR_NO_MEMORY:
		return "out of memory";
	case TCTI_A64_KBUILD_GENERATOR_IO:
		return "I/O failure";
	}
	return "unknown failure";
}

#ifndef TCTI_A64_KBUILD_GENERATOR_NO_BUILTIN_INPUT
#define TCTI_A64_SOURCE_MANIFEST_SOURCE(architecture, build, reference, schema, \
					sha256, count) \
	static const struct tcti_a64_kbuild_metadata builtin_metadata = { \
		architecture, build, reference, schema, sha256, count \
	};
#define TCTI_A64_SOURCE_MANIFEST_ROW(...)
#include "../isa/source_manifest.def"
#undef TCTI_A64_SOURCE_MANIFEST_ROW
#undef TCTI_A64_SOURCE_MANIFEST_SOURCE

#define TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, name, mnemonic, operation, mask, \
				     pattern, condition) \
	{ ordinal, name, mnemonic, operation, mask, pattern, condition },
static const struct tcti_a64_kbuild_source_row builtin_source[] = {
#include "../isa/source_manifest.def"
};
#undef TCTI_A64_SOURCE_MANIFEST_ROW
#undef TCTI_A64_SOURCE_MANIFEST_SOURCE

#define TCTI_A64_KBUILD_CLASS_UNCLASSIFIED TCTI_A64_KBUILD_UNCLASSIFIED
#define TCTI_A64_KBUILD_CLASS_REQUIRED TCTI_A64_KBUILD_REQUIRED
#define TCTI_A64_KBUILD_CLASS_NON_EL0 TCTI_A64_KBUILD_NON_EL0
#define TCTI_A64_KBUILD_CLASS_ARCHITECTURALLY_UNDEFINED \
	TCTI_A64_KBUILD_ARCHITECTURALLY_UNDEFINED
#define TCTI_A64_KBUILD_CLASS_ALIAS_OR_DUPLICATE \
	TCTI_A64_KBUILD_ALIAS_OR_DUPLICATE
#define TCTI_A64_KBUILD_REL_TCTI_A64_TARGET_RELATION_NONE \
	TCTI_A64_KBUILD_RELATION_NONE
#define TCTI_A64_KBUILD_REL_TCTI_A64_TARGET_RELATION_ALIAS \
	TCTI_A64_KBUILD_RELATION_ALIAS
#define TCTI_A64_KBUILD_REL_TCTI_A64_TARGET_RELATION_DUPLICATE \
	TCTI_A64_KBUILD_RELATION_DUPLICATE
#define TCTI_A64_KBUILD_STRING_(value) #value
#define TCTI_A64_KBUILD_STRING(value) TCTI_A64_KBUILD_STRING_(value)
#define TCTI_A64_KBUILD_CLASS_(value) TCTI_A64_KBUILD_CLASS_##value
#define TCTI_A64_KBUILD_CLASS(value) TCTI_A64_KBUILD_CLASS_(value)
#define TCTI_A64_KBUILD_REL_(value) TCTI_A64_KBUILD_REL_##value
#define TCTI_A64_KBUILD_REL(value) TCTI_A64_KBUILD_REL_(value)
#define TCTI_A64_TARGET_CLASSIFICATION(name, classification, relation, \
				       canonical, evidence, proof) \
	{ TCTI_A64_KBUILD_STRING(name), TCTI_A64_KBUILD_CLASS(classification), \
	  TCTI_A64_KBUILD_REL(relation), canonical, evidence, proof },
static const struct tcti_a64_kbuild_classification_row
builtin_classification[] = {
#include "../isa/target_classification.def"
};
#undef TCTI_A64_TARGET_CLASSIFICATION

#ifndef TCTI_A64_KBUILD_GENERATOR_NO_MAIN
int main(int argc, char **argv)
{
	enum tcti_a64_kbuild_generator_error error;

	(void)argv;
	if (argc != 1) {
		fprintf(stderr, "usage: target_isa_kbuild_generator > target.h\n");
		return EXIT_FAILURE;
	}
	error = tcti_a64_kbuild_generate_header(
		&builtin_metadata, builtin_source,
		sizeof(builtin_source) / sizeof(builtin_source[0]),
		builtin_classification,
		sizeof(builtin_classification) / sizeof(builtin_classification[0]),
		stdout);
	if (error != TCTI_A64_KBUILD_GENERATOR_OK) {
		fprintf(stderr, "target ISA Kbuild generator: %s\n",
			tcti_a64_kbuild_generator_error_name(error));
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
#endif
#endif
