/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_proof_candidate.h"

#include "target_lse_operation_catalog.h"
#include "target_scalar_operation_catalog.h"

#include <stdio.h>
#include <string.h>

#define EXPECT(value) do { \
	if (!(value)) { \
		fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #value); \
		return -1; \
	} \
} while (0)

static int scalar_obligations_translate_without_claiming_proof(void)
{
	uint32_t blockers;
	uint32_t obligations;

	EXPECT(tcti_target_proof_candidate_translate_scalar(
		       TCTI_SCALAR_OBLIGATION_DECODE |
			       TCTI_SCALAR_OBLIGATION_REGISTERS |
			       TCTI_SCALAR_OBLIGATION_SYSTEM_STATE,
		       &obligations, &blockers) == 0);
	EXPECT(obligations ==
	       (TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_DECODE |
		TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_REGISTERS |
		TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_SYSTEM_STATE));
	EXPECT(blockers ==
	       TCTI_TARGET_PROOF_CANDIDATE_BLOCKER_SYSTEM_STATE);
	EXPECT(tcti_target_proof_candidate_translate_scalar(
		       1U << 31, &obligations, &blockers) == -1);
	EXPECT(tcti_target_proof_candidate_translate_scalar(
		       TCTI_SCALAR_OBLIGATION_DECODE, NULL, &blockers) == -1);
	EXPECT(tcti_target_proof_candidate_translate_scalar(
		       TCTI_SCALAR_OBLIGATION_DECODE, &obligations, NULL) == -1);
	EXPECT(tcti_target_proof_candidate_translate_scalar(
		       0, &obligations, &blockers) == -1);
	return 0;
}

static int lse_obligations_translate_without_claiming_proof(void)
{
	uint32_t blockers;
	uint32_t obligations;

	EXPECT(tcti_target_proof_candidate_translate_lse(
		       TCTI_LSE_OPERATION_OBLIGATION_DECODE |
			       TCTI_LSE_OPERATION_OBLIGATION_MEMORY |
			       TCTI_LSE_OPERATION_OBLIGATION_UNPRIVILEGED_ACCESS,
		       &obligations, &blockers) == 0);
	EXPECT(obligations ==
	       (TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_DECODE |
		TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_MEMORY |
		TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_UNPRIVILEGED_ACCESS));
	EXPECT(blockers ==
	       TCTI_TARGET_PROOF_CANDIDATE_BLOCKER_UNPRIVILEGED_ACCESS);
	EXPECT(tcti_target_proof_candidate_translate_lse(
		       1U << 31, &obligations, &blockers) == -1);
	EXPECT(tcti_target_proof_candidate_translate_lse(
		       TCTI_LSE_OPERATION_OBLIGATION_DECODE, NULL,
		       &blockers) == -1);
	EXPECT(tcti_target_proof_candidate_translate_lse(
		       TCTI_LSE_OPERATION_OBLIGATION_DECODE, &obligations,
		       NULL) == -1);
	EXPECT(tcti_target_proof_candidate_translate_lse(
		       0, &obligations, &blockers) == -1);
	return 0;
}

