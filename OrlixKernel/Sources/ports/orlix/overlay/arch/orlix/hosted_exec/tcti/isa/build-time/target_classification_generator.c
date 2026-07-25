// SPDX-License-Identifier: GPL-2.0-only
/*
 * Emit a bootstrap classification seed from the pinned Arm source.
 *
 * The checked completion authority is source_manifest.def plus the reviewed
 * target_classification.def ledger.  This tool intentionally does not read
 * the legacy runtime projection, infer a disposition, or assert hard-coded
 * classification totals.  Its output is only a convenient, deliberately
 * blocking starting point when the pinned Arm source changes.
 */
#include "target_inventory_import.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum classification {
	CLASS_UNCLASSIFIED,
	CLASS_NON_EL0,
	CLASS_ARCH_UNDEFINED,
};

struct reviewed_classification_binding {
	const char *name;
	enum classification classification;
	const char *evidence;
	const char *proof_id;
};

struct source_manifest_row {
	const char *name;
	const char *mnemonic;
	const char *operation_id;
	uint32_t mask;
	uint32_t pattern;
};

struct source_manifest_provenance {
	const char *sha256;
	uint32_t source_length;
};

#define TCTI_A64_SOURCE_MANIFEST_SOURCE(architecture, build, release, schema, \
					 sha256, count, timestamp, length) \
	{ sha256, (length) }
#define TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, name, mnemonic, operation_id, \
				     mask, pattern, condition, offset, length)

static const struct source_manifest_provenance source_provenance =
#include "../source_manifest.def"
;

#undef TCTI_A64_SOURCE_MANIFEST_ROW
#undef TCTI_A64_SOURCE_MANIFEST_SOURCE

#define TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, name, mnemonic, operation_id, \
				     mask, pattern, condition, offset, length) \
	{ name, mnemonic, operation_id, (mask), (pattern) },
static const struct source_manifest_row source_manifest[] = {
#include "../source_manifest.def"
};
#undef TCTI_A64_SOURCE_MANIFEST_ROW
#undef TCTI_A64_SOURCE_MANIFEST_SOURCE

static const struct reviewed_classification_binding reviewed_bindings[] = {
	{ "UDF_only_perm_undef", CLASS_ARCH_UNDEFINED,
	  "pinned-source:architecturally-undefined-el0-rejection",
	  "kunit:source-leaf-udf-undefined" },
	{ "HVC_EX_exception", CLASS_NON_EL0,
	  "pinned-source:privileged-el0-rejection", "kunit:source-leaf-hvc-non-el0" },
	{ "SMC_EX_exception", CLASS_NON_EL0,
	  "pinned-source:privileged-el0-rejection", "kunit:source-leaf-smc-non-el0" },
	{ "DCPS1_DC_exception", CLASS_NON_EL0,
	  "pinned-source:privileged-el0-rejection", "kunit:source-leaf-dcps1-non-el0" },
	{ "DCPS2_DC_exception", CLASS_NON_EL0,
	  "pinned-source:privileged-el0-rejection", "kunit:source-leaf-dcps2-non-el0" },
	{ "DCPS3_DC_exception", CLASS_NON_EL0,
	  "pinned-source:privileged-el0-rejection", "kunit:source-leaf-dcps3-non-el0" },
	{ "SYSL_RC_systeminstrs", CLASS_NON_EL0,
	  "pinned-source:privileged-el0-rejection", "kunit:source-leaf-sysl-non-el0" },
	{ "ERET_64E_branch_reg", CLASS_NON_EL0,
	  "pinned-source:privileged-el0-rejection", "kunit:source-leaf-eret-non-el0" },
	{ "DRPS_64E_branch_reg", CLASS_NON_EL0,
	  "pinned-source:privileged-el0-rejection", "kunit:source-leaf-drps-non-el0" },
};

static const struct reviewed_classification_binding *
reviewed_binding_for(const char *name)
{
	size_t index;

	for (index = 0; index < sizeof(reviewed_bindings) /
			     sizeof(reviewed_bindings[0]); index++)
		if (!strcmp(reviewed_bindings[index].name, name))
			return &reviewed_bindings[index];
	return NULL;
}

