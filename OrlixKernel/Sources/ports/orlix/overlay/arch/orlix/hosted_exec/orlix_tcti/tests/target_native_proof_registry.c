// SPDX-License-Identifier: GPL-2.0-only
#include "target_instruction_artifact.h"
#include "target_native_proof_contract.h"
#include "target_native_proof_registry_private.h"
#include "target_proof_registry_provenance.h"

#ifdef __KERNEL__
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/overflow.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#else
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#endif

#define ORLIX_TCTI_ARRAY_COUNT(values) (sizeof(values) / sizeof((values)[0]))

#ifndef ORLIX_TCTI_KERNEL_ARCHIVE_INPUT_SHA256
#error "ORLIX_TCTI_KERNEL_ARCHIVE_INPUT_SHA256 must bind the exact archive inputs"
#endif
#ifndef ORLIX_TCTI_KERNEL_CONFIG_SHA256
#error "ORLIX_TCTI_KERNEL_CONFIG_SHA256 must bind the exact kernel config"
#endif
#ifndef ORLIX_TCTI_BUILD_PROFILE_SHA256
#error "ORLIX_TCTI_BUILD_PROFILE_SHA256 must bind the exact build profile"
#endif
#ifndef ORLIX_TCTI_DURABLE_SOURCE_REVISION
#error "ORLIX_TCTI_DURABLE_SOURCE_REVISION must bind the durable source revision"
#endif
#ifndef ORLIX_TCTI_INSTRUCTION_ARTIFACT_SHA256
#error "ORLIX_TCTI_INSTRUCTION_ARTIFACT_SHA256 must bind the complete instruction artifact"
#endif
#ifndef ORLIX_TCTI_NATIVE_RUNTIME_PROFILE
#define ORLIX_TCTI_NATIVE_RUNTIME_PROFILE \
	ORLIX_TCTI_NATIVE_RUNTIME_PROFILE_DEVELOPMENT
#endif

struct native_source_manifest_binding {
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

#define ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, name, mnemonic, operation, \
	mask, pattern, condition, offset, length) \
	{ ordinal, name, mnemonic, operation, mask, pattern, condition, offset, length },
static const struct native_source_manifest_binding
native_source_manifest_bindings[] = {
#include "../isa/source_manifest.def"
};
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE

struct native_source_classification_binding {
	orlix_tcti_proof_u32 classification;
};

#define NATIVE_SOURCE_CLASS_UNCLASSIFIED \
	ORLIX_TCTI_NATIVE_SOURCE_CLASS_UNCLASSIFIED
#define NATIVE_SOURCE_CLASS_REQUIRED ORLIX_TCTI_NATIVE_SOURCE_CLASS_REQUIRED_EL0
#define NATIVE_SOURCE_CLASS_NON_EL0 ORLIX_TCTI_NATIVE_SOURCE_CLASS_NON_EL0
#define NATIVE_SOURCE_CLASS_ARCHITECTURALLY_UNDEFINED \
	ORLIX_TCTI_NATIVE_SOURCE_CLASS_ARCH_UNDEFINED
#define ORLIX_TCTI_A64_TARGET_CLASSIFICATION(name, classification, relation, \
	evidence, proof, note) { NATIVE_SOURCE_CLASS_##classification },
static const struct native_source_classification_binding
native_source_classifications[] = {
#include "../isa/target_classification.def"
};
#undef ORLIX_TCTI_A64_TARGET_CLASSIFICATION
#undef NATIVE_SOURCE_CLASS_ARCHITECTURALLY_UNDEFINED
#undef NATIVE_SOURCE_CLASS_NON_EL0
#undef NATIVE_SOURCE_CLASS_REQUIRED
#undef NATIVE_SOURCE_CLASS_UNCLASSIFIED

_Static_assert(ORLIX_TCTI_ARRAY_COUNT(native_source_manifest_bindings) ==
		       ORLIX_TCTI_ARRAY_COUNT(native_source_classifications),
	       "every source leaf requires canonical classification identity");

static bool native_empty(const char *text)
{
	return !text || !text[0];
}

struct native_semantic_provenance_row {
	const char *locator;
	const char *sha256;
};

#define ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_SOURCE(...)
#define ORLIX_TCTI_A64_DDI0602_PROVENANCE_ROW(ordinal, leaf, file, decode, \
	decode_digest, decode_sections, decode_helpers, decode_closure, execute, \
	execute_digest, execute_sections, execute_helpers, execute_closure) \
	[ordinal] = { execute, execute_digest },
#define ORLIX_TCTI_A64_OFFICIAL_SEMANTICS_NOT_SPECIFIED_ROW(ordinal, leaf, \
	locator, offset, length, digest) [ordinal] = { locator, digest },
static const struct native_semantic_provenance_row
native_semantic_provenance[ORLIX_TCTI_TARGET_LINUX_PROOF_SOURCE_ROWS] = {
#include "../isa/target_asl_availability.def"
};
#undef ORLIX_TCTI_A64_OFFICIAL_SEMANTICS_NOT_SPECIFIED_ROW
#undef ORLIX_TCTI_A64_DDI0602_PROVENANCE_ROW
#undef ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_SOURCE

static struct orlix_tcti_native_proof_registry_entry *native_registry_entries;
static struct orlix_tcti_native_capture_declaration *native_capture_declarations;
static size_t native_registry_entry_count;
static bool native_registry_initialized;
#ifdef __KERNEL__
static DEFINE_MUTEX(native_registry_initialization_lock);
#else
static pthread_once_t native_registry_once = PTHREAD_ONCE_INIT;
#endif

static orlix_tcti_proof_u64 native_registry_hash(
	orlix_tcti_proof_u64 value, const void *bytes, size_t length)
{
	const unsigned char *cursor = bytes;
	size_t index;

	for (index = 0; index < length; index++) {
		value ^= cursor[index];
		value *= ORLIX_TCTI_PROOF_U64_C(1099511628211);
	}
	return value;
}

static orlix_tcti_proof_u64 native_registry_hash_u32(
	orlix_tcti_proof_u64 value, orlix_tcti_proof_u32 field)
{
	unsigned char bytes[4] = {
		(unsigned char)field,
		(unsigned char)(field >> 8),
		(unsigned char)(field >> 16),
		(unsigned char)(field >> 24),
	};

	return native_registry_hash(value, bytes, sizeof(bytes));
}

static orlix_tcti_proof_u64 native_registry_hash_u64(
	orlix_tcti_proof_u64 value, orlix_tcti_proof_u64 field)
{
	unsigned char bytes[8];
	size_t index;

	for (index = 0; index < sizeof(bytes); index++)
		bytes[index] = (unsigned char)(field >> (index * 8U));
	return native_registry_hash(value, bytes, sizeof(bytes));
}

static orlix_tcti_proof_u64 native_registry_hash_text(
	orlix_tcti_proof_u64 value, const char *text)
{
	return native_registry_hash(value, text, strlen(text) + 1U);
}

static bool native_registry_copy(char *destination, size_t capacity,
				 const char *source)
{
	size_t length;

	if (!destination || !capacity || native_empty(source))
		return false;
	length = strlen(source);
	if (length >= capacity)
		return false;
	memcpy(destination, source, length + 1U);
	return true;
}

