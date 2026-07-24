/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_hwcap_cohort_catalog.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define HWCAP_FP	(1UL << 0)
#define HWCAP_ASIMD	(1UL << 1)
#define HWCAP_AES	(1UL << 3)
#define HWCAP_PMULL	(1UL << 4)
#define HWCAP_SHA1	(1UL << 5)
#define HWCAP_SHA2	(1UL << 6)
#define HWCAP_CRC32	(1UL << 7)
#define HWCAP_SHA3	(1UL << 17)
#define HWCAP_SM3	(1UL << 18)
#define HWCAP_SM4	(1UL << 19)
#define HWCAP_SHA512	(1UL << 21)

#define AUXV_HWCAP TCTI_HWCAP_CATALOG_HWCAP
#define AUXV_HWCAP2 TCTI_HWCAP_CATALOG_HWCAP2
#define TCTI_A64_RUNTIME_CAPABILITY(word, bit, feature, extension) \
	{ word, bit, #bit, #feature },
static const struct tcti_hwcap_catalog_mapping current_mappings[] = {
#include "../isa/runtime_profile.def"
};
#undef TCTI_A64_RUNTIME_CAPABILITY
#undef AUXV_HWCAP2
#undef AUXV_HWCAP

#define TCTI_A64_PROFILE(major, minor, hwcap, hwcap2) \
	static const unsigned long current_hwcap = hwcap; \
	static const unsigned long current_hwcap2 = hwcap2;
#define TCTI_A64_SOURCE(...)
#define TCTI_A64_SOURCE_COUNTS(...)
#define TCTI_A64_FEATURE(...)
#define TCTI_A64_COUNTS(...)
#define TCTI_A64_ENCODING(...)
#define TCTI_A64_CONDITION(...)
#include "../isa/inventory.def"
#undef TCTI_A64_CONDITION
#undef TCTI_A64_ENCODING
#undef TCTI_A64_COUNTS
#undef TCTI_A64_FEATURE
#undef TCTI_A64_SOURCE_COUNTS
#undef TCTI_A64_SOURCE
#undef TCTI_A64_PROFILE

#define TCTI_A64_SOURCE_MANIFEST_SOURCE(release, build, ref, schema, sha, count) \
	static const char pinned_source_sha256[] = sha; \
	static const size_t pinned_source_declared_count = count;
#define TCTI_A64_SOURCE_MANIFEST_ROW(...)
#include "../isa/source_manifest.def"
#undef TCTI_A64_SOURCE_MANIFEST_ROW
#undef TCTI_A64_SOURCE_MANIFEST_SOURCE

#define TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, name, mnemonic, operation, mask, \
				     pattern, condition) \
	{ ordinal, name, mnemonic, operation, mask, pattern, condition },
static const struct tcti_target_ledger_source_row pinned_source_rows[] = {
#include "../isa/source_manifest.def"
};
#undef TCTI_A64_SOURCE_MANIFEST_ROW
#undef TCTI_A64_SOURCE_MANIFEST_SOURCE

struct condition_walk {
	const struct tcti_target_inventory *inventory;
	const char *feature;
	uint32_t ancestors[128];
};

static bool empty(const char *text)
{
	return !text || !text[0];
}

