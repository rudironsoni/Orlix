// SPDX-License-Identifier: GPL-2.0-only
/*
 * Generate the KUnit-facing A64 inventory header from normalized Arm data.
 *
 * The Arm AARCHMRS archive is a maintenance input, not a kernel build
 * dependency. source_manifest.def and target_classification.def are the
 * complete reviewed target ledgers. inventory.def is only the narrow runtime
 * decoder projection, whose disposition is derived from those ledgers here.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum inventory_disposition {
	INVENTORY_UNCLASSIFIED,
	INVENTORY_REQUIRED,
	INVENTORY_NON_EL0,
	INVENTORY_ARCHITECTURALLY_UNDEFINED,
	INVENTORY_ALIAS_OR_DUPLICATE,
};

enum inventory_condition_kind {
	INVENTORY_CONDITION_RM_NOT_31,
	INVENTORY_CONDITION_IMMH_NOT_ZERO,
	INVENTORY_CONDITION_OPTION_NOT_3,
	INVENTORY_CONDITION_RT_BELOW_24,
	INVENTORY_CONDITION_SYSTEM_ENCODING,
};

struct inventory_entry {
	const char *name;
	const char *mnemonic;
	const char *operation_id;
	uint32_t mask;
	uint32_t pattern;
	uint32_t witness;
};

struct source_manifest_entry {
	const char *name;
	const char *mnemonic;
	const char *operation_id;
	uint32_t mask;
	uint32_t pattern;
};

struct classification_entry {
	const char *name;
	enum inventory_disposition disposition;
};

struct inventory_condition {
	const char *name;
	enum inventory_condition_kind kind;
};

#define stringify_1(value) #value
#define stringify(value) stringify_1(value)

#define TCTI_A64_PROFILE(major, minor, hwcap, hwcap2) \
	static const unsigned int inventory_arch_major = (major); \
	static const unsigned int inventory_arch_minor = (minor); \
	static const char inventory_hwcap[] = stringify(hwcap); \
	static const char inventory_hwcap2[] = stringify(hwcap2);
#define TCTI_A64_SOURCE(release, build, ref, schema, sha256, url) \
	static const char inventory_source_release[] = release; \
	static const char inventory_source_build[] = build; \
	static const char inventory_source_ref[] = ref; \
	static const char inventory_source_schema[] = schema; \
	static const char inventory_source_sha256[] = sha256; \
	static const char inventory_source_url[] = url;
#define TCTI_A64_FEATURE(...)
#define TCTI_A64_RUNTIME_ENCODING(name, mnemonic, operation_id, mask, \
			  pattern, witness)
#define TCTI_A64_CONDITION(name, kind)
#include "../isa/inventory.def"
#undef TCTI_A64_CONDITION
#undef TCTI_A64_RUNTIME_ENCODING
#undef TCTI_A64_FEATURE
#undef TCTI_A64_SOURCE
#undef TCTI_A64_PROFILE

#define REQUIRED INVENTORY_REQUIRED
#define NON_EL0 INVENTORY_NON_EL0
#define ARCHITECTURALLY_UNDEFINED INVENTORY_ARCHITECTURALLY_UNDEFINED
#define TCTI_A64_PROFILE(major, minor, hwcap, hwcap2)
#define TCTI_A64_SOURCE(release, build, ref, schema, sha256, url)
#define TCTI_A64_FEATURE(...)
#define TCTI_A64_CONDITION(name, kind)
#define TCTI_A64_RUNTIME_ENCODING(name, mnemonic, operation_id, mask, \
			  pattern, witness) \
	{ stringify(name), stringify(mnemonic), stringify(operation_id), \
	  (mask), (pattern), (witness) },

static const struct inventory_entry inventory[] = {
#include "../isa/inventory.def"
};
#undef TCTI_A64_RUNTIME_ENCODING
#undef TCTI_A64_CONDITION
#undef TCTI_A64_FEATURE
#undef TCTI_A64_SOURCE
#undef TCTI_A64_PROFILE
#undef ARCHITECTURALLY_UNDEFINED
#undef NON_EL0
#undef REQUIRED

#define RM_NOT_31 INVENTORY_CONDITION_RM_NOT_31
#define IMMH_NOT_ZERO INVENTORY_CONDITION_IMMH_NOT_ZERO
#define OPTION_NOT_3 INVENTORY_CONDITION_OPTION_NOT_3
#define RT_BELOW_24 INVENTORY_CONDITION_RT_BELOW_24
#define SYSTEM_ENCODING INVENTORY_CONDITION_SYSTEM_ENCODING
#define TCTI_A64_PROFILE(major, minor, hwcap, hwcap2)
#define TCTI_A64_SOURCE(release, build, ref, schema, sha256, url)
#define TCTI_A64_FEATURE(...)
#define TCTI_A64_RUNTIME_ENCODING(name, mnemonic, operation_id, mask, \
			  pattern, witness)
#define TCTI_A64_CONDITION(name, kind) { stringify(name), (kind) },

static const struct inventory_condition inventory_conditions[] = {
#include "../isa/inventory.def"
};
#undef TCTI_A64_CONDITION
#undef TCTI_A64_RUNTIME_ENCODING
#undef TCTI_A64_FEATURE
#undef TCTI_A64_SOURCE
#undef TCTI_A64_PROFILE
#undef SYSTEM_ENCODING
#undef RT_BELOW_24
#undef OPTION_NOT_3
#undef IMMH_NOT_ZERO
#undef RM_NOT_31

#define TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, name, mnemonic, operation_id, \
				     mask, pattern, condition, offset, length) \
	{ name, mnemonic, operation_id, (mask), (pattern) },
static const struct source_manifest_entry source_manifest[] = {
#include "../isa/source_manifest.def"
};
#undef TCTI_A64_SOURCE_MANIFEST_ROW
#undef TCTI_A64_SOURCE_MANIFEST_SOURCE

#define UNCLASSIFIED INVENTORY_UNCLASSIFIED
#define REQUIRED INVENTORY_REQUIRED
#define REQUIRED_EL0 INVENTORY_REQUIRED
#define NON_EL0 INVENTORY_NON_EL0
#define ARCHITECTURALLY_UNDEFINED INVENTORY_ARCHITECTURALLY_UNDEFINED
#define ARCH_UNDEFINED_OR_UNALLOCATED INVENTORY_ARCHITECTURALLY_UNDEFINED
#define ALIAS_OR_DUPLICATE INVENTORY_ALIAS_OR_DUPLICATE
#define TCTI_A64_TARGET_RELATION_NONE
#define TCTI_A64_TARGET_RELATION_ALIAS
#define TCTI_A64_TARGET_RELATION_DUPLICATE
#define TCTI_A64_TARGET_CLASSIFICATION(name, classification, relation, \
				       canonical, evidence, proof) \
	{ stringify(name), (classification) },
static const struct classification_entry target_classification[] = {
#include "../isa/target_classification.def"
};
#undef TCTI_A64_TARGET_CLASSIFICATION
#undef TCTI_A64_TARGET_RELATION_DUPLICATE
#undef TCTI_A64_TARGET_RELATION_ALIAS
#undef TCTI_A64_TARGET_RELATION_NONE
#undef ALIAS_OR_DUPLICATE
#undef ARCH_UNDEFINED_OR_UNALLOCATED
#undef ARCHITECTURALLY_UNDEFINED
#undef NON_EL0
#undef REQUIRED_EL0
#undef REQUIRED
#undef UNCLASSIFIED

static bool condition_holds(enum inventory_condition_kind kind,
			    uint32_t instruction)
{
	switch (kind) {
	case INVENTORY_CONDITION_RM_NOT_31:
		return ((instruction >> 16) & 0x1fU) != 31;
	case INVENTORY_CONDITION_IMMH_NOT_ZERO:
		return ((instruction >> 19) & 0xfU) != 0;
	case INVENTORY_CONDITION_OPTION_NOT_3:
		return ((instruction >> 13) & 0x7U) != 3;
	case INVENTORY_CONDITION_RT_BELOW_24:
		return (instruction & 0x1fU) < 24;
	case INVENTORY_CONDITION_SYSTEM_ENCODING: {
		uint32_t op1 = (instruction >> 16) & 0x7U;
		uint32_t op2 = (instruction >> 5) & 0x7U;

		return op1 != 0 || op2 > 2;
	}
	}

	return false;
}

static const struct inventory_entry *find_inventory_entry(const char *name)
{
	size_t i;

	for (i = 0; i < sizeof(inventory) / sizeof(inventory[0]); i++)
		if (!strcmp(name, inventory[i].name))
			return &inventory[i];

	return NULL;
}

static const struct source_manifest_entry *find_source_manifest_entry(
	const char *name)
{
	size_t i;

	for (i = 0; i < sizeof(source_manifest) / sizeof(source_manifest[0]); i++)
		if (!strcmp(name, source_manifest[i].name))
			return &source_manifest[i];
	return NULL;
}

static const struct classification_entry *find_target_classification(
	const char *name)
{
	size_t i;

	for (i = 0;
	     i < sizeof(target_classification) / sizeof(target_classification[0]);
	     i++)
		if (!strcmp(name, target_classification[i].name))
			return &target_classification[i];
	return NULL;
}

static bool inventory_entry_is_required(const struct inventory_entry *entry)
{
	const struct classification_entry *classification =
		find_target_classification(entry->name);

	return classification && classification->disposition == INVENTORY_REQUIRED;
}

static int validate_inventory(void)
{
	size_t i;

	if (!inventory_arch_major || inventory_arch_minor > 99 ||
	    !inventory_hwcap[0] || !inventory_hwcap2[0] ||
	    !inventory_source_release[0] || !inventory_source_build[0] ||
	    !inventory_source_ref[0] || !inventory_source_schema[0] ||
	    strlen(inventory_source_sha256) != 64 || !inventory_source_url[0]) {
		fprintf(stderr, "incomplete configured A64 metadata\n");
		return -1;
	}
	if (!sizeof(source_manifest) || !sizeof(target_classification)) {
		fprintf(stderr, "target ledgers are empty\n");
		return -1;
	}

	for (i = 0; i < sizeof(inventory) / sizeof(inventory[0]); i++) {
		const struct inventory_entry *entry = &inventory[i];
		const struct source_manifest_entry *source;
		const struct classification_entry *classification;
		size_t j;

		if (!entry->name[0] || !entry->mnemonic[0] ||
		    !entry->operation_id[0]) {
			fprintf(stderr, "inventory entry %zu has empty identity\n", i);
			return -1;
		}
		if (entry->pattern & ~entry->mask) {
			fprintf(stderr, "%s pattern exceeds its mask\n", entry->name);
			return -1;
		}
		if ((entry->witness & entry->mask) != entry->pattern) {
			fprintf(stderr, "%s witness violates its mask\n", entry->name);
			return -1;
		}
		source = find_source_manifest_entry(entry->name);
		classification = find_target_classification(entry->name);
		if (!source || !classification ||
		    strcmp(entry->mnemonic, source->mnemonic) ||
		    strcmp(entry->operation_id, source->operation_id) ||
		    entry->mask != source->mask || entry->pattern != source->pattern) {
			fprintf(stderr, "%s is not a source-derived runtime projection\n",
				entry->name);
			return -1;
		}
		for (j = 0; j < i; j++) {
			if (!strcmp(entry->name, inventory[j].name)) {
				fprintf(stderr, "duplicate inventory name: %s\n",
					entry->name);
				return -1;
			}
		}

	}

	for (i = 0;
	     i < sizeof(inventory_conditions) / sizeof(inventory_conditions[0]);
	     i++) {
		const struct inventory_condition *condition =
			&inventory_conditions[i];
		const struct inventory_entry *entry =
			find_inventory_entry(condition->name);
		size_t j;

		if (!entry) {
			fprintf(stderr, "condition references unknown entry: %s\n",
				condition->name);
			return -1;
		}
		if (!condition_holds(condition->kind, entry->witness)) {
			fprintf(stderr, "%s witness violates its condition\n",
				condition->name);
			return -1;
		}
		for (j = 0; j < i; j++) {
			if (!strcmp(condition->name,
				    inventory_conditions[j].name)) {
				fprintf(stderr, "duplicate condition: %s\n",
					condition->name);
				return -1;
			}
		}
	}

	return 0;
}

static void emit_entry(FILE *output, const struct inventory_entry *entry)
{
	fprintf(output,
		"\t{ \"%s\", \"%s\", \"%s\", "
		"0x%08xU, 0x%08xU, 0x%08xU },\n",
		entry->name, entry->mnemonic, entry->operation_id,
		entry->mask, entry->pattern, entry->witness);
}

static int emit_header(FILE *output)
{
	size_t i;

	fprintf(output,
		"/* SPDX-License-Identifier: GPL-2.0-only */\n"
		"/* Generated by Kbuild host tool tests/gen_inventory.c. */\n"
		"/* Arm AARCHMRS source data is BSD-3-Clause. */\n"
		"#ifndef ORLIX_TCTI_A64_INVENTORY_H\n"
		"#define ORLIX_TCTI_A64_INVENTORY_H\n\n"
		"#include <asm/isa.h>\n\n"
		"#define ORLIX_TCTI_A64_SOURCE_RELEASE \"%s\"\n"
		"#define ORLIX_TCTI_A64_SOURCE_BUILD \"%s\"\n"
		"#define ORLIX_TCTI_A64_SOURCE_REF \"%s\"\n"
		"#define ORLIX_TCTI_A64_SOURCE_SCHEMA \"%s\"\n"
		"#define ORLIX_TCTI_A64_INSTRUCTIONS_SHA256 \"%s\"\n"
		"#define ORLIX_TCTI_A64_SOURCE_URL \"%s\"\n\n"
		"static_assert(ORLIX_EL0_ARCH_MAJOR == %u);\n"
		"static_assert(ORLIX_EL0_ARCH_MINOR == %u);\n"
		"static_assert(ORLIX_EL0_HWCAP == %s);\n"
		"static_assert(ORLIX_EL0_HWCAP2 == %s);\n\n"
		"struct tcti_a64_encoding_inventory_entry {\n"
		"\tconst char *name;\n"
		"\tconst char *mnemonic;\n"
		"\tconst char *operation_id;\n"
		"\tu32 mask;\n"
		"\tu32 pattern;\n"
		"\tu32 witness;\n"
		"};\n\n"
		"enum tcti_a64_condition_kind {\n"
		"\tTCTI_A64_CONDITION_RM_NOT_31,\n"
		"\tTCTI_A64_CONDITION_IMMH_NOT_ZERO,\n"
		"\tTCTI_A64_CONDITION_OPTION_NOT_3,\n"
		"\tTCTI_A64_CONDITION_RT_BELOW_24,\n"
		"\tTCTI_A64_CONDITION_SYSTEM_ENCODING,\n"
		"};\n\n"
		"struct tcti_a64_condition_inventory_entry {\n"
		"\tconst char *name;\n"
		"\tenum tcti_a64_condition_kind kind;\n"
		"};\n\n",
		inventory_source_release, inventory_source_build,
		inventory_source_ref, inventory_source_schema,
		inventory_source_sha256, inventory_source_url,
		inventory_arch_major, inventory_arch_minor,
		inventory_hwcap, inventory_hwcap2);

	fprintf(output, "static const struct tcti_a64_encoding_inventory_entry\n"
		"tcti_a64_encoding_inventory[] = {\n");
	for (i = 0; i < sizeof(inventory) / sizeof(inventory[0]); i++)
		if (inventory_entry_is_required(&inventory[i]))
			emit_entry(output, &inventory[i]);
	fprintf(output, "};\n\n");

	fprintf(output, "static const struct tcti_a64_encoding_inventory_entry\n"
		"tcti_a64_unsupported_encoding_inventory[] = {\n");
	for (i = 0; i < sizeof(inventory) / sizeof(inventory[0]); i++)
		if (!inventory_entry_is_required(&inventory[i]))
			emit_entry(output, &inventory[i]);
	fprintf(output, "};\n\n");

	fprintf(output, "static const struct tcti_a64_condition_inventory_entry\n"
		"tcti_a64_condition_inventory[] = {\n");
	for (i = 0;
	     i < sizeof(inventory_conditions) / sizeof(inventory_conditions[0]);
	     i++)
		fprintf(output, "\t{ \"%s\", %u },\n",
			inventory_conditions[i].name,
			(unsigned int)inventory_conditions[i].kind);
	fprintf(output, "};\n\n#endif /* ORLIX_TCTI_A64_INVENTORY_H */\n");

	return ferror(output) ? -1 : 0;
}

int main(int argc, char **argv)
{
	char *temporary;
	FILE *output;
	int status = EXIT_FAILURE;

	if (argc != 2) {
		fprintf(stderr, "usage: %s OUTPUT\n", argv[0]);
		return EXIT_FAILURE;
	}
	if (validate_inventory())
		return EXIT_FAILURE;

	temporary = malloc(strlen(argv[1]) + 32);
	if (!temporary) {
		perror("malloc");
		return EXIT_FAILURE;
	}
	sprintf(temporary, "%s.tmp.%ld", argv[1], (long)getpid());

	output = fopen(temporary, "w");
	if (!output) {
		perror(temporary);
		goto out_free;
	}
	if (emit_header(output) || fflush(output) || fsync(fileno(output))) {
		perror("write inventory header");
		goto out_close;
	}
	if (fclose(output)) {
		output = NULL;
		perror("close inventory header");
		goto out_unlink;
	}
	output = NULL;
	if (rename(temporary, argv[1])) {
		perror("rename inventory header");
		goto out_unlink;
	}

	status = EXIT_SUCCESS;
	goto out_free;

out_close:
	fclose(output);
out_unlink:
	unlink(temporary);
out_free:
	free(temporary);
	return status;
}