static orlix_tcti_proof_u32 native_observation_kind(
	orlix_tcti_proof_u32 obligation)
{
	switch (obligation) {
	case ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE:
		return ORLIX_TCTI_NATIVE_OBSERVATION_DECODE;
	case ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS:
		return ORLIX_TCTI_NATIVE_OBSERVATION_LEGAL_ENCODING;
	case ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS:
		return ORLIX_TCTI_NATIVE_OBSERVATION_REJECTED_ENCODING;
	case ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS:
		return ORLIX_TCTI_NATIVE_OBSERVATION_GPR;
	case ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY:
		return ORLIX_TCTI_NATIVE_OBSERVATION_MEMORY;
	case ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC:
		return ORLIX_TCTI_NATIVE_OBSERVATION_PC;
	case ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS:
		return ORLIX_TCTI_NATIVE_OBSERVATION_FAULT;
	case ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ATOMICITY:
		return ORLIX_TCTI_NATIVE_OBSERVATION_ATOMICITY;
	case ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ORDERING:
		return ORLIX_TCTI_NATIVE_OBSERVATION_ORDERING;
	case ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS:
		return ORLIX_TCTI_NATIVE_OBSERVATION_PSTATE;
	case ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LINUX_INTERFACE:
		return ORLIX_TCTI_NATIVE_OBSERVATION_LINUX_INTERFACE;
	case ORLIX_TCTI_TARGET_PROOF_OBLIGATION_OPERATIONAL_NOTE:
		return ORLIX_TCTI_NATIVE_OBSERVATION_OPERATIONAL_NOTE;
	case ORLIX_TCTI_TARGET_PROOF_OBLIGATION_RESULT:
		return ORLIX_TCTI_NATIVE_OBSERVATION_RESULT;
	case ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD:
		return ORLIX_TCTI_NATIVE_OBSERVATION_FP_SIMD;
	case ORLIX_TCTI_TARGET_PROOF_OBLIGATION_SVE:
		return ORLIX_TCTI_NATIVE_OBSERVATION_SVE;
	case ORLIX_TCTI_TARGET_PROOF_OBLIGATION_SME_TASK_STATE:
		return ORLIX_TCTI_NATIVE_OBSERVATION_SME_TASK_STATE;
	case ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ARITHMETIC:
		return ORLIX_TCTI_NATIVE_OBSERVATION_ARITHMETIC;
	case ORLIX_TCTI_TARGET_PROOF_OBLIGATION_SYSTEM_PT_CONTROL:
		return ORLIX_TCTI_NATIVE_OBSERVATION_SYSTEM_PT_CONTROL;
	default:
		return 0;
	}
}

static bool native_size_add(size_t *value, size_t increment)
{
#ifdef __KERNEL__
	return !check_add_overflow(*value, increment, value);
#else
	if (increment > SIZE_MAX - *value)
		return false;
	*value += increment;
	return true;
#endif
}

/* source is the canonical byte width for an EXACT selector, or the fixed
 * declaration prefix for a declaration-derived memory range. */
static const struct orlix_tcti_native_capture_selector
native_register_capture_selectors[] = {
	{
		.kind = ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_RESULT,
		.selector_id = 0U,
		.source = 32U, /* reason, status, fault address/access, entry/return PC */
		.size_rule = ORLIX_TCTI_NATIVE_CAPTURE_SIZE_EXACT,
		.flags = ORLIX_TCTI_NATIVE_CAPTURE_REQUIRED |
			ORLIX_TCTI_NATIVE_CAPTURE_APPLICABLE,
	},
	{
		.kind = ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_GPR,
		.selector_id = 1U,
		.source = 33U * sizeof(orlix_tcti_proof_u64), /* X0-X30 and SP */
		.size_rule = ORLIX_TCTI_NATIVE_CAPTURE_SIZE_EXACT,
		.flags = ORLIX_TCTI_NATIVE_CAPTURE_REQUIRED |
			ORLIX_TCTI_NATIVE_CAPTURE_APPLICABLE |
			ORLIX_TCTI_NATIVE_CAPTURE_AFTER,
	},
	{
		/* FPCR, FPSR, validity, then V0-V31 as little-endian lanes. */
		.kind = ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_FP_SIMD,
		.selector_id = 2U,
		.source = 24U,
		.size_rule = ORLIX_TCTI_NATIVE_CAPTURE_SIZE_DECLARATION_DERIVED,
		.flags = ORLIX_TCTI_NATIVE_CAPTURE_REQUIRED |
			ORLIX_TCTI_NATIVE_CAPTURE_APPLICABLE |
			ORLIX_TCTI_NATIVE_CAPTURE_AFTER,
	},
	{
		/* VL, Z0-Z31, P0-P15, and FFR all use their declared lengths. */
		.kind = ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_SVE,
		.selector_id = 3U,
		.source = 24U,
		.size_rule = ORLIX_TCTI_NATIVE_CAPTURE_SIZE_DECLARATION_DERIVED,
		.flags = ORLIX_TCTI_NATIVE_CAPTURE_REQUIRED |
			ORLIX_TCTI_NATIVE_CAPTURE_APPLICABLE |
			ORLIX_TCTI_NATIVE_CAPTURE_AFTER,
	},
	{
		/* Streaming mode, SVL, ZA, and ZT0 are independently length-bound. */
		.kind = ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_SME,
		.selector_id = 4U,
		.source = 24U,
		.size_rule = ORLIX_TCTI_NATIVE_CAPTURE_SIZE_DECLARATION_DERIVED,
		.flags = ORLIX_TCTI_NATIVE_CAPTURE_REQUIRED |
			ORLIX_TCTI_NATIVE_CAPTURE_APPLICABLE |
			ORLIX_TCTI_NATIVE_CAPTURE_AFTER,
	},
	{
		.kind = ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_DECODED_FIELD,
		.selector_id = 5U,
		.source = 48U,
		.size_rule = ORLIX_TCTI_NATIVE_CAPTURE_SIZE_EXACT,
		.flags = ORLIX_TCTI_NATIVE_CAPTURE_REQUIRED |
			ORLIX_TCTI_NATIVE_CAPTURE_APPLICABLE |
			ORLIX_TCTI_NATIVE_CAPTURE_BEFORE,
	},
	{
		.kind = ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_MEMORY,
		.selector_id = 6U,
		.source = 16U, /* address, length, effect, followed by exact bytes */
		.size_rule = ORLIX_TCTI_NATIVE_CAPTURE_SIZE_DECLARATION_DERIVED,
		.flags = ORLIX_TCTI_NATIVE_CAPTURE_REQUIRED |
			ORLIX_TCTI_NATIVE_CAPTURE_APPLICABLE,
	},
	{
		.kind = ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_FAULT,
		.selector_id = 7U,
		.source = 24U,
		.size_rule = ORLIX_TCTI_NATIVE_CAPTURE_SIZE_EXACT,
		.flags = ORLIX_TCTI_NATIVE_CAPTURE_REQUIRED |
			ORLIX_TCTI_NATIVE_CAPTURE_APPLICABLE,
	},
	{
		.kind = ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_ARITHMETIC,
		.selector_id = 8U,
		.source = 32U,
		.size_rule = ORLIX_TCTI_NATIVE_CAPTURE_SIZE_EXACT,
		.flags = ORLIX_TCTI_NATIVE_CAPTURE_REQUIRED |
			ORLIX_TCTI_NATIVE_CAPTURE_APPLICABLE |
			ORLIX_TCTI_NATIVE_CAPTURE_AFTER,
	},
	{
		.kind = ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_ATOMICITY,
		.selector_id = 9U,
		.source = 24U,
		.size_rule = ORLIX_TCTI_NATIVE_CAPTURE_SIZE_EXACT,
		.flags = ORLIX_TCTI_NATIVE_CAPTURE_REQUIRED |
			ORLIX_TCTI_NATIVE_CAPTURE_APPLICABLE,
	},
	{
		.kind = ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_ORDERING,
		.selector_id = 10U,
		.source = 16U,
		.size_rule = ORLIX_TCTI_NATIVE_CAPTURE_SIZE_EXACT,
		.flags = ORLIX_TCTI_NATIVE_CAPTURE_REQUIRED |
			ORLIX_TCTI_NATIVE_CAPTURE_APPLICABLE,
	},
	{
		.kind = ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_SYSTEM_CONTROL,
		.selector_id = 11U,
		.source = 32U,
		.size_rule = ORLIX_TCTI_NATIVE_CAPTURE_SIZE_EXACT,
		.flags = ORLIX_TCTI_NATIVE_CAPTURE_REQUIRED |
			ORLIX_TCTI_NATIVE_CAPTURE_APPLICABLE,
	},
	{
		.kind = ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_LINUX_INTERFACE,
		.selector_id = 12U,
		.source = 24U,
		.size_rule = ORLIX_TCTI_NATIVE_CAPTURE_SIZE_EXACT,
		.flags = ORLIX_TCTI_NATIVE_CAPTURE_REQUIRED |
			ORLIX_TCTI_NATIVE_CAPTURE_APPLICABLE |
			ORLIX_TCTI_NATIVE_CAPTURE_AFTER,
	},
};

static bool native_capture_declaration_build(
	struct orlix_tcti_native_capture_declaration *declaration,
	struct orlix_tcti_native_proof_registry_entry *entry)
{
	if (!declaration || !entry || !entry->production_capture)
		return false;
	*declaration = (struct orlix_tcti_native_capture_declaration) {
		.schema_version = ORLIX_TCTI_NATIVE_WIRE_SCHEMA_VERSION,
		.source_ordinal = entry->source.source_ordinal,
		.semantic_variant_identity = entry->source.semantic_variant_identity,
		.selector = entry->source.concrete_selector,
		.direction = entry->source.variant_direction,
		.condition_identity = entry->source.condition_identity,
		.obligation = entry->obligation,
		.required_record_kind = entry->observation_kind,
		.selector_count = ORLIX_TCTI_ARRAY_COUNT(native_register_capture_selectors),
		.selectors = native_register_capture_selectors,
	};
	declaration->declaration_identity =
		orlix_tcti_native_capture_declaration_identity(declaration);
	if (orlix_tcti_native_capture_declaration_validate(declaration))
		return false;
	entry->capture_declaration = declaration;
	return true;
}

