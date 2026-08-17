/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_NATIVE_PROOF_CONTRACT_H
#define ORLIX_TCTI_TARGET_NATIVE_PROOF_CONTRACT_H

#include "target_proof_registry.h"

#define ORLIX_TCTI_NATIVE_TEXT_MAX 256U
/* Pinned source maximum is 584 hex bytes plus the terminating NUL. */
#define ORLIX_TCTI_NATIVE_CONDITION_HEX_MAX 585U
#define ORLIX_TCTI_NATIVE_BUILD_ID_MAX 192U
#define ORLIX_TCTI_NATIVE_SHA256_SIZE 32U
#define ORLIX_TCTI_NATIVE_SHA256_HEX_SIZE 65U
/* Sealed production records are canonical, little-endian byte streams. */
#define ORLIX_TCTI_NATIVE_WIRE_MAGIC 0x4f545743U /* "OTWC", little endian */
#define ORLIX_TCTI_NATIVE_WIRE_SCHEMA_VERSION 1U
#define ORLIX_TCTI_NATIVE_WIRE_SEAL_VERSION 1U
/* op0/op1/CRn/CRm, write, SME PSTATE, streaming, PSTATE, TPIDR_EL0, FPMR */
#define ORLIX_TCTI_NATIVE_CAPTURE_SYSTEM_CONTROL_BYTES 40U

enum orlix_tcti_native_subject_kind {
	ORLIX_TCTI_NATIVE_SUBJECT_SOURCE_LEAF = 1,
	ORLIX_TCTI_NATIVE_SUBJECT_SEMANTIC_VARIANT,
};

enum orlix_tcti_native_source_branch {
	ORLIX_TCTI_NATIVE_SOURCE_BRANCH_UNCONDITIONAL = 1,
	ORLIX_TCTI_NATIVE_SOURCE_BRANCH_TRUE,
	ORLIX_TCTI_NATIVE_SOURCE_BRANCH_FALSE,
	ORLIX_TCTI_NATIVE_SOURCE_BRANCH_CASE,
};

enum orlix_tcti_native_source_classification {
	ORLIX_TCTI_NATIVE_SOURCE_CLASS_UNCLASSIFIED = 1,
	ORLIX_TCTI_NATIVE_SOURCE_CLASS_REQUIRED_EL0,
	ORLIX_TCTI_NATIVE_SOURCE_CLASS_NON_EL0,
	ORLIX_TCTI_NATIVE_SOURCE_CLASS_ARCH_UNDEFINED,
};

enum orlix_tcti_native_runtime_profile {
	ORLIX_TCTI_NATIVE_RUNTIME_PROFILE_DEVELOPMENT = 1,
	ORLIX_TCTI_NATIVE_RUNTIME_PROFILE_RELEASE,
};

enum orlix_tcti_native_observation_kind {
	ORLIX_TCTI_NATIVE_OBSERVATION_DECODE = 1,
	ORLIX_TCTI_NATIVE_OBSERVATION_LEGAL_ENCODING,
	ORLIX_TCTI_NATIVE_OBSERVATION_REJECTED_ENCODING,
	ORLIX_TCTI_NATIVE_OBSERVATION_RESULT,
	ORLIX_TCTI_NATIVE_OBSERVATION_GPR,
	ORLIX_TCTI_NATIVE_OBSERVATION_PC,
	ORLIX_TCTI_NATIVE_OBSERVATION_PSTATE,
	ORLIX_TCTI_NATIVE_OBSERVATION_MEMORY,
	ORLIX_TCTI_NATIVE_OBSERVATION_FAULT,
	ORLIX_TCTI_NATIVE_OBSERVATION_FP_SIMD,
	ORLIX_TCTI_NATIVE_OBSERVATION_SVE,
	ORLIX_TCTI_NATIVE_OBSERVATION_SME_TASK_STATE,
	ORLIX_TCTI_NATIVE_OBSERVATION_ATOMICITY,
	ORLIX_TCTI_NATIVE_OBSERVATION_ORDERING,
	ORLIX_TCTI_NATIVE_OBSERVATION_ARITHMETIC,
	ORLIX_TCTI_NATIVE_OBSERVATION_SYSTEM_PT_CONTROL,
	ORLIX_TCTI_NATIVE_OBSERVATION_OPERATIONAL_NOTE,
	ORLIX_TCTI_NATIVE_OBSERVATION_LINUX_INTERFACE,
};

enum orlix_tcti_native_encoding_disposition {
	ORLIX_TCTI_NATIVE_ENCODING_LEGAL = 1,
	ORLIX_TCTI_NATIVE_ENCODING_REJECTED,
	ORLIX_TCTI_NATIVE_ENCODING_RESERVED,
};

