/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_lse_operation_catalog.h"

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

struct orlix_tcti_lse_source_row {
	orlix_tcti_lse_catalog_u32 ordinal;
	const char *name;
	const char *mnemonic;
	const char *operation_id;
	orlix_tcti_lse_catalog_u32 mask;
	orlix_tcti_lse_catalog_u32 pattern;
	const char *condition;
};

#define ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(version, build, reference, schema, sha256, count, ...)
#define ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, name, mnemonic, operation, mask, pattern, condition, ...) \
	{ ordinal, name, mnemonic, operation, mask, pattern, condition },
static const struct orlix_tcti_lse_source_row source_rows[] = {
#include "../isa/source_manifest.def"
};
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE

#define TCND_FEATURE_LSE "464541545f4c5345"
#define TCND_FEATURE_LOR "464541545f4c4f52"
#define TCND_FEATURE_LSUI "464541545f4c535549"
#define TCND_FEATURE_LSE128 "464541545f4c5345313238"
#define TCND_FEATURE_THE "464541545f544845"
#define TCND_FEATURE_LRCPC "464541545f4c52435043"
#define TCND_FEATURE_LRCPC2 "464541545f4c5243504332"
#define TCND_FEATURE_LRCPC3 "464541545f4c5243504333"

#define BASE_OBLIGATIONS \
	(ORLIX_TCTI_LSE_OPERATION_OBLIGATION_DECODE | \
	 ORLIX_TCTI_LSE_OPERATION_OBLIGATION_LEGAL_ENCODINGS | \
	 ORLIX_TCTI_LSE_OPERATION_OBLIGATION_REJECTED_ENCODINGS | \
	 ORLIX_TCTI_LSE_OPERATION_OBLIGATION_REGISTERS | \
	 ORLIX_TCTI_LSE_OPERATION_OBLIGATION_MEMORY | \
	 ORLIX_TCTI_LSE_OPERATION_OBLIGATION_PC | \
	 ORLIX_TCTI_LSE_OPERATION_OBLIGATION_FAULTS)
#define ATOMIC_OBLIGATIONS \
	(BASE_OBLIGATIONS | ORLIX_TCTI_LSE_OPERATION_OBLIGATION_ATOMICITY | \
	 ORLIX_TCTI_LSE_OPERATION_OBLIGATION_ORDERING)

static struct orlix_tcti_lse_operation_catalog_entry catalog[
	ORLIX_TCTI_LSE_OPERATION_CATALOG_DIRECT_LEAF_COUNT];
static bool catalog_ready;

static const struct orlix_tcti_lse_operation_catalog_entry lse2_blocker = {
	.source_leaf = "FEAT_LSE2 semantic-variant mapping",
	.operation_id = "FEAT_LSE2",
	.cohort = ORLIX_TCTI_LSE_OPERATION_COHORT_LSE2_SEMANTIC_VARIANT,
	.semantic_class = ORLIX_TCTI_LSE_OPERATION_SEMANTIC_LSE2_VARIANT,
	.obligations = ATOMIC_OBLIGATIONS,
	.implementation_status = ORLIX_TCTI_LSE_OPERATION_SEMANTIC_VARIANT_UNMAPPED,
	.implementation_cohort = ORLIX_TCTI_LSE_OPERATION_COHORT_NONE,
	.proof_status = ORLIX_TCTI_LSE_OPERATION_PROOF_NONE,
	.proof_cohort = ORLIX_TCTI_LSE_OPERATION_COHORT_NONE,
	.direct_source_leaf = false,
};

static bool condition_has(const char *condition, const char *feature)
{
	return condition && strstr(condition, feature);
}

static enum orlix_tcti_lse_operation_cohort
cohort_for_condition(const char *condition)
{
	if (condition_has(condition, TCND_FEATURE_LSE128))
		return ORLIX_TCTI_LSE_OPERATION_COHORT_LSE128;
	if (condition_has(condition, TCND_FEATURE_LRCPC3) ||
	    condition_has(condition, TCND_FEATURE_LRCPC2) ||
	    condition_has(condition, TCND_FEATURE_LRCPC))
		return ORLIX_TCTI_LSE_OPERATION_COHORT_RCPC;
	if (condition_has(condition, TCND_FEATURE_LOR))
		return ORLIX_TCTI_LSE_OPERATION_COHORT_LOR;
	if (condition_has(condition, TCND_FEATURE_LSUI))
		return ORLIX_TCTI_LSE_OPERATION_COHORT_LSUI;
	if (condition_has(condition, TCND_FEATURE_THE))
		return ORLIX_TCTI_LSE_OPERATION_COHORT_THE;
	if (condition_has(condition, TCND_FEATURE_LSE))
		return ORLIX_TCTI_LSE_OPERATION_COHORT_BASE_LSE;
	return ORLIX_TCTI_LSE_OPERATION_COHORT_NONE;
}