static int condition_contains_feature(struct condition_walk *walk,
				      uint32_t expression, size_t depth,
				      bool *contains)
{
	const struct tcti_target_expr *node;
	size_t index;
	bool found;

	if (expression == TCTI_TARGET_EXPR_NONE)
		return 0;
	if (expression >= walk->inventory->expression_count ||
	    depth >= sizeof(walk->ancestors) /
			     sizeof(walk->ancestors[0]))
		return -1;
	for (index = 0; index < depth; index++)
		if (walk->ancestors[index] == expression)
			return -1;
	walk->ancestors[depth] = expression;
	node = &walk->inventory->expressions[expression];
	switch (node->kind) {
	case TCTI_TARGET_EXPR_BOOL:
	case TCTI_TARGET_EXPR_OPERAND:
	case TCTI_TARGET_EXPR_VALUE:
		return 0;
	case TCTI_TARGET_EXPR_FEATURE:
		if (empty(node->text))
			return -1;
		if (!strcmp(node->text, walk->feature))
			*contains = true;
		return 0;
	case TCTI_TARGET_EXPR_NOT:
		return condition_contains_feature(walk, node->left, depth + 1,
						  contains);
	case TCTI_TARGET_EXPR_AND:
	case TCTI_TARGET_EXPR_OR:
	case TCTI_TARGET_EXPR_EQ:
	case TCTI_TARGET_EXPR_NE:
	case TCTI_TARGET_EXPR_IN:
		if (condition_contains_feature(walk, node->left, depth + 1,
					       contains))
			return -1;
		return condition_contains_feature(walk, node->right, depth + 1,
						  contains);
	case TCTI_TARGET_EXPR_SET:
		if (node->first_item > walk->inventory->set_item_count ||
		    node->item_count >
			    walk->inventory->set_item_count - node->first_item)
			return -1;
		for (index = 0; index < node->item_count; index++) {
			found = false;
			if (condition_contains_feature(
				    walk,
				    walk->inventory->set_items[node->first_item +
							       index],
				    depth + 1, &found))
				return -1;
			if (found)
				*contains = true;
		}
		return 0;
	}
	return -1;
}

static int leaf_has_feature(const struct tcti_target_inventory *inventory,
			    size_t leaf_index, const char *feature, bool *has)
{
	struct condition_walk walk = {
		.inventory = inventory,
		.feature = feature,
	};

	*has = false;
	return condition_contains_feature(&walk,
					  inventory->leaves[leaf_index].condition,
					  0, has);
}

static unsigned long *word(struct tcti_hwcap_cohort_catalog *catalog,
			   enum tcti_hwcap_catalog_word capability_word,
			   bool authorized)
{
	if (capability_word == TCTI_HWCAP_CATALOG_HWCAP)
		return authorized ? &catalog->authorized_hwcap :
				    &catalog->mapped_hwcap;
	if (capability_word == TCTI_HWCAP_CATALOG_HWCAP2)
		return authorized ? &catalog->authorized_hwcap2 :
				    &catalog->mapped_hwcap2;
	return NULL;
}

static enum tcti_hwcap_catalog_blocker binding_blocker(
	const struct tcti_target_ledger_classification_row *classification,
	bool proof_resolved)
{
	if (classification->classification ==
	    TCTI_TARGET_LEDGER_UNCLASSIFIED)
		return TCTI_HWCAP_CATALOG_UNCLASSIFIED;
	if (empty(classification->proof))
		return TCTI_HWCAP_CATALOG_MISSING_PROOF;
	if (!proof_resolved)
		return TCTI_HWCAP_CATALOG_UNRESOLVED_PROOF;
	return TCTI_HWCAP_CATALOG_PROVED;
}

static int fail(struct tcti_hwcap_cohort_catalog *catalog,
		enum tcti_hwcap_catalog_error *error,
		enum tcti_hwcap_catalog_error value)
{
	tcti_hwcap_cohort_catalog_destroy(catalog);
	if (error)
		*error = value;
	return -1;
}

int tcti_hwcap_cohort_catalog_build_with_mappings(
	const struct tcti_target_inventory *inventory,
	const struct tcti_target_ledger_classification_row *classifications,
	size_t classification_count, const bool *proof_resolved,
	const struct tcti_hwcap_catalog_mapping *mappings,
	size_t mapping_count, unsigned long advertised_hwcap,
	unsigned long advertised_hwcap2,
	struct tcti_hwcap_cohort_catalog *catalog,
	enum tcti_hwcap_catalog_error *error)
{
	size_t maximum_bindings;
	size_t mapping_index;
	size_t leaf_index;