static bool native_condition_capacity_is_source_bound(void)
{
	size_t maximum = 0;
	size_t index;

	for (index = 0; index < ORLIX_TCTI_ARRAY_COUNT(
			native_source_manifest_bindings); index++) {
		size_t length = strlen(
			native_source_manifest_bindings[index].condition_tcnd_hex);

		if (length > maximum)
			maximum = length;
	}
	return maximum + 1U == ORLIX_TCTI_NATIVE_CONDITION_HEX_MAX;
}

static orlix_tcti_proof_u64 native_instruction_artifact_identity(
	const struct orlix_tcti_target_instruction_artifact *artifact)
{
	orlix_tcti_proof_u64 identity =
		ORLIX_TCTI_PROOF_U64_C(1469598103934665603);
	size_t index;

	identity = native_registry_hash_u32(identity, artifact->version);
	identity = native_registry_hash_text(identity, artifact->architecture);
	identity = native_registry_hash_text(identity, artifact->build);
	identity = native_registry_hash_text(identity, artifact->reference);
	identity = native_registry_hash_text(identity, artifact->schema);
	identity = native_registry_hash_text(identity, artifact->source_sha256);
	identity = native_registry_hash_u64(identity, artifact->leaf_count);
	for (index = 0; index < artifact->leaf_count; index++) {
		const struct orlix_tcti_target_instruction_artifact_leaf *leaf =
			&artifact->leaves[index];

		identity = native_registry_hash_u32(identity, leaf->name_offset);
		identity = native_registry_hash_u32(identity, leaf->mnemonic_offset);
		identity = native_registry_hash_u32(identity, leaf->operation_offset);
		identity = native_registry_hash_u32(identity, leaf->encoding_mask);
		identity = native_registry_hash_u32(identity, leaf->encoding_pattern);
		identity = native_registry_hash_u32(identity, leaf->condition_offset);
		identity = native_registry_hash_u32(identity, leaf->condition_length);
		identity = native_registry_hash_u32(identity, leaf->operand_first);
		identity = native_registry_hash_u32(identity, leaf->operand_count);
		identity = native_registry_hash_u32(identity, leaf->fixed_operand_first);
		identity = native_registry_hash_u32(identity, leaf->fixed_operand_count);
	}
	identity = native_registry_hash_u64(identity, artifact->operand_count);
	for (index = 0; index < artifact->operand_count; index++) {
		const struct orlix_tcti_target_instruction_artifact_operand *operand =
			&artifact->operands[index];

		identity = native_registry_hash_u32(identity, operand->name_offset);
		identity = native_registry_hash_u32(identity, operand->leaf_index);
		identity = native_registry_hash_u32(identity, operand->condition_offset);
		identity = native_registry_hash_u32(identity, operand->condition_length);
		identity = native_registry_hash_u32(identity, operand->variable_mask);
		identity = native_registry_hash_u32(identity, operand->start);
		identity = native_registry_hash_u32(identity, operand->width);
	}
	identity = native_registry_hash_u64(identity, artifact->fixed_operand_count);
	for (index = 0; index < artifact->fixed_operand_count; index++) {
		const struct orlix_tcti_target_instruction_artifact_fixed_operand *operand =
			&artifact->fixed_operands[index];

		identity = native_registry_hash_u32(identity, operand->name_offset);
		identity = native_registry_hash_u32(identity, operand->leaf_index);
		identity = native_registry_hash_u32(identity, operand->condition_offset);
		identity = native_registry_hash_u32(identity, operand->condition_length);
		identity = native_registry_hash_u32(identity, operand->fixed_mask);
		identity = native_registry_hash_u32(identity, operand->fixed_value);
		identity = native_registry_hash_u32(identity, operand->source_offset);
		identity = native_registry_hash_u32(identity, operand->source_length);
		identity = native_registry_hash_u32(identity,
			operand->source_identity_offset);
		identity = native_registry_hash_u32(identity, operand->start);
		identity = native_registry_hash_u32(identity, operand->width);
	}
	identity = native_registry_hash_u64(identity,
		artifact->instruction_alias_count);
	for (index = 0; index < artifact->instruction_alias_count; index++) {
		const struct orlix_tcti_target_instruction_artifact_instruction_alias
			*alias = &artifact->instruction_aliases[index];

		identity = native_registry_hash_u32(identity, alias->ordinal);
		identity = native_registry_hash_u32(identity, alias->name_offset);
		identity = native_registry_hash_u32(identity,
			alias->declared_operation_offset);
		identity = native_registry_hash_u32(identity,
			alias->resolved_operation_offset);
		identity = native_registry_hash_u32(identity, alias->condition_offset);
		identity = native_registry_hash_u32(identity, alias->condition_length);
		identity = native_registry_hash_u32(identity, alias->source_offset);
		identity = native_registry_hash_u32(identity, alias->source_length);
		identity = native_registry_hash_u32(identity,
			alias->source_identity_offset);
		identity = native_registry_hash_u32(identity,
			alias->condition_source_offset);
		identity = native_registry_hash_u32(identity,
			alias->condition_source_length);
		identity = native_registry_hash_u32(identity,
			alias->condition_identity_offset);
		identity = native_registry_hash_u32(identity,
			alias->preferred_source_offset);
		identity = native_registry_hash_u32(identity,
			alias->preferred_source_length);
		identity = native_registry_hash_u32(identity,
			alias->preferred_identity_offset);
		identity = native_registry_hash_u32(identity,
			alias->predicate_sha256_offset);
		identity = native_registry_hash_u32(identity, alias->relation_kind);
		identity = native_registry_hash_u32(identity, alias->predicate_kind);
		identity = native_registry_hash_u32(identity, alias->preferred_present);
	}
	identity = native_registry_hash_u64(identity,
		artifact->operation_alias_count);
	for (index = 0; index < artifact->operation_alias_count; index++) {
		const struct orlix_tcti_target_instruction_artifact_operation_alias
			*alias = &artifact->operation_aliases[index];

		identity = native_registry_hash_u32(identity,
			alias->declared_operation_offset);
		identity = native_registry_hash_u32(identity,
			alias->target_operation_offset);
		identity = native_registry_hash_u32(identity,
			alias->resolved_operation_offset);
		identity = native_registry_hash_u32(identity, alias->source_offset);
		identity = native_registry_hash_u32(identity, alias->source_length);
		identity = native_registry_hash_u32(identity,
			alias->source_identity_offset);
		identity = native_registry_hash_u32(identity, alias->predicate_offset);
		identity = native_registry_hash_u32(identity, alias->predicate_length);
		identity = native_registry_hash_u32(identity,
			alias->predicate_sha256_offset);
		identity = native_registry_hash_u32(identity, alias->relation_kind);
		identity = native_registry_hash_u32(identity, alias->predicate_kind);
	}
	identity = native_registry_hash_u64(identity,
		artifact->operational_note_count);
	for (index = 0; index < artifact->operational_note_count; index++) {
		const struct orlix_tcti_target_instruction_artifact_operational_note
			*note = &artifact->operational_notes[index];

		identity = native_registry_hash_u32(identity, note->leaf_index);
		identity = native_registry_hash_u32(identity, note->source_offset);
		identity = native_registry_hash_u32(identity, note->source_length);
		identity = native_registry_hash_u32(identity,
			note->source_identity_offset);
		identity = native_registry_hash_u32(identity,
			note->source_sha256_offset);
		identity = native_registry_hash_u32(identity, note->kind);
	}
	identity = native_registry_hash_u64(identity, artifact->string_pool_size);
	identity = native_registry_hash(identity, artifact->string_pool,
					 artifact->string_pool_size);
	identity = native_registry_hash_u64(identity, artifact->condition_pool_size);
	identity = native_registry_hash(identity, artifact->condition_pool,
					 artifact->condition_pool_size);
	return identity ? identity : 1U;
}

