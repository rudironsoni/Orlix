/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_PROOF_CANDIDATE_H
#define ORLIX_TCTI_TARGET_PROOF_CANDIDATE_H

#include <stddef.h>
#include <stdint.h>

#define ORLIX_TCTI_TARGET_PROOF_CANDIDATE_MAX_GROUPS 512U
#define ORLIX_TCTI_TARGET_PROOF_CANDIDATE_MAX_BINDINGS 1024U

enum orlix_tcti_target_proof_candidate_state {
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_UNPROVED,
};

enum orlix_tcti_target_proof_candidate_classification {
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_UNCLASSIFIED,
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_REQUIRED_EL0,
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_NON_EL0,
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_ARCH_UNDEFINED,
};

enum orlix_tcti_target_proof_candidate_source {
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_SOURCE_SCALAR,
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_SOURCE_LSE,
};

enum orlix_tcti_target_proof_candidate_obligation {
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_DECODE = 1U << 0,
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_LEGAL_ENCODINGS = 1U << 1,
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_REJECTED_ENCODINGS = 1U << 2,
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_REGISTERS = 1U << 3,
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_MEMORY = 1U << 4,
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_PC = 1U << 5,
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_FAULTS = 1U << 6,
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_ATOMICITY = 1U << 7,
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_ORDERING = 1U << 8,
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_FLAGS = 1U << 9,
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_SYSTEM_STATE = 1U << 10,
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_STRUCTURED_EXIT = 1U << 11,
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_OBLIGATION_UNPRIVILEGED_ACCESS = 1U << 12,
};

enum orlix_tcti_target_proof_candidate_blocker {
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_BLOCKER_SYSTEM_STATE = 1U << 0,
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_BLOCKER_STRUCTURED_EXIT = 1U << 1,
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_BLOCKER_UNPRIVILEGED_ACCESS = 1U << 2,
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_BLOCKER_LSE2 = 1U << 3,
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_BLOCKER_INCOMPLETE_PROVENANCE = 1U << 4,
};

struct orlix_tcti_target_proof_candidate_test_reference {
	const char *source;
	const char *source_sha256;
	const char *object;
	const char *suite;
	const char *suite_symbol;
	const char *case_array;
	const char *test_case;
};

struct orlix_tcti_target_proof_candidate_binding {
	enum orlix_tcti_target_proof_candidate_source source;
	enum orlix_tcti_target_proof_candidate_classification classification;
	enum orlix_tcti_target_proof_candidate_state state;
	const char *leaf_name;
	const char *mnemonic;
	const char *operation_id;
	uint32_t encoding_mask;
	uint32_t encoding_pattern;
	const char *condition_tcnd_hex;
	const char *binding_sha256;
	uint32_t source_ordinal;
	uint32_t obligations;
	uint32_t blockers;
	uint32_t variant_cohort;
	uint32_t semantic_class;
	struct orlix_tcti_target_proof_candidate_test_reference test_reference;
};

struct orlix_tcti_target_proof_candidate_group {
	enum orlix_tcti_target_proof_candidate_classification classification;
	enum orlix_tcti_target_proof_candidate_state state;
	const char *operation_id;
	uint32_t obligations;
	uint32_t blockers;
	uint32_t variant_cohort_mask;
	size_t binding_offset;
	size_t binding_count;
};

struct orlix_tcti_target_proof_candidate_set {
	enum orlix_tcti_target_proof_candidate_state state;
	const struct orlix_tcti_target_proof_candidate_group *groups;
	size_t group_count;
	const struct orlix_tcti_target_proof_candidate_binding *bindings;
	size_t binding_count;
	uint32_t blockers;
	const char *scalar_catalog_sha256;
	const char *lse_catalog_sha256;
};

enum orlix_tcti_target_proof_candidate_error {
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_OK,
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_INVALID_ARGUMENT,
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_SCALAR_CATALOG_INVALID,
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_LSE_CATALOG_INVALID,
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_TOO_MANY_GROUPS,
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_TOO_MANY_BINDINGS,
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_UNKNOWN_CLASSIFICATION,
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_UNKNOWN_OBLIGATION,
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_INCONSISTENT_OPERATION_PROFILE,
	ORLIX_TCTI_TARGET_PROOF_CANDIDATE_DUPLICATE_BINDING,
};

int orlix_tcti_target_proof_candidate_translate_scalar(
	uint32_t scalar_obligations, uint32_t *candidate_obligations,
	uint32_t *blockers);
int orlix_tcti_target_proof_candidate_translate_lse(
	uint32_t lse_obligations, uint32_t *candidate_obligations,
	uint32_t *blockers);

const struct orlix_tcti_target_proof_candidate_set *
orlix_tcti_target_proof_candidates(enum orlix_tcti_target_proof_candidate_error *error);

#endif /* ORLIX_TCTI_TARGET_PROOF_CANDIDATE_H */