	if (error)
		*error = TCTI_HWCAP_CATALOG_OK;
	if (!catalog)
		return -1;
	memset(catalog, 0, sizeof(*catalog));
	if (!inventory || !classifications || !proof_resolved || !mappings ||
	    !mapping_count)
		return fail(catalog, error,
			    TCTI_HWCAP_CATALOG_INVALID_ARGUMENT);
	if (inventory->leaf_count != TCTI_A64_TARGET_LEAF_COUNT ||
	    pinned_source_declared_count != TCTI_A64_TARGET_LEAF_COUNT ||
	    sizeof(pinned_source_rows) / sizeof(pinned_source_rows[0]) !=
		    TCTI_A64_TARGET_LEAF_COUNT ||
	    classification_count != inventory->leaf_count ||
	    !inventory->leaves ||
	    (inventory->expression_count && !inventory->expressions) ||
	    (inventory->set_item_count && !inventory->set_items))
		return fail(catalog, error,
			    TCTI_HWCAP_CATALOG_BAD_SOURCE_COUNT);
	if (mapping_count > 2U * sizeof(unsigned long) * 8U ||
	    mapping_count > SIZE_MAX / inventory->leaf_count)
		return fail(catalog, error,
			    TCTI_HWCAP_CATALOG_INVALID_MAPPING);
	maximum_bindings = mapping_count * inventory->leaf_count;
	if (maximum_bindings > SIZE_MAX / sizeof(*catalog->bindings))
		return fail(catalog, error,
			    TCTI_HWCAP_CATALOG_INVALID_MAPPING);
	catalog->cohorts = calloc(mapping_count, sizeof(*catalog->cohorts));
	catalog->bindings = calloc(maximum_bindings,
				   sizeof(*catalog->bindings));
	if (!catalog->cohorts || !catalog->bindings)
		return fail(catalog, error, TCTI_HWCAP_CATALOG_NO_MEMORY);
	catalog->cohort_count = mapping_count;
	catalog->advertised_hwcap = advertised_hwcap;
	catalog->advertised_hwcap2 = advertised_hwcap2;

	for (leaf_index = 0; leaf_index < inventory->leaf_count; leaf_index++) {
		const struct tcti_target_leaf *leaf =
			&inventory->leaves[leaf_index];
		const struct tcti_target_ledger_source_row *source =
			&pinned_source_rows[leaf_index];

		if (source->ordinal != leaf_index || empty(leaf->name) ||
		    empty(leaf->mnemonic) || empty(leaf->operation_id) ||
		    strcmp(leaf->name, source->name) ||
		    strcmp(leaf->mnemonic, source->mnemonic) ||
		    strcmp(leaf->operation_id, source->operation_id) ||
		    leaf->encoding_mask != source->mask ||
		    leaf->encoding_pattern != source->pattern)
			return fail(catalog, error,
				    TCTI_HWCAP_CATALOG_SOURCE_MISMATCH);
		if (
		    empty(classifications[leaf_index].name) ||
		    strcmp(leaf->name,
			   classifications[leaf_index].name) ||
		    classifications[leaf_index].classification <
			    TCTI_TARGET_LEDGER_UNCLASSIFIED ||
		    classifications[leaf_index].classification >
			    TCTI_TARGET_LEDGER_ALIAS_OR_DUPLICATE)
			return fail(catalog, error,
				    TCTI_HWCAP_CATALOG_BAD_CLASSIFICATION);
	}