static bool has_prefix(const char *text, const char *prefix)
{
	return text && prefix && !strncmp(text, prefix, strlen(prefix));
}

static bool lsui_is_atomic(const char *operation)
{
	return has_prefix(operation, "CAS") || has_prefix(operation, "STTXR") ||
		has_prefix(operation, "STLTXR") || has_prefix(operation, "LDTXR") ||
		has_prefix(operation, "LDATXR") || has_prefix(operation, "LDTADD") ||
		has_prefix(operation, "LDTCLR") || has_prefix(operation, "LDTSET") ||
		has_prefix(operation, "SWPT");
}

static enum orlix_tcti_lse_operation_semantic_class
semantic_for(const struct orlix_tcti_lse_source_row *row,
	     enum orlix_tcti_lse_operation_cohort cohort)
{
	if (cohort == ORLIX_TCTI_LSE_OPERATION_COHORT_LOR)
		return ORLIX_TCTI_LSE_OPERATION_SEMANTIC_ORDERED_LOAD_STORE;
	if (cohort == ORLIX_TCTI_LSE_OPERATION_COHORT_LSUI) {
		if (has_prefix(row->operation_id, "STTXR") ||
		    has_prefix(row->operation_id, "STLTXR") ||
		    has_prefix(row->operation_id, "LDTXR") ||
		    has_prefix(row->operation_id, "LDATXR"))
			return ORLIX_TCTI_LSE_OPERATION_SEMANTIC_EXCLUSIVE;
		return ORLIX_TCTI_LSE_OPERATION_SEMANTIC_UNPRIVILEGED_LOAD_STORE;
	}
	if (cohort == ORLIX_TCTI_LSE_OPERATION_COHORT_RCPC)
		return ORLIX_TCTI_LSE_OPERATION_SEMANTIC_RCPC_LOAD_STORE;
	if (has_prefix(row->operation_id, "CAS") ||
	    has_prefix(row->operation_id, "RCWCAS") ||
	    has_prefix(row->operation_id, "RCWSCAS"))
		return ORLIX_TCTI_LSE_OPERATION_SEMANTIC_COMPARE_AND_SWAP;
	return ORLIX_TCTI_LSE_OPERATION_SEMANTIC_ATOMIC_RMW;
}

static orlix_tcti_lse_catalog_u32
obligations_for(const struct orlix_tcti_lse_source_row *row,
			enum orlix_tcti_lse_operation_cohort cohort)
{
	if (cohort == ORLIX_TCTI_LSE_OPERATION_COHORT_LOR ||
	    cohort == ORLIX_TCTI_LSE_OPERATION_COHORT_RCPC)
		return BASE_OBLIGATIONS | ORLIX_TCTI_LSE_OPERATION_OBLIGATION_ORDERING;
	if (cohort == ORLIX_TCTI_LSE_OPERATION_COHORT_LSUI)
		return BASE_OBLIGATIONS |
			ORLIX_TCTI_LSE_OPERATION_OBLIGATION_UNPRIVILEGED_ACCESS |
			(lsui_is_atomic(row->operation_id) ?
			 ORLIX_TCTI_LSE_OPERATION_OBLIGATION_ATOMICITY |
			 ORLIX_TCTI_LSE_OPERATION_OBLIGATION_ORDERING : 0U);
	return ATOMIC_OBLIGATIONS;
}

