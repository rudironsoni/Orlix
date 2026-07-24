/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_LSE_OPERATION_CATALOG_H
#define ORLIX_TCTI_TARGET_LSE_OPERATION_CATALOG_H

#ifdef __KERNEL__
#include <linux/stddef.h>
#include <linux/types.h>
typedef u32 tcti_lse_catalog_u32;
#else
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef uint32_t tcti_lse_catalog_u32;
#endif

#define TCTI_LSE_OPERATION_CATALOG_DIRECT_LEAF_COUNT 357U

enum tcti_lse_operation_cohort {
	TCTI_LSE_OPERATION_COHORT_BASE_LSE,
	TCTI_LSE_OPERATION_COHORT_LOR,
	TCTI_LSE_OPERATION_COHORT_LSUI,
	TCTI_LSE_OPERATION_COHORT_LSE128,
	TCTI_LSE_OPERATION_COHORT_THE,
	TCTI_LSE_OPERATION_COHORT_RCPC,
	TCTI_LSE_OPERATION_COHORT_LSE2_SEMANTIC_VARIANT,
	TCTI_LSE_OPERATION_COHORT_NONE,
};

enum tcti_lse_operation_semantic_class {
	TCTI_LSE_OPERATION_SEMANTIC_COMPARE_AND_SWAP,
	TCTI_LSE_OPERATION_SEMANTIC_ATOMIC_RMW,
	TCTI_LSE_OPERATION_SEMANTIC_EXCLUSIVE,
	TCTI_LSE_OPERATION_SEMANTIC_ORDERED_LOAD_STORE,
	TCTI_LSE_OPERATION_SEMANTIC_UNPRIVILEGED_LOAD_STORE,
	TCTI_LSE_OPERATION_SEMANTIC_RCPC_LOAD_STORE,
	TCTI_LSE_OPERATION_SEMANTIC_LSE2_VARIANT,
};

enum tcti_lse_operation_obligation {
	TCTI_LSE_OPERATION_OBLIGATION_DECODE = 1U << 0,
	TCTI_LSE_OPERATION_OBLIGATION_LEGAL_ENCODINGS = 1U << 1,
	TCTI_LSE_OPERATION_OBLIGATION_REJECTED_ENCODINGS = 1U << 2,
	TCTI_LSE_OPERATION_OBLIGATION_REGISTERS = 1U << 3,
	TCTI_LSE_OPERATION_OBLIGATION_MEMORY = 1U << 4,
	TCTI_LSE_OPERATION_OBLIGATION_PC = 1U << 5,
	TCTI_LSE_OPERATION_OBLIGATION_FAULTS = 1U << 6,
	TCTI_LSE_OPERATION_OBLIGATION_ATOMICITY = 1U << 7,
	TCTI_LSE_OPERATION_OBLIGATION_ORDERING = 1U << 8,
	TCTI_LSE_OPERATION_OBLIGATION_UNPRIVILEGED_ACCESS = 1U << 9,
};

enum tcti_lse_operation_implementation_status {
	/* The decoder recognizes this source cohort, but it is not proof. */
	TCTI_LSE_OPERATION_STRUCTURAL_DECODER_ONLY,
	/* A broad decode range intentionally rejects this legal extension. */
	TCTI_LSE_OPERATION_REJECTED_BY_BASE_VALIDATION,
	/* Broad decode ranges must be audited before any extension claim. */
	TCTI_LSE_OPERATION_BROAD_DECODER_AUDIT_REQUIRED,
	TCTI_LSE_OPERATION_NO_FEATURE_IMPLEMENTATION,
	TCTI_LSE_OPERATION_SEMANTIC_VARIANT_UNMAPPED,
};

enum tcti_lse_operation_proof_status {
	TCTI_LSE_OPERATION_PROOF_NONE,
	TCTI_LSE_OPERATION_PROOF_KUNIT,
};

struct tcti_lse_operation_catalog_entry {
	tcti_lse_catalog_u32 source_ordinal;
	const char *source_leaf;
	const char *mnemonic;
	const char *operation_id;
	tcti_lse_catalog_u32 encoding_mask;
	tcti_lse_catalog_u32 encoding_pattern;
	const char *condition_tcnd_hex;
	enum tcti_lse_operation_cohort cohort;
	enum tcti_lse_operation_semantic_class semantic_class;
	tcti_lse_catalog_u32 obligations;
	enum tcti_lse_operation_implementation_status implementation_status;
	enum tcti_lse_operation_cohort implementation_cohort;
	enum tcti_lse_operation_proof_status proof_status;
	enum tcti_lse_operation_cohort proof_cohort;
	bool direct_source_leaf;
};

enum tcti_lse_operation_catalog_error {
	TCTI_LSE_OPERATION_CATALOG_OK,
	TCTI_LSE_OPERATION_CATALOG_BAD_COUNT,
	TCTI_LSE_OPERATION_CATALOG_MISSING_LEAF,
	TCTI_LSE_OPERATION_CATALOG_DUPLICATE_LEAF,
	TCTI_LSE_OPERATION_CATALOG_STALE_FINGERPRINT,
	TCTI_LSE_OPERATION_CATALOG_UNKNOWN_COHORT,
	TCTI_LSE_OPERATION_CATALOG_BAD_SEMANTIC_CLASS,
	TCTI_LSE_OPERATION_CATALOG_BAD_OBLIGATIONS,
	TCTI_LSE_OPERATION_CATALOG_CROSS_COHORT_IMPLEMENTATION,
	TCTI_LSE_OPERATION_CATALOG_CROSS_COHORT_PROOF,
	TCTI_LSE_OPERATION_CATALOG_INVALID_SUPPLEMENTAL_BLOCKER,
};

const struct tcti_lse_operation_catalog_entry *
tcti_lse_operation_catalog(size_t *count);

const struct tcti_lse_operation_catalog_entry *
tcti_lse_operation_catalog_lse2_blocker(void);

int tcti_lse_operation_catalog_validate(
	const struct tcti_lse_operation_catalog_entry *entries, size_t count,
	const struct tcti_lse_operation_catalog_entry *lse2_blocker,
	enum tcti_lse_operation_catalog_error *error);

#endif /* ORLIX_TCTI_TARGET_LSE_OPERATION_CATALOG_H */