static bool native_matrix_matches_binding(
	const struct orlix_tcti_target_linux_proof_disposition_row *matrix,
	const struct orlix_tcti_target_proof_binding *binding)
{
	if (matrix->subject_kind == ORLIX_TCTI_TARGET_LINUX_PROOF_SOURCE_LEAF)
		return matrix->source.source_index == binding->source_ordinal;
	/* A direct-leaf witness never authorizes a SystemAccessor variant. */
	return false;
}

static const struct orlix_tcti_target_production_capture_binding *
native_production_capture_binding(
	const struct orlix_tcti_target_proof_registry_entry *proof,
	size_t case_index, orlix_tcti_proof_u32 source_ordinal,
	orlix_tcti_proof_u32 obligation)
{
	const struct orlix_tcti_target_production_capture_binding *bindings;
	const struct orlix_tcti_target_production_capture_binding *found = NULL;
	size_t count;
	size_t index;

	bindings = orlix_tcti_target_production_capture_bindings(&count);
	for (index = 0; bindings && index < count; index++) {
		const struct orlix_tcti_target_production_capture_binding *candidate =
			&bindings[index];

		if (candidate->source_ordinal != source_ordinal ||
		    candidate->obligation != obligation ||
		    !orlix_tcti_target_production_capture_binding_applies(
			candidate, 1U, proof, case_index))
			continue;
		if (found)
			return NULL;
		found = candidate;
	}
	return found;
}

static bool native_registry_row_build(
	struct orlix_tcti_native_proof_registry_entry *native,
	const struct orlix_tcti_target_proof_registry_entry *proof,
	const struct orlix_tcti_target_proof_binding *binding,
	const struct orlix_tcti_target_proof_case *proof_case,
	size_t case_index,
	const struct orlix_tcti_target_linux_proof_disposition_row *matrix,
	orlix_tcti_proof_u32 obligation,
	const struct orlix_tcti_target_instruction_artifact *artifact)
{
	const struct orlix_tcti_target_instruction_artifact_leaf *leaf;
	const struct native_source_manifest_binding *source;
	const struct native_semantic_provenance_row *semantic;
	struct orlix_tcti_target_kunit_provenance_identity provenance;
	const struct orlix_tcti_target_production_capture_binding *capture;
	orlix_tcti_proof_u64 identity;

	orlix_tcti_proof_u32 source_ordinal;

	if (!native || !proof || !binding || !proof_case || !matrix || !artifact ||
	    !native_observation_kind(obligation) ||
	    orlix_tcti_target_kunit_provenance_identity(
		proof, proof_case->name, &provenance))
		return false;
	source_ordinal = binding->source_ordinal;
	capture = native_production_capture_binding(proof, case_index,
						    source_ordinal, obligation);
	if (source_ordinal >= artifact->leaf_count ||
	    source_ordinal >=
		ORLIX_TCTI_ARRAY_COUNT(native_source_manifest_bindings))
		return false;
	leaf = &artifact->leaves[source_ordinal];
	source = &native_source_manifest_bindings[source_ordinal];
	semantic = &native_semantic_provenance[source_ordinal];
	if (source->ordinal != source_ordinal ||
	    strcmp(source->leaf_name, binding->leaf_name) ||
	    strcmp(source->mnemonic, binding->mnemonic) ||
	    source->encoding_mask != binding->encoding_mask ||
	    source->encoding_pattern != binding->encoding_pattern ||
	    strcmp(source->condition_tcnd_hex, binding->condition_tcnd_hex) ||
	    strcmp(source->operation_id, proof->operation_id) ||
	    native_empty(semantic->locator) || native_empty(semantic->sha256))
		return false;

	memset(native, 0, sizeof(*native));
	native->source.subject_kind = matrix->subject_kind ==
		ORLIX_TCTI_TARGET_LINUX_PROOF_SOURCE_LEAF ?
		ORLIX_TCTI_NATIVE_SUBJECT_SOURCE_LEAF :
		ORLIX_TCTI_NATIVE_SUBJECT_SEMANTIC_VARIANT;
	native->source.classification =
		native_source_classifications[source_ordinal].classification;
	native->source.classification_present = 1U;
	native->source.source_ordinal = source->ordinal;
	native->source.source_index = matrix->source.source_index;
	native->source.secondary_index = matrix->source.secondary_index;
	native->source.tertiary_index = matrix->source.tertiary_index;
	if (matrix->subject_kind ==
	    ORLIX_TCTI_TARGET_LINUX_PROOF_SYSTEM_ACCESSOR_VARIANT) {
		native->source.semantic_variant_ordinal = matrix->source.source_index;
		native->source.variant_direction = matrix->source.encoding_mask;
		native->source.variant_disposition = matrix->source.encoding_pattern;
		native->source.semantic_variant_identity = matrix->source.identity;
	}
	native->source.encoding_mask = matrix->subject_kind ==
		ORLIX_TCTI_TARGET_LINUX_PROOF_SOURCE_LEAF ?
		source->encoding_mask : binding->encoding_mask;
	native->source.encoding_pattern = matrix->subject_kind ==
		ORLIX_TCTI_TARGET_LINUX_PROOF_SOURCE_LEAF ?
		source->encoding_pattern : binding->encoding_pattern;
	native->source.access_expression = matrix->source.access_expression;
	native->source.selector_count = matrix->source.selector_count;
	native->source.condition_expression = matrix->source.condition_expression;
	native->source.concrete_selector = matrix->source.concrete_selector;
	native->source.accessor_applicability = matrix->source.applicability;
	native->source.accessor_semantics = matrix->source.semantics;
	native->source.accessor_implementation = matrix->source.implementation;
	native->source.accessor_proof_state = matrix->source.proof_state;
	native->source.source_offset = matrix->source.source_offset;
	native->source.source_length = matrix->source.source_length;
	native->source.secondary_offset = matrix->source.secondary_offset;
	native->source.secondary_length = matrix->source.secondary_length;
	native->source.condition_offset = matrix->source.condition_offset;
	native->source.condition_length = matrix->source.condition_length;
	native->source.access_identity = matrix->source.access_identity;
	native->source.access_offset = matrix->source.access_offset;
	native->source.access_length = matrix->source.access_length;
	identity = ORLIX_TCTI_PROOF_U64_C(1469598103934665603);
	identity = native_registry_hash(identity, &source->ordinal,
				       sizeof(source->ordinal));
	identity = native_registry_hash(identity, &source->source_offset,
				       sizeof(source->source_offset));
	identity = native_registry_hash(identity, &source->source_length,
				       sizeof(source->source_length));
	identity = native_registry_hash(identity, source->leaf_name,
				       strlen(source->leaf_name));
	native->source.source_identity = matrix->source.identity ?
		matrix->source.identity : (identity ? identity : 1U);
	if (!native->source.condition_length) {
		native->source.condition_offset = leaf->condition_offset;
		native->source.condition_length = leaf->condition_length;
	}
	identity = native_registry_hash(
		ORLIX_TCTI_PROOF_U64_C(1469598103934665603),
		artifact->condition_pool + leaf->condition_offset,
		leaf->condition_length);
	native->source.condition_identity = matrix->source.condition_identity ?
		matrix->source.condition_identity : (identity ? identity : 1U);
	native->source.source_branch = matrix->subject_kind ==
		ORLIX_TCTI_TARGET_LINUX_PROOF_SOURCE_LEAF && !leaf->condition_length ?
		ORLIX_TCTI_NATIVE_SOURCE_BRANCH_UNCONDITIONAL :
		ORLIX_TCTI_NATIVE_SOURCE_BRANCH_CASE;
	native->source.source_branch_ordinal = matrix->source.concrete_selector;
	if (!native_registry_copy(native->source.leaf_name,
			sizeof(native->source.leaf_name), source->leaf_name) ||
	    !native_registry_copy(native->source.mnemonic,
			sizeof(native->source.mnemonic), source->mnemonic) ||
	    !native_registry_copy(native->source.operation_id,
			sizeof(native->source.operation_id), source->operation_id) ||
	    !native_registry_copy(native->source.condition_tcnd_hex,
			sizeof(native->source.condition_tcnd_hex),
			source->condition_tcnd_hex) ||
	    !native_registry_copy(native->source.artifact_architecture,
			sizeof(native->source.artifact_architecture),
			artifact->architecture) ||
	    !native_registry_copy(native->source.artifact_build,
			sizeof(native->source.artifact_build), artifact->build) ||
	    !native_registry_copy(native->source.artifact_reference,
			sizeof(native->source.artifact_reference), artifact->reference) ||
	    !native_registry_copy(native->source.artifact_schema,
			sizeof(native->source.artifact_schema), artifact->schema) ||
	    !native_registry_copy(native->source.artifact_source_sha256,
			sizeof(native->source.artifact_source_sha256),
			artifact->source_sha256) ||
	    (matrix->source.variant_name &&
	     !native_registry_copy(native->source.semantic_variant,
			sizeof(native->source.semantic_variant),
			matrix->source.variant_name)) ||
	    (matrix->source.kunit_suite &&
	     !native_registry_copy(native->source.accessor_kunit_suite,
			sizeof(native->source.accessor_kunit_suite),
			matrix->source.kunit_suite)) ||
	    (matrix->source.kunit_case &&
	     !native_registry_copy(native->source.accessor_kunit_case,
			sizeof(native->source.accessor_kunit_case),
			matrix->source.kunit_case)) ||
	    !native_registry_copy(native->semantics.ddi0602_locator,
			sizeof(native->semantics.ddi0602_locator), semantic->locator) ||
	    !native_registry_copy(native->semantics.ddi0602_sha256,
			sizeof(native->semantics.ddi0602_sha256), semantic->sha256) ||
	    (capture && !native_registry_copy(native->semantics.implementation_owner,
			sizeof(native->semantics.implementation_owner),
			capture->implementation_owner)) ||
	    (capture && !native_registry_copy(native->semantics.decoder_owner,
			sizeof(native->semantics.decoder_owner),
			capture->decoder_owner)) ||
	    (capture && !native_registry_copy(native->semantics.lowering_owner,
			sizeof(native->semantics.lowering_owner),
			capture->lowering_owner)) ||
	    !native_registry_copy(native->proof_id, sizeof(native->proof_id),
			proof->id) ||
	    !native_registry_copy(native->kunit_source,
			sizeof(native->kunit_source), provenance.source) ||
	    !native_registry_copy(native->kunit_source_sha256,
			sizeof(native->kunit_source_sha256), provenance.source_sha256) ||
	    !native_registry_copy(native->kunit_build_source,
			sizeof(native->kunit_build_source), provenance.build_source) ||
	    !native_registry_copy(native->kunit_build_source_sha256,
			sizeof(native->kunit_build_source_sha256),
			provenance.build_source_sha256) ||
	    !native_registry_copy(native->kunit_suite,
			sizeof(native->kunit_suite), provenance.suite) ||
	    !native_registry_copy(native->kunit_case,
			sizeof(native->kunit_case), provenance.case_name))
		return false;
	native->semantics.classification_mask = proof->classification_mask;
	if (!native_registry_copy(native->kernel_archive_input_sha256,
			sizeof(native->kernel_archive_input_sha256),
			ORLIX_TCTI_KERNEL_ARCHIVE_INPUT_SHA256) ||
	    !native_registry_copy(native->kernel_config_sha256,
			sizeof(native->kernel_config_sha256),
			ORLIX_TCTI_KERNEL_CONFIG_SHA256) ||
	    !native_registry_copy(native->build_profile_sha256,
			sizeof(native->build_profile_sha256),
			ORLIX_TCTI_BUILD_PROFILE_SHA256) ||
	    !native_registry_copy(native->durable_source_revision,
			sizeof(native->durable_source_revision),
			ORLIX_TCTI_DURABLE_SOURCE_REVISION) ||
	    !native_registry_copy(native->instruction_artifact_sha256,
			sizeof(native->instruction_artifact_sha256),
			ORLIX_TCTI_INSTRUCTION_ARTIFACT_SHA256))
		return false;
	native->instruction_artifact_identity =
		native_instruction_artifact_identity(artifact);
	native->runtime_profile = ORLIX_TCTI_NATIVE_RUNTIME_PROFILE;
	native->observation_kind = native_observation_kind(obligation);
	native->obligation = obligation;
	native->production_capture = capture != NULL;
	return true;
}

