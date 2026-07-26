/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_LSE_OPERATION_CATALOG_H
#define ORLIX_TCTI_TARGET_LSE_OPERATION_CATALOG_H

#ifdef __KERNEL__
#include <linux/stddef.h>
#include <linux/types.h>
typedef u32 orlix_tcti_lse_catalog_u32;
#else
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef uint32_t orlix_tcti_lse_catalog_u32;
#endif

#define ORLIX_TCTI_LSE_OPERATION_CATALOG_DIRECT_LEAF_COUNT 357U

enum orlix_tcti_lse_operation_cohort {
	ORLIX_TCTI_LSE_OPERATION_COHORT_BASE_LSE,
	ORLIX_TCTI_LSE_OPERATION_COHORT_LOR,
	ORLIX_TCTI_LSE_OPERATION_COHORT_LSUI,
	ORLIX_TCTI_LSE_OPERATION_COHORT_LSE128,
	ORLIX_TCTI_LSE_OPERATION_COHORT_THE,
	ORLIX_TCTI_LSE_OPERATION_COHORT_RCPC,
	ORLIX_TCTI_LSE_OPERATION_COHORT_LSE2_SEMANTIC_VARIANT,
	ORLIX_TCTI_LSE_OPERATION_COHORT_NONE,
};

enum orlix_tcti_lse_operation_semantic_class {
	ORLIX_TCTI_LSE_OPERATION_SEMANTIC_COMPARE_AND_SWAP,
	ORLIX_TCTI_LSE_OPERATION_SEMANTIC_ATOMIC_RMW,
	ORLIX_TCTI_LSE_OPERATION_SEMANTIC_EXCLUSIVE,
	ORLIX_TCTI_LSE_OPERATION_SEMANTIC_ORDERED_LOAD_STORE,
	ORLIX_TCTI_LSE_OPERATION_SEMANTIC_UNPRIVILEGED_LOAD_STORE,
	ORLIX_TCTI_LSE_OPERATION_SEMANTIC_RCPC_LOAD_STORE,
	ORLIX_TCTI_LSE_OPERATION_SEMANTIC_LSE2_VARIANT,
};

enum orlix_tcti_lse_operation_obligation {
	ORLIX_TCTI_LSE_OPERATION_OBLIGATION_DECODE = 1U << 0,
	ORLIX_TCTI_LSE_OPERATION_OBLIGATION_LEGAL_ENCODINGS = 1U << 1,
	ORLIX_TCTI_LSE_OPERATION_OBLIGATION_REJECTED_ENCODINGS = 1U << 2,
	ORLIX_TCTI_LSE_OPERATION_OBLIGATION_REGISTERS = 1U << 3,
	ORLIX_TCTI_LSE_OPERATION_OBLIGATION_MEMORY = 1U << 4,
	ORLIX_TCTI_LSE_OPERATION_OBLIGATION_PC = 1U << 5,
	ORLIX_TCTI_LSE_OPERATION_OBLIGATION_FAULTS = 1U << 6,
	ORLIX_TCTI_LSE_OPERATION_OBLIGATION_ATOMICITY = 1U << 7,
	ORLIX_TCTI_LSE_OPERATION_OBLIGATION_ORDERING = 1U << 8,
	ORLIX_TCTI_LSE_OPERATION_OBLIGATION_UNPRIVILEGED_ACCESS = 1U << 9,
};

enum orlix_tcti_lse_operation_implementation_status {
	/* The decoder recognizes this source cohort, but it is not proof. */
	ORLIX_TCTI_LSE_OPERATION_STRUCTURAL_DECODER_ONLY,
	/* A broad decode range intentionally rejects this legal extension. */
	ORLIX_TCTI_LSE_OPERATION_REJECTED_BY_BASE_VALIDATION,
	/* Broad decode ranges must be audited before any extension claim. */
	ORLIX_TCTI_LSE_OPERATION_BROAD_DECODER_AUDIT_REQUIRED,
	ORLIX_TCTI_LSE_OPERATION_NO_FEATURE_IMPLEMENTATION,
	ORLIX_TCTI_LSE_OPERATION_SEMANTIC_VARIANT_UNMAPPED,
};

enum orlix_tcti_lse_operation_proof_status {
	ORLIX_TCTI_LSE_OPERATION_PROOF_NONE,
	ORLIX_TCTI_LSE_OPERATION_PROOF_KUNIT,
};

struct orlix_tcti_lse_operation_catalog_entry {
	orlix_tcti_lse_catalog_u32 source_ordinal;
	const char *source_leaf;
	const char *mnemonic;
	const char *operation_id;
	orlix_tcti_lse_catalog_u32 encoding_mask;
	orlix_tcti_lse_catalog_u32 encoding_pattern;
	const char *condition_tcnd_hex;
	enum orlix_tcti_lse_operation_cohort cohort;
	enum orlix_tcti_lse_operation_semantic_class semantic_class;
	orlix_tcti_lse_catalog_u32 obligations;
	enum orlix_tcti_lse_operation_implementation_status implementation_status;
	enum orlix_tcti_lse_operation_cohort implementation_cohort;
	enum orlix_tcti_lse_operation_proof_status proof_status;
	enum orlix_tcti_lse_operation_cohort proof_cohort;
	bool direct_source_leaf;
};

enum orlix_tcti_lse_operation_catalog_error {
	ORLIX_TCTI_LSE_OPERATION_CATALOG_OK,
	ORLIX_TCTI_LSE_OPERATION_CATALOG_BAD_COUNT,
	ORLIX_TCTI_LSE_OPERATION_CATALOG_MISSING_LEAF,
	ORLIX_TCTI_LSE_OPERATION_CATALOG_DUPLICATE_LEAF,
	ORLIX_TCTI_LSE_OPERATION_CATALOG_STALE_FINGERPRINT,
	ORLIX_TCTI_LSE_OPERATION_CATALOG_UNKNOWN_COHORT,
	ORLIX_TCTI_LSE_OPERATION_CATALOG_BAD_SEMANTIC_CLASS,
	ORLIX_TCTI_LSE_OPERATION_CATALOG_BAD_OBLIGATIONS,
	ORLIX_TCTI_LSE_OPERATION_CATALOG_CROSS_COHORT_IMPLEMENTATION,
	ORLIX_TCTI_LSE_OPERATION_CATALOG_CROSS_COHORT_PROOF,
	ORLIX_TCTI_LSE_OPERATION_CATALOG_INVALID_SUPPLEMENTAL_BLOCKER,
};

const struct orlix_tcti_lse_operation_catalog_entry *
orlix_tcti_lse_operation_catalog(size_t *count);

const struct orlix_tcti_lse_operation_catalog_entry *
orlix_tcti_lse_operation_catalog_lse2_blocker(void);

int orlix_tcti_lse_operation_catalog_validate(
	const struct orlix_tcti_lse_operation_catalog_entry *entries, size_t count,
	const struct orlix_tcti_lse_operation_catalog_entry *lse2_blocker,
	enum orlix_tcti_lse_operation_catalog_error *error);

#endif /* ORLIX_TCTI_TARGET_LSE_OPERATION_CATALOG_H */
