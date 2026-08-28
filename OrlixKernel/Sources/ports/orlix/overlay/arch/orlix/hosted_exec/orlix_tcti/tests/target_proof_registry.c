/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_proof_registry.h"
#include "target_proof_registry_provenance.h"
#include "target_proof_ingestion.h"
#include "target_instruction_artifact.h"

#ifdef __KERNEL__
#include <linux/ctype.h>
#include <linux/kernel.h>
#include <linux/limits.h>
#include <linux/mutex.h>
#include <linux/string.h>
#else
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

#define ORLIX_TCTI_OPERATIONAL_NOTE_INDEX_NONE ((size_t)-1)

static void operational_note_mapping_fail(
	struct orlix_tcti_target_operational_note_mapping_result *result,
	enum orlix_tcti_target_operational_note_mapping_error error,
	size_t note_index, size_t mapping_index)
{
	if (result)
		*result = (struct orlix_tcti_target_operational_note_mapping_result) {
			.error = error,
			.note_index = note_index,
			.mapping_index = mapping_index,
			.mapped_count = 0,
		};
}

static const char *operational_note_identity(
	const struct orlix_tcti_target_instruction_artifact *artifact,
	size_t note_index)
{
	u32 offset = artifact->operational_notes[note_index].source_identity_offset;
	const u8 *nul;

	if (offset >= artifact->string_pool_size)
		return NULL;
	nul = memchr(artifact->string_pool + offset, '\0',
		artifact->string_pool_size - offset);
	return nul && nul != artifact->string_pool + offset ?
		(const char *)artifact->string_pool + offset : NULL;
}

static const char *operational_note_digest(
	const struct orlix_tcti_target_instruction_artifact *artifact,
	size_t note_index)
{
	u32 offset = artifact->operational_notes[note_index].source_sha256_offset;
	const u8 *nul;

	if (offset >= artifact->string_pool_size)
		return NULL;
	nul = memchr(artifact->string_pool + offset, '\0',
		artifact->string_pool_size - offset);
	return nul && nul != artifact->string_pool + offset ?
		(const char *)artifact->string_pool + offset : NULL;
}

const struct orlix_tcti_target_operational_note_proof_mapping *
orlix_tcti_target_operational_note_proof_mappings(size_t *count)
{
	if (count)
		*count = 0;
	return NULL;
}

int orlix_tcti_target_operational_note_proof_mappings_validate(
	const struct orlix_tcti_target_instruction_artifact *artifact,
	const struct orlix_tcti_target_operational_note_proof_mapping *mappings,
	size_t mapping_count,
	const struct orlix_tcti_target_proof_registry_entry *registry,
	size_t registry_count,
	struct orlix_tcti_target_operational_note_mapping_result *result)
{
	size_t note_index;
	size_t mapping_index;
	size_t mapped_count = 0;

	operational_note_mapping_fail(result,
		ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_MAPPING_OK,
		ORLIX_TCTI_OPERATIONAL_NOTE_INDEX_NONE,
		ORLIX_TCTI_OPERATIONAL_NOTE_INDEX_NONE);
	if (!artifact || (mapping_count && !mappings) ||
	    (registry_count && !registry)) {
		operational_note_mapping_fail(result,
			ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_MAPPING_MALFORMED,
			ORLIX_TCTI_OPERATIONAL_NOTE_INDEX_NONE,
			ORLIX_TCTI_OPERATIONAL_NOTE_INDEX_NONE);
		return -1;
	}
	for (note_index = 0; note_index < artifact->operational_note_count;
	     note_index++) {
		const struct orlix_tcti_target_instruction_artifact_operational_note *note =
			&artifact->operational_notes[note_index];
		const char *identity = operational_note_identity(artifact, note_index);
		const char *digest = operational_note_digest(artifact, note_index);
		size_t exact_count = 0;
		size_t partial_count = 0;
		size_t exact_index = ORLIX_TCTI_OPERATIONAL_NOTE_INDEX_NONE;

		if (!identity || !digest) {
			operational_note_mapping_fail(result,
				ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_MAPPING_MALFORMED,
				note_index, ORLIX_TCTI_OPERATIONAL_NOTE_INDEX_NONE);
			return -1;
		}
		for (mapping_index = 0; mapping_index < mapping_count; mapping_index++) {
			const struct orlix_tcti_target_operational_note_proof_mapping *mapping =
				&mappings[mapping_index];
			bool same_leaf = mapping->leaf_index == note->leaf_index;
			bool same_identity = mapping->source_identity &&
				!strcmp(mapping->source_identity, identity);

			if (same_leaf && same_identity) {
				exact_count++;
				exact_index = mapping_index;
			} else if (same_leaf || same_identity) {
				partial_count++;
			}
		}
		if (exact_count > 1U) {
			operational_note_mapping_fail(result,
				ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_MAPPING_DUPLICATE,
				note_index, exact_index);
			return -1;
		}
		if (!exact_count) {
			operational_note_mapping_fail(result, partial_count ?
				ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_MAPPING_AMBIGUOUS :
				ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_MAPPING_MISSING,
				note_index, ORLIX_TCTI_OPERATIONAL_NOTE_INDEX_NONE);
			return -1;
		}
		if (!mappings[exact_index].source_sha256 ||
		    strcmp(mappings[exact_index].source_sha256, digest)) {
			operational_note_mapping_fail(result,
				ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_MAPPING_DIGEST_MISMATCH,
				note_index, exact_index);
			return -1;
		}
		mapped_count++;
	}
	for (mapping_index = 0; mapping_index < mapping_count; mapping_index++) {
		const struct orlix_tcti_target_operational_note_proof_mapping *mapping =
			&mappings[mapping_index];
		const struct orlix_tcti_target_proof_registry_entry *entry = NULL;
		size_t registry_index;
		size_t case_index;
		size_t selected_case = ORLIX_TCTI_OPERATIONAL_NOTE_INDEX_NONE;
		size_t case_count = 0;
		size_t proof_count = 0;
		size_t binding_index;
		size_t binding_count = 0;
		const struct orlix_tcti_target_proof_binding *binding = NULL;
		bool source_exists = false;

		if (!mapping->source_identity || !mapping->source_sha256 ||
		    !mapping->proof_id ||
		    !mapping->kunit_case_name) {
			operational_note_mapping_fail(result,
				ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_MAPPING_MALFORMED,
				ORLIX_TCTI_OPERATIONAL_NOTE_INDEX_NONE, mapping_index);
			return -1;
		}
		for (note_index = 0; note_index < artifact->operational_note_count;
		     note_index++)
			if (mapping->leaf_index == artifact->operational_notes[note_index].leaf_index &&
			    !strcmp(mapping->source_identity,
				operational_note_identity(artifact, note_index)))
				source_exists = true;
		if (!source_exists) {
			operational_note_mapping_fail(result,
				ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_MAPPING_STALE,
				ORLIX_TCTI_OPERATIONAL_NOTE_INDEX_NONE, mapping_index);
			return -1;
		}
		for (registry_index = 0; registry_index < registry_count; registry_index++)
			if (!strcmp(registry[registry_index].id, mapping->proof_id)) {
				entry = &registry[registry_index];
				proof_count++;
			}
		if (!entry) {
			operational_note_mapping_fail(result,
				ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_MAPPING_UNKNOWN_PROOF,
				ORLIX_TCTI_OPERATIONAL_NOTE_INDEX_NONE, mapping_index);
			return -1;
		}
		if (proof_count != 1U) {
			operational_note_mapping_fail(result,
				ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_MAPPING_AMBIGUOUS,
				ORLIX_TCTI_OPERATIONAL_NOTE_INDEX_NONE, mapping_index);
			return -1;
		}
		if ((entry->binding_count && !entry->bindings) ||
		    (entry->kunit_case_count && !entry->kunit_cases)) {
			operational_note_mapping_fail(result,
				ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_MAPPING_MALFORMED,
				ORLIX_TCTI_OPERATIONAL_NOTE_INDEX_NONE, mapping_index);
			return -1;
		}
		for (case_index = 0; case_index < entry->kunit_case_count; case_index++)
			if (!strcmp(entry->kunit_cases[case_index].name,
				    mapping->kunit_case_name)) {
				case_count++;
				selected_case = case_index;
			}
		if (case_count != 1U) {
			operational_note_mapping_fail(result,
				case_count ? ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_MAPPING_AMBIGUOUS :
				ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_MAPPING_UNKNOWN_NATIVE_CASE,
				ORLIX_TCTI_OPERATIONAL_NOTE_INDEX_NONE, mapping_index);
			return -1;
		}
		if (!(entry->obligations &
		      ORLIX_TCTI_TARGET_PROOF_OBLIGATION_OPERATIONAL_NOTE) ||
		    !(entry->kunit_cases[selected_case].obligations &
		      ORLIX_TCTI_TARGET_PROOF_OBLIGATION_OPERATIONAL_NOTE)) {
			operational_note_mapping_fail(result,
				ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_MAPPING_INSUFFICIENT_OBLIGATIONS,
				ORLIX_TCTI_OPERATIONAL_NOTE_INDEX_NONE, mapping_index);
			return -1;
		}
		if (selected_case >= sizeof(orlix_tcti_proof_u64) * 8U) {
			operational_note_mapping_fail(result,
				ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_MAPPING_MALFORMED,
				ORLIX_TCTI_OPERATIONAL_NOTE_INDEX_NONE, mapping_index);
			return -1;
		}
		for (binding_index = 0; binding_index < entry->binding_count;
		     binding_index++)
			if (entry->bindings[binding_index].source_ordinal ==
			    mapping->leaf_index) {
				binding = &entry->bindings[binding_index];
				binding_count++;
			}
		if (binding_count != 1U) {
			operational_note_mapping_fail(result, binding_count ?
				ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_MAPPING_AMBIGUOUS :
				ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_MAPPING_BINDING_MISMATCH,
				ORLIX_TCTI_OPERATIONAL_NOTE_INDEX_NONE, mapping_index);
			return -1;
		}
		if (!(binding->kunit_case_mask &
		      (ORLIX_TCTI_PROOF_U64_C(1) << selected_case))) {
			operational_note_mapping_fail(result,
				ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_MAPPING_BINDING_MISMATCH,
				ORLIX_TCTI_OPERATIONAL_NOTE_INDEX_NONE, mapping_index);
			return -1;
		}
	}
	if (result)
		result->mapped_count = mapped_count;
	return 0;
}

#define KNOWN_CLASS_MASK \
	(ORLIX_TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0 | \
	 ORLIX_TCTI_TARGET_PROOF_CLASS_NON_EL0 | \
	 ORLIX_TCTI_TARGET_PROOF_CLASS_ARCH_UNDEFINED_OR_UNALLOCATED)
#define KNOWN_OBLIGATION_MASK \
	(ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ATOMICITY | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ORDERING | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LINUX_INTERFACE | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_OPERATIONAL_NOTE | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD)
#define BASELINE_OBLIGATIONS \
	(ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS)
#define NON_EL0_REJECTION_OBLIGATIONS \
	(BASELINE_OBLIGATIONS | ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY | ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS)
#define UNDEFINED_REJECTION_OBLIGATIONS \
	(ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY | ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS)
#define ORLIX_TCTI_TARGET_PROOF_MAX_ENTRIES 4350U
#define ORLIX_TCTI_TARGET_PROOF_MAX_BINDINGS 4350U
#define ORLIX_TCTI_TARGET_PROOF_MAX_SOURCE_BYTES (2U * 1024U * 1024U)
#define ORLIX_TCTI_TARGET_PROOF_MAX_SEMANTIC_PROVENANCE_ARTIFACT_BYTES \
	(4U * 1024U * 1024U)
#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

struct operation_requirements {
	const char *operation_id;
	orlix_tcti_proof_u32 obligations;
};

#define LSE_REQUIRED_OBLIGATIONS \
	(BASELINE_OBLIGATIONS | ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ATOMICITY | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ORDERING)
#define BASE_ATOMIC_REQUIRED_OBLIGATIONS LSE_REQUIRED_OBLIGATIONS
#define BASE_ATOMIC_FLAGS_REQUIRED_OBLIGATIONS \
	(BASE_ATOMIC_REQUIRED_OBLIGATIONS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS)
#define EXCLUSIVE_REQUIRED_OBLIGATIONS LSE_REQUIRED_OBLIGATIONS

struct kunit_source_provenance {
	const char *source;
	const char *sha256;
	const char *object;
	const char *dependency;
	const char *dependency_sha256;
	const char *include_directive;
};

struct kunit_dependency_terminal_artifact {
	const char *source;
	const char *name;
	const char *path;
};

struct kunit_case_provenance {
	const char *source;
	const char *suite;
	const char *suite_symbol;
	const char *case_array;
	const char *name;
	orlix_tcti_proof_u32 maximum_obligations;
};

/* Immutable identity and decode tuple from the pinned AARCHMRS source. */
struct source_manifest_binding {
	orlix_tcti_proof_u32 ordinal;
	const char *leaf_name;
	const char *mnemonic;
	const char *operation_id;
	orlix_tcti_proof_u32 encoding_mask;
	orlix_tcti_proof_u32 encoding_pattern;
	const char *condition_tcnd_hex;
	orlix_tcti_proof_u64 source_offset;
	orlix_tcti_proof_u64 source_length;
};

#define ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(...) \
	/* The source provenance header is consumed by the owning audit. */
#define ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, leaf, mnemonic, operation, \
					     mask, pattern, condition, offset, length) \
	{ ordinal, leaf, mnemonic, operation, mask, pattern, condition, offset, length },
static const struct source_manifest_binding source_manifest_bindings[] = {
#include "../isa/source_manifest.def"
};
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE

struct system_accessor_binding {
	orlix_tcti_proof_u32 accessor_index;
	orlix_tcti_proof_u32 encoding_index;
	const char *name;
	const char *variant_name;
	const char *generic_leaf;
	orlix_tcti_proof_u32 direction;
	orlix_tcti_proof_u32 disposition;
	orlix_tcti_proof_u32 selector_count;
	orlix_tcti_proof_u32 condition_expression;
	orlix_tcti_proof_u32 access_expression;
	orlix_tcti_proof_u32 concrete_selector;
	enum orlix_tcti_target_system_accessor_applicability applicability;
	enum orlix_tcti_target_system_accessor_semantics semantics;
	enum orlix_tcti_target_system_accessor_implementation implementation;
	enum orlix_tcti_target_system_accessor_proof_state proof_state;
	orlix_tcti_proof_u64 selector_identity;
	orlix_tcti_proof_u64 condition_identity;
	orlix_tcti_proof_u64 access_identity;
	const char *decoder_owner;
	const char *execution_owner;
	const char *kunit_suite;
	const char *kunit_case;
	orlix_tcti_proof_u64 accessor_source_offset;
	orlix_tcti_proof_u64 accessor_source_length;
	orlix_tcti_proof_u64 encoding_source_offset;
	orlix_tcti_proof_u64 encoding_source_length;
	orlix_tcti_proof_u64 condition_source_offset;
	orlix_tcti_proof_u64 condition_source_length;
	orlix_tcti_proof_u64 access_source_offset;
	orlix_tcti_proof_u64 access_source_length;
};

#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR_SOURCE(...)
#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR_COUNTS(...)
#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR_SEMANTIC_COUNTS(...)
#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR_IDENTITY(...)
#define ORLIX_TCTI_A64_SYSTEM_ACCESSOR(accessor, encoding, name, variant, generic, \
		direction, disposition, selectors, condition, access, concrete, \
		applicability, semantics, implementation, proof, selector_identity, \
		condition_identity, access_identity, decode_key, operation, decoder, executor, suite, test_case, \
		accessor_offset, accessor_length, encoding_offset, encoding_length, \
		condition_offset, condition_length, access_offset, access_length) \
	{ accessor, encoding, name, variant, generic, direction, disposition, selectors, \
	  condition, access, concrete, applicability, semantics, implementation, proof, \
	  selector_identity, condition_identity, access_identity, decoder, executor, \
	  suite, test_case, accessor_offset, accessor_length, encoding_offset, \
	  encoding_length, condition_offset, condition_length, access_offset, \
	  access_length },
static const struct system_accessor_binding system_accessor_bindings[] = {
#ifdef __KERNEL__
#define UINT64_C(value) value##ULL
#endif
#include "../isa/target_system_accessor_reconciliation.def"
#ifdef __KERNEL__
#undef UINT64_C
#endif
};
#undef ORLIX_TCTI_A64_SYSTEM_ACCESSOR
#undef ORLIX_TCTI_A64_SYSTEM_ACCESSOR_SEMANTIC_COUNTS
#undef ORLIX_TCTI_A64_SYSTEM_ACCESSOR_IDENTITY
#undef ORLIX_TCTI_A64_SYSTEM_ACCESSOR_COUNTS
#undef ORLIX_TCTI_A64_SYSTEM_ACCESSOR_SOURCE

_Static_assert(ARRAY_COUNT(source_manifest_bindings) ==
		       ORLIX_TCTI_TARGET_LINUX_PROOF_SOURCE_ROWS,
	       "Linux proof matrix requires all 4,350 source leaves");
_Static_assert(ARRAY_COUNT(system_accessor_bindings) ==
		       ORLIX_TCTI_TARGET_LINUX_PROOF_VARIANT_ROWS,
	       "Linux proof matrix requires all 2,014 retained variants");

#define ORLIX_TCTI_A64_SOURCE_BOUND_PROOF(ordinal, proof_id) \
	{ ordinal, proof_id },
static const struct orlix_tcti_target_proof_registry_projection_binding
source_bound_proofs[] = {
#include "../isa/source_bound_proof.def"
};
#undef ORLIX_TCTI_A64_SOURCE_BOUND_PROOF

/* Canonical typed X-macro input for registry and publisher consumers. */
#define ORLIX_TCTI_A64_PROOF_REGISTRY_BINDING(ordinal, proof_id) \
	{ ordinal, proof_id },
static const struct orlix_tcti_target_proof_registry_projection_binding
proof_registry_projection[] = {
#include "../isa/proof_registry_projection.def"
};
#undef ORLIX_TCTI_A64_PROOF_REGISTRY_BINDING


#define LSE_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_lse_decode_test.c"
#define LSE_SOURCE_BOUND_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_lse_source_bound_test.c"
#define LSE_SOURCE_BOUND_SUITE "orlix-tcti-lse-source-bound"
#define LSE_SOURCE_BOUND_SUITE_SYMBOL "orlix_tcti_lse_source_bound_test_suite"
#define LSE_SOURCE_BOUND_CASE_ARRAY "orlix_tcti_lse_source_bound_test_cases"
#define LSE128_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_lse128_resume_test.c"
#define LSE128_SUITE "orlix-tcti-lse128-resume"
#define LSE128_SUITE_SYMBOL "orlix_tcti_lse128_resume_test_suite"
#define LSE128_CASE_ARRAY "orlix_tcti_lse128_resume_test_cases"
#define BASE_ATOMIC_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_base_atomic_source_bound_test.c"
#define BASE_ATOMIC_SUITE "orlix-tcti-base-atomic-source-bound"
#define BASE_ATOMIC_SUITE_SYMBOL \
	"orlix_tcti_base_atomic_source_bound_suite"
#define BASE_ATOMIC_CASE_ARRAY \
	"orlix_tcti_base_atomic_source_bound_cases"
#define LOGICAL_SHIFT_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_logical_shifted_register_test.c"
#define LOGICAL_SHIFT_SUITE "orlix-tcti-logical-shifted-register"
#define LOGICAL_SHIFT_SUITE_SYMBOL "orlix_tcti_logical_shifted_register_test_suite"
#define LOGICAL_SHIFT_CASE_ARRAY "orlix_tcti_logical_shifted_register_test_cases"
#define CSSC_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_cssc_min_max_immediate_test.c"
#define CSSC_SUITE "orlix-tcti-cssc-min-max-immediate"
#define CSSC_SUITE_SYMBOL "orlix_tcti_cssc_min_max_immediate_test_suite"
#define CSSC_CASE_ARRAY "orlix_tcti_cssc_min_max_immediate_test_cases"
#define ADD_SUB_IMMEDIATE_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_add_sub_immediate_test.c"
#define ADD_SUB_IMMEDIATE_SUITE "orlix-tcti-add-sub-immediate"
#define ADD_SUB_IMMEDIATE_SUITE_SYMBOL "orlix_tcti_add_sub_immediate_test_suite"
#define ADD_SUB_IMMEDIATE_CASE_ARRAY "orlix_tcti_add_sub_immediate_test_cases"
#define LOGICAL_IMMEDIATE_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_logical_immediate_source_bound_test.c"
#define LOGICAL_IMMEDIATE_SUITE "orlix-tcti-logical-immediate-source-bound"
#define LOGICAL_IMMEDIATE_SUITE_SYMBOL \
	"orlix_tcti_logical_immediate_source_bound_test_suite"
#define LOGICAL_IMMEDIATE_CASE_ARRAY \
	"orlix_tcti_logical_immediate_source_bound_test_cases"
#define MOVE_WIDE_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_move_wide_source_bound_test.c"
#define MOVE_WIDE_SUITE "orlix-tcti-move-wide-source-bound"
#define MOVE_WIDE_SUITE_SYMBOL "orlix_tcti_move_wide_source_bound_test_suite"
#define MOVE_WIDE_CASE_ARRAY "orlix_tcti_move_wide_source_bound_test_cases"
#define SCALAR_BITOPS_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_scalar_bitops_source_bound_test.c"
#define SCALAR_BITOPS_SUITE "orlix-tcti-scalar-bitops-source-bound"
#define SCALAR_BITOPS_SUITE_SYMBOL "orlix_tcti_scalar_bitops_source_bound_test_suite"
#define SCALAR_BITOPS_CASE_ARRAY "orlix_tcti_scalar_bitops_source_bound_test_cases"
#define VARIABLE_SHIFT_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_variable_shift_source_bound_test.c"
#define VARIABLE_SHIFT_SUITE "orlix-tcti-variable-shift-source-bound"
#define VARIABLE_SHIFT_SUITE_SYMBOL \
	"orlix_tcti_variable_shift_source_bound_test_suite"
#define VARIABLE_SHIFT_CASE_ARRAY \
	"orlix_tcti_variable_shift_source_bound_test_cases"
#define ADD_SUB_REGISTER_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_add_sub_register_source_bound_test.c"
#define ADD_SUB_REGISTER_SUITE "orlix-tcti-add-sub-register-source-bound"
#define ADD_SUB_REGISTER_SUITE_SYMBOL \
	"orlix_tcti_add_sub_register_source_bound_test_suite"
#define ADD_SUB_REGISTER_CASE_ARRAY "asr_cases"
#define BASE_ADD_SUB_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_base_add_sub_source_bound_test.c"
#define BASE_ADD_SUB_SUITE "orlix-tcti-base-add-sub-source-bound"
#define BASE_ADD_SUB_SUITE_SYMBOL \
	"orlix_tcti_base_add_sub_source_bound_suite"
#define BASE_ADD_SUB_CASE_ARRAY \
	"orlix_tcti_base_add_sub_source_bound_cases"
#define BASE_ADD_SUB_CAPTURE_INCLUDE \
	"#include \"orlix_tcti_base_add_sub_production_capture.h\""
#define BASE_CONTROL_FLOW_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_base_control_flow_source_bound_test.c"
#define BASE_CONTROL_FLOW_SUITE "orlix-tcti-base-control-flow-source-bound"
#define BASE_CONTROL_FLOW_SUITE_SYMBOL \
	"orlix_tcti_base_control_flow_source_bound_suite"
#define BASE_CONTROL_FLOW_CASE_ARRAY \
	"orlix_tcti_base_control_flow_source_bound_cases"
#define BASE_CONTROL_FLOW_CAPTURE_INCLUDE \
	"#include \"orlix_tcti_base_control_flow_production_capture.h\""
#define BASE_EXCEPTIONS_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_base_exceptions_source_bound_test.c"
#define BASE_EXCEPTIONS_SUITE "orlix-tcti-base-exceptions-source-bound"
#define BASE_EXCEPTIONS_SUITE_SYMBOL \
	"orlix_tcti_base_exceptions_source_bound_suite"
#define BASE_EXCEPTIONS_CASE_ARRAY \
	"orlix_tcti_base_exceptions_source_bound_cases"
#define BASE_EXCEPTIONS_CAPTURE_INCLUDE \
	"#include \"orlix_tcti_base_exceptions_production_capture.h\""
#define BASE_CONDITIONAL_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_base_conditional_source_bound_test.c"
#define BASE_CONDITIONAL_SUITE "orlix-tcti-base-conditional-source-bound"
#define BASE_CONDITIONAL_SUITE_SYMBOL \
	"orlix_tcti_base_conditional_source_bound_suite"
#define BASE_CONDITIONAL_CASE_ARRAY \
	"orlix_tcti_base_conditional_source_bound_cases"
#define BASE_CONDITIONAL_CAPTURE_INCLUDE \
	"#include \"orlix_tcti_base_conditional_production_capture.h\""
#define BASE_BITFIELD_UNARY_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_base_bitfield_unary_source_bound_test.c"
#define BASE_BITFIELD_UNARY_SUITE "orlix-tcti-base-bitfield-unary-source-bound"
#define BASE_BITFIELD_UNARY_SUITE_SYMBOL \
	"orlix_tcti_base_bitfield_unary_source_bound_suite"
#define BASE_BITFIELD_UNARY_CASE_ARRAY \
	"orlix_tcti_base_bitfield_unary_source_bound_cases"
#define BASE_BITFIELD_UNARY_CAPTURE_INCLUDE \
	"#include \"orlix_tcti_base_bitfield_unary_production_capture.h\""
#define BASE_MULTIPLY_DIVIDE_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_base_multiply_divide_source_bound_test.c"
#define BASE_MULTIPLY_DIVIDE_SUITE "orlix-tcti-base-multiply-divide-source-bound"
#define BASE_MULTIPLY_DIVIDE_SUITE_SYMBOL \
	"orlix_tcti_base_multiply_divide_source_bound_suite"
#define BASE_MULTIPLY_DIVIDE_CASE_ARRAY \
	"orlix_tcti_base_multiply_divide_source_bound_cases"
#define BASE_MULTIPLY_DIVIDE_CAPTURE_INCLUDE \
	"#include \"orlix_tcti_base_multiply_divide_production_capture.h\""
#define ADVSIMD_CRYPTO_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_advsimd_crypto_source_bound_test.c"
#define ADVSIMD_CRYPTO_SUITE "orlix-tcti-advsimd-crypto-source-bound"
#define ADVSIMD_CRYPTO_SUITE_SYMBOL \
	"orlix_tcti_advsimd_crypto_source_bound_suite"
#define ADVSIMD_CRYPTO_CASE_ARRAY \
	"orlix_tcti_advsimd_crypto_source_bound_cases"
#define ADVSIMD_CRYPTO_CAPTURE_INCLUDE \
	"#include \"orlix_tcti_advsimd_crypto_production_capture.h\""
#define ADVSIMD_PERMUTE_MOVE_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_advsimd_permute_move_source_bound_test.c"
#define ADVSIMD_PERMUTE_MOVE_SUITE "orlix-tcti-advsimd-permute-move-source-bound"
#define ADVSIMD_PERMUTE_MOVE_SUITE_SYMBOL \
	"orlix_tcti_advsimd_permute_move_source_bound_suite"
#define ADVSIMD_PERMUTE_MOVE_CASE_ARRAY \
	"orlix_tcti_advsimd_permute_move_source_bound_cases"
#define ADVSIMD_PERMUTE_MOVE_CAPTURE_INCLUDE \
	"#include \"orlix_tcti_advsimd_permute_move_production_capture.h\""
#define ADVSIMD_INTEGER_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_advsimd_integer_source_bound_test.c"
#define ADVSIMD_INTEGER_SUITE "orlix-tcti-advsimd-integer-source-bound"
#define ADVSIMD_INTEGER_SUITE_SYMBOL \
	"orlix_tcti_advsimd_integer_source_bound_suite"
#define ADVSIMD_INTEGER_CASE_ARRAY \
	"orlix_tcti_advsimd_integer_source_bound_cases"
#define ADVSIMD_INTEGER_CAPTURE_INCLUDE \
	"#include \"orlix_tcti_advsimd_integer_production_capture.h\""
#define ADVSIMD_FP_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_advsimd_fp_source_bound_test.c"
#define ADVSIMD_FP_SUITE "orlix-tcti-advsimd-fp-source-bound"
#define ADVSIMD_FP_SUITE_SYMBOL \
	"orlix_tcti_advsimd_fp_source_bound_suite"
#define ADVSIMD_FP_CASE_ARRAY \
	"orlix_tcti_advsimd_fp_source_bound_cases"
#define ADVSIMD_FP_CAPTURE_INCLUDE \
	"#include \"orlix_tcti_advsimd_fp_production_capture.h\""
#define SCALAR_FP_FAMILY_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_scalar_fp_source_bound_test.c"
#define SCALAR_FP_FAMILY_SUITE "orlix-tcti-scalar-fp-source-bound"
#define SCALAR_FP_FAMILY_SUITE_SYMBOL \
	"orlix_tcti_scalar_fp_source_bound_suite"
#define SCALAR_FP_FAMILY_CASE_ARRAY \
	"orlix_tcti_scalar_fp_source_bound_cases"
#define SCALAR_FP_FAMILY_CAPTURE_INCLUDE \
	"#include \"orlix_tcti_scalar_fp_production_capture.h\""
#define AES_CONDITION \
	"54434e4401070000002d0700000017070000000c010000000101010000000101010000000101020000000c00000008464541545f414553"
#define SHA1_CONDITION \
	"54434e4401070000002e0700000017070000000c010000000101010000000101010000000101020000000d00000009464541545f53484131"
#define SHA256_CONDITION \
	"54434e440107000000300700000017070000000c010000000101010000000101010000000101020000000f0000000b464541545f534841323536"
#define ADVSIMD_CONDITION \
	"54434e440107000000310700000017070000000c01000000010101000000010101000000010102000000100000000c464541545f41647653494d44"
#define SM3_CONDITION \
	"54434e4401070000002d0700000017070000000c010000000101010000000101010000000101020000000c00000008464541545f534d33"
#define SHA512_CONDITION \
	"54434e440107000000300700000017070000000c010000000101010000000101010000000101020000000f0000000b464541545f534841353132"
#define SHA3_CONDITION \
	"54434e4401070000002e0700000017070000000c010000000101010000000101010000000101020000000d00000009464541545f53484133"
#define SM4_CONDITION \
	"54434e4401070000002d0700000017070000000c010000000101010000000101010000000101020000000c00000008464541545f534d34"
#define ADD_SUB_PT_CONDITION \
	"54434e4401070000002d0700000017070000000c010000000101010000000101010000000101020000000c00000008464541545f435041"
#define SCALAR_FP_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_scalar_fp_semantics_test.c"
#define SCALAR_FP_SUITE "orlix-tcti-scalar-fp-semantics"
#define SCALAR_FP_SUITE_SYMBOL "orlix_tcti_scalar_fp_semantics_test_suite"
#define SCALAR_FP_CASE_ARRAY "orlix_tcti_scalar_fp_semantics_test_cases"
#define ADVSIMD_FP_ARITHMETIC_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_advsimd_fp_arithmetic_source_bound_test.c"
#define ADVSIMD_FP_ARITHMETIC_SUITE \
	"orlix-tcti-advsimd-fp-arithmetic-source-bound"
#define ADVSIMD_FP_ARITHMETIC_SUITE_SYMBOL \
	"orlix_tcti_advsimd_fp_arithmetic_test_suite"
#define ADVSIMD_FP_ARITHMETIC_CASE_ARRAY \
	"orlix_tcti_advsimd_fp_arithmetic_test_cases"
#define ADVSIMD_HALVING_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_advsimd_halving_source_bound_test.c"
#define ADVSIMD_HALVING_SUITE \
	"orlix-tcti-advsimd-halving-source-bound"
#define ADVSIMD_HALVING_SUITE_SYMBOL \
	"orlix_tcti_advsimd_halving_source_bound_suite"
#define ADVSIMD_HALVING_CASE_ARRAY \
	"orlix_tcti_advsimd_halving_source_bound_cases"
#define ADVSIMD_MUL_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_advsimd_mul_source_bound_test.c"
#define ADVSIMD_MUL_SUITE "orlix-tcti-advsimd-mul-source-bound"
#define ADVSIMD_MUL_SUITE_SYMBOL "advsimd_mul_source_bound_suite"
#define ADVSIMD_MUL_CASE_ARRAY "advsimd_mul_source_bound_cases"
#define ADVSIMD_MINMAX_REDUCTION_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_advsimd_integer_minmax_reduction_source_bound_test.c"
#define ADVSIMD_MINMAX_REDUCTION_SUITE \
	"orlix-tcti-advsimd-integer-minmax-reduction-source-bound"
#define ADVSIMD_MINMAX_REDUCTION_SUITE_SYMBOL "orlix_tcti_minmaxv_suite"
#define ADVSIMD_MINMAX_REDUCTION_CASE_ARRAY "orlix_tcti_minmaxv_cases"
#define INTEGER_CONDITIONAL_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_integer_conditional_source_bound_test.c"
#define INTEGER_CONDITIONAL_SUITE "orlix-tcti-integer-conditional-source-bound"
#define INTEGER_CONDITIONAL_SUITE_SYMBOL \
	"orlix_tcti_integer_conditional_source_bound_test_suite"
#define INTEGER_CONDITIONAL_CASE_ARRAY \
	"orlix_tcti_integer_conditional_source_bound_test_cases"
#define FLAG_MANIPULATION_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_flag_manipulation_source_bound_test.c"
#define FLAG_MANIPULATION_SUITE \
	"orlix-tcti-flag-manipulation-source-bound"
#define FLAG_MANIPULATION_SUITE_SYMBOL \
	"orlix_tcti_flag_manipulation_test_suite"
#define FLAG_MANIPULATION_CASE_ARRAY \
	"orlix_tcti_flag_manipulation_test_cases"
#define FLAG_MANIPULATION_SEMANTIC_PROVENANCE_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/isa/generations/current/manifest"
#define FLAG_MANIPULATION_SEMANTIC_PROVENANCE_SOURCE_SHA256 \
	"1927584b4abe0870fea34e3885e9b026e8e9c3863b62af309f5a2d35d86874ab"
#define FLAG_MANIPULATION_SEMANTIC_PROVENANCE_INCLUDE \
	"#include \"../isa/generations/current/target_asl_availability.def\""
#define FLAG_MANIPULATION_SEMANTIC_PROVENANCE_ARTIFACT_NAME \
	"target_asl_availability.def"
#define FLAG_MANIPULATION_SEMANTIC_PROVENANCE_ARTIFACT \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/isa/generations/current/target_asl_availability.def"
#define FLAG_MANIPULATION_CONDITION \
	"54434e4401070000002f0700000017070000000c010000000101010000000101010000000101020000000e0000000a464541545f466c61674d"
#define SOURCE_LEAF_CLASSIFICATION_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_source_leaf_classification_test.c"
#define SOURCE_LEAF_CLASSIFICATION_SUITE \
	"orlix-tcti-source-leaf-classification"
#define SOURCE_LEAF_CLASSIFICATION_SUITE_SYMBOL \
	"orlix_tcti_source_leaf_classification_test_suite"
#define SOURCE_LEAF_CLASSIFICATION_CASE_ARRAY \
	"orlix_tcti_source_leaf_classification_test_cases"
#define TRANSLATION_CHANGE_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_translation_change_source_bound_test.c"
#define TRANSLATION_CHANGE_SUITE "orlix-tcti-translation-change-source-bound"
#define TRANSLATION_CHANGE_SUITE_SYMBOL "tchange_suite"
#define TRANSLATION_CHANGE_CASE_ARRAY "tchange_cases"
#define SYSTEM_ACCESSOR_PARTITION_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_system_accessor_partition_test.h"
#define SYSTEM_ACCESSOR_PARTITION_SOURCE_SHA256 \
	ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION_SOURCE_SHA256
#define SYSTEM_ACCESSOR_PARTITION_INCLUDE \
	"#include \"orlix_tcti_system_accessor_partition_test.h\""
static const struct orlix_tcti_target_production_capture_binding
production_capture_bindings[] = {
#define ORLIX_TCTI_PRODUCTION_CAPTURE(source, suite, case_name, ordinal, \
		obligation_value, implementation, decoder, lowering) \
	{ source, suite, case_name, ordinal, obligation_value, implementation, \
	  decoder, lowering },
#include "target_production_capture_family.def"
#undef ORLIX_TCTI_PRODUCTION_CAPTURE
};

#define MOPS_COPY_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_mops_copy_test.c"
#define MOPS_COPY_SUITE "orlix-tcti-mops-copy"
#define MOPS_COPY_SUITE_SYMBOL "orlix_tcti_mops_copy_suite"
#define MOPS_COPY_CASE_ARRAY "orlix_tcti_mops_copy_cases"
#define DECODE_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_decode_test.c"
#define DECODE_SUITE "orlix-tcti-decode"
#define DECODE_SUITE_SYMBOL "orlix_tcti_decode_test_suite"
#define DECODE_CASE_ARRAY "orlix_tcti_decode_test_cases"
#define BASE_LOAD_STORE_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_base_load_store_source_bound_test.c"
#define BASE_LOAD_STORE_SUITE "orlix-tcti-base-load-store-source-bound"
#define BASE_LOAD_STORE_SUITE_SYMBOL \
	"orlix_tcti_base_load_store_source_bound_test_suite"
#define BASE_LOAD_STORE_CASE_ARRAY "bls_cases"
#define BASE_LOAD_STORE_HELPER_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_memory_proof.h"
#define BASE_LOAD_STORE_HELPER_INCLUDE \
	"#include \"orlix_tcti_memory_proof.h\""
#define ADVSIMD_LOAD_STORE_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_advsimd_structure_load_store_source_bound_test.c"
#define ADVSIMD_LOAD_STORE_SUITE "orlix-tcti-advsimd-structure-source-bound"
#define ADVSIMD_LOAD_STORE_SUITE_SYMBOL \
	"orlix_tcti_advsimd_structure_suite"
#define ADVSIMD_LOAD_STORE_CASE_ARRAY "orlix_tcti_advsimd_structure_cases"
#define CSSC_CONDITION \
	"54434e4401070000002e0700000017070000000c010000000101010000000101010000000101020000000d00000009464541545f43535343"
#define ADD_SUB_IMMEDIATE_CONDITION \
	"54434e440107000000220700000017070000000c010000000101010000000101010000000101010000000101"
#define LOGICAL_SHIFT_CONDITION \
	"54434e440107000000220700000017070000000c010000000101010000000101010000000101010000000101"
#define SCALAR_CONDITION LOGICAL_SHIFT_CONDITION
#define EXTR_CONDITION \
	"54434e4401070000003b0700000030070000000c0100000001010100000001010a0000001a0300000008000000046f70323104000000080000000427313127010000000101"
#define SCALAR_FP_CONDITION \
	"54434e4401070000002c0700000017070000000c010000000101010000000101010000000101020000000b00000007464541545f4650"
#define LSE_CONDITION \
	"54434e4401070000002d0700000017070000000c010000000101010000000101010000000101020000000c00000008464541545f4c5345"
#define LSE128_CONDITION \
	"54434e440107000000300700000017070000000c010000000101010000000101010000000101020000000f0000000b464541545f4c5345313238"
#define LOGICAL_BASE_OBLIGATIONS \
	(BASELINE_OBLIGATIONS | ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC)
#define LOGICAL_FLAGS_OBLIGATIONS \
	(LOGICAL_BASE_OBLIGATIONS | ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS)
#define SCALAR_BASE_OBLIGATIONS LOGICAL_BASE_OBLIGATIONS
#define SCALAR_FLAGS_OBLIGATIONS LOGICAL_FLAGS_OBLIGATIONS
#define SCALAR_FP_CONVERT_OBLIGATIONS \
	(BASELINE_OBLIGATIONS | ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC | ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS)
#define SCALAR_FP_FAMILY_OBLIGATIONS \
	(BASELINE_OBLIGATIONS | ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC)
#define ADVSIMD_FP_ARITHMETIC_OBLIGATIONS \
	(BASELINE_OBLIGATIONS | ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC | ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS)
#define ADVSIMD_CRYPTO_REQUIRED_OBLIGATIONS \
	(BASELINE_OBLIGATIONS | ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC)
#define ADVSIMD_PERMUTE_MOVE_SIMD_OBLIGATIONS \
	(BASELINE_OBLIGATIONS | ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC)
#define ADVSIMD_PERMUTE_MOVE_GPR_OBLIGATIONS \
	(BASELINE_OBLIGATIONS | ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC)
#define ADVSIMD_INTEGER_SIMD_OBLIGATIONS \
	(BASELINE_OBLIGATIONS | ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC)
#define ADVSIMD_FP_SIMD_OBLIGATIONS ADVSIMD_INTEGER_SIMD_OBLIGATIONS
#define ADVSIMD_HALVING_OBLIGATIONS ADVSIMD_FP_ARITHMETIC_OBLIGATIONS
#define ADVSIMD_MUL_OBLIGATIONS ADVSIMD_FP_ARITHMETIC_OBLIGATIONS
#define ADVSIMD_MINMAX_REDUCTION_OBLIGATIONS \
	ADVSIMD_FP_ARITHMETIC_OBLIGATIONS
#define INTEGER_CONDITIONAL_BASE_OBLIGATIONS \
	(BASELINE_OBLIGATIONS | ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC)
#define INTEGER_CONDITIONAL_FLAGS_OBLIGATIONS \
	(INTEGER_CONDITIONAL_BASE_OBLIGATIONS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS)
#define ORDINARY_LOAD_STORE_OBLIGATIONS \
	(ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS)
#define ADVSIMD_LOAD_STORE_OBLIGATIONS ORDINARY_LOAD_STORE_OBLIGATIONS
#define PRODUCTION_CAPTURE_FAMILY_OBLIGATIONS \
	(BASELINE_OBLIGATIONS | ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC)
#define MOPS_COPY_OBLIGATIONS \
	(BASELINE_OBLIGATIONS | ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS)
#define KUNIT_BUILD_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/Makefile"
#define KUNIT_BUILD_SOURCE_SHA256 \
	ORLIX_TCTI_KUNIT_BUILD_SOURCE_SHA256
#define KSELFTEST_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/tools/testing/selftests/orlix/orlix_tcti_lse_atomic_probe.c"
#define KSELFTEST_SOURCE_SHA256 \
	"dfe85ec0e2761dca4e15a57a0900e89ae82232351bf2f9e5deef1573f8f9bb3f"
#define KSELFTEST_BUILD_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/tools/testing/selftests/orlix/Makefile"
#define KSELFTEST_BUILD_SOURCE_SHA256 \
	"fc07605b3988ee31d01a2c01ff8d1324266d3b7ece8a3c44055eccf7c0c5ce72"
#define KSELFTEST_PROCESS_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/tools/testing/selftests/orlix/process_lifecycle_probe.c"
#define KSELFTEST_PROCESS_SOURCE_SHA256 \
	"a26c7fafc9df670e433f6e58c8bb87e6ac6c7a143170f03287beb1412e809b51"
#define KSELFTEST_SIGNAL_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/tools/testing/selftests/orlix/signal_wait_probe.c"
#define KSELFTEST_SIGNAL_SOURCE_SHA256 \
	"f782e3e950729f96a598fea10f97b88ee31d54b7ddf91a09b161fa14a2f95b9f"
#define KSELFTEST_STACK_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/tools/testing/selftests/orlix/stack_growth_probe.c"
#define KSELFTEST_STACK_SOURCE_SHA256 \
	"57778d4b2b5d6903f8f8bfba1a0c572ebc93951c9b63830d924cec51cda82bf8"
#define KSELFTEST_MMAP_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/tools/testing/selftests/orlix/file_mmap_content_probe.c"
#define KSELFTEST_MMAP_SOURCE_SHA256 \
	"64307b95350c0b2dbcfb7e045074b1b3baefd0884823c4579a775f8259fa6682"
#define KSELFTEST_PTY_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/tools/testing/selftests/orlix/pty_terminal_probe.c"
#define KSELFTEST_PTY_SOURCE_SHA256 \
	"d485075a6cc20624d6c98e06ed2ea92152327c93ddf41cfc4b3b36a3268e9024"
#define KSELFTEST_FD_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/tools/testing/selftests/orlix/fd_alias_probe.c"
#define KSELFTEST_FD_SOURCE_SHA256 \
	"78159391394b48d1d58f1a226ae6be794b1461e1c13958e94a18b0e910a34858"
#define KSELFTEST_SYSTEM_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/tools/testing/selftests/orlix/orlix_tcti_system_probe.c"
#define KSELFTEST_SYSTEM_SOURCE_SHA256 \
	"9bd80e30688b50056fd9400270ba068733f97115efb589c47c8f6671fa9a9693"
#define KSELFTEST_EXCEPTION_INTERFACE_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/tools/testing/selftests/orlix/orlix_tcti_exception_interface_probe.c"
#define KSELFTEST_EXCEPTION_INTERFACE_SOURCE_SHA256 \
	"a40d475d27be1bef1cdd0f2aea646a692780ef071e54390034de65467fe343b9"

static const struct orlix_tcti_target_kselftest_provenance
	exception_interface_kselftest = {
		KSELFTEST_EXCEPTION_INTERFACE_SOURCE,
		KSELFTEST_EXCEPTION_INTERFACE_SOURCE_SHA256,
		KSELFTEST_BUILD_SOURCE, KSELFTEST_BUILD_SOURCE_SHA256,
		"orlix_tcti_exception_interface_probe", "main",
	};

/* Reviewed Kbuild inputs. The digest makes source/index drift fail closed. */
static const struct kunit_source_provenance kunit_sources[] = {
	{ LSE_SOURCE,
	  "2bf5cc6539732cb419c2b92c58f2aeeb56a311243567387b2a6903191ff05f29",
	  "orlix_tcti_lse_decode_test.o", NULL, NULL, NULL },
	{ LSE_SOURCE_BOUND_SOURCE,
	  "8d1f815dbf881146c4aeab1f519d5fb23f7f95e5742a97a74c56e8a343d15723",
	  "orlix_tcti_lse_source_bound_test.o", NULL, NULL, NULL },
	{ LSE128_SOURCE,
	  "5d80460fba9cebadbe3f54b3b9a7f8deeb2be0c8d341826a9af8f0fb09ebb0bd",
	  "orlix_tcti_lse128_resume_test.o", NULL, NULL, NULL },
	{ BASE_ATOMIC_SOURCE,
	  "1058543b93ff3051d528b5280b810235da77c8d95d036c8dede772d85261eddc",
	  "orlix_tcti_base_atomic_source_bound_test.o",
	  BASE_LOAD_STORE_HELPER_SOURCE,
	  "81af9073e09b365867b177687a7fd540ae3a964396090c86f7e669057a94adfe",
	  BASE_LOAD_STORE_HELPER_INCLUDE },
	{ LOGICAL_SHIFT_SOURCE,
	  "44ebc99fb0297ee4353472b931f888986e80d5aecf84096dc3ab58cb58a95e64",
	  "orlix_tcti_logical_shifted_register_test.o", NULL, NULL, NULL },
	{ CSSC_SOURCE,
	  "346d943a9e0484242e8fd30dbaac608e221d8aa782f29e5c365aaf027a39a852",
	  "orlix_tcti_cssc_min_max_immediate_test.o", NULL, NULL, NULL },
	{ ADD_SUB_IMMEDIATE_SOURCE,
	  "47749670119e7e39ddf66f574805d68dfd7c4eb5b22cc812cff692523443dee4",
	  "orlix_tcti_add_sub_immediate_test.o", NULL, NULL, NULL },
	{ LOGICAL_IMMEDIATE_SOURCE,
	  "157ab1feea0a028efd757d6c9d7a85698452f501c739412f287550033f2e6213",
	  "orlix_tcti_logical_immediate_source_bound_test.o", NULL, NULL, NULL },
	{ MOVE_WIDE_SOURCE,
	  "8b3abe4824459f84171288030d94b523b75e7483a2d417395d263ad09968bf75",
	  "orlix_tcti_move_wide_source_bound_test.o", NULL, NULL, NULL },
	{ SCALAR_BITOPS_SOURCE,
	  "0d26714996c7523c16e1942c0b8423664a4347be758a491279095ee9b7f7bfc2",
	  "orlix_tcti_scalar_bitops_source_bound_test.o", NULL, NULL, NULL },
	{ VARIABLE_SHIFT_SOURCE,
	  "7b0863bc16155e49b6d9f309032a157a3a2cf61db60c84f875eafd4ba9770d93",
	  "orlix_tcti_variable_shift_source_bound_test.o", NULL, NULL, NULL },
	{ ADD_SUB_REGISTER_SOURCE,
	  "f3648d0527379a31f76804762f730f33209c06658eb657b3c662ae14f00cc55e",
	  "orlix_tcti_add_sub_register_source_bound_test.o", NULL, NULL, NULL },
	{ BASE_ADD_SUB_SOURCE,
	  "1740d6e16cf305d3e36bd8767df187c25ce381ea79638042fc8ed5a6bc138739",
	  "orlix_tcti_base_add_sub_source_bound_test.o",
	  BASE_LOAD_STORE_HELPER_SOURCE,
	  "81af9073e09b365867b177687a7fd540ae3a964396090c86f7e669057a94adfe",
	  BASE_LOAD_STORE_HELPER_INCLUDE },
	{ BASE_CONTROL_FLOW_SOURCE,
	  "8ed64b822e04bf804a2d620497b235cbf0656f791d7ad7ca1b1012a35356375a",
	  "orlix_tcti_base_control_flow_source_bound_test.o",
	  BASE_LOAD_STORE_HELPER_SOURCE,
	  "81af9073e09b365867b177687a7fd540ae3a964396090c86f7e669057a94adfe",
	  BASE_LOAD_STORE_HELPER_INCLUDE },
	{ BASE_EXCEPTIONS_SOURCE,
	  "99c8e4fdb18334d547e0aec46daffb2e573f4033b2d9bb11dc7f5a101f67ea12",
	  "orlix_tcti_base_exceptions_source_bound_test.o",
	  BASE_LOAD_STORE_HELPER_SOURCE,
	  "81af9073e09b365867b177687a7fd540ae3a964396090c86f7e669057a94adfe",
	  BASE_LOAD_STORE_HELPER_INCLUDE },
	{ BASE_CONDITIONAL_SOURCE,
	  "9ff7322513d81a382be0c16219d3bf4e4a197114eb3c73ba67fa1769ec4c0be7",
	  "orlix_tcti_base_conditional_source_bound_test.o",
	  BASE_LOAD_STORE_HELPER_SOURCE,
	  "81af9073e09b365867b177687a7fd540ae3a964396090c86f7e669057a94adfe",
	  BASE_LOAD_STORE_HELPER_INCLUDE },
	{ BASE_BITFIELD_UNARY_SOURCE,
	  "18cc5870d682cc498def40d3e1cb070c0b24bfd04c7124ba4de8f8ff60a1ecf5",
	  "orlix_tcti_base_bitfield_unary_source_bound_test.o",
	  BASE_LOAD_STORE_HELPER_SOURCE,
	  "81af9073e09b365867b177687a7fd540ae3a964396090c86f7e669057a94adfe",
	  BASE_LOAD_STORE_HELPER_INCLUDE },
	{ BASE_MULTIPLY_DIVIDE_SOURCE,
	  "f6b727465e779a0ef9d5e3b55a573c5dcf5cd3c4ffdbb37ba49f2190b2a28d4f",
	  "orlix_tcti_base_multiply_divide_source_bound_test.o",
	  BASE_LOAD_STORE_HELPER_SOURCE,
	  "81af9073e09b365867b177687a7fd540ae3a964396090c86f7e669057a94adfe",
	  BASE_LOAD_STORE_HELPER_INCLUDE },
	{ ADVSIMD_CRYPTO_SOURCE,
	  "57d40c45be85dc59af1eae398cb423a64db1913cbfc97f1983656df27708e026",
	  "orlix_tcti_advsimd_crypto_source_bound_test.o",
	  BASE_LOAD_STORE_HELPER_SOURCE,
	  "81af9073e09b365867b177687a7fd540ae3a964396090c86f7e669057a94adfe",
	  BASE_LOAD_STORE_HELPER_INCLUDE },
	{ ADVSIMD_PERMUTE_MOVE_SOURCE,
	  "7602e1f8916fee15571f27ff402b8edda7d1a373f597bd063ee8766f376cfdd6",
	  "orlix_tcti_advsimd_permute_move_source_bound_test.o",
	  BASE_LOAD_STORE_HELPER_SOURCE,
	  "81af9073e09b365867b177687a7fd540ae3a964396090c86f7e669057a94adfe",
	  BASE_LOAD_STORE_HELPER_INCLUDE },
	{ ADVSIMD_INTEGER_SOURCE,
	  "4dfba6a51521161fe234e2b66d797d3575a5b570351625d28c004325676043ba",
	  "orlix_tcti_advsimd_integer_source_bound_test.o",
	  BASE_LOAD_STORE_HELPER_SOURCE,
	  "81af9073e09b365867b177687a7fd540ae3a964396090c86f7e669057a94adfe",
	  BASE_LOAD_STORE_HELPER_INCLUDE },
	{ ADVSIMD_FP_SOURCE,
	  "cbdff96a1ca367126d637df1ad6cf37ab0fc1d7d963e8aa29536b7127e7692f1",
	  "orlix_tcti_advsimd_fp_source_bound_test.o",
	  BASE_LOAD_STORE_HELPER_SOURCE,
	  "81af9073e09b365867b177687a7fd540ae3a964396090c86f7e669057a94adfe",
	  BASE_LOAD_STORE_HELPER_INCLUDE },
	{ SCALAR_FP_FAMILY_SOURCE,
	  "7c0715518d77285f28538e9fcba4239581f302729bf6aea72e7613fa067335aa",
	  "orlix_tcti_scalar_fp_source_bound_test.o",
	  BASE_LOAD_STORE_HELPER_SOURCE,
	  "81af9073e09b365867b177687a7fd540ae3a964396090c86f7e669057a94adfe",
	  BASE_LOAD_STORE_HELPER_INCLUDE },
	{ SOURCE_LEAF_CLASSIFICATION_SOURCE,
	  "02957dd1bc834975fba91fdf41170b319d373d17084255b6629d9659da27d18c",
	  "orlix_tcti_source_leaf_classification_test.o",
	  SYSTEM_ACCESSOR_PARTITION_SOURCE,
	  SYSTEM_ACCESSOR_PARTITION_SOURCE_SHA256,
	  SYSTEM_ACCESSOR_PARTITION_INCLUDE },
#define ORLIX_TCTI_PROOF_FAMILY_METADATA(source_value, source_sha256_value, \
		object_value, suite_value, suite_symbol_value, case_array_value, \
		decode_case_value, production_case_value) \
	{ source_value, source_sha256_value, object_value, NULL, NULL, NULL },
#include "target_production_capture_family.def"
#undef ORLIX_TCTI_PROOF_FAMILY_METADATA
	{ TRANSLATION_CHANGE_SOURCE,
	  "ef3ade2d616da3c0dc7a698f972fc09b08b6787e2cc1264743fdae97791c42d6",
	  "orlix_tcti_translation_change_source_bound_test.o", NULL, NULL, NULL },
	{ DECODE_SOURCE,
	  ORLIX_TCTI_DECODE_SOURCE_SHA256,
	  "orlix_tcti_decode_test.o", NULL, NULL, NULL },
	{ BASE_LOAD_STORE_SOURCE,
	  "00207f93cbf4ae47ce6ea838f3ace4d000da700da4174fa36ad7617dafed3f21",
	  "orlix_tcti_base_load_store_source_bound_test.o",
	  BASE_LOAD_STORE_HELPER_SOURCE,
	  "81af9073e09b365867b177687a7fd540ae3a964396090c86f7e669057a94adfe",
	  BASE_LOAD_STORE_HELPER_INCLUDE },
	{ ADVSIMD_LOAD_STORE_SOURCE,
	  "441aa9fddcbea066244107d7418f5ec1f31345ef946ffc1a141c6bd5333c342a",
	  "orlix_tcti_advsimd_structure_load_store_source_bound_test.o",
	  BASE_LOAD_STORE_HELPER_SOURCE,
	  "81af9073e09b365867b177687a7fd540ae3a964396090c86f7e669057a94adfe",
	  BASE_LOAD_STORE_HELPER_INCLUDE },
	{ SCALAR_FP_SOURCE,
	  "8c245ce10f189b2f7a3fef91c9fc478b3e4e1cd2af69a98ec2b930229b75c993",
	  "orlix_tcti_scalar_fp_semantics_test.o", NULL, NULL, NULL },
	{ ADVSIMD_FP_ARITHMETIC_SOURCE,
	  "c1e420d4451b386a8d8af24740bf445c4fbef8701b4a02f4f882416e26f4e1ed",
	  "orlix_tcti_advsimd_fp_arithmetic_source_bound_test.o", NULL, NULL, NULL },
	{ ADVSIMD_HALVING_SOURCE,
	  "4395dc8d94bb79802111db4b4a2d5e269def048c3ba3746be58b22e048535b6e",
	  "orlix_tcti_advsimd_halving_source_bound_test.o", NULL, NULL, NULL },
	{ ADVSIMD_MUL_SOURCE,
	  "93e647a701efdf32967ddc75229e6c56e081d1dd0d534f4a9de41803d93ea558",
	  "orlix_tcti_advsimd_mul_source_bound_test.o", NULL, NULL, NULL },
	{ ADVSIMD_MINMAX_REDUCTION_SOURCE,
	  "0458c2ac841380d2e535e55e6a766268d86de4e42c82c66e9ede5ebd1558581e",
	  "orlix_tcti_advsimd_integer_minmax_reduction_source_bound_test.o",
	  NULL, NULL, NULL },
	{ INTEGER_CONDITIONAL_SOURCE,
	  "957600abc81a59dfb011b2177b507ab206c41ef5a371cae363f583fa50a8e58f",
	  "orlix_tcti_integer_conditional_source_bound_test.o", NULL, NULL, NULL },
	{ MOPS_COPY_SOURCE,
	  "7bbbaa5dfb159b65f538c61ffb09f76fc5c5f72b6edd2f244491fdaf2b225ef0",
	  "orlix_tcti_mops_copy_test.o", NULL, NULL, NULL },
	{ FLAG_MANIPULATION_SOURCE,
	  "c58ee600547be6fd79a87b9776d1c6e380d524dbefb4a2e9dbd4e3a351cb2099",
	  "orlix_tcti_flag_manipulation_source_bound_test.o",
	  FLAG_MANIPULATION_SEMANTIC_PROVENANCE_SOURCE,
	  FLAG_MANIPULATION_SEMANTIC_PROVENANCE_SOURCE_SHA256,
	  FLAG_MANIPULATION_SEMANTIC_PROVENANCE_INCLUDE },
	};

static const struct kunit_dependency_terminal_artifact
	kunit_dependency_terminal_artifacts[] = {
	{ FLAG_MANIPULATION_SOURCE, FLAG_MANIPULATION_SEMANTIC_PROVENANCE_ARTIFACT_NAME,
	  FLAG_MANIPULATION_SEMANTIC_PROVENANCE_ARTIFACT },
};

/* Per-case upper bounds prevent a registered case from self-proving new duties. */
static const struct kunit_case_provenance kunit_case_provenance[] = {
	{ BASE_ATOMIC_SOURCE, BASE_ATOMIC_SUITE, BASE_ATOMIC_SUITE_SYMBOL,
	  BASE_ATOMIC_CASE_ARRAY,
	  "orlix_tcti_base_atomic_decodes_exact_source_cohort",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ BASE_ATOMIC_SOURCE, BASE_ATOMIC_SUITE, BASE_ATOMIC_SUITE_SYMBOL,
	  BASE_ATOMIC_CASE_ARRAY,
	  "orlix_tcti_base_atomic_binds_pinned_ddi0602_semantics",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ BASE_ATOMIC_SOURCE, BASE_ATOMIC_SUITE, BASE_ATOMIC_SUITE_SYMBOL,
	  BASE_ATOMIC_CASE_ARRAY,
	  "orlix_tcti_base_atomic_decodes_register_variants",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS },
	{ BASE_ATOMIC_SOURCE, BASE_ATOMIC_SUITE, BASE_ATOMIC_SUITE_SYMBOL,
	  BASE_ATOMIC_CASE_ARRAY,
	  "orlix_tcti_base_atomic_rejects_reserved_registers",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ BASE_ATOMIC_SOURCE, BASE_ATOMIC_SUITE, BASE_ATOMIC_SUITE_SYMBOL,
	  BASE_ATOMIC_CASE_ARRAY,
	  "orlix_tcti_base_atomic_classifies_fixed_bit_neighbours",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ BASE_ATOMIC_SOURCE, BASE_ATOMIC_SUITE, BASE_ATOMIC_SUITE_SYMBOL,
	  BASE_ATOMIC_CASE_ARRAY,
	  "orlix_tcti_base_atomic_decodes_ordered_access_shapes",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ORDERING },
	{ BASE_ATOMIC_SOURCE, BASE_ATOMIC_SUITE, BASE_ATOMIC_SUITE_SYMBOL,
	  BASE_ATOMIC_CASE_ARRAY,
	  "orlix_tcti_base_atomic_executes_every_source_leaf",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ BASE_ATOMIC_SOURCE, BASE_ATOMIC_SUITE, BASE_ATOMIC_SUITE_SYMBOL,
	  BASE_ATOMIC_CASE_ARRAY,
	  "orlix_tcti_base_atomic_executes_every_fp_leaf",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ BASE_ATOMIC_SOURCE, BASE_ATOMIC_SUITE, BASE_ATOMIC_SUITE_SYMBOL,
	  BASE_ATOMIC_CASE_ARRAY,
	  "orlix_tcti_base_atomic_rcw_conditional_writes_and_flags",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ BASE_ATOMIC_SOURCE, BASE_ATOMIC_SUITE, BASE_ATOMIC_SUITE_SYMBOL,
	  BASE_ATOMIC_CASE_ARRAY,
	  "orlix_tcti_base_atomic_executes_ls64_state",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ BASE_ATOMIC_SOURCE, BASE_ATOMIC_SUITE, BASE_ATOMIC_SUITE_SYMBOL,
	  BASE_ATOMIC_CASE_ARRAY, "orlix_tcti_base_atomic_faults_are_precise",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS },
	{ BASE_ATOMIC_SOURCE, BASE_ATOMIC_SUITE, BASE_ATOMIC_SUITE_SYMBOL,
	  BASE_ATOMIC_CASE_ARRAY,
	  "orlix_tcti_base_atomic_concurrent_no_tearing",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ATOMICITY },
	{ BASE_ATOMIC_SOURCE, BASE_ATOMIC_SUITE, BASE_ATOMIC_SUITE_SYMBOL,
	  BASE_ATOMIC_CASE_ARRAY,
	  "orlix_tcti_base_atomic_exclusive_monitor_is_exact",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ATOMICITY },
	{ BASE_ATOMIC_SOURCE, BASE_ATOMIC_SUITE, BASE_ATOMIC_SUITE_SYMBOL,
	  BASE_ATOMIC_CASE_ARRAY,
	  "orlix_tcti_base_atomic_forbidden_ordering_outcomes",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ORDERING },
	{ BASE_ATOMIC_SOURCE, BASE_ATOMIC_SUITE, BASE_ATOMIC_SUITE_SYMBOL,
	  BASE_ATOMIC_CASE_ARRAY,
	  "orlix_tcti_base_atomic_production_resume",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ORDERING },
	{ BASE_ATOMIC_SOURCE, BASE_ATOMIC_SUITE, BASE_ATOMIC_SUITE_SYMBOL,
	  BASE_ATOMIC_CASE_ARRAY,
	  "orlix_tcti_base_atomic_unmapped_faults",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS },
	{ BASE_ATOMIC_SOURCE, BASE_ATOMIC_SUITE, BASE_ATOMIC_SUITE_SYMBOL,
	  BASE_ATOMIC_CASE_ARRAY,
	  "orlix_tcti_base_atomic_monitor_mismatch_fails_store",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ATOMICITY },
	{ BASE_ATOMIC_SOURCE, BASE_ATOMIC_SUITE, BASE_ATOMIC_SUITE_SYMBOL,
	  BASE_ATOMIC_CASE_ARRAY,
	  "orlix_tcti_base_atomic_casp_whole_pair",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ATOMICITY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY },
	{ BASE_ATOMIC_SOURCE, BASE_ATOMIC_SUITE, BASE_ATOMIC_SUITE_SYMBOL,
	  BASE_ATOMIC_CASE_ARRAY,
	  "orlix_tcti_base_atomic_cas_rmw_returns_old_value",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ATOMICITY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY },
	{ BASE_ATOMIC_SOURCE, BASE_ATOMIC_SUITE, BASE_ATOMIC_SUITE_SYMBOL,
	  BASE_ATOMIC_CASE_ARRAY,
	  "orlix_tcti_base_atomic_production_atomicity",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ATOMICITY },
	{ DECODE_SOURCE, DECODE_SUITE, DECODE_SUITE_SYMBOL, DECODE_CASE_ARRAY,
	  "orlix_tcti_decode_exhaustive_load_store_exclusive_family",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ DECODE_SOURCE, DECODE_SUITE, DECODE_SUITE_SYMBOL, DECODE_CASE_ARRAY,
	  "orlix_tcti_decode_load_store_exclusive_all_register_fields",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS },
	{ DECODE_SOURCE, DECODE_SUITE, DECODE_SUITE_SYMBOL, DECODE_CASE_ARRAY,
	  "orlix_tcti_switch_fails_store_exclusive_without_reservation",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS },
	{ DECODE_SOURCE, DECODE_SUITE, DECODE_SUITE_SYMBOL, DECODE_CASE_ARRAY,
	  "orlix_tcti_gadget_executes_complete_load_store_exclusive_family",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS },
	{ LSE_SOURCE, "orlix-tcti-lse-decode",
	  "orlix_tcti_lse_decode_test_suite", "orlix_tcti_lse_decode_test_cases",
	  "orlix_tcti_lse_execute_rmw_matrix",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS },
	{ LSE_SOURCE_BOUND_SOURCE, LSE_SOURCE_BOUND_SUITE,
	  LSE_SOURCE_BOUND_SUITE_SYMBOL, LSE_SOURCE_BOUND_CASE_ARRAY,
	  "lse_source_bound_decodes_every_base_leaf",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ LSE_SOURCE_BOUND_SOURCE, LSE_SOURCE_BOUND_SUITE,
	  LSE_SOURCE_BOUND_SUITE_SYMBOL, LSE_SOURCE_BOUND_CASE_ARRAY,
	  "lse_source_bound_executes_every_base_leaf",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ LSE_SOURCE_BOUND_SOURCE, LSE_SOURCE_BOUND_SUITE,
	  LSE_SOURCE_BOUND_SUITE_SYMBOL, LSE_SOURCE_BOUND_CASE_ARRAY,
	  "lse_source_bound_rejects_reserved_encodings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ LSE128_SOURCE, LSE128_SUITE, LSE128_SUITE_SYMBOL,
	  LSE128_CASE_ARRAY, "orlix_tcti_lse128_resume_all_source_leaves",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ LSE128_SOURCE, LSE128_SUITE, LSE128_SUITE_SYMBOL,
	  LSE128_CASE_ARRAY, "orlix_tcti_lse128_resume_rejects_reserved_operations",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ LSE128_SOURCE, LSE128_SUITE, LSE128_SUITE_SYMBOL,
	  LSE128_CASE_ARRAY, "orlix_tcti_lse128_resume_alignment_faults_all_leaves",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS },
	{ LSE128_SOURCE, LSE128_SUITE, LSE128_SUITE_SYMBOL,
	  LSE128_CASE_ARRAY, "orlix_tcti_lse128_resume_readonly_faults_all_leaves",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS },
	{ LSE128_SOURCE, LSE128_SUITE, LSE128_SUITE_SYMBOL,
	  LSE128_CASE_ARRAY, "orlix_tcti_lse128_resume_unmapped_faults_all_leaves",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS },
	{ LSE128_SOURCE, LSE128_SUITE, LSE128_SUITE_SYMBOL,
	  LSE128_CASE_ARRAY,
	  "orlix_tcti_lse128_resume_rejects_fixed_bit_near_misses",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ LOGICAL_SHIFT_SOURCE, LOGICAL_SHIFT_SUITE,
	  LOGICAL_SHIFT_SUITE_SYMBOL, LOGICAL_SHIFT_CASE_ARRAY,
	  "orlix_tcti_logical_shifted_register_source_bindings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ LOGICAL_SHIFT_SOURCE, LOGICAL_SHIFT_SUITE,
	  LOGICAL_SHIFT_SUITE_SYMBOL, LOGICAL_SHIFT_CASE_ARRAY,
	  "orlix_tcti_logical_shifted_register_fixed_bit_neighbours",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ LOGICAL_SHIFT_SOURCE, LOGICAL_SHIFT_SUITE,
	  LOGICAL_SHIFT_SUITE_SYMBOL, LOGICAL_SHIFT_CASE_ARRAY,
	  "orlix_tcti_logical_shifted_register_complete_field_matrix",
	  LOGICAL_FLAGS_OBLIGATIONS },
	{ LOGICAL_SHIFT_SOURCE, LOGICAL_SHIFT_SUITE,
	  LOGICAL_SHIFT_SUITE_SYMBOL, LOGICAL_SHIFT_CASE_ARRAY,
	  "orlix_tcti_logical_shifted_register_register_and_overlap_matrix",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ LOGICAL_SHIFT_SOURCE, LOGICAL_SHIFT_SUITE,
	  LOGICAL_SHIFT_SUITE_SYMBOL, LOGICAL_SHIFT_CASE_ARRAY,
	  "orlix_tcti_logical_shifted_register_aliases",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ LOGICAL_SHIFT_SOURCE, LOGICAL_SHIFT_SUITE,
	  LOGICAL_SHIFT_SUITE_SYMBOL, LOGICAL_SHIFT_CASE_ARRAY,
	  "orlix_tcti_logical_shifted_register_reserved_structured_exits",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ CSSC_SOURCE, CSSC_SUITE, CSSC_SUITE_SYMBOL, CSSC_CASE_ARRAY,
	  "orlix_tcti_integer_minmax_exact_source_cohort",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ CSSC_SOURCE, CSSC_SUITE, CSSC_SUITE_SYMBOL, CSSC_CASE_ARRAY,
	  "orlix_tcti_cssc_min_max_immediate_source_fingerprints",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ CSSC_SOURCE, CSSC_SUITE, CSSC_SUITE_SYMBOL, CSSC_CASE_ARRAY,
	  "orlix_tcti_cssc_min_max_immediate_all_legal_fields",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ CSSC_SOURCE, CSSC_SUITE, CSSC_SUITE_SYMBOL, CSSC_CASE_ARRAY,
	  "orlix_tcti_cssc_min_max_immediate_execute_all_immediates",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ CSSC_SOURCE, CSSC_SUITE, CSSC_SUITE_SYMBOL, CSSC_CASE_ARRAY,
	  "orlix_tcti_cssc_min_max_immediate_zero_registers_and_pstate",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ CSSC_SOURCE, CSSC_SUITE, CSSC_SUITE_SYMBOL, CSSC_CASE_ARRAY,
	  "orlix_tcti_cssc_min_max_immediate_overlap",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ CSSC_SOURCE, CSSC_SUITE, CSSC_SUITE_SYMBOL, CSSC_CASE_ARRAY,
	  "orlix_tcti_cssc_min_max_immediate_rejects_non_cssc_encodings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ CSSC_SOURCE, CSSC_SUITE, CSSC_SUITE_SYMBOL, CSSC_CASE_ARRAY,
	  "orlix_tcti_cssc_data_processing_source_fingerprints",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ CSSC_SOURCE, CSSC_SUITE, CSSC_SUITE_SYMBOL, CSSC_CASE_ARRAY,
	  "orlix_tcti_cssc_data_processing_all_legal_register_fields",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ CSSC_SOURCE, CSSC_SUITE, CSSC_SUITE_SYMBOL, CSSC_CASE_ARRAY,
	  "orlix_tcti_cssc_data_processing_execute_boundaries",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ CSSC_SOURCE, CSSC_SUITE, CSSC_SUITE_SYMBOL, CSSC_CASE_ARRAY,
	  "orlix_tcti_cssc_data_processing_zero_registers",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ CSSC_SOURCE, CSSC_SUITE, CSSC_SUITE_SYMBOL, CSSC_CASE_ARRAY,
	  "orlix_tcti_cssc_data_processing_minmax_overlap",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ CSSC_SOURCE, CSSC_SUITE, CSSC_SUITE_SYMBOL, CSSC_CASE_ARRAY,
	  "orlix_tcti_cssc_data_processing_rejects_reserved_opcodes",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ ADD_SUB_IMMEDIATE_SOURCE, ADD_SUB_IMMEDIATE_SUITE,
	  ADD_SUB_IMMEDIATE_SUITE_SYMBOL, ADD_SUB_IMMEDIATE_CASE_ARRAY,
	  "orlix_tcti_add_sub_immediate_source_bindings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ ADD_SUB_IMMEDIATE_SOURCE, ADD_SUB_IMMEDIATE_SUITE,
	  ADD_SUB_IMMEDIATE_SUITE_SYMBOL, ADD_SUB_IMMEDIATE_CASE_ARRAY,
	  "orlix_tcti_add_sub_immediate_all_legal_encodings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ ADD_SUB_IMMEDIATE_SOURCE, ADD_SUB_IMMEDIATE_SUITE,
	  ADD_SUB_IMMEDIATE_SUITE_SYMBOL, ADD_SUB_IMMEDIATE_CASE_ARRAY,
	  "orlix_tcti_add_sub_immediate_production_path_arithmetic",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ ADD_SUB_IMMEDIATE_SOURCE, ADD_SUB_IMMEDIATE_SUITE,
	  ADD_SUB_IMMEDIATE_SUITE_SYMBOL, ADD_SUB_IMMEDIATE_CASE_ARRAY,
	  "orlix_tcti_add_sub_immediate_special_register_aliases",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ ADD_SUB_IMMEDIATE_SOURCE, ADD_SUB_IMMEDIATE_SUITE,
	  ADD_SUB_IMMEDIATE_SUITE_SYMBOL, ADD_SUB_IMMEDIATE_CASE_ARRAY,
	  "orlix_tcti_add_sub_immediate_cmn_cmp_nzcv_boundaries",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ ADD_SUB_IMMEDIATE_SOURCE, ADD_SUB_IMMEDIATE_SUITE,
	  ADD_SUB_IMMEDIATE_SUITE_SYMBOL, ADD_SUB_IMMEDIATE_CASE_ARRAY,
	  "orlix_tcti_add_sub_immediate_source_mask_boundaries",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE },
	{ LOGICAL_IMMEDIATE_SOURCE, LOGICAL_IMMEDIATE_SUITE,
	  LOGICAL_IMMEDIATE_SUITE_SYMBOL, LOGICAL_IMMEDIATE_CASE_ARRAY,
	  "orlix_tcti_logical_immediate_source_bindings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ LOGICAL_IMMEDIATE_SOURCE, LOGICAL_IMMEDIATE_SUITE,
	  LOGICAL_IMMEDIATE_SUITE_SYMBOL, LOGICAL_IMMEDIATE_CASE_ARRAY,
	  "orlix_tcti_logical_immediate_fixed_bit_neighbours",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ LOGICAL_IMMEDIATE_SOURCE, LOGICAL_IMMEDIATE_SUITE,
	  LOGICAL_IMMEDIATE_SUITE_SYMBOL, LOGICAL_IMMEDIATE_CASE_ARRAY,
	  "orlix_tcti_logical_immediate_complete_decode_matrix",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ LOGICAL_IMMEDIATE_SOURCE, LOGICAL_IMMEDIATE_SUITE,
	  LOGICAL_IMMEDIATE_SUITE_SYMBOL, LOGICAL_IMMEDIATE_CASE_ARRAY,
	  "orlix_tcti_logical_immediate_production_path_semantics",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ LOGICAL_IMMEDIATE_SOURCE, LOGICAL_IMMEDIATE_SUITE,
	  LOGICAL_IMMEDIATE_SUITE_SYMBOL, LOGICAL_IMMEDIATE_CASE_ARRAY,
	  "orlix_tcti_logical_immediate_aliases_and_special_registers",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ LOGICAL_IMMEDIATE_SOURCE, LOGICAL_IMMEDIATE_SUITE,
	  LOGICAL_IMMEDIATE_SUITE_SYMBOL, LOGICAL_IMMEDIATE_CASE_ARRAY,
	  "orlix_tcti_logical_immediate_reserved_structured_exits",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ MOVE_WIDE_SOURCE, MOVE_WIDE_SUITE, MOVE_WIDE_SUITE_SYMBOL,
	  MOVE_WIDE_CASE_ARRAY, "orlix_tcti_move_wide_source_bindings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ MOVE_WIDE_SOURCE, MOVE_WIDE_SUITE, MOVE_WIDE_SUITE_SYMBOL,
	  MOVE_WIDE_CASE_ARRAY, "orlix_tcti_move_wide_complete_legal_decode_matrix",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ MOVE_WIDE_SOURCE, MOVE_WIDE_SUITE, MOVE_WIDE_SUITE_SYMBOL,
	  MOVE_WIDE_CASE_ARRAY, "orlix_tcti_move_wide_all_immediates_decode",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ MOVE_WIDE_SOURCE, MOVE_WIDE_SUITE, MOVE_WIDE_SUITE_SYMBOL,
	  MOVE_WIDE_CASE_ARRAY, "orlix_tcti_move_wide_production_path_semantics",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ MOVE_WIDE_SOURCE, MOVE_WIDE_SUITE, MOVE_WIDE_SUITE_SYMBOL,
	  MOVE_WIDE_CASE_ARRAY, "orlix_tcti_move_wide_fixed_bit_neighbours_and_reserved",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ SCALAR_BITOPS_SOURCE, SCALAR_BITOPS_SUITE,
	  SCALAR_BITOPS_SUITE_SYMBOL, SCALAR_BITOPS_CASE_ARRAY,
	  "orlix_tcti_scalar_bitops_source_bindings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ VARIABLE_SHIFT_SOURCE, VARIABLE_SHIFT_SUITE,
	  VARIABLE_SHIFT_SUITE_SYMBOL, VARIABLE_SHIFT_CASE_ARRAY,
	  "orlix_tcti_variable_shift_source_bindings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ VARIABLE_SHIFT_SOURCE, VARIABLE_SHIFT_SUITE,
	  VARIABLE_SHIFT_SUITE_SYMBOL, VARIABLE_SHIFT_CASE_ARRAY,
	  "orlix_tcti_variable_shift_production_path_semantics",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ ADD_SUB_REGISTER_SOURCE, ADD_SUB_REGISTER_SUITE,
	  ADD_SUB_REGISTER_SUITE_SYMBOL, ADD_SUB_REGISTER_CASE_ARRAY,
	  "asr_source_and_decode", ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ ADD_SUB_REGISTER_SOURCE, ADD_SUB_REGISTER_SUITE,
	  ADD_SUB_REGISTER_SUITE_SYMBOL, ADD_SUB_REGISTER_CASE_ARRAY,
	  "asr_resume_semantics", ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ ADD_SUB_REGISTER_SOURCE, ADD_SUB_REGISTER_SUITE,
	  ADD_SUB_REGISTER_SUITE_SYMBOL, ADD_SUB_REGISTER_CASE_ARRAY,
	  "asr_reserved_structured_exits",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ BASE_ADD_SUB_SOURCE, BASE_ADD_SUB_SUITE, BASE_ADD_SUB_SUITE_SYMBOL,
	  BASE_ADD_SUB_CASE_ARRAY,
	  "orlix_tcti_base_add_sub_decodes_exact_source_cohort",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ BASE_ADD_SUB_SOURCE, BASE_ADD_SUB_SUITE, BASE_ADD_SUB_SUITE_SYMBOL,
	  BASE_ADD_SUB_CASE_ARRAY,
	  "orlix_tcti_base_add_sub_binds_pinned_ddi0602_semantics",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ BASE_ADD_SUB_SOURCE, BASE_ADD_SUB_SUITE, BASE_ADD_SUB_SUITE_SYMBOL,
	  BASE_ADD_SUB_CASE_ARRAY, "orlix_tcti_base_add_sub_production_resume",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ BASE_ADD_SUB_SOURCE, BASE_ADD_SUB_SUITE, BASE_ADD_SUB_SUITE_SYMBOL,
	  BASE_ADD_SUB_CASE_ARRAY, "orlix_tcti_base_add_sub_reserved_encodings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ BASE_ADD_SUB_SOURCE, BASE_ADD_SUB_SUITE, BASE_ADD_SUB_SUITE_SYMBOL,
	  BASE_ADD_SUB_CASE_ARRAY,
	  "orlix_tcti_base_add_sub_sp_zr_and_w_upper_zero",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ BASE_ADD_SUB_SOURCE, BASE_ADD_SUB_SUITE, BASE_ADD_SUB_SUITE_SYMBOL,
	  BASE_ADD_SUB_CASE_ARRAY,
	  "orlix_tcti_base_add_sub_extend_shift_and_carry",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ BASE_ADD_SUB_SOURCE, BASE_ADD_SUB_SUITE, BASE_ADD_SUB_SUITE_SYMBOL,
	  BASE_ADD_SUB_CASE_ARRAY,
	  "orlix_tcti_base_add_sub_simd_state_unchanged",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS },
	{ BASE_ADD_SUB_SOURCE, BASE_ADD_SUB_SUITE, BASE_ADD_SUB_SUITE_SYMBOL,
	  BASE_ADD_SUB_CASE_ARRAY,
	  "orlix_tcti_base_add_sub_pointer_preserves_tag",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ BASE_CONTROL_FLOW_SOURCE, BASE_CONTROL_FLOW_SUITE,
	  BASE_CONTROL_FLOW_SUITE_SYMBOL, BASE_CONTROL_FLOW_CASE_ARRAY,
	  "orlix_tcti_base_control_flow_decodes_exact_source_cohort",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ BASE_CONTROL_FLOW_SOURCE, BASE_CONTROL_FLOW_SUITE,
	  BASE_CONTROL_FLOW_SUITE_SYMBOL, BASE_CONTROL_FLOW_CASE_ARRAY,
	  "orlix_tcti_base_control_flow_binds_pinned_ddi0602_semantics",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ BASE_CONTROL_FLOW_SOURCE, BASE_CONTROL_FLOW_SUITE,
	  BASE_CONTROL_FLOW_SUITE_SYMBOL, BASE_CONTROL_FLOW_CASE_ARRAY,
	  "orlix_tcti_base_control_flow_production_resume",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ BASE_CONTROL_FLOW_SOURCE, BASE_CONTROL_FLOW_SUITE,
	  BASE_CONTROL_FLOW_SUITE_SYMBOL, BASE_CONTROL_FLOW_CASE_ARRAY,
	  "orlix_tcti_base_control_flow_link_and_xzr",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ BASE_CONTROL_FLOW_SOURCE, BASE_CONTROL_FLOW_SUITE,
	  BASE_CONTROL_FLOW_SUITE_SYMBOL, BASE_CONTROL_FLOW_CASE_ARRAY,
	  "orlix_tcti_base_control_flow_offsets_and_alignment",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ BASE_CONTROL_FLOW_SOURCE, BASE_CONTROL_FLOW_SUITE,
	  BASE_CONTROL_FLOW_SUITE_SYMBOL, BASE_CONTROL_FLOW_CASE_ARRAY,
	  "orlix_tcti_base_control_flow_simd_and_flags_unchanged",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ BASE_CONTROL_FLOW_SOURCE, BASE_CONTROL_FLOW_SUITE,
	  BASE_CONTROL_FLOW_SUITE_SYMBOL, BASE_CONTROL_FLOW_CASE_ARRAY,
	  "orlix_tcti_base_control_flow_non_el0_rejected",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ BASE_CONTROL_FLOW_SOURCE, BASE_CONTROL_FLOW_SUITE,
	  BASE_CONTROL_FLOW_SUITE_SYMBOL, BASE_CONTROL_FLOW_CASE_ARRAY,
	  "orlix_tcti_base_control_flow_reserved_encodings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ BASE_EXCEPTIONS_SOURCE, BASE_EXCEPTIONS_SUITE,
	  BASE_EXCEPTIONS_SUITE_SYMBOL, BASE_EXCEPTIONS_CASE_ARRAY,
	  "orlix_tcti_base_exceptions_decodes_exact_source_cohort",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ BASE_EXCEPTIONS_SOURCE, BASE_EXCEPTIONS_SUITE,
	  BASE_EXCEPTIONS_SUITE_SYMBOL, BASE_EXCEPTIONS_CASE_ARRAY,
	  "orlix_tcti_base_exceptions_binds_pinned_ddi0602_semantics",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ BASE_EXCEPTIONS_SOURCE, BASE_EXCEPTIONS_SUITE,
	  BASE_EXCEPTIONS_SUITE_SYMBOL, BASE_EXCEPTIONS_CASE_ARRAY,
	  "orlix_tcti_base_exceptions_production_resume",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ BASE_EXCEPTIONS_SOURCE, BASE_EXCEPTIONS_SUITE,
	  BASE_EXCEPTIONS_SUITE_SYMBOL, BASE_EXCEPTIONS_CASE_ARRAY,
	  "orlix_tcti_base_exceptions_el0_traps",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ BASE_EXCEPTIONS_SOURCE, BASE_EXCEPTIONS_SUITE,
	  BASE_EXCEPTIONS_SUITE_SYMBOL, BASE_EXCEPTIONS_CASE_ARRAY,
	  "orlix_tcti_base_exceptions_syscall_handoff",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ BASE_EXCEPTIONS_SOURCE, BASE_EXCEPTIONS_SUITE,
	  BASE_EXCEPTIONS_SUITE_SYMBOL, BASE_EXCEPTIONS_CASE_ARRAY,
	  "orlix_tcti_base_exceptions_simd_and_flags_unchanged",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ BASE_EXCEPTIONS_SOURCE, BASE_EXCEPTIONS_SUITE,
	  BASE_EXCEPTIONS_SUITE_SYMBOL, BASE_EXCEPTIONS_CASE_ARRAY,
	  "orlix_tcti_base_exceptions_non_el0_rejected",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ BASE_EXCEPTIONS_SOURCE, BASE_EXCEPTIONS_SUITE,
	  BASE_EXCEPTIONS_SUITE_SYMBOL, BASE_EXCEPTIONS_CASE_ARRAY,
	  "orlix_tcti_base_exceptions_reserved_encodings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ BASE_CONDITIONAL_SOURCE, BASE_CONDITIONAL_SUITE,
	  BASE_CONDITIONAL_SUITE_SYMBOL, BASE_CONDITIONAL_CASE_ARRAY,
	  "orlix_tcti_base_conditional_decodes_exact_source_cohort",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ BASE_CONDITIONAL_SOURCE, BASE_CONDITIONAL_SUITE,
	  BASE_CONDITIONAL_SUITE_SYMBOL, BASE_CONDITIONAL_CASE_ARRAY,
	  "orlix_tcti_base_conditional_binds_pinned_ddi0602_semantics",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ BASE_CONDITIONAL_SOURCE, BASE_CONDITIONAL_SUITE,
	  BASE_CONDITIONAL_SUITE_SYMBOL, BASE_CONDITIONAL_CASE_ARRAY,
	  "orlix_tcti_base_conditional_production_resume",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ BASE_CONDITIONAL_SOURCE, BASE_CONDITIONAL_SUITE,
	  BASE_CONDITIONAL_SUITE_SYMBOL, BASE_CONDITIONAL_CASE_ARRAY,
	  "orlix_tcti_base_conditional_taken_and_not_taken",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ BASE_CONDITIONAL_SOURCE, BASE_CONDITIONAL_SUITE,
	  BASE_CONDITIONAL_SUITE_SYMBOL, BASE_CONDITIONAL_CASE_ARRAY,
	  "orlix_tcti_base_conditional_csel_aliases_and_w_upper",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ BASE_CONDITIONAL_SOURCE, BASE_CONDITIONAL_SUITE,
	  BASE_CONDITIONAL_SUITE_SYMBOL, BASE_CONDITIONAL_CASE_ARRAY,
	  "orlix_tcti_base_conditional_ccmp_true_false_nzcv",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ BASE_CONDITIONAL_SOURCE, BASE_CONDITIONAL_SUITE,
	  BASE_CONDITIONAL_SUITE_SYMBOL, BASE_CONDITIONAL_CASE_ARRAY,
	  "orlix_tcti_base_conditional_simd_and_flags_unchanged",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ BASE_CONDITIONAL_SOURCE, BASE_CONDITIONAL_SUITE,
	  BASE_CONDITIONAL_SUITE_SYMBOL, BASE_CONDITIONAL_CASE_ARRAY,
	  "orlix_tcti_base_conditional_non_el0_rejected",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ BASE_CONDITIONAL_SOURCE, BASE_CONDITIONAL_SUITE,
	  BASE_CONDITIONAL_SUITE_SYMBOL, BASE_CONDITIONAL_CASE_ARRAY,
	  "orlix_tcti_base_conditional_reserved_encodings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ BASE_BITFIELD_UNARY_SOURCE, BASE_BITFIELD_UNARY_SUITE,
	  BASE_BITFIELD_UNARY_SUITE_SYMBOL, BASE_BITFIELD_UNARY_CASE_ARRAY,
	  "orlix_tcti_base_bitfield_unary_decodes_exact_source_cohort",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ BASE_BITFIELD_UNARY_SOURCE, BASE_BITFIELD_UNARY_SUITE,
	  BASE_BITFIELD_UNARY_SUITE_SYMBOL, BASE_BITFIELD_UNARY_CASE_ARRAY,
	  "orlix_tcti_base_bitfield_unary_binds_pinned_ddi0602_semantics",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ BASE_BITFIELD_UNARY_SOURCE, BASE_BITFIELD_UNARY_SUITE,
	  BASE_BITFIELD_UNARY_SUITE_SYMBOL, BASE_BITFIELD_UNARY_CASE_ARRAY,
	  "orlix_tcti_base_bitfield_unary_production_resume",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ BASE_BITFIELD_UNARY_SOURCE, BASE_BITFIELD_UNARY_SUITE,
	  BASE_BITFIELD_UNARY_SUITE_SYMBOL, BASE_BITFIELD_UNARY_CASE_ARRAY,
	  "orlix_tcti_base_bitfield_unary_aliases_and_w_upper",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ BASE_BITFIELD_UNARY_SOURCE, BASE_BITFIELD_UNARY_SUITE,
	  BASE_BITFIELD_UNARY_SUITE_SYMBOL, BASE_BITFIELD_UNARY_CASE_ARRAY,
	  "orlix_tcti_base_bitfield_unary_extract_overlap",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ BASE_BITFIELD_UNARY_SOURCE, BASE_BITFIELD_UNARY_SUITE,
	  BASE_BITFIELD_UNARY_SUITE_SYMBOL, BASE_BITFIELD_UNARY_CASE_ARRAY,
	  "orlix_tcti_base_bitfield_unary_rbit_rev_clz",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ BASE_BITFIELD_UNARY_SOURCE, BASE_BITFIELD_UNARY_SUITE,
	  BASE_BITFIELD_UNARY_SUITE_SYMBOL, BASE_BITFIELD_UNARY_CASE_ARRAY,
	  "orlix_tcti_base_bitfield_unary_simd_and_flags_unchanged",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ BASE_BITFIELD_UNARY_SOURCE, BASE_BITFIELD_UNARY_SUITE,
	  BASE_BITFIELD_UNARY_SUITE_SYMBOL, BASE_BITFIELD_UNARY_CASE_ARRAY,
	  "orlix_tcti_base_bitfield_unary_non_el0_rejected",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ BASE_BITFIELD_UNARY_SOURCE, BASE_BITFIELD_UNARY_SUITE,
	  BASE_BITFIELD_UNARY_SUITE_SYMBOL, BASE_BITFIELD_UNARY_CASE_ARRAY,
	  "orlix_tcti_base_bitfield_unary_reserved_encodings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ BASE_MULTIPLY_DIVIDE_SOURCE, BASE_MULTIPLY_DIVIDE_SUITE,
	  BASE_MULTIPLY_DIVIDE_SUITE_SYMBOL, BASE_MULTIPLY_DIVIDE_CASE_ARRAY,
	  "orlix_tcti_base_multiply_divide_decodes_exact_source_cohort",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ BASE_MULTIPLY_DIVIDE_SOURCE, BASE_MULTIPLY_DIVIDE_SUITE,
	  BASE_MULTIPLY_DIVIDE_SUITE_SYMBOL, BASE_MULTIPLY_DIVIDE_CASE_ARRAY,
	  "orlix_tcti_base_multiply_divide_binds_pinned_ddi0602_semantics",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ BASE_MULTIPLY_DIVIDE_SOURCE, BASE_MULTIPLY_DIVIDE_SUITE,
	  BASE_MULTIPLY_DIVIDE_SUITE_SYMBOL, BASE_MULTIPLY_DIVIDE_CASE_ARRAY,
	  "orlix_tcti_base_multiply_divide_production_resume",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ BASE_MULTIPLY_DIVIDE_SOURCE, BASE_MULTIPLY_DIVIDE_SUITE,
	  BASE_MULTIPLY_DIVIDE_SUITE_SYMBOL, BASE_MULTIPLY_DIVIDE_CASE_ARRAY,
	  "orlix_tcti_base_multiply_divide_divide_zero_signed_min_w_upper",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ BASE_MULTIPLY_DIVIDE_SOURCE, BASE_MULTIPLY_DIVIDE_SUITE,
	  BASE_MULTIPLY_DIVIDE_SUITE_SYMBOL, BASE_MULTIPLY_DIVIDE_CASE_ARRAY,
	  "orlix_tcti_base_multiply_divide_long_form_high_half",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ BASE_MULTIPLY_DIVIDE_SOURCE, BASE_MULTIPLY_DIVIDE_SUITE,
	  BASE_MULTIPLY_DIVIDE_SUITE_SYMBOL, BASE_MULTIPLY_DIVIDE_CASE_ARRAY,
	  "orlix_tcti_base_multiply_divide_accumulator_alias_xzr",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ BASE_MULTIPLY_DIVIDE_SOURCE, BASE_MULTIPLY_DIVIDE_SUITE,
	  BASE_MULTIPLY_DIVIDE_SUITE_SYMBOL, BASE_MULTIPLY_DIVIDE_CASE_ARRAY,
	  "orlix_tcti_base_multiply_divide_simd_and_flags_unchanged",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ BASE_MULTIPLY_DIVIDE_SOURCE, BASE_MULTIPLY_DIVIDE_SUITE,
	  BASE_MULTIPLY_DIVIDE_SUITE_SYMBOL, BASE_MULTIPLY_DIVIDE_CASE_ARRAY,
	  "orlix_tcti_base_multiply_divide_pointer_preserves_tag",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ BASE_MULTIPLY_DIVIDE_SOURCE, BASE_MULTIPLY_DIVIDE_SUITE,
	  BASE_MULTIPLY_DIVIDE_SUITE_SYMBOL, BASE_MULTIPLY_DIVIDE_CASE_ARRAY,
	  "orlix_tcti_base_multiply_divide_reserved_encodings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ ADVSIMD_CRYPTO_SOURCE, ADVSIMD_CRYPTO_SUITE,
	  ADVSIMD_CRYPTO_SUITE_SYMBOL, ADVSIMD_CRYPTO_CASE_ARRAY,
	  "orlix_tcti_advsimd_crypto_decodes_exact_source_cohort",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ ADVSIMD_CRYPTO_SOURCE, ADVSIMD_CRYPTO_SUITE,
	  ADVSIMD_CRYPTO_SUITE_SYMBOL, ADVSIMD_CRYPTO_CASE_ARRAY,
	  "orlix_tcti_advsimd_crypto_binds_pinned_ddi0602_semantics",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ ADVSIMD_CRYPTO_SOURCE, ADVSIMD_CRYPTO_SUITE,
	  ADVSIMD_CRYPTO_SUITE_SYMBOL, ADVSIMD_CRYPTO_CASE_ARRAY,
	  "orlix_tcti_advsimd_crypto_production_resume",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ ADVSIMD_CRYPTO_SOURCE, ADVSIMD_CRYPTO_SUITE,
	  ADVSIMD_CRYPTO_SUITE_SYMBOL, ADVSIMD_CRYPTO_CASE_ARRAY,
	  "orlix_tcti_advsimd_crypto_destructive_alias",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ ADVSIMD_CRYPTO_SOURCE, ADVSIMD_CRYPTO_SUITE,
	  ADVSIMD_CRYPTO_SUITE_SYMBOL, ADVSIMD_CRYPTO_CASE_ARRAY,
	  "orlix_tcti_advsimd_crypto_pmull_halves_polynomial",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ ADVSIMD_CRYPTO_SOURCE, ADVSIMD_CRYPTO_SUITE,
	  ADVSIMD_CRYPTO_SUITE_SYMBOL, ADVSIMD_CRYPTO_CASE_ARRAY,
	  "orlix_tcti_advsimd_crypto_aes_round_upper_gprs",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ ADVSIMD_CRYPTO_SOURCE, ADVSIMD_CRYPTO_SUITE,
	  ADVSIMD_CRYPTO_SUITE_SYMBOL, ADVSIMD_CRYPTO_CASE_ARRAY,
	  "orlix_tcti_advsimd_crypto_reserved_encodings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ ADVSIMD_PERMUTE_MOVE_SOURCE, ADVSIMD_PERMUTE_MOVE_SUITE,
	  ADVSIMD_PERMUTE_MOVE_SUITE_SYMBOL, ADVSIMD_PERMUTE_MOVE_CASE_ARRAY,
	  "orlix_tcti_advsimd_permute_move_decodes_exact_source_cohort",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ ADVSIMD_PERMUTE_MOVE_SOURCE, ADVSIMD_PERMUTE_MOVE_SUITE,
	  ADVSIMD_PERMUTE_MOVE_SUITE_SYMBOL, ADVSIMD_PERMUTE_MOVE_CASE_ARRAY,
	  "orlix_tcti_advsimd_permute_move_binds_pinned_ddi0602_semantics",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ ADVSIMD_PERMUTE_MOVE_SOURCE, ADVSIMD_PERMUTE_MOVE_SUITE,
	  ADVSIMD_PERMUTE_MOVE_SUITE_SYMBOL, ADVSIMD_PERMUTE_MOVE_CASE_ARRAY,
	  "orlix_tcti_advsimd_permute_move_production_resume",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ ADVSIMD_PERMUTE_MOVE_SOURCE, ADVSIMD_PERMUTE_MOVE_SUITE,
	  ADVSIMD_PERMUTE_MOVE_SUITE_SYMBOL, ADVSIMD_PERMUTE_MOVE_CASE_ARRAY,
	  "orlix_tcti_advsimd_permute_move_non_el0_rejected",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ ADVSIMD_PERMUTE_MOVE_SOURCE, ADVSIMD_PERMUTE_MOVE_SUITE,
	  ADVSIMD_PERMUTE_MOVE_SUITE_SYMBOL, ADVSIMD_PERMUTE_MOVE_CASE_ARRAY,
	  "orlix_tcti_advsimd_permute_move_lane_q0_overlap",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ ADVSIMD_PERMUTE_MOVE_SOURCE, ADVSIMD_PERMUTE_MOVE_SUITE,
	  ADVSIMD_PERMUTE_MOVE_SUITE_SYMBOL, ADVSIMD_PERMUTE_MOVE_CASE_ARRAY,
	  "orlix_tcti_advsimd_permute_move_tbl_tbx_smov",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ ADVSIMD_PERMUTE_MOVE_SOURCE, ADVSIMD_PERMUTE_MOVE_SUITE,
	  ADVSIMD_PERMUTE_MOVE_SUITE_SYMBOL, ADVSIMD_PERMUTE_MOVE_CASE_ARRAY,
	  "orlix_tcti_advsimd_permute_move_reserved_encodings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ ADVSIMD_INTEGER_SOURCE, ADVSIMD_INTEGER_SUITE,
	  ADVSIMD_INTEGER_SUITE_SYMBOL, ADVSIMD_INTEGER_CASE_ARRAY,
	  "orlix_tcti_advsimd_integer_decodes_exact_source_cohort",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ ADVSIMD_INTEGER_SOURCE, ADVSIMD_INTEGER_SUITE,
	  ADVSIMD_INTEGER_SUITE_SYMBOL, ADVSIMD_INTEGER_CASE_ARRAY,
	  "orlix_tcti_advsimd_integer_binds_pinned_ddi0602_semantics",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ ADVSIMD_INTEGER_SOURCE, ADVSIMD_INTEGER_SUITE,
	  ADVSIMD_INTEGER_SUITE_SYMBOL, ADVSIMD_INTEGER_CASE_ARRAY,
	  "orlix_tcti_advsimd_integer_production_resume",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ ADVSIMD_INTEGER_SOURCE, ADVSIMD_INTEGER_SUITE,
	  ADVSIMD_INTEGER_SUITE_SYMBOL, ADVSIMD_INTEGER_CASE_ARRAY,
	  "orlix_tcti_advsimd_integer_non_el0_rejected",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ ADVSIMD_INTEGER_SOURCE, ADVSIMD_INTEGER_SUITE,
	  ADVSIMD_INTEGER_SUITE_SYMBOL, ADVSIMD_INTEGER_CASE_ARRAY,
	  "orlix_tcti_advsimd_integer_saturation_qc",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ ADVSIMD_INTEGER_SOURCE, ADVSIMD_INTEGER_SUITE,
	  ADVSIMD_INTEGER_SUITE_SYMBOL, ADVSIMD_INTEGER_CASE_ARRAY,
	  "orlix_tcti_advsimd_integer_narrow_shift_overlap",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ ADVSIMD_INTEGER_SOURCE, ADVSIMD_INTEGER_SUITE,
	  ADVSIMD_INTEGER_SUITE_SYMBOL, ADVSIMD_INTEGER_CASE_ARRAY,
	  "orlix_tcti_advsimd_integer_reserved_encodings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ ADVSIMD_FP_SOURCE, ADVSIMD_FP_SUITE,
	  ADVSIMD_FP_SUITE_SYMBOL, ADVSIMD_FP_CASE_ARRAY,
	  "orlix_tcti_advsimd_fp_decodes_exact_source_cohort",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ ADVSIMD_FP_SOURCE, ADVSIMD_FP_SUITE,
	  ADVSIMD_FP_SUITE_SYMBOL, ADVSIMD_FP_CASE_ARRAY,
	  "orlix_tcti_advsimd_fp_binds_pinned_ddi0602_semantics",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ ADVSIMD_FP_SOURCE, ADVSIMD_FP_SUITE,
	  ADVSIMD_FP_SUITE_SYMBOL, ADVSIMD_FP_CASE_ARRAY,
	  "orlix_tcti_advsimd_fp_production_resume",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ ADVSIMD_FP_SOURCE, ADVSIMD_FP_SUITE,
	  ADVSIMD_FP_SUITE_SYMBOL, ADVSIMD_FP_CASE_ARRAY,
	  "orlix_tcti_advsimd_fp_non_el0_rejected",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ ADVSIMD_FP_SOURCE, ADVSIMD_FP_SUITE,
	  ADVSIMD_FP_SUITE_SYMBOL, ADVSIMD_FP_CASE_ARRAY,
	  "orlix_tcti_advsimd_fp_nan_rounding_overlap",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ ADVSIMD_FP_SOURCE, ADVSIMD_FP_SUITE,
	  ADVSIMD_FP_SUITE_SYMBOL, ADVSIMD_FP_CASE_ARRAY,
	  "orlix_tcti_advsimd_fp_reserved_encodings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ SCALAR_FP_FAMILY_SOURCE, SCALAR_FP_FAMILY_SUITE,
	  SCALAR_FP_FAMILY_SUITE_SYMBOL, SCALAR_FP_FAMILY_CASE_ARRAY,
	  "orlix_tcti_scalar_fp_decodes_exact_source_cohort",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ SCALAR_FP_FAMILY_SOURCE, SCALAR_FP_FAMILY_SUITE,
	  SCALAR_FP_FAMILY_SUITE_SYMBOL, SCALAR_FP_FAMILY_CASE_ARRAY,
	  "orlix_tcti_scalar_fp_binds_pinned_ddi0602_semantics",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ SCALAR_FP_FAMILY_SOURCE, SCALAR_FP_FAMILY_SUITE,
	  SCALAR_FP_FAMILY_SUITE_SYMBOL, SCALAR_FP_FAMILY_CASE_ARRAY,
	  "orlix_tcti_scalar_fp_production_resume",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ SCALAR_FP_FAMILY_SOURCE, SCALAR_FP_FAMILY_SUITE,
	  SCALAR_FP_FAMILY_SUITE_SYMBOL, SCALAR_FP_FAMILY_CASE_ARRAY,
	  "orlix_tcti_scalar_fp_optional_rejected",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ SCALAR_FP_FAMILY_SOURCE, SCALAR_FP_FAMILY_SUITE,
	  SCALAR_FP_FAMILY_SUITE_SYMBOL, SCALAR_FP_FAMILY_CASE_ARRAY,
	  "orlix_tcti_scalar_fp_edge_vectors",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ SCALAR_FP_FAMILY_SOURCE, SCALAR_FP_FAMILY_SUITE,
	  SCALAR_FP_FAMILY_SUITE_SYMBOL, SCALAR_FP_FAMILY_CASE_ARRAY,
	  "orlix_tcti_scalar_fp_reserved_encodings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ SOURCE_LEAF_CLASSIFICATION_SOURCE,
	  SOURCE_LEAF_CLASSIFICATION_SUITE,
	  SOURCE_LEAF_CLASSIFICATION_SUITE_SYMBOL,
	  SOURCE_LEAF_CLASSIFICATION_CASE_ARRAY,
	  "orlix_tcti_source_leaf_rejections_match_pinned_tuples",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ SOURCE_LEAF_CLASSIFICATION_SOURCE,
	  SOURCE_LEAF_CLASSIFICATION_SUITE,
	  SOURCE_LEAF_CLASSIFICATION_SUITE_SYMBOL,
	  SOURCE_LEAF_CLASSIFICATION_CASE_ARRAY,
	  "orlix_tcti_source_leaf_rejections_are_structured_el0_exits",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
#define ORLIX_TCTI_PROOF_FAMILY_METADATA(source_value, source_sha256_value, \
		object_value, suite_value, suite_symbol_value, case_array_value, \
		decode_case_value, production_case_value) \
	{ source_value, suite_value, suite_symbol_value, case_array_value, \
	  decode_case_value, ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE | \
		ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS }, \
	{ source_value, suite_value, suite_symbol_value, case_array_value, \
	  production_case_value, ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS | \
		ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
#include "target_production_capture_family.def"
#undef ORLIX_TCTI_PROOF_FAMILY_METADATA
	{ TRANSLATION_CHANGE_SOURCE, TRANSLATION_CHANGE_SUITE,
	  TRANSLATION_CHANGE_SUITE_SYMBOL, TRANSLATION_CHANGE_CASE_ARRAY,
	  "tchange_source_and_feature_domain",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ TRANSLATION_CHANGE_SOURCE, TRANSLATION_CHANGE_SUITE,
	  TRANSLATION_CHANGE_SUITE_SYMBOL, TRANSLATION_CHANGE_CASE_ARRAY,
	  "tchange_all_source_encodings_decode_unsupported",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ TRANSLATION_CHANGE_SOURCE, TRANSLATION_CHANGE_SUITE,
	  TRANSLATION_CHANGE_SUITE_SYMBOL, TRANSLATION_CHANGE_CASE_ARRAY,
	  "tchange_production_el0_rejection_preserves_state",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS },
	{ DECODE_SOURCE, DECODE_SUITE, DECODE_SUITE_SYMBOL, DECODE_CASE_ARRAY,
	  "orlix_tcti_decode_exhaustive_load_store_unsigned_immediate_family",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ DECODE_SOURCE, DECODE_SUITE, DECODE_SUITE_SYMBOL, DECODE_CASE_ARRAY,
	  "orlix_tcti_decode_exhaustive_load_store_signed_immediate_family",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ DECODE_SOURCE, DECODE_SUITE, DECODE_SUITE_SYMBOL, DECODE_CASE_ARRAY,
	  "orlix_tcti_decode_exhaustive_load_store_register_offset_family",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ BASE_LOAD_STORE_SOURCE, BASE_LOAD_STORE_SUITE,
	  BASE_LOAD_STORE_SUITE_SYMBOL, BASE_LOAD_STORE_CASE_ARRAY,
	  "bls_source_decode",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ BASE_LOAD_STORE_SOURCE, BASE_LOAD_STORE_SUITE,
	  BASE_LOAD_STORE_SUITE_SYMBOL, BASE_LOAD_STORE_CASE_ARRAY,
	  "bls_reserved_is_rejected",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ BASE_LOAD_STORE_SOURCE, BASE_LOAD_STORE_SUITE,
	  BASE_LOAD_STORE_SUITE_SYMBOL, BASE_LOAD_STORE_CASE_ARRAY,
	  "bls_production_resume",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ BASE_LOAD_STORE_SOURCE, BASE_LOAD_STORE_SUITE,
	  BASE_LOAD_STORE_SUITE_SYMBOL, BASE_LOAD_STORE_CASE_ARRAY,
	  "bls_unmapped_load_faults",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS },
	{ BASE_LOAD_STORE_SOURCE, BASE_LOAD_STORE_SUITE,
	  BASE_LOAD_STORE_SUITE_SYMBOL, BASE_LOAD_STORE_CASE_ARRAY,
	  "bls_write_protect_store_faults",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY },
	{ BASE_LOAD_STORE_SOURCE, BASE_LOAD_STORE_SUITE,
	  BASE_LOAD_STORE_SUITE_SYMBOL, BASE_LOAD_STORE_CASE_ARRAY,
	  "bls_pair_second_element_faults",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS },
	{ BASE_LOAD_STORE_SOURCE, BASE_LOAD_STORE_SUITE,
	  BASE_LOAD_STORE_SUITE_SYMBOL, BASE_LOAD_STORE_CASE_ARRAY,
	  "bls_prefetch_is_not_a_fault",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ BASE_LOAD_STORE_SOURCE, BASE_LOAD_STORE_SUITE,
	  BASE_LOAD_STORE_SUITE_SYMBOL, BASE_LOAD_STORE_CASE_ARRAY,
	  "bls_page_cross_single_faults",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS },
	{ BASE_LOAD_STORE_SOURCE, BASE_LOAD_STORE_SUITE,
	  BASE_LOAD_STORE_SUITE_SYMBOL, BASE_LOAD_STORE_CASE_ARRAY,
	  "bls_regoff_uses_rm",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY },
	{ BASE_LOAD_STORE_SOURCE, BASE_LOAD_STORE_SUITE,
	  BASE_LOAD_STORE_SUITE_SYMBOL, BASE_LOAD_STORE_CASE_ARRAY,
	  "bls_xzr_and_sp_base",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY },
	{ ADVSIMD_LOAD_STORE_SOURCE, ADVSIMD_LOAD_STORE_SUITE,
	  ADVSIMD_LOAD_STORE_SUITE_SYMBOL, ADVSIMD_LOAD_STORE_CASE_ARRAY,
	  "orlix_tcti_advsimd_structure_every_source_leaf_reaches_decoder",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ ADVSIMD_LOAD_STORE_SOURCE, ADVSIMD_LOAD_STORE_SUITE,
	  ADVSIMD_LOAD_STORE_SUITE_SYMBOL, ADVSIMD_LOAD_STORE_CASE_ARRAY,
	  "orlix_tcti_advsimd_structure_every_contract_row_executes",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ ADVSIMD_LOAD_STORE_SOURCE, ADVSIMD_LOAD_STORE_SUITE,
	  ADVSIMD_LOAD_STORE_SUITE_SYMBOL, ADVSIMD_LOAD_STORE_CASE_ARRAY,
	  "orlix_tcti_advsimd_structure_single_lane_writeback_and_pc",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ ADVSIMD_LOAD_STORE_SOURCE, ADVSIMD_LOAD_STORE_SUITE,
	  ADVSIMD_LOAD_STORE_SUITE_SYMBOL, ADVSIMD_LOAD_STORE_CASE_ARRAY,
	  "orlix_tcti_advsimd_structure_replicate_and_pc",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ ADVSIMD_LOAD_STORE_SOURCE, ADVSIMD_LOAD_STORE_SUITE,
	  ADVSIMD_LOAD_STORE_SUITE_SYMBOL, ADVSIMD_LOAD_STORE_CASE_ARRAY,
	  "orlix_tcti_advsimd_structure_fault_preserves_pc_and_writeback",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS },
	{ ADVSIMD_LOAD_STORE_SOURCE, ADVSIMD_LOAD_STORE_SUITE,
	  ADVSIMD_LOAD_STORE_SUITE_SYMBOL, ADVSIMD_LOAD_STORE_CASE_ARRAY,
	  "orlix_tcti_advsimd_structure_write_fault_preserves_full_state",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS },
	{ ADVSIMD_LOAD_STORE_SOURCE, ADVSIMD_LOAD_STORE_SUITE,
	  ADVSIMD_LOAD_STORE_SUITE_SYMBOL, ADVSIMD_LOAD_STORE_CASE_ARRAY,
	  "orlix_tcti_advsimd_structure_reserved_encodings_reject",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ ADVSIMD_LOAD_STORE_SOURCE, ADVSIMD_LOAD_STORE_SUITE,
	  ADVSIMD_LOAD_STORE_SUITE_SYMBOL, ADVSIMD_LOAD_STORE_CASE_ARRAY,
	  "asl_production_resume",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ ADVSIMD_LOAD_STORE_SOURCE, ADVSIMD_LOAD_STORE_SUITE,
	  ADVSIMD_LOAD_STORE_SUITE_SYMBOL, ADVSIMD_LOAD_STORE_CASE_ARRAY,
	  "asl_unmapped_load_faults",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS },
	{ ADVSIMD_LOAD_STORE_SOURCE, ADVSIMD_LOAD_STORE_SUITE,
	  ADVSIMD_LOAD_STORE_SUITE_SYMBOL, ADVSIMD_LOAD_STORE_CASE_ARRAY,
	  "asl_write_protect_store_faults",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY },
	{ ADVSIMD_LOAD_STORE_SOURCE, ADVSIMD_LOAD_STORE_SUITE,
	  ADVSIMD_LOAD_STORE_SUITE_SYMBOL, ADVSIMD_LOAD_STORE_CASE_ARRAY,
	  "asl_page_cross_second_element_faults",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS },
	{ ADVSIMD_LOAD_STORE_SOURCE, ADVSIMD_LOAD_STORE_SUITE,
	  ADVSIMD_LOAD_STORE_SUITE_SYMBOL, ADVSIMD_LOAD_STORE_CASE_ARRAY,
	  "asl_postindex_uses_rm",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY },
	{ SCALAR_FP_SOURCE, SCALAR_FP_SUITE, SCALAR_FP_SUITE_SYMBOL,
	  SCALAR_FP_CASE_ARRAY, "orlix_tcti_scalar_fp_convert_resume_source_rows",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ SCALAR_FP_SOURCE, SCALAR_FP_SUITE, SCALAR_FP_SUITE_SYMBOL,
	  SCALAR_FP_CASE_ARRAY,
	  "orlix_tcti_scalar_fp_convert_reserved_forms_exit_without_state_change",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ INTEGER_CONDITIONAL_SOURCE, INTEGER_CONDITIONAL_SUITE,
	  INTEGER_CONDITIONAL_SUITE_SYMBOL, INTEGER_CONDITIONAL_CASE_ARRAY,
	  "orlix_tcti_integer_conditional_source_bindings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ INTEGER_CONDITIONAL_SOURCE, INTEGER_CONDITIONAL_SUITE,
	  INTEGER_CONDITIONAL_SUITE_SYMBOL, INTEGER_CONDITIONAL_CASE_ARRAY,
	  "orlix_tcti_integer_conditional_all_divide_leaves_production_path",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ INTEGER_CONDITIONAL_SOURCE, INTEGER_CONDITIONAL_SUITE,
	  INTEGER_CONDITIONAL_SUITE_SYMBOL, INTEGER_CONDITIONAL_CASE_ARRAY,
	  "orlix_tcti_integer_conditional_compare_leaves_production_path",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ INTEGER_CONDITIONAL_SOURCE, INTEGER_CONDITIONAL_SUITE,
	  INTEGER_CONDITIONAL_SUITE_SYMBOL, INTEGER_CONDITIONAL_CASE_ARRAY,
	  "orlix_tcti_integer_conditional_select_leaves_production_path",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ INTEGER_CONDITIONAL_SOURCE, INTEGER_CONDITIONAL_SUITE,
	  INTEGER_CONDITIONAL_SUITE_SYMBOL, INTEGER_CONDITIONAL_CASE_ARRAY,
	  "orlix_tcti_integer_conditional_multiply_leaves_production_path",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ INTEGER_CONDITIONAL_SOURCE, INTEGER_CONDITIONAL_SUITE,
	  INTEGER_CONDITIONAL_SUITE_SYMBOL, INTEGER_CONDITIONAL_CASE_ARRAY,
	  "orlix_tcti_integer_conditional_reserved_encodings_fail_before_state",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ ADVSIMD_FP_ARITHMETIC_SOURCE, ADVSIMD_FP_ARITHMETIC_SUITE,
	  ADVSIMD_FP_ARITHMETIC_SUITE_SYMBOL, ADVSIMD_FP_ARITHMETIC_CASE_ARRAY,
	  "orlix_tcti_advsimd_fp_fmla_source_leaf_execute_exact_bits",
	  ADVSIMD_FP_ARITHMETIC_OBLIGATIONS },
	{ ADVSIMD_FP_ARITHMETIC_SOURCE, ADVSIMD_FP_ARITHMETIC_SUITE,
	  ADVSIMD_FP_ARITHMETIC_SUITE_SYMBOL, ADVSIMD_FP_ARITHMETIC_CASE_ARRAY,
	  "orlix_tcti_advsimd_fp_fadd_source_leaf_execute_exact_bits",
	  ADVSIMD_FP_ARITHMETIC_OBLIGATIONS },
	{ ADVSIMD_FP_ARITHMETIC_SOURCE, ADVSIMD_FP_ARITHMETIC_SUITE,
	  ADVSIMD_FP_ARITHMETIC_SUITE_SYMBOL, ADVSIMD_FP_ARITHMETIC_CASE_ARRAY,
	  "orlix_tcti_advsimd_fp_fcmeq_source_leaf_execute_exact_bits",
	  ADVSIMD_FP_ARITHMETIC_OBLIGATIONS },
	{ ADVSIMD_FP_ARITHMETIC_SOURCE, ADVSIMD_FP_ARITHMETIC_SUITE,
	  ADVSIMD_FP_ARITHMETIC_SUITE_SYMBOL, ADVSIMD_FP_ARITHMETIC_CASE_ARRAY,
	  "orlix_tcti_advsimd_fp_fmax_source_leaf_execute_exact_bits",
	  ADVSIMD_FP_ARITHMETIC_OBLIGATIONS },
	{ ADVSIMD_FP_ARITHMETIC_SOURCE, ADVSIMD_FP_ARITHMETIC_SUITE,
	  ADVSIMD_FP_ARITHMETIC_SUITE_SYMBOL, ADVSIMD_FP_ARITHMETIC_CASE_ARRAY,
	  "orlix_tcti_advsimd_fp_fsub_source_leaf_execute_exact_bits",
	  ADVSIMD_FP_ARITHMETIC_OBLIGATIONS },
	{ ADVSIMD_FP_ARITHMETIC_SOURCE, ADVSIMD_FP_ARITHMETIC_SUITE,
	  ADVSIMD_FP_ARITHMETIC_SUITE_SYMBOL, ADVSIMD_FP_ARITHMETIC_CASE_ARRAY,
	  "orlix_tcti_advsimd_fp_fmin_source_leaf_execute_exact_bits",
	  ADVSIMD_FP_ARITHMETIC_OBLIGATIONS },
	{ ADVSIMD_FP_ARITHMETIC_SOURCE, ADVSIMD_FP_ARITHMETIC_SUITE,
	  ADVSIMD_FP_ARITHMETIC_SUITE_SYMBOL, ADVSIMD_FP_ARITHMETIC_CASE_ARRAY,
	  "orlix_tcti_advsimd_fp_fmul_source_leaf_execute_exact_bits",
	  ADVSIMD_FP_ARITHMETIC_OBLIGATIONS },
	{ ADVSIMD_FP_ARITHMETIC_SOURCE, ADVSIMD_FP_ARITHMETIC_SUITE,
	  ADVSIMD_FP_ARITHMETIC_SUITE_SYMBOL, ADVSIMD_FP_ARITHMETIC_CASE_ARRAY,
	  "orlix_tcti_advsimd_fp_fdiv_source_leaf_execute_exact_bits",
	  ADVSIMD_FP_ARITHMETIC_OBLIGATIONS },
	{ ADVSIMD_HALVING_SOURCE, ADVSIMD_HALVING_SUITE,
	  ADVSIMD_HALVING_SUITE_SYMBOL, ADVSIMD_HALVING_CASE_ARRAY,
	  "orlix_tcti_advsimd_halving_source_leaves_decode",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ ADVSIMD_HALVING_SOURCE, ADVSIMD_HALVING_SUITE,
	  ADVSIMD_HALVING_SUITE_SYMBOL, ADVSIMD_HALVING_CASE_ARRAY,
	  "orlix_tcti_advsimd_halving_reserved_64bit_lanes_reject",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ ADVSIMD_HALVING_SOURCE, ADVSIMD_HALVING_SUITE,
	  ADVSIMD_HALVING_SUITE_SYMBOL, ADVSIMD_HALVING_CASE_ARRAY,
	  "orlix_tcti_advsimd_halving_source_leaves_execute",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ ADVSIMD_MUL_SOURCE, ADVSIMD_MUL_SUITE, ADVSIMD_MUL_SUITE_SYMBOL,
	  ADVSIMD_MUL_CASE_ARRAY, "advsimd_mul_bind_canonical_artifacts",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ ADVSIMD_MUL_SOURCE, ADVSIMD_MUL_SUITE, ADVSIMD_MUL_SUITE_SYMBOL,
	  ADVSIMD_MUL_CASE_ARRAY, "advsimd_mul_execute_all_aliases",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ ADVSIMD_MUL_SOURCE, ADVSIMD_MUL_SUITE, ADVSIMD_MUL_SUITE_SYMBOL,
	  ADVSIMD_MUL_CASE_ARRAY,
	  "advsimd_mul_reserved_forms_reject_without_mutation",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ ADVSIMD_MINMAX_REDUCTION_SOURCE, ADVSIMD_MINMAX_REDUCTION_SUITE,
	  ADVSIMD_MINMAX_REDUCTION_SUITE_SYMBOL,
	  ADVSIMD_MINMAX_REDUCTION_CASE_ARRAY,
	  "orlix_tcti_minmaxv_source_bindings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ ADVSIMD_MINMAX_REDUCTION_SOURCE, ADVSIMD_MINMAX_REDUCTION_SUITE,
	  ADVSIMD_MINMAX_REDUCTION_SUITE_SYMBOL,
	  ADVSIMD_MINMAX_REDUCTION_CASE_ARRAY,
	  "orlix_tcti_minmaxv_complete_encoding_domain",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ ADVSIMD_MINMAX_REDUCTION_SOURCE, ADVSIMD_MINMAX_REDUCTION_SUITE,
	  ADVSIMD_MINMAX_REDUCTION_SUITE_SYMBOL,
	  ADVSIMD_MINMAX_REDUCTION_CASE_ARRAY,
	  "orlix_tcti_minmaxv_legal_arrangements_resume",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ ADVSIMD_MINMAX_REDUCTION_SOURCE, ADVSIMD_MINMAX_REDUCTION_SUITE,
	  ADVSIMD_MINMAX_REDUCTION_SUITE_SYMBOL,
	  ADVSIMD_MINMAX_REDUCTION_CASE_ARRAY,
	  "orlix_tcti_minmaxv_reserved_arrangements_resume",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ MOPS_COPY_SOURCE, MOPS_COPY_SUITE, MOPS_COPY_SUITE_SYMBOL,
	  MOPS_COPY_CASE_ARRAY, "mops_decode_all_96_source_leaves",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ MOPS_COPY_SOURCE, MOPS_COPY_SUITE, MOPS_COPY_SUITE_SYMBOL,
	  MOPS_COPY_CASE_ARRAY,
	  "mops_rejects_reserved_and_constrained_unpredictable_forms",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ MOPS_COPY_SOURCE, MOPS_COPY_SUITE, MOPS_COPY_SUITE_SYMBOL,
	  MOPS_COPY_CASE_ARRAY, "mops_production_gadget_forwards_and_writes_back",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ MOPS_COPY_SOURCE, MOPS_COPY_SUITE, MOPS_COPY_SUITE_SYMBOL,
	  MOPS_COPY_CASE_ARRAY,
	  "mops_production_gadget_selects_backward_overlap_direction",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ MOPS_COPY_SOURCE, MOPS_COPY_SUITE, MOPS_COPY_SUITE_SYMBOL,
	  MOPS_COPY_CASE_ARRAY, "mops_production_gadget_executes_all_options",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ MOPS_COPY_SOURCE, MOPS_COPY_SUITE, MOPS_COPY_SUITE_SYMBOL,
	  MOPS_COPY_CASE_ARRAY, "mops_production_rejects_constrained_register_forms",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ MOPS_COPY_SOURCE, MOPS_COPY_SUITE, MOPS_COPY_SUITE_SYMBOL,
	  MOPS_COPY_CASE_ARRAY, "mops_epilogue_preserves_partial_guest_progress_on_fault",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS },
	{ MOPS_COPY_SOURCE, MOPS_COPY_SUITE, MOPS_COPY_SUITE_SYMBOL,
	  MOPS_COPY_CASE_ARRAY, "mops_epilogue_preserves_destination_fault_progress",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS },
	{ MOPS_COPY_SOURCE, MOPS_COPY_SUITE, MOPS_COPY_SUITE_SYMBOL,
	  MOPS_COPY_CASE_ARRAY, "mops_epilogue_preserves_backward_fault_progress",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS },
	{ FLAG_MANIPULATION_SOURCE, FLAG_MANIPULATION_SUITE,
	  FLAG_MANIPULATION_SUITE_SYMBOL, FLAG_MANIPULATION_CASE_ARRAY,
	  "orlix_tcti_flag_source_and_slice_bindings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ FLAG_MANIPULATION_SOURCE, FLAG_MANIPULATION_SUITE,
	  FLAG_MANIPULATION_SUITE_SYMBOL, FLAG_MANIPULATION_CASE_ARRAY,
	  "orlix_tcti_flag_legal_encoding_matrix",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ FLAG_MANIPULATION_SOURCE, FLAG_MANIPULATION_SUITE,
	  FLAG_MANIPULATION_SUITE_SYMBOL, FLAG_MANIPULATION_CASE_ARRAY,
	  "orlix_tcti_flag_reserved_fixed_bit_matrix",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ FLAG_MANIPULATION_SOURCE, FLAG_MANIPULATION_SUITE,
	  FLAG_MANIPULATION_SUITE_SYMBOL, FLAG_MANIPULATION_CASE_ARRAY,
	  "orlix_tcti_flag_pstate_semantics_matrix",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ FLAG_MANIPULATION_SOURCE, FLAG_MANIPULATION_SUITE,
	  FLAG_MANIPULATION_SUITE_SYMBOL, FLAG_MANIPULATION_CASE_ARRAY,
	  "orlix_tcti_flag_non_el0_rejected_without_state_change",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },

};

/* Exact Arm operation_id values. Missing rows are audit blockers. */
static const struct operation_requirements operation_requirements[] = {
	{ "RMIF", INTEGER_CONDITIONAL_FLAGS_OBLIGATIONS },
	{ "SETF", INTEGER_CONDITIONAL_FLAGS_OBLIGATIONS },
	{ "ADD_addsub_imm", SCALAR_FLAGS_OBLIGATIONS },
	{ "ADDS_addsub_imm", SCALAR_FLAGS_OBLIGATIONS },
	{ "SUB_addsub_imm", SCALAR_FLAGS_OBLIGATIONS },
	{ "SUBS_addsub_imm", SCALAR_FLAGS_OBLIGATIONS },
#define ORLIX_TCTI_PROOF_FAMILY_OPERATION(proof_id_value, operation_value, linux_value, obligations_value) \
	{ operation_value, PRODUCTION_CAPTURE_FAMILY_OBLIGATIONS },
#include "target_production_capture_family.def"
#undef ORLIX_TCTI_PROOF_FAMILY_OPERATION
	{ "BR", PRODUCTION_CAPTURE_FAMILY_OBLIGATIONS },
	{ "BLR", PRODUCTION_CAPTURE_FAMILY_OBLIGATIONS },
	{ "RET", PRODUCTION_CAPTURE_FAMILY_OBLIGATIONS },
	{ "B_uncond", PRODUCTION_CAPTURE_FAMILY_OBLIGATIONS },
	{ "BL", PRODUCTION_CAPTURE_FAMILY_OBLIGATIONS },
	{ "SVC", PRODUCTION_CAPTURE_FAMILY_OBLIGATIONS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LINUX_INTERFACE },
	{ "BRK", PRODUCTION_CAPTURE_FAMILY_OBLIGATIONS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LINUX_INTERFACE },
	{ "HLT", PRODUCTION_CAPTURE_FAMILY_OBLIGATIONS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LINUX_INTERFACE },
	{ "B_cond", PRODUCTION_CAPTURE_FAMILY_OBLIGATIONS },
	{ "CBZ", PRODUCTION_CAPTURE_FAMILY_OBLIGATIONS },
	{ "CBNZ", PRODUCTION_CAPTURE_FAMILY_OBLIGATIONS },
	{ "TBZ", PRODUCTION_CAPTURE_FAMILY_OBLIGATIONS },
	{ "TBNZ", PRODUCTION_CAPTURE_FAMILY_OBLIGATIONS },
	{ "AND_log_shift", LOGICAL_BASE_OBLIGATIONS },
	{ "BIC_log_shift", LOGICAL_BASE_OBLIGATIONS },
	{ "ORR_log_shift", LOGICAL_BASE_OBLIGATIONS },
	{ "ORN_log_shift", LOGICAL_BASE_OBLIGATIONS },
	{ "EOR_log_shift", LOGICAL_BASE_OBLIGATIONS },
	{ "EON", LOGICAL_BASE_OBLIGATIONS },
	{ "ANDS_log_shift", LOGICAL_FLAGS_OBLIGATIONS },
	{ "BICS", LOGICAL_FLAGS_OBLIGATIONS },
	{ "SMAX_imm", BASELINE_OBLIGATIONS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "UMAX_imm", BASELINE_OBLIGATIONS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "SMIN_imm", BASELINE_OBLIGATIONS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "UMIN_imm", BASELINE_OBLIGATIONS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "SMAX_reg", BASELINE_OBLIGATIONS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "UMAX_reg", BASELINE_OBLIGATIONS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "SMIN_reg", BASELINE_OBLIGATIONS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "UMIN_reg", BASELINE_OBLIGATIONS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "CTZ", BASELINE_OBLIGATIONS |
		 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "CNT", BASELINE_OBLIGATIONS |
		 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "ABS", BASELINE_OBLIGATIONS |
		ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "UDIV", INTEGER_CONDITIONAL_BASE_OBLIGATIONS },
	{ "SDIV", INTEGER_CONDITIONAL_BASE_OBLIGATIONS },
	{ "CCMN_reg", INTEGER_CONDITIONAL_FLAGS_OBLIGATIONS },
	{ "CCMP_reg", INTEGER_CONDITIONAL_FLAGS_OBLIGATIONS },
	{ "CCMN_imm", INTEGER_CONDITIONAL_FLAGS_OBLIGATIONS },
	{ "CCMP_imm", INTEGER_CONDITIONAL_FLAGS_OBLIGATIONS },
	{ "CSEL", INTEGER_CONDITIONAL_BASE_OBLIGATIONS },
	{ "CSINC", INTEGER_CONDITIONAL_BASE_OBLIGATIONS },
	{ "CSINV", INTEGER_CONDITIONAL_BASE_OBLIGATIONS },
	{ "CSNEG", INTEGER_CONDITIONAL_BASE_OBLIGATIONS },
	{ "MADD", INTEGER_CONDITIONAL_BASE_OBLIGATIONS },
	{ "MSUB", INTEGER_CONDITIONAL_BASE_OBLIGATIONS },
	{ "SMADDL", INTEGER_CONDITIONAL_BASE_OBLIGATIONS },
	{ "SMSUBL", INTEGER_CONDITIONAL_BASE_OBLIGATIONS },
	{ "SMULH", INTEGER_CONDITIONAL_BASE_OBLIGATIONS },
	{ "UMADDL", INTEGER_CONDITIONAL_BASE_OBLIGATIONS },
	{ "UMSUBL", INTEGER_CONDITIONAL_BASE_OBLIGATIONS },
	{ "UMULH", INTEGER_CONDITIONAL_BASE_OBLIGATIONS },
#define SCALAR_REQUIREMENT(operation, obligations) { operation, obligations }
	SCALAR_REQUIREMENT("AND_log_imm", SCALAR_BASE_OBLIGATIONS),
	SCALAR_REQUIREMENT("ORR_log_imm", SCALAR_BASE_OBLIGATIONS),
	SCALAR_REQUIREMENT("EOR_log_imm", SCALAR_BASE_OBLIGATIONS),
	SCALAR_REQUIREMENT("ANDS_log_imm", SCALAR_FLAGS_OBLIGATIONS),
	SCALAR_REQUIREMENT("MOVN", SCALAR_BASE_OBLIGATIONS),
	SCALAR_REQUIREMENT("MOVZ", SCALAR_BASE_OBLIGATIONS),
	SCALAR_REQUIREMENT("MOVK", SCALAR_BASE_OBLIGATIONS),
	SCALAR_REQUIREMENT("EXTR", SCALAR_BASE_OBLIGATIONS),
	SCALAR_REQUIREMENT("SBFM", SCALAR_BASE_OBLIGATIONS),
	SCALAR_REQUIREMENT("BFM", SCALAR_BASE_OBLIGATIONS),
	SCALAR_REQUIREMENT("UBFM", SCALAR_BASE_OBLIGATIONS),
	SCALAR_REQUIREMENT("RBIT_int", SCALAR_BASE_OBLIGATIONS),
	SCALAR_REQUIREMENT("REV16_int", SCALAR_BASE_OBLIGATIONS),
	SCALAR_REQUIREMENT("REV", SCALAR_BASE_OBLIGATIONS),
	SCALAR_REQUIREMENT("REV32_int", SCALAR_BASE_OBLIGATIONS),
	SCALAR_REQUIREMENT("CLZ_int", SCALAR_BASE_OBLIGATIONS),
	SCALAR_REQUIREMENT("CLS_int", SCALAR_BASE_OBLIGATIONS),
	SCALAR_REQUIREMENT("LSLV", SCALAR_BASE_OBLIGATIONS),
	SCALAR_REQUIREMENT("LSRV", SCALAR_BASE_OBLIGATIONS),
	SCALAR_REQUIREMENT("ASRV", SCALAR_BASE_OBLIGATIONS),
	SCALAR_REQUIREMENT("RORV", SCALAR_BASE_OBLIGATIONS),
	SCALAR_REQUIREMENT("ADD_addsub_shift", SCALAR_FLAGS_OBLIGATIONS),
	SCALAR_REQUIREMENT("ADDS_addsub_shift", SCALAR_FLAGS_OBLIGATIONS),
	SCALAR_REQUIREMENT("SUB_addsub_shift", SCALAR_FLAGS_OBLIGATIONS),
	SCALAR_REQUIREMENT("SUBS_addsub_shift", SCALAR_FLAGS_OBLIGATIONS),
	SCALAR_REQUIREMENT("ADD_addsub_ext", SCALAR_FLAGS_OBLIGATIONS),
	SCALAR_REQUIREMENT("ADDS_addsub_ext", SCALAR_FLAGS_OBLIGATIONS),
	SCALAR_REQUIREMENT("SUB_addsub_ext", SCALAR_FLAGS_OBLIGATIONS),
	SCALAR_REQUIREMENT("SUBS_addsub_ext", SCALAR_FLAGS_OBLIGATIONS),
	SCALAR_REQUIREMENT("ADC", SCALAR_FLAGS_OBLIGATIONS),
	SCALAR_REQUIREMENT("ADCS", SCALAR_FLAGS_OBLIGATIONS),
	SCALAR_REQUIREMENT("SBC", SCALAR_FLAGS_OBLIGATIONS),
	SCALAR_REQUIREMENT("SBCS", SCALAR_FLAGS_OBLIGATIONS),
	SCALAR_REQUIREMENT("ADDPT", SCALAR_FLAGS_OBLIGATIONS),
	SCALAR_REQUIREMENT("SUBPT", SCALAR_FLAGS_OBLIGATIONS),
	SCALAR_REQUIREMENT("MADDPT", SCALAR_BASE_OBLIGATIONS),
	SCALAR_REQUIREMENT("MSUBPT", SCALAR_BASE_OBLIGATIONS),
	SCALAR_REQUIREMENT("AESE_advsimd", ADVSIMD_CRYPTO_REQUIRED_OBLIGATIONS),
	SCALAR_REQUIREMENT("AESD_advsimd", ADVSIMD_CRYPTO_REQUIRED_OBLIGATIONS),
	SCALAR_REQUIREMENT("AESMC_advsimd", ADVSIMD_CRYPTO_REQUIRED_OBLIGATIONS),
	SCALAR_REQUIREMENT("AESIMC_advsimd", ADVSIMD_CRYPTO_REQUIRED_OBLIGATIONS),
	SCALAR_REQUIREMENT("SHA1C_advsimd", ADVSIMD_CRYPTO_REQUIRED_OBLIGATIONS),
	SCALAR_REQUIREMENT("SHA1P_advsimd", ADVSIMD_CRYPTO_REQUIRED_OBLIGATIONS),
	SCALAR_REQUIREMENT("SHA1M_advsimd", ADVSIMD_CRYPTO_REQUIRED_OBLIGATIONS),
	SCALAR_REQUIREMENT("SHA1SU0_advsimd", ADVSIMD_CRYPTO_REQUIRED_OBLIGATIONS),
	SCALAR_REQUIREMENT("SHA256H_advsimd", ADVSIMD_CRYPTO_REQUIRED_OBLIGATIONS),
	SCALAR_REQUIREMENT("SHA256H2_advsimd", ADVSIMD_CRYPTO_REQUIRED_OBLIGATIONS),
	SCALAR_REQUIREMENT("SHA256SU1_advsimd", ADVSIMD_CRYPTO_REQUIRED_OBLIGATIONS),
	SCALAR_REQUIREMENT("SHA1H_advsimd", ADVSIMD_CRYPTO_REQUIRED_OBLIGATIONS),
	SCALAR_REQUIREMENT("SHA1SU1_advsimd", ADVSIMD_CRYPTO_REQUIRED_OBLIGATIONS),
	SCALAR_REQUIREMENT("SHA256SU0_advsimd", ADVSIMD_CRYPTO_REQUIRED_OBLIGATIONS),
	SCALAR_REQUIREMENT("PMULL_advsimd", ADVSIMD_CRYPTO_REQUIRED_OBLIGATIONS),
	SCALAR_REQUIREMENT("SM3TT1A_advsimd", ADVSIMD_CRYPTO_REQUIRED_OBLIGATIONS),
	SCALAR_REQUIREMENT("SM3TT1B_advsimd", ADVSIMD_CRYPTO_REQUIRED_OBLIGATIONS),
	SCALAR_REQUIREMENT("SM3TT2A_advsimd", ADVSIMD_CRYPTO_REQUIRED_OBLIGATIONS),
	SCALAR_REQUIREMENT("SM3TT2B_advsimd", ADVSIMD_CRYPTO_REQUIRED_OBLIGATIONS),
	SCALAR_REQUIREMENT("SHA512H_advsimd", ADVSIMD_CRYPTO_REQUIRED_OBLIGATIONS),
	SCALAR_REQUIREMENT("SHA512H2_advsimd", ADVSIMD_CRYPTO_REQUIRED_OBLIGATIONS),
	SCALAR_REQUIREMENT("SHA512SU1_advsimd", ADVSIMD_CRYPTO_REQUIRED_OBLIGATIONS),
	SCALAR_REQUIREMENT("RAX1_advsimd", ADVSIMD_CRYPTO_REQUIRED_OBLIGATIONS),
	SCALAR_REQUIREMENT("SM3PARTW1_advsimd", ADVSIMD_CRYPTO_REQUIRED_OBLIGATIONS),
	SCALAR_REQUIREMENT("SM3PARTW2_advsimd", ADVSIMD_CRYPTO_REQUIRED_OBLIGATIONS),
	SCALAR_REQUIREMENT("SM4EKEY_advsimd", ADVSIMD_CRYPTO_REQUIRED_OBLIGATIONS),
	SCALAR_REQUIREMENT("EOR3_advsimd", ADVSIMD_CRYPTO_REQUIRED_OBLIGATIONS),
	SCALAR_REQUIREMENT("BCAX_advsimd", ADVSIMD_CRYPTO_REQUIRED_OBLIGATIONS),
	SCALAR_REQUIREMENT("SM3SS1_advsimd", ADVSIMD_CRYPTO_REQUIRED_OBLIGATIONS),
	SCALAR_REQUIREMENT("XAR_advsimd", ADVSIMD_CRYPTO_REQUIRED_OBLIGATIONS),
	SCALAR_REQUIREMENT("SHA512SU0_advsimd", ADVSIMD_CRYPTO_REQUIRED_OBLIGATIONS),
	SCALAR_REQUIREMENT("SM4E_advsimd", ADVSIMD_CRYPTO_REQUIRED_OBLIGATIONS),
	SCALAR_REQUIREMENT("DUP_advsimd_elt", ADVSIMD_PERMUTE_MOVE_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("TBL_advsimd", ADVSIMD_PERMUTE_MOVE_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("TBX_advsimd", ADVSIMD_PERMUTE_MOVE_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("UZP1_advsimd", ADVSIMD_PERMUTE_MOVE_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("TRN1_advsimd", ADVSIMD_PERMUTE_MOVE_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("ZIP1_advsimd", ADVSIMD_PERMUTE_MOVE_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("UZP2_advsimd", ADVSIMD_PERMUTE_MOVE_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("TRN2_advsimd", ADVSIMD_PERMUTE_MOVE_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("ZIP2_advsimd", ADVSIMD_PERMUTE_MOVE_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("EXT_advsimd", ADVSIMD_PERMUTE_MOVE_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("DUP_advsimd_gen", ADVSIMD_PERMUTE_MOVE_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SMOV_advsimd", ADVSIMD_PERMUTE_MOVE_GPR_OBLIGATIONS),
	SCALAR_REQUIREMENT("UMOV_advsimd", ADVSIMD_PERMUTE_MOVE_GPR_OBLIGATIONS),
	SCALAR_REQUIREMENT("INS_advsimd_gen", ADVSIMD_PERMUTE_MOVE_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("INS_advsimd_elt", ADVSIMD_PERMUTE_MOVE_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("REV64_advsimd", ADVSIMD_PERMUTE_MOVE_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("REV16_advsimd", ADVSIMD_PERMUTE_MOVE_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("REV32_advsimd", ADVSIMD_PERMUTE_MOVE_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("MOVI_advsimd", ADVSIMD_PERMUTE_MOVE_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SUQADD_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SQABS_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("CMGT_advsimd_zero", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("CMEQ_advsimd_zero", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("CMLT_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("ABS_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SQXTN_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("USQADD_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SQNEG_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("CMGE_advsimd_zero", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("CMLE_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("NEG_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SQXTUN_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("UQXTN_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("ADDP_advsimd_pair", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SQDMLAL_advsimd_vec", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SQDMLSL_advsimd_vec", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SQDMULL_advsimd_vec", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SQADD_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SQSUB_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("CMGT_advsimd_reg", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("CMGE_advsimd_reg", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SSHL_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SQSHL_advsimd_reg", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SRSHL_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SQRSHL_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("ADD_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("CMTST_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SQDMULH_advsimd_vec", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("UQADD_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("UQSUB_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("CMHI_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("CMHS_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("USHL_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("UQSHL_advsimd_reg", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("URSHL_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("UQRSHL_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SUB_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("CMEQ_advsimd_reg", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SQRDMULH_advsimd_vec", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SSHR_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SSRA_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SRSHR_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SRSRA_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SHL_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SQSHL_advsimd_imm", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SQSHRN_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SQRSHRN_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("USHR_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("USRA_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("URSHR_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("URSRA_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SRI_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SLI_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SQSHLU_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("UQSHL_advsimd_imm", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SQSHRUN_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SQRSHRUN_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("UQSHRN_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("UQRSHRN_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SQDMLAL_advsimd_elt", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SQDMLSL_advsimd_elt", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SQDMULL_advsimd_elt", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SQDMULH_advsimd_elt", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SQRDMULH_advsimd_elt", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SADDLP_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("CLS_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("CNT_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SADALP_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("XTN_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("URECPE_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("UADDLP_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("CLZ_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("UADALP_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SHLL_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("NOT_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("RBIT_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("URSQRTE_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SADDLV_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SMAXV_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SMINV_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("ADDV_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("UADDLV_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("UMAXV_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("UMINV_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SADDL_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SADDW_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SSUBL_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SSUBW_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("ADDHN_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SABAL_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SUBHN_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SABDL_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SMLAL_advsimd_vec", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SMLSL_advsimd_vec", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SMULL_advsimd_vec", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("UADDL_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("UADDW_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("USUBL_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("USUBW_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("RADDHN_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("UABAL_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("RSUBHN_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("UABDL_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("UMLAL_advsimd_vec", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("UMLSL_advsimd_vec", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("UMULL_advsimd_vec", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SHADD_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SRHADD_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SHSUB_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SMAX_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SMIN_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SABD_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SABA_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("MLA_advsimd_vec", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("MUL_advsimd_vec", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SMAXP_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SMINP_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("ADDP_advsimd_vec", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("AND_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("BIC_advsimd_reg", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("ORR_advsimd_reg", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("ORN_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("UHADD_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("URHADD_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("UHSUB_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("UMAX_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("UMIN_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("UABD_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("UABA_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("MLS_advsimd_vec", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("PMUL_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("UMAXP_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("UMINP_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("EOR_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("BSL_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("BIT_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("BIF_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("ORR_advsimd_imm", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("MVNI_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("BIC_advsimd_imm", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SHRN_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("RSHRN_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SSHLL_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("USHLL_advsimd", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SMLAL_advsimd_elt", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SMLSL_advsimd_elt", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("MUL_advsimd_elt", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SMULL_advsimd_elt", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("MLA_advsimd_elt", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("UMLAL_advsimd_elt", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("MLS_advsimd_elt", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("UMLSL_advsimd_elt", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("UMULL_advsimd_elt", ADVSIMD_INTEGER_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FABD_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FABS_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FACGE_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FACGT_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FADDP_advsimd_pair", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FADDP_advsimd_vec", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FADD_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCMEQ_advsimd_reg", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCMEQ_advsimd_zero", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCMGE_advsimd_reg", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCMGE_advsimd_zero", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCMGT_advsimd_reg", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCMGT_advsimd_zero", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCMLE_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCMLT_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCVTAS_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCVTAU_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCVTL_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCVTMS_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCVTMU_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCVTNS_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCVTNU_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCVTN_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCVTPS_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCVTPU_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCVTXN_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCVTZS_advsimd_fix", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCVTZS_advsimd_int", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCVTZU_advsimd_fix", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCVTZU_advsimd_int", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FDIV_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FMAXNMP_advsimd_pair", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FMAXNMP_advsimd_vec", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FMAXNMV_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FMAXNM_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FMAXP_advsimd_pair", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FMAXP_advsimd_vec", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FMAXV_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FMAX_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FMINNMP_advsimd_pair", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FMINNMP_advsimd_vec", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FMINNMV_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FMINNM_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FMINP_advsimd_pair", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FMINP_advsimd_vec", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FMINV_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FMIN_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FMLA_advsimd_elt", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FMLA_advsimd_vec", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FMLS_advsimd_elt", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FMLS_advsimd_vec", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FMOV_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FMULX_advsimd_elt", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FMULX_advsimd_vec", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FMUL_advsimd_elt", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FMUL_advsimd_vec", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FNEG_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FRECPE_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FRECPS_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FRECPX_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FRINTA_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FRINTI_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FRINTM_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FRINTN_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FRINTP_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FRINTX_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FRINTZ_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FRSQRTE_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FRSQRTS_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FSQRT_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("FSUB_advsimd", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SCVTF_advsimd_fix", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("SCVTF_advsimd_int", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("UCVTF_advsimd_fix", ADVSIMD_FP_SIMD_OBLIGATIONS),
	SCALAR_REQUIREMENT("UCVTF_advsimd_int", ADVSIMD_FP_SIMD_OBLIGATIONS),

	SCALAR_REQUIREMENT("SCVTF_float_fix", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("UCVTF_float_fix", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCVTZS_float_fix", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCVTZU_float_fix", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCVTNS_float", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCVTNU_float", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("SCVTF_float_int", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("UCVTF_float_int", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCVTAS_float", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCVTAU_float", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCVTPS_float", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCVTPU_float", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCVTMS_float", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCVTMU_float", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCVTZS_float_int", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCVTZU_float_int", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FMOV_float_gen", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FMOV_float", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FMOV_float_imm", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FABS_float", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FNEG_float", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FSQRT_float", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCVT_float", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FRINTN_float", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FRINTP_float", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FRINTM_float", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FRINTZ_float", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FRINTA_float", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FRINTX_float", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FRINTI_float", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCMP_float", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCMPE_float", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCCMP_float", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCCMPE_float", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FMUL_float", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FDIV_float", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FADD_float", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FSUB_float", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FMAX_float", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FMIN_float", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FMAXNM_float", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FMINNM_float", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FNMUL_float", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FCSEL_float", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FMADD_float", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FMSUB_float", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FNMADD_float", SCALAR_FP_FAMILY_OBLIGATIONS),
	SCALAR_REQUIREMENT("FNMSUB_float", SCALAR_FP_FAMILY_OBLIGATIONS),
#undef SCALAR_REQUIREMENT
	{ "LDRB_imm", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDRB_reg", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDRH_imm", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDRH_reg", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDRSB_imm", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDRSB_reg", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDRSH_imm", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDRSH_reg", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDRSW_imm", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDRSW_reg", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDR_imm_fpsimd", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDR_imm_gen", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDR_reg_fpsimd", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDR_reg_gen", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDTR", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDTRB", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDTRH", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDTRSB", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDTRSH", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDTRSW", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDURB", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDURH", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDURSB", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDURSH", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDURSW", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDUR_fpsimd", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDUR_gen", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "STRB_imm", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "STRB_reg", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "STRH_imm", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "STRH_reg", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "STR_imm_fpsimd", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "STR_imm_gen", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "STR_reg_fpsimd", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "STR_reg_gen", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "STTR", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "STTRB", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "STTRH", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "STURB", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "STURH", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "STUR_fpsimd", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "STUR_gen", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "GCSSTR", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "GCSSTTR", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDNP_fpsimd", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDNP_gen", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDPSW", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDP_fpsimd", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDP_gen", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDRSW_lit", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDR_lit_fpsimd", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDR_lit_gen", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDTNP_fpsimd", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDTNP_gen", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDTP_fpsimd", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LDTP_gen", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "PRFM_imm", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "PRFM_lit", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "PRFM_reg", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "PRFUM", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "RPRFM_reg", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "STGP", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "STNP_fpsimd", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "STNP_gen", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "STP_fpsimd", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "STP_gen", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "STTNP_fpsimd", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "STTNP_gen", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "STTP_fpsimd", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "STTP_gen", ORDINARY_LOAD_STORE_OBLIGATIONS },
	{ "LD1R_advsimd", ADVSIMD_LOAD_STORE_OBLIGATIONS },
	{ "LD1_advsimd_mult", ADVSIMD_LOAD_STORE_OBLIGATIONS },
	{ "LD1_advsimd_sngl", ADVSIMD_LOAD_STORE_OBLIGATIONS },
	{ "LD2R_advsimd", ADVSIMD_LOAD_STORE_OBLIGATIONS },
	{ "LD2_advsimd_mult", ADVSIMD_LOAD_STORE_OBLIGATIONS },
	{ "LD2_advsimd_sngl", ADVSIMD_LOAD_STORE_OBLIGATIONS },
	{ "LD3R_advsimd", ADVSIMD_LOAD_STORE_OBLIGATIONS },
	{ "LD3_advsimd_mult", ADVSIMD_LOAD_STORE_OBLIGATIONS },
	{ "LD3_advsimd_sngl", ADVSIMD_LOAD_STORE_OBLIGATIONS },
	{ "LD4R_advsimd", ADVSIMD_LOAD_STORE_OBLIGATIONS },
	{ "LD4_advsimd_mult", ADVSIMD_LOAD_STORE_OBLIGATIONS },
	{ "LD4_advsimd_sngl", ADVSIMD_LOAD_STORE_OBLIGATIONS },
	{ "LDAP1_advsimd_sngl", ADVSIMD_LOAD_STORE_OBLIGATIONS },
	{ "ST1_advsimd_mult", ADVSIMD_LOAD_STORE_OBLIGATIONS },
	{ "ST1_advsimd_sngl", ADVSIMD_LOAD_STORE_OBLIGATIONS },
	{ "ST2_advsimd_mult", ADVSIMD_LOAD_STORE_OBLIGATIONS },
	{ "ST2_advsimd_sngl", ADVSIMD_LOAD_STORE_OBLIGATIONS },
	{ "ST3_advsimd_mult", ADVSIMD_LOAD_STORE_OBLIGATIONS },
	{ "ST3_advsimd_sngl", ADVSIMD_LOAD_STORE_OBLIGATIONS },
	{ "ST4_advsimd_mult", ADVSIMD_LOAD_STORE_OBLIGATIONS },
	{ "ST4_advsimd_sngl", ADVSIMD_LOAD_STORE_OBLIGATIONS },
	{ "STL1_advsimd_sngl", ADVSIMD_LOAD_STORE_OBLIGATIONS },
#define LSE_REQUIREMENT(operation) \
	{ operation, LSE_REQUIRED_OBLIGATIONS }
	LSE_REQUIREMENT("CASB"),
	LSE_REQUIREMENT("CASH"),
	LSE_REQUIREMENT("CAS"),
	LSE_REQUIREMENT("CASP"),
	LSE_REQUIREMENT("LDADDB"),
	LSE_REQUIREMENT("LDADDH"),
	LSE_REQUIREMENT("LDADD"),
	LSE_REQUIREMENT("LDCLRB"),
	LSE_REQUIREMENT("LDCLRH"),
	LSE_REQUIREMENT("LDCLR"),
	LSE_REQUIREMENT("LDEORB"),
	LSE_REQUIREMENT("LDEORH"),
	LSE_REQUIREMENT("LDEOR"),
	LSE_REQUIREMENT("LDSETB"),
	LSE_REQUIREMENT("LDSETH"),
	LSE_REQUIREMENT("LDSET"),
	LSE_REQUIREMENT("LDSMAXB"),
	LSE_REQUIREMENT("LDSMAXH"),
	LSE_REQUIREMENT("LDSMAX"),
	LSE_REQUIREMENT("LDSMINB"),
	LSE_REQUIREMENT("LDSMINH"),
	LSE_REQUIREMENT("LDSMIN"),
	LSE_REQUIREMENT("LDUMAXB"),
	LSE_REQUIREMENT("LDUMAXH"),
	LSE_REQUIREMENT("LDUMAX"),
	LSE_REQUIREMENT("LDUMINB"),
	LSE_REQUIREMENT("LDUMINH"),
	LSE_REQUIREMENT("LDUMIN"),
	LSE_REQUIREMENT("SWPB"),
	LSE_REQUIREMENT("SWPH"),
	LSE_REQUIREMENT("SWP"),
	LSE_REQUIREMENT("LDCLRP"),
	LSE_REQUIREMENT("LDSETP"),
	LSE_REQUIREMENT("SWPP"),
#undef LSE_REQUIREMENT
#define EXCLUSIVE_REQUIREMENT(operation) \
	{ operation, EXCLUSIVE_REQUIRED_OBLIGATIONS }
	EXCLUSIVE_REQUIREMENT("STXP"),
	EXCLUSIVE_REQUIREMENT("STLXP"),
	EXCLUSIVE_REQUIREMENT("LDXP"),
	EXCLUSIVE_REQUIREMENT("LDAXP"),
	EXCLUSIVE_REQUIREMENT("STXRB"),
	EXCLUSIVE_REQUIREMENT("STLXRB"),
	EXCLUSIVE_REQUIREMENT("LDXRB"),
	EXCLUSIVE_REQUIREMENT("LDAXRB"),
	EXCLUSIVE_REQUIREMENT("STXRH"),
	EXCLUSIVE_REQUIREMENT("STLXRH"),
	EXCLUSIVE_REQUIREMENT("LDXRH"),
	EXCLUSIVE_REQUIREMENT("LDAXRH"),
	EXCLUSIVE_REQUIREMENT("STXR"),
	EXCLUSIVE_REQUIREMENT("STLXR"),
	EXCLUSIVE_REQUIREMENT("LDXR"),
	EXCLUSIVE_REQUIREMENT("LDAXR"),
#undef EXCLUSIVE_REQUIREMENT
#define MOPS_COPY_REQUIREMENT(operation) { operation, MOPS_COPY_OBLIGATIONS }
	MOPS_COPY_REQUIREMENT("CPYFP"),
	MOPS_COPY_REQUIREMENT("CPYFPWT"),
	MOPS_COPY_REQUIREMENT("CPYFPRT"),
	MOPS_COPY_REQUIREMENT("CPYFPT"),
	MOPS_COPY_REQUIREMENT("CPYFPWN"),
	MOPS_COPY_REQUIREMENT("CPYFPWTWN"),
	MOPS_COPY_REQUIREMENT("CPYFPRTWN"),
	MOPS_COPY_REQUIREMENT("CPYFPTWN"),
	MOPS_COPY_REQUIREMENT("CPYFPRN"),
	MOPS_COPY_REQUIREMENT("CPYFPWTRN"),
	MOPS_COPY_REQUIREMENT("CPYFPRTRN"),
	MOPS_COPY_REQUIREMENT("CPYFPTRN"),
	MOPS_COPY_REQUIREMENT("CPYFPN"),
	MOPS_COPY_REQUIREMENT("CPYFPWTN"),
	MOPS_COPY_REQUIREMENT("CPYFPRTN"),
	MOPS_COPY_REQUIREMENT("CPYFPTN"),
	MOPS_COPY_REQUIREMENT("CPYP"),
	MOPS_COPY_REQUIREMENT("CPYPWT"),
	MOPS_COPY_REQUIREMENT("CPYPRT"),
	MOPS_COPY_REQUIREMENT("CPYPT"),
	MOPS_COPY_REQUIREMENT("CPYPWN"),
	MOPS_COPY_REQUIREMENT("CPYPWTWN"),
	MOPS_COPY_REQUIREMENT("CPYPRTWN"),
	MOPS_COPY_REQUIREMENT("CPYPTWN"),
	MOPS_COPY_REQUIREMENT("CPYPRN"),
	MOPS_COPY_REQUIREMENT("CPYPWTRN"),
	MOPS_COPY_REQUIREMENT("CPYPRTRN"),
	MOPS_COPY_REQUIREMENT("CPYPTRN"),
	MOPS_COPY_REQUIREMENT("CPYPN"),
	MOPS_COPY_REQUIREMENT("CPYPWTN"),
	MOPS_COPY_REQUIREMENT("CPYPRTN"),
	MOPS_COPY_REQUIREMENT("CPYPTN"),
#undef MOPS_COPY_REQUIREMENT
};

static const struct orlix_tcti_target_proof_case logical_base_cases[] = {
	{ "orlix_tcti_logical_shifted_register_source_bindings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_logical_shifted_register_fixed_bit_neighbours",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ "orlix_tcti_logical_shifted_register_complete_field_matrix",
	  LOGICAL_BASE_OBLIGATIONS },
	{ "orlix_tcti_logical_shifted_register_register_and_overlap_matrix",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_logical_shifted_register_reserved_structured_exits",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
};

static const struct orlix_tcti_target_proof_case logical_base_alias_cases[] = {
	{ "orlix_tcti_logical_shifted_register_source_bindings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_logical_shifted_register_fixed_bit_neighbours",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ "orlix_tcti_logical_shifted_register_complete_field_matrix",
	  LOGICAL_BASE_OBLIGATIONS },
	{ "orlix_tcti_logical_shifted_register_register_and_overlap_matrix",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_logical_shifted_register_aliases",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_logical_shifted_register_reserved_structured_exits",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
};

static const struct orlix_tcti_target_proof_case logical_flags_cases[] = {
	{ "orlix_tcti_logical_shifted_register_source_bindings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_logical_shifted_register_fixed_bit_neighbours",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ "orlix_tcti_logical_shifted_register_complete_field_matrix",
	  LOGICAL_FLAGS_OBLIGATIONS },
	{ "orlix_tcti_logical_shifted_register_register_and_overlap_matrix",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "orlix_tcti_logical_shifted_register_reserved_structured_exits",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
};

static const struct orlix_tcti_target_proof_case logical_flags_alias_cases[] = {
	{ "orlix_tcti_logical_shifted_register_source_bindings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_logical_shifted_register_fixed_bit_neighbours",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ "orlix_tcti_logical_shifted_register_complete_field_matrix",
	  LOGICAL_FLAGS_OBLIGATIONS },
	{ "orlix_tcti_logical_shifted_register_register_and_overlap_matrix",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "orlix_tcti_logical_shifted_register_aliases",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "orlix_tcti_logical_shifted_register_reserved_structured_exits",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
};

#define ADVSIMD_FP_ARITHMETIC_PROOF(name, ordinal, leaf, mnemonic, pattern) \
	static const struct orlix_tcti_target_proof_case name##_cases[] \
		__attribute__((unused)) = { \
		{ #name, ADVSIMD_FP_ARITHMETIC_OBLIGATIONS }, \
	}; \
	static const struct orlix_tcti_target_proof_binding name##_bindings[] \
		__attribute__((unused)) = { \
		{ leaf, mnemonic, 0xbfa0fc00U, pattern, \
		  "54434e440107000000310700000017070000000c01000000010101000000010101000000010102000000100000000c464541545f41647653494d44", \
		  ORLIX_TCTI_PROOF_U64_C(1), ordinal }, \
	}
ADVSIMD_FP_ARITHMETIC_PROOF(orlix_tcti_advsimd_fp_fmla_source_leaf_execute_exact_bits, 3919U, "FMLA_asimdsame_only", "FMLA", 0x0e20cc00U);
ADVSIMD_FP_ARITHMETIC_PROOF(orlix_tcti_advsimd_fp_fadd_source_leaf_execute_exact_bits, 3920U, "FADD_asimdsame_only", "FADD", 0x0e20d400U);
ADVSIMD_FP_ARITHMETIC_PROOF(orlix_tcti_advsimd_fp_fcmeq_source_leaf_execute_exact_bits, 3922U, "FCMEQ_asimdsame_only", "FCMEQ", 0x0e20e400U);
ADVSIMD_FP_ARITHMETIC_PROOF(orlix_tcti_advsimd_fp_fmax_source_leaf_execute_exact_bits, 3923U, "FMAX_asimdsame_only", "FMAX", 0x0e20f400U);
ADVSIMD_FP_ARITHMETIC_PROOF(orlix_tcti_advsimd_fp_fsub_source_leaf_execute_exact_bits, 3930U, "FSUB_asimdsame_only", "FSUB", 0x0ea0d400U);
ADVSIMD_FP_ARITHMETIC_PROOF(orlix_tcti_advsimd_fp_fmin_source_leaf_execute_exact_bits, 3932U, "FMIN_asimdsame_only", "FMIN", 0x0ea0f400U);
ADVSIMD_FP_ARITHMETIC_PROOF(orlix_tcti_advsimd_fp_fmul_source_leaf_execute_exact_bits, 3961U, "FMUL_asimdsame_only", "FMUL", 0x2e20dc00U);
ADVSIMD_FP_ARITHMETIC_PROOF(orlix_tcti_advsimd_fp_fdiv_source_leaf_execute_exact_bits, 3965U, "FDIV_asimdsame_only", "FDIV", 0x2e20fc00U);
#undef ADVSIMD_FP_ARITHMETIC_PROOF

static const struct orlix_tcti_target_proof_case advsimd_halving_cases[]
	__attribute__((unused)) = {
	{ "orlix_tcti_advsimd_halving_source_leaves_decode",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_advsimd_halving_reserved_64bit_lanes_reject",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ "orlix_tcti_advsimd_halving_source_leaves_execute",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
};

#define ADVSIMD_HALVING_BINDING(name, ordinal, leaf, mnemonic, pattern) \
	static const struct orlix_tcti_target_proof_binding name##_bindings[] \
		__attribute__((unused)) = { \
		{ leaf, mnemonic, 0xbf20fc00U, pattern, \
		  "54434e440107000000310700000017070000000c01000000010101000000010101000000010102000000100000000c464541545f41647653494d44", \
		  ORLIX_TCTI_PROOF_U64_C(1), ordinal }, \
	}
ADVSIMD_HALVING_BINDING(advsimd_halving_shadd, 3895U,
	"SHADD_asimdsame_only", "SHADD", 0x0e200400U);
ADVSIMD_HALVING_BINDING(advsimd_halving_srhadd, 3897U,
	"SRHADD_asimdsame_only", "SRHADD", 0x0e201400U);
ADVSIMD_HALVING_BINDING(advsimd_halving_shsub, 3898U,
	"SHSUB_asimdsame_only", "SHSUB", 0x0e202400U);
ADVSIMD_HALVING_BINDING(advsimd_halving_uhadd, 3937U,
	"UHADD_asimdsame_only", "UHADD", 0x2e200400U);
ADVSIMD_HALVING_BINDING(advsimd_halving_urhadd, 3939U,
	"URHADD_asimdsame_only", "URHADD", 0x2e201400U);
ADVSIMD_HALVING_BINDING(advsimd_halving_uhsub, 3940U,
	"UHSUB_asimdsame_only", "UHSUB", 0x2e202400U);
#undef ADVSIMD_HALVING_BINDING

static const struct orlix_tcti_target_proof_case advsimd_mul_cases[]
	__attribute__((unused)) = {
	{ "advsimd_mul_bind_canonical_artifacts",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ "advsimd_mul_execute_all_aliases",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "advsimd_mul_reserved_forms_reject_without_mutation",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
};

#define ADVSIMD_MUL_BINDING(name, ordinal, leaf, mnemonic, pattern) \
	static const struct orlix_tcti_target_proof_binding name##_bindings[] \
		__attribute__((unused)) = { \
		{ leaf, mnemonic, 0xbf20fc00U, pattern, \
		  "54434e440107000000310700000017070000000c01000000010101000000010101000000010102000000100000000c464541545f41647653494d44", \
		  ORLIX_TCTI_PROOF_U64_C(1), ordinal }, \
	}

ADVSIMD_MUL_BINDING(advsimd_mul_mla, 3912U, "MLA_asimdsame_only", "MLA",
		    0x0e209400U);
ADVSIMD_MUL_BINDING(advsimd_mul_mul, 3913U, "MUL_asimdsame_only", "MUL",
		    0x0e209c00U);
ADVSIMD_MUL_BINDING(advsimd_mul_mls, 3954U, "MLS_asimdsame_only", "MLS",
		    0x2e209400U);

#undef ADVSIMD_MUL_BINDING

static const struct orlix_tcti_target_proof_case
advsimd_minmax_reduction_cases[] __attribute__((unused)) = {
	{ "orlix_tcti_minmaxv_source_bindings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_minmaxv_complete_encoding_domain",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ "orlix_tcti_minmaxv_legal_arrangements_resume",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "orlix_tcti_minmaxv_reserved_arrangements_resume",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
};

#define ADVSIMD_MINMAX_REDUCTION_BINDING(name, ordinal, leaf, mnemonic, pattern) \
	static const struct orlix_tcti_target_proof_binding name##_bindings[] \
		__attribute__((unused)) = { \
		{ leaf, mnemonic, 0xbf3ffc00U, pattern, \
		  "54434e440107000000310700000017070000000c01000000010101000000010101000000010102000000100000000c464541545f41647653494d44", \
		  ORLIX_TCTI_PROOF_U64_C(0xf), ordinal }, \
	}
ADVSIMD_MINMAX_REDUCTION_BINDING(advsimd_minmax_reduction_smaxv, 3855U,
	"SMAXV_asimdall_only", "SMAXV", 0x0e30a800U);
ADVSIMD_MINMAX_REDUCTION_BINDING(advsimd_minmax_reduction_sminv, 3856U,
	"SMINV_asimdall_only", "SMINV", 0x0e31a800U);
ADVSIMD_MINMAX_REDUCTION_BINDING(advsimd_minmax_reduction_umaxv, 3863U,
	"UMAXV_asimdall_only", "UMAXV", 0x2e30a800U);
ADVSIMD_MINMAX_REDUCTION_BINDING(advsimd_minmax_reduction_uminv, 3864U,
	"UMINV_asimdall_only", "UMINV", 0x2e31a800U);
#undef ADVSIMD_MINMAX_REDUCTION_BINDING

#define LOGICAL_BINDING(ordinal, leaf, mnemonic, pattern, cases) \
	{ leaf, mnemonic, 0xff200000U, pattern, LOGICAL_SHIFT_CONDITION, cases, ordinal }

static const struct orlix_tcti_target_proof_binding logical_and_bindings[] = {
	LOGICAL_BINDING(3434U, "AND_32_log_shift", "AND", 0x0a000000U, ORLIX_TCTI_PROOF_U64_C(0x1f)),
	LOGICAL_BINDING(3442U, "AND_64_log_shift", "AND", 0x8a000000U, ORLIX_TCTI_PROOF_U64_C(0x1f)),
};

static const struct orlix_tcti_target_proof_binding logical_bic_bindings[] = {
	LOGICAL_BINDING(3435U, "BIC_32_log_shift", "BIC", 0x0a200000U, ORLIX_TCTI_PROOF_U64_C(0x1f)),
	LOGICAL_BINDING(3443U, "BIC_64_log_shift", "BIC", 0x8a200000U, ORLIX_TCTI_PROOF_U64_C(0x1f)),
};

static const struct orlix_tcti_target_proof_binding logical_orr_bindings[] = {
	LOGICAL_BINDING(3436U, "ORR_32_log_shift", "ORR", 0x2a000000U, ORLIX_TCTI_PROOF_U64_C(0x3f)),
	LOGICAL_BINDING(3444U, "ORR_64_log_shift", "ORR", 0xaa000000U, ORLIX_TCTI_PROOF_U64_C(0x3f)),
};

static const struct orlix_tcti_target_proof_binding logical_orn_bindings[] = {
	LOGICAL_BINDING(3437U, "ORN_32_log_shift", "ORN", 0x2a200000U, ORLIX_TCTI_PROOF_U64_C(0x3f)),
	LOGICAL_BINDING(3445U, "ORN_64_log_shift", "ORN", 0xaa200000U, ORLIX_TCTI_PROOF_U64_C(0x3f)),
};

static const struct orlix_tcti_target_proof_binding logical_eor_bindings[] = {
	LOGICAL_BINDING(3438U, "EOR_32_log_shift", "EOR", 0x4a000000U, ORLIX_TCTI_PROOF_U64_C(0x1f)),
	LOGICAL_BINDING(3446U, "EOR_64_log_shift", "EOR", 0xca000000U, ORLIX_TCTI_PROOF_U64_C(0x1f)),
};

static const struct orlix_tcti_target_proof_binding logical_eon_bindings[] = {
	LOGICAL_BINDING(3439U, "EON_32_log_shift", "EON", 0x4a200000U, ORLIX_TCTI_PROOF_U64_C(0x1f)),
	LOGICAL_BINDING(3447U, "EON_64_log_shift", "EON", 0xca200000U, ORLIX_TCTI_PROOF_U64_C(0x1f)),
};

static const struct orlix_tcti_target_proof_binding logical_ands_bindings[] = {
	LOGICAL_BINDING(3440U, "ANDS_32_log_shift", "ANDS", 0x6a000000U, ORLIX_TCTI_PROOF_U64_C(0x3f)),
	LOGICAL_BINDING(3448U, "ANDS_64_log_shift", "ANDS", 0xea000000U, ORLIX_TCTI_PROOF_U64_C(0x3f)),
};

static const struct orlix_tcti_target_proof_binding logical_bics_bindings[] = {
	LOGICAL_BINDING(3441U, "BICS_32_log_shift", "BICS", 0x6a200000U, ORLIX_TCTI_PROOF_U64_C(0x1f)),
	LOGICAL_BINDING(3449U, "BICS_64_log_shift", "BICS", 0xea200000U, ORLIX_TCTI_PROOF_U64_C(0x1f)),
};

#undef LOGICAL_BINDING

#define CSSC_OBLIGATIONS \
	(BASELINE_OBLIGATIONS | ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC | ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS)

static const struct orlix_tcti_target_proof_case cssc_cases[] = {
	{ "orlix_tcti_integer_minmax_exact_source_cohort",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_cssc_min_max_immediate_source_fingerprints",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_cssc_min_max_immediate_all_legal_fields",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_cssc_min_max_immediate_execute_all_immediates",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "orlix_tcti_cssc_min_max_immediate_zero_registers_and_pstate",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "orlix_tcti_cssc_min_max_immediate_overlap",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "orlix_tcti_cssc_min_max_immediate_rejects_non_cssc_encodings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
};

static const struct orlix_tcti_target_proof_case cssc_data_processing_cases[] = {
	{ "orlix_tcti_integer_minmax_exact_source_cohort",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_cssc_data_processing_source_fingerprints",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_cssc_data_processing_all_legal_register_fields",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_cssc_data_processing_execute_boundaries",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "orlix_tcti_cssc_data_processing_zero_registers",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "orlix_tcti_cssc_data_processing_minmax_overlap",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "orlix_tcti_cssc_data_processing_rejects_reserved_opcodes",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
};

#define ADD_SUB_IMMEDIATE_BASE_OBLIGATIONS \
	(ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS)
#define ADD_SUB_IMMEDIATE_FLAGS_OBLIGATIONS \
	ADD_SUB_IMMEDIATE_BASE_OBLIGATIONS

static const struct orlix_tcti_target_proof_case add_sub_immediate_base_cases[]
	__attribute__((unused)) = {
	{ "orlix_tcti_add_sub_immediate_source_bindings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_add_sub_immediate_all_legal_encodings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_add_sub_immediate_production_path_arithmetic",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_add_sub_immediate_special_register_aliases",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_add_sub_immediate_source_mask_boundaries",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE },
};

static const struct orlix_tcti_target_proof_case add_sub_immediate_flags_cases[]
	__attribute__((unused)) = {
	{ "orlix_tcti_add_sub_immediate_source_bindings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_add_sub_immediate_all_legal_encodings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_add_sub_immediate_production_path_arithmetic",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "orlix_tcti_add_sub_immediate_special_register_aliases",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "orlix_tcti_add_sub_immediate_cmn_cmp_nzcv_boundaries",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "orlix_tcti_add_sub_immediate_source_mask_boundaries",
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE },
};

static const struct orlix_tcti_target_proof_case base_add_sub_production_cases[] = {
	{ "orlix_tcti_base_add_sub_decodes_exact_source_cohort",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_base_add_sub_binds_pinned_ddi0602_semantics",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_base_add_sub_production_resume",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "orlix_tcti_base_add_sub_reserved_encodings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ "orlix_tcti_base_add_sub_sp_zr_and_w_upper_zero",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "orlix_tcti_base_add_sub_extend_shift_and_carry",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "orlix_tcti_base_add_sub_simd_state_unchanged",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS },
	{ "orlix_tcti_base_add_sub_pointer_preserves_tag",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
};

static const struct orlix_tcti_target_proof_case
base_control_flow_production_cases[] = {
	{ "orlix_tcti_base_control_flow_decodes_exact_source_cohort",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_base_control_flow_binds_pinned_ddi0602_semantics",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_base_control_flow_production_resume",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_base_control_flow_link_and_xzr",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_base_control_flow_offsets_and_alignment",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_base_control_flow_simd_and_flags_unchanged",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_base_control_flow_non_el0_rejected",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_base_control_flow_reserved_encodings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
};

static const struct orlix_tcti_target_proof_case
base_exceptions_production_cases[] = {
	{ "orlix_tcti_base_exceptions_decodes_exact_source_cohort",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_base_exceptions_binds_pinned_ddi0602_semantics",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_base_exceptions_production_resume",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_base_exceptions_el0_traps",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_base_exceptions_syscall_handoff",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_base_exceptions_simd_and_flags_unchanged",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_base_exceptions_non_el0_rejected",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_base_exceptions_reserved_encodings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
};

static const struct orlix_tcti_target_proof_case
base_conditional_production_cases[] = {
	{ "orlix_tcti_base_conditional_decodes_exact_source_cohort",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_base_conditional_binds_pinned_ddi0602_semantics",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_base_conditional_production_resume",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_base_conditional_taken_and_not_taken",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_base_conditional_csel_aliases_and_w_upper",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_base_conditional_simd_and_flags_unchanged",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_base_conditional_non_el0_rejected",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_base_conditional_reserved_encodings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
};

static const struct orlix_tcti_target_proof_case
base_bitfield_unary_production_cases[] = {
	{ "orlix_tcti_base_bitfield_unary_decodes_exact_source_cohort",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_base_bitfield_unary_binds_pinned_ddi0602_semantics",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_base_bitfield_unary_production_resume",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_base_bitfield_unary_aliases_and_w_upper",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_base_bitfield_unary_extract_overlap",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_base_bitfield_unary_rbit_rev_clz",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_base_bitfield_unary_simd_and_flags_unchanged",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_base_bitfield_unary_non_el0_rejected",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_base_bitfield_unary_reserved_encodings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
};

static const struct orlix_tcti_target_proof_case
base_multiply_divide_production_cases[] = {
	{ "orlix_tcti_base_multiply_divide_decodes_exact_source_cohort",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_base_multiply_divide_binds_pinned_ddi0602_semantics",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_base_multiply_divide_production_resume",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_base_multiply_divide_divide_zero_signed_min_w_upper",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_base_multiply_divide_long_form_high_half",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_base_multiply_divide_accumulator_alias_xzr",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_base_multiply_divide_simd_and_flags_unchanged",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_base_multiply_divide_pointer_preserves_tag",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_base_multiply_divide_reserved_encodings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
};

static const struct orlix_tcti_target_proof_case
advsimd_crypto_production_cases[] = {
	{ "orlix_tcti_advsimd_crypto_decodes_exact_source_cohort",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_advsimd_crypto_binds_pinned_ddi0602_semantics",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_advsimd_crypto_production_resume",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_advsimd_crypto_destructive_alias",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_advsimd_crypto_pmull_halves_polynomial",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_advsimd_crypto_aes_round_upper_gprs",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_advsimd_crypto_reserved_encodings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
};

static const struct orlix_tcti_target_proof_case
advsimd_permute_move_simd_cases[] = {
	{ "orlix_tcti_advsimd_permute_move_decodes_exact_source_cohort",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_advsimd_permute_move_binds_pinned_ddi0602_semantics",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_advsimd_permute_move_production_resume",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_advsimd_permute_move_non_el0_rejected",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ "orlix_tcti_advsimd_permute_move_lane_q0_overlap",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_advsimd_permute_move_tbl_tbx_smov",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_advsimd_permute_move_reserved_encodings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
};

static const struct orlix_tcti_target_proof_case
advsimd_integer_simd_cases[] = {
	{ "orlix_tcti_advsimd_integer_decodes_exact_source_cohort",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_advsimd_integer_binds_pinned_ddi0602_semantics",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_advsimd_integer_production_resume",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_advsimd_integer_non_el0_rejected",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ "orlix_tcti_advsimd_integer_saturation_qc",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_advsimd_integer_narrow_shift_overlap",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_advsimd_integer_reserved_encodings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
};

static const struct orlix_tcti_target_proof_case
advsimd_fp_simd_cases[] = {
	{ "orlix_tcti_advsimd_fp_decodes_exact_source_cohort",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_advsimd_fp_binds_pinned_ddi0602_semantics",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_advsimd_fp_production_resume",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_advsimd_fp_non_el0_rejected",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ "orlix_tcti_advsimd_fp_nan_rounding_overlap",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_advsimd_fp_reserved_encodings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
};

static const struct orlix_tcti_target_proof_case scalar_fp_family_cases[] = {
	{ "orlix_tcti_scalar_fp_decodes_exact_source_cohort",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_scalar_fp_binds_pinned_ddi0602_semantics",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_scalar_fp_production_resume",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_scalar_fp_optional_rejected",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ "orlix_tcti_scalar_fp_edge_vectors",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_scalar_fp_reserved_encodings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
};

static const struct orlix_tcti_target_proof_case
advsimd_permute_move_gpr_cases[] = {
	{ "orlix_tcti_advsimd_permute_move_decodes_exact_source_cohort",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_advsimd_permute_move_binds_pinned_ddi0602_semantics",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_advsimd_permute_move_production_resume",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_advsimd_permute_move_tbl_tbx_smov",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_advsimd_permute_move_reserved_encodings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
};

static const struct orlix_tcti_target_proof_case
base_conditional_flags_cases[] = {
	{ "orlix_tcti_base_conditional_decodes_exact_source_cohort",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_base_conditional_binds_pinned_ddi0602_semantics",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_base_conditional_production_resume",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_base_conditional_taken_and_not_taken",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_base_conditional_csel_aliases_and_w_upper",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_base_conditional_ccmp_true_false_nzcv",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "orlix_tcti_base_conditional_simd_and_flags_unchanged",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_base_conditional_non_el0_rejected",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_base_conditional_reserved_encodings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
};

static const struct orlix_tcti_target_proof_case lse_source_bound_cases[]
	__attribute__((unused)) = {
	{ "lse_source_bound_decodes_every_base_leaf",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "lse_source_bound_executes_every_base_leaf",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "lse_source_bound_rejects_reserved_encodings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
};

static const struct orlix_tcti_target_proof_case base_atomic_source_bound_cases[] = {
	{ "orlix_tcti_base_atomic_decodes_exact_source_cohort",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_base_atomic_binds_pinned_ddi0602_semantics",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_base_atomic_decodes_register_variants",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS },
	{ "orlix_tcti_base_atomic_rejects_reserved_registers",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ "orlix_tcti_base_atomic_classifies_fixed_bit_neighbours",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ "orlix_tcti_base_atomic_decodes_ordered_access_shapes",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ORDERING },
	{ "orlix_tcti_base_atomic_executes_every_source_leaf",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "orlix_tcti_base_atomic_executes_every_fp_leaf",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "orlix_tcti_base_atomic_rcw_conditional_writes_and_flags",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "orlix_tcti_base_atomic_executes_ls64_state",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_base_atomic_faults_are_precise",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS },
	{ "orlix_tcti_base_atomic_concurrent_no_tearing",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ATOMICITY },
	{ "orlix_tcti_base_atomic_exclusive_monitor_is_exact",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ATOMICITY },
	{ "orlix_tcti_base_atomic_forbidden_ordering_outcomes",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ORDERING },
	{ "orlix_tcti_base_atomic_production_resume",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ORDERING },
	{ "orlix_tcti_base_atomic_unmapped_faults",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS },
	{ "orlix_tcti_base_atomic_monitor_mismatch_fails_store",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ATOMICITY },
	{ "orlix_tcti_base_atomic_casp_whole_pair",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ATOMICITY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY },
	{ "orlix_tcti_base_atomic_cas_rmw_returns_old_value",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ATOMICITY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY },
	{ "orlix_tcti_base_atomic_production_atomicity",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ATOMICITY },
};

static const struct orlix_tcti_target_proof_case base_atomic_production_cases[] = {
	{ "orlix_tcti_base_atomic_production_resume",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ORDERING },
	{ "orlix_tcti_base_atomic_unmapped_faults",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS },
	{ "orlix_tcti_base_atomic_production_atomicity",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ATOMICITY },
};

static const struct orlix_tcti_target_proof_case base_ordered_source_bound_cases[] = {
	{ "orlix_tcti_base_atomic_decodes_exact_source_cohort",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_base_atomic_binds_pinned_ddi0602_semantics",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_base_atomic_decodes_register_variants",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS },
	{ "orlix_tcti_base_atomic_rejects_reserved_registers",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ "orlix_tcti_base_atomic_classifies_fixed_bit_neighbours",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ "orlix_tcti_base_atomic_decodes_ordered_access_shapes",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ORDERING },
	{ "orlix_tcti_base_atomic_executes_every_source_leaf",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "orlix_tcti_base_atomic_executes_every_fp_leaf",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "orlix_tcti_base_atomic_rcw_conditional_writes_and_flags",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "orlix_tcti_base_atomic_executes_ls64_state",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_base_atomic_faults_are_precise",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS },
	{ "orlix_tcti_base_atomic_forbidden_ordering_outcomes",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ORDERING },
	{ "orlix_tcti_base_atomic_production_resume",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ORDERING },
	{ "orlix_tcti_base_atomic_unmapped_faults",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS },
};

static const struct orlix_tcti_target_proof_case lse128_source_bound_cases[]
	__attribute__((unused)) = {
	{ "orlix_tcti_lse128_resume_all_source_leaves",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_lse128_resume_rejects_reserved_operations",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ "orlix_tcti_lse128_resume_alignment_faults_all_leaves",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS },
	{ "orlix_tcti_lse128_resume_readonly_faults_all_leaves",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS },
	{ "orlix_tcti_lse128_resume_unmapped_faults_all_leaves",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS },
	{ "orlix_tcti_lse128_resume_rejects_fixed_bit_near_misses",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
};

static const struct orlix_tcti_target_proof_case exclusive_source_bound_cases[]
	__attribute__((unused)) = {
	{ "orlix_tcti_decode_exhaustive_load_store_exclusive_family",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ "orlix_tcti_decode_load_store_exclusive_all_register_fields",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS },
	{ "orlix_tcti_switch_fails_store_exclusive_without_reservation",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS },
	{ "orlix_tcti_gadget_executes_complete_load_store_exclusive_family",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS },
};

#define ADD_SUB_IMMEDIATE_BASE_BINDING(ordinal, leaf, mnemonic, pattern) \
	{ leaf, mnemonic, 0xff800000U, pattern, ADD_SUB_IMMEDIATE_CONDITION, \
	  ORLIX_TCTI_PROOF_U64_C(0xff), ordinal }
#define ADD_SUB_IMMEDIATE_FLAGS_BINDING(ordinal, leaf, mnemonic, pattern) \
	{ leaf, mnemonic, 0xff800000U, pattern, ADD_SUB_IMMEDIATE_CONDITION, \
	  ORLIX_TCTI_PROOF_U64_C(0xff), ordinal }

static const struct orlix_tcti_target_proof_binding add_sub_immediate_add_bindings[] = {
	ADD_SUB_IMMEDIATE_BASE_BINDING(2173U, "ADD_32_addsub_imm", "ADD", 0x11000000U),
	ADD_SUB_IMMEDIATE_BASE_BINDING(2177U, "ADD_64_addsub_imm", "ADD", 0x91000000U),
};

static const struct orlix_tcti_target_proof_binding add_sub_immediate_adds_bindings[] = {
	ADD_SUB_IMMEDIATE_FLAGS_BINDING(2174U, "ADDS_32S_addsub_imm", "ADDS", 0x31000000U),
	ADD_SUB_IMMEDIATE_FLAGS_BINDING(2178U, "ADDS_64S_addsub_imm", "ADDS", 0xb1000000U),
};

static const struct orlix_tcti_target_proof_binding add_sub_immediate_sub_bindings[] = {
	ADD_SUB_IMMEDIATE_BASE_BINDING(2175U, "SUB_32_addsub_imm", "SUB", 0x51000000U),
	ADD_SUB_IMMEDIATE_BASE_BINDING(2179U, "SUB_64_addsub_imm", "SUB", 0xd1000000U),
};

static const struct orlix_tcti_target_proof_binding add_sub_immediate_subs_bindings[] = {
	ADD_SUB_IMMEDIATE_FLAGS_BINDING(2176U, "SUBS_32S_addsub_imm", "SUBS", 0x71000000U),
	ADD_SUB_IMMEDIATE_FLAGS_BINDING(2180U, "SUBS_64S_addsub_imm", "SUBS", 0xf1000000U),
};

#undef ADD_SUB_IMMEDIATE_FLAGS_BINDING
#undef ADD_SUB_IMMEDIATE_BASE_BINDING

#define CSSC_BINDING(ordinal, leaf, mnemonic, pattern) \
	{ leaf, mnemonic, 0xfffc0000U, pattern, CSSC_CONDITION, ORLIX_TCTI_PROOF_U64_C(0x1f), ordinal }

static const struct orlix_tcti_target_proof_binding cssc_smax_bindings[] = {
	CSSC_BINDING(2183U, "SMAX_32_minmax_imm", "SMAX", 0x11c00000U),
	CSSC_BINDING(2187U, "SMAX_64_minmax_imm", "SMAX", 0x91c00000U),
};

static const struct orlix_tcti_target_proof_binding cssc_umax_bindings[] = {
	CSSC_BINDING(2184U, "UMAX_32U_minmax_imm", "UMAX", 0x11c40000U),
	CSSC_BINDING(2188U, "UMAX_64U_minmax_imm", "UMAX", 0x91c40000U),
};

static const struct orlix_tcti_target_proof_binding cssc_smin_bindings[] = {
	CSSC_BINDING(2185U, "SMIN_32_minmax_imm", "SMIN", 0x11c80000U),
	CSSC_BINDING(2189U, "SMIN_64_minmax_imm", "SMIN", 0x91c80000U),
};

static const struct orlix_tcti_target_proof_binding cssc_umin_bindings[] = {
	CSSC_BINDING(2186U, "UMIN_32U_minmax_imm", "UMIN", 0x11cc0000U),
	CSSC_BINDING(2190U, "UMIN_64U_minmax_imm", "UMIN", 0x91cc0000U),
};

#undef CSSC_BINDING

#define CSSC_DP2_BINDING(ordinal, leaf, mnemonic, pattern) \
	{ leaf, mnemonic, 0xffe0fc00U, pattern, CSSC_CONDITION, ORLIX_TCTI_PROOF_U64_C(0x1f), ordinal }

static const struct orlix_tcti_target_proof_binding cssc_smax_reg_bindings[] = {
	CSSC_DP2_BINDING(3368U, "SMAX_32_dp_2src", "SMAX", 0x1ac06000U),
	CSSC_DP2_BINDING(3384U, "SMAX_64_dp_2src", "SMAX", 0x9ac06000U),
};

static const struct orlix_tcti_target_proof_binding cssc_umax_reg_bindings[] = {
	CSSC_DP2_BINDING(3369U, "UMAX_32_dp_2src", "UMAX", 0x1ac06400U),
	CSSC_DP2_BINDING(3385U, "UMAX_64_dp_2src", "UMAX", 0x9ac06400U),
};

static const struct orlix_tcti_target_proof_binding cssc_smin_reg_bindings[] = {
	CSSC_DP2_BINDING(3370U, "SMIN_32_dp_2src", "SMIN", 0x1ac06800U),
	CSSC_DP2_BINDING(3386U, "SMIN_64_dp_2src", "SMIN", 0x9ac06800U),
};

static const struct orlix_tcti_target_proof_binding cssc_umin_reg_bindings[] = {
	CSSC_DP2_BINDING(3371U, "UMIN_32_dp_2src", "UMIN", 0x1ac06c00U),
	CSSC_DP2_BINDING(3387U, "UMIN_64_dp_2src", "UMIN", 0x9ac06c00U),
};

#undef CSSC_DP2_BINDING

static const struct orlix_tcti_target_proof_case integer_conditional_base_cases[]
	__attribute__((unused)) = {
	{ "orlix_tcti_integer_conditional_source_bindings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_integer_conditional_all_divide_leaves_production_path",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_integer_conditional_select_leaves_production_path",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_integer_conditional_multiply_leaves_production_path",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_integer_conditional_reserved_encodings_fail_before_state",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
};

static const struct orlix_tcti_target_proof_case integer_conditional_flags_cases[]
	__attribute__((unused)) = {
	{ "orlix_tcti_integer_conditional_source_bindings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_integer_conditional_compare_leaves_production_path",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "orlix_tcti_integer_conditional_reserved_encodings_fail_before_state",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
};

static const struct orlix_tcti_target_proof_case flag_manipulation_cases[] = {
	{ "orlix_tcti_flag_source_and_slice_bindings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_flag_legal_encoding_matrix",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_flag_reserved_fixed_bit_matrix",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ "orlix_tcti_flag_pstate_semantics_matrix",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "orlix_tcti_flag_non_el0_rejected_without_state_change",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
};

#define FLAG_MANIPULATION_CASES ORLIX_TCTI_PROOF_U64_C(0x1f)
static const struct orlix_tcti_target_proof_binding flag_rmif_bindings[] = {
	{ "RMIF_only_rmif", "RMIF", 0xffe07c10U, 0xba000400U,
	  FLAG_MANIPULATION_CONDITION, FLAG_MANIPULATION_CASES, 3476U },
};
static const struct orlix_tcti_target_proof_binding flag_setf_bindings[] = {
	{ "SETF8_only_setf", "SETF8", 0xfffffc1fU, 0x3a00080dU,
	  FLAG_MANIPULATION_CONDITION, FLAG_MANIPULATION_CASES, 3477U },
	{ "SETF16_only_setf", "SETF16", 0xfffffc1fU, 0x3a00480dU,
	  FLAG_MANIPULATION_CONDITION, FLAG_MANIPULATION_CASES, 3478U },
};
#undef FLAG_MANIPULATION_CASES

#define INTEGER_CONDITIONAL_BINDING(ordinal, leaf, mnemonic, mask, pattern, cases) \
	{ leaf, mnemonic, mask, pattern, ADD_SUB_IMMEDIATE_CONDITION, cases, ordinal }
#define INTEGER_CONDITIONAL_PAIR_BINDINGS(name, ordinal32, leaf32, ordinal64, leaf64, \
					  mnemonic, mask, pattern32, pattern64, cases) \
	static const struct orlix_tcti_target_proof_binding name[] = { \
		INTEGER_CONDITIONAL_BINDING(ordinal32, leaf32, mnemonic, mask, pattern32, cases), \
		INTEGER_CONDITIONAL_BINDING(ordinal64, leaf64, mnemonic, mask, pattern64, cases), \
	}

INTEGER_CONDITIONAL_PAIR_BINDINGS(integer_udiv_bindings, 3356U,
	"UDIV_32_dp_2src", 3373U, "UDIV_64_dp_2src", "UDIV", 0xffe0fc00U,
	0x1ac00800U, 0x9ac00800U, ORLIX_TCTI_PROOF_U64_C(0x1ff));
INTEGER_CONDITIONAL_PAIR_BINDINGS(integer_sdiv_bindings, 3357U,
	"SDIV_32_dp_2src", 3374U, "SDIV_64_dp_2src", "SDIV", 0xffe0fc00U,
	0x1ac00c00U, 0x9ac00c00U, ORLIX_TCTI_PROOF_U64_C(0x1ff));
INTEGER_CONDITIONAL_PAIR_BINDINGS(integer_ccmn_reg_bindings, 3479U,
	"CCMN_32_condcmp_reg", 3481U, "CCMN_64_condcmp_reg", "CCMN", 0xffe00c10U,
	0x3a400000U, 0xba400000U, ORLIX_TCTI_PROOF_U64_C(0x27));
INTEGER_CONDITIONAL_PAIR_BINDINGS(integer_ccmp_reg_bindings, 3480U,
	"CCMP_32_condcmp_reg", 3482U, "CCMP_64_condcmp_reg", "CCMP", 0xffe00c10U,
	0x7a400000U, 0xfa400000U, ORLIX_TCTI_PROOF_U64_C(0x27));
INTEGER_CONDITIONAL_PAIR_BINDINGS(integer_ccmn_imm_bindings, 3483U,
	"CCMN_32_condcmp_imm", 3485U, "CCMN_64_condcmp_imm", "CCMN", 0xffe00c10U,
	0x3a400800U, 0xba400800U, ORLIX_TCTI_PROOF_U64_C(0x27));
INTEGER_CONDITIONAL_PAIR_BINDINGS(integer_ccmp_imm_bindings, 3484U,
	"CCMP_32_condcmp_imm", 3486U, "CCMP_64_condcmp_imm", "CCMP", 0xffe00c10U,
	0x7a400800U, 0xfa400800U, ORLIX_TCTI_PROOF_U64_C(0x27));
INTEGER_CONDITIONAL_PAIR_BINDINGS(integer_csel_bindings, 3487U,
	"CSEL_32_condsel", 3491U, "CSEL_64_condsel", "CSEL", 0xffe00c00U,
	0x1a800000U, 0x9a800000U, ORLIX_TCTI_PROOF_U64_C(0x15));
INTEGER_CONDITIONAL_PAIR_BINDINGS(integer_csinc_bindings, 3488U,
	"CSINC_32_condsel", 3492U, "CSINC_64_condsel", "CSINC", 0xffe00c00U,
	0x1a800400U, 0x9a800400U, ORLIX_TCTI_PROOF_U64_C(0x15));
INTEGER_CONDITIONAL_PAIR_BINDINGS(integer_csinv_bindings, 3489U,
	"CSINV_32_condsel", 3493U, "CSINV_64_condsel", "CSINV", 0xffe00c00U,
	0x5a800000U, 0xda800000U, ORLIX_TCTI_PROOF_U64_C(0x15));
INTEGER_CONDITIONAL_PAIR_BINDINGS(integer_csneg_bindings, 3490U,
	"CSNEG_32_condsel", 3494U, "CSNEG_64_condsel", "CSNEG", 0xffe00c00U,
	0x5a800400U, 0xda800400U, ORLIX_TCTI_PROOF_U64_C(0x15));
INTEGER_CONDITIONAL_PAIR_BINDINGS(integer_madd_bindings, 3495U,
	"MADD_32A_dp_3src", 3497U, "MADD_64A_dp_3src", "MADD", 0xffe08000U,
	0x1b000000U, 0x9b000000U, ORLIX_TCTI_PROOF_U64_C(0x1ff));
INTEGER_CONDITIONAL_PAIR_BINDINGS(integer_msub_bindings, 3496U,
	"MSUB_32A_dp_3src", 3498U, "MSUB_64A_dp_3src", "MSUB", 0xffe08000U,
	0x1b008000U, 0x9b008000U, ORLIX_TCTI_PROOF_U64_C(0x1ff));
static const struct orlix_tcti_target_proof_binding integer_smaddl_bindings[] = {
	INTEGER_CONDITIONAL_BINDING(3499U, "SMADDL_64WA_dp_3src", "SMADDL",
		0xffe08000U, 0x9b200000U, ORLIX_TCTI_PROOF_U64_C(0x1ff)),
};
static const struct orlix_tcti_target_proof_binding integer_smsubl_bindings[] = {
	INTEGER_CONDITIONAL_BINDING(3500U, "SMSUBL_64WA_dp_3src", "SMSUBL",
		0xffe08000U, 0x9b208000U, ORLIX_TCTI_PROOF_U64_C(0x1ff)),
};
static const struct orlix_tcti_target_proof_binding integer_smulh_bindings[] = {
	INTEGER_CONDITIONAL_BINDING(3501U, "SMULH_64_dp_3src", "SMULH",
		0xffe0fc00U, 0x9b407c00U, ORLIX_TCTI_PROOF_U64_C(0x1ff)),
};
static const struct orlix_tcti_target_proof_binding integer_umaddl_bindings[] = {
	INTEGER_CONDITIONAL_BINDING(3504U, "UMADDL_64WA_dp_3src", "UMADDL",
		0xffe08000U, 0x9ba00000U, ORLIX_TCTI_PROOF_U64_C(0x1ff)),
};
static const struct orlix_tcti_target_proof_binding integer_umsubl_bindings[] = {
	INTEGER_CONDITIONAL_BINDING(3505U, "UMSUBL_64WA_dp_3src", "UMSUBL",
		0xffe08000U, 0x9ba08000U, ORLIX_TCTI_PROOF_U64_C(0x1ff)),
};
static const struct orlix_tcti_target_proof_binding integer_umulh_bindings[] = {
	INTEGER_CONDITIONAL_BINDING(3506U, "UMULH_64_dp_3src", "UMULH",
		0xffe0fc00U, 0x9bc07c00U, ORLIX_TCTI_PROOF_U64_C(0x1ff)),
};
#undef INTEGER_CONDITIONAL_PAIR_BINDINGS
#undef INTEGER_CONDITIONAL_BINDING

#define LOGICAL_ENTRY(proof_id, operation, obligations, cases, bindings) \
	{ proof_id, operation, ORLIX_TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0, obligations, \
	  ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE, \
	  LOGICAL_SHIFT_SOURCE, LOGICAL_SHIFT_SUITE, cases, ARRAY_COUNT(cases), \
	  bindings, ARRAY_COUNT(bindings), NULL, obligations }

#define CSSC_ENTRY(proof_id, operation, bindings) \
	{ proof_id, operation, ORLIX_TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0, \
	  CSSC_OBLIGATIONS, ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE, \
	  CSSC_SOURCE, CSSC_SUITE, cssc_cases, ARRAY_COUNT(cssc_cases), \
	  bindings, ARRAY_COUNT(bindings), NULL, CSSC_OBLIGATIONS }

#define CSSC_DATA_ENTRY(proof_id, operation, bindings) \
	{ proof_id, operation, ORLIX_TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0, \
	  CSSC_OBLIGATIONS, ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE, \
	  CSSC_SOURCE, CSSC_SUITE, cssc_data_processing_cases, \
	  ARRAY_COUNT(cssc_data_processing_cases), bindings, ARRAY_COUNT(bindings), \
	  NULL, CSSC_OBLIGATIONS }
#define INTEGER_CONDITIONAL_ENTRY(proof_id, operation, obligations, cases, bindings) \
	{ proof_id, operation, ORLIX_TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0, \
	  obligations, ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE, \
	  BASE_MULTIPLY_DIVIDE_SOURCE, BASE_MULTIPLY_DIVIDE_SUITE, \
	  cases, ARRAY_COUNT(cases), \
	  bindings, ARRAY_COUNT(bindings), NULL, obligations }
#define BASE_CONDITIONAL_ENTRY(proof_id, operation, obligations, cases, bindings) \
	{ proof_id, operation, ORLIX_TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0, \
	  obligations, ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE, \
	  BASE_CONDITIONAL_SOURCE, BASE_CONDITIONAL_SUITE, \
	  cases, ARRAY_COUNT(cases), \
	  bindings, ARRAY_COUNT(bindings), NULL, obligations }
#define FLAG_MANIPULATION_ENTRY(proof_id, operation, bindings) \
	{ proof_id, operation, ORLIX_TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0, \
	  INTEGER_CONDITIONAL_FLAGS_OBLIGATIONS, \
	  ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE, \
	  FLAG_MANIPULATION_SOURCE, FLAG_MANIPULATION_SUITE, \
	  flag_manipulation_cases, ARRAY_COUNT(flag_manipulation_cases), \
	  bindings, ARRAY_COUNT(bindings), NULL, \
	  INTEGER_CONDITIONAL_FLAGS_OBLIGATIONS }

#define ADD_SUB_IMMEDIATE_ENTRY(proof_id, operation, obligations, cases, bindings) \
	{ proof_id, operation, ORLIX_TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0, obligations, \
	  ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE, \
	  BASE_ADD_SUB_SOURCE, BASE_ADD_SUB_SUITE, cases, \
	  ARRAY_COUNT(cases), bindings, ARRAY_COUNT(bindings), NULL, obligations }

#define ADVSIMD_FP_ARITHMETIC_ENTRY(proof_id, operation, cases, bindings) \
	{ proof_id, operation, ORLIX_TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0, \
	  ADVSIMD_FP_ARITHMETIC_OBLIGATIONS, \
	  ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE, \
	  ADVSIMD_FP_ARITHMETIC_SOURCE, ADVSIMD_FP_ARITHMETIC_SUITE, cases, \
	  ARRAY_COUNT(cases), bindings, ARRAY_COUNT(bindings), NULL, \
	  ADVSIMD_FP_ARITHMETIC_OBLIGATIONS }
#define ADVSIMD_HALVING_ENTRY(proof_id, operation, bindings) \
	{ proof_id, operation, ORLIX_TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0, \
	  ADVSIMD_HALVING_OBLIGATIONS, \
	  ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE, \
	  ADVSIMD_HALVING_SOURCE, ADVSIMD_HALVING_SUITE, \
	  advsimd_halving_cases, ARRAY_COUNT(advsimd_halving_cases), \
	  bindings, ARRAY_COUNT(bindings), NULL, ADVSIMD_HALVING_OBLIGATIONS }
#define ADVSIMD_MUL_ENTRY(proof_id, operation, bindings) \
	{ proof_id, operation, ORLIX_TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0, \
	  ADVSIMD_MUL_OBLIGATIONS, \
	  ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE, \
	  ADVSIMD_MUL_SOURCE, ADVSIMD_MUL_SUITE, advsimd_mul_cases, \
	  ARRAY_COUNT(advsimd_mul_cases), bindings, ARRAY_COUNT(bindings), NULL, \
	  ADVSIMD_MUL_OBLIGATIONS }
#define ADVSIMD_MINMAX_REDUCTION_ENTRY(proof_id, operation, bindings) \
	{ proof_id, operation, ORLIX_TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0, \
	  ADVSIMD_MINMAX_REDUCTION_OBLIGATIONS, \
	  ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE, \
	  ADVSIMD_MINMAX_REDUCTION_SOURCE, ADVSIMD_MINMAX_REDUCTION_SUITE, \
	  advsimd_minmax_reduction_cases, \
	  ARRAY_COUNT(advsimd_minmax_reduction_cases), \
	  bindings, ARRAY_COUNT(bindings), NULL, \
	  ADVSIMD_MINMAX_REDUCTION_OBLIGATIONS }
#define CORE_PROOF_REGISTRY_ENTRY_COUNT 40U
#define ORDINARY_LOAD_STORE_PROOF_REGISTRY_ENTRY_COUNT 70U
#define ORDINARY_LOAD_STORE_PROOF_REGISTRY_BINDING_COUNT 209U
#define ADVSIMD_LOAD_STORE_PROOF_REGISTRY_ENTRY_COUNT 22U
#define ADVSIMD_LOAD_STORE_PROOF_REGISTRY_BINDING_COUNT 152U
#define LSE_PROOF_REGISTRY_ENTRY_COUNT 34U
#define LSE_PROOF_REGISTRY_BINDING_COUNT 180U
#define SCALAR_PROOF_REGISTRY_ENTRY_COUNT 378U
#define SCALAR_PROOF_REGISTRY_BINDING_COUNT 624U
#define EXCLUSIVE_PROOF_REGISTRY_ENTRY_COUNT 16U
#define EXCLUSIVE_PROOF_REGISTRY_BINDING_COUNT 24U
#define SOURCE_LEAF_REJECTION_PROOF_REGISTRY_ENTRY_COUNT 39U
#define SOURCE_LEAF_REJECTION_PROOF_REGISTRY_BINDING_COUNT 81U
enum {
	PRODUCTION_CAPTURE_FAMILY_PROOF_REGISTRY_ENTRY_COUNT = 0
#define ORLIX_TCTI_PROOF_FAMILY_OPERATION(proof_id_value, operation_value, linux_value, obligations_value) + 1
#include "target_production_capture_family.def"
#undef ORLIX_TCTI_PROOF_FAMILY_OPERATION
};
#define PRODUCTION_CAPTURE_FAMILY_PROOF_REGISTRY_BINDING_COUNT 0U
#define MOPS_COPY_PROOF_REGISTRY_ENTRY_COUNT 32U
#define MOPS_COPY_PROOF_REGISTRY_BINDING_COUNT 96U
#define BASE_ATOMIC_MISSING_PROOF_REGISTRY_ENTRY_COUNT 294U

static bool proof_registry_initialized;
static bool proof_registry_initialization_attempted;
static enum orlix_tcti_target_proof_registry_error
	proof_registry_initialization_error =
		ORLIX_TCTI_TARGET_PROOF_REGISTRY_OK;
#ifdef __KERNEL__
static DEFINE_MUTEX(proof_registry_initialization_lock);
#else
static pthread_once_t proof_registry_initialization_once = PTHREAD_ONCE_INIT;
#endif
static struct orlix_tcti_target_proof_registry_entry proof_registry_entries[
	CORE_PROOF_REGISTRY_ENTRY_COUNT + LSE_PROOF_REGISTRY_ENTRY_COUNT +
	ORDINARY_LOAD_STORE_PROOF_REGISTRY_ENTRY_COUNT +
	ADVSIMD_LOAD_STORE_PROOF_REGISTRY_ENTRY_COUNT +
	SCALAR_PROOF_REGISTRY_ENTRY_COUNT +
	EXCLUSIVE_PROOF_REGISTRY_ENTRY_COUNT +
	SOURCE_LEAF_REJECTION_PROOF_REGISTRY_ENTRY_COUNT +
	PRODUCTION_CAPTURE_FAMILY_PROOF_REGISTRY_ENTRY_COUNT +
	MOPS_COPY_PROOF_REGISTRY_ENTRY_COUNT +
	BASE_ATOMIC_MISSING_PROOF_REGISTRY_ENTRY_COUNT] = {
	LOGICAL_ENTRY("kunit:logical-shifted-register-and", "AND_log_shift",
		      LOGICAL_BASE_OBLIGATIONS, logical_base_cases,
		      logical_and_bindings),
	LOGICAL_ENTRY("kunit:logical-shifted-register-bic", "BIC_log_shift",
		      LOGICAL_BASE_OBLIGATIONS, logical_base_cases,
		      logical_bic_bindings),
	LOGICAL_ENTRY("kunit:logical-shifted-register-orr", "ORR_log_shift",
		      LOGICAL_BASE_OBLIGATIONS, logical_base_alias_cases,
		      logical_orr_bindings),
	LOGICAL_ENTRY("kunit:logical-shifted-register-orn", "ORN_log_shift",
		      LOGICAL_BASE_OBLIGATIONS, logical_base_alias_cases,
		      logical_orn_bindings),
	LOGICAL_ENTRY("kunit:logical-shifted-register-eor", "EOR_log_shift",
		      LOGICAL_BASE_OBLIGATIONS, logical_base_cases,
		      logical_eor_bindings),
	LOGICAL_ENTRY("kunit:logical-shifted-register-eon", "EON",
		      LOGICAL_BASE_OBLIGATIONS, logical_base_cases,
		      logical_eon_bindings),
	LOGICAL_ENTRY("kunit:logical-shifted-register-ands", "ANDS_log_shift",
		      LOGICAL_FLAGS_OBLIGATIONS, logical_flags_alias_cases,
		      logical_ands_bindings),
	LOGICAL_ENTRY("kunit:logical-shifted-register-bics", "BICS",
		      LOGICAL_FLAGS_OBLIGATIONS, logical_flags_cases,
		      logical_bics_bindings),
	CSSC_ENTRY("kunit:cssc-min-max-immediate-smax", "SMAX_imm",
		   cssc_smax_bindings),
	CSSC_ENTRY("kunit:cssc-min-max-immediate-umax", "UMAX_imm",
		   cssc_umax_bindings),
	CSSC_ENTRY("kunit:cssc-min-max-immediate-smin", "SMIN_imm",
		   cssc_smin_bindings),
	CSSC_ENTRY("kunit:cssc-min-max-immediate-umin", "UMIN_imm",
		   cssc_umin_bindings),
	CSSC_DATA_ENTRY("kunit:cssc-data-processing-smax", "SMAX_reg",
			cssc_smax_reg_bindings),
	CSSC_DATA_ENTRY("kunit:cssc-data-processing-umax", "UMAX_reg",
			cssc_umax_reg_bindings),
	CSSC_DATA_ENTRY("kunit:cssc-data-processing-smin", "SMIN_reg",
			cssc_smin_reg_bindings),
	CSSC_DATA_ENTRY("kunit:cssc-data-processing-umin", "UMIN_reg",
			cssc_umin_reg_bindings),
	INTEGER_CONDITIONAL_ENTRY("kunit:integer-conditional-udiv", "UDIV",
		INTEGER_CONDITIONAL_BASE_OBLIGATIONS, base_multiply_divide_production_cases,
		integer_udiv_bindings),
	INTEGER_CONDITIONAL_ENTRY("kunit:integer-conditional-sdiv", "SDIV",
		INTEGER_CONDITIONAL_BASE_OBLIGATIONS, base_multiply_divide_production_cases,
		integer_sdiv_bindings),
	BASE_CONDITIONAL_ENTRY("kunit:integer-conditional-ccmn-reg", "CCMN_reg",
		INTEGER_CONDITIONAL_FLAGS_OBLIGATIONS, base_conditional_flags_cases,
		integer_ccmn_reg_bindings),
	BASE_CONDITIONAL_ENTRY("kunit:integer-conditional-ccmp-reg", "CCMP_reg",
		INTEGER_CONDITIONAL_FLAGS_OBLIGATIONS, base_conditional_flags_cases,
		integer_ccmp_reg_bindings),
	BASE_CONDITIONAL_ENTRY("kunit:integer-conditional-ccmn-imm", "CCMN_imm",
		INTEGER_CONDITIONAL_FLAGS_OBLIGATIONS, base_conditional_flags_cases,
		integer_ccmn_imm_bindings),
	BASE_CONDITIONAL_ENTRY("kunit:integer-conditional-ccmp-imm", "CCMP_imm",
		INTEGER_CONDITIONAL_FLAGS_OBLIGATIONS, base_conditional_flags_cases,
		integer_ccmp_imm_bindings),
	BASE_CONDITIONAL_ENTRY("kunit:integer-conditional-csel", "CSEL",
		INTEGER_CONDITIONAL_BASE_OBLIGATIONS, base_conditional_production_cases,
		integer_csel_bindings),
	BASE_CONDITIONAL_ENTRY("kunit:integer-conditional-csinc", "CSINC",
		INTEGER_CONDITIONAL_BASE_OBLIGATIONS, base_conditional_production_cases,
		integer_csinc_bindings),
	BASE_CONDITIONAL_ENTRY("kunit:integer-conditional-csinv", "CSINV",
		INTEGER_CONDITIONAL_BASE_OBLIGATIONS, base_conditional_production_cases,
		integer_csinv_bindings),
	BASE_CONDITIONAL_ENTRY("kunit:integer-conditional-csneg", "CSNEG",
		INTEGER_CONDITIONAL_BASE_OBLIGATIONS, base_conditional_production_cases,
		integer_csneg_bindings),
	INTEGER_CONDITIONAL_ENTRY("kunit:integer-conditional-madd", "MADD",
		INTEGER_CONDITIONAL_BASE_OBLIGATIONS, base_multiply_divide_production_cases,
		integer_madd_bindings),
	INTEGER_CONDITIONAL_ENTRY("kunit:integer-conditional-msub", "MSUB",
		INTEGER_CONDITIONAL_BASE_OBLIGATIONS, base_multiply_divide_production_cases,
		integer_msub_bindings),
	INTEGER_CONDITIONAL_ENTRY("kunit:integer-conditional-smaddl", "SMADDL",
		INTEGER_CONDITIONAL_BASE_OBLIGATIONS, base_multiply_divide_production_cases,
		integer_smaddl_bindings),
	INTEGER_CONDITIONAL_ENTRY("kunit:integer-conditional-smsubl", "SMSUBL",
		INTEGER_CONDITIONAL_BASE_OBLIGATIONS, base_multiply_divide_production_cases,
		integer_smsubl_bindings),
	INTEGER_CONDITIONAL_ENTRY("kunit:integer-conditional-smulh", "SMULH",
		INTEGER_CONDITIONAL_BASE_OBLIGATIONS, base_multiply_divide_production_cases,
		integer_smulh_bindings),
	INTEGER_CONDITIONAL_ENTRY("kunit:integer-conditional-umaddl", "UMADDL",
		INTEGER_CONDITIONAL_BASE_OBLIGATIONS, base_multiply_divide_production_cases,
		integer_umaddl_bindings),
	INTEGER_CONDITIONAL_ENTRY("kunit:integer-conditional-umsubl", "UMSUBL",
		INTEGER_CONDITIONAL_BASE_OBLIGATIONS, base_multiply_divide_production_cases,
		integer_umsubl_bindings),
	INTEGER_CONDITIONAL_ENTRY("kunit:integer-conditional-umulh", "UMULH",
		INTEGER_CONDITIONAL_BASE_OBLIGATIONS, base_multiply_divide_production_cases,
		integer_umulh_bindings),
	FLAG_MANIPULATION_ENTRY("kunit:flag-manipulation-rmif", "RMIF",
		flag_rmif_bindings),
	FLAG_MANIPULATION_ENTRY("kunit:flag-manipulation-setf", "SETF",
		flag_setf_bindings),
	ADD_SUB_IMMEDIATE_ENTRY("kunit:add-sub-immediate-add", "ADD_addsub_imm",
				ADD_SUB_IMMEDIATE_BASE_OBLIGATIONS,
				base_add_sub_production_cases,
				add_sub_immediate_add_bindings),
	ADD_SUB_IMMEDIATE_ENTRY("kunit:add-sub-immediate-adds", "ADDS_addsub_imm",
				ADD_SUB_IMMEDIATE_FLAGS_OBLIGATIONS,
				base_add_sub_production_cases,
				add_sub_immediate_adds_bindings),
	ADD_SUB_IMMEDIATE_ENTRY("kunit:add-sub-immediate-sub", "SUB_addsub_imm",
				ADD_SUB_IMMEDIATE_BASE_OBLIGATIONS,
				base_add_sub_production_cases,
				add_sub_immediate_sub_bindings),
	ADD_SUB_IMMEDIATE_ENTRY("kunit:add-sub-immediate-subs", "SUBS_addsub_imm",
				ADD_SUB_IMMEDIATE_FLAGS_OBLIGATIONS,
				base_add_sub_production_cases,
				add_sub_immediate_subs_bindings),
};

#undef LOGICAL_ENTRY
#undef INTEGER_CONDITIONAL_ENTRY
#undef BASE_CONDITIONAL_ENTRY
#undef CSSC_DATA_ENTRY
#undef CSSC_ENTRY
#undef ADD_SUB_IMMEDIATE_ENTRY
#undef ADVSIMD_MUL_ENTRY
#undef CSSC_OBLIGATIONS
#undef ADD_SUB_IMMEDIATE_FLAGS_OBLIGATIONS
#undef ADD_SUB_IMMEDIATE_BASE_OBLIGATIONS

struct ordinary_load_store_registry_operation {
	const char *operation_id;
	const char *proof_id;
	size_t binding_offset;
	size_t binding_count;
};

static struct ordinary_load_store_registry_operation
	ordinary_load_store_registry_operations[] = {
	{ .operation_id = "LDRB_imm", .proof_id = "kunit:ordinary-load-store-ldrb-imm" },
	{ .operation_id = "LDRB_reg", .proof_id = "kunit:ordinary-load-store-ldrb-reg" },
	{ .operation_id = "LDRH_imm", .proof_id = "kunit:ordinary-load-store-ldrh-imm" },
	{ .operation_id = "LDRH_reg", .proof_id = "kunit:ordinary-load-store-ldrh-reg" },
	{ .operation_id = "LDRSB_imm", .proof_id = "kunit:ordinary-load-store-ldrsb-imm" },
	{ .operation_id = "LDRSB_reg", .proof_id = "kunit:ordinary-load-store-ldrsb-reg" },
	{ .operation_id = "LDRSH_imm", .proof_id = "kunit:ordinary-load-store-ldrsh-imm" },
	{ .operation_id = "LDRSH_reg", .proof_id = "kunit:ordinary-load-store-ldrsh-reg" },
	{ .operation_id = "LDRSW_imm", .proof_id = "kunit:ordinary-load-store-ldrsw-imm" },
	{ .operation_id = "LDRSW_reg", .proof_id = "kunit:ordinary-load-store-ldrsw-reg" },
	{ .operation_id = "LDR_imm_fpsimd", .proof_id = "kunit:ordinary-load-store-ldr-imm-fpsimd" },
	{ .operation_id = "LDR_imm_gen", .proof_id = "kunit:ordinary-load-store-ldr-imm-gen" },
	{ .operation_id = "LDR_reg_fpsimd", .proof_id = "kunit:ordinary-load-store-ldr-reg-fpsimd" },
	{ .operation_id = "LDR_reg_gen", .proof_id = "kunit:ordinary-load-store-ldr-reg-gen" },
	{ .operation_id = "LDTR", .proof_id = "kunit:ordinary-load-store-ldtr" },
	{ .operation_id = "LDTRB", .proof_id = "kunit:ordinary-load-store-ldtrb" },
	{ .operation_id = "LDTRH", .proof_id = "kunit:ordinary-load-store-ldtrh" },
	{ .operation_id = "LDTRSB", .proof_id = "kunit:ordinary-load-store-ldtrsb" },
	{ .operation_id = "LDTRSH", .proof_id = "kunit:ordinary-load-store-ldtrsh" },
	{ .operation_id = "LDTRSW", .proof_id = "kunit:ordinary-load-store-ldtrsw" },
	{ .operation_id = "LDURB", .proof_id = "kunit:ordinary-load-store-ldurb" },
	{ .operation_id = "LDURH", .proof_id = "kunit:ordinary-load-store-ldurh" },
	{ .operation_id = "LDURSB", .proof_id = "kunit:ordinary-load-store-ldursb" },
	{ .operation_id = "LDURSH", .proof_id = "kunit:ordinary-load-store-ldursh" },
	{ .operation_id = "LDURSW", .proof_id = "kunit:ordinary-load-store-ldursw" },
	{ .operation_id = "LDUR_fpsimd", .proof_id = "kunit:ordinary-load-store-ldur-fpsimd" },
	{ .operation_id = "LDUR_gen", .proof_id = "kunit:ordinary-load-store-ldur-gen" },
	{ .operation_id = "STRB_imm", .proof_id = "kunit:ordinary-load-store-strb-imm" },
	{ .operation_id = "STRB_reg", .proof_id = "kunit:ordinary-load-store-strb-reg" },
	{ .operation_id = "STRH_imm", .proof_id = "kunit:ordinary-load-store-strh-imm" },
	{ .operation_id = "STRH_reg", .proof_id = "kunit:ordinary-load-store-strh-reg" },
	{ .operation_id = "STR_imm_fpsimd", .proof_id = "kunit:ordinary-load-store-str-imm-fpsimd" },
	{ .operation_id = "STR_imm_gen", .proof_id = "kunit:ordinary-load-store-str-imm-gen" },
	{ .operation_id = "STR_reg_fpsimd", .proof_id = "kunit:ordinary-load-store-str-reg-fpsimd" },
	{ .operation_id = "STR_reg_gen", .proof_id = "kunit:ordinary-load-store-str-reg-gen" },
	{ .operation_id = "STTR", .proof_id = "kunit:ordinary-load-store-sttr" },
	{ .operation_id = "STTRB", .proof_id = "kunit:ordinary-load-store-sttrb" },
	{ .operation_id = "STTRH", .proof_id = "kunit:ordinary-load-store-sttrh" },
	{ .operation_id = "STURB", .proof_id = "kunit:ordinary-load-store-sturb" },
	{ .operation_id = "STURH", .proof_id = "kunit:ordinary-load-store-sturh" },
	{ .operation_id = "STUR_fpsimd", .proof_id = "kunit:ordinary-load-store-stur-fpsimd" },
	{ .operation_id = "STUR_gen", .proof_id = "kunit:ordinary-load-store-stur-gen" },
	{ .operation_id = "GCSSTR", .proof_id = "kunit:ordinary-load-store-gcsstr" },
	{ .operation_id = "GCSSTTR", .proof_id = "kunit:ordinary-load-store-gcssttr" },
	{ .operation_id = "LDNP_fpsimd", .proof_id = "kunit:ordinary-load-store-ldnp-fpsimd" },
	{ .operation_id = "LDNP_gen", .proof_id = "kunit:ordinary-load-store-ldnp-gen" },
	{ .operation_id = "LDPSW", .proof_id = "kunit:ordinary-load-store-ldpsw" },
	{ .operation_id = "LDP_fpsimd", .proof_id = "kunit:ordinary-load-store-ldp-fpsimd" },
	{ .operation_id = "LDP_gen", .proof_id = "kunit:ordinary-load-store-ldp-gen" },
	{ .operation_id = "LDRSW_lit", .proof_id = "kunit:ordinary-load-store-ldrsw-lit" },
	{ .operation_id = "LDR_lit_fpsimd", .proof_id = "kunit:ordinary-load-store-ldr-lit-fpsimd" },
	{ .operation_id = "LDR_lit_gen", .proof_id = "kunit:ordinary-load-store-ldr-lit-gen" },
	{ .operation_id = "LDTNP_fpsimd", .proof_id = "kunit:ordinary-load-store-ldtnp-fpsimd" },
	{ .operation_id = "LDTNP_gen", .proof_id = "kunit:ordinary-load-store-ldtnp-gen" },
	{ .operation_id = "LDTP_fpsimd", .proof_id = "kunit:ordinary-load-store-ldtp-fpsimd" },
	{ .operation_id = "LDTP_gen", .proof_id = "kunit:ordinary-load-store-ldtp-gen" },
	{ .operation_id = "PRFM_imm", .proof_id = "kunit:ordinary-load-store-prfm-imm" },
	{ .operation_id = "PRFM_lit", .proof_id = "kunit:ordinary-load-store-prfm-lit" },
	{ .operation_id = "PRFM_reg", .proof_id = "kunit:ordinary-load-store-prfm-reg" },
	{ .operation_id = "PRFUM", .proof_id = "kunit:ordinary-load-store-prfum" },
	{ .operation_id = "RPRFM_reg", .proof_id = "kunit:ordinary-load-store-rprfm-reg" },
	{ .operation_id = "STGP", .proof_id = "kunit:ordinary-load-store-stgp" },
	{ .operation_id = "STNP_fpsimd", .proof_id = "kunit:ordinary-load-store-stnp-fpsimd" },
	{ .operation_id = "STNP_gen", .proof_id = "kunit:ordinary-load-store-stnp-gen" },
	{ .operation_id = "STP_fpsimd", .proof_id = "kunit:ordinary-load-store-stp-fpsimd" },
	{ .operation_id = "STP_gen", .proof_id = "kunit:ordinary-load-store-stp-gen" },
	{ .operation_id = "STTNP_fpsimd", .proof_id = "kunit:ordinary-load-store-sttnp-fpsimd" },
	{ .operation_id = "STTNP_gen", .proof_id = "kunit:ordinary-load-store-sttnp-gen" },
	{ .operation_id = "STTP_fpsimd", .proof_id = "kunit:ordinary-load-store-sttp-fpsimd" },
	{ .operation_id = "STTP_gen", .proof_id = "kunit:ordinary-load-store-sttp-gen" },
};

static struct orlix_tcti_target_proof_binding ordinary_load_store_registry_bindings[
	ORDINARY_LOAD_STORE_PROOF_REGISTRY_BINDING_COUNT];
static bool ordinary_load_store_registry_ready;

struct advsimd_load_store_registry_operation {
	const char *operation_id;
	const char *proof_id;
	size_t binding_offset;
	size_t binding_count;
};

static struct advsimd_load_store_registry_operation
	advsimd_load_store_registry_operations[] = {
	{ .operation_id = "LD1R_advsimd", .proof_id = "kunit:advsimd-load-store-ld1r-advsimd" },
	{ .operation_id = "LD1_advsimd_mult", .proof_id = "kunit:advsimd-load-store-ld1-advsimd-mult" },
	{ .operation_id = "LD1_advsimd_sngl", .proof_id = "kunit:advsimd-load-store-ld1-advsimd-sngl" },
	{ .operation_id = "LD2R_advsimd", .proof_id = "kunit:advsimd-load-store-ld2r-advsimd" },
	{ .operation_id = "LD2_advsimd_mult", .proof_id = "kunit:advsimd-load-store-ld2-advsimd-mult" },
	{ .operation_id = "LD2_advsimd_sngl", .proof_id = "kunit:advsimd-load-store-ld2-advsimd-sngl" },
	{ .operation_id = "LD3R_advsimd", .proof_id = "kunit:advsimd-load-store-ld3r-advsimd" },
	{ .operation_id = "LD3_advsimd_mult", .proof_id = "kunit:advsimd-load-store-ld3-advsimd-mult" },
	{ .operation_id = "LD3_advsimd_sngl", .proof_id = "kunit:advsimd-load-store-ld3-advsimd-sngl" },
	{ .operation_id = "LD4R_advsimd", .proof_id = "kunit:advsimd-load-store-ld4r-advsimd" },
	{ .operation_id = "LD4_advsimd_mult", .proof_id = "kunit:advsimd-load-store-ld4-advsimd-mult" },
	{ .operation_id = "LD4_advsimd_sngl", .proof_id = "kunit:advsimd-load-store-ld4-advsimd-sngl" },
	{ .operation_id = "LDAP1_advsimd_sngl", .proof_id = "kunit:advsimd-load-store-ldap1-advsimd-sngl" },
	{ .operation_id = "ST1_advsimd_mult", .proof_id = "kunit:advsimd-load-store-st1-advsimd-mult" },
	{ .operation_id = "ST1_advsimd_sngl", .proof_id = "kunit:advsimd-load-store-st1-advsimd-sngl" },
	{ .operation_id = "ST2_advsimd_mult", .proof_id = "kunit:advsimd-load-store-st2-advsimd-mult" },
	{ .operation_id = "ST2_advsimd_sngl", .proof_id = "kunit:advsimd-load-store-st2-advsimd-sngl" },
	{ .operation_id = "ST3_advsimd_mult", .proof_id = "kunit:advsimd-load-store-st3-advsimd-mult" },
	{ .operation_id = "ST3_advsimd_sngl", .proof_id = "kunit:advsimd-load-store-st3-advsimd-sngl" },
	{ .operation_id = "ST4_advsimd_mult", .proof_id = "kunit:advsimd-load-store-st4-advsimd-mult" },
	{ .operation_id = "ST4_advsimd_sngl", .proof_id = "kunit:advsimd-load-store-st4-advsimd-sngl" },
	{ .operation_id = "STL1_advsimd_sngl", .proof_id = "kunit:advsimd-load-store-stl1-advsimd-sngl" },
};

static struct orlix_tcti_target_proof_binding advsimd_load_store_registry_bindings[
	ADVSIMD_LOAD_STORE_PROOF_REGISTRY_BINDING_COUNT];
static bool advsimd_load_store_registry_ready;

struct lse_registry_operation {
	const char *operation_id;
	const char *proof_id;
	const char *condition;
	const char *source;
	const char *suite;
	const struct orlix_tcti_target_proof_case *cases;
	size_t case_count;
	size_t expected_bindings;
	size_t binding_offset;
	size_t binding_count;
};

#define LSE_OPERATION(operation, slug, expected) \
	{ operation, "kunit:lse-base-" slug, LSE_CONDITION, \
	  BASE_ATOMIC_SOURCE, BASE_ATOMIC_SUITE, \
	  base_atomic_production_cases, ARRAY_COUNT(base_atomic_production_cases), expected, \
	  0, 0 }
#define LSE128_OPERATION(operation, slug) \
	{ operation, "kunit:lse128-" slug, LSE128_CONDITION, BASE_ATOMIC_SOURCE, \
	  BASE_ATOMIC_SUITE, base_atomic_production_cases, \
	  ARRAY_COUNT(base_atomic_production_cases), 4, 0, 0 }

static struct lse_registry_operation lse_registry_operations[] = {
	LSE_OPERATION("CASB", "casb", 4),
	LSE_OPERATION("CASH", "cash", 4),
	LSE_OPERATION("CAS", "cas", 8),
	LSE_OPERATION("CASP", "casp", 8),
	LSE_OPERATION("LDADDB", "ldaddb", 4),
	LSE_OPERATION("LDADDH", "ldaddh", 4),
	LSE_OPERATION("LDADD", "ldadd", 8),
	LSE_OPERATION("LDCLRB", "ldclrb", 4),
	LSE_OPERATION("LDCLRH", "ldclrh", 4),
	LSE_OPERATION("LDCLR", "ldclr", 8),
	LSE_OPERATION("LDEORB", "ldeorb", 4),
	LSE_OPERATION("LDEORH", "ldeorh", 4),
	LSE_OPERATION("LDEOR", "ldeor", 8),
	LSE_OPERATION("LDSETB", "ldsetb", 4),
	LSE_OPERATION("LDSETH", "ldseth", 4),
	LSE_OPERATION("LDSET", "ldset", 8),
	LSE_OPERATION("LDSMAXB", "ldsmaxb", 4),
	LSE_OPERATION("LDSMAXH", "ldsmaxh", 4),
	LSE_OPERATION("LDSMAX", "ldsmax", 8),
	LSE_OPERATION("LDSMINB", "ldsminb", 4),
	LSE_OPERATION("LDSMINH", "ldsminh", 4),
	LSE_OPERATION("LDSMIN", "ldsmin", 8),
	LSE_OPERATION("LDUMAXB", "ldumaxb", 4),
	LSE_OPERATION("LDUMAXH", "ldumaxh", 4),
	LSE_OPERATION("LDUMAX", "ldumax", 8),
	LSE_OPERATION("LDUMINB", "lduminb", 4),
	LSE_OPERATION("LDUMINH", "lduminh", 4),
	LSE_OPERATION("LDUMIN", "ldumin", 8),
	LSE_OPERATION("SWPB", "swpb", 4),
	LSE_OPERATION("SWPH", "swph", 4),
	LSE_OPERATION("SWP", "swp", 8),
	LSE128_OPERATION("LDCLRP", "ldclrp"),
	LSE128_OPERATION("LDSETP", "ldsetp"),
	LSE128_OPERATION("SWPP", "swpp"),
};

#undef LSE128_OPERATION
#undef LSE_OPERATION

static struct orlix_tcti_target_proof_binding lse_registry_bindings[
	LSE_PROOF_REGISTRY_BINDING_COUNT];
static bool lse_registry_ready;

static struct lse_registry_operation *
lse_registry_operation_for(const struct source_manifest_binding *source)
{
	size_t index;

	for (index = 0; index < ARRAY_COUNT(lse_registry_operations); index++)
		if (!strcmp(source->operation_id,
			    lse_registry_operations[index].operation_id) &&
		    !strcmp(source->condition_tcnd_hex,
			    lse_registry_operations[index].condition))
			return &lse_registry_operations[index];
	return NULL;
}

static bool build_lse_registry(void)
{
	size_t binding_offset = 0;
	size_t index;

	if (lse_registry_ready)
		return true;
	if (ARRAY_COUNT(lse_registry_operations) !=
	    LSE_PROOF_REGISTRY_ENTRY_COUNT)
		return false;
	for (index = 0; index < ARRAY_COUNT(source_manifest_bindings); index++) {
		struct lse_registry_operation *operation =
			lse_registry_operation_for(&source_manifest_bindings[index]);

		if (operation)
			operation->binding_count++;
	}
	for (index = 0; index < ARRAY_COUNT(lse_registry_operations); index++) {
		struct lse_registry_operation *operation =
			&lse_registry_operations[index];

		if (operation->binding_count != operation->expected_bindings)
			return false;
		operation->binding_offset = binding_offset;
		binding_offset += operation->binding_count;
		operation->binding_count = 0;
	}
	if (binding_offset != LSE_PROOF_REGISTRY_BINDING_COUNT)
		return false;
	for (index = 0; index < ARRAY_COUNT(source_manifest_bindings); index++) {
		const struct source_manifest_binding *source =
			&source_manifest_bindings[index];
		struct lse_registry_operation *operation =
			lse_registry_operation_for(source);
		struct orlix_tcti_target_proof_binding *binding;

		if (!operation)
			continue;
		binding = &lse_registry_bindings[
			operation->binding_offset + operation->binding_count++];
		binding->leaf_name = source->leaf_name;
		binding->mnemonic = source->mnemonic;
		binding->encoding_mask = source->encoding_mask;
		binding->encoding_pattern = source->encoding_pattern;
		binding->condition_tcnd_hex = source->condition_tcnd_hex;
		binding->kunit_case_mask =
			(ORLIX_TCTI_PROOF_U64_C(1) << operation->case_count) - 1;
		binding->source_ordinal = source->ordinal;
	}
	for (index = 0; index < ARRAY_COUNT(lse_registry_operations); index++) {
		struct lse_registry_operation *operation =
			&lse_registry_operations[index];
		struct orlix_tcti_target_proof_registry_entry *entry =
			&proof_registry_entries[
				CORE_PROOF_REGISTRY_ENTRY_COUNT + index];

		if (operation->binding_count != operation->expected_bindings)
			return false;
		*entry = (struct orlix_tcti_target_proof_registry_entry) {
			.id = operation->proof_id,
			.operation_id = operation->operation_id,
			.classification_mask =
				ORLIX_TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0,
			.obligations = LSE_REQUIRED_OBLIGATIONS,
			.linux_interface =
				ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE,
			.kunit_source = operation->source,
			.kunit_suite = operation->suite,
			.kunit_cases = operation->cases,
			.kunit_case_count = operation->case_count,
			.bindings =
				&lse_registry_bindings[operation->binding_offset],
			.binding_count = operation->binding_count,
			.kselftest = NULL,
			.unproved_obligations = LSE_REQUIRED_OBLIGATIONS,
		};
	}
	lse_registry_ready = true;
	return true;
}

/*
 * These source suites are registered as ownership evidence only. The complete
 * obligation mask remains unresolved until an execution result is imported.
 */
struct scalar_registry_operation {
	const char *operation_id;
	const char *proof_id;
	const char *source;
	const char *suite;
	const char *condition;
	const struct orlix_tcti_target_proof_case *cases;
	size_t case_count;
	orlix_tcti_proof_u32 minimum_ordinal;
	orlix_tcti_proof_u32 maximum_ordinal;
	size_t expected_bindings;
	size_t binding_offset;
	size_t binding_count;
};

static const struct orlix_tcti_target_proof_case logical_immediate_base_cases[] = {
	{ "orlix_tcti_logical_immediate_source_bindings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_logical_immediate_fixed_bit_neighbours",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ "orlix_tcti_logical_immediate_complete_decode_matrix",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_logical_immediate_production_path_semantics",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_logical_immediate_aliases_and_special_registers",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_logical_immediate_reserved_structured_exits",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
};

static const struct orlix_tcti_target_proof_case logical_immediate_flags_cases[] = {
	{ "orlix_tcti_logical_immediate_source_bindings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_logical_immediate_fixed_bit_neighbours",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ "orlix_tcti_logical_immediate_complete_decode_matrix",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_logical_immediate_production_path_semantics",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "orlix_tcti_logical_immediate_aliases_and_special_registers",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "orlix_tcti_logical_immediate_reserved_structured_exits",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
};

static const struct orlix_tcti_target_proof_case move_wide_cases[] = {
	{ "orlix_tcti_move_wide_source_bindings", ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_move_wide_complete_legal_decode_matrix",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_move_wide_all_immediates_decode",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_move_wide_production_path_semantics",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "orlix_tcti_move_wide_fixed_bit_neighbours_and_reserved",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
};

static const struct orlix_tcti_target_proof_case scalar_bitops_cases[]
	__attribute__((unused)) = {
	{ "orlix_tcti_scalar_bitops_source_bindings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
};

static const struct orlix_tcti_target_proof_case variable_shift_cases[] = {
	{ "orlix_tcti_variable_shift_source_bindings",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_variable_shift_production_path_semantics",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
};

static const struct orlix_tcti_target_proof_case add_sub_register_base_cases[]
	__attribute__((unused)) = {
	{ "asr_source_and_decode", ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "asr_resume_semantics", ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "asr_reserved_structured_exits",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
};

static const struct orlix_tcti_target_proof_case add_sub_register_flags_cases[]
	__attribute__((unused)) = {
	{ "asr_source_and_decode", ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "asr_resume_semantics", ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "asr_reserved_structured_exits",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
};

static const struct orlix_tcti_target_proof_case scalar_fp_convert_cases[]
	__attribute__((unused)) = {
	{ "orlix_tcti_scalar_fp_convert_resume_source_rows",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "orlix_tcti_scalar_fp_convert_reserved_forms_exit_without_state_change",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
};

#define SCALAR_OPERATION(operation, proof, source_file, source_suite, case_set, \
			 minimum, maximum, expected) \
	{ operation, proof, source_file, source_suite, SCALAR_CONDITION, case_set, \
	  ARRAY_COUNT(case_set), minimum, maximum, expected, 0, 0 }
#define SCALAR_FP_OPERATION(operation, proof, minimum, maximum, expected) \
	{ operation, proof, SCALAR_FP_FAMILY_SOURCE, SCALAR_FP_FAMILY_SUITE, \
	  SCALAR_FP_CONDITION, scalar_fp_family_cases, \
	  ARRAY_COUNT(scalar_fp_family_cases), minimum, maximum, expected, 0, 0 }
#define CRYPTO_OPERATION(operation, proof, condition, minimum) \
	{ operation, proof, ADVSIMD_CRYPTO_SOURCE, ADVSIMD_CRYPTO_SUITE, \
	  condition, advsimd_crypto_production_cases, \
	  ARRAY_COUNT(advsimd_crypto_production_cases), minimum, minimum, 1, 0, 0 }
#define PERMUTE_MOVE_SIMD_OPERATION(operation, proof, minimum, maximum, expected) \
	{ operation, proof, ADVSIMD_PERMUTE_MOVE_SOURCE, ADVSIMD_PERMUTE_MOVE_SUITE, \
	  ADVSIMD_CONDITION, advsimd_permute_move_simd_cases, \
	  ARRAY_COUNT(advsimd_permute_move_simd_cases), minimum, maximum, expected, 0, 0 }
#define PERMUTE_MOVE_GPR_OPERATION(operation, proof, minimum, maximum, expected) \
	{ operation, proof, ADVSIMD_PERMUTE_MOVE_SOURCE, ADVSIMD_PERMUTE_MOVE_SUITE, \
	  ADVSIMD_CONDITION, advsimd_permute_move_gpr_cases, \
	  ARRAY_COUNT(advsimd_permute_move_gpr_cases), minimum, maximum, expected, 0, 0 }
#define INTEGER_SIMD_OPERATION(operation, proof, minimum, maximum, expected) \
	{ operation, proof, ADVSIMD_INTEGER_SOURCE, ADVSIMD_INTEGER_SUITE, \
	  "", advsimd_integer_simd_cases, \
	  ARRAY_COUNT(advsimd_integer_simd_cases), minimum, maximum, expected, 0, 0 }
#define FP_SIMD_OPERATION(operation, proof, minimum, maximum, expected) \
	{ operation, proof, ADVSIMD_FP_SOURCE, ADVSIMD_FP_SUITE, \
	  ADVSIMD_CONDITION, advsimd_fp_simd_cases, \
	  ARRAY_COUNT(advsimd_fp_simd_cases), minimum, maximum, expected, 0, 0 }
#define FP_SIMD_SHIFT_OPERATION(operation, proof, minimum, maximum, expected) \
	{ operation, proof, ADVSIMD_FP_SOURCE, ADVSIMD_FP_SUITE, \
	  "", advsimd_fp_simd_cases, \
	  ARRAY_COUNT(advsimd_fp_simd_cases), minimum, maximum, expected, 0, 0 }

static struct scalar_registry_operation scalar_registry_operations[] = {
	SCALAR_OPERATION("AND_log_imm", "kunit:logical-immediate-and",
		LOGICAL_IMMEDIATE_SOURCE, LOGICAL_IMMEDIATE_SUITE,
		logical_immediate_base_cases, 2191U, 2198U, 2),
	SCALAR_OPERATION("ORR_log_imm", "kunit:logical-immediate-orr",
		LOGICAL_IMMEDIATE_SOURCE, LOGICAL_IMMEDIATE_SUITE,
		logical_immediate_base_cases, 2191U, 2198U, 2),
	SCALAR_OPERATION("EOR_log_imm", "kunit:logical-immediate-eor",
		LOGICAL_IMMEDIATE_SOURCE, LOGICAL_IMMEDIATE_SUITE,
		logical_immediate_base_cases, 2191U, 2198U, 2),
	SCALAR_OPERATION("ANDS_log_imm", "kunit:logical-immediate-ands",
		LOGICAL_IMMEDIATE_SOURCE, LOGICAL_IMMEDIATE_SUITE,
		logical_immediate_flags_cases, 2191U, 2198U, 2),
	SCALAR_OPERATION("MOVN", "kunit:move-wide-movn", MOVE_WIDE_SOURCE,
		MOVE_WIDE_SUITE, move_wide_cases, 2199U, 2204U, 2),
	SCALAR_OPERATION("MOVZ", "kunit:move-wide-movz", MOVE_WIDE_SOURCE,
		MOVE_WIDE_SUITE, move_wide_cases, 2199U, 2204U, 2),
	SCALAR_OPERATION("MOVK", "kunit:move-wide-movk", MOVE_WIDE_SOURCE,
		MOVE_WIDE_SUITE, move_wide_cases, 2199U, 2204U, 2),
	{ "EXTR", "kunit:bitfield-unary-extr", BASE_BITFIELD_UNARY_SOURCE,
	  BASE_BITFIELD_UNARY_SUITE, EXTR_CONDITION, base_bitfield_unary_production_cases,
	  ARRAY_COUNT(base_bitfield_unary_production_cases), 2169U, 2170U, 2, 0, 0 },
	SCALAR_OPERATION("SBFM", "kunit:bitfield-unary-sbfm",
		BASE_BITFIELD_UNARY_SOURCE, BASE_BITFIELD_UNARY_SUITE,
		base_bitfield_unary_production_cases, 2205U, 2208U, 2),
	SCALAR_OPERATION("BFM", "kunit:bitfield-unary-bfm",
		BASE_BITFIELD_UNARY_SOURCE, BASE_BITFIELD_UNARY_SUITE,
		base_bitfield_unary_production_cases, 2206U, 2209U, 2),
	SCALAR_OPERATION("UBFM", "kunit:bitfield-unary-ubfm",
		BASE_BITFIELD_UNARY_SOURCE, BASE_BITFIELD_UNARY_SUITE,
		base_bitfield_unary_production_cases, 2207U, 2210U, 2),
	SCALAR_OPERATION("RBIT_int", "kunit:scalar-bitops-rbit",
		BASE_BITFIELD_UNARY_SOURCE, BASE_BITFIELD_UNARY_SUITE,
		base_bitfield_unary_production_cases, 3389U, 3397U, 2),
	SCALAR_OPERATION("REV16_int", "kunit:scalar-bitops-rev16",
		BASE_BITFIELD_UNARY_SOURCE, BASE_BITFIELD_UNARY_SUITE,
		base_bitfield_unary_production_cases, 3390U, 3398U, 2),
	SCALAR_OPERATION("REV", "kunit:scalar-bitops-rev",
		BASE_BITFIELD_UNARY_SOURCE, BASE_BITFIELD_UNARY_SUITE,
		base_bitfield_unary_production_cases, 3391U, 3400U, 2),
	SCALAR_OPERATION("CLZ_int", "kunit:scalar-bitops-clz",
		BASE_BITFIELD_UNARY_SOURCE, BASE_BITFIELD_UNARY_SUITE,
		base_bitfield_unary_production_cases, 3392U, 3401U, 2),
	SCALAR_OPERATION("CLS_int", "kunit:scalar-bitops-cls",
		BASE_BITFIELD_UNARY_SOURCE, BASE_BITFIELD_UNARY_SUITE,
		base_bitfield_unary_production_cases, 3393U, 3402U, 2),
	SCALAR_OPERATION("REV32_int", "kunit:scalar-bitops-rev32",
		BASE_BITFIELD_UNARY_SOURCE, BASE_BITFIELD_UNARY_SUITE,
		base_bitfield_unary_production_cases, 3399U, 3399U, 1),
	SCALAR_OPERATION("LSLV", "kunit:variable-shift-lslv",
		VARIABLE_SHIFT_SOURCE, VARIABLE_SHIFT_SUITE,
		variable_shift_cases, 3358U, 3377U, 2),
	SCALAR_OPERATION("LSRV", "kunit:variable-shift-lsrv",
		VARIABLE_SHIFT_SOURCE, VARIABLE_SHIFT_SUITE,
		variable_shift_cases, 3359U, 3378U, 2),
	SCALAR_OPERATION("ASRV", "kunit:variable-shift-asrv",
		VARIABLE_SHIFT_SOURCE, VARIABLE_SHIFT_SUITE,
		variable_shift_cases, 3360U, 3379U, 2),
	SCALAR_OPERATION("RORV", "kunit:variable-shift-rorv",
		VARIABLE_SHIFT_SOURCE, VARIABLE_SHIFT_SUITE,
		variable_shift_cases, 3361U, 3380U, 2),
	SCALAR_OPERATION("ADD_addsub_shift", "kunit:add-sub-register-add-shift",
		BASE_ADD_SUB_SOURCE, BASE_ADD_SUB_SUITE,
		base_add_sub_production_cases, 3450U, 3457U, 2),
	SCALAR_OPERATION("ADDS_addsub_shift", "kunit:add-sub-register-adds-shift",
		BASE_ADD_SUB_SOURCE, BASE_ADD_SUB_SUITE,
		base_add_sub_production_cases, 3450U, 3457U, 2),
	SCALAR_OPERATION("SUB_addsub_shift", "kunit:add-sub-register-sub-shift",
		BASE_ADD_SUB_SOURCE, BASE_ADD_SUB_SUITE,
		base_add_sub_production_cases, 3450U, 3457U, 2),
	SCALAR_OPERATION("SUBS_addsub_shift", "kunit:add-sub-register-subs-shift",
		BASE_ADD_SUB_SOURCE, BASE_ADD_SUB_SUITE,
		base_add_sub_production_cases, 3450U, 3457U, 2),
	SCALAR_OPERATION("ADD_addsub_ext", "kunit:add-sub-register-add-ext",
		BASE_ADD_SUB_SOURCE, BASE_ADD_SUB_SUITE,
		base_add_sub_production_cases, 3458U, 3465U, 2),
	SCALAR_OPERATION("ADDS_addsub_ext", "kunit:add-sub-register-adds-ext",
		BASE_ADD_SUB_SOURCE, BASE_ADD_SUB_SUITE,
		base_add_sub_production_cases, 3458U, 3465U, 2),
	SCALAR_OPERATION("SUB_addsub_ext", "kunit:add-sub-register-sub-ext",
		BASE_ADD_SUB_SOURCE, BASE_ADD_SUB_SUITE,
		base_add_sub_production_cases, 3458U, 3465U, 2),
	SCALAR_OPERATION("SUBS_addsub_ext", "kunit:add-sub-register-subs-ext",
		BASE_ADD_SUB_SOURCE, BASE_ADD_SUB_SUITE,
		base_add_sub_production_cases, 3458U, 3465U, 2),
	SCALAR_OPERATION("ADC", "kunit:add-sub-register-adc",
		BASE_ADD_SUB_SOURCE, BASE_ADD_SUB_SUITE,
		base_add_sub_production_cases, 3466U, 3473U, 2),
	SCALAR_OPERATION("ADCS", "kunit:add-sub-register-adcs",
		BASE_ADD_SUB_SOURCE, BASE_ADD_SUB_SUITE,
		base_add_sub_production_cases, 3466U, 3473U, 2),
	SCALAR_OPERATION("SBC", "kunit:add-sub-register-sbc",
		BASE_ADD_SUB_SOURCE, BASE_ADD_SUB_SUITE,
		base_add_sub_production_cases, 3466U, 3473U, 2),
	SCALAR_OPERATION("SBCS", "kunit:add-sub-register-sbcs",
		BASE_ADD_SUB_SOURCE, BASE_ADD_SUB_SUITE,
		base_add_sub_production_cases, 3466U, 3473U, 2),
	{ "ADDPT", "kunit:base-add-sub-addpt", BASE_ADD_SUB_SOURCE,
	  BASE_ADD_SUB_SUITE, ADD_SUB_PT_CONDITION, base_add_sub_production_cases,
	  ARRAY_COUNT(base_add_sub_production_cases), 3474U, 3474U, 1, 0, 0 },
	{ "SUBPT", "kunit:base-add-sub-subpt", BASE_ADD_SUB_SOURCE,
	  BASE_ADD_SUB_SUITE, ADD_SUB_PT_CONDITION, base_add_sub_production_cases,
	  ARRAY_COUNT(base_add_sub_production_cases), 3475U, 3475U, 1, 0, 0 },
	{ "MADDPT", "kunit:base-multiply-divide-maddpt", BASE_MULTIPLY_DIVIDE_SOURCE,
	  BASE_MULTIPLY_DIVIDE_SUITE, ADD_SUB_PT_CONDITION,
	  base_multiply_divide_production_cases,
	  ARRAY_COUNT(base_multiply_divide_production_cases), 3502U, 3502U, 1, 0, 0 },
	{ "MSUBPT", "kunit:base-multiply-divide-msubpt", BASE_MULTIPLY_DIVIDE_SOURCE,
	  BASE_MULTIPLY_DIVIDE_SUITE, ADD_SUB_PT_CONDITION,
	  base_multiply_divide_production_cases,
	  ARRAY_COUNT(base_multiply_divide_production_cases), 3503U, 3503U, 1, 0, 0 },
	SCALAR_OPERATION("BR", "kunit:branch-control-br",
		BASE_CONTROL_FLOW_SOURCE, BASE_CONTROL_FLOW_SUITE,
		base_control_flow_production_cases, 2288U, 2288U, 1),
	SCALAR_OPERATION("BLR", "kunit:branch-control-blr",
		BASE_CONTROL_FLOW_SOURCE, BASE_CONTROL_FLOW_SUITE,
		base_control_flow_production_cases, 2291U, 2291U, 1),
	SCALAR_OPERATION("RET", "kunit:branch-control-ret",
		BASE_CONTROL_FLOW_SOURCE, BASE_CONTROL_FLOW_SUITE,
		base_control_flow_production_cases, 2294U, 2294U, 1),
	SCALAR_OPERATION("B_uncond", "kunit:branch-control-b-uncond",
		BASE_CONTROL_FLOW_SOURCE, BASE_CONTROL_FLOW_SUITE,
		base_control_flow_production_cases, 2312U, 2312U, 1),
	SCALAR_OPERATION("BL", "kunit:branch-control-bl",
		BASE_CONTROL_FLOW_SOURCE, BASE_CONTROL_FLOW_SUITE,
		base_control_flow_production_cases, 2313U, 2313U, 1),
	SCALAR_OPERATION("SVC", "kunit:branch-control-svc",
		BASE_EXCEPTIONS_SOURCE, BASE_EXCEPTIONS_SUITE,
		base_exceptions_production_cases, 2227U, 2227U, 1),
	SCALAR_OPERATION("BRK", "kunit:branch-control-brk",
		BASE_EXCEPTIONS_SOURCE, BASE_EXCEPTIONS_SUITE,
		base_exceptions_production_cases, 2230U, 2230U, 1),
	SCALAR_OPERATION("HLT", "kunit:branch-control-hlt",
		BASE_EXCEPTIONS_SOURCE, BASE_EXCEPTIONS_SUITE,
		base_exceptions_production_cases, 2231U, 2231U, 1),
	SCALAR_OPERATION("B_cond", "kunit:branch-control-b-cond",
		BASE_CONDITIONAL_SOURCE, BASE_CONDITIONAL_SUITE,
		base_conditional_production_cases, 2211U, 2211U, 1),
	SCALAR_OPERATION("CBZ", "kunit:branch-control-cbz",
		BASE_CONDITIONAL_SOURCE, BASE_CONDITIONAL_SUITE,
		base_conditional_production_cases, 2314U, 2317U, 2),
	SCALAR_OPERATION("CBNZ", "kunit:branch-control-cbnz",
		BASE_CONDITIONAL_SOURCE, BASE_CONDITIONAL_SUITE,
		base_conditional_production_cases, 2314U, 2317U, 2),
	SCALAR_OPERATION("TBZ", "kunit:branch-control-tbz",
		BASE_CONDITIONAL_SOURCE, BASE_CONDITIONAL_SUITE,
		base_conditional_production_cases, 2342U, 2342U, 1),
	SCALAR_OPERATION("TBNZ", "kunit:branch-control-tbnz",
		BASE_CONDITIONAL_SOURCE, BASE_CONDITIONAL_SUITE,
		base_conditional_production_cases, 2343U, 2343U, 1),
	CRYPTO_OPERATION("AESE_advsimd", "kunit:advsimd-crypto-aese",
		AES_CONDITION, 3507U),
	CRYPTO_OPERATION("AESD_advsimd", "kunit:advsimd-crypto-aesd",
		AES_CONDITION, 3508U),
	CRYPTO_OPERATION("AESMC_advsimd", "kunit:advsimd-crypto-aesmc",
		AES_CONDITION, 3509U),
	CRYPTO_OPERATION("AESIMC_advsimd", "kunit:advsimd-crypto-aesimc",
		AES_CONDITION, 3510U),
	CRYPTO_OPERATION("SHA1C_advsimd", "kunit:advsimd-crypto-sha1c",
		SHA1_CONDITION, 3511U),
	CRYPTO_OPERATION("SHA1P_advsimd", "kunit:advsimd-crypto-sha1p",
		SHA1_CONDITION, 3512U),
	CRYPTO_OPERATION("SHA1M_advsimd", "kunit:advsimd-crypto-sha1m",
		SHA1_CONDITION, 3513U),
	CRYPTO_OPERATION("SHA1SU0_advsimd", "kunit:advsimd-crypto-sha1su0",
		SHA1_CONDITION, 3514U),
	CRYPTO_OPERATION("SHA256H_advsimd", "kunit:advsimd-crypto-sha256h",
		SHA256_CONDITION, 3515U),
	CRYPTO_OPERATION("SHA256H2_advsimd", "kunit:advsimd-crypto-sha256h2",
		SHA256_CONDITION, 3516U),
	CRYPTO_OPERATION("SHA256SU1_advsimd", "kunit:advsimd-crypto-sha256su1",
		SHA256_CONDITION, 3517U),
	CRYPTO_OPERATION("SHA1H_advsimd", "kunit:advsimd-crypto-sha1h",
		SHA1_CONDITION, 3518U),
	CRYPTO_OPERATION("SHA1SU1_advsimd", "kunit:advsimd-crypto-sha1su1",
		SHA1_CONDITION, 3519U),
	CRYPTO_OPERATION("SHA256SU0_advsimd", "kunit:advsimd-crypto-sha256su0",
		SHA256_CONDITION, 3520U),
	CRYPTO_OPERATION("PMULL_advsimd", "kunit:advsimd-crypto-pmull",
		ADVSIMD_CONDITION, 3883U),
	CRYPTO_OPERATION("SM3TT1A_advsimd", "kunit:advsimd-crypto-sm3tt1a",
		SM3_CONDITION, 4067U),
	CRYPTO_OPERATION("SM3TT1B_advsimd", "kunit:advsimd-crypto-sm3tt1b",
		SM3_CONDITION, 4068U),
	CRYPTO_OPERATION("SM3TT2A_advsimd", "kunit:advsimd-crypto-sm3tt2a",
		SM3_CONDITION, 4069U),
	CRYPTO_OPERATION("SM3TT2B_advsimd", "kunit:advsimd-crypto-sm3tt2b",
		SM3_CONDITION, 4070U),
	CRYPTO_OPERATION("SHA512H_advsimd", "kunit:advsimd-crypto-sha512h",
		SHA512_CONDITION, 4071U),
	CRYPTO_OPERATION("SHA512H2_advsimd", "kunit:advsimd-crypto-sha512h2",
		SHA512_CONDITION, 4072U),
	CRYPTO_OPERATION("SHA512SU1_advsimd", "kunit:advsimd-crypto-sha512su1",
		SHA512_CONDITION, 4073U),
	CRYPTO_OPERATION("RAX1_advsimd", "kunit:advsimd-crypto-rax1",
		SHA3_CONDITION, 4074U),
	CRYPTO_OPERATION("SM3PARTW1_advsimd", "kunit:advsimd-crypto-sm3partw1",
		SM3_CONDITION, 4075U),
	CRYPTO_OPERATION("SM3PARTW2_advsimd", "kunit:advsimd-crypto-sm3partw2",
		SM3_CONDITION, 4076U),
	CRYPTO_OPERATION("SM4EKEY_advsimd", "kunit:advsimd-crypto-sm4ekey",
		SM4_CONDITION, 4077U),
	CRYPTO_OPERATION("EOR3_advsimd", "kunit:advsimd-crypto-eor3",
		SHA3_CONDITION, 4078U),
	CRYPTO_OPERATION("BCAX_advsimd", "kunit:advsimd-crypto-bcax",
		SHA3_CONDITION, 4079U),
	CRYPTO_OPERATION("SM3SS1_advsimd", "kunit:advsimd-crypto-sm3ss1",
		SM3_CONDITION, 4080U),
	CRYPTO_OPERATION("XAR_advsimd", "kunit:advsimd-crypto-xar",
		SHA3_CONDITION, 4081U),
	CRYPTO_OPERATION("SHA512SU0_advsimd", "kunit:advsimd-crypto-sha512su0",
		SHA512_CONDITION, 4082U),
	CRYPTO_OPERATION("SM4E_advsimd", "kunit:advsimd-crypto-sm4e",
		SM4_CONDITION, 4083U),
	PERMUTE_MOVE_SIMD_OPERATION("DUP_advsimd_elt",
		"kunit:advsimd-permute-move-dup-elt", 3521U, 3691U, 2),
	PERMUTE_MOVE_SIMD_OPERATION("TBL_advsimd",
		"kunit:advsimd-permute-move-tbl", 3672U, 3678U, 4),
	PERMUTE_MOVE_SIMD_OPERATION("TBX_advsimd",
		"kunit:advsimd-permute-move-tbx", 3673U, 3679U, 4),
	PERMUTE_MOVE_SIMD_OPERATION("UZP1_advsimd",
		"kunit:advsimd-permute-move-uzp1", 3684U, 3684U, 1),
	PERMUTE_MOVE_SIMD_OPERATION("TRN1_advsimd",
		"kunit:advsimd-permute-move-trn1", 3685U, 3685U, 1),
	PERMUTE_MOVE_SIMD_OPERATION("ZIP1_advsimd",
		"kunit:advsimd-permute-move-zip1", 3686U, 3686U, 1),
	PERMUTE_MOVE_SIMD_OPERATION("UZP2_advsimd",
		"kunit:advsimd-permute-move-uzp2", 3687U, 3687U, 1),
	PERMUTE_MOVE_SIMD_OPERATION("TRN2_advsimd",
		"kunit:advsimd-permute-move-trn2", 3688U, 3688U, 1),
	PERMUTE_MOVE_SIMD_OPERATION("ZIP2_advsimd",
		"kunit:advsimd-permute-move-zip2", 3689U, 3689U, 1),
	PERMUTE_MOVE_SIMD_OPERATION("EXT_advsimd",
		"kunit:advsimd-permute-move-ext", 3690U, 3690U, 1),
	PERMUTE_MOVE_SIMD_OPERATION("DUP_advsimd_gen",
		"kunit:advsimd-permute-move-dup-gen", 3692U, 3692U, 1),
	PERMUTE_MOVE_GPR_OPERATION("SMOV_advsimd",
		"kunit:advsimd-permute-move-smov", 3693U, 3696U, 2),
	PERMUTE_MOVE_GPR_OPERATION("UMOV_advsimd",
		"kunit:advsimd-permute-move-umov", 3694U, 3697U, 2),
	PERMUTE_MOVE_SIMD_OPERATION("INS_advsimd_gen",
		"kunit:advsimd-permute-move-ins-gen", 3695U, 3695U, 1),
	PERMUTE_MOVE_SIMD_OPERATION("INS_advsimd_elt",
		"kunit:advsimd-permute-move-ins-elt", 3698U, 3698U, 1),
	PERMUTE_MOVE_SIMD_OPERATION("REV64_advsimd",
		"kunit:advsimd-permute-move-rev64", 3783U, 3783U, 1),
	PERMUTE_MOVE_SIMD_OPERATION("REV16_advsimd",
		"kunit:advsimd-permute-move-rev16", 3784U, 3784U, 1),
	PERMUTE_MOVE_SIMD_OPERATION("REV32_advsimd",
		"kunit:advsimd-permute-move-rev32", 3818U, 3818U, 1),
	PERMUTE_MOVE_SIMD_OPERATION("MOVI_advsimd",
		"kunit:advsimd-permute-move-movi", 3979U, 3993U, 6),
	INTEGER_SIMD_OPERATION("SUQADD_advsimd",
		"kunit:advsimd-integer-suqadd-advsimd", 3553U, 3786U, 2),
	INTEGER_SIMD_OPERATION("SQABS_advsimd",
		"kunit:advsimd-integer-sqabs-advsimd", 3554U, 3790U, 2),
	INTEGER_SIMD_OPERATION("CMGT_advsimd_zero",
		"kunit:advsimd-integer-cmgt-advsimd-zero", 3555U, 3791U, 2),
	INTEGER_SIMD_OPERATION("CMEQ_advsimd_zero",
		"kunit:advsimd-integer-cmeq-advsimd-zero", 3556U, 3792U, 2),
	INTEGER_SIMD_OPERATION("CMLT_advsimd",
		"kunit:advsimd-integer-cmlt-advsimd", 3557U, 3793U, 2),
	INTEGER_SIMD_OPERATION("ABS_advsimd",
		"kunit:advsimd-integer-abs-advsimd", 3558U, 3794U, 2),
	INTEGER_SIMD_OPERATION("SQXTN_advsimd",
		"kunit:advsimd-integer-sqxtn-advsimd", 3559U, 3796U, 2),
	INTEGER_SIMD_OPERATION("USQADD_advsimd",
		"kunit:advsimd-integer-usqadd-advsimd", 3571U, 3820U, 2),
	INTEGER_SIMD_OPERATION("SQNEG_advsimd",
		"kunit:advsimd-integer-sqneg-advsimd", 3572U, 3823U, 2),
	INTEGER_SIMD_OPERATION("CMGE_advsimd_zero",
		"kunit:advsimd-integer-cmge-advsimd-zero", 3573U, 3824U, 2),
	INTEGER_SIMD_OPERATION("CMLE_advsimd",
		"kunit:advsimd-integer-cmle-advsimd", 3574U, 3825U, 2),
	INTEGER_SIMD_OPERATION("NEG_advsimd",
		"kunit:advsimd-integer-neg-advsimd", 3575U, 3826U, 2),
	INTEGER_SIMD_OPERATION("SQXTUN_advsimd",
		"kunit:advsimd-integer-sqxtun-advsimd", 3576U, 3827U, 2),
	INTEGER_SIMD_OPERATION("UQXTN_advsimd",
		"kunit:advsimd-integer-uqxtn-advsimd", 3577U, 3829U, 2),
	INTEGER_SIMD_OPERATION("ADDP_advsimd_pair",
		"kunit:advsimd-integer-addp-advsimd-pair", 3588U, 3588U, 1),
	INTEGER_SIMD_OPERATION("SQDMLAL_advsimd_vec",
		"kunit:advsimd-integer-sqdmlal-advsimd-vec", 3599U, 3878U, 2),
	INTEGER_SIMD_OPERATION("SQDMLSL_advsimd_vec",
		"kunit:advsimd-integer-sqdmlsl-advsimd-vec", 3600U, 3880U, 2),
	INTEGER_SIMD_OPERATION("SQDMULL_advsimd_vec",
		"kunit:advsimd-integer-sqdmull-advsimd-vec", 3601U, 3882U, 2),
	INTEGER_SIMD_OPERATION("SQADD_advsimd",
		"kunit:advsimd-integer-sqadd-advsimd", 3602U, 3896U, 2),
	INTEGER_SIMD_OPERATION("SQSUB_advsimd",
		"kunit:advsimd-integer-sqsub-advsimd", 3603U, 3899U, 2),
	INTEGER_SIMD_OPERATION("CMGT_advsimd_reg",
		"kunit:advsimd-integer-cmgt-advsimd-reg", 3604U, 3900U, 2),
	INTEGER_SIMD_OPERATION("CMGE_advsimd_reg",
		"kunit:advsimd-integer-cmge-advsimd-reg", 3605U, 3901U, 2),
	INTEGER_SIMD_OPERATION("SSHL_advsimd",
		"kunit:advsimd-integer-sshl-advsimd", 3606U, 3902U, 2),
	INTEGER_SIMD_OPERATION("SQSHL_advsimd_reg",
		"kunit:advsimd-integer-sqshl-advsimd-reg", 3607U, 3903U, 2),
	INTEGER_SIMD_OPERATION("SRSHL_advsimd",
		"kunit:advsimd-integer-srshl-advsimd", 3608U, 3904U, 2),
	INTEGER_SIMD_OPERATION("SQRSHL_advsimd",
		"kunit:advsimd-integer-sqrshl-advsimd", 3609U, 3905U, 2),
	INTEGER_SIMD_OPERATION("ADD_advsimd",
		"kunit:advsimd-integer-add-advsimd", 3610U, 3910U, 2),
	INTEGER_SIMD_OPERATION("CMTST_advsimd",
		"kunit:advsimd-integer-cmtst-advsimd", 3611U, 3911U, 2),
	INTEGER_SIMD_OPERATION("SQDMULH_advsimd_vec",
		"kunit:advsimd-integer-sqdmulh-advsimd-vec", 3612U, 3916U, 2),
	INTEGER_SIMD_OPERATION("UQADD_advsimd",
		"kunit:advsimd-integer-uqadd-advsimd", 3617U, 3938U, 2),
	INTEGER_SIMD_OPERATION("UQSUB_advsimd",
		"kunit:advsimd-integer-uqsub-advsimd", 3618U, 3941U, 2),
	INTEGER_SIMD_OPERATION("CMHI_advsimd",
		"kunit:advsimd-integer-cmhi-advsimd", 3619U, 3942U, 2),
	INTEGER_SIMD_OPERATION("CMHS_advsimd",
		"kunit:advsimd-integer-cmhs-advsimd", 3620U, 3943U, 2),
	INTEGER_SIMD_OPERATION("USHL_advsimd",
		"kunit:advsimd-integer-ushl-advsimd", 3621U, 3944U, 2),
	INTEGER_SIMD_OPERATION("UQSHL_advsimd_reg",
		"kunit:advsimd-integer-uqshl-advsimd-reg", 3622U, 3945U, 2),
	INTEGER_SIMD_OPERATION("URSHL_advsimd",
		"kunit:advsimd-integer-urshl-advsimd", 3623U, 3946U, 2),
	INTEGER_SIMD_OPERATION("UQRSHL_advsimd",
		"kunit:advsimd-integer-uqrshl-advsimd", 3624U, 3947U, 2),
	INTEGER_SIMD_OPERATION("SUB_advsimd",
		"kunit:advsimd-integer-sub-advsimd", 3625U, 3952U, 2),
	INTEGER_SIMD_OPERATION("CMEQ_advsimd_reg",
		"kunit:advsimd-integer-cmeq-advsimd-reg", 3626U, 3953U, 2),
	INTEGER_SIMD_OPERATION("SQRDMULH_advsimd_vec",
		"kunit:advsimd-integer-sqrdmulh-advsimd-vec", 3627U, 3958U, 2),
	INTEGER_SIMD_OPERATION("SSHR_advsimd",
		"kunit:advsimd-integer-sshr-advsimd", 3633U, 3995U, 2),
	INTEGER_SIMD_OPERATION("SSRA_advsimd",
		"kunit:advsimd-integer-ssra-advsimd", 3634U, 3996U, 2),
	INTEGER_SIMD_OPERATION("SRSHR_advsimd",
		"kunit:advsimd-integer-srshr-advsimd", 3635U, 3997U, 2),
	INTEGER_SIMD_OPERATION("SRSRA_advsimd",
		"kunit:advsimd-integer-srsra-advsimd", 3636U, 3998U, 2),
	INTEGER_SIMD_OPERATION("SHL_advsimd",
		"kunit:advsimd-integer-shl-advsimd", 3637U, 3999U, 2),
	INTEGER_SIMD_OPERATION("SQSHL_advsimd_imm",
		"kunit:advsimd-integer-sqshl-advsimd-imm", 3638U, 4000U, 2),
	INTEGER_SIMD_OPERATION("SQSHRN_advsimd",
		"kunit:advsimd-integer-sqshrn-advsimd", 3639U, 4003U, 2),
	INTEGER_SIMD_OPERATION("SQRSHRN_advsimd",
		"kunit:advsimd-integer-sqrshrn-advsimd", 3640U, 4004U, 2),
	INTEGER_SIMD_OPERATION("USHR_advsimd",
		"kunit:advsimd-integer-ushr-advsimd", 3643U, 4008U, 2),
	INTEGER_SIMD_OPERATION("USRA_advsimd",
		"kunit:advsimd-integer-usra-advsimd", 3644U, 4009U, 2),
	INTEGER_SIMD_OPERATION("URSHR_advsimd",
		"kunit:advsimd-integer-urshr-advsimd", 3645U, 4010U, 2),
	INTEGER_SIMD_OPERATION("URSRA_advsimd",
		"kunit:advsimd-integer-ursra-advsimd", 3646U, 4011U, 2),
	INTEGER_SIMD_OPERATION("SRI_advsimd",
		"kunit:advsimd-integer-sri-advsimd", 3647U, 4012U, 2),
	INTEGER_SIMD_OPERATION("SLI_advsimd",
		"kunit:advsimd-integer-sli-advsimd", 3648U, 4013U, 2),
	INTEGER_SIMD_OPERATION("SQSHLU_advsimd",
		"kunit:advsimd-integer-sqshlu-advsimd", 3649U, 4014U, 2),
	INTEGER_SIMD_OPERATION("UQSHL_advsimd_imm",
		"kunit:advsimd-integer-uqshl-advsimd-imm", 3650U, 4015U, 2),
	INTEGER_SIMD_OPERATION("SQSHRUN_advsimd",
		"kunit:advsimd-integer-sqshrun-advsimd", 3651U, 4016U, 2),
	INTEGER_SIMD_OPERATION("SQRSHRUN_advsimd",
		"kunit:advsimd-integer-sqrshrun-advsimd", 3652U, 4017U, 2),
	INTEGER_SIMD_OPERATION("UQSHRN_advsimd",
		"kunit:advsimd-integer-uqshrn-advsimd", 3653U, 4018U, 2),
	INTEGER_SIMD_OPERATION("UQRSHRN_advsimd",
		"kunit:advsimd-integer-uqrshrn-advsimd", 3654U, 4019U, 2),
	INTEGER_SIMD_OPERATION("SQDMLAL_advsimd_elt",
		"kunit:advsimd-integer-sqdmlal-advsimd-elt", 3657U, 4024U, 2),
	INTEGER_SIMD_OPERATION("SQDMLSL_advsimd_elt",
		"kunit:advsimd-integer-sqdmlsl-advsimd-elt", 3658U, 4026U, 2),
	INTEGER_SIMD_OPERATION("SQDMULL_advsimd_elt",
		"kunit:advsimd-integer-sqdmull-advsimd-elt", 3659U, 4029U, 2),
	INTEGER_SIMD_OPERATION("SQDMULH_advsimd_elt",
		"kunit:advsimd-integer-sqdmulh-advsimd-elt", 3660U, 4030U, 2),
	INTEGER_SIMD_OPERATION("SQRDMULH_advsimd_elt",
		"kunit:advsimd-integer-sqrdmulh-advsimd-elt", 3661U, 4031U, 2),
	INTEGER_SIMD_OPERATION("SADDLP_advsimd",
		"kunit:advsimd-integer-saddlp-advsimd", 3785U, 3785U, 1),
	INTEGER_SIMD_OPERATION("CLS_advsimd",
		"kunit:advsimd-integer-cls-advsimd", 3787U, 3787U, 1),
	INTEGER_SIMD_OPERATION("CNT_advsimd",
		"kunit:advsimd-integer-cnt-advsimd", 3788U, 3788U, 1),
	INTEGER_SIMD_OPERATION("SADALP_advsimd",
		"kunit:advsimd-integer-sadalp-advsimd", 3789U, 3789U, 1),
	INTEGER_SIMD_OPERATION("XTN_advsimd",
		"kunit:advsimd-integer-xtn-advsimd", 3795U, 3795U, 1),
	INTEGER_SIMD_OPERATION("URECPE_advsimd",
		"kunit:advsimd-integer-urecpe-advsimd", 3815U, 3815U, 1),
	INTEGER_SIMD_OPERATION("UADDLP_advsimd",
		"kunit:advsimd-integer-uaddlp-advsimd", 3819U, 3819U, 1),
	INTEGER_SIMD_OPERATION("CLZ_advsimd",
		"kunit:advsimd-integer-clz-advsimd", 3821U, 3821U, 1),
	INTEGER_SIMD_OPERATION("UADALP_advsimd",
		"kunit:advsimd-integer-uadalp-advsimd", 3822U, 3822U, 1),
	INTEGER_SIMD_OPERATION("SHLL_advsimd",
		"kunit:advsimd-integer-shll-advsimd", 3828U, 3828U, 1),
	INTEGER_SIMD_OPERATION("NOT_advsimd",
		"kunit:advsimd-integer-not-advsimd", 3839U, 3839U, 1),
	INTEGER_SIMD_OPERATION("RBIT_advsimd",
		"kunit:advsimd-integer-rbit-advsimd", 3841U, 3841U, 1),
	INTEGER_SIMD_OPERATION("URSQRTE_advsimd",
		"kunit:advsimd-integer-ursqrte-advsimd", 3849U, 3849U, 1),
	INTEGER_SIMD_OPERATION("SADDLV_advsimd",
		"kunit:advsimd-integer-saddlv-advsimd", 3854U, 3854U, 1),
	INTEGER_SIMD_OPERATION("SMAXV_advsimd",
		"kunit:advsimd-integer-smaxv-advsimd", 3855U, 3855U, 1),
	INTEGER_SIMD_OPERATION("SMINV_advsimd",
		"kunit:advsimd-integer-sminv-advsimd", 3856U, 3856U, 1),
	INTEGER_SIMD_OPERATION("ADDV_advsimd",
		"kunit:advsimd-integer-addv-advsimd", 3857U, 3857U, 1),
	INTEGER_SIMD_OPERATION("UADDLV_advsimd",
		"kunit:advsimd-integer-uaddlv-advsimd", 3862U, 3862U, 1),
	INTEGER_SIMD_OPERATION("UMAXV_advsimd",
		"kunit:advsimd-integer-umaxv-advsimd", 3863U, 3863U, 1),
	INTEGER_SIMD_OPERATION("UMINV_advsimd",
		"kunit:advsimd-integer-uminv-advsimd", 3864U, 3864U, 1),
	INTEGER_SIMD_OPERATION("SADDL_advsimd",
		"kunit:advsimd-integer-saddl-advsimd", 3869U, 3869U, 1),
	INTEGER_SIMD_OPERATION("SADDW_advsimd",
		"kunit:advsimd-integer-saddw-advsimd", 3870U, 3870U, 1),
	INTEGER_SIMD_OPERATION("SSUBL_advsimd",
		"kunit:advsimd-integer-ssubl-advsimd", 3871U, 3871U, 1),
	INTEGER_SIMD_OPERATION("SSUBW_advsimd",
		"kunit:advsimd-integer-ssubw-advsimd", 3872U, 3872U, 1),
	INTEGER_SIMD_OPERATION("ADDHN_advsimd",
		"kunit:advsimd-integer-addhn-advsimd", 3873U, 3873U, 1),
	INTEGER_SIMD_OPERATION("SABAL_advsimd",
		"kunit:advsimd-integer-sabal-advsimd", 3874U, 3874U, 1),
	INTEGER_SIMD_OPERATION("SUBHN_advsimd",
		"kunit:advsimd-integer-subhn-advsimd", 3875U, 3875U, 1),
	INTEGER_SIMD_OPERATION("SABDL_advsimd",
		"kunit:advsimd-integer-sabdl-advsimd", 3876U, 3876U, 1),
	INTEGER_SIMD_OPERATION("SMLAL_advsimd_vec",
		"kunit:advsimd-integer-smlal-advsimd-vec", 3877U, 3877U, 1),
	INTEGER_SIMD_OPERATION("SMLSL_advsimd_vec",
		"kunit:advsimd-integer-smlsl-advsimd-vec", 3879U, 3879U, 1),
	INTEGER_SIMD_OPERATION("SMULL_advsimd_vec",
		"kunit:advsimd-integer-smull-advsimd-vec", 3881U, 3881U, 1),
	INTEGER_SIMD_OPERATION("UADDL_advsimd",
		"kunit:advsimd-integer-uaddl-advsimd", 3884U, 3884U, 1),
	INTEGER_SIMD_OPERATION("UADDW_advsimd",
		"kunit:advsimd-integer-uaddw-advsimd", 3885U, 3885U, 1),
	INTEGER_SIMD_OPERATION("USUBL_advsimd",
		"kunit:advsimd-integer-usubl-advsimd", 3886U, 3886U, 1),
	INTEGER_SIMD_OPERATION("USUBW_advsimd",
		"kunit:advsimd-integer-usubw-advsimd", 3887U, 3887U, 1),
	INTEGER_SIMD_OPERATION("RADDHN_advsimd",
		"kunit:advsimd-integer-raddhn-advsimd", 3888U, 3888U, 1),
	INTEGER_SIMD_OPERATION("UABAL_advsimd",
		"kunit:advsimd-integer-uabal-advsimd", 3889U, 3889U, 1),
	INTEGER_SIMD_OPERATION("RSUBHN_advsimd",
		"kunit:advsimd-integer-rsubhn-advsimd", 3890U, 3890U, 1),
	INTEGER_SIMD_OPERATION("UABDL_advsimd",
		"kunit:advsimd-integer-uabdl-advsimd", 3891U, 3891U, 1),
	INTEGER_SIMD_OPERATION("UMLAL_advsimd_vec",
		"kunit:advsimd-integer-umlal-advsimd-vec", 3892U, 3892U, 1),
	INTEGER_SIMD_OPERATION("UMLSL_advsimd_vec",
		"kunit:advsimd-integer-umlsl-advsimd-vec", 3893U, 3893U, 1),
	INTEGER_SIMD_OPERATION("UMULL_advsimd_vec",
		"kunit:advsimd-integer-umull-advsimd-vec", 3894U, 3894U, 1),
	INTEGER_SIMD_OPERATION("SHADD_advsimd",
		"kunit:advsimd-integer-shadd-advsimd", 3895U, 3895U, 1),
	INTEGER_SIMD_OPERATION("SRHADD_advsimd",
		"kunit:advsimd-integer-srhadd-advsimd", 3897U, 3897U, 1),
	INTEGER_SIMD_OPERATION("SHSUB_advsimd",
		"kunit:advsimd-integer-shsub-advsimd", 3898U, 3898U, 1),
	INTEGER_SIMD_OPERATION("SMAX_advsimd",
		"kunit:advsimd-integer-smax-advsimd", 3906U, 3906U, 1),
	INTEGER_SIMD_OPERATION("SMIN_advsimd",
		"kunit:advsimd-integer-smin-advsimd", 3907U, 3907U, 1),
	INTEGER_SIMD_OPERATION("SABD_advsimd",
		"kunit:advsimd-integer-sabd-advsimd", 3908U, 3908U, 1),
	INTEGER_SIMD_OPERATION("SABA_advsimd",
		"kunit:advsimd-integer-saba-advsimd", 3909U, 3909U, 1),
	INTEGER_SIMD_OPERATION("MLA_advsimd_vec",
		"kunit:advsimd-integer-mla-advsimd-vec", 3912U, 3912U, 1),
	INTEGER_SIMD_OPERATION("MUL_advsimd_vec",
		"kunit:advsimd-integer-mul-advsimd-vec", 3913U, 3913U, 1),
	INTEGER_SIMD_OPERATION("SMAXP_advsimd",
		"kunit:advsimd-integer-smaxp-advsimd", 3914U, 3914U, 1),
	INTEGER_SIMD_OPERATION("SMINP_advsimd",
		"kunit:advsimd-integer-sminp-advsimd", 3915U, 3915U, 1),
	INTEGER_SIMD_OPERATION("ADDP_advsimd_vec",
		"kunit:advsimd-integer-addp-advsimd-vec", 3917U, 3917U, 1),
	INTEGER_SIMD_OPERATION("AND_advsimd",
		"kunit:advsimd-integer-and-advsimd", 3925U, 3925U, 1),
	INTEGER_SIMD_OPERATION("BIC_advsimd_reg",
		"kunit:advsimd-integer-bic-advsimd-reg", 3927U, 3927U, 1),
	INTEGER_SIMD_OPERATION("ORR_advsimd_reg",
		"kunit:advsimd-integer-orr-advsimd-reg", 3934U, 3934U, 1),
	INTEGER_SIMD_OPERATION("ORN_advsimd",
		"kunit:advsimd-integer-orn-advsimd", 3936U, 3936U, 1),
	INTEGER_SIMD_OPERATION("UHADD_advsimd",
		"kunit:advsimd-integer-uhadd-advsimd", 3937U, 3937U, 1),
	INTEGER_SIMD_OPERATION("URHADD_advsimd",
		"kunit:advsimd-integer-urhadd-advsimd", 3939U, 3939U, 1),
	INTEGER_SIMD_OPERATION("UHSUB_advsimd",
		"kunit:advsimd-integer-uhsub-advsimd", 3940U, 3940U, 1),
	INTEGER_SIMD_OPERATION("UMAX_advsimd",
		"kunit:advsimd-integer-umax-advsimd", 3948U, 3948U, 1),
	INTEGER_SIMD_OPERATION("UMIN_advsimd",
		"kunit:advsimd-integer-umin-advsimd", 3949U, 3949U, 1),
	INTEGER_SIMD_OPERATION("UABD_advsimd",
		"kunit:advsimd-integer-uabd-advsimd", 3950U, 3950U, 1),
	INTEGER_SIMD_OPERATION("UABA_advsimd",
		"kunit:advsimd-integer-uaba-advsimd", 3951U, 3951U, 1),
	INTEGER_SIMD_OPERATION("MLS_advsimd_vec",
		"kunit:advsimd-integer-mls-advsimd-vec", 3954U, 3954U, 1),
	INTEGER_SIMD_OPERATION("PMUL_advsimd",
		"kunit:advsimd-integer-pmul-advsimd", 3955U, 3955U, 1),
	INTEGER_SIMD_OPERATION("UMAXP_advsimd",
		"kunit:advsimd-integer-umaxp-advsimd", 3956U, 3956U, 1),
	INTEGER_SIMD_OPERATION("UMINP_advsimd",
		"kunit:advsimd-integer-uminp-advsimd", 3957U, 3957U, 1),
	INTEGER_SIMD_OPERATION("EOR_advsimd",
		"kunit:advsimd-integer-eor-advsimd", 3966U, 3966U, 1),
	INTEGER_SIMD_OPERATION("BSL_advsimd",
		"kunit:advsimd-integer-bsl-advsimd", 3968U, 3968U, 1),
	INTEGER_SIMD_OPERATION("BIT_advsimd",
		"kunit:advsimd-integer-bit-advsimd", 3976U, 3976U, 1),
	INTEGER_SIMD_OPERATION("BIF_advsimd",
		"kunit:advsimd-integer-bif-advsimd", 3978U, 3978U, 1),
	INTEGER_SIMD_OPERATION("ORR_advsimd_imm",
		"kunit:advsimd-integer-orr-advsimd-imm", 3980U, 3982U, 2),
	INTEGER_SIMD_OPERATION("MVNI_advsimd",
		"kunit:advsimd-integer-mvni-advsimd", 3987U, 3991U, 3),
	INTEGER_SIMD_OPERATION("BIC_advsimd_imm",
		"kunit:advsimd-integer-bic-advsimd-imm", 3988U, 3990U, 2),
	INTEGER_SIMD_OPERATION("SHRN_advsimd",
		"kunit:advsimd-integer-shrn-advsimd", 4001U, 4001U, 1),
	INTEGER_SIMD_OPERATION("RSHRN_advsimd",
		"kunit:advsimd-integer-rshrn-advsimd", 4002U, 4002U, 1),
	INTEGER_SIMD_OPERATION("SSHLL_advsimd",
		"kunit:advsimd-integer-sshll-advsimd", 4005U, 4005U, 1),
	INTEGER_SIMD_OPERATION("USHLL_advsimd",
		"kunit:advsimd-integer-ushll-advsimd", 4020U, 4020U, 1),
	INTEGER_SIMD_OPERATION("SMLAL_advsimd_elt",
		"kunit:advsimd-integer-smlal-advsimd-elt", 4023U, 4023U, 1),
	INTEGER_SIMD_OPERATION("SMLSL_advsimd_elt",
		"kunit:advsimd-integer-smlsl-advsimd-elt", 4025U, 4025U, 1),
	INTEGER_SIMD_OPERATION("MUL_advsimd_elt",
		"kunit:advsimd-integer-mul-advsimd-elt", 4027U, 4027U, 1),
	INTEGER_SIMD_OPERATION("SMULL_advsimd_elt",
		"kunit:advsimd-integer-smull-advsimd-elt", 4028U, 4028U, 1),
	INTEGER_SIMD_OPERATION("MLA_advsimd_elt",
		"kunit:advsimd-integer-mla-advsimd-elt", 4048U, 4048U, 1),
	INTEGER_SIMD_OPERATION("UMLAL_advsimd_elt",
		"kunit:advsimd-integer-umlal-advsimd-elt", 4049U, 4049U, 1),
	INTEGER_SIMD_OPERATION("MLS_advsimd_elt",
		"kunit:advsimd-integer-mls-advsimd-elt", 4050U, 4050U, 1),
	INTEGER_SIMD_OPERATION("UMLSL_advsimd_elt",
		"kunit:advsimd-integer-umlsl-advsimd-elt", 4051U, 4051U, 1),
	INTEGER_SIMD_OPERATION("UMULL_advsimd_elt",
		"kunit:advsimd-integer-umull-advsimd-elt", 4052U, 4052U, 1),
	FP_SIMD_OPERATION("FABD_advsimd",
		"kunit:advsimd-fp-fabd-advsimd", 3630U, 3970U, 2),
	FP_SIMD_OPERATION("FABS_advsimd",
		"kunit:advsimd-fp-fabs-advsimd", 3810U, 3810U, 1),
	FP_SIMD_OPERATION("FACGE_advsimd",
		"kunit:advsimd-fp-facge-advsimd", 3629U, 3963U, 2),
	FP_SIMD_OPERATION("FACGT_advsimd",
		"kunit:advsimd-fp-facgt-advsimd", 3632U, 3973U, 2),
	FP_SIMD_OPERATION("FADDP_advsimd_pair",
		"kunit:advsimd-fp-faddp-advsimd-pair", 3595U, 3595U, 1),
	FP_SIMD_OPERATION("FADDP_advsimd_vec",
		"kunit:advsimd-fp-faddp-advsimd-vec", 3960U, 3960U, 1),
	FP_SIMD_OPERATION("FADD_advsimd",
		"kunit:advsimd-fp-fadd-advsimd", 3920U, 3920U, 1),
	FP_SIMD_OPERATION("FCMEQ_advsimd_reg",
		"kunit:advsimd-fp-fcmeq-advsimd-reg", 3614U, 3922U, 2),
	FP_SIMD_OPERATION("FCMEQ_advsimd_zero",
		"kunit:advsimd-fp-fcmeq-advsimd-zero", 3565U, 3808U, 2),
	FP_SIMD_OPERATION("FCMGE_advsimd_reg",
		"kunit:advsimd-fp-fcmge-advsimd-reg", 3628U, 3962U, 2),
	FP_SIMD_OPERATION("FCMGE_advsimd_zero",
		"kunit:advsimd-fp-fcmge-advsimd-zero", 3583U, 3843U, 2),
	FP_SIMD_OPERATION("FCMGT_advsimd_reg",
		"kunit:advsimd-fp-fcmgt-advsimd-reg", 3631U, 3972U, 2),
	FP_SIMD_OPERATION("FCMGT_advsimd_zero",
		"kunit:advsimd-fp-fcmgt-advsimd-zero", 3564U, 3807U, 2),
	FP_SIMD_OPERATION("FCMLE_advsimd",
		"kunit:advsimd-fp-fcmle-advsimd", 3584U, 3844U, 2),
	FP_SIMD_OPERATION("FCMLT_advsimd",
		"kunit:advsimd-fp-fcmlt-advsimd", 3566U, 3809U, 2),
	FP_SIMD_OPERATION("FCVTAS_advsimd",
		"kunit:advsimd-fp-fcvtas-advsimd", 3562U, 3803U, 2),
	FP_SIMD_OPERATION("FCVTAU_advsimd",
		"kunit:advsimd-fp-fcvtau-advsimd", 3581U, 3835U, 2),
	FP_SIMD_OPERATION("FCVTL_advsimd",
		"kunit:advsimd-fp-fcvtl-advsimd", 3798U, 3798U, 1),
	FP_SIMD_OPERATION("FCVTMS_advsimd",
		"kunit:advsimd-fp-fcvtms-advsimd", 3561U, 3802U, 2),
	FP_SIMD_OPERATION("FCVTMU_advsimd",
		"kunit:advsimd-fp-fcvtmu-advsimd", 3580U, 3834U, 2),
	FP_SIMD_OPERATION("FCVTNS_advsimd",
		"kunit:advsimd-fp-fcvtns-advsimd", 3560U, 3801U, 2),
	FP_SIMD_OPERATION("FCVTNU_advsimd",
		"kunit:advsimd-fp-fcvtnu-advsimd", 3579U, 3833U, 2),
	FP_SIMD_OPERATION("FCVTN_advsimd",
		"kunit:advsimd-fp-fcvtn-advsimd", 3797U, 3797U, 1),
	FP_SIMD_OPERATION("FCVTPS_advsimd",
		"kunit:advsimd-fp-fcvtps-advsimd", 3567U, 3813U, 2),
	FP_SIMD_OPERATION("FCVTPU_advsimd",
		"kunit:advsimd-fp-fcvtpu-advsimd", 3585U, 3847U, 2),
	FP_SIMD_OPERATION("FCVTXN_advsimd",
		"kunit:advsimd-fp-fcvtxn-advsimd", 3578U, 3830U, 2),
	FP_SIMD_SHIFT_OPERATION("FCVTZS_advsimd_fix",
		"kunit:advsimd-fp-fcvtzs-advsimd-fix", 3642U, 4007U, 2),
	FP_SIMD_OPERATION("FCVTZS_advsimd_int",
		"kunit:advsimd-fp-fcvtzs-advsimd-int", 3568U, 3814U, 2),
	FP_SIMD_SHIFT_OPERATION("FCVTZU_advsimd_fix",
		"kunit:advsimd-fp-fcvtzu-advsimd-fix", 3656U, 4022U, 2),
	FP_SIMD_OPERATION("FCVTZU_advsimd_int",
		"kunit:advsimd-fp-fcvtzu-advsimd-int", 3586U, 3848U, 2),
	FP_SIMD_OPERATION("FDIV_advsimd",
		"kunit:advsimd-fp-fdiv-advsimd", 3965U, 3965U, 1),
	FP_SIMD_OPERATION("FMAXNMP_advsimd_pair",
		"kunit:advsimd-fp-fmaxnmp-advsimd-pair", 3594U, 3594U, 1),
	FP_SIMD_OPERATION("FMAXNMP_advsimd_vec",
		"kunit:advsimd-fp-fmaxnmp-advsimd-vec", 3959U, 3959U, 1),
	FP_SIMD_OPERATION("FMAXNMV_advsimd",
		"kunit:advsimd-fp-fmaxnmv-advsimd", 3865U, 3865U, 1),
	FP_SIMD_OPERATION("FMAXNM_advsimd",
		"kunit:advsimd-fp-fmaxnm-advsimd", 3918U, 3918U, 1),
	FP_SIMD_OPERATION("FMAXP_advsimd_pair",
		"kunit:advsimd-fp-fmaxp-advsimd-pair", 3596U, 3596U, 1),
	FP_SIMD_OPERATION("FMAXP_advsimd_vec",
		"kunit:advsimd-fp-fmaxp-advsimd-vec", 3964U, 3964U, 1),
	FP_SIMD_OPERATION("FMAXV_advsimd",
		"kunit:advsimd-fp-fmaxv-advsimd", 3866U, 3866U, 1),
	FP_SIMD_OPERATION("FMAX_advsimd",
		"kunit:advsimd-fp-fmax-advsimd", 3923U, 3923U, 1),
	FP_SIMD_OPERATION("FMINNMP_advsimd_pair",
		"kunit:advsimd-fp-fminnmp-advsimd-pair", 3597U, 3597U, 1),
	FP_SIMD_OPERATION("FMINNMP_advsimd_vec",
		"kunit:advsimd-fp-fminnmp-advsimd-vec", 3969U, 3969U, 1),
	FP_SIMD_OPERATION("FMINNMV_advsimd",
		"kunit:advsimd-fp-fminnmv-advsimd", 3867U, 3867U, 1),
	FP_SIMD_OPERATION("FMINNM_advsimd",
		"kunit:advsimd-fp-fminnm-advsimd", 3928U, 3928U, 1),
	FP_SIMD_OPERATION("FMINP_advsimd_pair",
		"kunit:advsimd-fp-fminp-advsimd-pair", 3598U, 3598U, 1),
	FP_SIMD_OPERATION("FMINP_advsimd_vec",
		"kunit:advsimd-fp-fminp-advsimd-vec", 3974U, 3974U, 1),
	FP_SIMD_OPERATION("FMINV_advsimd",
		"kunit:advsimd-fp-fminv-advsimd", 3868U, 3868U, 1),
	FP_SIMD_OPERATION("FMIN_advsimd",
		"kunit:advsimd-fp-fmin-advsimd", 3932U, 3932U, 1),
	FP_SIMD_OPERATION("FMLA_advsimd_elt",
		"kunit:advsimd-fp-fmla-advsimd-elt", 3665U, 4041U, 2),
	FP_SIMD_OPERATION("FMLA_advsimd_vec",
		"kunit:advsimd-fp-fmla-advsimd-vec", 3919U, 3919U, 1),
	FP_SIMD_OPERATION("FMLS_advsimd_elt",
		"kunit:advsimd-fp-fmls-advsimd-elt", 3666U, 4042U, 2),
	FP_SIMD_OPERATION("FMLS_advsimd_vec",
		"kunit:advsimd-fp-fmls-advsimd-vec", 3929U, 3929U, 1),
	FP_SIMD_OPERATION("FMOV_advsimd",
		"kunit:advsimd-fp-fmov-advsimd", 3985U, 3994U, 2),
	FP_SIMD_OPERATION("FMULX_advsimd_elt",
		"kunit:advsimd-fp-fmulx-advsimd-elt", 3671U, 4058U, 2),
	FP_SIMD_OPERATION("FMULX_advsimd_vec",
		"kunit:advsimd-fp-fmulx-advsimd-vec", 3613U, 3921U, 2),
	FP_SIMD_OPERATION("FMUL_advsimd_elt",
		"kunit:advsimd-fp-fmul-advsimd-elt", 3667U, 4043U, 2),
	FP_SIMD_OPERATION("FMUL_advsimd_vec",
		"kunit:advsimd-fp-fmul-advsimd-vec", 3961U, 3961U, 1),
	FP_SIMD_OPERATION("FNEG_advsimd",
		"kunit:advsimd-fp-fneg-advsimd", 3845U, 3845U, 1),
	FP_SIMD_OPERATION("FRECPE_advsimd",
		"kunit:advsimd-fp-frecpe-advsimd", 3569U, 3816U, 2),
	FP_SIMD_OPERATION("FRECPS_advsimd",
		"kunit:advsimd-fp-frecps-advsimd", 3615U, 3924U, 2),
	FP_SIMD_OPERATION("FRECPX_advsimd",
		"kunit:advsimd-fp-frecpx-advsimd", 3570U, 3570U, 1),
	FP_SIMD_OPERATION("FRINTA_advsimd",
		"kunit:advsimd-fp-frinta-advsimd", 3831U, 3831U, 1),
	FP_SIMD_OPERATION("FRINTI_advsimd",
		"kunit:advsimd-fp-frinti-advsimd", 3846U, 3846U, 1),
	FP_SIMD_OPERATION("FRINTM_advsimd",
		"kunit:advsimd-fp-frintm-advsimd", 3800U, 3800U, 1),
	FP_SIMD_OPERATION("FRINTN_advsimd",
		"kunit:advsimd-fp-frintn-advsimd", 3799U, 3799U, 1),
	FP_SIMD_OPERATION("FRINTP_advsimd",
		"kunit:advsimd-fp-frintp-advsimd", 3811U, 3811U, 1),
	FP_SIMD_OPERATION("FRINTX_advsimd",
		"kunit:advsimd-fp-frintx-advsimd", 3832U, 3832U, 1),
	FP_SIMD_OPERATION("FRINTZ_advsimd",
		"kunit:advsimd-fp-frintz-advsimd", 3812U, 3812U, 1),
	FP_SIMD_OPERATION("FRSQRTE_advsimd",
		"kunit:advsimd-fp-frsqrte-advsimd", 3587U, 3850U, 2),
	FP_SIMD_OPERATION("FRSQRTS_advsimd",
		"kunit:advsimd-fp-frsqrts-advsimd", 3616U, 3933U, 2),
	FP_SIMD_OPERATION("FSQRT_advsimd",
		"kunit:advsimd-fp-fsqrt-advsimd", 3851U, 3851U, 1),
	FP_SIMD_OPERATION("FSUB_advsimd",
		"kunit:advsimd-fp-fsub-advsimd", 3930U, 3930U, 1),
	FP_SIMD_SHIFT_OPERATION("SCVTF_advsimd_fix",
		"kunit:advsimd-fp-scvtf-advsimd-fix", 3641U, 4006U, 2),
	FP_SIMD_OPERATION("SCVTF_advsimd_int",
		"kunit:advsimd-fp-scvtf-advsimd-int", 3563U, 3804U, 2),
	FP_SIMD_SHIFT_OPERATION("UCVTF_advsimd_fix",
		"kunit:advsimd-fp-ucvtf-advsimd-fix", 3655U, 4021U, 2),
	FP_SIMD_OPERATION("UCVTF_advsimd_int",
		"kunit:advsimd-fp-ucvtf-advsimd-int", 3582U, 3836U, 2),
	SCALAR_FP_OPERATION("SCVTF_float_fix",
		"kunit:scalar-fp-convert-scvtf-fix", 4084U, 4100U, 4),
	SCALAR_FP_OPERATION("UCVTF_float_fix",
		"kunit:scalar-fp-convert-ucvtf-fix", 4085U, 4101U, 4),
	SCALAR_FP_OPERATION("FCVTZS_float_fix",
		"kunit:scalar-fp-convert-fcvtzs-fix", 4086U, 4102U, 4),
	SCALAR_FP_OPERATION("FCVTZU_float_fix",
		"kunit:scalar-fp-convert-fcvtzu-fix", 4087U, 4103U, 4),
	SCALAR_FP_OPERATION("FCVTNS_float",
		"kunit:scalar-fp-convert-fcvtns", 4108U, 4161U, 4),
	SCALAR_FP_OPERATION("FCVTNU_float",
		"kunit:scalar-fp-convert-fcvtnu", 4109U, 4162U, 4),
	SCALAR_FP_OPERATION("SCVTF_float_int",
		"kunit:scalar-fp-convert-scvtf-int", 4110U, 4163U, 4),
	SCALAR_FP_OPERATION("UCVTF_float_int",
		"kunit:scalar-fp-convert-ucvtf-int", 4111U, 4164U, 4),
	SCALAR_FP_OPERATION("FCVTAS_float",
		"kunit:scalar-fp-convert-fcvtas", 4112U, 4165U, 4),
	SCALAR_FP_OPERATION("FCVTAU_float",
		"kunit:scalar-fp-convert-fcvtau", 4113U, 4166U, 4),
	SCALAR_FP_OPERATION("FMOV_float_gen",
		"kunit:scalar-fp-fmov-gen", 4114U, 4176U, 6),
	SCALAR_FP_OPERATION("FCVTPS_float",
		"kunit:scalar-fp-convert-fcvtps", 4116U, 4169U, 4),
	SCALAR_FP_OPERATION("FCVTPU_float",
		"kunit:scalar-fp-convert-fcvtpu", 4117U, 4170U, 4),
	SCALAR_FP_OPERATION("FCVTMS_float",
		"kunit:scalar-fp-convert-fcvtms", 4118U, 4171U, 4),
	SCALAR_FP_OPERATION("FCVTMU_float",
		"kunit:scalar-fp-convert-fcvtmu", 4119U, 4172U, 4),
	SCALAR_FP_OPERATION("FCVTZS_float_int",
		"kunit:scalar-fp-convert-fcvtzs-int", 4120U, 4173U, 4),
	SCALAR_FP_OPERATION("FCVTZU_float_int",
		"kunit:scalar-fp-convert-fcvtzu-int", 4121U, 4174U, 4),
	SCALAR_FP_OPERATION("FMOV_float",
		"kunit:scalar-fp-fmov", 4239U, 4256U, 2),
	SCALAR_FP_OPERATION("FABS_float",
		"kunit:scalar-fp-fabs", 4240U, 4257U, 2),
	SCALAR_FP_OPERATION("FNEG_float",
		"kunit:scalar-fp-fneg", 4241U, 4258U, 2),
	SCALAR_FP_OPERATION("FSQRT_float",
		"kunit:scalar-fp-fsqrt", 4242U, 4259U, 2),
	SCALAR_FP_OPERATION("FCVT_float",
		"kunit:scalar-fp-fcvt", 4243U, 4279U, 6),
	SCALAR_FP_OPERATION("FRINTN_float",
		"kunit:scalar-fp-frintn", 4245U, 4263U, 2),
	SCALAR_FP_OPERATION("FRINTP_float",
		"kunit:scalar-fp-frintp", 4246U, 4264U, 2),
	SCALAR_FP_OPERATION("FRINTM_float",
		"kunit:scalar-fp-frintm", 4247U, 4265U, 2),
	SCALAR_FP_OPERATION("FRINTZ_float",
		"kunit:scalar-fp-frintz", 4248U, 4266U, 2),
	SCALAR_FP_OPERATION("FRINTA_float",
		"kunit:scalar-fp-frinta", 4249U, 4267U, 2),
	SCALAR_FP_OPERATION("FRINTX_float",
		"kunit:scalar-fp-frintx", 4250U, 4268U, 2),
	SCALAR_FP_OPERATION("FRINTI_float",
		"kunit:scalar-fp-frinti", 4251U, 4269U, 2),
	SCALAR_FP_OPERATION("FCMP_float",
		"kunit:scalar-fp-fcmp", 4287U, 4292U, 4),
	SCALAR_FP_OPERATION("FCMPE_float",
		"kunit:scalar-fp-fcmpe", 4289U, 4294U, 4),
	SCALAR_FP_OPERATION("FMOV_float_imm",
		"kunit:scalar-fp-fmov-imm", 4299U, 4300U, 2),
	SCALAR_FP_OPERATION("FCCMP_float",
		"kunit:scalar-fp-fccmp", 4302U, 4304U, 2),
	SCALAR_FP_OPERATION("FCCMPE_float",
		"kunit:scalar-fp-fccmpe", 4303U, 4305U, 2),
	SCALAR_FP_OPERATION("FMUL_float",
		"kunit:scalar-fp-fmul", 4308U, 4317U, 2),
	SCALAR_FP_OPERATION("FDIV_float",
		"kunit:scalar-fp-fdiv", 4309U, 4318U, 2),
	SCALAR_FP_OPERATION("FADD_float",
		"kunit:scalar-fp-fadd", 4310U, 4319U, 2),
	SCALAR_FP_OPERATION("FSUB_float",
		"kunit:scalar-fp-fsub", 4311U, 4320U, 2),
	SCALAR_FP_OPERATION("FMAX_float",
		"kunit:scalar-fp-fmax", 4312U, 4321U, 2),
	SCALAR_FP_OPERATION("FMIN_float",
		"kunit:scalar-fp-fmin", 4313U, 4322U, 2),
	SCALAR_FP_OPERATION("FMAXNM_float",
		"kunit:scalar-fp-fmaxnm", 4314U, 4323U, 2),
	SCALAR_FP_OPERATION("FMINNM_float",
		"kunit:scalar-fp-fminnm", 4315U, 4324U, 2),
	SCALAR_FP_OPERATION("FNMUL_float",
		"kunit:scalar-fp-fnmul", 4316U, 4325U, 2),
	SCALAR_FP_OPERATION("FCSEL_float",
		"kunit:scalar-fp-fcsel", 4335U, 4336U, 2),
	SCALAR_FP_OPERATION("FMADD_float",
		"kunit:scalar-fp-fmadd", 4338U, 4342U, 2),
	SCALAR_FP_OPERATION("FMSUB_float",
		"kunit:scalar-fp-fmsub", 4339U, 4343U, 2),
	SCALAR_FP_OPERATION("FNMADD_float",
		"kunit:scalar-fp-fnmadd", 4340U, 4344U, 2),
	SCALAR_FP_OPERATION("FNMSUB_float",
		"kunit:scalar-fp-fnmsub", 4341U, 4345U, 2),
};

#undef CRYPTO_OPERATION
#undef PERMUTE_MOVE_SIMD_OPERATION
#undef PERMUTE_MOVE_GPR_OPERATION
#undef INTEGER_SIMD_OPERATION
#undef SCALAR_FP_OPERATION
#undef SCALAR_OPERATION

static struct orlix_tcti_target_proof_binding scalar_registry_bindings[
	SCALAR_PROOF_REGISTRY_BINDING_COUNT];
static bool scalar_registry_ready;

static struct scalar_registry_operation *
scalar_registry_operation_for(const struct source_manifest_binding *source)
{
	size_t index;

	for (index = 0; index < ARRAY_COUNT(scalar_registry_operations); index++) {
		struct scalar_registry_operation *operation =
			&scalar_registry_operations[index];

		if (!strcmp(source->operation_id, operation->operation_id) &&
		    (!operation->condition[0] ||
		     !strcmp(source->condition_tcnd_hex, operation->condition)) &&
		    source->ordinal >= operation->minimum_ordinal &&
		    source->ordinal <= operation->maximum_ordinal)
			return operation;
	}
	return NULL;
}

static bool build_scalar_registry(void)
{
	size_t binding_offset = 0;
	size_t index;

	if (scalar_registry_ready)
		return true;
	if (ARRAY_COUNT(scalar_registry_operations) !=
	    SCALAR_PROOF_REGISTRY_ENTRY_COUNT)
		return false;
	for (index = 0; index < ARRAY_COUNT(source_manifest_bindings); index++) {
		struct scalar_registry_operation *operation =
			scalar_registry_operation_for(&source_manifest_bindings[index]);

		if (operation)
			operation->binding_count++;
	}
	for (index = 0; index < ARRAY_COUNT(scalar_registry_operations); index++) {
		struct scalar_registry_operation *operation =
			&scalar_registry_operations[index];

		if (operation->binding_count != operation->expected_bindings)
			return false;
		operation->binding_offset = binding_offset;
		binding_offset += operation->binding_count;
		operation->binding_count = 0;
	}
	if (binding_offset != SCALAR_PROOF_REGISTRY_BINDING_COUNT)
		return false;
	for (index = 0; index < ARRAY_COUNT(source_manifest_bindings); index++) {
		const struct source_manifest_binding *source =
			&source_manifest_bindings[index];
		struct scalar_registry_operation *operation =
			scalar_registry_operation_for(source);
		struct orlix_tcti_target_proof_binding *binding;

		if (!operation)
			continue;
		binding = &scalar_registry_bindings[operation->binding_offset +
			operation->binding_count++];
		binding->leaf_name = source->leaf_name;
		binding->mnemonic = source->mnemonic;
		binding->encoding_mask = source->encoding_mask;
		binding->encoding_pattern = source->encoding_pattern;
		binding->condition_tcnd_hex = source->condition_tcnd_hex;
		binding->kunit_case_mask =
			(ORLIX_TCTI_PROOF_U64_C(1) << operation->case_count) - 1;
		binding->source_ordinal = source->ordinal;
	}
	for (index = 0; index < ARRAY_COUNT(scalar_registry_operations); index++) {
		struct scalar_registry_operation *operation =
			&scalar_registry_operations[index];
		struct orlix_tcti_target_proof_registry_entry *entry =
			&proof_registry_entries[CORE_PROOF_REGISTRY_ENTRY_COUNT +
				LSE_PROOF_REGISTRY_ENTRY_COUNT + index];
		orlix_tcti_proof_u32 obligations;

		if (operation->binding_count != operation->expected_bindings ||
		    orlix_tcti_target_proof_operation_requirements(operation->operation_id, 1,
						   &obligations))
			return false;
		*entry = (struct orlix_tcti_target_proof_registry_entry){
			.id = operation->proof_id,
			.operation_id = operation->operation_id,
			.classification_mask = ORLIX_TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0,
			.obligations = obligations,
			.linux_interface =
				(!strcmp(operation->proof_id, "kunit:branch-control-svc") ||
				 !strcmp(operation->proof_id, "kunit:branch-control-brk") ||
				 !strcmp(operation->proof_id, "kunit:branch-control-hlt")) ?
				ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_REQUIRED :
				ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE,
			.kunit_source = operation->source,
			.kunit_suite = operation->suite,
			.kunit_cases = operation->cases,
			.kunit_case_count = operation->case_count,
			.bindings = &scalar_registry_bindings[operation->binding_offset],
			.binding_count = operation->binding_count,
			.kselftest =
				(!strcmp(operation->proof_id, "kunit:branch-control-svc") ||
				 !strcmp(operation->proof_id, "kunit:branch-control-brk") ||
				 !strcmp(operation->proof_id, "kunit:branch-control-hlt")) ?
				&exception_interface_kselftest : NULL,
			.unproved_obligations = obligations,
		};
	}
	scalar_registry_ready = true;
	return true;
}

struct exclusive_registry_operation {
	const char *operation_id;
	const char *proof_id;
	size_t expected_bindings;
	size_t binding_offset;
	size_t binding_count;
};

static struct exclusive_registry_operation exclusive_registry_operations[] = {
	{ "STXP", "kunit:exclusive-stxp", 2, 0, 0 },
	{ "STLXP", "kunit:exclusive-stlxp", 2, 0, 0 },
	{ "LDXP", "kunit:exclusive-ldxp", 2, 0, 0 },
	{ "LDAXP", "kunit:exclusive-ldaxp", 2, 0, 0 },
	{ "STXRB", "kunit:exclusive-stxrb", 1, 0, 0 },
	{ "STLXRB", "kunit:exclusive-stlxrb", 1, 0, 0 },
	{ "LDXRB", "kunit:exclusive-ldxrb", 1, 0, 0 },
	{ "LDAXRB", "kunit:exclusive-ldaxrb", 1, 0, 0 },
	{ "STXRH", "kunit:exclusive-stxrh", 1, 0, 0 },
	{ "STLXRH", "kunit:exclusive-stlxrh", 1, 0, 0 },
	{ "LDXRH", "kunit:exclusive-ldxrh", 1, 0, 0 },
	{ "LDAXRH", "kunit:exclusive-ldaxrh", 1, 0, 0 },
	{ "STXR", "kunit:exclusive-stxr", 2, 0, 0 },
	{ "STLXR", "kunit:exclusive-stlxr", 2, 0, 0 },
	{ "LDXR", "kunit:exclusive-ldxr", 2, 0, 0 },
	{ "LDAXR", "kunit:exclusive-ldaxr", 2, 0, 0 },
};

static struct orlix_tcti_target_proof_binding exclusive_registry_bindings[
	EXCLUSIVE_PROOF_REGISTRY_BINDING_COUNT];
static bool exclusive_registry_ready;

static struct exclusive_registry_operation *
exclusive_registry_operation_for(const struct source_manifest_binding *source)
{
	size_t index;

	if (strcmp(source->condition_tcnd_hex, LOGICAL_SHIFT_CONDITION))
		return NULL;
	for (index = 0; index < ARRAY_COUNT(exclusive_registry_operations); index++)
		if (!strcmp(source->operation_id,
			    exclusive_registry_operations[index].operation_id))
			return &exclusive_registry_operations[index];
	return NULL;
}

static bool build_exclusive_registry(void)
{
	size_t binding_offset = 0;
	size_t index;

	if (exclusive_registry_ready)
		return true;
	for (index = 0; index < ARRAY_COUNT(source_manifest_bindings); index++) {
		struct exclusive_registry_operation *operation =
			exclusive_registry_operation_for(&source_manifest_bindings[index]);

		if (operation)
			operation->binding_count++;
	}
	for (index = 0; index < ARRAY_COUNT(exclusive_registry_operations); index++) {
		struct exclusive_registry_operation *operation =
			&exclusive_registry_operations[index];

		if (operation->binding_count != operation->expected_bindings)
			return false;
		operation->binding_offset = binding_offset;
		binding_offset += operation->binding_count;
		operation->binding_count = 0;
	}
	if (binding_offset != EXCLUSIVE_PROOF_REGISTRY_BINDING_COUNT)
		return false;
	for (index = 0; index < ARRAY_COUNT(source_manifest_bindings); index++) {
		const struct source_manifest_binding *source =
			&source_manifest_bindings[index];
		struct exclusive_registry_operation *operation =
			exclusive_registry_operation_for(source);
		struct orlix_tcti_target_proof_binding *binding;

		if (!operation)
			continue;
		binding = &exclusive_registry_bindings[
			operation->binding_offset + operation->binding_count++];
		binding->leaf_name = source->leaf_name;
		binding->mnemonic = source->mnemonic;
		binding->encoding_mask = source->encoding_mask;
		binding->encoding_pattern = source->encoding_pattern;
		binding->condition_tcnd_hex = source->condition_tcnd_hex;
		binding->kunit_case_mask =
			(ORLIX_TCTI_PROOF_U64_C(1) << ARRAY_COUNT(base_atomic_production_cases)) - 1;
		binding->source_ordinal = source->ordinal;
	}
	for (index = 0; index < ARRAY_COUNT(exclusive_registry_operations); index++) {
		struct exclusive_registry_operation *operation =
			&exclusive_registry_operations[index];
		struct orlix_tcti_target_proof_registry_entry *entry =
			&proof_registry_entries[CORE_PROOF_REGISTRY_ENTRY_COUNT +
				LSE_PROOF_REGISTRY_ENTRY_COUNT +
				SCALAR_PROOF_REGISTRY_ENTRY_COUNT + index];

		if (operation->binding_count != operation->expected_bindings)
			return false;
		*entry = (struct orlix_tcti_target_proof_registry_entry) {
			.id = operation->proof_id,
			.operation_id = operation->operation_id,
			.classification_mask = ORLIX_TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0,
			.obligations = EXCLUSIVE_REQUIRED_OBLIGATIONS,
			.linux_interface =
				ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE,
			.kunit_source = BASE_ATOMIC_SOURCE,
			.kunit_suite = BASE_ATOMIC_SUITE,
			.kunit_cases = base_atomic_production_cases,
			.kunit_case_count = ARRAY_COUNT(base_atomic_production_cases),
			.bindings = &exclusive_registry_bindings[operation->binding_offset],
			.binding_count = operation->binding_count,
			.kselftest = NULL,
			/* KUnit source ownership does not prove atomicity or ordering. */
			.unproved_obligations = EXCLUSIVE_REQUIRED_OBLIGATIONS,
		};
	}
	exclusive_registry_ready = true;
	return true;
}

static bool empty(const char *text)
{
	return !text || !text[0];
}

static bool one_bit(orlix_tcti_proof_u32 value)
{
	return value && !(value & (value - 1));
}

static const struct source_manifest_binding *
source_manifest_binding(orlix_tcti_proof_u32 ordinal)
{
	size_t index;

	for (index = 0; index < sizeof(source_manifest_bindings) /
				      sizeof(source_manifest_bindings[0]); index++)
		if (source_manifest_bindings[index].ordinal == ordinal)
			return &source_manifest_bindings[index];
	return NULL;
}

static const struct orlix_tcti_target_proof_case ordinary_load_store_cases[] = {
	{ "bls_source_decode",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "bls_reserved_is_rejected",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ "bls_production_resume",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "bls_unmapped_load_faults",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS },
};

static struct ordinary_load_store_registry_operation *
ordinary_load_store_registry_operation_for(const char *proof_id)
{
	size_t index;

	for (index = 0; index < ARRAY_COUNT(ordinary_load_store_registry_operations);
	     index++)
		if (!strcmp(proof_id,
			    ordinary_load_store_registry_operations[index].proof_id))
			return &ordinary_load_store_registry_operations[index];
	return NULL;
}

static orlix_tcti_proof_u64 ordinary_load_store_case_mask(
	const struct source_manifest_binding *source)
{
	(void)source;
	return ORLIX_TCTI_PROOF_U64_C(0xf);
}

static bool build_ordinary_load_store_registry(void)
{
	size_t binding_offset = 0;
	size_t index;
	size_t entry_base = ARRAY_COUNT(proof_registry_entries) -
		ORDINARY_LOAD_STORE_PROOF_REGISTRY_ENTRY_COUNT -
		ADVSIMD_LOAD_STORE_PROOF_REGISTRY_ENTRY_COUNT -
		PRODUCTION_CAPTURE_FAMILY_PROOF_REGISTRY_ENTRY_COUNT -
		MOPS_COPY_PROOF_REGISTRY_ENTRY_COUNT -
		BASE_ATOMIC_MISSING_PROOF_REGISTRY_ENTRY_COUNT;

	if (ordinary_load_store_registry_ready)
		return true;
	if (ARRAY_COUNT(ordinary_load_store_registry_operations) !=
	    ORDINARY_LOAD_STORE_PROOF_REGISTRY_ENTRY_COUNT)
		return false;
	for (index = 0; index < ARRAY_COUNT(source_bound_proofs); index++) {
		struct ordinary_load_store_registry_operation *operation =
			ordinary_load_store_registry_operation_for(
				source_bound_proofs[index].proof_id);
		const struct source_manifest_binding *source;

		if (!operation)
			continue;
		source = source_manifest_binding(
			source_bound_proofs[index].source_ordinal);
		if (!source || strcmp(source->operation_id, operation->operation_id) ||
		    !ordinary_load_store_case_mask(source))
			return false;
		operation->binding_count++;
	}
	for (index = 0; index < ARRAY_COUNT(ordinary_load_store_registry_operations);
	     index++) {
		struct ordinary_load_store_registry_operation *operation =
			&ordinary_load_store_registry_operations[index];

		if (!operation->binding_count)
			return false;
		operation->binding_offset = binding_offset;
		binding_offset += operation->binding_count;
		operation->binding_count = 0;
	}
	if (binding_offset != ORDINARY_LOAD_STORE_PROOF_REGISTRY_BINDING_COUNT)
		return false;
	for (index = 0; index < ARRAY_COUNT(source_bound_proofs); index++) {
		struct ordinary_load_store_registry_operation *operation =
			ordinary_load_store_registry_operation_for(
				source_bound_proofs[index].proof_id);
		const struct source_manifest_binding *source;
		struct orlix_tcti_target_proof_binding *binding;

		if (!operation)
			continue;
		source = source_manifest_binding(
			source_bound_proofs[index].source_ordinal);
		binding = &ordinary_load_store_registry_bindings[
			operation->binding_offset + operation->binding_count++];
		*binding = (struct orlix_tcti_target_proof_binding) {
			.leaf_name = source->leaf_name,
			.mnemonic = source->mnemonic,
			.encoding_mask = source->encoding_mask,
			.encoding_pattern = source->encoding_pattern,
			.condition_tcnd_hex = source->condition_tcnd_hex,
			.kunit_case_mask = ordinary_load_store_case_mask(source),
			.source_ordinal = source->ordinal,
		};
	}
	for (index = 0; index < ARRAY_COUNT(ordinary_load_store_registry_operations);
	     index++) {
		struct ordinary_load_store_registry_operation *operation =
			&ordinary_load_store_registry_operations[index];

		if (!operation->binding_count)
			return false;
		proof_registry_entries[entry_base + index] =
			(struct orlix_tcti_target_proof_registry_entry) {
				.id = operation->proof_id,
				.operation_id = operation->operation_id,
				.classification_mask =
					ORLIX_TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0,
				.obligations = ORDINARY_LOAD_STORE_OBLIGATIONS,
				.linux_interface =
					ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE,
				.kunit_source = BASE_LOAD_STORE_SOURCE,
				.kunit_suite = BASE_LOAD_STORE_SUITE,
				.kunit_cases = ordinary_load_store_cases,
				.kunit_case_count = ARRAY_COUNT(ordinary_load_store_cases),
				.bindings = &ordinary_load_store_registry_bindings[
					operation->binding_offset],
				.binding_count = operation->binding_count,
				.kselftest = NULL,
				.unproved_obligations =
					ORDINARY_LOAD_STORE_OBLIGATIONS,
			};
	}
	ordinary_load_store_registry_ready = true;
	return true;
}

static const struct orlix_tcti_target_proof_case advsimd_load_store_cases[] = {
	{ "orlix_tcti_advsimd_structure_every_source_leaf_reaches_decoder",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "orlix_tcti_advsimd_structure_reserved_encodings_reject",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ "asl_production_resume",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "asl_unmapped_load_faults",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS },
};

static struct advsimd_load_store_registry_operation *
advsimd_load_store_registry_operation_for(const char *proof_id)
{
	size_t index;

	for (index = 0; index < ARRAY_COUNT(advsimd_load_store_registry_operations);
	     index++)
		if (!strcmp(proof_id,
			    advsimd_load_store_registry_operations[index].proof_id))
			return &advsimd_load_store_registry_operations[index];
	return NULL;
}

static orlix_tcti_proof_u64 advsimd_load_store_case_mask(
	const struct source_manifest_binding *source)
{
	(void)source;
	return ORLIX_TCTI_PROOF_U64_C(0xf);
}

static bool build_advsimd_load_store_registry(void)
{
	size_t binding_offset = 0;
	size_t index;
	size_t entry_base = ARRAY_COUNT(proof_registry_entries) -
		ADVSIMD_LOAD_STORE_PROOF_REGISTRY_ENTRY_COUNT -
		PRODUCTION_CAPTURE_FAMILY_PROOF_REGISTRY_ENTRY_COUNT -
		MOPS_COPY_PROOF_REGISTRY_ENTRY_COUNT -
		BASE_ATOMIC_MISSING_PROOF_REGISTRY_ENTRY_COUNT;

	if (advsimd_load_store_registry_ready)
		return true;
	if (ARRAY_COUNT(advsimd_load_store_registry_operations) !=
	    ADVSIMD_LOAD_STORE_PROOF_REGISTRY_ENTRY_COUNT)
		return false;
	for (index = 0; index < ARRAY_COUNT(source_bound_proofs); index++) {
		struct advsimd_load_store_registry_operation *operation =
			advsimd_load_store_registry_operation_for(
				source_bound_proofs[index].proof_id);
		const struct source_manifest_binding *source;

		if (!operation)
			continue;
		source = source_manifest_binding(
			source_bound_proofs[index].source_ordinal);
		if (!source || strcmp(source->operation_id, operation->operation_id) ||
		    !advsimd_load_store_case_mask(source))
			return false;
		operation->binding_count++;
	}
	for (index = 0; index < ARRAY_COUNT(advsimd_load_store_registry_operations);
	     index++) {
		struct advsimd_load_store_registry_operation *operation =
			&advsimd_load_store_registry_operations[index];

		if (!operation->binding_count)
			return false;
		operation->binding_offset = binding_offset;
		binding_offset += operation->binding_count;
		operation->binding_count = 0;
	}
	if (binding_offset != ADVSIMD_LOAD_STORE_PROOF_REGISTRY_BINDING_COUNT)
		return false;
	for (index = 0; index < ARRAY_COUNT(source_bound_proofs); index++) {
		struct advsimd_load_store_registry_operation *operation =
			advsimd_load_store_registry_operation_for(
				source_bound_proofs[index].proof_id);
		const struct source_manifest_binding *source;
		struct orlix_tcti_target_proof_binding *binding;

		if (!operation)
			continue;
		source = source_manifest_binding(
			source_bound_proofs[index].source_ordinal);
		binding = &advsimd_load_store_registry_bindings[
			operation->binding_offset + operation->binding_count++];
		*binding = (struct orlix_tcti_target_proof_binding) {
			.leaf_name = source->leaf_name,
			.mnemonic = source->mnemonic,
			.encoding_mask = source->encoding_mask,
			.encoding_pattern = source->encoding_pattern,
			.condition_tcnd_hex = source->condition_tcnd_hex,
			.kunit_case_mask = advsimd_load_store_case_mask(source),
			.source_ordinal = source->ordinal,
		};
	}
	for (index = 0; index < ARRAY_COUNT(advsimd_load_store_registry_operations);
	     index++) {
		struct advsimd_load_store_registry_operation *operation =
			&advsimd_load_store_registry_operations[index];

		if (!operation->binding_count)
			return false;
		proof_registry_entries[entry_base + index] =
			(struct orlix_tcti_target_proof_registry_entry) {
				.id = operation->proof_id,
				.operation_id = operation->operation_id,
				.classification_mask =
					ORLIX_TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0,
				.obligations = ADVSIMD_LOAD_STORE_OBLIGATIONS,
				.linux_interface =
					ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE,
				.kunit_source = ADVSIMD_LOAD_STORE_SOURCE,
				.kunit_suite = ADVSIMD_LOAD_STORE_SUITE,
				.kunit_cases = advsimd_load_store_cases,
				.kunit_case_count = ARRAY_COUNT(advsimd_load_store_cases),
				.bindings = &advsimd_load_store_registry_bindings[
					operation->binding_offset],
				.binding_count = operation->binding_count,
				.kselftest = NULL,
				.unproved_obligations =
					ADVSIMD_LOAD_STORE_OBLIGATIONS,
			};
	}
	advsimd_load_store_registry_ready = true;
	return true;
}

static bool proof_binding_matches_source_manifest(
	const struct orlix_tcti_target_proof_registry_entry *entry,
	const struct orlix_tcti_target_proof_binding *binding)
{
	const struct source_manifest_binding *source =
		source_manifest_binding(binding->source_ordinal);

	return source && !strcmp(binding->leaf_name, source->leaf_name) &&
		!strcmp(binding->mnemonic, source->mnemonic) &&
		!strcmp(entry->operation_id, source->operation_id) &&
		binding->encoding_mask == source->encoding_mask &&
		binding->encoding_pattern == source->encoding_pattern &&
		!strcmp(binding->condition_tcnd_hex,
			source->condition_tcnd_hex);
}

static size_t source_bound_proof_matches_binding(
	const struct orlix_tcti_target_proof_registry_projection_binding *projection,
	size_t projection_count,
	const struct orlix_tcti_target_proof_registry_entry *entry,
	const struct orlix_tcti_target_proof_binding *binding)
{
	size_t index;
	size_t matches = 0;

	for (index = 0; index < projection_count; index++)
		if (projection[index].source_ordinal == binding->source_ordinal &&
		    !strcmp(projection[index].proof_id, entry->id))
			matches++;
	return matches;
}

static size_t source_bound_proof_matches_registry(
	const struct orlix_tcti_target_proof_registry_entry *entries, size_t count,
	const struct orlix_tcti_target_proof_registry_projection_binding *source_bound)
{
	size_t entry_index;
	size_t matches = 0;

	for (entry_index = 0; entry_index < count; entry_index++) {
		const struct orlix_tcti_target_proof_registry_entry *entry =
			&entries[entry_index];
		size_t binding_index;

		for (binding_index = 0; binding_index < entry->binding_count;
		     binding_index++)
			if (entries[entry_index].bindings[binding_index].source_ordinal ==
					source_bound->source_ordinal &&
			    !strcmp(entry->id, source_bound->proof_id))
				matches++;
	}
	return matches;
}

static const struct orlix_tcti_target_proof_case translation_change_cases[] = {
	{ "tchange_source_and_feature_domain",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "tchange_all_source_encodings_decode_unsupported",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ "tchange_production_el0_rejection_preserves_state",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS },
};

struct source_leaf_rejection_registry_operation {
	const char *proof_id;
	unsigned int classification;
	const char *operation_id;
	const char *source;
	const char *suite;
	const struct orlix_tcti_target_proof_case *cases;
	size_t case_count;
	size_t binding_offset;
	size_t binding_count;
};

static struct source_leaf_rejection_registry_operation
source_leaf_rejection_registry_operations[] = {
	{ .proof_id = "kunit:source-leaf-udf-undefined",
	  .classification = 3 },
	{ .proof_id = "kunit:source-leaf-hvc-non-el0",
	  .classification = 2 },
	{ .proof_id = "kunit:source-leaf-smc-non-el0",
	  .classification = 2 },
	{ .proof_id = "kunit:source-leaf-dcps1-non-el0",
	  .classification = 2 },
	{ .proof_id = "kunit:source-leaf-dcps2-non-el0",
	  .classification = 2 },
	{ .proof_id = "kunit:source-leaf-dcps3-non-el0",
	  .classification = 2 },
	{ .proof_id = "kunit:source-leaf-tenter-non-el0",
	  .classification = 2 },
	{ .proof_id = "kunit:source-leaf-bc-cond-non-el0",
	  .classification = 2,
	  .source = BASE_CONDITIONAL_SOURCE,
	  .suite = BASE_CONDITIONAL_SUITE,
	  .cases = base_conditional_production_cases,
	  .case_count = ARRAY_COUNT(base_conditional_production_cases) },
	{ .proof_id = "kunit:source-leaf-cbbcc-regs-non-el0",
	  .classification = 2,
	  .source = BASE_CONDITIONAL_SOURCE,
	  .suite = BASE_CONDITIONAL_SUITE,
	  .cases = base_conditional_production_cases,
	  .case_count = ARRAY_COUNT(base_conditional_production_cases) },
	{ .proof_id = "kunit:source-leaf-cbhcc-regs-non-el0",
	  .classification = 2,
	  .source = BASE_CONDITIONAL_SOURCE,
	  .suite = BASE_CONDITIONAL_SUITE,
	  .cases = base_conditional_production_cases,
	  .case_count = ARRAY_COUNT(base_conditional_production_cases) },
	{ .proof_id = "kunit:source-leaf-cbcc-regs-non-el0",
	  .classification = 2,
	  .source = BASE_CONDITIONAL_SOURCE,
	  .suite = BASE_CONDITIONAL_SUITE,
	  .cases = base_conditional_production_cases,
	  .case_count = ARRAY_COUNT(base_conditional_production_cases) },
	{ .proof_id = "kunit:source-leaf-cbcc-imm-non-el0",
	  .classification = 2,
	  .source = BASE_CONDITIONAL_SOURCE,
	  .suite = BASE_CONDITIONAL_SUITE,
	  .cases = base_conditional_production_cases,
	  .case_count = ARRAY_COUNT(base_conditional_production_cases) },
	{ .proof_id = "kunit:source-leaf-ctz-non-el0",
	  .classification = 2,
	  .source = BASE_BITFIELD_UNARY_SOURCE,
	  .suite = BASE_BITFIELD_UNARY_SUITE,
	  .cases = base_bitfield_unary_production_cases,
	  .case_count = ARRAY_COUNT(base_bitfield_unary_production_cases) },
	{ .proof_id = "kunit:source-leaf-cnt-non-el0",
	  .classification = 2,
	  .source = BASE_BITFIELD_UNARY_SOURCE,
	  .suite = BASE_BITFIELD_UNARY_SUITE,
	  .cases = base_bitfield_unary_production_cases,
	  .case_count = ARRAY_COUNT(base_bitfield_unary_production_cases) },
	{ .proof_id = "kunit:source-leaf-abs-non-el0",
	  .classification = 2,
	  .source = BASE_BITFIELD_UNARY_SOURCE,
	  .suite = BASE_BITFIELD_UNARY_SUITE,
	  .cases = base_bitfield_unary_production_cases,
	  .case_count = ARRAY_COUNT(base_bitfield_unary_production_cases) },
	{ .proof_id = "kunit:source-leaf-luti4-non-el0",
	  .classification = 2 },
	{ .proof_id = "kunit:source-leaf-luti2-non-el0",
	  .classification = 2 },
	{ .proof_id = "kunit:source-leaf-sqrdmlah-vec-non-el0",
	  .classification = 2 },
	{ .proof_id = "kunit:source-leaf-sqrdmlsh-vec-non-el0",
	  .classification = 2 },
	{ .proof_id = "kunit:source-leaf-sqrdmlah-elt-non-el0",
	  .classification = 2 },
	{ .proof_id = "kunit:source-leaf-sqrdmlsh-elt-non-el0",
	  .classification = 2 },
	{ .proof_id = "kunit:source-leaf-sdot-vec-non-el0",
	  .classification = 2 },
	{ .proof_id = "kunit:source-leaf-usdot-vec-non-el0",
	  .classification = 2 },
	{ .proof_id = "kunit:source-leaf-udot-vec-non-el0",
	  .classification = 2 },
	{ .proof_id = "kunit:source-leaf-sdot-elt-non-el0",
	  .classification = 2 },
	{ .proof_id = "kunit:source-leaf-sudot-elt-non-el0",
	  .classification = 2 },
	{ .proof_id = "kunit:source-leaf-usdot-elt-non-el0",
	  .classification = 2 },
	{ .proof_id = "kunit:source-leaf-udot-elt-non-el0",
	  .classification = 2 },
	{ .proof_id = "kunit:source-leaf-smmla-non-el0",
	  .classification = 2 },
	{ .proof_id = "kunit:source-leaf-usmmla-non-el0",
	  .classification = 2 },
	{ .proof_id = "kunit:source-leaf-ummla-non-el0",
	  .classification = 2 },

	{ .proof_id = "kunit:source-leaf-eret-non-el0",
	  .classification = 2,
	  .source = BASE_CONTROL_FLOW_SOURCE,
	  .suite = BASE_CONTROL_FLOW_SUITE,
	  .cases = base_control_flow_production_cases,
	  .case_count = ARRAY_COUNT(base_control_flow_production_cases) },
	{ .proof_id = "kunit:source-leaf-ereta-non-el0",
	  .classification = 2,
	  .source = BASE_CONTROL_FLOW_SOURCE,
	  .suite = BASE_CONTROL_FLOW_SUITE,
	  .cases = base_control_flow_production_cases,
	  .case_count = ARRAY_COUNT(base_control_flow_production_cases) },
	{ .proof_id = "kunit:source-leaf-texit-non-el0",
	  .classification = 2,
	  .source = BASE_CONTROL_FLOW_SOURCE,
	  .suite = BASE_CONTROL_FLOW_SUITE,
	  .cases = base_control_flow_production_cases,
	  .case_count = ARRAY_COUNT(base_control_flow_production_cases) },
	{ .proof_id = "kunit:source-leaf-drps-non-el0",
	  .classification = 2,
	  .source = BASE_CONTROL_FLOW_SOURCE,
	  .suite = BASE_CONTROL_FLOW_SUITE,
	  .cases = base_control_flow_production_cases,
	  .case_count = ARRAY_COUNT(base_control_flow_production_cases) },
	{ .proof_id = "kunit:translation-change-tchangeb-reg-el0-rejection",
	  .classification = 2,
	  .source = TRANSLATION_CHANGE_SOURCE,
	  .suite = TRANSLATION_CHANGE_SUITE,
	  .cases = translation_change_cases,
	  .case_count = ARRAY_COUNT(translation_change_cases) },
	{ .proof_id = "kunit:translation-change-tchangef-reg-el0-rejection",
	  .classification = 2,
	  .source = TRANSLATION_CHANGE_SOURCE,
	  .suite = TRANSLATION_CHANGE_SUITE,
	  .cases = translation_change_cases,
	  .case_count = ARRAY_COUNT(translation_change_cases) },
	{ .proof_id = "kunit:translation-change-tchangeb-imm-el0-rejection",
	  .classification = 2,
	  .source = TRANSLATION_CHANGE_SOURCE,
	  .suite = TRANSLATION_CHANGE_SUITE,
	  .cases = translation_change_cases,
	  .case_count = ARRAY_COUNT(translation_change_cases) },
	{ .proof_id = "kunit:translation-change-tchangef-imm-el0-rejection",
	  .classification = 2,
	  .source = TRANSLATION_CHANGE_SOURCE,
	  .suite = TRANSLATION_CHANGE_SUITE,
	  .cases = translation_change_cases,
	  .case_count = ARRAY_COUNT(translation_change_cases) },
};
static struct orlix_tcti_target_proof_binding
source_leaf_rejection_registry_bindings[
	SOURCE_LEAF_REJECTION_PROOF_REGISTRY_BINDING_COUNT];
static const struct orlix_tcti_target_proof_case source_leaf_rejection_cases[] = {
	{ "orlix_tcti_source_leaf_rejections_match_pinned_tuples",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ "orlix_tcti_source_leaf_rejections_are_structured_el0_exits",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
};

static const struct orlix_tcti_target_proof_case source_leaf_undefined_cases[] = {
	{ "orlix_tcti_source_leaf_rejections_match_pinned_tuples",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ "orlix_tcti_source_leaf_rejections_are_structured_el0_exits",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
};
static bool source_leaf_rejection_registry_ready;

static struct source_leaf_rejection_registry_operation *
source_leaf_rejection_registry_operation_for(const char *proof_id)
{
	size_t index;

	for (index = 0;
	     index < ARRAY_COUNT(source_leaf_rejection_registry_operations);
	     index++)
		if (!strcmp(proof_id,
			    source_leaf_rejection_registry_operations[index].proof_id))
			return &source_leaf_rejection_registry_operations[index];
	return NULL;
}

static bool build_source_leaf_rejection_registry(void)
{
	size_t binding_offset = 0;
	size_t entry_base = ARRAY_COUNT(proof_registry_entries) -
		ORDINARY_LOAD_STORE_PROOF_REGISTRY_ENTRY_COUNT -
		ADVSIMD_LOAD_STORE_PROOF_REGISTRY_ENTRY_COUNT -
		SOURCE_LEAF_REJECTION_PROOF_REGISTRY_ENTRY_COUNT -
		PRODUCTION_CAPTURE_FAMILY_PROOF_REGISTRY_ENTRY_COUNT -
		MOPS_COPY_PROOF_REGISTRY_ENTRY_COUNT -
		BASE_ATOMIC_MISSING_PROOF_REGISTRY_ENTRY_COUNT;
	size_t index;

	if (source_leaf_rejection_registry_ready)
		return true;
	for (index = 0; index < ARRAY_COUNT(source_bound_proofs); index++) {
		struct source_leaf_rejection_registry_operation *operation =
			source_leaf_rejection_registry_operation_for(
				source_bound_proofs[index].proof_id);

		if (operation)
			operation->binding_count++;
	}
	for (index = 0;
	     index < ARRAY_COUNT(source_leaf_rejection_registry_operations);
	     index++) {
		struct source_leaf_rejection_registry_operation *operation =
			&source_leaf_rejection_registry_operations[index];

		if (!operation->binding_count ||
		    binding_offset + operation->binding_count >
		    ARRAY_COUNT(source_leaf_rejection_registry_bindings))
			return false;
		operation->binding_offset = binding_offset;
		binding_offset += operation->binding_count;
		operation->binding_count = 0;
	}
	for (index = 0; index < ARRAY_COUNT(source_bound_proofs); index++) {
		struct source_leaf_rejection_registry_operation *operation =
			source_leaf_rejection_registry_operation_for(
				source_bound_proofs[index].proof_id);
		const struct source_manifest_binding *source;
		struct orlix_tcti_target_proof_binding *binding;

		if (!operation)
			continue;
		source = source_manifest_binding(
			source_bound_proofs[index].source_ordinal);
		if (!source)
			return false;
		binding = &source_leaf_rejection_registry_bindings[
			operation->binding_offset + operation->binding_count++];
		*binding = (struct orlix_tcti_target_proof_binding) {
			.leaf_name = source->leaf_name,
			.mnemonic = source->mnemonic,
			.encoding_mask = source->encoding_mask,
			.encoding_pattern = source->encoding_pattern,
			.condition_tcnd_hex = source->condition_tcnd_hex,
			.kunit_case_mask = operation->cases ?
				((ORLIX_TCTI_PROOF_U64_C(1) << operation->case_count) - 1) :
				ORLIX_TCTI_PROOF_U64_C(0x3),
			.source_ordinal = source->ordinal,
		};
		if (operation->operation_id &&
		    strcmp(operation->operation_id, source->operation_id))
			return false;
		operation->operation_id = source->operation_id;
	}
	for (index = 0;
	     index < ARRAY_COUNT(source_leaf_rejection_registry_operations);
	     index++) {
		const struct source_leaf_rejection_registry_operation *operation =
			&source_leaf_rejection_registry_operations[index];
		orlix_tcti_proof_u32 obligations = operation->classification == 2 ?
			NON_EL0_REJECTION_OBLIGATIONS :
			UNDEFINED_REJECTION_OBLIGATIONS;

		if (!operation->binding_count || !operation->operation_id)
			return false;
		proof_registry_entries[entry_base + index] =
			(struct orlix_tcti_target_proof_registry_entry) {
				.id = operation->proof_id,
				.operation_id = operation->operation_id,
				.classification_mask = 1U << operation->classification,
				.obligations = obligations,
				.linux_interface =
					ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE,
				.kunit_source = operation->source ? operation->source :
					SOURCE_LEAF_CLASSIFICATION_SOURCE,
				.kunit_suite = operation->suite ? operation->suite :
					SOURCE_LEAF_CLASSIFICATION_SUITE,
			.kunit_cases = operation->cases ? operation->cases :
				operation->classification == 2 ?
				source_leaf_rejection_cases :
				source_leaf_undefined_cases,
			.kunit_case_count = operation->cases ? operation->case_count :
				operation->classification == 2 ?
				ARRAY_COUNT(source_leaf_rejection_cases) :
				ARRAY_COUNT(source_leaf_undefined_cases),
				.bindings = &source_leaf_rejection_registry_bindings[
					operation->binding_offset],
				.binding_count = operation->binding_count,
				.kselftest = NULL,
				.unproved_obligations = obligations,
			};
	}
	source_leaf_rejection_registry_ready = true;
	return true;
}

static struct orlix_tcti_target_proof_binding
production_capture_family_registry_bindings[
	PRODUCTION_CAPTURE_FAMILY_PROOF_REGISTRY_BINDING_COUNT ? 
	PRODUCTION_CAPTURE_FAMILY_PROOF_REGISTRY_BINDING_COUNT : 1];
/* Private, static descriptors are the registration capability. */
struct production_capture_family_descriptor {
	const char *kunit_source;
	const char *kunit_source_sha256;
	const char *kunit_object;
	const char *kunit_suite;
	const char *kunit_suite_symbol;
	const char *kunit_case_array;
	const struct orlix_tcti_target_proof_case *kunit_cases;
	size_t kunit_case_count;
	const struct orlix_tcti_target_kselftest_provenance *kselftest;
};

struct production_capture_operation_descriptor {
	const char *proof_id;
	const char *operation_id;
	orlix_tcti_proof_u32 obligations;
	enum orlix_tcti_target_proof_linux_interface linux_interface;
};
static const struct orlix_tcti_target_proof_case
production_capture_family_cases[] = {
#define ORLIX_TCTI_PROOF_FAMILY_METADATA(source_value, source_sha256_value, \
		object_value, suite_value, suite_symbol_value, case_array_value, \
		decode_case_value, production_case_value) \
	{ decode_case_value, ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE | \
		ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS }, \
	{ production_case_value, ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS | \
		ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
#include "target_production_capture_family.def"
#undef ORLIX_TCTI_PROOF_FAMILY_METADATA
};
static const struct production_capture_family_descriptor
production_capture_families[] = {
#define ORLIX_TCTI_PROOF_FAMILY_METADATA(source_value, source_sha256_value, \
		object_value, suite_value, suite_symbol_value, case_array_value, \
		decode_case_value, production_case_value) \
	{ \
		.kunit_source = source_value, \
		.kunit_source_sha256 = source_sha256_value, \
		.kunit_object = object_value, \
		.kunit_suite = suite_value, \
		.kunit_suite_symbol = suite_symbol_value, \
		.kunit_case_array = case_array_value, \
		.kunit_cases = production_capture_family_cases, \
		.kunit_case_count = 2U, \
		.kselftest = &exception_interface_kselftest, \
	},
#include "target_production_capture_family.def"
#undef ORLIX_TCTI_PROOF_FAMILY_METADATA
};
static const struct production_capture_operation_descriptor
production_capture_family_operations[] = {
#define ORLIX_TCTI_PROOF_FAMILY_OBLIGATIONS_REQUIRED \
	(PRODUCTION_CAPTURE_FAMILY_OBLIGATIONS | \
	 ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LINUX_INTERFACE)
#define ORLIX_TCTI_PROOF_FAMILY_OBLIGATIONS_NOT_APPLICABLE \
	PRODUCTION_CAPTURE_FAMILY_OBLIGATIONS
#define ORLIX_TCTI_PROOF_FAMILY_OPERATION(proof_id_value, operation_value, linux_value, obligations_value) \
	{ proof_id_value, operation_value, obligations_value, \
	  ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_##linux_value },
#include "target_production_capture_family.def"
	{ "", "", 0, 0 },
#undef ORLIX_TCTI_PROOF_FAMILY_OPERATION
#undef ORLIX_TCTI_PROOF_FAMILY_OBLIGATIONS_NOT_APPLICABLE
#undef ORLIX_TCTI_PROOF_FAMILY_OBLIGATIONS_REQUIRED
};
static bool production_capture_family_registry_ready;

static int production_capture_family_build(
	const struct production_capture_family_descriptor *family,
	const struct production_capture_operation_descriptor *operations,
	size_t operation_count,
	struct orlix_tcti_target_proof_registry_entry *entries,
	size_t entry_count,
	struct orlix_tcti_target_proof_binding *bindings,
	size_t binding_count);

static bool build_production_capture_family_registry(void)
{
	size_t entry_base = ARRAY_COUNT(proof_registry_entries) -
		PRODUCTION_CAPTURE_FAMILY_PROOF_REGISTRY_ENTRY_COUNT -
		MOPS_COPY_PROOF_REGISTRY_ENTRY_COUNT -
		BASE_ATOMIC_MISSING_PROOF_REGISTRY_ENTRY_COUNT;

	if (production_capture_family_registry_ready)
		return true;
	if (!PRODUCTION_CAPTURE_FAMILY_PROOF_REGISTRY_ENTRY_COUNT) {
		production_capture_family_registry_ready = true;
		return true;
	}
	if (!ARRAY_COUNT(production_capture_families))
		return false;
	/*
	 * Families share one operation table today. A later family adds its
	 * own METADATA row here. Bind that family's operations before calling
	 * production_capture_family_build for that row.
	 */
	if (production_capture_family_build(
			&production_capture_families[0],
			production_capture_family_operations,
			ARRAY_COUNT(production_capture_family_operations),
			&proof_registry_entries[entry_base],
			PRODUCTION_CAPTURE_FAMILY_PROOF_REGISTRY_ENTRY_COUNT,
			production_capture_family_registry_bindings,
			ARRAY_COUNT(production_capture_family_registry_bindings)))
		return false;
	production_capture_family_registry_ready = true;
	return true;
}

struct mops_copy_registry_operation {
	const char *proof_id;
	const char *operation_id;
	size_t binding_offset;
	size_t binding_count;
};

#define MOPS_COPY_OPERATION(operation, slug) \
	{ .proof_id = "kunit:mops-copy-" slug, .operation_id = operation }
static struct mops_copy_registry_operation mops_copy_registry_operations[] = {
	MOPS_COPY_OPERATION("CPYFP", "cpyfp"),
	MOPS_COPY_OPERATION("CPYFPWT", "cpyfpwt"),
	MOPS_COPY_OPERATION("CPYFPRT", "cpyfprt"),
	MOPS_COPY_OPERATION("CPYFPT", "cpyfpt"),
	MOPS_COPY_OPERATION("CPYFPWN", "cpyfpwn"),
	MOPS_COPY_OPERATION("CPYFPWTWN", "cpyfpwtwn"),
	MOPS_COPY_OPERATION("CPYFPRTWN", "cpyfprtwn"),
	MOPS_COPY_OPERATION("CPYFPTWN", "cpyfptwn"),
	MOPS_COPY_OPERATION("CPYFPRN", "cpyfprn"),
	MOPS_COPY_OPERATION("CPYFPWTRN", "cpyfpwtrn"),
	MOPS_COPY_OPERATION("CPYFPRTRN", "cpyfprtrn"),
	MOPS_COPY_OPERATION("CPYFPTRN", "cpyfptrn"),
	MOPS_COPY_OPERATION("CPYFPN", "cpyfpn"),
	MOPS_COPY_OPERATION("CPYFPWTN", "cpyfpwtn"),
	MOPS_COPY_OPERATION("CPYFPRTN", "cpyfprtn"),
	MOPS_COPY_OPERATION("CPYFPTN", "cpyfptn"),
	MOPS_COPY_OPERATION("CPYP", "cpyp"),
	MOPS_COPY_OPERATION("CPYPWT", "cpypwt"),
	MOPS_COPY_OPERATION("CPYPRT", "cpyprt"),
	MOPS_COPY_OPERATION("CPYPT", "cpypt"),
	MOPS_COPY_OPERATION("CPYPWN", "cpypwn"),
	MOPS_COPY_OPERATION("CPYPWTWN", "cpypwtwn"),
	MOPS_COPY_OPERATION("CPYPRTWN", "cpyprtwn"),
	MOPS_COPY_OPERATION("CPYPTWN", "cpyptwn"),
	MOPS_COPY_OPERATION("CPYPRN", "cpyprn"),
	MOPS_COPY_OPERATION("CPYPWTRN", "cpypwtrn"),
	MOPS_COPY_OPERATION("CPYPRTRN", "cpyprtrn"),
	MOPS_COPY_OPERATION("CPYPTRN", "cpyptrn"),
	MOPS_COPY_OPERATION("CPYPN", "cpypn"),
	MOPS_COPY_OPERATION("CPYPWTN", "cpypwtn"),
	MOPS_COPY_OPERATION("CPYPRTN", "cpyprtn"),
	MOPS_COPY_OPERATION("CPYPTN", "cpyptn"),
};
#undef MOPS_COPY_OPERATION

static struct orlix_tcti_target_proof_binding mops_copy_registry_bindings[
	MOPS_COPY_PROOF_REGISTRY_BINDING_COUNT];
static const struct orlix_tcti_target_proof_case mops_copy_cases[] = {
	{ "mops_decode_all_96_source_leaves",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "mops_rejects_reserved_and_constrained_unpredictable_forms",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS },
	{ "mops_production_gadget_forwards_and_writes_back",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "mops_production_gadget_selects_backward_overlap_direction",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "mops_production_gadget_executes_all_options",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "mops_production_rejects_constrained_register_forms",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "mops_epilogue_preserves_partial_guest_progress_on_fault",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS },
	{ "mops_epilogue_preserves_destination_fault_progress",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS },
	{ "mops_epilogue_preserves_backward_fault_progress",
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS },
};
static bool mops_copy_registry_ready;

static struct mops_copy_registry_operation *
mops_copy_registry_operation_for(const char *proof_id)
{
	size_t index;

	for (index = 0; index < ARRAY_COUNT(mops_copy_registry_operations); index++)
		if (!strcmp(proof_id, mops_copy_registry_operations[index].proof_id))
			return &mops_copy_registry_operations[index];
	return NULL;
}

static bool build_mops_copy_registry(void)
{
	size_t binding_offset = 0;
	size_t entry_base = ARRAY_COUNT(proof_registry_entries) -
		MOPS_COPY_PROOF_REGISTRY_ENTRY_COUNT -
		BASE_ATOMIC_MISSING_PROOF_REGISTRY_ENTRY_COUNT;
	size_t index;

	if (mops_copy_registry_ready)
		return true;
	for (index = 0; index < ARRAY_COUNT(source_bound_proofs); index++) {
		struct mops_copy_registry_operation *operation =
			mops_copy_registry_operation_for(source_bound_proofs[index].proof_id);

		if (operation)
			operation->binding_count++;
	}
	for (index = 0; index < ARRAY_COUNT(mops_copy_registry_operations); index++) {
		struct mops_copy_registry_operation *operation =
			&mops_copy_registry_operations[index];

		if (operation->binding_count != 3U ||
		    binding_offset + operation->binding_count >
			ARRAY_COUNT(mops_copy_registry_bindings))
			return false;
		operation->binding_offset = binding_offset;
		binding_offset += operation->binding_count;
		operation->binding_count = 0;
	}
	if (binding_offset != ARRAY_COUNT(mops_copy_registry_bindings))
		return false;
	for (index = 0; index < ARRAY_COUNT(source_bound_proofs); index++) {
		struct mops_copy_registry_operation *operation =
			mops_copy_registry_operation_for(source_bound_proofs[index].proof_id);
		const struct source_manifest_binding *source;
		struct orlix_tcti_target_proof_binding *binding;

		if (!operation)
			continue;
		source = source_manifest_binding(
			source_bound_proofs[index].source_ordinal);
		if (!source || strcmp(operation->operation_id, source->operation_id))
			return false;
		binding = &mops_copy_registry_bindings[
			operation->binding_offset + operation->binding_count++];
		*binding = (struct orlix_tcti_target_proof_binding) {
			.leaf_name = source->leaf_name,
			.mnemonic = source->mnemonic,
			.encoding_mask = source->encoding_mask,
			.encoding_pattern = source->encoding_pattern,
			.condition_tcnd_hex = source->condition_tcnd_hex,
			.kunit_case_mask =
				(ORLIX_TCTI_PROOF_U64_C(1) << ARRAY_COUNT(mops_copy_cases)) - 1,
			.source_ordinal = source->ordinal,
		};
	}
	for (index = 0; index < ARRAY_COUNT(mops_copy_registry_operations); index++) {
		const struct mops_copy_registry_operation *operation =
			&mops_copy_registry_operations[index];

		if (operation->binding_count != 3U)
			return false;
		proof_registry_entries[entry_base + index] =
			(struct orlix_tcti_target_proof_registry_entry) {
				.id = operation->proof_id,
				.operation_id = operation->operation_id,
				.classification_mask =
					ORLIX_TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0,
				.obligations = MOPS_COPY_OBLIGATIONS,
				.linux_interface =
					ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE,
				.kunit_source = MOPS_COPY_SOURCE,
				.kunit_suite = MOPS_COPY_SUITE,
				.kunit_cases = mops_copy_cases,
				.kunit_case_count = ARRAY_COUNT(mops_copy_cases),
				.bindings = &mops_copy_registry_bindings[
					operation->binding_offset],
				.binding_count = operation->binding_count,
				.kselftest = NULL,
				.unproved_obligations = MOPS_COPY_OBLIGATIONS,
			};
	}
	mops_copy_registry_ready = true;
	return true;
}

static struct orlix_tcti_target_proof_binding base_atomic_missing_bindings[
	BASE_ATOMIC_MISSING_PROOF_REGISTRY_ENTRY_COUNT];
static bool base_atomic_missing_registry_ready;

static bool base_atomic_missing_proof_id(const char *proof_id)
{
	static const char prefix[] = "kunit:base-atomic-leaf-";

	return proof_id && !strncmp(proof_id, prefix, sizeof(prefix) - 1U);
}

static bool base_atomic_is_ordered_access(const char *operation_id)
{
	return !strncmp(operation_id, "LDAR", 4) ||
		!strncmp(operation_id, "LDAP", 4) ||
		!strncmp(operation_id, "LDLAR", 5) ||
		!strncmp(operation_id, "STLR", 4) ||
		!strncmp(operation_id, "STLUR", 5) ||
		!strncmp(operation_id, "STLLR", 5) ||
		!strncmp(operation_id, "LDIAPP", 6) ||
		!strncmp(operation_id, "STILP", 5) ||
		!strncmp(operation_id, "STLP", 4);
}

static orlix_tcti_proof_u32 base_atomic_obligations(const char *operation_id)
{

	/* Unchanged NZCV/FP status is itself an observed per-leaf obligation. */
	return base_atomic_is_ordered_access(operation_id) ?
		BASE_ATOMIC_FLAGS_REQUIRED_OBLIGATIONS &
			~ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ATOMICITY :
		BASE_ATOMIC_FLAGS_REQUIRED_OBLIGATIONS;
}

enum base_atomic_case_index {
	BASE_ATOMIC_CASE_COHORT,
	BASE_ATOMIC_CASE_DDI0602,
	BASE_ATOMIC_CASE_REGISTER_VARIANTS,
	BASE_ATOMIC_CASE_RESERVED_REGISTERS,
	BASE_ATOMIC_CASE_FIXED_NEIGHBOURS,
	BASE_ATOMIC_CASE_ORDERED_SHAPES,
	BASE_ATOMIC_CASE_EXECUTE,
	BASE_ATOMIC_CASE_FP_EXECUTE,
	BASE_ATOMIC_CASE_RCW,
	BASE_ATOMIC_CASE_LS64,
	BASE_ATOMIC_CASE_FAULTS,
	BASE_ATOMIC_CASE_ATOMICITY,
	BASE_ATOMIC_CASE_EXCLUSIVE_MONITOR,
	BASE_ATOMIC_CASE_ORDERING,
	BASE_ATOMIC_CASE_PRODUCTION_RESUME,
	BASE_ATOMIC_CASE_UNMAPPED_FAULTS,
	BASE_ATOMIC_CASE_MONITOR_MISMATCH,
	BASE_ATOMIC_CASE_CASP,
	BASE_ATOMIC_CASE_CAS_RMW,
	BASE_ATOMIC_CASE_PRODUCTION_ATOMICITY,
};

static orlix_tcti_proof_u64 base_atomic_case_mask(
	const struct source_manifest_binding *source)
{
	const char *operation = source->operation_id;
	bool ordered_access = base_atomic_is_ordered_access(operation);
	orlix_tcti_proof_u64 mask =
		ORLIX_TCTI_PROOF_U64_C(1) << BASE_ATOMIC_CASE_COHORT |
		ORLIX_TCTI_PROOF_U64_C(1) << BASE_ATOMIC_CASE_DDI0602 |
		ORLIX_TCTI_PROOF_U64_C(1) << BASE_ATOMIC_CASE_REGISTER_VARIANTS |
		ORLIX_TCTI_PROOF_U64_C(1) << BASE_ATOMIC_CASE_FIXED_NEIGHBOURS |
		ORLIX_TCTI_PROOF_U64_C(1) << BASE_ATOMIC_CASE_EXECUTE |
		ORLIX_TCTI_PROOF_U64_C(1) << BASE_ATOMIC_CASE_FAULTS;

	if (strstr(operation, "CASP") || !strcmp(operation, "LD64B") ||
	    !strncmp(operation, "ST64B", 5))
		mask |= ORLIX_TCTI_PROOF_U64_C(1) <<
			BASE_ATOMIC_CASE_RESERVED_REGISTERS;
	if (!strncmp(operation, "RCW", 3))
		mask |= ORLIX_TCTI_PROOF_U64_C(1) << BASE_ATOMIC_CASE_RCW;
	if (!strcmp(operation, "LD64B") || !strncmp(operation, "ST64B", 5))
		mask |= ORLIX_TCTI_PROOF_U64_C(1) << BASE_ATOMIC_CASE_LS64;
	if (!strncmp(operation, "LDF", 3) || !strncmp(operation, "STF", 3) ||
	    !strncmp(operation, "LDBF", 4) || !strncmp(operation, "STBF", 4))
		mask |= ORLIX_TCTI_PROOF_U64_C(1) << BASE_ATOMIC_CASE_FP_EXECUTE;
	if (!ordered_access)
		mask |= ORLIX_TCTI_PROOF_U64_C(1) << BASE_ATOMIC_CASE_ATOMICITY;
	if (ordered_access) {
		mask |= ORLIX_TCTI_PROOF_U64_C(1) << BASE_ATOMIC_CASE_ORDERED_SHAPES;
		/* The ordered-only registry projects ordering at case index 11. */
		mask |= ORLIX_TCTI_PROOF_U64_C(1) << BASE_ATOMIC_CASE_ATOMICITY;
		/* production_resume and unmapped_faults follow that 12-entry prefix. */
		mask |= ORLIX_TCTI_PROOF_U64_C(1) << 12;
		mask |= ORLIX_TCTI_PROOF_U64_C(1) << 13;
	} else {
		mask |= ORLIX_TCTI_PROOF_U64_C(1) << BASE_ATOMIC_CASE_ORDERING;
		mask |= ORLIX_TCTI_PROOF_U64_C(1) << BASE_ATOMIC_CASE_PRODUCTION_RESUME;
		mask |= ORLIX_TCTI_PROOF_U64_C(1) << BASE_ATOMIC_CASE_UNMAPPED_FAULTS;
		mask |= ORLIX_TCTI_PROOF_U64_C(1) << BASE_ATOMIC_CASE_PRODUCTION_ATOMICITY;
	}
	return mask;
}

static bool base_atomic_missing_operation(const char *operation_id)
{
	size_t index;

	for (index = 0; index < ARRAY_COUNT(source_bound_proofs); index++) {
		const struct source_manifest_binding *source;

		if (!base_atomic_missing_proof_id(source_bound_proofs[index].proof_id))
			continue;
		source = source_manifest_binding(source_bound_proofs[index].source_ordinal);
		if (source && !strcmp(source->operation_id, operation_id))
			return true;
	}
	return false;
}

static bool build_base_atomic_missing_registry(void)
{
	const size_t entry_base = ARRAY_COUNT(proof_registry_entries) -
		BASE_ATOMIC_MISSING_PROOF_REGISTRY_ENTRY_COUNT;
	size_t count = 0;
	size_t index;

	if (base_atomic_missing_registry_ready)
		return true;
	for (index = 0; index < ARRAY_COUNT(source_bound_proofs); index++) {
		const struct source_manifest_binding *source;
		struct orlix_tcti_target_proof_binding *binding;
		struct orlix_tcti_target_proof_registry_entry *entry;
		orlix_tcti_proof_u32 obligations;

		if (!base_atomic_missing_proof_id(source_bound_proofs[index].proof_id))
			continue;
		if (count >= ARRAY_COUNT(base_atomic_missing_bindings))
			return false;
		source = source_manifest_binding(source_bound_proofs[index].source_ordinal);
		if (!source)
			return false;
		obligations = base_atomic_obligations(source->operation_id);
		binding = &base_atomic_missing_bindings[count];
		*binding = (struct orlix_tcti_target_proof_binding) {
			.leaf_name = source->leaf_name,
			.mnemonic = source->mnemonic,
			.encoding_mask = source->encoding_mask,
			.encoding_pattern = source->encoding_pattern,
			.condition_tcnd_hex = source->condition_tcnd_hex,
			.kunit_case_mask = base_atomic_case_mask(source),
			.source_ordinal = source->ordinal,
		};
		entry = &proof_registry_entries[entry_base + count];
		*entry = (struct orlix_tcti_target_proof_registry_entry) {
			.id = source_bound_proofs[index].proof_id,
			.operation_id = source->operation_id,
			.classification_mask =
				ORLIX_TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0,
			.obligations = obligations,
			.linux_interface =
				ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE,
			.kunit_source = BASE_ATOMIC_SOURCE,
			.kunit_suite = BASE_ATOMIC_SUITE,
			.kunit_cases = base_atomic_is_ordered_access(
				source->operation_id) ? base_ordered_source_bound_cases :
				base_atomic_source_bound_cases,
			.kunit_case_count = base_atomic_is_ordered_access(
				source->operation_id) ?
				ARRAY_COUNT(base_ordered_source_bound_cases) :
				ARRAY_COUNT(base_atomic_source_bound_cases),
			.bindings = binding,
			.binding_count = 1,
			.kselftest = NULL,
			.unproved_obligations = obligations,
		};
		count++;
	}
	if (count != ARRAY_COUNT(base_atomic_missing_bindings))
		return false;
	base_atomic_missing_registry_ready = true;
	return true;
}

int orlix_tcti_target_proof_operation_requirements(
	const char *operation_id, unsigned int classification,
	orlix_tcti_proof_u32 *requirements)
{
	size_t index;

	if (empty(operation_id) || !requirements)
		return -1;
	if (classification == 2) {
		*requirements = NON_EL0_REJECTION_OBLIGATIONS;
		return 0;
	}
	if (classification == 3) {
		*requirements = UNDEFINED_REJECTION_OBLIGATIONS;
		return 0;
	}
	if (classification != 1)
		return -1;
	for (index = 0; index < sizeof(operation_requirements) /
				      sizeof(operation_requirements[0]); index++)
		if (!strcmp(operation_id,
			    operation_requirements[index].operation_id)) {
			*requirements = operation_requirements[index].obligations;
			return 0;
		}
	if (base_atomic_missing_operation(operation_id)) {
		*requirements = base_atomic_obligations(operation_id);
		return 0;
	}
	return -1;
}

struct sha256_state {
	orlix_tcti_proof_u32 hash[8];
	orlix_tcti_proof_u64 bytes;
	orlix_tcti_proof_u8 block[64];
	size_t used;
};

static orlix_tcti_proof_u32 rotate_right(orlix_tcti_proof_u32 value, unsigned int shift)
{
	return (value >> shift) | (value << (32 - shift));
}

static void sha256_transform(struct sha256_state *state, const orlix_tcti_proof_u8 *block)
{
	static const orlix_tcti_proof_u32 constants[64] = {
		0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
		0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
		0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
		0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
		0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
		0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
		0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
		0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
		0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
		0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
		0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
		0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
		0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
		0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
		0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
		0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
	};
	orlix_tcti_proof_u32 words[64];
	orlix_tcti_proof_u32 a, b, c, d, e, f, g, h;
	size_t index;

	for (index = 0; index < 16; index++)
		words[index] = ((orlix_tcti_proof_u32)block[index * 4] << 24) |
			((orlix_tcti_proof_u32)block[index * 4 + 1] << 16) |
			((orlix_tcti_proof_u32)block[index * 4 + 2] << 8) |
			block[index * 4 + 3];
	for (index = 16; index < 64; index++) {
		orlix_tcti_proof_u32 s0 = rotate_right(words[index - 15], 7) ^
			rotate_right(words[index - 15], 18) ^
			(words[index - 15] >> 3);
		orlix_tcti_proof_u32 s1 = rotate_right(words[index - 2], 17) ^
			rotate_right(words[index - 2], 19) ^
			(words[index - 2] >> 10);

		words[index] = words[index - 16] + s0 + words[index - 7] + s1;
	}
	a = state->hash[0]; b = state->hash[1]; c = state->hash[2];
	d = state->hash[3]; e = state->hash[4]; f = state->hash[5];
	g = state->hash[6]; h = state->hash[7];
	for (index = 0; index < 64; index++) {
		orlix_tcti_proof_u32 sum1 = rotate_right(e, 6) ^ rotate_right(e, 11) ^
			rotate_right(e, 25);
		orlix_tcti_proof_u32 choice = (e & f) ^ (~e & g);
		orlix_tcti_proof_u32 temporary1 = h + sum1 + choice + constants[index] +
			words[index];
		orlix_tcti_proof_u32 sum0 = rotate_right(a, 2) ^ rotate_right(a, 13) ^
			rotate_right(a, 22);
		orlix_tcti_proof_u32 majority = (a & b) ^ (a & c) ^ (b & c);
		orlix_tcti_proof_u32 temporary2 = sum0 + majority;

		h = g; g = f; f = e; e = d + temporary1;
		d = c; c = b; b = a; a = temporary1 + temporary2;
	}
	state->hash[0] += a; state->hash[1] += b; state->hash[2] += c;
	state->hash[3] += d; state->hash[4] += e; state->hash[5] += f;
	state->hash[6] += g; state->hash[7] += h;
}

static void sha256_update(struct sha256_state *state, const orlix_tcti_proof_u8 *data,
			  size_t length)
{
	state->bytes += length;
	while (length) {
		size_t available = sizeof(state->block) - state->used;
		size_t copied = length < available ? length : available;

		memcpy(state->block + state->used, data, copied);
		state->used += copied;
		data += copied;
		length -= copied;
		if (state->used == sizeof(state->block)) {
			sha256_transform(state, state->block);
			state->used = 0;
		}
	}
}

static void sha256_digest(const orlix_tcti_proof_u8 *data, size_t length, orlix_tcti_proof_u8 out[32])
{
	struct sha256_state state = {
		.hash = { 0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U,
			  0xa54ff53aU, 0x510e527fU, 0x9b05688cU,
			  0x1f83d9abU, 0x5be0cd19U },
	};
	orlix_tcti_proof_u64 bits;
	size_t index;

	sha256_update(&state, data, length);
	bits = state.bytes * 8;
	state.block[state.used++] = 0x80;
	if (state.used > 56) {
		memset(state.block + state.used, 0, sizeof(state.block) - state.used);
		sha256_transform(&state, state.block);
		state.used = 0;
	}
	memset(state.block + state.used, 0, 56 - state.used);
	for (index = 0; index < 8; index++)
		state.block[63 - index] = (orlix_tcti_proof_u8)(bits >> (index * 8));
	sha256_transform(&state, state.block);
	for (index = 0; index < 8; index++) {
		out[index * 4] = (orlix_tcti_proof_u8)(state.hash[index] >> 24);
		out[index * 4 + 1] = (orlix_tcti_proof_u8)(state.hash[index] >> 16);
		out[index * 4 + 2] = (orlix_tcti_proof_u8)(state.hash[index] >> 8);
		out[index * 4 + 3] = (orlix_tcti_proof_u8)state.hash[index];
	}
}

#ifndef __KERNEL__
static bool sha256_matches(const orlix_tcti_proof_u8 *data, size_t length, const char *hex)
{
	static const char digits[] = "0123456789abcdef";
	orlix_tcti_proof_u8 digest[32];
	size_t index;

	if (!hex || strlen(hex) != 64)
		return false;
	sha256_digest(data, length, digest);
	for (index = 0; index < sizeof(digest); index++)
		if (hex[index * 2] != digits[digest[index] >> 4] ||
		    hex[index * 2 + 1] != digits[digest[index] & 0xf])
			return false;
	return true;
}
#endif

int orlix_tcti_target_proof_source_size_allowed(orlix_tcti_proof_u64 size)
{
	return size < (orlix_tcti_proof_u64)SIZE_MAX;
}

#ifndef __KERNEL__
static char *read_file_limited(const char *path, size_t *length, size_t maximum_size)
{
	FILE *file;
	char *data;
	long size;

	file = fopen(path, "rb");
	if (!length || !file || fseek(file, 0, SEEK_END) ||
	    (size = ftell(file)) < 0 ||
	    (unsigned long long)size > maximum_size ||
	    fseek(file, 0, SEEK_SET))
		goto fail;
	data = malloc((size_t)size + 1);
	if (!data || fread(data, 1, (size_t)size, file) != (size_t)size) {
		free(data);
		goto fail;
	}
	fclose(file);
	data[size] = '\0';
	*length = (size_t)size;
	return data;
fail:
	if (file)
		fclose(file);
	return NULL;
}

static char *read_source(const char *path, size_t *length)
{
	return read_file_limited(path, length, ORLIX_TCTI_TARGET_PROOF_MAX_SOURCE_BYTES);
}

static bool manifest_artifact_digest(const char *manifest, size_t manifest_length,
	const char *artifact_name, char digest[65], size_t *artifact_length)
{
	const char *line = manifest;
	const char *manifest_end = manifest + manifest_length;
	size_t name_length;
	size_t matches = 0;

	if (empty(artifact_name))
		return false;
	name_length = strlen(artifact_name);
	while (line < manifest_end) {
		const char *line_end = memchr(line, '\n', manifest_end - line);
		const char *cursor;
		size_t length = 0;
		size_t index;

		if (!line_end)
			line_end = manifest_end;
		if ((size_t)(line_end - line) <= strlen("artifact=") + name_length ||
		    strncmp(line, "artifact=", strlen("artifact=")) ||
		    strncmp(line + strlen("artifact="), artifact_name, name_length) ||
		    line[strlen("artifact=") + name_length] != ' ') {

			line = line_end < manifest_end ? line_end + 1 : manifest_end;
			continue;
		}
		cursor = line + strlen("artifact=") + name_length + 1;
		if (cursor == line_end)
			return false;
		while (cursor < line_end && isdigit((unsigned char)*cursor)) {
			if (length > (SIZE_MAX - (size_t)(*cursor - '0')) / 10U)
				return false;
			length = length * 10U + (size_t)(*cursor - '0');
			cursor++;
		}
		if (cursor == line + strlen("artifact=") + name_length + 1 ||
		    (size_t)(line_end - cursor) < strlen(" sha256=") + 64U ||
		    strncmp(cursor, " sha256=", strlen(" sha256=")))
			return false;
		cursor += strlen(" sha256=");
		for (index = 0; index < 64U; index++)
			if (!isxdigit((unsigned char)cursor[index]) ||
			    isupper((unsigned char)cursor[index]))
				return false;
		if (cursor + 64U < line_end &&
		    !isspace((unsigned char)cursor[64]))
			return false;
		if (++matches != 1U)
			return false;
		memcpy(digest, cursor, 64U);
		digest[64] = '\0';
		*artifact_length = length;
		line = line_end < manifest_end ? line_end + 1 : manifest_end;
	}
	return matches == 1U;
}

static int valid_kunit_manifest_terminal_artifact(
	const char *manifest_path, const char *artifact_name, const char *artifact_path)
{
	char *manifest;
	char *artifact;
	char digest[65];
	size_t manifest_length;
	size_t artifact_length;
	size_t expected_length;
	int result = -1;

	if (empty(manifest_path) || empty(artifact_name) || empty(artifact_path))
		return -1;
	manifest = read_source(manifest_path, &manifest_length);
	if (!manifest || memchr(manifest, '\0', manifest_length) ||
	    !manifest_artifact_digest(manifest, manifest_length, artifact_name, digest,
				      &expected_length))
		goto out_manifest;
	artifact = read_file_limited(artifact_path, &artifact_length,
		ORLIX_TCTI_TARGET_PROOF_MAX_SEMANTIC_PROVENANCE_ARTIFACT_BYTES);
	if (artifact && !memchr(artifact, '\0', artifact_length) &&
	    artifact_length == expected_length &&
	    sha256_matches((const orlix_tcti_proof_u8 *)artifact, artifact_length,
			   digest))
		result = 0;
	free(artifact);
out_manifest:
	free(manifest);
	return result;
}

static bool source_registers_case(const char *source, const char *case_name)
{
	const char *cursor = source;
	size_t length = strlen(case_name);

	while ((cursor = strstr(cursor, "KUNIT_CASE"))) {
		cursor += strlen("KUNIT_CASE");
		while (isspace((unsigned char)*cursor))
			cursor++;
		if (*cursor++ != '(')
			continue;
		while (isspace((unsigned char)*cursor))
			cursor++;
		if (!strncmp(cursor, case_name, length) &&
		    (cursor[length] == ')' ||
		     isspace((unsigned char)cursor[length])))
			return true;
	}
	return false;
}

static bool source_registers_suite(const char *source, const char *suite)
{
	const char *cursor = source;
	size_t length = strlen(suite);

	while ((cursor = strstr(cursor, ".name"))) {
		cursor += strlen(".name");
		while (isspace((unsigned char)*cursor))
			cursor++;
		if (*cursor++ != '=')
			continue;
		while (isspace((unsigned char)*cursor))
			cursor++;
		if (*cursor++ != '\"')
			continue;
		if (!strncmp(cursor, suite, length) && cursor[length] == '\"')
			return true;
	}
	return false;
}

static bool source_connects_case_to_suite(
	const char *source, const struct kunit_case_provenance *provenance)
{
	char declaration[160];
	char name_assignment[160];
	char cases_assignment[160];
	char registration[160];
	char array_declaration[160];
	char case_registration[192];
	const char *array;
	const char *array_end;
	const char *registered_case;

	if (snprintf(declaration, sizeof(declaration), "struct kunit_suite %s",
		     provenance->suite_symbol) >= (int)sizeof(declaration) ||
	    snprintf(name_assignment, sizeof(name_assignment), ".name = \"%s\"",
		     provenance->suite) >= (int)sizeof(name_assignment) ||
	    snprintf(cases_assignment, sizeof(cases_assignment),
		     ".test_cases = %s", provenance->case_array) >=
		     (int)sizeof(cases_assignment) ||
	    snprintf(registration, sizeof(registration), "kunit_test_suite(%s)",
		     provenance->suite_symbol) >= (int)sizeof(registration) ||
	    snprintf(array_declaration, sizeof(array_declaration),
		     "struct kunit_case %s[] = {", provenance->case_array) >=
		     (int)sizeof(array_declaration) ||
	    snprintf(case_registration, sizeof(case_registration), "KUNIT_CASE(%s)",
		     provenance->name) >= (int)sizeof(case_registration))
		return false;
	if (!strstr(source, declaration) || !strstr(source, name_assignment) ||
	    !strstr(source, cases_assignment) || !strstr(source, registration))
		return false;
	array = strstr(source, array_declaration);
	if (!array || !(array_end = strstr(array, "};")))
		return false;
	registered_case = strstr(array, case_registration);
	return registered_case && registered_case < array_end;
}
#endif

static const struct kunit_source_provenance *
find_kunit_source(const char *path)
{
	size_t index;

	for (index = 0; index < sizeof(kunit_sources) /
				      sizeof(kunit_sources[0]); index++)
		if (!strcmp(path, kunit_sources[index].source))
			return &kunit_sources[index];
	return NULL;
}

static const struct kunit_dependency_terminal_artifact *
find_kunit_dependency_terminal_artifact(const char *source)
{
	size_t index;

	for (index = 0; index < ARRAY_COUNT(kunit_dependency_terminal_artifacts);
	     index++)
		if (!strcmp(source, kunit_dependency_terminal_artifacts[index].source))
			return &kunit_dependency_terminal_artifacts[index];
	return NULL;
}

static const struct kunit_case_provenance *
find_kunit_case(const struct orlix_tcti_target_proof_registry_entry *entry,
		const struct orlix_tcti_target_proof_case *proof_case)
{
	size_t index;

	for (index = 0; index < sizeof(kunit_case_provenance) /
				      sizeof(kunit_case_provenance[0]); index++) {
		const struct kunit_case_provenance *item =
			&kunit_case_provenance[index];

		if (!strcmp(entry->kunit_source, item->source) &&
		    !strcmp(entry->kunit_suite, item->suite) &&
		    !strcmp(proof_case->name, item->name))
			return item;
	}
	return NULL;
}

static bool valid_kunit_dependency(
	const struct kunit_source_provenance *source_metadata,
	const char *dependency_sha256)
{
#ifdef __KERNEL__
	return source_metadata && !empty(source_metadata->dependency) &&
		!empty(source_metadata->dependency_sha256) &&
		!empty(dependency_sha256) &&
		!strcmp(source_metadata->dependency_sha256, dependency_sha256);
#else
	char *dependency;
	const struct kunit_dependency_terminal_artifact *terminal_artifact;
	size_t dependency_length;
	bool valid;

	if (empty(source_metadata->dependency) || empty(dependency_sha256))
		return false;
	dependency = read_source(source_metadata->dependency, &dependency_length);
	if (!dependency)
		return false;
	terminal_artifact = find_kunit_dependency_terminal_artifact(
		source_metadata->source);
	valid = !memchr(dependency, '\0', dependency_length) &&
		sha256_matches((const orlix_tcti_proof_u8 *)dependency,
			       dependency_length, dependency_sha256) &&
		(!terminal_artifact ||
		 valid_kunit_manifest_terminal_artifact(
			source_metadata->dependency,
			terminal_artifact->name, terminal_artifact->path) == 0);
	free(dependency);
	return valid;
#endif
}

int orlix_tcti_target_kunit_dependency_validate_for_test(
	const char *source, const char *dependency, const char *dependency_sha256)
{
	const struct kunit_source_provenance *source_metadata;

	if (empty(source) || empty(dependency) || empty(dependency_sha256))
		return -1;
	source_metadata = find_kunit_source(source);
	if (!source_metadata || empty(source_metadata->dependency) ||
	    empty(source_metadata->dependency_sha256) ||
	    empty(source_metadata->include_directive) ||
	    strcmp(dependency, source_metadata->dependency) ||
	    strcmp(dependency_sha256, source_metadata->dependency_sha256))
		return -1;
	return valid_kunit_dependency(source_metadata,
			      source_metadata->dependency_sha256) ? 0 : -1;
}

static bool valid_kunit_provenance(
	const struct orlix_tcti_target_proof_registry_entry *entry)
{
#ifndef __KERNEL__
	char *build_source;
	char *source;
#endif
	const struct kunit_source_provenance *source_metadata;
#ifndef __KERNEL__
	size_t build_length;
	size_t source_length;
#endif
	size_t index;
#ifndef __KERNEL__
	bool valid = false;
#endif

	source_metadata = find_kunit_source(entry->kunit_source);
	if (!source_metadata)
		return false;
#ifdef __KERNEL__
	if (!!source_metadata->dependency != !!source_metadata->dependency_sha256 ||
	    !!source_metadata->dependency != !!source_metadata->include_directive)
		return false;
	for (index = 0; index < entry->kunit_case_count; index++) {
		const struct kunit_case_provenance *case_metadata =
			find_kunit_case(entry, &entry->kunit_cases[index]);

		if (!case_metadata ||
		    (entry->kunit_cases[index].obligations &
		     ~case_metadata->maximum_obligations))
			return false;
	}
	return true;
#else
	build_source = read_source(KUNIT_BUILD_SOURCE, &build_length);
	if (!build_source)
		return false;
	if (!sha256_matches((const orlix_tcti_proof_u8 *)build_source, build_length,
			    KUNIT_BUILD_SOURCE_SHA256) ||
	    !strstr(build_source, source_metadata->object)) {
		free(build_source);
		return false;
	}
	free(build_source);
	source = read_source(entry->kunit_source, &source_length);
	if (!source)
		return false;
	if (memchr(source, '\0', source_length) ||
	    !sha256_matches((const orlix_tcti_proof_u8 *)source, source_length,
			    source_metadata->sha256) ||
	    !source_registers_suite(source, entry->kunit_suite))
		goto out;
	if (!!source_metadata->dependency != !!source_metadata->dependency_sha256 ||
	    !!source_metadata->dependency != !!source_metadata->include_directive)
		goto out;
	if (source_metadata->dependency &&
	    (!strstr(source, source_metadata->include_directive) ||
	     !valid_kunit_dependency(source_metadata,
				     source_metadata->dependency_sha256)))
		goto out;
	for (index = 0; index < entry->kunit_case_count; index++) {
		const struct kunit_case_provenance *case_metadata =
			find_kunit_case(entry, &entry->kunit_cases[index]);

		if (!case_metadata ||
		    (entry->kunit_cases[index].obligations &
		     ~case_metadata->maximum_obligations) ||
		    !source_registers_case(source, entry->kunit_cases[index].name) ||
		    !source_connects_case_to_suite(source, case_metadata))
			goto out;
	}
	valid = true;
out:
	free(source);
	return valid;
#endif
}

static int production_capture_family_build(
	const struct production_capture_family_descriptor *family,
	const struct production_capture_operation_descriptor *operations,
	size_t operation_count,
	struct orlix_tcti_target_proof_registry_entry *entries,
	size_t entry_count,
	struct orlix_tcti_target_proof_binding *bindings,
	size_t binding_count)
{
	const struct kunit_source_provenance *source_metadata;
	struct orlix_tcti_target_proof_registry_entry provenance_entry;
	size_t binding_offset = 0;
	size_t operation_index;
	size_t index;

	if (!family || !operations || !operation_count || !entries ||
	    entry_count != operation_count || !bindings ||
	    !family->kunit_cases || !family->kunit_case_count ||
	    empty(family->kunit_source) || empty(family->kunit_source_sha256) ||
	    empty(family->kunit_object) || empty(family->kunit_suite) ||
	    empty(family->kunit_suite_symbol) || empty(family->kunit_case_array))
		return -1;
	source_metadata = find_kunit_source(family->kunit_source);
	if (!source_metadata || strcmp(source_metadata->sha256,
				       family->kunit_source_sha256) ||
	    strcmp(source_metadata->object, family->kunit_object))
		return -1;
	provenance_entry = (struct orlix_tcti_target_proof_registry_entry) {
		.kunit_source = family->kunit_source,
		.kunit_suite = family->kunit_suite,
		.kunit_cases = family->kunit_cases,
		.kunit_case_count = family->kunit_case_count,
	};
	for (index = 0; index < family->kunit_case_count; index++) {
		const struct kunit_case_provenance *case_metadata =
			find_kunit_case(&provenance_entry, &family->kunit_cases[index]);

		if (!case_metadata || strcmp(case_metadata->suite_symbol,
					 family->kunit_suite_symbol) ||
		    strcmp(case_metadata->case_array, family->kunit_case_array))
			return -1;
	}
	for (index = 0; index < ARRAY_COUNT(kunit_case_provenance); index++)
		if (!strcmp(kunit_case_provenance[index].source,
			    family->kunit_source) &&
		    !strcmp(kunit_case_provenance[index].suite,
			    family->kunit_suite)) {
			size_t case_index;

			for (case_index = 0; case_index < family->kunit_case_count;
			     case_index++)
				if (!strcmp(kunit_case_provenance[index].name,
					    family->kunit_cases[case_index].name))
					break;
			if (case_index == family->kunit_case_count)
				return -1;
		}
	for (operation_index = 0; operation_index < operation_count;
	     operation_index++) {
		const struct production_capture_operation_descriptor *operation =
			&operations[operation_index];
		orlix_tcti_proof_u32 required;
		size_t operation_bindings = 0;

		if (empty(operation->proof_id) || empty(operation->operation_id) ||
		    (operation->linux_interface !=
		     ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_REQUIRED &&
		     operation->linux_interface !=
		     ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE) ||
		    orlix_tcti_target_proof_operation_requirements(
			    operation->operation_id, 1U, &required))
			return -1;
		if (operation->linux_interface ==
		    ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_REQUIRED) {
			if (!family->kselftest ||
			    orlix_tcti_target_kselftest_provenance_validate(
				    family->kselftest))
				return -1;
			required |= ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LINUX_INTERFACE;
		}
		if (operation->obligations != required)
			return -1;
		for (index = 0; index < operation_index; index++)
			if (!strcmp(operation->proof_id, operations[index].proof_id) ||
			    !strcmp(operation->operation_id, operations[index].operation_id))
				return -1;
		for (index = 0; index < ARRAY_COUNT(source_bound_proofs); index++) {
			const struct source_manifest_binding *source;

			if (strcmp(operation->proof_id,
				   source_bound_proofs[index].proof_id))
				continue;
			source = source_manifest_binding(
				source_bound_proofs[index].source_ordinal);
			if (!source || strcmp(source->operation_id,
					      operation->operation_id))
				return -1;
			operation_bindings++;
		}
		if (!operation_bindings || operation_bindings > binding_count - binding_offset)
			return -1;
		binding_offset += operation_bindings;
	}
	if (binding_offset != binding_count)
		return -1;
	binding_offset = 0;
	for (operation_index = 0; operation_index < operation_count;
	     operation_index++) {
		const struct production_capture_operation_descriptor *operation =
			&operations[operation_index];
		size_t operation_bindings = 0;

		for (index = 0; index < ARRAY_COUNT(source_bound_proofs); index++) {
			const struct source_manifest_binding *source;

			if (strcmp(operation->proof_id,
				   source_bound_proofs[index].proof_id))
				continue;
			source = source_manifest_binding(
				source_bound_proofs[index].source_ordinal);
			bindings[binding_offset + operation_bindings++] =
				(struct orlix_tcti_target_proof_binding) {
					.leaf_name = source->leaf_name,
					.mnemonic = source->mnemonic,
					.encoding_mask = source->encoding_mask,
					.encoding_pattern = source->encoding_pattern,
					.condition_tcnd_hex = source->condition_tcnd_hex,
					.kunit_case_mask = ORLIX_TCTI_PROOF_U64_C(0x3),
					.source_ordinal = source->ordinal,
				};
		}
		entries[operation_index] =
			(struct orlix_tcti_target_proof_registry_entry) {
				.id = operation->proof_id,
				.operation_id = operation->operation_id,
				.classification_mask =
					ORLIX_TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0,
				.obligations = operation->obligations,
				.linux_interface = operation->linux_interface,
				.kunit_source = family->kunit_source,
				.kunit_suite = family->kunit_suite,
				.kunit_cases = family->kunit_cases,
				.kunit_case_count = family->kunit_case_count,
				.bindings = &bindings[binding_offset],
				.binding_count = operation_bindings,
				.kselftest = operation->linux_interface ==
					ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_REQUIRED ?
					family->kselftest : NULL,
				.unproved_obligations = operation->obligations,
			};
		binding_offset += operation_bindings;
	}
	return 0;
}

int orlix_tcti_target_kunit_provenance_identity(
	const struct orlix_tcti_target_proof_registry_entry *entry,
	const char *case_name,
	struct orlix_tcti_target_kunit_provenance_identity *identity)
{
	const struct kunit_source_provenance *source;
	const struct kunit_case_provenance *proof_case = NULL;
	size_t index;

	if (!entry || empty(case_name) || !identity)
		return -1;
	source = find_kunit_source(entry->kunit_source);
	if (!source)
		return -1;
	for (index = 0; index < ARRAY_COUNT(kunit_case_provenance); index++) {
		const struct kunit_case_provenance *candidate =
			&kunit_case_provenance[index];

		if (!strcmp(candidate->source, entry->kunit_source) &&
		    !strcmp(candidate->suite, entry->kunit_suite) &&
		    !strcmp(candidate->name, case_name)) {
			if (proof_case)
				return -1;
			proof_case = candidate;
		}
	}
	if (!proof_case)
		return -1;
	identity->source = source->source;
	identity->source_sha256 = source->sha256;
	identity->build_source = KUNIT_BUILD_SOURCE;
	identity->build_source_sha256 = KUNIT_BUILD_SOURCE_SHA256;
	identity->suite = proof_case->suite;
	identity->case_name = proof_case->name;
	return 0;
}

const struct orlix_tcti_target_production_capture_binding *
orlix_tcti_target_production_capture_bindings(size_t *count)
{
	if (count)
		*count = ARRAY_COUNT(production_capture_bindings);
	return production_capture_bindings;
}

int orlix_tcti_target_production_capture_binding_applies(
	const struct orlix_tcti_target_production_capture_binding *bindings,
	size_t binding_count,
	const struct orlix_tcti_target_proof_registry_entry *entry,
	size_t case_index)
{
	size_t index;

	if ((!bindings && binding_count) || !entry ||
	    case_index >= entry->kunit_case_count ||
	    empty(entry->kunit_source) || empty(entry->kunit_suite) ||
	    !entry->kunit_cases || empty(entry->kunit_cases[case_index].name))
		return 0;
	for (index = 0; index < binding_count; index++) {
		size_t binding_index;
		int source_found = 0;

		for (binding_index = 0; binding_index < entry->binding_count;
		     binding_index++)
			if (entry->bindings[binding_index].source_ordinal ==
			    bindings[index].source_ordinal) {
				source_found = 1;
				break;
			}
		if (source_found &&
		    (entry->kunit_cases[case_index].obligations &
		     bindings[index].obligation) &&
		    !strcmp(bindings[index].kunit_source, entry->kunit_source) &&
		    !strcmp(bindings[index].kunit_suite, entry->kunit_suite) &&
		    !strcmp(bindings[index].kunit_case,
			    entry->kunit_cases[case_index].name))
			return 1;
	}
	return 0;
}

int orlix_tcti_target_production_capture_bindings_validate(
	const struct orlix_tcti_target_production_capture_binding *bindings,
	size_t binding_count,
	const struct orlix_tcti_target_proof_registry_entry *entries,
	size_t entry_count,
	enum orlix_tcti_target_production_capture_error *error)
{
	size_t index;

	if (error)
		*error = ORLIX_TCTI_TARGET_PRODUCTION_CAPTURE_INVALID;
	if ((!bindings && binding_count) || (!entries && entry_count))
		return -1;
	for (index = 0; index < binding_count; index++) {
		size_t entry_index;
		size_t previous;
		int found = 0;

		if (empty(bindings[index].kunit_source) ||
		    empty(bindings[index].kunit_suite) ||
		    empty(bindings[index].kunit_case) ||
		    !bindings[index].obligation ||
		    empty(bindings[index].implementation_owner) ||
		    empty(bindings[index].decoder_owner) ||
		    empty(bindings[index].lowering_owner))
			return -1;
		for (previous = 0; previous < index; previous++)
			if (!strcmp(bindings[index].kunit_source,
				    bindings[previous].kunit_source) &&
			    !strcmp(bindings[index].kunit_suite,
				    bindings[previous].kunit_suite) &&
			    !strcmp(bindings[index].kunit_case,
				    bindings[previous].kunit_case) &&
			    bindings[index].source_ordinal ==
				    bindings[previous].source_ordinal &&
			    bindings[index].obligation ==
				    bindings[previous].obligation) {
				if (error)
					*error =
						ORLIX_TCTI_TARGET_PRODUCTION_CAPTURE_DUPLICATE;
				return -1;
			}
		for (entry_index = 0; entry_index < entry_count; entry_index++) {
			size_t case_index;

			for (case_index = 0;
			     case_index < entries[entry_index].kunit_case_count;
			     case_index++)
				if (orlix_tcti_target_production_capture_binding_applies(
						&bindings[index], 1U,
						&entries[entry_index], case_index))
					found = 1;
		}
		if (!found) {
			if (error)
				*error = ORLIX_TCTI_TARGET_PRODUCTION_CAPTURE_UNKNOWN;
			return -1;
		}
	}
	if (error)
		*error = ORLIX_TCTI_TARGET_PRODUCTION_CAPTURE_OK;
	return 0;
}

int orlix_tcti_target_proof_case_has_production_capture(
	const struct orlix_tcti_target_proof_registry_entry *entry,
	size_t case_index, orlix_tcti_proof_u32 source_ordinal,
	orlix_tcti_proof_u32 obligation)
{
	size_t index;

	for (index = 0; index < ARRAY_COUNT(production_capture_bindings); index++)
		if (production_capture_bindings[index].source_ordinal ==
				source_ordinal &&
		    production_capture_bindings[index].obligation == obligation &&
		    orlix_tcti_target_production_capture_binding_applies(
			&production_capture_bindings[index], 1U, entry, case_index))
			return 1;
	return 0;
}

int orlix_tcti_target_kselftest_provenance_validate(
	const struct orlix_tcti_target_kselftest_provenance *provenance)
{
	static const struct orlix_tcti_target_kselftest_provenance allowed[] = {
		{ KSELFTEST_SOURCE, KSELFTEST_SOURCE_SHA256,
		  KSELFTEST_BUILD_SOURCE, KSELFTEST_BUILD_SOURCE_SHA256,
		  "orlix_tcti_lse_atomic_probe", "main" },
		{ KSELFTEST_PROCESS_SOURCE, KSELFTEST_PROCESS_SOURCE_SHA256,
		  KSELFTEST_BUILD_SOURCE, KSELFTEST_BUILD_SOURCE_SHA256,
		  "process_lifecycle_probe", "main" },
		{ KSELFTEST_SIGNAL_SOURCE, KSELFTEST_SIGNAL_SOURCE_SHA256,
		  KSELFTEST_BUILD_SOURCE, KSELFTEST_BUILD_SOURCE_SHA256,
		  "signal_wait_probe", "main" },
		{ KSELFTEST_STACK_SOURCE, KSELFTEST_STACK_SOURCE_SHA256,
		  KSELFTEST_BUILD_SOURCE, KSELFTEST_BUILD_SOURCE_SHA256,
		  "stack_growth_probe", "main" },
		{ KSELFTEST_MMAP_SOURCE, KSELFTEST_MMAP_SOURCE_SHA256,
		  KSELFTEST_BUILD_SOURCE, KSELFTEST_BUILD_SOURCE_SHA256,
		  "file_mmap_content_probe", "main" },
		{ KSELFTEST_PTY_SOURCE, KSELFTEST_PTY_SOURCE_SHA256,
		  KSELFTEST_BUILD_SOURCE, KSELFTEST_BUILD_SOURCE_SHA256,
		  "pty_terminal_probe", "main" },
		{ KSELFTEST_FD_SOURCE, KSELFTEST_FD_SOURCE_SHA256,
		  KSELFTEST_BUILD_SOURCE, KSELFTEST_BUILD_SOURCE_SHA256,
		  "fd_alias_probe", "main" },
		{ KSELFTEST_SYSTEM_SOURCE, KSELFTEST_SYSTEM_SOURCE_SHA256,
		  KSELFTEST_BUILD_SOURCE, KSELFTEST_BUILD_SOURCE_SHA256,
		  "orlix_tcti_system_probe", "main" },
		{ KSELFTEST_EXCEPTION_INTERFACE_SOURCE,
		  KSELFTEST_EXCEPTION_INTERFACE_SOURCE_SHA256,
		  KSELFTEST_BUILD_SOURCE, KSELFTEST_BUILD_SOURCE_SHA256,
		  "orlix_tcti_exception_interface_probe", "main" },
	};
#ifndef __KERNEL__
	static bool checked[ARRAY_COUNT(allowed)];
	static bool cached_valid[ARRAY_COUNT(allowed)];
	char *build_source;
	char *source;
	size_t build_length;
	size_t source_length;
#endif
	size_t index;
#ifndef __KERNEL__
	bool valid;
#endif

	if (!provenance || empty(provenance->source) ||
	    empty(provenance->source_sha256) ||
	    empty(provenance->build_source) ||
	    empty(provenance->build_source_sha256) ||
	    empty(provenance->program) || empty(provenance->case_name) ||
	    strcmp(provenance->build_source, KSELFTEST_BUILD_SOURCE) ||
	    strcmp(provenance->build_source_sha256,
		   KSELFTEST_BUILD_SOURCE_SHA256))
		return -1;
	for (index = 0; index < ARRAY_COUNT(allowed); index++)
		if (!strcmp(provenance->source, allowed[index].source) &&
		    !strcmp(provenance->source_sha256,
			    allowed[index].source_sha256) &&
		    !strcmp(provenance->program, allowed[index].program) &&
		    !strcmp(provenance->case_name, allowed[index].case_name))
			break;
	if (index == ARRAY_COUNT(allowed))
		return -1;
#ifdef __KERNEL__
	return 0;
#else
	if (checked[index])
		return cached_valid[index] ? 0 : -1;
	build_source = read_source(provenance->build_source, &build_length);
	if (!build_source)
		return -1;
	source = read_source(provenance->source, &source_length);
	if (!source) {
		free(build_source);
		return -1;
	}
	valid = !memchr(build_source, '\0', build_length) &&
		!memchr(source, '\0', source_length) &&
		sha256_matches((const orlix_tcti_proof_u8 *)build_source, build_length,
			       provenance->build_source_sha256) &&
		sha256_matches((const orlix_tcti_proof_u8 *)source, source_length,
			       provenance->source_sha256) &&
		strstr(build_source, provenance->program) &&
		strstr(source, "int main(");
	free(source);
	free(build_source);
	checked[index] = true;
	cached_valid[index] = valid;
	return valid ? 0 : -1;
#endif
}

int orlix_tcti_target_proof_source_evidence_validate(
	const char *source_path, const char *source_sha256, const char *assertion)
{
#ifdef __KERNEL__
	return empty(source_path) || empty(source_sha256) || empty(assertion) ?
		-1 : -EOPNOTSUPP;
#else
	char *source;
	size_t source_length;
	bool valid;

	if (empty(source_path) || empty(source_sha256) || empty(assertion))
		return -1;
	source = read_source(source_path, &source_length);
	if (!source)
		return -1;
	valid = !memchr(source, '\0', source_length) &&
		sha256_matches((const orlix_tcti_proof_u8 *)source, source_length,
			       source_sha256) && strstr(source, assertion);
	free(source);
	return valid ? 0 : -1;
#endif
}

int orlix_tcti_target_proof_registry_validate(
	const struct orlix_tcti_target_proof_registry_entry *entries, size_t count,
	enum orlix_tcti_target_proof_registry_error *error)
{
	size_t index;
	size_t previous;

	if (error)
		*error = ORLIX_TCTI_TARGET_PROOF_REGISTRY_OK;
	if (!entries && count)
		goto invalid;
	for (index = 0; index < count; index++) {
		const struct orlix_tcti_target_proof_registry_entry *entry = &entries[index];
		orlix_tcti_proof_u32 case_obligations = 0;
		orlix_tcti_proof_u32 required;
		unsigned int classification;
		size_t binding;
		size_t proof_case;

		if (empty(entry->id) || empty(entry->operation_id) ||
		    !one_bit(entry->classification_mask) ||
		    (entry->classification_mask & ~KNOWN_CLASS_MASK) ||
		    !entry->obligations ||
		    (entry->obligations & ~KNOWN_OBLIGATION_MASK) ||
		    entry->linux_interface >
			ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_REQUIRED ||
		    empty(entry->kunit_source) || empty(entry->kunit_suite) ||
		    !entry->kunit_cases || !entry->kunit_case_count ||
		    entry->kunit_case_count > 64 ||
		    !entry->bindings || !entry->binding_count)
			goto invalid;
		if (entry->classification_mask ==
		    ORLIX_TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0)
			classification = 1;
		else if (entry->classification_mask ==
			 ORLIX_TCTI_TARGET_PROOF_CLASS_NON_EL0)
			classification = 2;
		else
			classification = 3;
		if (orlix_tcti_target_proof_operation_requirements(
			    entry->operation_id, classification, &required)) {
			if (error)
				*error =
					ORLIX_TCTI_TARGET_PROOF_REGISTRY_UNKNOWN_FAMILY_REQUIREMENTS;
			return -1;
		}
		if (entry->linux_interface ==
		    ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_REQUIRED)
			required |= ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LINUX_INTERFACE;
		if (entry->obligations != required) {
			if (error)
				*error =
					ORLIX_TCTI_TARGET_PROOF_REGISTRY_INSUFFICIENT_OBLIGATIONS;
			return -1;
		}
		/*
		 * This registry authenticates static ownership only. Source hashes,
		 * Kbuild membership, and named cases are not native execution
		 * records, so they discharge no architectural obligation.
		 */
		if (entry->unproved_obligations != required) {
			if (error)
				*error =
					ORLIX_TCTI_TARGET_PROOF_REGISTRY_INSUFFICIENT_OBLIGATIONS;
			return -1;
		}
		if (entry->linux_interface ==
		    ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_REQUIRED) {
			/* KUnit cannot satisfy Linux-visible interface proof. */
			if (orlix_tcti_target_kselftest_provenance_validate(entry->kselftest)) {
				if (error)
					*error =
						ORLIX_TCTI_TARGET_PROOF_REGISTRY_INVALID_KSELFTEST_PROVENANCE;
				return -1;
			}
		}
		if (entry->linux_interface ==
			    ORLIX_TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE &&
		    entry->kselftest)
			goto invalid;
		for (proof_case = 0; proof_case < entry->kunit_case_count;
		     proof_case++) {
			const struct orlix_tcti_target_proof_case *item =
				&entry->kunit_cases[proof_case];

			if (empty(item->name) || !item->obligations ||
			    (item->obligations & ~KNOWN_OBLIGATION_MASK))
				goto invalid;
			if (item->obligations &
			    ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LINUX_INTERFACE)
				goto invalid;
			for (previous = 0; previous < proof_case; previous++)
				if (!strcmp(item->name,
					    entry->kunit_cases[previous].name))
					goto invalid;
			case_obligations |= item->obligations;
		}
		if (case_obligations & ~required)
			goto invalid;
		if (!valid_kunit_provenance(entry)) {
			if (error)
				*error =
					ORLIX_TCTI_TARGET_PROOF_REGISTRY_INVALID_KUNIT_PROVENANCE;
			return -1;
		}
		for (binding = 0; binding < entry->binding_count; binding++) {
			const struct orlix_tcti_target_proof_binding *item =
				&entry->bindings[binding];
			orlix_tcti_proof_u32 binding_obligations = 0;
			size_t case_index;

			if (empty(item->leaf_name) || empty(item->mnemonic) ||
			    empty(item->condition_tcnd_hex) ||
			    !item->kunit_case_mask ||
			    (entry->kunit_case_count < 64 &&
			     (item->kunit_case_mask >> entry->kunit_case_count)))
				goto invalid;
			if (!proof_binding_matches_source_manifest(entry, item)) {
				if (error)
					*error =
						ORLIX_TCTI_TARGET_PROOF_REGISTRY_BINDING_MISMATCH;
				return -1;
			}
			for (case_index = 0; case_index < entry->kunit_case_count;
			     case_index++)
				if (item->kunit_case_mask & (ORLIX_TCTI_PROOF_U64_C(1) << case_index))
					binding_obligations |=
						entry->kunit_cases[case_index].obligations;
			if (binding_obligations & ~required)
				goto invalid;
			for (previous = 0; previous < binding; previous++)
				if (!strcmp(item->leaf_name,
					    entry->bindings[previous].leaf_name))
					goto invalid;
		}
		for (previous = 0; previous < index; previous++) {
			if (!strcmp(entry->id, entries[previous].id)) {
				if (error)
					*error =
						ORLIX_TCTI_TARGET_PROOF_REGISTRY_DUPLICATE_ID;
				return -1;
			}
			if (entry->classification_mask ==
				    entries[previous].classification_mask &&
			    !strcmp(entry->operation_id,
				    entries[previous].operation_id) &&
			    !(base_atomic_missing_proof_id(entry->id) &&
			      base_atomic_missing_proof_id(entries[previous].id)))
				goto invalid;
		}
	}
	return 0;
invalid:
	if (error)
		*error = ORLIX_TCTI_TARGET_PROOF_REGISTRY_INVALID_ENTRY;
	return -1;
}

int orlix_tcti_target_proof_registry_source_bound_projection_validate(
	const struct orlix_tcti_target_proof_registry_entry *entries, size_t count,
	enum orlix_tcti_target_proof_registry_error *error)
{
	return orlix_tcti_target_proof_registry_projection_validate(entries, count,
		source_bound_proofs, ARRAY_COUNT(source_bound_proofs), error);
}

int orlix_tcti_target_proof_registry_projection_validate(
	const struct orlix_tcti_target_proof_registry_entry *entries, size_t count,
	const struct orlix_tcti_target_proof_registry_projection_binding *projection,
	size_t projection_count,
	enum orlix_tcti_target_proof_registry_error *error)
{
	size_t entry_index;
	size_t index;

	if ((projection_count && !projection) ||
	    orlix_tcti_target_proof_registry_validate(entries, count, error))
		return -1;
	for (entry_index = 0; entry_index < count; entry_index++) {
		const struct orlix_tcti_target_proof_registry_entry *entry =
			&entries[entry_index];
		size_t binding_index;

		for (binding_index = 0; binding_index < entry->binding_count;
		     binding_index++)
			if (source_bound_proof_matches_binding(
				    projection, projection_count, entry,
				    &entry->bindings[binding_index]) != 1) {
				if (error)
					*error =
						ORLIX_TCTI_TARGET_PROOF_REGISTRY_BINDING_MISMATCH;
				return -1;
			}
	}
	for (index = 0; index < projection_count; index++)
		if (source_bound_proof_matches_registry(entries, count,
						 &projection[index]) != 1) {
			if (error)
				*error = ORLIX_TCTI_TARGET_PROOF_REGISTRY_BINDING_MISMATCH;
			return -1;
		}
	return 0;
}

const struct orlix_tcti_target_proof_registry_projection_binding *
orlix_tcti_target_proof_registry_projection_bindings(size_t *count)
{
	if (count)
		*count = ARRAY_COUNT(proof_registry_projection);
	return proof_registry_projection;
}

enum orlix_tcti_target_proof_registry_error orlix_tcti_target_proof_registry_lookup(
	const struct orlix_tcti_target_proof_registry_entry *entries, size_t count,
	const struct orlix_tcti_target_proof_reference *reference)
{
	const struct orlix_tcti_target_proof_registry_entry *entry = NULL;
	size_t binding;
	size_t index;
	orlix_tcti_proof_u32 class_bit;

	if (!reference || empty(reference->id) || empty(reference->leaf_name) ||
	    empty(reference->mnemonic) || empty(reference->operation_id) ||
	    empty(reference->condition_tcnd_hex))
		return ORLIX_TCTI_TARGET_PROOF_REGISTRY_MISSING_REFERENCE;
	if (reference->classification >= 32)
		return ORLIX_TCTI_TARGET_PROOF_REGISTRY_CLASSIFICATION_MISMATCH;
	class_bit = 1U << reference->classification;
	for (index = 0; index < count; index++)
		if (!strcmp(entries[index].id, reference->id)) {
			entry = &entries[index];
			break;
		}
	if (!entry)
		return ORLIX_TCTI_TARGET_PROOF_REGISTRY_UNKNOWN_PROOF;
	if (!(entry->classification_mask & class_bit))
		return ORLIX_TCTI_TARGET_PROOF_REGISTRY_CLASSIFICATION_MISMATCH;
	if (strcmp(entry->operation_id, reference->operation_id))
		return ORLIX_TCTI_TARGET_PROOF_REGISTRY_FAMILY_MISMATCH;
	for (binding = 0; binding < entry->binding_count; binding++)
		if (!strcmp(entry->bindings[binding].leaf_name,
			    reference->leaf_name) &&
		    !strcmp(entry->bindings[binding].mnemonic,
			    reference->mnemonic) &&
		    entry->bindings[binding].encoding_mask ==
			    reference->encoding_mask &&
		    entry->bindings[binding].encoding_pattern ==
			    reference->encoding_pattern &&
		    !strcmp(entry->bindings[binding].condition_tcnd_hex,
			    reference->condition_tcnd_hex))
			return ORLIX_TCTI_TARGET_PROOF_REGISTRY_OK;
	return ORLIX_TCTI_TARGET_PROOF_REGISTRY_BINDING_MISMATCH;
}

static void proof_registry_initialize(void)
{
	enum orlix_tcti_target_proof_registry_error error =
		ORLIX_TCTI_TARGET_PROOF_REGISTRY_OK;

	proof_registry_initialization_attempted = true;
	proof_registry_initialized = false;
	proof_registry_initialization_error =
		ORLIX_TCTI_TARGET_PROOF_REGISTRY_INVALID_ENTRY;
	if (!build_lse_registry() || !build_scalar_registry() ||
	    !build_exclusive_registry() || !build_ordinary_load_store_registry() ||
	    !build_advsimd_load_store_registry() ||
	    !build_source_leaf_rejection_registry() ||
	    !build_production_capture_family_registry() ||
	    !build_mops_copy_registry() ||
	    !build_base_atomic_missing_registry())
		return;
	if (orlix_tcti_target_production_capture_bindings_validate(
			production_capture_bindings,
			ARRAY_COUNT(production_capture_bindings),
			proof_registry_entries,
			ARRAY_COUNT(proof_registry_entries), NULL))
		return;
	if (orlix_tcti_target_proof_registry_projection_validate(
			proof_registry_entries,
			ARRAY_COUNT(proof_registry_entries),
			proof_registry_projection,
			ARRAY_COUNT(proof_registry_projection), &error)) {
		proof_registry_initialization_error = error;
		return;
	}
	proof_registry_initialization_error =
		ORLIX_TCTI_TARGET_PROOF_REGISTRY_OK;
	proof_registry_initialized = true;
}

const struct orlix_tcti_target_proof_registry_entry *
orlix_tcti_target_proof_registry_entries(size_t *count)
{
#ifdef __KERNEL__
	mutex_lock(&proof_registry_initialization_lock);
	if (!proof_registry_initialization_attempted)
		proof_registry_initialize();
	mutex_unlock(&proof_registry_initialization_lock);
#else
	if (pthread_once(&proof_registry_initialization_once,
			 proof_registry_initialize))
		proof_registry_initialized = false;
#endif
	if (!proof_registry_initialized) {
		if (count)
			*count = 0;
		return NULL;
	}
	if (count)
		*count = sizeof(proof_registry_entries) /
			 sizeof(proof_registry_entries[0]);
	return proof_registry_entries;
}

enum orlix_tcti_target_proof_registry_error
orlix_tcti_target_proof_registry_initialization_error(void)
{
	return proof_registry_initialization_error;
}

#define KSELFTEST_PROVENANCE(source_value, digest_value, program_value) \
	{ source_value, digest_value, KSELFTEST_BUILD_SOURCE, \
	  KSELFTEST_BUILD_SOURCE_SHA256, program_value, "main" }

static const struct orlix_tcti_target_kselftest_provenance syscall_kselftests[] = {
	KSELFTEST_PROVENANCE(KSELFTEST_PROCESS_SOURCE,
			     KSELFTEST_PROCESS_SOURCE_SHA256,
			     "process_lifecycle_probe"),
	KSELFTEST_PROVENANCE(KSELFTEST_SIGNAL_SOURCE,
			     KSELFTEST_SIGNAL_SOURCE_SHA256, "signal_wait_probe"),
	KSELFTEST_PROVENANCE(KSELFTEST_STACK_SOURCE,
			     KSELFTEST_STACK_SOURCE_SHA256, "stack_growth_probe"),
	KSELFTEST_PROVENANCE(KSELFTEST_MMAP_SOURCE,
			     KSELFTEST_MMAP_SOURCE_SHA256,
			     "file_mmap_content_probe"),
	KSELFTEST_PROVENANCE(KSELFTEST_FD_SOURCE, KSELFTEST_FD_SOURCE_SHA256,
			     "fd_alias_probe"),
	KSELFTEST_PROVENANCE(KSELFTEST_PTY_SOURCE,
			     KSELFTEST_PTY_SOURCE_SHA256, "pty_terminal_probe"),
};

static const struct orlix_tcti_target_kselftest_provenance signal_kselftests[] = {
	KSELFTEST_PROVENANCE(KSELFTEST_SIGNAL_SOURCE,
			     KSELFTEST_SIGNAL_SOURCE_SHA256, "signal_wait_probe"),
};

static const struct orlix_tcti_target_kselftest_provenance memory_kselftests[] = {
	KSELFTEST_PROVENANCE(KSELFTEST_STACK_SOURCE,
			     KSELFTEST_STACK_SOURCE_SHA256, "stack_growth_probe"),
	KSELFTEST_PROVENANCE(KSELFTEST_MMAP_SOURCE,
			     KSELFTEST_MMAP_SOURCE_SHA256,
			     "file_mmap_content_probe"),
};

static const struct orlix_tcti_target_kselftest_provenance atomic_kselftests[] = {
	KSELFTEST_PROVENANCE(KSELFTEST_SOURCE, KSELFTEST_SOURCE_SHA256,
			     "orlix_tcti_lse_atomic_probe"),
};

static const struct orlix_tcti_target_kselftest_provenance system_kselftests[] = {
	KSELFTEST_PROVENANCE(KSELFTEST_SYSTEM_SOURCE,
			     KSELFTEST_SYSTEM_SOURCE_SHA256,
			     "orlix_tcti_system_probe"),
};

#undef KSELFTEST_PROVENANCE

struct linux_proof_policy {
	enum orlix_tcti_target_linux_proof_disposition disposition;
	enum orlix_tcti_target_linux_not_applicable_reason reason;
	orlix_tcti_proof_u32 owner_mask;
	const struct orlix_tcti_target_kselftest_provenance *kselftests;
	size_t kselftest_count;
};

enum source_linux_policy_class {
	SOURCE_LINUX_POLICY_UNCLASSIFIED,
	SOURCE_LINUX_POLICY_SYSCALL,
	SOURCE_LINUX_POLICY_FAULT,
	SOURCE_LINUX_POLICY_ATOMIC,
	SOURCE_LINUX_POLICY_MEMORY,
	SOURCE_LINUX_POLICY_SYSTEM_ACCESS,
	SOURCE_LINUX_POLICY_PREFETCH_HINT,
	SOURCE_LINUX_POLICY_ARCHITECTURAL_ONLY,
};

/*
 * One independently reviewable policy class per pinned source ordinal.
 * This partition is deliberately separate from decoder/KUnit/HWCAP state.
 * A new source row has no policy until this 4,350-byte canonical artifact is
 * explicitly extended; it can never inherit a not_applicable default.
 */
#define SOURCE_LINUX_POLICY_CHUNK_ROWS 44U
#define SOURCE_LINUX_POLICY_CHUNK_WIDTH 100U
#define SOURCE_LINUX_POLICY_FINAL_WIDTH 50U
static const char source_linux_policy_classes
	[SOURCE_LINUX_POLICY_CHUNK_ROWS][SOURCE_LINUX_POLICY_CHUNK_WIDTH + 1U] = {
	"7777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"7777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"7777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"7777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"7777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"7777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"7777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"7777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"7777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"7777777777777777777777777777777777777777777777777777777777777777777777777444444444477774444444477774",
	"4444777777774444444444444444444444444444444444444444444444444444444444444444444444444444444444444444",
	"4444444444444444444444444444444444444444444444444444444444444444444444444477774444444444444444477774",
	"4444444444444444444444444444777744444444444444444444444444444444444444444444444444444444444444444444",
	"4444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444",
	"4444444444444444444444444444444444444444444444444444444477777777777777777777777777777777777777777777",
	"7777777777777777777777777777777777777777777777777777777747777774777777777777777777777777777777777777",
	"7777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"7777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"7777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"7777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"7777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"7777777777777777777777777777777777777777777777777777444444444444447777777777777777777777777777777777",
	"7777777777777777777777777771222222277777777777777777777777777777777474777777757775555557777777777772",
	"2272777777777777777777777777777777777777777773333333344444444444444444444444444444444444444444444444",
	"4444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444",
	"4444477777777777777773337773337773337773337777777777777773333333377444444444444444444444444444444444",
	"4444444444444444444444444444444333333333333333344444444444444444444444444447777777777774444444444444",
	"4446777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"7777777777777777777777774443444344434443444344434443444344444444444444444444444444444444444444444444",
	"4444444444444444444444444444444444444447444444444444444444444444444444444444444444444444444444444444",
	"4333333333777333333333777333333333777433333333377733333333377733333333377733333333377743333333337773",
	"3333333333333333333333333343333333333333333334444333333333333333333433333333344444444444444444444444",
	"4444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444444",
	"4444444444444444444444446744444444444444444444444444464477777777777777777777777777777777777777777777",
	"7777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"7777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"7777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"7777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"7777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"7777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"7777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"7777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"7777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777777",
	"77777777777777777777777777777777777777777777777777",
};

_Static_assert((SOURCE_LINUX_POLICY_CHUNK_ROWS - 1U) *
		       SOURCE_LINUX_POLICY_CHUNK_WIDTH +
		       SOURCE_LINUX_POLICY_FINAL_WIDTH ==
		       ORLIX_TCTI_TARGET_LINUX_PROOF_SOURCE_ROWS,
	       "Linux policy must classify exactly 4,350 source ordinals");
_Static_assert(sizeof(source_linux_policy_classes[0]) - 1U ==
		       SOURCE_LINUX_POLICY_CHUNK_WIDTH,
	       "each Linux policy chunk must hold 100 source ordinals");

static enum source_linux_policy_class source_linux_policy_class(
	const struct source_manifest_binding *source)
{
	char policy;

	if (!source || source->ordinal >=
			ORLIX_TCTI_TARGET_LINUX_PROOF_SOURCE_ROWS)
		return SOURCE_LINUX_POLICY_UNCLASSIFIED;
	policy = source_linux_policy_classes[
		source->ordinal / SOURCE_LINUX_POLICY_CHUNK_WIDTH][
		source->ordinal % SOURCE_LINUX_POLICY_CHUNK_WIDTH];
	if (policy < '1' || policy > '7')
		return SOURCE_LINUX_POLICY_UNCLASSIFIED;
	return (enum source_linux_policy_class)(policy - '0');
}

static bool source_text_starts_with(const char *text, const char *prefix)
{
	return text && prefix && !strncmp(text, prefix, strlen(prefix));
}

static bool source_is_fault_operation(const struct source_manifest_binding *source)
{
	static const char *const operations[] = {
		"BRK", "HLT", "UDF", "HVC", "SMC", "DCPS1", "DCPS2",
		"DCPS3", "ERET", "ERETA", "DRPS",
	};
	size_t index;

	for (index = 0; index < ARRAY_COUNT(operations); index++)
		if (!strcmp(source->operation_id, operations[index]))
			return true;
	return false;
}

static bool source_is_atomic_operation(
	const struct source_manifest_binding *source)
{
	static const char *const prefixes[] = {
		"CAS", "SWP", "LDADD", "LDCLR", "LDEOR", "LDSET",
		"LDSMAX", "LDSMIN", "LDUMAX", "LDUMIN",
	};
	size_t index;

	for (index = 0; index < ARRAY_COUNT(prefixes); index++)
		if (source_text_starts_with(source->mnemonic, prefixes[index]))
			return true;
	return false;
}

static bool source_is_memory_operation(
	const struct source_manifest_binding *source)
{
	return !source_is_atomic_operation(source) &&
		(source_text_starts_with(source->mnemonic, "LD") ||
		 source_text_starts_with(source->mnemonic, "ST"));
}

static bool source_is_system_access_operation(
	const struct source_manifest_binding *source)
{
	return source_text_starts_with(source->operation_id, "MRS") ||
		source_text_starts_with(source->operation_id, "MSR") ||
		source_text_starts_with(source->operation_id, "SYS");
}

static bool source_linux_policy_class_matches_source(
	const struct source_manifest_binding *source,
	enum source_linux_policy_class policy)
{
	bool known;

	switch (policy) {
	case SOURCE_LINUX_POLICY_SYSCALL:
		return !strcmp(source->operation_id, "SVC");
	case SOURCE_LINUX_POLICY_FAULT:
		return source_is_fault_operation(source);
	case SOURCE_LINUX_POLICY_ATOMIC:
		return source_is_atomic_operation(source);
	case SOURCE_LINUX_POLICY_MEMORY:
		return source_is_memory_operation(source);
	case SOURCE_LINUX_POLICY_SYSTEM_ACCESS:
		return source_is_system_access_operation(source);
	case SOURCE_LINUX_POLICY_PREFETCH_HINT:
		return !strcmp(source->mnemonic, "PRFM");
	case SOURCE_LINUX_POLICY_ARCHITECTURAL_ONLY:
		known = !strcmp(source->operation_id, "SVC") ||
			source_is_fault_operation(source) ||
			source_is_atomic_operation(source) ||
			source_is_memory_operation(source) ||
			source_is_system_access_operation(source) ||
			!strcmp(source->mnemonic, "PRFM");
		return !known;
	case SOURCE_LINUX_POLICY_UNCLASSIFIED:
	default:
		return false;
	}
}

static bool source_linux_policy_partition_valid(void)
{
	static const size_t expected_counts[] = {
		[SOURCE_LINUX_POLICY_SYSCALL] = 1U,
		[SOURCE_LINUX_POLICY_FAULT] = 11U,
		[SOURCE_LINUX_POLICY_ATOMIC] = 196U,
		[SOURCE_LINUX_POLICY_MEMORY] = 1085U,
		[SOURCE_LINUX_POLICY_SYSTEM_ACCESS] = 7U,
		[SOURCE_LINUX_POLICY_PREFETCH_HINT] = 3U,
		[SOURCE_LINUX_POLICY_ARCHITECTURAL_ONLY] = 3047U,
	};
	size_t counts[ARRAY_COUNT(expected_counts)] = { 0 };
	size_t index;

	for (index = 0; index < ARRAY_COUNT(source_manifest_bindings); index++) {
		const struct source_manifest_binding *source =
			&source_manifest_bindings[index];
		enum source_linux_policy_class policy =
			source_linux_policy_class(source);

		if (source->ordinal != index ||
		    policy == SOURCE_LINUX_POLICY_UNCLASSIFIED ||
		    policy >= (enum source_linux_policy_class)ARRAY_COUNT(counts) ||
		    !source_linux_policy_class_matches_source(source, policy))
			return false;
		counts[policy]++;
	}
	for (index = SOURCE_LINUX_POLICY_SYSCALL;
	     index < ARRAY_COUNT(expected_counts); index++)
		if (counts[index] != expected_counts[index])
			return false;
	return counts[SOURCE_LINUX_POLICY_UNCLASSIFIED] == 0U;
}

static struct linux_proof_policy source_linux_policy(
	const struct source_manifest_binding *source)
{
	struct linux_proof_policy policy = { 0 };

	switch (source_linux_policy_class(source)) {
	case SOURCE_LINUX_POLICY_SYSCALL:
		policy.disposition = ORLIX_TCTI_TARGET_LINUX_PROOF_KSELFTEST_OWNED;
		policy.reason = ORLIX_TCTI_TARGET_LINUX_NA_NONE;
		policy.owner_mask = ORLIX_TCTI_TARGET_LINUX_OWNER_SYSCALL_PROCESS |
			ORLIX_TCTI_TARGET_LINUX_OWNER_SIGNAL_FAULT |
			ORLIX_TCTI_TARGET_LINUX_OWNER_MEMORY_VFS |
			ORLIX_TCTI_TARGET_LINUX_OWNER_FD_PTY_TERMINAL;
		policy.kselftests = syscall_kselftests;
		policy.kselftest_count = ARRAY_COUNT(syscall_kselftests);
		break;
	case SOURCE_LINUX_POLICY_FAULT:
		policy.disposition = ORLIX_TCTI_TARGET_LINUX_PROOF_KSELFTEST_OWNED;
		policy.reason = ORLIX_TCTI_TARGET_LINUX_NA_NONE;
		policy.owner_mask = ORLIX_TCTI_TARGET_LINUX_OWNER_SIGNAL_FAULT;
		policy.kselftests = signal_kselftests;
		policy.kselftest_count = ARRAY_COUNT(signal_kselftests);
		break;
	case SOURCE_LINUX_POLICY_ATOMIC:
		policy.disposition = ORLIX_TCTI_TARGET_LINUX_PROOF_KSELFTEST_OWNED;
		policy.reason = ORLIX_TCTI_TARGET_LINUX_NA_NONE;
		policy.owner_mask = ORLIX_TCTI_TARGET_LINUX_OWNER_ATOMIC_ORDERING |
			ORLIX_TCTI_TARGET_LINUX_OWNER_MEMORY_VFS;
		policy.kselftests = atomic_kselftests;
		policy.kselftest_count = ARRAY_COUNT(atomic_kselftests);
		break;
	case SOURCE_LINUX_POLICY_MEMORY:
		policy.disposition = ORLIX_TCTI_TARGET_LINUX_PROOF_KSELFTEST_OWNED;
		policy.reason = ORLIX_TCTI_TARGET_LINUX_NA_NONE;
		policy.owner_mask = ORLIX_TCTI_TARGET_LINUX_OWNER_MEMORY_VFS;
		policy.kselftests = memory_kselftests;
		policy.kselftest_count = ARRAY_COUNT(memory_kselftests);
		break;
	case SOURCE_LINUX_POLICY_SYSTEM_ACCESS:
		policy.disposition = ORLIX_TCTI_TARGET_LINUX_PROOF_KSELFTEST_OWNED;
		policy.reason = ORLIX_TCTI_TARGET_LINUX_NA_NONE;
		policy.owner_mask = ORLIX_TCTI_TARGET_LINUX_OWNER_SYSTEM_ACCESS;
		policy.kselftests = system_kselftests;
		policy.kselftest_count = ARRAY_COUNT(system_kselftests);
		break;
	case SOURCE_LINUX_POLICY_PREFETCH_HINT:
		policy.disposition = ORLIX_TCTI_TARGET_LINUX_PROOF_NOT_APPLICABLE;
		policy.reason = ORLIX_TCTI_TARGET_LINUX_NA_PREFETCH_HINT;
		break;
	case SOURCE_LINUX_POLICY_ARCHITECTURAL_ONLY:
		policy.disposition = ORLIX_TCTI_TARGET_LINUX_PROOF_NOT_APPLICABLE;
		policy.reason =
			ORLIX_TCTI_TARGET_LINUX_NA_ARCHITECTURAL_SEMANTICS_ONLY;
		break;
	case SOURCE_LINUX_POLICY_UNCLASSIFIED:
	default:
		break;
	}
	return policy;
}

static struct linux_proof_policy system_accessor_linux_policy(void)
{
	return (struct linux_proof_policy) {
		.disposition = ORLIX_TCTI_TARGET_LINUX_PROOF_KSELFTEST_OWNED,
		.reason = ORLIX_TCTI_TARGET_LINUX_NA_NONE,
		.owner_mask = ORLIX_TCTI_TARGET_LINUX_OWNER_SYSTEM_ACCESS,
		.kselftests = system_kselftests,
		.kselftest_count = ARRAY_COUNT(system_kselftests),
	};
}

static struct orlix_tcti_target_linux_proof_disposition_row
linux_source_row(const struct source_manifest_binding *source)
{
	const struct linux_proof_policy policy = source_linux_policy(source);

	return (struct orlix_tcti_target_linux_proof_disposition_row) {
		.subject_kind = ORLIX_TCTI_TARGET_LINUX_PROOF_SOURCE_LEAF,
		.source = {
			.source_index = source->ordinal,
			.name = source->leaf_name,
			.mnemonic = source->mnemonic,
			.operation_id = source->operation_id,
			.encoding_mask = source->encoding_mask,
			.encoding_pattern = source->encoding_pattern,
			.condition_tcnd_hex = source->condition_tcnd_hex,
			.source_offset = source->source_offset,
			.source_length = source->source_length,
		},
		.disposition = policy.disposition,
		.not_applicable_reason = policy.reason,
		.linux_owner_mask = policy.owner_mask,
		.kselftests = policy.kselftests,
		.kselftest_count = policy.kselftest_count,
		.execution_state = ORLIX_TCTI_TARGET_LINUX_EXECUTION_NOT_OBSERVED,
	};
}

static struct orlix_tcti_target_linux_proof_disposition_row
linux_variant_row(size_t ordinal, const struct system_accessor_binding *variant)
{
	const struct linux_proof_policy policy = system_accessor_linux_policy();

	return (struct orlix_tcti_target_linux_proof_disposition_row) {
		.subject_kind = ORLIX_TCTI_TARGET_LINUX_PROOF_SYSTEM_ACCESSOR_VARIANT,
		.source = {
			.source_index = (orlix_tcti_proof_u32)ordinal,
			.secondary_index = variant->accessor_index,
			.tertiary_index = variant->encoding_index,
			.name = variant->name,
			.variant_name = variant->variant_name,
			.operation_id = variant->generic_leaf,
			.encoding_mask = variant->direction,
			.encoding_pattern = variant->disposition,
			.selector_count = variant->selector_count,
			.condition_expression = variant->condition_expression,
			.access_expression = variant->access_expression,
			.concrete_selector = variant->concrete_selector,
			.applicability = variant->applicability,
			.semantics = variant->semantics,
			.implementation = variant->implementation,
			.proof_state = variant->proof_state,
			.identity = variant->selector_identity,
			.condition_identity = variant->condition_identity,
			.access_identity = variant->access_identity,
			.decoder_owner = variant->decoder_owner,
			.execution_owner = variant->execution_owner,
			.kunit_suite = variant->kunit_suite,
			.kunit_case = variant->kunit_case,
			.source_offset = variant->accessor_source_offset,
			.source_length = variant->accessor_source_length,
			.secondary_offset = variant->encoding_source_offset,
			.secondary_length = variant->encoding_source_length,
			.condition_offset = variant->condition_source_offset,
			.condition_length = variant->condition_source_length,
			.access_offset = variant->access_source_offset,
			.access_length = variant->access_source_length,
		},
		.disposition = policy.disposition,
		.not_applicable_reason = policy.reason,
		.linux_owner_mask = policy.owner_mask,
		.kselftests = policy.kselftests,
		.kselftest_count = policy.kselftest_count,
		.execution_state = ORLIX_TCTI_TARGET_LINUX_EXECUTION_NOT_OBSERVED,
	};
}

static bool linux_source_identity_equal(
	const struct orlix_tcti_target_linux_proof_source_identity *left,
	const struct orlix_tcti_target_linux_proof_source_identity *right)
{
	return left->source_index == right->source_index &&
		left->secondary_index == right->secondary_index &&
		left->tertiary_index == right->tertiary_index &&
		left->name && right->name && !strcmp(left->name, right->name) &&
		((!left->mnemonic && !right->mnemonic) ||
		 (left->mnemonic && right->mnemonic &&
		  !strcmp(left->mnemonic, right->mnemonic))) &&
		left->operation_id && right->operation_id &&
		!strcmp(left->operation_id, right->operation_id) &&
		left->encoding_mask == right->encoding_mask &&
		left->encoding_pattern == right->encoding_pattern &&
		left->selector_count == right->selector_count &&
		left->condition_expression == right->condition_expression &&
		left->access_expression == right->access_expression &&
		left->concrete_selector == right->concrete_selector &&
		left->applicability == right->applicability &&
		left->semantics == right->semantics &&
		left->implementation == right->implementation &&
		left->proof_state == right->proof_state &&
		((!left->condition_tcnd_hex && !right->condition_tcnd_hex) ||
		 (left->condition_tcnd_hex && right->condition_tcnd_hex &&
		  !strcmp(left->condition_tcnd_hex, right->condition_tcnd_hex))) &&
		left->identity == right->identity &&
		left->condition_identity == right->condition_identity &&
		left->access_identity == right->access_identity &&
		((!left->variant_name && !right->variant_name) ||
		 (left->variant_name && right->variant_name &&
		  !strcmp(left->variant_name, right->variant_name))) &&
		((!left->decoder_owner && !right->decoder_owner) ||
		 (left->decoder_owner && right->decoder_owner &&
		  !strcmp(left->decoder_owner, right->decoder_owner))) &&
		((!left->execution_owner && !right->execution_owner) ||
		 (left->execution_owner && right->execution_owner &&
		  !strcmp(left->execution_owner, right->execution_owner))) &&
		((!left->kunit_suite && !right->kunit_suite) ||
		 (left->kunit_suite && right->kunit_suite &&
		  !strcmp(left->kunit_suite, right->kunit_suite))) &&
		((!left->kunit_case && !right->kunit_case) ||
		 (left->kunit_case && right->kunit_case &&
		  !strcmp(left->kunit_case, right->kunit_case))) &&
		left->source_offset == right->source_offset &&
		left->source_length == right->source_length &&
		left->secondary_offset == right->secondary_offset &&
		left->secondary_length == right->secondary_length &&
		left->condition_offset == right->condition_offset &&
		left->condition_length == right->condition_length &&
		left->access_offset == right->access_offset &&
		left->access_length == right->access_length;
}

int orlix_tcti_target_linux_source_policy_validate_for_test(
	const struct orlix_tcti_target_linux_proof_source_identity *source)
{
	struct orlix_tcti_target_linux_proof_disposition_row expected;

	if (!source || source->source_index >=
			ORLIX_TCTI_TARGET_LINUX_PROOF_SOURCE_ROWS ||
	    !source_linux_policy_partition_valid())
		return -1;
	expected = linux_source_row(
		&source_manifest_bindings[source->source_index]);
	if (!expected.disposition ||
	    !linux_source_identity_equal(source, &expected.source))
		return -1;
	return 0;
}

static bool linux_provenance_group_equal(
	const struct orlix_tcti_target_linux_proof_disposition_row *row,
	const struct linux_proof_policy *policy)
{
	size_t index;

	if (row->kselftest_count != policy->kselftest_count ||
	    (!row->kselftests && row->kselftest_count))
		return false;
	for (index = 0; index < row->kselftest_count; index++) {
		const struct orlix_tcti_target_kselftest_provenance *left =
			&row->kselftests[index];
		const struct orlix_tcti_target_kselftest_provenance *right =
			&policy->kselftests[index];

		if (strcmp(left->source, right->source) ||
		    strcmp(left->source_sha256, right->source_sha256) ||
		    strcmp(left->build_source, right->build_source) ||
		    strcmp(left->build_source_sha256, right->build_source_sha256) ||
		    strcmp(left->program, right->program) ||
		    strcmp(left->case_name, right->case_name) ||
		    orlix_tcti_target_kselftest_provenance_validate(left))
			return false;
	}
	return true;
}

static bool linux_provenance_group_is_substitution(
	const struct orlix_tcti_target_linux_proof_disposition_row *row)
{
	size_t index;

	for (index = 0; index < row->kselftest_count; index++) {
		const struct orlix_tcti_target_kselftest_provenance *provenance =
			&row->kselftests[index];

		if (empty(provenance->source) || empty(provenance->program) ||
		    strstr(provenance->source, "/hosted_exec/orlix_tcti/tests/") ||
		    strstr(provenance->source, ".swift") ||
		    strstr(provenance->source, ".log") ||
		    strstr(provenance->source, "XCTest") ||
		    strstr(provenance->program, "kunit") ||
		    strstr(provenance->program, "audit") ||
		    strstr(provenance->program, "report"))
			return true;
	}
	return false;
}

static void linux_matrix_error(
	struct orlix_tcti_target_linux_proof_matrix_result *result,
	orlix_tcti_proof_u32 error)
{
	result->error_mask |= error;
	result->errors++;
}

int orlix_tcti_target_linux_proof_matrix_validate(
	const struct orlix_tcti_target_linux_proof_disposition_row *rows,
	size_t count, struct orlix_tcti_target_linux_proof_matrix_result *result)
{
	bool source_seen[ORLIX_TCTI_TARGET_LINUX_PROOF_SOURCE_ROWS] = { false };
	bool variant_seen[ORLIX_TCTI_TARGET_LINUX_PROOF_VARIANT_ROWS] = { false };
	size_t index;

	if (!result)
		return -1;
	memset(result, 0, sizeof(*result));
	result->total_rows = count;
	if (!source_linux_policy_partition_valid()) {
		result->malformed_rows = ORLIX_TCTI_TARGET_LINUX_PROOF_SOURCE_ROWS;
		linux_matrix_error(result,
			ORLIX_TCTI_TARGET_LINUX_MATRIX_ERROR_MALFORMED);
		return -1;
	}
	if (!rows || count != ORLIX_TCTI_TARGET_LINUX_PROOF_TOTAL_ROWS)
		linux_matrix_error(result, ORLIX_TCTI_TARGET_LINUX_MATRIX_ERROR_COUNT);
	if (!rows)
		return -1;
	for (index = 0; index < count; index++) {
		const struct orlix_tcti_target_linux_proof_disposition_row *row =
			&rows[index];
		struct orlix_tcti_target_linux_proof_disposition_row expected;
		struct linux_proof_policy policy;
		bool *seen;

		if (row->subject_kind == ORLIX_TCTI_TARGET_LINUX_PROOF_SOURCE_LEAF &&
		    row->source.source_index < ARRAY_COUNT(source_manifest_bindings)) {
			expected = linux_source_row(
				&source_manifest_bindings[row->source.source_index]);
			policy = source_linux_policy(
				&source_manifest_bindings[row->source.source_index]);
			seen = &source_seen[row->source.source_index];
			result->source_leaf_rows++;
		} else if (row->subject_kind ==
			   ORLIX_TCTI_TARGET_LINUX_PROOF_SYSTEM_ACCESSOR_VARIANT &&
			   row->source.source_index <
				ARRAY_COUNT(system_accessor_bindings)) {
			expected = linux_variant_row(row->source.source_index,
				&system_accessor_bindings[row->source.source_index]);
			policy = system_accessor_linux_policy();
			seen = &variant_seen[row->source.source_index];
			result->semantic_variant_rows++;
		} else {
			result->malformed_rows++;
			linux_matrix_error(result,
				ORLIX_TCTI_TARGET_LINUX_MATRIX_ERROR_MALFORMED);
			continue;
		}
		if (*seen) {
			result->duplicate_rows++;
			linux_matrix_error(result,
				ORLIX_TCTI_TARGET_LINUX_MATRIX_ERROR_DUPLICATE);
			continue;
		}
		*seen = true;
		if (!linux_source_identity_equal(&row->source, &expected.source)) {
			result->stale_rows++;
			linux_matrix_error(result,
				ORLIX_TCTI_TARGET_LINUX_MATRIX_ERROR_STALE);
			continue;
		}
		if (row->disposition != policy.disposition ||
		    row->not_applicable_reason != policy.reason ||
		    row->linux_owner_mask != policy.owner_mask ||
		    row->execution_state !=
			ORLIX_TCTI_TARGET_LINUX_EXECUTION_NOT_OBSERVED) {
			result->ambiguous_rows++;
			linux_matrix_error(result,
				ORLIX_TCTI_TARGET_LINUX_MATRIX_ERROR_AMBIGUOUS);
			continue;
		}
		if (row->disposition ==
		    ORLIX_TCTI_TARGET_LINUX_PROOF_KSELFTEST_OWNED) {
			result->kselftest_owned_rows++;
			if (linux_provenance_group_is_substitution(row)) {
				result->substitution_rows++;
				linux_matrix_error(result,
					ORLIX_TCTI_TARGET_LINUX_MATRIX_ERROR_SUBSTITUTION);
			} else if (!linux_provenance_group_equal(row, &policy)) {
				result->invalid_provenance_rows++;
				linux_matrix_error(result,
					ORLIX_TCTI_TARGET_LINUX_MATRIX_ERROR_PROVENANCE);
			}
		} else if (row->disposition ==
			   ORLIX_TCTI_TARGET_LINUX_PROOF_NOT_APPLICABLE &&
			   row->not_applicable_reason !=
				ORLIX_TCTI_TARGET_LINUX_NA_NONE &&
			   !row->kselftests && !row->kselftest_count &&
			   !row->linux_owner_mask) {
			result->not_applicable_rows++;
		} else {
			result->malformed_rows++;
			linux_matrix_error(result,
				ORLIX_TCTI_TARGET_LINUX_MATRIX_ERROR_MALFORMED);
		}
	}
	for (index = 0; index < ARRAY_COUNT(source_seen); index++)
		if (!source_seen[index])
			result->missing_rows++;
	for (index = 0; index < ARRAY_COUNT(variant_seen); index++)
		if (!variant_seen[index])
			result->missing_rows++;
	if (result->missing_rows)
		linux_matrix_error(result, ORLIX_TCTI_TARGET_LINUX_MATRIX_ERROR_MISSING);
	return result->error_mask ? -1 : 0;
}

static struct orlix_tcti_target_linux_proof_disposition_row
linux_proof_rows[ORLIX_TCTI_TARGET_LINUX_PROOF_TOTAL_ROWS];
static bool linux_proof_rows_initialized;
#ifdef __KERNEL__
static DEFINE_MUTEX(linux_proof_rows_initialization_lock);
#else
static pthread_once_t linux_proof_rows_initialization_once = PTHREAD_ONCE_INIT;
#endif

static void linux_proof_rows_initialize(void)
{
	size_t index;

	for (index = 0; index < ARRAY_COUNT(source_manifest_bindings); index++)
		linux_proof_rows[index] =
			linux_source_row(&source_manifest_bindings[index]);
	for (index = 0; index < ARRAY_COUNT(system_accessor_bindings); index++)
		linux_proof_rows[ARRAY_COUNT(source_manifest_bindings) + index] =
			linux_variant_row(index, &system_accessor_bindings[index]);
	linux_proof_rows_initialized = true;
}

const struct orlix_tcti_target_linux_proof_disposition_row *
orlix_tcti_target_linux_proof_dispositions(size_t *count)
{
#ifdef __KERNEL__
	mutex_lock(&linux_proof_rows_initialization_lock);
	if (!linux_proof_rows_initialized)
		linux_proof_rows_initialize();
	mutex_unlock(&linux_proof_rows_initialization_lock);
#else
	if (pthread_once(&linux_proof_rows_initialization_once,
			 linux_proof_rows_initialize))
		linux_proof_rows_initialized = false;
#endif
	if (!linux_proof_rows_initialized) {
		if (count)
			*count = 0;
		return NULL;
	}
	if (count)
		*count = ARRAY_COUNT(linux_proof_rows);
	return linux_proof_rows;
}