static bool native_static_matrix_row_build(
	struct orlix_tcti_native_proof_registry_entry *native,
	const struct orlix_tcti_target_linux_proof_disposition_row *matrix,
	const struct orlix_tcti_target_instruction_artifact *artifact)
{
	const struct orlix_tcti_target_instruction_artifact_leaf *leaf;
	const struct native_source_manifest_binding *source;
	const struct native_semantic_provenance_row *semantic;
	orlix_tcti_proof_u32 source_ordinal;

	if (!native || !matrix || !artifact ||
	    (matrix->subject_kind != ORLIX_TCTI_TARGET_LINUX_PROOF_SOURCE_LEAF &&
	     matrix->subject_kind !=
		ORLIX_TCTI_TARGET_LINUX_PROOF_SYSTEM_ACCESSOR_VARIANT))
		return false;
	source_ordinal = matrix->subject_kind ==
		ORLIX_TCTI_TARGET_LINUX_PROOF_SOURCE_LEAF ?
		matrix->source.source_index : matrix->source.tertiary_index;
	if (source_ordinal >= artifact->leaf_count || source_ordinal >=
		    ORLIX_TCTI_ARRAY_COUNT(native_source_manifest_bindings))
		return false;
	leaf = &artifact->leaves[source_ordinal];
	source = &native_source_manifest_bindings[source_ordinal];
	semantic = &native_semantic_provenance[source_ordinal];
	if (source->ordinal != source_ordinal || native_empty(semantic->locator) ||
	    native_empty(semantic->sha256))
		return false;

	memset(native, 0, sizeof(*native));
	native->source.subject_kind = matrix->subject_kind ==
		ORLIX_TCTI_TARGET_LINUX_PROOF_SOURCE_LEAF ?
		ORLIX_TCTI_NATIVE_SUBJECT_SOURCE_LEAF :
		ORLIX_TCTI_NATIVE_SUBJECT_SEMANTIC_VARIANT;
	native->source.classification =
		native_source_classifications[source_ordinal].classification;
	native->source.classification_present = 1U;
	native->source.source_ordinal = source_ordinal;
	native->source.source_index = matrix->source.source_index;
	native->source.secondary_index = matrix->source.secondary_index;
	native->source.tertiary_index = matrix->source.tertiary_index;
	if (matrix->subject_kind ==
	    ORLIX_TCTI_TARGET_LINUX_PROOF_SYSTEM_ACCESSOR_VARIANT) {
		native->source.semantic_variant_ordinal = matrix->source.source_index;
		native->source.variant_direction = matrix->source.encoding_mask;
		native->source.variant_disposition = matrix->source.encoding_pattern;
		native->source.semantic_variant_identity = matrix->source.identity;
	}
	native->source.encoding_mask = source->encoding_mask;
	native->source.encoding_pattern = source->encoding_pattern;
	native->source.access_expression = matrix->source.access_expression;
	native->source.selector_count = matrix->source.selector_count;
	native->source.condition_expression = matrix->source.condition_expression;
	native->source.concrete_selector = matrix->source.concrete_selector;
	native->source.accessor_applicability = matrix->source.applicability;
	native->source.accessor_semantics = matrix->source.semantics;
	native->source.accessor_implementation = matrix->source.implementation;
	native->source.accessor_proof_state = matrix->source.proof_state;
	native->source.source_identity = matrix->source.identity;
	if (!native->source.source_identity) {
		orlix_tcti_proof_u64 identity =
			ORLIX_TCTI_PROOF_U64_C(1469598103934665603);

		identity = native_registry_hash_u32(identity, source->ordinal);
		identity = native_registry_hash_u64(identity, source->source_offset);
		identity = native_registry_hash_u64(identity, source->source_length);
		identity = native_registry_hash_text(identity, source->leaf_name);
		native->source.source_identity = identity ? identity : 1U;
	}
	native->source.source_offset = matrix->source.source_offset;
	native->source.source_length = matrix->source.source_length;
	native->source.secondary_offset = matrix->source.secondary_offset;
	native->source.secondary_length = matrix->source.secondary_length;
	native->source.condition_identity = matrix->source.condition_identity;
	if (!native->source.condition_identity) {
		orlix_tcti_proof_u64 identity = native_registry_hash(
			ORLIX_TCTI_PROOF_U64_C(1469598103934665603),
			artifact->condition_pool + leaf->condition_offset,
			leaf->condition_length);

		native->source.condition_identity = identity ? identity : 1U;
	}
	native->source.condition_offset = matrix->source.condition_offset ?
		matrix->source.condition_offset : leaf->condition_offset;
	native->source.condition_length = matrix->source.condition_length ?
		matrix->source.condition_length : leaf->condition_length;
	native->source.access_identity = matrix->source.access_identity;
	native->source.access_offset = matrix->source.access_offset;
	native->source.access_length = matrix->source.access_length;
	native->source.source_branch = matrix->subject_kind ==
		ORLIX_TCTI_TARGET_LINUX_PROOF_SOURCE_LEAF && !leaf->condition_length ?
		ORLIX_TCTI_NATIVE_SOURCE_BRANCH_UNCONDITIONAL :
		ORLIX_TCTI_NATIVE_SOURCE_BRANCH_CASE;
	native->source.source_branch_ordinal = matrix->source.concrete_selector;
	if (!native_registry_copy(native->source.leaf_name,
			sizeof(native->source.leaf_name), source->leaf_name) ||
	    !native_registry_copy(native->source.mnemonic,
			sizeof(native->source.mnemonic), source->mnemonic) ||
	    !native_registry_copy(native->source.operation_id,
			sizeof(native->source.operation_id), source->operation_id) ||
	    (matrix->source.variant_name &&
	     !native_registry_copy(native->source.semantic_variant,
			sizeof(native->source.semantic_variant),
			matrix->source.variant_name)) ||
	    !native_registry_copy(native->source.condition_tcnd_hex,
			sizeof(native->source.condition_tcnd_hex),
			source->condition_tcnd_hex) ||
	    !native_registry_copy(native->source.artifact_architecture,
			sizeof(native->source.artifact_architecture),
			artifact->architecture) ||
	    !native_registry_copy(native->source.artifact_build,
			sizeof(native->source.artifact_build), artifact->build) ||
	    !native_registry_copy(native->source.artifact_reference,
			sizeof(native->source.artifact_reference),
			artifact->reference) ||
	    !native_registry_copy(native->source.artifact_schema,
			sizeof(native->source.artifact_schema), artifact->schema) ||
	    !native_registry_copy(native->source.artifact_source_sha256,
			sizeof(native->source.artifact_source_sha256),
			artifact->source_sha256) ||
	    (matrix->source.kunit_suite &&
	     !native_registry_copy(native->source.accessor_kunit_suite,
			sizeof(native->source.accessor_kunit_suite),
			matrix->source.kunit_suite)) ||
	    (matrix->source.kunit_case &&
	     !native_registry_copy(native->source.accessor_kunit_case,
			sizeof(native->source.accessor_kunit_case),
			matrix->source.kunit_case)) ||
	    !native_registry_copy(native->semantics.ddi0602_locator,
			sizeof(native->semantics.ddi0602_locator), semantic->locator) ||
	    !native_registry_copy(native->semantics.ddi0602_sha256,
			sizeof(native->semantics.ddi0602_sha256), semantic->sha256) ||
	    (matrix->source.execution_owner &&
	     !native_registry_copy(native->semantics.implementation_owner,
			sizeof(native->semantics.implementation_owner),
			matrix->source.execution_owner)) ||
	    (matrix->source.decoder_owner &&
	     !native_registry_copy(native->semantics.decoder_owner,
			sizeof(native->semantics.decoder_owner),
			matrix->source.decoder_owner)) ||
	    (matrix->source.execution_owner &&
	     !native_registry_copy(native->semantics.lowering_owner,
			sizeof(native->semantics.lowering_owner),
			matrix->source.execution_owner)) ||
	    !native_registry_copy(native->kernel_archive_input_sha256,
			sizeof(native->kernel_archive_input_sha256),
			ORLIX_TCTI_KERNEL_ARCHIVE_INPUT_SHA256) ||
	    !native_registry_copy(native->kernel_config_sha256,
			sizeof(native->kernel_config_sha256),
			ORLIX_TCTI_KERNEL_CONFIG_SHA256) ||
	    !native_registry_copy(native->build_profile_sha256,
			sizeof(native->build_profile_sha256),
			ORLIX_TCTI_BUILD_PROFILE_SHA256) ||
	    !native_registry_copy(native->durable_source_revision,
			sizeof(native->durable_source_revision),
			ORLIX_TCTI_DURABLE_SOURCE_REVISION) ||
	    !native_registry_copy(native->instruction_artifact_sha256,
			sizeof(native->instruction_artifact_sha256),
			ORLIX_TCTI_INSTRUCTION_ARTIFACT_SHA256))
		return false;
	switch (native->source.classification) {
	case ORLIX_TCTI_NATIVE_SOURCE_CLASS_REQUIRED_EL0:
		native->semantics.classification_mask =
			ORLIX_TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0;
		break;
	case ORLIX_TCTI_NATIVE_SOURCE_CLASS_NON_EL0:
		native->semantics.classification_mask =
			ORLIX_TCTI_TARGET_PROOF_CLASS_NON_EL0;
		break;
	case ORLIX_TCTI_NATIVE_SOURCE_CLASS_ARCH_UNDEFINED:
		native->semantics.classification_mask =
			ORLIX_TCTI_TARGET_PROOF_CLASS_ARCH_UNDEFINED_OR_UNALLOCATED;
		break;
	case ORLIX_TCTI_NATIVE_SOURCE_CLASS_UNCLASSIFIED:
	default:
		break;
	}
	native->instruction_artifact_identity =
		native_instruction_artifact_identity(artifact);
	native->runtime_profile = ORLIX_TCTI_NATIVE_RUNTIME_PROFILE;
	native->static_obligation = 1U;
	return true;
}

