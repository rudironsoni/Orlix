/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_hwcap_cohort_catalog.h"
#include "target_proof_registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define EXPECT(condition)                                                        \
	do {                                                                      \
		if (!(condition)) {                                                \
			fprintf(stderr, "%s:%d: expectation failed: %s\n",         \
				__FILE__, __LINE__, #condition);                    \
			failures++;                                                 \
		}                                                                 \
	} while (0)

struct fixture {
	struct tcti_target_inventory inventory;
	struct tcti_target_ledger_classification_row *classifications;
	bool *proof_resolved;
};

static const char *const features[] = {
	"FEAT_FP", "FEAT_AdvSIMD", "FEAT_AES", "FEAT_SHA1",
	"FEAT_SHA256", "FEAT_CRC32", "FEAT_SHA3", "FEAT_SHA512",
	"FEAT_SM3", "FEAT_SM4",
};

static unsigned long mapped_hwcap(
	const struct tcti_hwcap_catalog_mapping *mappings, size_t count)
{
	unsigned long value = 0;
	size_t index;

	for (index = 0; index < count; index++)
		if (mappings[index].word == TCTI_HWCAP_CATALOG_HWCAP)
			value |= mappings[index].bit;
	return value;
}

static size_t apply_production_registry_proofs(
	const struct tcti_target_inventory *inventory,
	const struct tcti_target_ledger_classification_row *classifications,
	bool *proof_resolved,
	const struct tcti_target_proof_registry_entry *registry,
	size_t registry_count)
{
	size_t resolved = 0;
	size_t entry_index;

	for (entry_index = 0; entry_index < registry_count; entry_index++) {
		const struct tcti_target_proof_registry_entry *entry =
			&registry[entry_index];
		size_t binding_index;

		for (binding_index = 0; binding_index < entry->binding_count;
		     binding_index++) {
			const struct tcti_target_proof_binding *binding =
				&entry->bindings[binding_index];
			size_t matches = 0;
			size_t leaf_index;

			for (leaf_index = 0; leaf_index < inventory->leaf_count;
			     leaf_index++) {
				const struct tcti_target_leaf *leaf =
					&inventory->leaves[leaf_index];

				if (strcmp(binding->leaf_name, leaf->name) ||
				    strcmp(binding->mnemonic, leaf->mnemonic) ||
				    strcmp(entry->operation_id,
					   leaf->operation_id) ||
				    binding->encoding_mask !=
					    leaf->encoding_mask ||
				    binding->encoding_pattern !=
					    leaf->encoding_pattern)
					continue;
				EXPECT(entry->classification_mask &
				       (1U << classifications[leaf_index]
						       .classification));
				if (!proof_resolved[leaf_index])
					resolved++;
				proof_resolved[leaf_index] = true;
				matches++;
			}
			EXPECT(matches == 1);
		}
	}
	return resolved;
}

#define stringify_1(value) #value
#define stringify(value) stringify_1(value)
#define UNCLASSIFIED TCTI_TARGET_LEDGER_UNCLASSIFIED
#define REQUIRED TCTI_TARGET_LEDGER_REQUIRED_EL0
#define REQUIRED_EL0 TCTI_TARGET_LEDGER_REQUIRED_EL0
#define NON_EL0 TCTI_TARGET_LEDGER_NON_EL0
#define ARCHITECTURALLY_UNDEFINED \
	TCTI_TARGET_LEDGER_ARCH_UNDEFINED_OR_UNALLOCATED
#define ARCH_UNDEFINED_OR_UNALLOCATED \
	TCTI_TARGET_LEDGER_ARCH_UNDEFINED_OR_UNALLOCATED
#define ALIAS_OR_DUPLICATE TCTI_TARGET_LEDGER_ALIAS_OR_DUPLICATE
#define TCTI_A64_TARGET_CLASSIFICATION(name, classification, relation, \
				       canonical, evidence, proof) \
	{ stringify(name), classification, relation, canonical, evidence, proof, \
	  NULL, NULL, NULL },