static void status_for(enum orlix_tcti_lse_operation_cohort cohort,
		       enum orlix_tcti_lse_operation_implementation_status *status,
		       enum orlix_tcti_lse_operation_cohort *implementation_cohort)
{
	*implementation_cohort = ORLIX_TCTI_LSE_OPERATION_COHORT_NONE;
	switch (cohort) {
	case ORLIX_TCTI_LSE_OPERATION_COHORT_BASE_LSE:
		*status = ORLIX_TCTI_LSE_OPERATION_STRUCTURAL_DECODER_ONLY;
		*implementation_cohort = cohort;
		break;
	case ORLIX_TCTI_LSE_OPERATION_COHORT_LOR:
		*status = ORLIX_TCTI_LSE_OPERATION_REJECTED_BY_BASE_VALIDATION;
		break;
	case ORLIX_TCTI_LSE_OPERATION_COHORT_LSUI:
		*status = ORLIX_TCTI_LSE_OPERATION_BROAD_DECODER_AUDIT_REQUIRED;
		break;
	default:
		*status = ORLIX_TCTI_LSE_OPERATION_NO_FEATURE_IMPLEMENTATION;
		break;
	}
}

static const struct orlix_tcti_lse_source_row *source_for(const char *name)
{
	size_t index;

	for (index = 0; index < sizeof(source_rows) / sizeof(source_rows[0]);
	     index++)
		if (!strcmp(source_rows[index].name, name))
			return &source_rows[index];
	return NULL;
}

static void build_catalog(void)
{
	size_t source_index;
	size_t catalog_index = 0;

	if (catalog_ready)
		return;
	for (source_index = 0;
	     source_index < sizeof(source_rows) / sizeof(source_rows[0]);
	     source_index++) {
		const struct orlix_tcti_lse_source_row *source = &source_rows[source_index];
		enum orlix_tcti_lse_operation_cohort cohort =
			cohort_for_condition(source->condition);
		struct orlix_tcti_lse_operation_catalog_entry *entry;

		if (cohort == ORLIX_TCTI_LSE_OPERATION_COHORT_NONE)
			continue;
		if (catalog_index >= ORLIX_TCTI_LSE_OPERATION_CATALOG_DIRECT_LEAF_COUNT)
			return;
		entry = &catalog[catalog_index++];
		entry->source_ordinal = source->ordinal;
		entry->source_leaf = source->name;
		entry->mnemonic = source->mnemonic;
		entry->operation_id = source->operation_id;
		entry->encoding_mask = source->mask;
		entry->encoding_pattern = source->pattern;
		entry->condition_tcnd_hex = source->condition;
		entry->cohort = cohort;
		entry->semantic_class = semantic_for(source, cohort);
		entry->obligations = obligations_for(source, cohort);
		status_for(cohort, &entry->implementation_status,
			   &entry->implementation_cohort);
		entry->proof_status = ORLIX_TCTI_LSE_OPERATION_PROOF_NONE;
		entry->proof_cohort = ORLIX_TCTI_LSE_OPERATION_COHORT_NONE;
		entry->direct_source_leaf = true;
	}
	if (catalog_index == ORLIX_TCTI_LSE_OPERATION_CATALOG_DIRECT_LEAF_COUNT)
		catalog_ready = true;
}

const struct orlix_tcti_lse_operation_catalog_entry *
orlix_tcti_lse_operation_catalog(size_t *count)
{
	build_catalog();
	if (count)
		*count = catalog_ready ? sizeof(catalog) / sizeof(catalog[0]) : 0;
	return catalog_ready ? catalog : NULL;
}

const struct orlix_tcti_lse_operation_catalog_entry *
orlix_tcti_lse_operation_catalog_lse2_blocker(void)
{
	return &lse2_blocker;
}

static int fail(enum orlix_tcti_lse_operation_catalog_error value,
		enum orlix_tcti_lse_operation_catalog_error *error)
{
	if (error)
		*error = value;
	return -1;
}

static bool valid_semantic(enum orlix_tcti_lse_operation_semantic_class semantic)
{
	return semantic >= ORLIX_TCTI_LSE_OPERATION_SEMANTIC_COMPARE_AND_SWAP &&
		semantic <= ORLIX_TCTI_LSE_OPERATION_SEMANTIC_LSE2_VARIANT;
}

int orlix_tcti_lse_operation_catalog_validate(
	const struct orlix_tcti_lse_operation_catalog_entry *entries, size_t count,
	const struct orlix_tcti_lse_operation_catalog_entry *supplemental,
	enum orlix_tcti_lse_operation_catalog_error *error)
{
	size_t index;

