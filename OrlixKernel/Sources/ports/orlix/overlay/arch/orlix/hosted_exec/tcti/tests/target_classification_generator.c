// SPDX-License-Identifier: GPL-2.0-only
/*
 * Emit a one-time classification seed for every pinned Arm source leaf.
 *
 * This host maintenance tool deliberately imports the complete target before
 * overlaying the legacy inventory. It neither reads nor evaluates the runtime
 * HWCAP profile: ADR 0029 makes that projection irrelevant to target
 * completeness. The emitted rows are a migration starting point only. Review
 * the copied ledger to add its relation, evidence, and proof fields; this tool
 * does not validate or replace those reviewed fields.
 */
#include "target_inventory_import.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CURRENT_CLASSIFICATION_COUNT 1078U
#define REQUIRED_COUNT 1068U
#define NON_EL0_COUNT 9U
#define ARCH_UNDEFINED_COUNT 1U

enum classification {
	CLASS_UNCLASSIFIED,
	CLASS_REQUIRED,
	CLASS_NON_EL0,
	CLASS_ARCH_UNDEFINED,
};

struct current_classification {
	char *name;
	enum classification classification;
};

struct source_bytes {
	char *data;
	size_t length;
};

static int read_source(const char *path, struct source_bytes *source)
{
	FILE *file;
	long length;

	file = fopen(path, "rb");
	if (!file) {
		fprintf(stderr, "%s: %s\n", path, strerror(errno));
		return -1;
	}
	if (fseek(file, 0, SEEK_END) || (length = ftell(file)) < 0 ||
	    fseek(file, 0, SEEK_SET)) {
		fprintf(stderr, "%s: cannot determine source length\n", path);
		fclose(file);
		return -1;
	}
	if ((unsigned long)length > SIZE_MAX - 1) {
		fprintf(stderr, "%s: source too large\n", path);
		fclose(file);
		return -1;
	}
	source->data = malloc((size_t)length + 1);
	if (!source->data) {
		fprintf(stderr, "%s: cannot allocate source bytes\n", path);
		fclose(file);
		return -1;
	}
	if (fread(source->data, 1, (size_t)length, file) != (size_t)length ||
	    fclose(file)) {
		fprintf(stderr, "%s: cannot read source\n", path);
		free(source->data);
		source->data = NULL;
		return -1;
	}
	source->data[length] = '\0';
	source->length = (size_t)length;
	return 0;
}

static const char *skip_space(const char *cursor)
{
	while (*cursor && isspace((unsigned char)*cursor))
		cursor++;
	return cursor;
}

static int parse_classification(const char *text, size_t length,
				enum classification *classification)
{
	if (length == strlen("REQUIRED") && !memcmp(text, "REQUIRED", length))
		*classification = CLASS_REQUIRED;
	else if (length == strlen("NON_EL0") && !memcmp(text, "NON_EL0", length))
		*classification = CLASS_NON_EL0;
	else if (length == strlen("ARCHITECTURALLY_UNDEFINED") &&
		 !memcmp(text, "ARCHITECTURALLY_UNDEFINED", length))
		*classification = CLASS_ARCH_UNDEFINED;
	else
		return -1;
	return 0;
}

static int add_current_classification(struct current_classification *rows,
				      size_t *count, const char *name,
				      size_t name_length,
				      enum classification classification)
{
	size_t index;

	if (*count == CURRENT_CLASSIFICATION_COUNT)
		return -1;
	for (index = 0; index < *count; index++)
		if (strlen(rows[index].name) == name_length &&
		    !memcmp(rows[index].name, name, name_length))
			return -1;
	rows[*count].name = malloc(name_length + 1);
	if (!rows[*count].name)
		return -1;
	memcpy(rows[*count].name, name, name_length);
	rows[*count].name[name_length] = '\0';
	rows[*count].classification = classification;
	(*count)++;
	return 0;
}

static void destroy_current_classifications(struct current_classification *rows,
					    size_t count)
{
	size_t index;

	for (index = 0; index < count; index++)
		free(rows[index].name);
}

static int import_current_classifications(const struct source_bytes *source,
					  struct current_classification *rows,
					  size_t *count)
{
	static const char marker[] = "TCTI_A64_ENCODING(";
	const char *cursor = source->data;
	size_t required = 0;
	size_t non_el0 = 0;
	size_t arch_undefined = 0;

	*count = 0;
	while ((cursor = strstr(cursor, marker))) {
		const char *line_start = cursor;
		const char *class_start;
		const char *class_end;
		const char *name_start;
		const char *name_end;
		enum classification classification;

		while (line_start > source->data && line_start[-1] != '\n')
			line_start--;
		if (skip_space(line_start) != cursor) {
			cursor += sizeof(marker) - 1;
			continue;
		}
		cursor += sizeof(marker) - 1;
		class_start = skip_space(cursor);
		class_end = class_start;
		while (*class_end && *class_end != ',')
			class_end++;
		if (*class_end != ',' ||
		    parse_classification(class_start, (size_t)(class_end - class_start),
					 &classification))
			goto invalid;
		name_start = skip_space(class_end + 1);
		name_end = name_start;
		while (*name_end && *name_end != ',')
			name_end++;
		if (*name_end != ',' || name_end == name_start ||
		    add_current_classification(rows, count, name_start,
					       (size_t)(name_end - name_start),
					       classification))
			goto invalid;
		if (classification == CLASS_REQUIRED)
			required++;
		else if (classification == CLASS_NON_EL0)
			non_el0++;
		else
			arch_undefined++;
		cursor = name_end + 1;
	}
	if (*count == CURRENT_CLASSIFICATION_COUNT && required == REQUIRED_COUNT &&
	    non_el0 == NON_EL0_COUNT && arch_undefined == ARCH_UNDEFINED_COUNT)
		return 0;
invalid:
	fprintf(stderr, "inventory.def does not contain the pinned %u/%u/%u classification set\n",
		REQUIRED_COUNT, NON_EL0_COUNT, ARCH_UNDEFINED_COUNT);
	destroy_current_classifications(rows, *count);
	*count = 0;
	return -1;
}

