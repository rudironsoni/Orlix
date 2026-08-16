/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_proof_candidate.h"

#include "target_lse_operation_catalog.h"
#include "target_scalar_operation_catalog.h"

#include <stdbool.h>
#include <string.h>

struct classification_row {
	const char *leaf_name;
	enum orlix_tcti_target_proof_candidate_classification classification;
};

#define ORLIX_TCTI_CANDIDATE_CLASS_UNCLASSIFIED \
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_UNCLASSIFIED
#define ORLIX_TCTI_CANDIDATE_CLASS_REQUIRED \
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_REQUIRED_EL0
#define ORLIX_TCTI_CANDIDATE_CLASS_NON_EL0 \
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_NON_EL0
#define ORLIX_TCTI_CANDIDATE_CLASS_ARCHITECTURALLY_UNDEFINED \
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_ARCH_UNDEFINED
#define ORLIX_TCTI_CANDIDATE_STRINGIFY_INNER(value) #value
#define ORLIX_TCTI_CANDIDATE_STRINGIFY(value) ORLIX_TCTI_CANDIDATE_STRINGIFY_INNER(value)
#define ORLIX_TCTI_A64_TARGET_CLASSIFICATION(leaf, classification, relation, \
				       related_leaf, evidence, proof) \
	{ ORLIX_TCTI_CANDIDATE_STRINGIFY(leaf), \
	  ORLIX_TCTI_CANDIDATE_CLASS_##classification },
static const struct classification_row classification_rows[] = {
#include "../isa/generations/current/target_classification.def"
};
#undef ORLIX_TCTI_A64_TARGET_CLASSIFICATION
#undef ORLIX_TCTI_CANDIDATE_STRINGIFY
#undef ORLIX_TCTI_CANDIDATE_STRINGIFY_INNER
#undef ORLIX_TCTI_CANDIDATE_CLASS_ARCHITECTURALLY_UNDEFINED
#undef ORLIX_TCTI_CANDIDATE_CLASS_NON_EL0
#undef ORLIX_TCTI_CANDIDATE_CLASS_REQUIRED
#undef ORLIX_TCTI_CANDIDATE_CLASS_UNCLASSIFIED

static struct orlix_tcti_target_proof_candidate_group
	candidate_groups[ORLIX_TCTI_TARGET_PROOF_CANDIDATE_MAX_GROUPS];
static struct orlix_tcti_target_proof_candidate_binding
	candidate_bindings[ORLIX_TCTI_TARGET_PROOF_CANDIDATE_MAX_BINDINGS];
static struct orlix_tcti_target_proof_candidate_set candidate_set;
static enum orlix_tcti_target_proof_candidate_error candidate_error;
static bool candidate_initialized;

static int translate_bit(uint32_t input, uint32_t source_bit,
			 uint32_t candidate_bit, uint32_t *remaining,
			 uint32_t *translated)
{
	if (!(input & source_bit))
		return 0;
	*remaining &= ~source_bit;
	*translated |= candidate_bit;
	return 0;
}

int orlix_tcti_target_proof_candidate_translate_scalar(
	uint32_t scalar_obligations, uint32_t *candidate_obligations,
	uint32_t *blockers)
{
	uint32_t remaining = scalar_obligations;
	uint32_t translated = 0;
	uint32_t blocked = 0;

	if (!candidate_obligations || !blockers || !scalar_obligations)
		return -1;
	translate_bit(scalar_obligations, ORLIX_TCTI_SCALAR_OBLIGATION_DECODE,
		      ORLIX_TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_DECODE,
		      &remaining, &translated);
	translate_bit(scalar_obligations, ORLIX_TCTI_SCALAR_OBLIGATION_LEGAL_ENCODINGS,
		      ORLIX_TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_LEGAL_ENCODINGS,
		      &remaining, &translated);
	translate_bit(scalar_obligations,
		      ORLIX_TCTI_SCALAR_OBLIGATION_REJECTED_ENCODINGS,
		      ORLIX_TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_REJECTED_ENCODINGS,
		      &remaining, &translated);
	translate_bit(scalar_obligations, ORLIX_TCTI_SCALAR_OBLIGATION_REGISTERS,
		      ORLIX_TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_REGISTERS,
		      &remaining, &translated);
	translate_bit(scalar_obligations, ORLIX_TCTI_SCALAR_OBLIGATION_PC,
		      ORLIX_TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_PC,
		      &remaining, &translated);
	translate_bit(scalar_obligations, ORLIX_TCTI_SCALAR_OBLIGATION_FLAGS,
		      ORLIX_TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_FLAGS,
		      &remaining, &translated);
	translate_bit(scalar_obligations, ORLIX_TCTI_SCALAR_OBLIGATION_FAULTS,
		      ORLIX_TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_FAULTS,
		      &remaining, &translated);
	translate_bit(scalar_obligations, ORLIX_TCTI_SCALAR_OBLIGATION_ORDERING,
		      ORLIX_TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_ORDERING,
		      &remaining, &translated);
	if (scalar_obligations & ORLIX_TCTI_SCALAR_OBLIGATION_SYSTEM_STATE) {
		remaining &= ~ORLIX_TCTI_SCALAR_OBLIGATION_SYSTEM_STATE;
		translated |= ORLIX_TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_SYSTEM_STATE;
		blocked |= ORLIX_TCTI_TARGET_PROOF_CANDIDATE_BLOCKER_SYSTEM_STATE;
	}
	if (scalar_obligations & ORLIX_TCTI_SCALAR_OBLIGATION_STRUCTURED_EXIT) {
		remaining &= ~ORLIX_TCTI_SCALAR_OBLIGATION_STRUCTURED_EXIT;
		translated |=
			ORLIX_TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_STRUCTURED_EXIT;
		blocked |= ORLIX_TCTI_TARGET_PROOF_CANDIDATE_BLOCKER_STRUCTURED_EXIT;
	}
	if (remaining)
		return -1;
	*candidate_obligations = translated;
	*blockers = blocked;
	return 0;
}

int orlix_tcti_target_proof_candidate_translate_lse(
	uint32_t lse_obligations, uint32_t *candidate_obligations,
	uint32_t *blockers)
{
	uint32_t remaining = lse_obligations;
	uint32_t translated = 0;
	uint32_t blocked = 0;

	if (!candidate_obligations || !blockers || !lse_obligations)
		return -1;
	translate_bit(lse_obligations, ORLIX_TCTI_LSE_OPERATION_OBLIGATION_DECODE,
		      ORLIX_TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_DECODE,
		      &remaining, &translated);
	translate_bit(lse_obligations,
		      ORLIX_TCTI_LSE_OPERATION_OBLIGATION_LEGAL_ENCODINGS,
		      ORLIX_TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_LEGAL_ENCODINGS,
		      &remaining, &translated);
	translate_bit(lse_obligations,
		      ORLIX_TCTI_LSE_OPERATION_OBLIGATION_REJECTED_ENCODINGS,
		      ORLIX_TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_REJECTED_ENCODINGS,
		      &remaining, &translated);
	translate_bit(lse_obligations, ORLIX_TCTI_LSE_OPERATION_OBLIGATION_REGISTERS,
		      ORLIX_TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_REGISTERS,
		      &remaining, &translated);
	translate_bit(lse_obligations, ORLIX_TCTI_LSE_OPERATION_OBLIGATION_MEMORY,
		      ORLIX_TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_MEMORY,
		      &remaining, &translated);
	translate_bit(lse_obligations, ORLIX_TCTI_LSE_OPERATION_OBLIGATION_PC,
		      ORLIX_TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_PC,
		      &remaining, &translated);
	translate_bit(lse_obligations, ORLIX_TCTI_LSE_OPERATION_OBLIGATION_FAULTS,
		      ORLIX_TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_FAULTS,
		      &remaining, &translated);
	translate_bit(lse_obligations, ORLIX_TCTI_LSE_OPERATION_OBLIGATION_ATOMICITY,
		      ORLIX_TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_ATOMICITY,
		      &remaining, &translated);
	translate_bit(lse_obligations, ORLIX_TCTI_LSE_OPERATION_OBLIGATION_ORDERING,
		      ORLIX_TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_ORDERING,
		      &remaining, &translated);
	if (lse_obligations &
	    ORLIX_TCTI_LSE_OPERATION_OBLIGATION_UNPRIVILEGED_ACCESS) {
		remaining &=
			~ORLIX_TCTI_LSE_OPERATION_OBLIGATION_UNPRIVILEGED_ACCESS;
		translated |=
			ORLIX_TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_UNPRIVILEGED_ACCESS;
		blocked |=
			ORLIX_TCTI_TARGET_PROOF_CANDIDATE_BLOCKER_UNPRIVILEGED_ACCESS;
	}
	if (remaining)
		return -1;
	*candidate_obligations = translated;
	*blockers = blocked;
	return 0;
}

static int classification_for(
	const char *leaf_name,
	enum orlix_tcti_target_proof_candidate_classification *classification)
{
	size_t index;

	if (!leaf_name || !classification)
		return -1;
	for (index = 0; index < sizeof(classification_rows) /
				      sizeof(classification_rows[0]); index++)
		if (!strcmp(leaf_name, classification_rows[index].leaf_name)) {
			*classification =
				classification_rows[index].classification;
			return 0;
		}
	return -1;
}

static struct orlix_tcti_target_proof_candidate_group *find_group(
	enum orlix_tcti_target_proof_candidate_classification classification,
	const char *operation_id, size_t group_count)
{
	size_t index;

	for (index = 0; index < group_count; index++)
		if (candidate_groups[index].classification == classification &&
		    !strcmp(candidate_groups[index].operation_id, operation_id))
			return &candidate_groups[index];
	return NULL;
}

static int add_group(
	enum orlix_tcti_target_proof_candidate_classification classification,
	const char *operation_id, uint32_t obligations, uint32_t blockers,
	uint32_t cohort, size_t *group_count)
{
	struct orlix_tcti_target_proof_candidate_group *group;

	group = find_group(classification, operation_id, *group_count);
	if (group) {
		if (group->obligations != obligations)
			return -2;
		group->blockers |= blockers;
		if (cohort < 32)
			group->variant_cohort_mask |= 1U << cohort;
		return 0;
	}
	if (*group_count >= ORLIX_TCTI_TARGET_PROOF_CANDIDATE_MAX_GROUPS)
		return -1;
	group = &candidate_groups[(*group_count)++];
	memset(group, 0, sizeof(*group));
	group->classification = classification;
	group->state = ORLIX_TCTI_TARGET_PROOF_CANDIDATE_UNPROVED;
	group->operation_id = operation_id;
	group->obligations = obligations;
	group->blockers = blockers;
	if (cohort < 32)
		group->variant_cohort_mask = 1U << cohort;
	return 0;
}

static bool duplicate_binding(const char *leaf_name, size_t binding_count)
{
	size_t index;

	for (index = 0; index < binding_count; index++)
		if (!strcmp(candidate_bindings[index].leaf_name, leaf_name))
			return true;
	return false;
}

static int append_scalar_binding(
	const struct orlix_tcti_scalar_operation_catalog_entry *entry,
	size_t *binding_count)
{
	struct orlix_tcti_target_proof_candidate_binding *binding;
	uint32_t blockers;
	uint32_t obligations;

	if (*binding_count >= ORLIX_TCTI_TARGET_PROOF_CANDIDATE_MAX_BINDINGS)
		return -1;
	if (duplicate_binding(entry->leaf_name, *binding_count))
		return -2;
	if (orlix_tcti_target_proof_candidate_translate_scalar(
		    entry->obligations, &obligations, &blockers))
		return -3;
	binding = &candidate_bindings[(*binding_count)++];
	memset(binding, 0, sizeof(*binding));
	binding->source = ORLIX_TCTI_TARGET_PROOF_CANDIDATE_SOURCE_SCALAR;
	binding->classification = ORLIX_TCTI_TARGET_PROOF_CANDIDATE_REQUIRED_EL0;
	binding->state = ORLIX_TCTI_TARGET_PROOF_CANDIDATE_UNPROVED;
	binding->leaf_name = entry->leaf_name;
	binding->mnemonic = entry->mnemonic;
	binding->operation_id = entry->operation_id;
	binding->encoding_mask = entry->encoding_mask;
	binding->encoding_pattern = entry->encoding_pattern;
	binding->condition_tcnd_hex = entry->condition_tcnd_hex;
	binding->binding_sha256 = entry->source_sha256;
	binding->source_ordinal = UINT32_MAX;
	binding->obligations = obligations;
	binding->blockers = blockers;
	binding->semantic_class = UINT32_MAX;
	binding->test_reference.source = entry->kunit_source;
	binding->test_reference.source_sha256 = entry->kunit_source_sha256;
	binding->test_reference.object = entry->kunit_object;
	binding->test_reference.suite = entry->kunit_suite;
	binding->test_reference.suite_symbol = entry->kunit_suite_symbol;
	binding->test_reference.case_array = entry->kunit_case_array;
	binding->test_reference.test_case = entry->kunit_case;
	return 0;
}

static int append_lse_binding(
	const struct orlix_tcti_lse_operation_catalog_entry *entry,
	enum orlix_tcti_target_proof_candidate_classification classification,
	size_t *binding_count)
{
	struct orlix_tcti_target_proof_candidate_binding *binding;
	uint32_t blockers;
	uint32_t obligations;

	if (*binding_count >= ORLIX_TCTI_TARGET_PROOF_CANDIDATE_MAX_BINDINGS)
		return -1;
	if (duplicate_binding(entry->source_leaf, *binding_count))
		return -2;
	if (orlix_tcti_target_proof_candidate_translate_lse(
		    entry->obligations, &obligations, &blockers))
		return -3;
	binding = &candidate_bindings[(*binding_count)++];
	memset(binding, 0, sizeof(*binding));
	binding->source = ORLIX_TCTI_TARGET_PROOF_CANDIDATE_SOURCE_LSE;
	binding->classification = classification;
	binding->state = ORLIX_TCTI_TARGET_PROOF_CANDIDATE_UNPROVED;
	binding->leaf_name = entry->source_leaf;
	binding->mnemonic = entry->mnemonic;
	binding->operation_id = entry->operation_id;
	binding->encoding_mask = entry->encoding_mask;
	binding->encoding_pattern = entry->encoding_pattern;
	binding->condition_tcnd_hex = entry->condition_tcnd_hex;
	binding->source_ordinal = entry->source_ordinal;
	binding->obligations = obligations;
	binding->blockers =
		blockers |
		ORLIX_TCTI_TARGET_PROOF_CANDIDATE_BLOCKER_INCOMPLETE_PROVENANCE;
	binding->variant_cohort = entry->cohort;
	binding->semantic_class = entry->semantic_class;
	return 0;
}

static enum orlix_tcti_target_proof_candidate_error initialize_candidates(void)
{
	const struct orlix_tcti_scalar_operation_catalog_entry *scalar_entries;
	const struct orlix_tcti_lse_operation_catalog_entry *lse_entries;
	const struct orlix_tcti_lse_operation_catalog_entry *lse2_blocker;
	enum orlix_tcti_scalar_operation_catalog_error scalar_error;
	enum orlix_tcti_lse_operation_catalog_error lse_error;
	size_t scalar_count;
	size_t lse_count;
	size_t group_count = 0;
	size_t binding_count = 0;
	size_t index;

	scalar_entries = orlix_tcti_scalar_operation_catalog_entries(&scalar_count);
	if (orlix_tcti_scalar_operation_catalog_validate(
		    scalar_entries, scalar_count, &scalar_error))
		return ORLIX_TCTI_TARGET_PROOF_CANDIDATE_SCALAR_CATALOG_INVALID;
	lse_entries = orlix_tcti_lse_operation_catalog(&lse_count);
	lse2_blocker = orlix_tcti_lse_operation_catalog_lse2_blocker();
	if (orlix_tcti_lse_operation_catalog_validate(
		    lse_entries, lse_count, lse2_blocker, &lse_error))
		return ORLIX_TCTI_TARGET_PROOF_CANDIDATE_LSE_CATALOG_INVALID;

	for (index = 0; index < scalar_count; index++) {
		uint32_t blockers;
		uint32_t obligations;
		int status;

		if (scalar_entries[index].proof_state !=
		    ORLIX_TCTI_SCALAR_OPERATION_UNPROVED)
			return ORLIX_TCTI_TARGET_PROOF_CANDIDATE_SCALAR_CATALOG_INVALID;
		if (orlix_tcti_target_proof_candidate_translate_scalar(
			    scalar_entries[index].obligations,
			    &obligations, &blockers))
			return ORLIX_TCTI_TARGET_PROOF_CANDIDATE_UNKNOWN_OBLIGATION;
		status = add_group(ORLIX_TCTI_TARGET_PROOF_CANDIDATE_REQUIRED_EL0,
				   scalar_entries[index].operation_id,
				   obligations, blockers, 32, &group_count);
		if (status == -1)
			return ORLIX_TCTI_TARGET_PROOF_CANDIDATE_TOO_MANY_GROUPS;
		if (status == -2)
			return ORLIX_TCTI_TARGET_PROOF_CANDIDATE_INCONSISTENT_OPERATION_PROFILE;
	}
	for (index = 0; index < lse_count; index++) {
		enum orlix_tcti_target_proof_candidate_classification classification;
		uint32_t blockers;
		uint32_t obligations;
		int status;

		if (!lse_entries[index].direct_source_leaf ||
		    lse_entries[index].proof_status !=
			    ORLIX_TCTI_LSE_OPERATION_PROOF_NONE)
			return ORLIX_TCTI_TARGET_PROOF_CANDIDATE_LSE_CATALOG_INVALID;
		if (classification_for(lse_entries[index].source_leaf,
				       &classification))
			return ORLIX_TCTI_TARGET_PROOF_CANDIDATE_UNKNOWN_CLASSIFICATION;
		if (orlix_tcti_target_proof_candidate_translate_lse(
			    lse_entries[index].obligations,
			    &obligations, &blockers))
			return ORLIX_TCTI_TARGET_PROOF_CANDIDATE_UNKNOWN_OBLIGATION;
		blockers |=
			ORLIX_TCTI_TARGET_PROOF_CANDIDATE_BLOCKER_INCOMPLETE_PROVENANCE;
		status = add_group(classification,
				   lse_entries[index].operation_id,
				   obligations, blockers,
				   lse_entries[index].cohort, &group_count);
		if (status == -1)
			return ORLIX_TCTI_TARGET_PROOF_CANDIDATE_TOO_MANY_GROUPS;
		if (status == -2)
			return ORLIX_TCTI_TARGET_PROOF_CANDIDATE_INCONSISTENT_OPERATION_PROFILE;
	}

	for (index = 0; index < group_count; index++) {
		struct orlix_tcti_target_proof_candidate_group *group =
			&candidate_groups[index];
		size_t source_index;

		group->binding_offset = binding_count;
		for (source_index = 0; source_index < scalar_count;
		     source_index++)
			if (group->classification ==
				    ORLIX_TCTI_TARGET_PROOF_CANDIDATE_REQUIRED_EL0 &&
			    !strcmp(group->operation_id,
				    scalar_entries[source_index].operation_id)) {
				int status = append_scalar_binding(
					&scalar_entries[source_index],
					&binding_count);

				if (status == -1)
					return ORLIX_TCTI_TARGET_PROOF_CANDIDATE_TOO_MANY_BINDINGS;
				if (status == -2)
					return ORLIX_TCTI_TARGET_PROOF_CANDIDATE_DUPLICATE_BINDING;
				if (status)
					return ORLIX_TCTI_TARGET_PROOF_CANDIDATE_UNKNOWN_OBLIGATION;
			}
		for (source_index = 0; source_index < lse_count; source_index++) {
			enum orlix_tcti_target_proof_candidate_classification classification;

			if (classification_for(
				    lse_entries[source_index].source_leaf,
				    &classification))
				return ORLIX_TCTI_TARGET_PROOF_CANDIDATE_UNKNOWN_CLASSIFICATION;
			if (group->classification != classification ||
			    strcmp(group->operation_id,
				   lse_entries[source_index].operation_id))
				continue;
			{
				int status = append_lse_binding(
					&lse_entries[source_index],
					classification, &binding_count);

				if (status == -1)
					return ORLIX_TCTI_TARGET_PROOF_CANDIDATE_TOO_MANY_BINDINGS;
				if (status == -2)
					return ORLIX_TCTI_TARGET_PROOF_CANDIDATE_DUPLICATE_BINDING;
				if (status)
					return ORLIX_TCTI_TARGET_PROOF_CANDIDATE_UNKNOWN_OBLIGATION;
			}
		}
		group->binding_count = binding_count - group->binding_offset;
	}

	candidate_set.state = ORLIX_TCTI_TARGET_PROOF_CANDIDATE_UNPROVED;
	candidate_set.groups = candidate_groups;
	candidate_set.group_count = group_count;
	candidate_set.bindings = candidate_bindings;
	candidate_set.binding_count = binding_count;
	candidate_set.blockers =
		ORLIX_TCTI_TARGET_PROOF_CANDIDATE_BLOCKER_LSE2 |
		ORLIX_TCTI_TARGET_PROOF_CANDIDATE_BLOCKER_INCOMPLETE_PROVENANCE;
	for (index = 0; index < group_count; index++)
		candidate_set.blockers |= candidate_groups[index].blockers;
	candidate_set.scalar_catalog_sha256 =
		ORLIX_TCTI_SCALAR_OPERATION_CATALOG_REVIEWED_SHA256;
	candidate_set.lse_catalog_sha256 = NULL;
	return ORLIX_TCTI_TARGET_PROOF_CANDIDATE_OK;
}

const struct orlix_tcti_target_proof_candidate_set *
orlix_tcti_target_proof_candidates(enum orlix_tcti_target_proof_candidate_error *error)
{
	if (!candidate_initialized) {
		candidate_error = initialize_candidates();
		candidate_initialized = true;
	}
	if (error)
		*error = candidate_error;
	return candidate_error == ORLIX_TCTI_TARGET_PROOF_CANDIDATE_OK ?
		&candidate_set : NULL;
}