enum orlix_tcti_native_memory_effect {
	ORLIX_TCTI_NATIVE_MEMORY_UNCHANGED = 1,
	ORLIX_TCTI_NATIVE_MEMORY_READ,
	ORLIX_TCTI_NATIVE_MEMORY_WRITE,
	ORLIX_TCTI_NATIVE_MEMORY_READ_MODIFY_WRITE,
};

enum orlix_tcti_native_state_disposition {
	ORLIX_TCTI_NATIVE_STATE_CAPTURED = 1,
	ORLIX_TCTI_NATIVE_STATE_NOT_APPLICABLE,
	ORLIX_TCTI_NATIVE_STATE_UNAVAILABLE,
	ORLIX_TCTI_NATIVE_STATE_PRIVILEGED,
};

enum orlix_tcti_native_linux_disposition {
	ORLIX_TCTI_NATIVE_LINUX_VISIBLE_EXECUTED = 1,
	ORLIX_TCTI_NATIVE_LINUX_NOT_APPLICABLE,
};

enum orlix_tcti_native_capture_selector_kind {
	ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_RESULT = 1,
	ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_GPR,
	ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_MEMORY,
	ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_TASK_STATE,
	ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_DECODED_FIELD,
	ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_PSTATE,
	ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_FP_SIMD,
	ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_SVE,
	ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_SME,
	ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_FAULT,
	ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_ATOMICITY,
	ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_ORDERING,
	ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_ARITHMETIC,
	ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_SYSTEM_CONTROL,
	ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_LINUX_INTERFACE,
};

enum orlix_tcti_native_capture_size_rule {
	ORLIX_TCTI_NATIVE_CAPTURE_SIZE_EXACT = 1,
	ORLIX_TCTI_NATIVE_CAPTURE_SIZE_ACCESS_WIDTH,
	ORLIX_TCTI_NATIVE_CAPTURE_SIZE_VECTOR_LENGTH,
	ORLIX_TCTI_NATIVE_CAPTURE_SIZE_STREAMING_VECTOR_SQUARE,
	ORLIX_TCTI_NATIVE_CAPTURE_SIZE_DECLARATION_DERIVED,
};

enum orlix_tcti_native_capture_selector_flags {
	ORLIX_TCTI_NATIVE_CAPTURE_BEFORE = 1U << 0,
	ORLIX_TCTI_NATIVE_CAPTURE_AFTER = 1U << 1,
	ORLIX_TCTI_NATIVE_CAPTURE_REQUIRED = 1U << 2,
	ORLIX_TCTI_NATIVE_CAPTURE_APPLICABLE = 1U << 3,
};

/* Immutable, family-owned authorization.  A direct leaf has variant fields 0. */
struct orlix_tcti_native_capture_selector {
	orlix_tcti_proof_u32 kind;
	orlix_tcti_proof_u32 selector_id;
	orlix_tcti_proof_u32 source;
	orlix_tcti_proof_u32 size_rule;
	orlix_tcti_proof_u32 flags;
};

struct orlix_tcti_native_capture_declaration {
	orlix_tcti_proof_u32 schema_version;
	orlix_tcti_proof_u64 declaration_identity;
	orlix_tcti_proof_u32 source_ordinal;
	orlix_tcti_proof_u64 semantic_variant_identity;
	orlix_tcti_proof_u32 selector;
	orlix_tcti_proof_u32 direction;
	orlix_tcti_proof_u64 condition_identity;
	orlix_tcti_proof_u32 obligation;
	orlix_tcti_proof_u32 required_record_kind;
	orlix_tcti_proof_u32 selector_count;
	const struct orlix_tcti_native_capture_selector *selectors;
};

/* A sealed native record is private to capture and ingestion. */
struct orlix_tcti_native_wire_record;