static const struct tcti_target_ledger_classification_row
current_classifications[] = {
#include "../isa/target_classification.def"
};
#undef TCTI_A64_TARGET_CLASSIFICATION
#undef ALIAS_OR_DUPLICATE
#undef ARCH_UNDEFINED_OR_UNALLOCATED
#undef ARCHITECTURALLY_UNDEFINED
#undef NON_EL0
#undef REQUIRED_EL0
#undef REQUIRED
#undef UNCLASSIFIED
#undef stringify
#undef stringify_1

static int fixture_initialize(struct fixture *fixture)
{
	size_t index;
	size_t source_count;
	const struct tcti_target_ledger_source_row *source;

	memset(fixture, 0, sizeof(*fixture));
	fixture->inventory.leaf_count = TCTI_A64_TARGET_LEAF_COUNT;
	fixture->inventory.expression_count =
		sizeof(features) / sizeof(features[0]);
	fixture->inventory.leaves = calloc(fixture->inventory.leaf_count,
					  sizeof(*fixture->inventory.leaves));
	fixture->inventory.expressions =
		calloc(fixture->inventory.expression_count,
		       sizeof(*fixture->inventory.expressions));
	fixture->classifications =
		calloc(fixture->inventory.leaf_count,
		       sizeof(*fixture->classifications));
	fixture->proof_resolved =
		calloc(fixture->inventory.leaf_count,
		       sizeof(*fixture->proof_resolved));
	if (!fixture->inventory.leaves || !fixture->inventory.expressions ||
	    !fixture->classifications || !fixture->proof_resolved)
		return -1;
	source = tcti_hwcap_catalog_pinned_source(&source_count);
	if (!source || source_count != fixture->inventory.leaf_count)
		return -1;
	for (index = 0; index < fixture->inventory.expression_count; index++) {
		fixture->inventory.expressions[index].kind =
			TCTI_TARGET_EXPR_FEATURE;
		fixture->inventory.expressions[index].text =
			(char *)features[index];
	}
	for (index = 0; index < fixture->inventory.leaf_count; index++) {
		fixture->inventory.leaves[index].name =
			(char *)source[index].name;
		fixture->inventory.leaves[index].mnemonic =
			(char *)source[index].mnemonic;
		fixture->inventory.leaves[index].operation_id =
			(char *)source[index].operation_id;
		fixture->inventory.leaves[index].encoding_mask =
			source[index].mask;
		fixture->inventory.leaves[index].encoding_pattern =
			source[index].pattern;
		fixture->inventory.leaves[index].condition =
			index < fixture->inventory.expression_count ?
				(uint32_t)index : TCTI_TARGET_EXPR_NONE;
		fixture->classifications[index].name =
			source[index].name;
		fixture->classifications[index].classification =
			TCTI_TARGET_LEDGER_REQUIRED_EL0;
		fixture->classifications[index].proof = "resolved-proof";
		fixture->proof_resolved[index] = true;
	}
	return 0;
}

static void fixture_destroy(struct fixture *fixture)
{
	free(fixture->proof_resolved);
	free(fixture->classifications);
	free(fixture->inventory.expressions);
	free(fixture->inventory.leaves);
	memset(fixture, 0, sizeof(*fixture));
}

static int fixture_resize_expressions(struct fixture *fixture, size_t count)
{
	struct tcti_target_expr *expressions;
	size_t old_count = fixture->inventory.expression_count;

	expressions = realloc(fixture->inventory.expressions,
			      count * sizeof(*expressions));
	if (!expressions)
		return -1;
	fixture->inventory.expressions = expressions;
	memset(&expressions[old_count], 0,
	       (count - old_count) * sizeof(*expressions));
	fixture->inventory.expression_count = count;
	return 0;
}