	if (error)
		*error = ORLIX_TCTI_LSE_OPERATION_CATALOG_OK;
	if (!entries || count != ORLIX_TCTI_LSE_OPERATION_CATALOG_DIRECT_LEAF_COUNT)
		return fail(ORLIX_TCTI_LSE_OPERATION_CATALOG_BAD_COUNT, error);
	if (!supplemental || supplemental->direct_source_leaf ||
	    supplemental->cohort != ORLIX_TCTI_LSE_OPERATION_COHORT_LSE2_SEMANTIC_VARIANT ||
	    supplemental->semantic_class != ORLIX_TCTI_LSE_OPERATION_SEMANTIC_LSE2_VARIANT ||
	    supplemental->implementation_status !=
		ORLIX_TCTI_LSE_OPERATION_SEMANTIC_VARIANT_UNMAPPED ||
	    supplemental->proof_status != ORLIX_TCTI_LSE_OPERATION_PROOF_NONE)
		return fail(ORLIX_TCTI_LSE_OPERATION_CATALOG_INVALID_SUPPLEMENTAL_BLOCKER,
			    error);

	for (index = 0; index < count; index++) {
		const struct orlix_tcti_lse_operation_catalog_entry *entry = &entries[index];
		const struct orlix_tcti_lse_source_row *source;
		size_t other;

		if (!entry->direct_source_leaf || !entry->source_leaf ||
		    entry->cohort == ORLIX_TCTI_LSE_OPERATION_COHORT_NONE ||
		    entry->cohort == ORLIX_TCTI_LSE_OPERATION_COHORT_LSE2_SEMANTIC_VARIANT)
			return fail(ORLIX_TCTI_LSE_OPERATION_CATALOG_UNKNOWN_COHORT, error);
		if (!valid_semantic(entry->semantic_class))
			return fail(ORLIX_TCTI_LSE_OPERATION_CATALOG_BAD_SEMANTIC_CLASS, error);
		if ((entry->obligations & BASE_OBLIGATIONS) != BASE_OBLIGATIONS)
			return fail(ORLIX_TCTI_LSE_OPERATION_CATALOG_BAD_OBLIGATIONS, error);
		if (entry->cohort == ORLIX_TCTI_LSE_OPERATION_COHORT_LSUI &&
		    !(entry->obligations &
		      ORLIX_TCTI_LSE_OPERATION_OBLIGATION_UNPRIVILEGED_ACCESS))
			return fail(ORLIX_TCTI_LSE_OPERATION_CATALOG_BAD_OBLIGATIONS, error);
		if (entry->implementation_cohort != ORLIX_TCTI_LSE_OPERATION_COHORT_NONE &&
		    entry->implementation_cohort != entry->cohort)
			return fail(ORLIX_TCTI_LSE_OPERATION_CATALOG_CROSS_COHORT_IMPLEMENTATION,
			    error);
		if (entry->proof_status != ORLIX_TCTI_LSE_OPERATION_PROOF_NONE &&
		    entry->proof_cohort != entry->cohort)
			return fail(ORLIX_TCTI_LSE_OPERATION_CATALOG_CROSS_COHORT_PROOF, error);
		if (entry->proof_status == ORLIX_TCTI_LSE_OPERATION_PROOF_NONE &&
		    entry->proof_cohort != ORLIX_TCTI_LSE_OPERATION_COHORT_NONE)
			return fail(ORLIX_TCTI_LSE_OPERATION_CATALOG_CROSS_COHORT_PROOF, error);
		source = source_for(entry->source_leaf);
		if (!source)
			return fail(ORLIX_TCTI_LSE_OPERATION_CATALOG_MISSING_LEAF, error);
		if (entry->source_ordinal != source->ordinal ||
		    strcmp(entry->mnemonic, source->mnemonic) ||
		    strcmp(entry->operation_id, source->operation_id) ||
		    entry->encoding_mask != source->mask ||
		    entry->encoding_pattern != source->pattern ||
		    strcmp(entry->condition_tcnd_hex, source->condition))
			return fail(ORLIX_TCTI_LSE_OPERATION_CATALOG_STALE_FINGERPRINT, error);
		for (other = 0; other < index; other++)
			if (!strcmp(entry->source_leaf, entries[other].source_leaf))
				return fail(ORLIX_TCTI_LSE_OPERATION_CATALOG_DUPLICATE_LEAF,
					    error);
	}
	return 0;
}
