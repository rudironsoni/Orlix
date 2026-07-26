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

#endif /* ORLIX_TCTI_TARGET_PROOF_REGISTRY_H */