static void current_catalog_maps_every_advertised_bit_to_source(void)
{
	struct fixture fixture;
	struct tcti_hwcap_cohort_catalog catalog;
	enum tcti_hwcap_catalog_error error;
	size_t mapping_count;
	const struct tcti_hwcap_catalog_mapping *mappings;
	unsigned long expected_mapped_hwcap;

	EXPECT(fixture_initialize(&fixture) == 0);
	mappings = tcti_hwcap_catalog_current_mappings(&mapping_count);
	EXPECT(mappings != NULL);
	EXPECT(mapping_count == 11);
	expected_mapped_hwcap = mapped_hwcap(mappings, mapping_count);
	EXPECT(!strcmp(tcti_hwcap_catalog_pinned_source_sha256(),
		       "a1ad2c6538a47cd97d8762791ac5af88bce1d5f6aff096c9b77aef853e76acfe"));
	EXPECT(tcti_hwcap_cohort_catalog_build(
		       &fixture.inventory, fixture.classifications,
		       fixture.inventory.leaf_count, fixture.proof_resolved,
		       &catalog, &error) == 0);
	EXPECT(error == TCTI_HWCAP_CATALOG_OK);
	EXPECT(catalog.cohort_count == mapping_count);
	EXPECT(catalog.binding_count == mapping_count);
	EXPECT(catalog.mapped_hwcap == expected_mapped_hwcap);
	EXPECT(catalog.authorized_hwcap == expected_mapped_hwcap);
	EXPECT(catalog.advertised_hwcap == 0);
	EXPECT(catalog.advertised_without_proof_hwcap == 0);
	EXPECT(catalog.mapped_hwcap2 == 0);
	EXPECT(catalog.authorized_hwcap2 == 0);
	tcti_hwcap_cohort_catalog_destroy(&catalog);
	fixture_destroy(&fixture);
}

static void blockers_are_retained_per_exact_leaf(void)
{
	struct fixture fixture;
	struct tcti_hwcap_cohort_catalog catalog;
	enum tcti_hwcap_catalog_error error;
	size_t mapping_count;
	const struct tcti_hwcap_catalog_mapping *mappings;
	size_t index;
	size_t blockers = 0;

	EXPECT(fixture_initialize(&fixture) == 0);
	fixture.classifications[0].classification =
		TCTI_TARGET_LEDGER_UNCLASSIFIED;
	fixture.classifications[0].proof = "";
	fixture.proof_resolved[0] = false;
	fixture.proof_resolved[2] = false;
	fixture.classifications[3].proof = "";
	mappings = tcti_hwcap_catalog_current_mappings(&mapping_count);
	EXPECT(tcti_hwcap_cohort_catalog_build(
		       &fixture.inventory, fixture.classifications,
		       fixture.inventory.leaf_count, fixture.proof_resolved,
		       &catalog, &error) == 0);
	for (index = 0; index < catalog.binding_count; index++) {
		const struct tcti_hwcap_catalog_binding *binding =
			&catalog.bindings[index];

		if (binding->blocker == TCTI_HWCAP_CATALOG_PROVED)
			continue;
		blockers++;
		EXPECT(binding->leaf_index == 0 || binding->leaf_index == 2 ||
		       binding->leaf_index == 3);
		if (binding->leaf_index == 0)
			EXPECT(!strcmp(
				mappings[binding->mapping_index].feature,
				"FEAT_FP"));
		else if (binding->leaf_index == 2)
			EXPECT(!strcmp(
				mappings[binding->mapping_index].feature,
				"FEAT_AES"));
		else
			EXPECT(!strcmp(
				mappings[binding->mapping_index].feature,
				"FEAT_SHA1"));
	}
	EXPECT(blockers == 4);
	EXPECT(catalog.authorized_hwcap != catalog.mapped_hwcap);
	EXPECT(catalog.advertised_hwcap == 0);
	EXPECT(catalog.advertised_without_proof_hwcap == 0);
	tcti_hwcap_cohort_catalog_destroy(&catalog);
	fixture_destroy(&fixture);
}

