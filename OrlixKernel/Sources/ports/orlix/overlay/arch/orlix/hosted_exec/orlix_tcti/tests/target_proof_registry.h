/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_PROOF_REGISTRY_H
#define ORLIX_TCTI_TARGET_PROOF_REGISTRY_H

#ifdef __KERNEL__
#include <linux/stddef.h>
#include <linux/types.h>
typedef u8 orlix_tcti_proof_u8;
typedef u32 orlix_tcti_proof_u32;
typedef u64 orlix_tcti_proof_u64;
#define ORLIX_TCTI_PROOF_U64_C(value) value##ULL
#else
#include <stddef.h>
#include <stdint.h>
typedef uint8_t orlix_tcti_proof_u8;
typedef uint32_t orlix_tcti_proof_u32;
typedef uint64_t orlix_tcti_proof_u64;
#define ORLIX_TCTI_PROOF_U64_C(value) UINT64_C(value)
#endif

#define ORLIX_TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0 (1U << 1)
#define ORLIX_TCTI_TARGET_PROOF_CLASS_NON_EL0 (1U << 2)
#define ORLIX_TCTI_TARGET_PROOF_CLASS_ARCH_UNDEFINED_OR_UNALLOCATED (1U << 3)
#define ORLIX_TCTI_TARGET_PROOF_CLASS_ALIAS_OR_DUPLICATE (1U << 4)

#define ORLIX_TCTI_TARGET_LINUX_PROOF_SOURCE_ROWS 4350U
#define ORLIX_TCTI_TARGET_LINUX_PROOF_VARIANT_ROWS 2014U
#define ORLIX_TCTI_TARGET_LINUX_PROOF_TOTAL_ROWS \
	(ORLIX_TCTI_TARGET_LINUX_PROOF_SOURCE_ROWS + \
	 ORLIX_TCTI_TARGET_LINUX_PROOF_VARIANT_ROWS)

enum orlix_tcti_target_proof_obligation {
	ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE = 1U << 0,
	ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS = 1U << 1,
	ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS = 1U << 2,
	ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS = 1U << 3,
	ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY = 1U << 4,
	ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC = 1U << 5,
	ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS = 1U << 6,
	ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ATOMICITY = 1U << 7,
	ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ORDERING = 1U << 8,
	ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS = 1U << 9,
	ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LINUX_INTERFACE = 1U << 10,
};

enum orlix_tcti_target_proof_linux_interface {
	ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE,
	ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_REQUIRED,
};

struct orlix_tcti_target_proof_binding {
	const char *leaf_name;
	const char *mnemonic;
	orlix_tcti_proof_u32 encoding_mask;
	orlix_tcti_proof_u32 encoding_pattern;
	const char *condition_tcnd_hex;
	orlix_tcti_proof_u64 kunit_case_mask;
	orlix_tcti_proof_u32 source_ordinal;
};

struct orlix_tcti_target_proof_case {
	const char *name;
	orlix_tcti_proof_u32 obligations;
};

struct orlix_tcti_target_kselftest_provenance {
	const char *source;
	const char *source_sha256;
	const char *build_source;
	const char *build_source_sha256;
	const char *program;
	const char *case_name;
};

enum orlix_tcti_target_linux_proof_subject_kind {
	ORLIX_TCTI_TARGET_LINUX_PROOF_SOURCE_LEAF = 1,
	ORLIX_TCTI_TARGET_LINUX_PROOF_SYSTEM_ACCESSOR_VARIANT,
};

enum orlix_tcti_target_linux_proof_disposition {
	ORLIX_TCTI_TARGET_LINUX_PROOF_KSELFTEST_OWNED = 1,
	ORLIX_TCTI_TARGET_LINUX_PROOF_NOT_APPLICABLE,
};

enum orlix_tcti_target_linux_not_applicable_reason {
	ORLIX_TCTI_TARGET_LINUX_NA_NONE,
	ORLIX_TCTI_TARGET_LINUX_NA_REGISTER_OR_FLAG_SEMANTICS,
	ORLIX_TCTI_TARGET_LINUX_NA_LOCAL_CONTROL_FLOW,
	ORLIX_TCTI_TARGET_LINUX_NA_LOCAL_BARRIER_OR_HINT,
	ORLIX_TCTI_TARGET_LINUX_NA_ALIAS_OR_DUPLICATE,
	ORLIX_TCTI_TARGET_LINUX_NA_PREFETCH_HINT,
	ORLIX_TCTI_TARGET_LINUX_NA_ARCHITECTURAL_SEMANTICS_ONLY,
};