static const char *classification_name(enum classification classification)
{
	switch (classification) {
	case CLASS_UNCLASSIFIED:
		return "UNCLASSIFIED";
	case CLASS_NON_EL0:
		return "NON_EL0";
	case CLASS_ARCH_UNDEFINED:
		return "ARCHITECTURALLY_UNDEFINED";
	}
	return NULL;
}

static int validate_source_manifest(const struct tcti_target_inventory *inventory,
				    const char *bytes, size_t length)
{
	char digest[65];
	size_t index;

	tcti_target_inventory_sha256(bytes, length, digest);
	if (length != source_provenance.source_length ||
	    strcmp(digest, source_provenance.sha256)) {
		fprintf(stderr, "pinned source digest differs from source manifest\n");
		return -1;
	}
	if (inventory->leaf_count != sizeof(source_manifest) /
				     sizeof(source_manifest[0])) {
		fprintf(stderr, "pinned source leaf count differs from source manifest\n");
		return -1;
	}
	for (index = 0; index < inventory->leaf_count; index++) {
		const struct tcti_target_leaf *leaf = &inventory->leaves[index];
		const struct source_manifest_row *manifest = &source_manifest[index];

		if (strcmp(leaf->name, manifest->name) ||
		    strcmp(leaf->mnemonic, manifest->mnemonic) ||
		    strcmp(leaf->operation_id, manifest->operation_id) ||
		    leaf->encoding_mask != manifest->mask ||
		    leaf->encoding_pattern != manifest->pattern) {
			fprintf(stderr, "pinned source differs from manifest at ordinal %zu\n",
				index);
			return -1;
		}
	}
	return 0;
}

static int emit_seed(FILE *output, const struct tcti_target_inventory *inventory)
{
	size_t index;

	fputs("/* SPDX-License-Identifier: GPL-2.0-only */\n"
	      "/* Bootstrap seed. Copy reviewed fields into target_classification.def. */\n"
	      "/* Every blank field is a deliberate completion blocker. */\n",
	      output);
	for (index = 0; index < inventory->leaf_count; index++) {
		const struct reviewed_classification_binding *binding =
			reviewed_binding_for(inventory->leaves[index].name);
		enum classification classification = binding ? binding->classification :
			CLASS_UNCLASSIFIED;
		const char *evidence = binding ? binding->evidence : "";
		const char *proof_id = binding ? binding->proof_id : "";

		if (fprintf(output,
			"TCTI_A64_TARGET_CLASSIFICATION(%s, %s, "
			"TCTI_A64_TARGET_RELATION_NONE, \"\", \"%s\", \"%s\")\n",
			inventory->leaves[index].name, classification_name(classification),
			evidence, proof_id) < 0)
			return -1;
	}
	return ferror(output) ? -1 : 0;
}

int main(int argc, char **argv)
{
	struct tcti_target_inventory inventory = { 0 };
	struct tcti_target_import_error error = { 0 };
	const char *instructions;
	FILE *input;
	long length;
	char *bytes = NULL;
	int status = EXIT_FAILURE;

	if (argc != 2) {
		fprintf(stderr, "usage: %s Instructions.json\n", argv[0]);
		return EXIT_FAILURE;
	}
	instructions = argv[1];
	input = fopen(instructions, "rb");
	if (!input || fseek(input, 0, SEEK_END) || (length = ftell(input)) < 0 ||
	    fseek(input, 0, SEEK_SET))
		goto out;
	if ((uintmax_t)length > SIZE_MAX - 1U)
		goto out;
	bytes = malloc((size_t)length + 1U);
	if (!bytes || fread(bytes, 1, (size_t)length, input) != (size_t)length)
		goto out;
	bytes[length] = '\0';
	if (tcti_target_inventory_import(bytes, (size_t)length, &inventory, &error)) {
		fprintf(stderr, "import error %d at %zu: %s\n", error.code,
			error.offset, error.message);
		goto out;
	}
	status = validate_source_manifest(&inventory, bytes, (size_t)length) ||
		emit_seed(stdout, &inventory) ?
		EXIT_FAILURE : EXIT_SUCCESS;
out:
	if (input)
		fclose(input);
	tcti_target_inventory_destroy(&inventory);
	free(bytes);
	return status;
}