static void duplicate_and_missing_mappings_fail(void)
{
	struct fixture fixture;
	struct tcti_hwcap_cohort_catalog catalog;
	enum tcti_hwcap_catalog_error error;
	const struct tcti_hwcap_catalog_mapping *current;
	struct tcti_hwcap_catalog_mapping mappings[11];
	size_t mapping_count;
	unsigned long previous_unproved_hwcap;

	EXPECT(fixture_initialize(&fixture) == 0);
	current = tcti_hwcap_catalog_current_mappings(&mapping_count);
	EXPECT(mapping_count == sizeof(mappings) / sizeof(mappings[0]));
	previous_unproved_hwcap = mapped_hwcap(current, mapping_count);
	memcpy(mappings, current, sizeof(mappings));
	mappings[10].bit = mappings[0].bit;
	EXPECT(tcti_hwcap_cohort_catalog_build_with_mappings(
		       &fixture.inventory, fixture.classifications,
		       fixture.inventory.leaf_count, fixture.proof_resolved,
		       mappings, mapping_count,
		       previous_unproved_hwcap,
		       tcti_hwcap_catalog_current_hwcap2(), &catalog,
		       &error) == -1);
	EXPECT(error == TCTI_HWCAP_CATALOG_DUPLICATE_MAPPING);

	memcpy(mappings, current, sizeof(mappings));
	mappings[10].bit_name = mappings[0].bit_name;
	EXPECT(tcti_hwcap_cohort_catalog_build_with_mappings(
		       &fixture.inventory, fixture.classifications,
		       fixture.inventory.leaf_count, fixture.proof_resolved,
		       mappings, mapping_count,
		       previous_unproved_hwcap,
		       tcti_hwcap_catalog_current_hwcap2(), &catalog,
		       &error) == -1);
	EXPECT(error == TCTI_HWCAP_CATALOG_DUPLICATE_MAPPING);

	EXPECT(tcti_hwcap_cohort_catalog_build_with_mappings(
		       &fixture.inventory, fixture.classifications,
		       fixture.inventory.leaf_count, fixture.proof_resolved,
		       current, mapping_count - 1,
		       previous_unproved_hwcap,
		       tcti_hwcap_catalog_current_hwcap2(), &catalog,
		       &error) == -1);
	EXPECT(error == TCTI_HWCAP_CATALOG_MISSING_MAPPING);

	memcpy(mappings, current, sizeof(mappings));
	mappings[10].feature = "FEAT_NOT_IN_PINNED_SOURCE";
	EXPECT(tcti_hwcap_cohort_catalog_build_with_mappings(
		       &fixture.inventory, fixture.classifications,
		       fixture.inventory.leaf_count, fixture.proof_resolved,
		       mappings, mapping_count,
		       previous_unproved_hwcap,
		       tcti_hwcap_catalog_current_hwcap2(), &catalog,
		       &error) == -1);
	EXPECT(error == TCTI_HWCAP_CATALOG_EMPTY_COHORT);
	fixture_destroy(&fixture);
}

static void source_identity_drift_fails(void)
{
	struct fixture fixture;
	struct tcti_hwcap_cohort_catalog catalog;
	enum tcti_hwcap_catalog_error error;
	char *original;

	EXPECT(fixture_initialize(&fixture) == 0);
	original = fixture.inventory.leaves[17].operation_id;
	fixture.inventory.leaves[17].operation_id = "WRONG_OPERATION";
	EXPECT(tcti_hwcap_cohort_catalog_build(
		       &fixture.inventory, fixture.classifications,
		       fixture.inventory.leaf_count, fixture.proof_resolved,
		       &catalog, &error) == -1);
	EXPECT(error == TCTI_HWCAP_CATALOG_SOURCE_MISMATCH);
	fixture.inventory.leaves[17].operation_id = original;
	fixture_destroy(&fixture);
}