enum orlix_tcti_target_linux_interface_owner {
	ORLIX_TCTI_TARGET_LINUX_OWNER_NONE = 0,
	ORLIX_TCTI_TARGET_LINUX_OWNER_SYSCALL_PROCESS = 1U << 0,
	ORLIX_TCTI_TARGET_LINUX_OWNER_SIGNAL_FAULT = 1U << 1,
	ORLIX_TCTI_TARGET_LINUX_OWNER_MEMORY_VFS = 1U << 2,
	ORLIX_TCTI_TARGET_LINUX_OWNER_FD_PTY_TERMINAL = 1U << 3,
	ORLIX_TCTI_TARGET_LINUX_OWNER_ATOMIC_ORDERING = 1U << 4,
	ORLIX_TCTI_TARGET_LINUX_OWNER_SYSTEM_ACCESS = 1U << 5,
};

enum orlix_tcti_target_linux_execution_state {
	ORLIX_TCTI_TARGET_LINUX_EXECUTION_NOT_OBSERVED,
	ORLIX_TCTI_TARGET_LINUX_EXECUTION_OBSERVED,
};

struct orlix_tcti_target_linux_proof_source_identity {
	orlix_tcti_proof_u32 source_index;
	orlix_tcti_proof_u32 secondary_index;
	orlix_tcti_proof_u32 tertiary_index;
	const char *name;
	const char *mnemonic;
	const char *operation_id;
	orlix_tcti_proof_u32 encoding_mask;
	orlix_tcti_proof_u32 encoding_pattern;
	const char *condition_tcnd_hex;
	orlix_tcti_proof_u64 identity;
	orlix_tcti_proof_u64 condition_identity;
	orlix_tcti_proof_u64 source_offset;
	orlix_tcti_proof_u64 source_length;
	orlix_tcti_proof_u64 secondary_offset;
	orlix_tcti_proof_u64 secondary_length;
	orlix_tcti_proof_u64 condition_offset;
	orlix_tcti_proof_u64 condition_length;
};

struct orlix_tcti_target_linux_proof_disposition_row {
	enum orlix_tcti_target_linux_proof_subject_kind subject_kind;
	struct orlix_tcti_target_linux_proof_source_identity source;
	enum orlix_tcti_target_linux_proof_disposition disposition;
	enum orlix_tcti_target_linux_not_applicable_reason not_applicable_reason;
	orlix_tcti_proof_u32 linux_owner_mask;
	const struct orlix_tcti_target_kselftest_provenance *kselftests;
	size_t kselftest_count;
	enum orlix_tcti_target_linux_execution_state execution_state;
};

enum orlix_tcti_target_linux_proof_matrix_error {
	ORLIX_TCTI_TARGET_LINUX_MATRIX_ERROR_NONE = 0,
	ORLIX_TCTI_TARGET_LINUX_MATRIX_ERROR_COUNT = 1U << 0,
	ORLIX_TCTI_TARGET_LINUX_MATRIX_ERROR_MISSING = 1U << 1,
	ORLIX_TCTI_TARGET_LINUX_MATRIX_ERROR_DUPLICATE = 1U << 2,
	ORLIX_TCTI_TARGET_LINUX_MATRIX_ERROR_STALE = 1U << 3,
	ORLIX_TCTI_TARGET_LINUX_MATRIX_ERROR_MALFORMED = 1U << 4,
	ORLIX_TCTI_TARGET_LINUX_MATRIX_ERROR_AMBIGUOUS = 1U << 5,
	ORLIX_TCTI_TARGET_LINUX_MATRIX_ERROR_PROVENANCE = 1U << 6,
	ORLIX_TCTI_TARGET_LINUX_MATRIX_ERROR_SUBSTITUTION = 1U << 7,
};

struct orlix_tcti_target_linux_proof_matrix_result {
	orlix_tcti_proof_u32 error_mask;
	size_t errors;
	size_t total_rows;
	size_t source_leaf_rows;
	size_t semantic_variant_rows;
	size_t kselftest_owned_rows;
	size_t not_applicable_rows;
	size_t executed_kselftest_rows;
	size_t missing_rows;
	size_t duplicate_rows;
	size_t stale_rows;
	size_t malformed_rows;
	size_t ambiguous_rows;
	size_t invalid_provenance_rows;
	size_t substitution_rows;
};