static int candidates_cover_both_source_catalogs_and_stay_unproved(void)
{
	enum tcti_target_proof_candidate_error error;
	const struct tcti_target_proof_candidate_set *again;
	const struct tcti_target_proof_candidate_set *set;
	size_t expected_bindings =
		TCTI_SCALAR_OPERATION_CATALOG_EXPECTED_COUNT +
		TCTI_LSE_OPERATION_CATALOG_DIRECT_LEAF_COUNT;
	size_t scalar_bindings = 0;
	size_t lse_bindings = 0;
	size_t next_binding = 0;
	size_t group_index;
	size_t binding_index;

	set = tcti_target_proof_candidates(&error);
	EXPECT(set != NULL);
	EXPECT(error == TCTI_TARGET_PROOF_CANDIDATE_OK);
	again = tcti_target_proof_candidates(NULL);
	EXPECT(again == set);
	EXPECT(set->state == TCTI_TARGET_PROOF_CANDIDATE_UNPROVED);
	EXPECT(set->groups != NULL);
	EXPECT(set->group_count > 0);
	EXPECT(set->group_count <= TCTI_TARGET_PROOF_CANDIDATE_MAX_GROUPS);
	EXPECT(set->bindings != NULL);
	EXPECT(set->binding_count == expected_bindings);
	EXPECT(set->binding_count <= TCTI_TARGET_PROOF_CANDIDATE_MAX_BINDINGS);
	EXPECT((set->blockers &
		TCTI_TARGET_PROOF_CANDIDATE_BLOCKER_LSE2) != 0);
	EXPECT((set->blockers &
		TCTI_TARGET_PROOF_CANDIDATE_BLOCKER_INCOMPLETE_PROVENANCE) != 0);
	EXPECT(set->scalar_catalog_sha256 != NULL);
	EXPECT(strcmp(set->scalar_catalog_sha256,
		      TCTI_SCALAR_OPERATION_CATALOG_REVIEWED_SHA256) == 0);
	EXPECT(set->lse_catalog_sha256 == NULL);

	for (group_index = 0; group_index < set->group_count; group_index++) {
		const struct tcti_target_proof_candidate_group *group =
			&set->groups[group_index];

		EXPECT(group->state == TCTI_TARGET_PROOF_CANDIDATE_UNPROVED);
		EXPECT(group->classification <=
		       TCTI_TARGET_PROOF_CANDIDATE_ARCH_UNDEFINED);
		EXPECT(group->operation_id != NULL);
		EXPECT(group->operation_id[0] != '\0');
		EXPECT(group->obligations != 0);
		EXPECT(group->binding_offset == next_binding);
		EXPECT(group->binding_count > 0);
		EXPECT(group->binding_count <=
		       set->binding_count - group->binding_offset);
		EXPECT((set->blockers & group->blockers) == group->blockers);
		next_binding += group->binding_count;

		for (binding_index = group->binding_offset;
		     binding_index < next_binding; binding_index++) {
			const struct tcti_target_proof_candidate_binding *binding =
				&set->bindings[binding_index];
			size_t previous;

			EXPECT(binding->state ==
			       TCTI_TARGET_PROOF_CANDIDATE_UNPROVED);
			EXPECT(binding->classification == group->classification);
			EXPECT(binding->leaf_name != NULL);
			EXPECT(binding->mnemonic != NULL);
			EXPECT(binding->operation_id != NULL);
			EXPECT(strcmp(binding->operation_id,
				      group->operation_id) == 0);
			EXPECT(binding->condition_tcnd_hex != NULL);
			EXPECT(binding->obligations == group->obligations);
			EXPECT((group->blockers & binding->blockers) ==
			       binding->blockers);
			for (previous = 0; previous < binding_index; previous++)
				EXPECT(strcmp(binding->leaf_name,
					      set->bindings[previous].leaf_name) != 0);

			if (binding->source ==
			    TCTI_TARGET_PROOF_CANDIDATE_SOURCE_SCALAR) {
				scalar_bindings++;
				EXPECT(binding->classification ==
				       TCTI_TARGET_PROOF_CANDIDATE_REQUIRED_EL0);
				EXPECT(binding->source_ordinal == UINT32_MAX);
				EXPECT(binding->binding_sha256 != NULL);
				EXPECT(binding->test_reference.source != NULL);
				EXPECT(binding->test_reference.source_sha256 != NULL);
				EXPECT(binding->test_reference.object != NULL);
				EXPECT(binding->test_reference.suite != NULL);
				EXPECT(binding->test_reference.suite_symbol != NULL);
				EXPECT(binding->test_reference.case_array != NULL);
				EXPECT(binding->test_reference.test_case != NULL);
			} else {
				EXPECT(binding->source ==
				       TCTI_TARGET_PROOF_CANDIDATE_SOURCE_LSE);
				lse_bindings++;
				EXPECT(binding->source_ordinal != UINT32_MAX);
				EXPECT((binding->blockers &
					TCTI_TARGET_PROOF_CANDIDATE_BLOCKER_INCOMPLETE_PROVENANCE) != 0);
				EXPECT(binding->test_reference.source == NULL);
				if (binding->variant_cohort < 32)
					EXPECT((group->variant_cohort_mask &
						(1U << binding->variant_cohort)) != 0);
			}
		}
	}
	EXPECT(next_binding == set->binding_count);
	EXPECT(scalar_bindings ==
	       TCTI_SCALAR_OPERATION_CATALOG_EXPECTED_COUNT);
	EXPECT(lse_bindings ==
	       TCTI_LSE_OPERATION_CATALOG_DIRECT_LEAF_COUNT);
	return 0;
}

int main(void)
{
	static const struct {
		const char *name;
		int (*run)(void);
	} tests[] = {
		{ "scalar_obligations_translate_without_claiming_proof",
		  scalar_obligations_translate_without_claiming_proof },
		{ "lse_obligations_translate_without_claiming_proof",
		  lse_obligations_translate_without_claiming_proof },
		{ "candidates_cover_both_source_catalogs_and_stay_unproved",
		  candidates_cover_both_source_catalogs_and_stay_unproved },
	};
	size_t index;

	for (index = 0; index < sizeof(tests) / sizeof(tests[0]); index++) {
		if (tests[index].run())
			return 1;
		printf("PASS %s\n", tests[index].name);
	}
	return 0;
}