/* SystemAccessor is a supplemental semantic-variant domain.  The generated
 * reconciliation row, rather than its shared MRS/MSR source leaf, is the
 * capture authority.  Keep the static row as the impossible/unexecuted
 * witness and add a separate, sealed production row only for implemented
 * concrete variants. */
static bool native_system_accessor_capture_row_build(
	struct orlix_tcti_native_proof_registry_entry *native,
	const struct orlix_tcti_target_linux_proof_disposition_row *matrix,
	const struct orlix_tcti_target_instruction_artifact *artifact)
{
	if (!native || !matrix ||
	    matrix->subject_kind !=
		ORLIX_TCTI_TARGET_LINUX_PROOF_SYSTEM_ACCESSOR_VARIANT ||
	    matrix->source.implementation !=
		ORLIX_TCTI_TARGET_SYSTEM_ACCESSOR_IMPLEMENTED ||
	    matrix->source.concrete_selector == UINT_MAX ||
	    !matrix->source.identity ||
	    !native_static_matrix_row_build(native, matrix, artifact))
		return false;
	native->static_obligation = 0U;
	native->production_capture = 1U;
	native->observation_kind = ORLIX_TCTI_NATIVE_OBSERVATION_GPR;
	native->obligation = ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS;
	if (!native_registry_copy(native->proof_id, sizeof(native->proof_id),
			"kunit:system-accessor-production") ||
	    !native_registry_copy(native->kunit_source, sizeof(native->kunit_source),
			"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_source_leaf_classification_test.c") ||
	    !native_registry_copy(native->kunit_source_sha256,
			sizeof(native->kunit_source_sha256),
			"2cfebb266253ef9be5aa73dbd3959be285a754e3ece63812497ecdfc5a90dfea") ||
	    !native_registry_copy(native->kunit_build_source,
			sizeof(native->kunit_build_source),
			"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/Makefile") ||
	    !native_registry_copy(native->kunit_build_source_sha256,
			sizeof(native->kunit_build_source_sha256),
			ORLIX_TCTI_KUNIT_BUILD_SOURCE_SHA256) ||
	    !native_registry_copy(native->kunit_suite, sizeof(native->kunit_suite),
			"orlix-tcti-source-leaf-classification") ||
	    !native_registry_copy(native->kunit_case, sizeof(native->kunit_case),
			"orlix_tcti_system_accessor_partition_implemented_production_observations"))
		return false;
	return true;
}