struct orlix_tcti_native_source_identity {
	orlix_tcti_proof_u32 subject_kind;
	orlix_tcti_proof_u32 classification;
	orlix_tcti_proof_u8 classification_present;
	orlix_tcti_proof_u32 source_ordinal;
	orlix_tcti_proof_u32 source_index;
	orlix_tcti_proof_u32 secondary_index;
	orlix_tcti_proof_u32 tertiary_index;
	orlix_tcti_proof_u32 semantic_variant_ordinal;
	orlix_tcti_proof_u32 variant_direction;
	orlix_tcti_proof_u32 variant_disposition;
	orlix_tcti_proof_u64 semantic_variant_identity;
	orlix_tcti_proof_u32 encoding_mask;
	orlix_tcti_proof_u32 encoding_pattern;
	orlix_tcti_proof_u32 access_expression;
	orlix_tcti_proof_u32 selector_count;
	orlix_tcti_proof_u32 condition_expression;
	orlix_tcti_proof_u32 concrete_selector;
	orlix_tcti_proof_u32 accessor_applicability;
	orlix_tcti_proof_u32 accessor_semantics;
	orlix_tcti_proof_u32 accessor_implementation;
	orlix_tcti_proof_u32 accessor_proof_state;
	orlix_tcti_proof_u64 source_identity;
	orlix_tcti_proof_u64 source_offset;
	orlix_tcti_proof_u64 source_length;
	orlix_tcti_proof_u64 secondary_offset;
	orlix_tcti_proof_u64 secondary_length;
	orlix_tcti_proof_u64 condition_identity;
	orlix_tcti_proof_u64 condition_offset;
	orlix_tcti_proof_u64 condition_length;
	orlix_tcti_proof_u64 access_identity;
	orlix_tcti_proof_u64 access_offset;
	orlix_tcti_proof_u64 access_length;
	orlix_tcti_proof_u32 source_branch;
	orlix_tcti_proof_u32 source_branch_ordinal;
	char leaf_name[ORLIX_TCTI_NATIVE_TEXT_MAX];
	char mnemonic[ORLIX_TCTI_NATIVE_TEXT_MAX];
	char operation_id[ORLIX_TCTI_NATIVE_TEXT_MAX];
	char semantic_variant[ORLIX_TCTI_NATIVE_TEXT_MAX];
	char condition_tcnd_hex[ORLIX_TCTI_NATIVE_CONDITION_HEX_MAX];
	char artifact_architecture[64];
	char artifact_build[64];
	char artifact_reference[64];
	char artifact_schema[64];
	char artifact_source_sha256[ORLIX_TCTI_NATIVE_SHA256_HEX_SIZE];
	char accessor_kunit_suite[ORLIX_TCTI_NATIVE_TEXT_MAX];
	char accessor_kunit_case[ORLIX_TCTI_NATIVE_TEXT_MAX];
};

struct orlix_tcti_native_semantic_provenance {
	char ddi0602_locator[ORLIX_TCTI_NATIVE_TEXT_MAX];
	char ddi0602_sha256[ORLIX_TCTI_NATIVE_SHA256_HEX_SIZE];
	orlix_tcti_proof_u32 classification_mask;
	char implementation_owner[ORLIX_TCTI_NATIVE_TEXT_MAX];
	char decoder_owner[ORLIX_TCTI_NATIVE_TEXT_MAX];
	char lowering_owner[ORLIX_TCTI_NATIVE_TEXT_MAX];
};

struct orlix_tcti_native_execution_provenance {
	char proof_id[ORLIX_TCTI_NATIVE_TEXT_MAX];
	char kunit_source[ORLIX_TCTI_NATIVE_TEXT_MAX];
	char kunit_source_sha256[ORLIX_TCTI_NATIVE_SHA256_HEX_SIZE];
	char kunit_build_source[ORLIX_TCTI_NATIVE_TEXT_MAX];
	char kunit_build_source_sha256[ORLIX_TCTI_NATIVE_SHA256_HEX_SIZE];
	char kunit_suite[ORLIX_TCTI_NATIVE_TEXT_MAX];
	char kunit_case[ORLIX_TCTI_NATIVE_TEXT_MAX];
	char executing_kernel_identity[ORLIX_TCTI_NATIVE_BUILD_ID_MAX];
	char executing_instruction_build_identity[ORLIX_TCTI_NATIVE_BUILD_ID_MAX];
	char kernel_archive_input_sha256[ORLIX_TCTI_NATIVE_SHA256_HEX_SIZE];
	char kernel_config_sha256[ORLIX_TCTI_NATIVE_SHA256_HEX_SIZE];
	char build_profile_sha256[ORLIX_TCTI_NATIVE_SHA256_HEX_SIZE];
	char durable_source_revision[ORLIX_TCTI_NATIVE_SHA256_HEX_SIZE];
	char instruction_artifact_sha256[ORLIX_TCTI_NATIVE_SHA256_HEX_SIZE];
	orlix_tcti_proof_u64 instruction_artifact_identity;
	orlix_tcti_proof_u32 runtime_profile;
};

/*
 * Canonical per-leaf, per-variant, per-obligation registration. It contains
 * no execution result and therefore grants no credit by itself. Family
 * cohorts pass their exact rows through the ingestion selector without
 * changing this common schema.
 */