	for (mapping_index = 0; mapping_index < mapping_count;
	     mapping_index++) {
		const struct tcti_hwcap_catalog_mapping *mapping =
			&mappings[mapping_index];
		struct tcti_hwcap_catalog_cohort *cohort =
			&catalog->cohorts[mapping_index];
		unsigned long *mapped = word(catalog, mapping->word, false);
		unsigned long *authorized =
			word(catalog, mapping->word, true);
		size_t previous;

		if (!mapped || !mapping->bit ||
		    (mapping->bit & (mapping->bit - 1)) ||
		    empty(mapping->bit_name) || empty(mapping->feature))
			return fail(catalog, error,
				    TCTI_HWCAP_CATALOG_INVALID_MAPPING);
		for (previous = 0; previous < mapping_index; previous++)
			if (!strcmp(mapping->bit_name,
				    mappings[previous].bit_name))
				return fail(
					catalog, error,
					TCTI_HWCAP_CATALOG_DUPLICATE_MAPPING);
		if (*mapped & mapping->bit)
			return fail(catalog, error,
				    TCTI_HWCAP_CATALOG_DUPLICATE_MAPPING);
		*mapped |= mapping->bit;
		cohort->mapping_index = mapping_index;
		cohort->first_binding = catalog->binding_count;
		for (leaf_index = 0; leaf_index < inventory->leaf_count;
		     leaf_index++) {
			struct tcti_hwcap_catalog_binding *binding;
			bool has_feature;

			if (leaf_has_feature(inventory, leaf_index,
					     mapping->feature, &has_feature))
				return fail(catalog, error,
					    TCTI_HWCAP_CATALOG_INVALID_CONDITION);
			if (!has_feature)
				continue;
			binding = &catalog->bindings[catalog->binding_count++];
			binding->mapping_index = mapping_index;
			binding->leaf_index = leaf_index;
			binding->classification =
				classifications[leaf_index].classification;
			binding->proof = classifications[leaf_index].proof;
			binding->blocker = binding_blocker(
				&classifications[leaf_index],
				proof_resolved[leaf_index]);
			cohort->binding_count++;
			if (binding->blocker != TCTI_HWCAP_CATALOG_PROVED)
				cohort->blocker_count++;
		}
		if (!cohort->binding_count)
			return fail(catalog, error,
				    TCTI_HWCAP_CATALOG_EMPTY_COHORT);
		cohort->authorized = !cohort->blocker_count;
		if (cohort->authorized)
			*authorized |= mapping->bit;
	}

	if ((advertised_hwcap & ~catalog->mapped_hwcap) ||
	    (advertised_hwcap2 & ~catalog->mapped_hwcap2))
		return fail(catalog, error,
			    TCTI_HWCAP_CATALOG_MISSING_MAPPING);
	catalog->advertised_without_proof_hwcap =
		advertised_hwcap & ~catalog->authorized_hwcap;
	catalog->advertised_without_proof_hwcap2 =
		advertised_hwcap2 & ~catalog->authorized_hwcap2;
	return 0;
}

const struct tcti_hwcap_catalog_mapping *
tcti_hwcap_catalog_current_mappings(size_t *count)
{
	if (count)
		*count = sizeof(current_mappings) / sizeof(current_mappings[0]);
	return current_mappings;
}

const struct tcti_target_ledger_source_row *
tcti_hwcap_catalog_pinned_source(size_t *count)
{
	if (count)
		*count = sizeof(pinned_source_rows) /
			 sizeof(pinned_source_rows[0]);
	return pinned_source_rows;
}

const char *tcti_hwcap_catalog_pinned_source_sha256(void)
{
	return pinned_source_sha256;
}

unsigned long tcti_hwcap_catalog_current_hwcap(void)
{
	return current_hwcap;
}

unsigned long tcti_hwcap_catalog_current_hwcap2(void)
{
	return current_hwcap2;
}

int tcti_hwcap_cohort_catalog_build(
	const struct tcti_target_inventory *inventory,
	const struct tcti_target_ledger_classification_row *classifications,
	size_t classification_count, const bool *proof_resolved,
	struct tcti_hwcap_cohort_catalog *catalog,
	enum tcti_hwcap_catalog_error *error)
{
	return tcti_hwcap_cohort_catalog_build_with_mappings(
		inventory, classifications, classification_count, proof_resolved,
		current_mappings,
		sizeof(current_mappings) / sizeof(current_mappings[0]),
		current_hwcap, current_hwcap2, catalog, error);
}

void tcti_hwcap_cohort_catalog_destroy(
	struct tcti_hwcap_cohort_catalog *catalog)
{
	if (!catalog)
		return;
	free(catalog->bindings);
	free(catalog->cohorts);
	memset(catalog, 0, sizeof(*catalog));
}