static bool native_registry_build(void)
{
	const struct orlix_tcti_target_instruction_artifact *artifact;
	const struct orlix_tcti_target_linux_proof_disposition_row *matrix;
	const struct orlix_tcti_target_proof_registry_entry *proofs;
	struct orlix_tcti_native_proof_registry_entry *rows;
	size_t matrix_count;
	size_t proof_count;
	size_t count = 0;
	size_t proof_index;
	size_t row_index = 0;

	proofs = orlix_tcti_target_proof_registry_entries(&proof_count);
	matrix = orlix_tcti_target_linux_proof_dispositions(&matrix_count);
	artifact = orlix_tcti_target_instruction_artifact_canonical();
	if (!proofs || !proof_count || !matrix ||
	    matrix_count != ORLIX_TCTI_TARGET_LINUX_PROOF_TOTAL_ROWS || !artifact)
		return false;
	if (!native_condition_capacity_is_source_bound())
		return false;
	for (proof_index = 0; proof_index < proof_count; proof_index++) {
		const struct orlix_tcti_target_proof_registry_entry *proof =
			&proofs[proof_index];
		size_t binding_index;

		for (binding_index = 0; binding_index < proof->binding_count;
		     binding_index++) {
			size_t matrix_index;

			for (matrix_index = 0; matrix_index < matrix_count;
			     matrix_index++) {
				size_t case_index;

				if (!native_matrix_matches_binding(&matrix[matrix_index],
						&proof->bindings[binding_index]))
					continue;
				for (case_index = 0;
				     case_index < proof->kunit_case_count;
				     case_index++) {
					orlix_tcti_proof_u32 obligation;

					if (case_index >= sizeof(orlix_tcti_proof_u64) * 8U ||
					    !(proof->bindings[binding_index].kunit_case_mask &
					      (ORLIX_TCTI_PROOF_U64_C(1) << case_index)))
						continue;
					for (obligation = 1U;
					     obligation <= ORLIX_TCTI_TARGET_PROOF_OBLIGATION_SYSTEM_PT_CONTROL;
					     obligation <<= 1U)
						if ((proof->kunit_cases[case_index].obligations & obligation) &&
						    native_production_capture_binding(proof, case_index,
							proof->bindings[binding_index].source_ordinal,
							obligation) &&
						    !native_size_add(&count, 1U))
							return false;
				}
			}
		}
	}
	for (proof_index = 0; proof_index < matrix_count; proof_index++)
		if (matrix[proof_index].subject_kind ==
			    ORLIX_TCTI_TARGET_LINUX_PROOF_SYSTEM_ACCESSOR_VARIANT &&
		    matrix[proof_index].source.implementation ==
			    ORLIX_TCTI_TARGET_SYSTEM_ACCESSOR_IMPLEMENTED &&
		    matrix[proof_index].source.concrete_selector != UINT_MAX &&
		    !native_size_add(&count, 1U))
			return false;
	if (!native_size_add(&count, matrix_count))
		return false;
	if (!count || count > SIZE_MAX / sizeof(*rows))
		return false;
#ifdef __KERNEL__
	rows = kvmalloc_array(count, sizeof(*rows), GFP_KERNEL | __GFP_ZERO);
#else
	rows = calloc(count, sizeof(*rows));
#endif
	if (!rows)
		return false;
#ifdef __KERNEL__
	native_capture_declarations = kvmalloc_array(count,
		sizeof(*native_capture_declarations), GFP_KERNEL | __GFP_ZERO);
#else
	native_capture_declarations = calloc(count,
		sizeof(*native_capture_declarations));
#endif
	if (!native_capture_declarations)
		goto fail;
	for (proof_index = 0; proof_index < proof_count; proof_index++) {
		const struct orlix_tcti_target_proof_registry_entry *proof =
			&proofs[proof_index];
		size_t binding_index;

		for (binding_index = 0; binding_index < proof->binding_count;
		     binding_index++) {
			const struct orlix_tcti_target_proof_binding *binding =
				&proof->bindings[binding_index];
			size_t matrix_index;

			for (matrix_index = 0; matrix_index < matrix_count;
			     matrix_index++) {
				size_t case_index;

				if (!native_matrix_matches_binding(&matrix[matrix_index], binding))
					continue;
				for (case_index = 0;
				     case_index < proof->kunit_case_count;
				     case_index++) {
					const struct orlix_tcti_target_proof_case *proof_case =
						&proof->kunit_cases[case_index];
					orlix_tcti_proof_u32 obligation;

					if (case_index >= sizeof(orlix_tcti_proof_u64) * 8U ||
					    !(binding->kunit_case_mask &
					      (ORLIX_TCTI_PROOF_U64_C(1) << case_index)))
						continue;
					for (obligation = 1U;
					     obligation <= ORLIX_TCTI_TARGET_PROOF_OBLIGATION_SYSTEM_PT_CONTROL;
					     obligation <<= 1U) {
						if (!(proof_case->obligations & obligation))
							continue;
						if (!native_production_capture_binding(proof,
								case_index, binding->source_ordinal,
								obligation))
							continue;
						if (row_index >= count ||
						    !native_registry_row_build(&rows[row_index], proof,
							binding, proof_case, case_index,
							&matrix[matrix_index],
							obligation, artifact))
							goto fail;
						row_index++;
					}
				}
			}
		}
	}
	for (proof_index = 0; proof_index < matrix_count; proof_index++) {
		if (matrix[proof_index].subject_kind ==
			    ORLIX_TCTI_TARGET_LINUX_PROOF_SYSTEM_ACCESSOR_VARIANT &&
		    matrix[proof_index].source.implementation ==
			    ORLIX_TCTI_TARGET_SYSTEM_ACCESSOR_IMPLEMENTED &&
		    matrix[proof_index].source.concrete_selector != UINT_MAX) {
			if (row_index >= count ||
			    !native_system_accessor_capture_row_build(&rows[row_index],
				&matrix[proof_index], artifact))
				goto fail;
			row_index++;
		}
	}
	for (proof_index = 0; proof_index < matrix_count; proof_index++) {
		if (row_index >= count ||
		    !native_static_matrix_row_build(&rows[row_index],
			    &matrix[proof_index], artifact))
			goto fail;
		row_index++;
	}
	for (proof_index = 0; proof_index < row_index; proof_index++)
		if (rows[proof_index].production_capture &&
		    !native_capture_declaration_build(
			&native_capture_declarations[proof_index], &rows[proof_index]))
			goto fail;
	if (row_index != count || orlix_tcti_native_proof_registry_validate(
			rows, count, NULL))
		goto fail;
	native_registry_entries = rows;
	native_registry_entry_count = count;
	native_registry_initialized = true;
	return true;

fail:
	#ifdef __KERNEL__
	kvfree(native_capture_declarations);
	#else
	free(native_capture_declarations);
	#endif
	native_capture_declarations = NULL;
#ifdef __KERNEL__
	kvfree(rows);
#else
	free(rows);
#endif
	return false;
}

#ifndef __KERNEL__
static void native_registry_build_once(void)
{
	native_registry_initialized = native_registry_build();
}
#endif

const struct orlix_tcti_native_proof_registry_entry *
orlix_tcti_native_proof_registry_entries(size_t *count)
{
#ifdef __KERNEL__
	mutex_lock(&native_registry_initialization_lock);
	if (!native_registry_initialized)
		native_registry_build();
	mutex_unlock(&native_registry_initialization_lock);
#else
	if (pthread_once(&native_registry_once, native_registry_build_once)) {
		if (count)
			*count = 0;
		return NULL;
	}
#endif
	if (!native_registry_initialized || !native_registry_entries) {
		if (count)
			*count = 0;
		return NULL;
	}
	if (count)
		*count = native_registry_entry_count;
	return native_registry_entries;
}