struct orlix_tcti_native_proof_registry_entry {
	struct orlix_tcti_native_source_identity source;
	struct orlix_tcti_native_semantic_provenance semantics;
	char proof_id[ORLIX_TCTI_NATIVE_TEXT_MAX];
	char kunit_source[ORLIX_TCTI_NATIVE_TEXT_MAX];
	char kunit_source_sha256[ORLIX_TCTI_NATIVE_SHA256_HEX_SIZE];
	char kunit_build_source[ORLIX_TCTI_NATIVE_TEXT_MAX];
	char kunit_build_source_sha256[ORLIX_TCTI_NATIVE_SHA256_HEX_SIZE];
	char kunit_suite[ORLIX_TCTI_NATIVE_TEXT_MAX];
	char kunit_case[ORLIX_TCTI_NATIVE_TEXT_MAX];
	char kernel_archive_input_sha256[ORLIX_TCTI_NATIVE_SHA256_HEX_SIZE];
	char kernel_config_sha256[ORLIX_TCTI_NATIVE_SHA256_HEX_SIZE];
	char build_profile_sha256[ORLIX_TCTI_NATIVE_SHA256_HEX_SIZE];
	char durable_source_revision[ORLIX_TCTI_NATIVE_SHA256_HEX_SIZE];
	char instruction_artifact_sha256[ORLIX_TCTI_NATIVE_SHA256_HEX_SIZE];
	orlix_tcti_proof_u64 instruction_artifact_identity;
	orlix_tcti_proof_u32 runtime_profile;
	orlix_tcti_proof_u32 observation_kind;
	orlix_tcti_proof_u32 obligation;
	const struct orlix_tcti_native_capture_declaration *capture_declaration;
	orlix_tcti_proof_u8 production_capture;
	orlix_tcti_proof_u8 static_obligation;
};

/*
 * A lookup key selects one immutable canonical row. It carries no source,
 * semantic, owner, span, DDI, artifact, or build attribution.
 */
struct orlix_tcti_native_proof_key {
	orlix_tcti_proof_u32 source_ordinal;
	orlix_tcti_proof_u64 semantic_variant_identity;
	orlix_tcti_proof_u32 obligation;
	const char *proof_id;
	const char *kunit_case;
};

enum orlix_tcti_native_contract_error {
	ORLIX_TCTI_NATIVE_CONTRACT_OK,
	ORLIX_TCTI_NATIVE_CONTRACT_INVALID,
	ORLIX_TCTI_NATIVE_CONTRACT_SOURCE_MISMATCH,
	ORLIX_TCTI_NATIVE_CONTRACT_VARIANT_MISMATCH,
	ORLIX_TCTI_NATIVE_CONTRACT_FAMILY_MISMATCH,
	ORLIX_TCTI_NATIVE_CONTRACT_CLASSIFICATION_MISMATCH,
	ORLIX_TCTI_NATIVE_CONTRACT_CONDITION_MISMATCH,
	ORLIX_TCTI_NATIVE_CONTRACT_DDI_MISMATCH,
	ORLIX_TCTI_NATIVE_CONTRACT_BUILD_MISMATCH,
	ORLIX_TCTI_NATIVE_CONTRACT_PROFILE_MISMATCH,
	ORLIX_TCTI_NATIVE_CONTRACT_SUITE_MISMATCH,
	ORLIX_TCTI_NATIVE_CONTRACT_CASE_MISMATCH,
	ORLIX_TCTI_NATIVE_CONTRACT_OBLIGATION_MISMATCH,
	ORLIX_TCTI_NATIVE_CONTRACT_TYPE_SUBSTITUTION,
	ORLIX_TCTI_NATIVE_CONTRACT_UNKNOWN,
};

int orlix_tcti_native_source_identity_equal(
	const struct orlix_tcti_native_source_identity *left,
	const struct orlix_tcti_native_source_identity *right);
int orlix_tcti_native_proof_registry_validate(
	const struct orlix_tcti_native_proof_registry_entry *entries,
	size_t count, enum orlix_tcti_native_contract_error *error);
const struct orlix_tcti_native_proof_registry_entry *
orlix_tcti_native_proof_registry_entries(size_t *count);
const struct orlix_tcti_native_proof_registry_entry *
orlix_tcti_native_proof_registry_lookup(
	const struct orlix_tcti_native_proof_key *key,
	enum orlix_tcti_native_contract_error *error);
int orlix_tcti_native_proof_registry_resolve_production(
	const void *capture_token,
	orlix_tcti_proof_u32 source_ordinal,
	orlix_tcti_proof_u64 semantic_variant_identity,
	orlix_tcti_proof_u32 obligation,
	const struct orlix_tcti_native_proof_registry_entry **entry,
	enum orlix_tcti_native_contract_error *error);
int orlix_tcti_native_proof_registry_capture_token_semantic_variant_identity(
	const void *capture_token,
	orlix_tcti_proof_u64 *semantic_variant_identity);
orlix_tcti_proof_u64 orlix_tcti_native_proof_registry_entry_identity(
	const struct orlix_tcti_native_proof_registry_entry *entry);
orlix_tcti_proof_u64 orlix_tcti_native_capture_declaration_identity(
	const struct orlix_tcti_native_capture_declaration *declaration);
int orlix_tcti_native_capture_declaration_validate(
	const struct orlix_tcti_native_capture_declaration *declaration);
#endif /* ORLIX_TCTI_TARGET_NATIVE_PROOF_CONTRACT_H */