static void family_summary_cannot_override_source_blocker(void)
{
	struct fixture fixture;
	struct tcti_hwcap_cohort_catalog catalog;
	enum tcti_hwcap_catalog_error error;
	const struct tcti_hwcap_catalog_mapping *mappings;
	size_t mapping_count;
	size_t index;
	unsigned long fp_bit = 0;

	EXPECT(fixture_initialize(&fixture) == 0);
	fixture.proof_resolved[0] = false;
	mappings = tcti_hwcap_catalog_current_mappings(&mapping_count);
	EXPECT(tcti_hwcap_cohort_catalog_build(
		       &fixture.inventory, fixture.classifications,
		       fixture.inventory.leaf_count, fixture.proof_resolved,
		       &catalog, &error) == 0);
	for (index = 0; index < mapping_count; index++)
		if (!strcmp(mappings[index].bit_name, "HWCAP_FP"))
			fp_bit = mappings[index].bit;
	EXPECT(fp_bit != 0);
	EXPECT((catalog.authorized_hwcap & fp_bit) == 0);
	EXPECT((catalog.mapped_hwcap & fp_bit) == fp_bit);
	EXPECT((catalog.advertised_hwcap & fp_bit) == 0);
	EXPECT(catalog.advertised_without_proof_hwcap == 0);
	tcti_hwcap_cohort_catalog_destroy(&catalog);
	fixture_destroy(&fixture);
}

static void normalized_feature_grammar_builds_exact_cohorts(void)
{
	struct fixture fixture;
	struct tcti_hwcap_cohort_catalog catalog;
	enum tcti_hwcap_catalog_error error;
	const struct tcti_hwcap_catalog_mapping *mappings;
	size_t mapping_count;
	size_t mapping_index;
	size_t binding_index;
	size_t fp_bindings = 0;
	size_t fp_unique_leaves = 0;
	size_t aes_leaf_one_bindings = 0;
	bool saw_leaf[3] = { false, false, false };

	EXPECT(fixture_initialize(&fixture) == 0);
	EXPECT(fixture_resize_expressions(&fixture, 22) == 0);

	/* FEAT_FP OR an unknown feature. */
	fixture.inventory.expressions[10].kind = TCTI_TARGET_EXPR_FEATURE;
	fixture.inventory.expressions[10].text = "FEAT_UNKNOWN_TO_HWCAP";
	fixture.inventory.expressions[11].kind = TCTI_TARGET_EXPR_OR;
	fixture.inventory.expressions[11].left = 0;
	fixture.inventory.expressions[11].right = 10;
	fixture.inventory.leaves[0].condition = 11;

	/* FEAT_SHA1 implies FEAT_SHA256: !SHA1 OR SHA256. */
	fixture.inventory.expressions[12].kind = TCTI_TARGET_EXPR_NOT;
	fixture.inventory.expressions[12].left = 3;
	fixture.inventory.expressions[13].kind = TCTI_TARGET_EXPR_OR;
	fixture.inventory.expressions[13].left = 12;
	fixture.inventory.expressions[13].right = 4;
	fixture.inventory.leaves[3].condition = 13;

	/* AdvSIMD iff AES: (AdvSIMD && AES) || (!AdvSIMD && !AES). */
	fixture.inventory.expressions[14].kind = TCTI_TARGET_EXPR_AND;
	fixture.inventory.expressions[14].left = 1;
	fixture.inventory.expressions[14].right = 2;
	fixture.inventory.expressions[15].kind = TCTI_TARGET_EXPR_NOT;
	fixture.inventory.expressions[15].left = 1;
	fixture.inventory.expressions[16].kind = TCTI_TARGET_EXPR_NOT;
	fixture.inventory.expressions[16].left = 2;
	fixture.inventory.expressions[17].kind = TCTI_TARGET_EXPR_AND;
	fixture.inventory.expressions[17].left = 15;
	fixture.inventory.expressions[17].right = 16;
	fixture.inventory.expressions[18].kind = TCTI_TARGET_EXPR_OR;
	fixture.inventory.expressions[18].left = 14;
	fixture.inventory.expressions[18].right = 17;
	fixture.inventory.leaves[1].condition = 18;

	/* A shared-node DAG mentions FEAT_FP twice but remains one leaf. */
	fixture.inventory.expressions[19].kind = TCTI_TARGET_EXPR_OR;
	fixture.inventory.expressions[19].left = 0;
	fixture.inventory.expressions[19].right = 0;
	fixture.inventory.leaves[20].condition = 19;

	/* No direct feature identifier and an unknown-only feature. */
	fixture.inventory.expressions[20].kind = TCTI_TARGET_EXPR_BOOL;
	fixture.inventory.expressions[20].boolean = true;
	fixture.inventory.leaves[21].condition = 20;
	fixture.inventory.expressions[21].kind = TCTI_TARGET_EXPR_FEATURE;
	fixture.inventory.expressions[21].text = "FEAT_UNKNOWN_ONLY";
	fixture.inventory.leaves[22].condition = 21;

	mappings = tcti_hwcap_catalog_current_mappings(&mapping_count);
	EXPECT(tcti_hwcap_cohort_catalog_build(
		       &fixture.inventory, fixture.classifications,
		       fixture.inventory.leaf_count, fixture.proof_resolved,
		       &catalog, &error) == 0);
	for (mapping_index = 0; mapping_index < mapping_count;
	     mapping_index++) {
		const struct tcti_hwcap_catalog_cohort *cohort =
			&catalog.cohorts[mapping_index];

		for (binding_index = cohort->first_binding;
		     binding_index <
			     cohort->first_binding + cohort->binding_count;
		     binding_index++) {
			const struct tcti_hwcap_catalog_binding *binding =
				&catalog.bindings[binding_index];

			EXPECT(binding->leaf_index != 21);
			EXPECT(binding->leaf_index != 22);
			if (!strcmp(mappings[mapping_index].feature,
				    "FEAT_FP")) {
				fp_bindings++;
				if (binding->leaf_index < 3 &&
				    !saw_leaf[binding->leaf_index]) {
					saw_leaf[binding->leaf_index] = true;
					fp_unique_leaves++;
				}
				if (binding->leaf_index == 20)
					fp_unique_leaves++;
			}
			if (!strcmp(mappings[mapping_index].feature,
				    "FEAT_AES") &&
			    binding->leaf_index == 1)
				aes_leaf_one_bindings++;
		}
	}
	EXPECT(fp_bindings == 2);
	EXPECT(fp_unique_leaves == 2);
	EXPECT(aes_leaf_one_bindings == 2);
	EXPECT(catalog.advertised_without_proof_hwcap == 0);
	tcti_hwcap_cohort_catalog_destroy(&catalog);
	fixture_destroy(&fixture);
}