orlix_tcti_proof_u64 orlix_tcti_native_proof_registry_entry_identity(
	const struct orlix_tcti_native_proof_registry_entry *entry)
{
	orlix_tcti_proof_u64 identity =
		ORLIX_TCTI_PROOF_U64_C(1469598103934665603);
	const struct orlix_tcti_native_source_identity *source;

#define HASH_ENTRY_U32(field) identity = native_registry_hash_u32(identity, field)
#define HASH_ENTRY_U64(field) identity = native_registry_hash_u64(identity, field)
#define HASH_ENTRY_TEXT(field) identity = native_registry_hash_text(identity, field)

	if (!entry)
		return 0;
	source = &entry->source;
	HASH_ENTRY_U32(source->subject_kind);
	HASH_ENTRY_U32(source->classification);
	HASH_ENTRY_U32(source->classification_present);
	HASH_ENTRY_U32(source->source_ordinal);
	HASH_ENTRY_U32(source->source_index);
	HASH_ENTRY_U32(source->secondary_index);
	HASH_ENTRY_U32(source->tertiary_index);
	HASH_ENTRY_U32(source->semantic_variant_ordinal);
	HASH_ENTRY_U32(source->variant_direction);
	HASH_ENTRY_U32(source->variant_disposition);
	HASH_ENTRY_U64(source->semantic_variant_identity);
	HASH_ENTRY_U32(source->encoding_mask);
	HASH_ENTRY_U32(source->encoding_pattern);
	HASH_ENTRY_U32(source->access_expression);
	HASH_ENTRY_U32(source->selector_count);
	HASH_ENTRY_U32(source->condition_expression);
	HASH_ENTRY_U32(source->concrete_selector);
	HASH_ENTRY_U32(source->accessor_applicability);
	HASH_ENTRY_U32(source->accessor_semantics);
	HASH_ENTRY_U32(source->accessor_implementation);
	HASH_ENTRY_U32(source->accessor_proof_state);
	HASH_ENTRY_U64(source->source_identity);
	HASH_ENTRY_U64(source->source_offset);
	HASH_ENTRY_U64(source->source_length);
	HASH_ENTRY_U64(source->secondary_offset);
	HASH_ENTRY_U64(source->secondary_length);
	HASH_ENTRY_U64(source->condition_identity);
	HASH_ENTRY_U64(source->condition_offset);
	HASH_ENTRY_U64(source->condition_length);
	HASH_ENTRY_U64(source->access_identity);
	HASH_ENTRY_U64(source->access_offset);
	HASH_ENTRY_U64(source->access_length);
	HASH_ENTRY_U32(source->source_branch);
	HASH_ENTRY_U32(source->source_branch_ordinal);
	HASH_ENTRY_TEXT(source->leaf_name);
	HASH_ENTRY_TEXT(source->mnemonic);
	HASH_ENTRY_TEXT(source->operation_id);
	HASH_ENTRY_TEXT(source->semantic_variant);
	HASH_ENTRY_TEXT(source->condition_tcnd_hex);
	HASH_ENTRY_TEXT(source->artifact_architecture);
	HASH_ENTRY_TEXT(source->artifact_build);
	HASH_ENTRY_TEXT(source->artifact_reference);
	HASH_ENTRY_TEXT(source->artifact_schema);
	HASH_ENTRY_TEXT(source->artifact_source_sha256);
	HASH_ENTRY_TEXT(source->accessor_kunit_suite);
	HASH_ENTRY_TEXT(source->accessor_kunit_case);
	HASH_ENTRY_TEXT(entry->semantics.ddi0602_locator);
	HASH_ENTRY_TEXT(entry->semantics.ddi0602_sha256);
	HASH_ENTRY_U32(entry->semantics.classification_mask);
	HASH_ENTRY_TEXT(entry->semantics.implementation_owner);
	HASH_ENTRY_TEXT(entry->semantics.decoder_owner);
	HASH_ENTRY_TEXT(entry->semantics.lowering_owner);
	HASH_ENTRY_TEXT(entry->proof_id);
	HASH_ENTRY_TEXT(entry->kunit_source);
	HASH_ENTRY_TEXT(entry->kunit_source_sha256);
	HASH_ENTRY_TEXT(entry->kunit_build_source);
	HASH_ENTRY_TEXT(entry->kunit_build_source_sha256);
	HASH_ENTRY_TEXT(entry->kunit_suite);
	HASH_ENTRY_TEXT(entry->kunit_case);
	HASH_ENTRY_TEXT(entry->kernel_archive_input_sha256);
	HASH_ENTRY_TEXT(entry->kernel_config_sha256);
	HASH_ENTRY_TEXT(entry->build_profile_sha256);
	HASH_ENTRY_TEXT(entry->durable_source_revision);
	HASH_ENTRY_TEXT(entry->instruction_artifact_sha256);
	HASH_ENTRY_U64(entry->instruction_artifact_identity);
	HASH_ENTRY_U32(entry->runtime_profile);
	HASH_ENTRY_U32(entry->observation_kind);
	HASH_ENTRY_U32(entry->obligation);
	HASH_ENTRY_U64(entry->capture_declaration ?
		entry->capture_declaration->declaration_identity : 0U);
	HASH_ENTRY_U32(entry->production_capture);
	HASH_ENTRY_U32(entry->static_obligation);
#undef HASH_ENTRY_TEXT
#undef HASH_ENTRY_U64
#undef HASH_ENTRY_U32
	return identity ? identity : 1U;
}

const void *orlix_tcti_native_proof_registry_capture_token_internal(size_t index)
{
	const struct orlix_tcti_target_production_capture_binding *bindings;
	const struct orlix_tcti_native_proof_registry_entry *entries;
	const struct orlix_tcti_native_proof_registry_entry *match = NULL;
	size_t count;
	size_t binding_count;
	size_t entry_index;

	bindings = orlix_tcti_target_production_capture_bindings(&binding_count);
	entries = orlix_tcti_native_proof_registry_entries(&count);
	if (!bindings || index >= binding_count || !entries)
		return NULL;
	for (entry_index = 0; entry_index < count; entry_index++) {
		const struct orlix_tcti_native_proof_registry_entry *candidate =
			&entries[entry_index];

		if (!candidate->production_capture ||
		    candidate->source.source_ordinal != bindings[index].source_ordinal ||
		    candidate->obligation != bindings[index].obligation ||
		    strcmp(candidate->kunit_source, bindings[index].kunit_source) ||
		    strcmp(candidate->kunit_suite, bindings[index].kunit_suite) ||
		    strcmp(candidate->kunit_case, bindings[index].kunit_case))
			continue;
		if (match)
			return NULL;
		match = candidate;
	}
	return match;
}

const void *orlix_tcti_native_proof_registry_capture_token_for_entry(
	const struct orlix_tcti_native_proof_registry_entry *entry)
{
	const struct orlix_tcti_native_proof_registry_entry *entries;
	size_t count;
	size_t index;

	entries = orlix_tcti_native_proof_registry_entries(&count);
	for (index = 0; entries && index < count; index++)
		if (&entries[index] == entry)
			return entry;
	return NULL;
}

int orlix_tcti_native_proof_registry_capture_token_semantic_variant_identity(
	const void *capture_token, orlix_tcti_proof_u64 *semantic_variant_identity)
{
	const struct orlix_tcti_native_proof_registry_entry *entries;
	size_t count;
	size_t index;

	if (!capture_token || !semantic_variant_identity)
		return -1;
	entries = orlix_tcti_native_proof_registry_entries(&count);
	for (index = 0; entries && index < count; index++)
		if (&entries[index] == capture_token) {
			*semantic_variant_identity =
				entries[index].source.semantic_variant_identity;
			return 0;
		}
	return -1;
}

int orlix_tcti_native_proof_registry_resolve_production_entries(
	const struct orlix_tcti_native_proof_registry_entry *entries, size_t count,
	const void *capture_token,
	orlix_tcti_proof_u32 source_ordinal,
	orlix_tcti_proof_u64 semantic_variant_identity,
	orlix_tcti_proof_u32 obligation,
	const struct orlix_tcti_native_proof_registry_entry **entry,
	enum orlix_tcti_native_contract_error *error)
{
	const struct orlix_tcti_native_proof_registry_entry *match = NULL;
	size_t index;

	if (entry)
		*entry = NULL;
	if (!entries || !count || !capture_token || !entry) {
		if (error)
			*error = ORLIX_TCTI_NATIVE_CONTRACT_INVALID;
		return -1;
	}
	for (index = 0; index < count; index++) {
		const struct orlix_tcti_native_proof_registry_entry *candidate =
			&entries[index];

		if ((const void *)candidate != capture_token ||
		    !candidate->production_capture ||
		    candidate->source.source_ordinal != source_ordinal ||
		    candidate->source.semantic_variant_identity !=
			semantic_variant_identity ||
		    candidate->obligation != obligation)
			continue;
		if (match) {
			if (error)
				*error = ORLIX_TCTI_NATIVE_CONTRACT_INVALID;
			return -1;
		}
		match = candidate;
	}
	if (!match) {
		if (error)
			*error = ORLIX_TCTI_NATIVE_CONTRACT_UNKNOWN;
		return -1;
	}
	*entry = match;
	if (error)
		*error = ORLIX_TCTI_NATIVE_CONTRACT_OK;
	return 0;
}

int orlix_tcti_native_proof_registry_resolve_production(
	const void *capture_token,
	orlix_tcti_proof_u32 source_ordinal,
	orlix_tcti_proof_u64 semantic_variant_identity,
	orlix_tcti_proof_u32 obligation,
	const struct orlix_tcti_native_proof_registry_entry **entry,
	enum orlix_tcti_native_contract_error *error)
{
	const struct orlix_tcti_native_proof_registry_entry *entries;
	size_t count;

	entries = orlix_tcti_native_proof_registry_entries(&count);
	return orlix_tcti_native_proof_registry_resolve_production_entries(entries,
		count, capture_token, source_ordinal, semantic_variant_identity,
		obligation, entry, error);
}
