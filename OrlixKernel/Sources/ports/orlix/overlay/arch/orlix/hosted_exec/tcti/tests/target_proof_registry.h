/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_PROOF_REGISTRY_H
#define ORLIX_TCTI_TARGET_PROOF_REGISTRY_H

#include <stddef.h>
#include <stdint.h>

#define TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0 (1U << 1)
#define TCTI_TARGET_PROOF_CLASS_NON_EL0 (1U << 2)
#define TCTI_TARGET_PROOF_CLASS_ARCH_UNDEFINED_OR_UNALLOCATED (1U << 3)
#define TCTI_TARGET_PROOF_CLASS_ALIAS_OR_DUPLICATE (1U << 4)

enum tcti_target_proof_obligation {
	TCTI_TARGET_PROOF_OBLIGATION_DECODE = 1U << 0,
	TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS = 1U << 1,
	TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS = 1U << 2,
	TCTI_TARGET_PROOF_OBLIGATION_REGISTERS = 1U << 3,
	TCTI_TARGET_PROOF_OBLIGATION_MEMORY = 1U << 4,
	TCTI_TARGET_PROOF_OBLIGATION_PC = 1U << 5,
	TCTI_TARGET_PROOF_OBLIGATION_FAULTS = 1U << 6,
	TCTI_TARGET_PROOF_OBLIGATION_ATOMICITY = 1U << 7,
	TCTI_TARGET_PROOF_OBLIGATION_ORDERING = 1U << 8,
	TCTI_TARGET_PROOF_OBLIGATION_FLAGS = 1U << 9,
	TCTI_TARGET_PROOF_OBLIGATION_LINUX_INTERFACE = 1U << 10,
};

enum tcti_target_proof_linux_interface {
	TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE,
	TCTI_TARGET_PROOF_LINUX_INTERFACE_REQUIRED,
};

struct tcti_target_proof_binding {
	const char *leaf_name;
	const char *mnemonic;
	uint32_t encoding_mask;
	uint32_t encoding_pattern;
	const char *condition_tcnd_hex;
	uint64_t kunit_case_mask;
};

struct tcti_target_proof_case {
	const char *name;
	uint32_t obligations;
};

struct tcti_target_kselftest_provenance {
	const char *source;
	const char *source_sha256;
	const char *build_source;
	const char *build_source_sha256;
	const char *program;
	const char *case_name;
};

struct tcti_target_proof_registry_entry {
	const char *id;
	const char *operation_id;
	uint32_t classification_mask;
	uint32_t obligations;
	enum tcti_target_proof_linux_interface linux_interface;
	const char *kunit_source;
	const char *kunit_suite;
	const struct tcti_target_proof_case *kunit_cases;
	size_t kunit_case_count;
	const struct tcti_target_proof_binding *bindings;
	size_t binding_count;
	const struct tcti_target_kselftest_provenance *kselftest;
};

struct tcti_target_proof_reference {
	const char *id;
	const char *leaf_name;
	const char *mnemonic;
	const char *operation_id;
	uint32_t encoding_mask;
	uint32_t encoding_pattern;
	const char *condition_tcnd_hex;
	unsigned int classification;
};

enum tcti_target_proof_registry_error {
	TCTI_TARGET_PROOF_REGISTRY_OK,
	TCTI_TARGET_PROOF_REGISTRY_INVALID_ENTRY,
	TCTI_TARGET_PROOF_REGISTRY_DUPLICATE_ID,
	TCTI_TARGET_PROOF_REGISTRY_MISSING_REFERENCE,
	TCTI_TARGET_PROOF_REGISTRY_UNKNOWN_PROOF,
	TCTI_TARGET_PROOF_REGISTRY_CLASSIFICATION_MISMATCH,
	TCTI_TARGET_PROOF_REGISTRY_FAMILY_MISMATCH,
	TCTI_TARGET_PROOF_REGISTRY_INSUFFICIENT_OBLIGATIONS,
	TCTI_TARGET_PROOF_REGISTRY_UNKNOWN_FAMILY_REQUIREMENTS,
	TCTI_TARGET_PROOF_REGISTRY_BINDING_MISMATCH,
	TCTI_TARGET_PROOF_REGISTRY_INVALID_KUNIT_PROVENANCE,
	TCTI_TARGET_PROOF_REGISTRY_INVALID_KSELFTEST_PROVENANCE,
};

int tcti_target_proof_registry_validate(
	const struct tcti_target_proof_registry_entry *entries, size_t count,
	enum tcti_target_proof_registry_error *error);
int tcti_target_kselftest_provenance_validate(
	const struct tcti_target_kselftest_provenance *provenance);
int tcti_target_proof_source_evidence_validate(
	const char *source, const char *source_sha256, const char *assertion);
int tcti_target_proof_source_size_allowed(uint64_t size);
int tcti_target_proof_operation_requirements(
	const char *operation_id, unsigned int classification,
	uint32_t *requirements);
enum tcti_target_proof_registry_error tcti_target_proof_registry_lookup(
	const struct tcti_target_proof_registry_entry *entries, size_t count,
	const struct tcti_target_proof_reference *reference);
const struct tcti_target_proof_registry_entry *
tcti_target_proof_registry_entries(size_t *count);

#endif /* ORLIX_TCTI_TARGET_PROOF_REGISTRY_H */