static void cyclic_feature_condition_fails_closed(void)
{
	struct fixture fixture;
	struct tcti_hwcap_cohort_catalog catalog;
	enum tcti_hwcap_catalog_error error;

	EXPECT(fixture_initialize(&fixture) == 0);
	EXPECT(fixture_resize_expressions(&fixture, 11) == 0);
	fixture.inventory.expressions[10].kind = TCTI_TARGET_EXPR_NOT;
	fixture.inventory.expressions[10].left = 10;
	fixture.inventory.leaves[0].condition = 10;
	EXPECT(tcti_hwcap_cohort_catalog_build(
		       &fixture.inventory, fixture.classifications,
		       fixture.inventory.leaf_count, fixture.proof_resolved,
		       &catalog, &error) == -1);
	EXPECT(error == TCTI_HWCAP_CATALOG_INVALID_CONDITION);
	fixture_destroy(&fixture);
}

static char *read_file(const char *path, size_t *length)
{
	FILE *file = fopen(path, "rb");
	char *data;
	long size;

	if (!file || fseek(file, 0, SEEK_END) ||
	    (size = ftell(file)) < 0 || fseek(file, 0, SEEK_SET)) {
		if (file)
			fclose(file);
		return NULL;
	}
	data = malloc((size_t)size + 1);
	if (!data || fread(data, 1, (size_t)size, file) != (size_t)size) {
		free(data);
		fclose(file);
		return NULL;
	}
	fclose(file);
	data[size] = '\0';
	*length = (size_t)size;
	return data;
}