struct orlix_tcti_target_proof_registry_entry {
	const char *id;
	const char *operation_id;
	orlix_tcti_proof_u32 classification_mask;
	orlix_tcti_proof_u32 obligations;
	enum orlix_tcti_target_proof_linux_interface linux_interface;
	const char *kunit_source;
	const char *kunit_suite;
	const struct orlix_tcti_target_proof_case *kunit_cases;
	size_t kunit_case_count;
	const struct orlix_tcti_target_proof_binding *bindings;
	size_t binding_count;
	const struct orlix_tcti_target_kselftest_provenance *kselftest;
	/*
	 * Required duties not discharged by the registered KUnit cases.
	 * Static source ownership remains valid, but completion must fail closed
	 * until native execution evidence clears every bit.
	 */
	orlix_tcti_proof_u32 unproved_obligations;
};

struct orlix_tcti_target_proof_reference {
	const char *id;
	const char *leaf_name;
	const char *mnemonic;
	const char *operation_id;
	orlix_tcti_proof_u32 encoding_mask;
	orlix_tcti_proof_u32 encoding_pattern;
	const char *condition_tcnd_hex;
	unsigned int classification;
};

enum orlix_tcti_target_proof_registry_error {
	ORLIX_TCTI_TARGET_PROOF_REGISTRY_OK,
	ORLIX_TCTI_TARGET_PROOF_REGISTRY_INVALID_ENTRY,
	ORLIX_TCTI_TARGET_PROOF_REGISTRY_DUPLICATE_ID,
	ORLIX_TCTI_TARGET_PROOF_REGISTRY_MISSING_REFERENCE,
	ORLIX_TCTI_TARGET_PROOF_REGISTRY_UNKNOWN_PROOF,
	ORLIX_TCTI_TARGET_PROOF_REGISTRY_CLASSIFICATION_MISMATCH,
	ORLIX_TCTI_TARGET_PROOF_REGISTRY_FAMILY_MISMATCH,
	ORLIX_TCTI_TARGET_PROOF_REGISTRY_INSUFFICIENT_OBLIGATIONS,
	ORLIX_TCTI_TARGET_PROOF_REGISTRY_UNKNOWN_FAMILY_REQUIREMENTS,
	ORLIX_TCTI_TARGET_PROOF_REGISTRY_BINDING_MISMATCH,
	ORLIX_TCTI_TARGET_PROOF_REGISTRY_INVALID_KUNIT_PROVENANCE,
	ORLIX_TCTI_TARGET_PROOF_REGISTRY_INVALID_KSELFTEST_PROVENANCE,
};

int orlix_tcti_target_proof_registry_validate(
	const struct orlix_tcti_target_proof_registry_entry *entries, size_t count,
	enum orlix_tcti_target_proof_registry_error *error);
int orlix_tcti_target_proof_registry_source_bound_projection_validate(
	const struct orlix_tcti_target_proof_registry_entry *entries, size_t count,
	enum orlix_tcti_target_proof_registry_error *error);
int orlix_tcti_target_kselftest_provenance_validate(
	const struct orlix_tcti_target_kselftest_provenance *provenance);
int orlix_tcti_target_proof_source_evidence_validate(
	const char *source, const char *source_sha256, const char *assertion);
int orlix_tcti_target_proof_source_size_allowed(orlix_tcti_proof_u64 size);
int orlix_tcti_target_proof_operation_requirements(
	const char *operation_id, unsigned int classification,
	orlix_tcti_proof_u32 *requirements);
enum orlix_tcti_target_proof_registry_error orlix_tcti_target_proof_registry_lookup(
	const struct orlix_tcti_target_proof_registry_entry *entries, size_t count,
	const struct orlix_tcti_target_proof_reference *reference);
const struct orlix_tcti_target_proof_registry_entry *
orlix_tcti_target_proof_registry_entries(size_t *count);
const struct orlix_tcti_target_linux_proof_disposition_row *
orlix_tcti_target_linux_proof_dispositions(size_t *count);
int orlix_tcti_target_linux_proof_matrix_validate(
	const struct orlix_tcti_target_linux_proof_disposition_row *rows,
	size_t count, struct orlix_tcti_target_linux_proof_matrix_result *result);
int orlix_tcti_target_linux_source_policy_validate_for_test(
	const struct orlix_tcti_target_linux_proof_source_identity *source);

#endif /* ORLIX_TCTI_TARGET_PROOF_REGISTRY_H */