static const struct current_classification *find_current_classification(
	const struct current_classification *rows, size_t count, const char *name)
{
	size_t index;

	for (index = 0; index < count; index++)
		if (!strcmp(rows[index].name, name))
			return &rows[index];
	return NULL;
}

static const char *classification_name(enum classification classification)
{
	switch (classification) {
	case CLASS_REQUIRED:
		return "REQUIRED";
	case CLASS_NON_EL0:
		return "NON_EL0";
	case CLASS_ARCH_UNDEFINED:
		return "ARCHITECTURALLY_UNDEFINED";
	case CLASS_UNCLASSIFIED:
		return "UNCLASSIFIED";
	}
	return NULL;
}

static int emit_ledger(FILE *output, const struct tcti_target_inventory *inventory,
			       const struct current_classification *current,
			       size_t current_count)
{
	size_t index;
	size_t counts[CLASS_ARCH_UNDEFINED + 1] = { 0 };

	fputs("/* SPDX-License-Identifier: GPL-2.0-only */\n"
	      "/*\n"
	      " * Bootstrap seed emitted by target_classification_generator.c.\n"
	      " * Copy this into a reviewed ledger before adding relation, evidence,\n"
	      " * and proof fields. This seed is not an authoritative ledger.\n"
	      " * Arm AARCHMRS vFATAp1-A build 818, 2026-06_rel, 4,350 leaves.\n"
	      " * Blank evidence and proof_id fields are deliberate audit blockers.\n"
	      " */\n", output);
	for (index = 0; index < inventory->leaf_count; index++) {
		const struct current_classification *row;
		enum classification classification = CLASS_UNCLASSIFIED;

		row = find_current_classification(current, current_count,
					  inventory->leaves[index].name);
		if (row)
			classification = row->classification;
		counts[classification]++;
		fputs("TCTI_A64_TARGET_CLASSIFICATION(", output);
		if (fputs(inventory->leaves[index].name, output) == EOF ||
		    fprintf(output, ", %s, TCTI_A64_TARGET_RELATION_NONE, \"\", \"\", \"\")\n",
			    classification_name(classification)) < 0)
			return -1;
	}
	if (counts[CLASS_REQUIRED] != REQUIRED_COUNT ||
	    counts[CLASS_NON_EL0] != NON_EL0_COUNT ||
	    counts[CLASS_ARCH_UNDEFINED] != ARCH_UNDEFINED_COUNT ||
	    counts[CLASS_UNCLASSIFIED] !=
		TCTI_A64_TARGET_LEAF_COUNT - CURRENT_CLASSIFICATION_COUNT) {
		fprintf(stderr, "inventory classifications do not match Arm source identities\n");
		return -1;
	}
	return ferror(output) ? -1 : 0;
}

int main(int argc, char **argv)
{
	struct source_bytes instructions = { 0 };
	struct source_bytes inventory_def = { 0 };
	struct tcti_target_inventory inventory = { 0 };
	struct tcti_target_import_error error = { 0 };
	struct current_classification current[CURRENT_CLASSIFICATION_COUNT] = { { 0 } };
	const char *instructions_path;
	const char *inventory_path;
	size_t current_count = 0;
	int status = EXIT_FAILURE;

	if (argc == 3) {
		instructions_path = argv[1];
		inventory_path = argv[2];
	} else {
		fprintf(stderr, "usage: %s Instructions.json inventory.def\n", argv[0]);
		return EXIT_FAILURE;
	}
	if (read_source(instructions_path, &instructions) ||
	    read_source(inventory_path, &inventory_def) ||
	    tcti_target_inventory_import(instructions.data, instructions.length,
					 &inventory, &error)) {
		if (error.code)
			fprintf(stderr, "import error %d at %zu: %s\n", error.code,
				error.offset, error.message);
		goto out;
	}
	if (inventory.leaf_count != TCTI_A64_TARGET_LEAF_COUNT ||
	    import_current_classifications(&inventory_def, current, &current_count) ||
	    emit_ledger(stdout, &inventory, current, current_count))
		goto out;
	status = EXIT_SUCCESS;
out:
	destroy_current_classifications(current, current_count);
	tcti_target_inventory_destroy(&inventory);
	free(instructions.data);
	free(inventory_def.data);
	return status;
}