static void current_source_and_classifications_remain_fail_closed(
	const char *instructions_path)
{
	struct tcti_target_inventory inventory = { 0 };
	struct tcti_target_import_error import_error;
	struct tcti_hwcap_cohort_catalog catalog;
	enum tcti_hwcap_catalog_error error;
	bool *proof_resolved;
	const struct tcti_target_proof_registry_entry *registry;
	size_t registry_count;
	size_t mapping_count;
	const struct tcti_hwcap_catalog_mapping *mappings;
	char *json;
	size_t length;
	size_t index;

	json = read_file(instructions_path, &length);
	EXPECT(json != NULL);
	if (!json)
		return;
	EXPECT(tcti_target_inventory_import(json, length, &inventory,
					    &import_error) == 0);
	free(json);
	if (inventory.leaf_count != TCTI_A64_TARGET_LEAF_COUNT) {
		tcti_target_inventory_destroy(&inventory);
		return;
	}
	EXPECT(sizeof(current_classifications) /
		       sizeof(current_classifications[0]) ==
	       inventory.leaf_count);
	proof_resolved = calloc(inventory.leaf_count,
				sizeof(*proof_resolved));
	EXPECT(proof_resolved != NULL);
	if (!proof_resolved) {
		tcti_target_inventory_destroy(&inventory);
		return;
	}
	registry = tcti_target_proof_registry_entries(&registry_count);
	EXPECT(registry != NULL);
	EXPECT(registry_count > 0);
	EXPECT(tcti_target_proof_registry_validate(
		       registry, registry_count, NULL) == 0);
	EXPECT(apply_production_registry_proofs(
		       &inventory, current_classifications, proof_resolved,
		       registry, registry_count) > 0);
	EXPECT(tcti_hwcap_catalog_current_hwcap() == 0);
	EXPECT(tcti_hwcap_catalog_current_hwcap2() == 0);
	mappings = tcti_hwcap_catalog_current_mappings(&mapping_count);
	EXPECT(tcti_hwcap_cohort_catalog_build(
		       &inventory, current_classifications,
		       sizeof(current_classifications) /
			       sizeof(current_classifications[0]),
		       proof_resolved, &catalog, &error) == 0);
	EXPECT(catalog.binding_count > catalog.cohort_count);
	EXPECT(catalog.mapped_hwcap == mapped_hwcap(mappings, mapping_count));
	EXPECT(catalog.mapped_hwcap != 0);
	EXPECT(catalog.authorized_hwcap == 0);
	EXPECT(catalog.advertised_hwcap == 0);
	EXPECT(catalog.advertised_without_proof_hwcap == 0);
	for (index = 0; index < catalog.binding_count; index++) {
		const struct tcti_hwcap_catalog_binding *binding =
			&catalog.bindings[index];

		EXPECT(binding->leaf_index < inventory.leaf_count);
		EXPECT(binding->classification ==
		       current_classifications[binding->leaf_index]
			       .classification);
		EXPECT(binding->blocker != TCTI_HWCAP_CATALOG_PROVED);
	}
	tcti_hwcap_cohort_catalog_destroy(&catalog);
	free(proof_resolved);
	tcti_target_inventory_destroy(&inventory);
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s /path/to/Instructions.json\n",
			argv[0]);
		return 2;
	}
	current_catalog_maps_every_advertised_bit_to_source();
	blockers_are_retained_per_exact_leaf();
	duplicate_and_missing_mappings_fail();
	source_identity_drift_fails();
	family_summary_cannot_override_source_blocker();
	normalized_feature_grammar_builds_exact_cohorts();
	cyclic_feature_condition_fails_closed();
	current_source_and_classifications_remain_fail_closed(argv[1]);

	if (failures) {
		fprintf(stderr, "HWCAP cohort catalog tests: %d failed\n",
			failures);
		return 1;
	}
	puts("HWCAP cohort catalog tests: 8 passed");
	return 0;
}
