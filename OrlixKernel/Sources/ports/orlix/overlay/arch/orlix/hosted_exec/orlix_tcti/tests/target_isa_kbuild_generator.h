/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_ISA_KBUILD_GENERATOR_H
#define ORLIX_TCTI_TARGET_ISA_KBUILD_GENERATOR_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define ORLIX_TCTI_A64_KBUILD_SOURCE_COUNT 4350U
#define ORLIX_TCTI_A64_KBUILD_ARCHITECTURE "vFATAp1-A"
#define ORLIX_TCTI_A64_KBUILD_BUILD "818"
#define ORLIX_TCTI_A64_KBUILD_REFERENCE "2026-06_rel"
#define ORLIX_TCTI_A64_KBUILD_SCHEMA "2.9.5"
#define ORLIX_TCTI_A64_KBUILD_INSTRUCTIONS_SHA256 \
	"a1ad2c6538a47cd97d8762791ac5af88bce1d5f6aff096c9b77aef853e76acfe"
#define ORLIX_TCTI_A64_KBUILD_INSTRUCTIONS_TIMESTAMP "2026-06-24 17:12:14"
#define ORLIX_TCTI_A64_KBUILD_INSTRUCTIONS_SOURCE_BYTES 115441429U
#define ORLIX_TCTI_A64_KBUILD_SYSTEM_ACCESSOR_COUNT 2014U
#define ORLIX_TCTI_A64_KBUILD_REGISTERS_TIMESTAMP "2026-06-24 17:12:14"
#define ORLIX_TCTI_A64_KBUILD_REGISTERS_SHA256 \
	"5bd76c3c3ce90322eb4fd179675dafe82df2fd1cb789beee516e5b29c471b874"

enum orlix_tcti_a64_kbuild_classification {
	ORLIX_TCTI_A64_KBUILD_UNCLASSIFIED,
	ORLIX_TCTI_A64_KBUILD_REQUIRED,
	ORLIX_TCTI_A64_KBUILD_NON_EL0,
	ORLIX_TCTI_A64_KBUILD_ARCHITECTURALLY_UNDEFINED,
	ORLIX_TCTI_A64_KBUILD_ALIAS_OR_DUPLICATE,
};

enum orlix_tcti_a64_kbuild_relation {
	ORLIX_TCTI_A64_KBUILD_RELATION_NONE,
	ORLIX_TCTI_A64_KBUILD_RELATION_ALIAS,
	ORLIX_TCTI_A64_KBUILD_RELATION_DUPLICATE,
};

struct orlix_tcti_a64_kbuild_metadata {
	const char *architecture;
	const char *build;
	const char *reference;
	const char *schema;
	const char *instructions_sha256;
	const char *timestamp;
	uint32_t source_byte_length;
	uint32_t source_count;
};

struct orlix_tcti_a64_kbuild_source_row {
	uint32_t ordinal;
	const char *name;
	const char *mnemonic;
	const char *operation;
	uint32_t mask;
	uint32_t pattern;
	const char *condition_tcnd_hex;
	uint32_t source_offset;
	uint32_t source_length;
};

struct orlix_tcti_a64_kbuild_classification_row {
	const char *name;
	enum orlix_tcti_a64_kbuild_classification classification;
	enum orlix_tcti_a64_kbuild_relation relation;
	const char *canonical;
	const char *evidence;
	const char *proof;
};

enum orlix_tcti_a64_kbuild_system_accessor_direction {
	ORLIX_TCTI_A64_KBUILD_SYSTEM_ACCESSOR_DIRECTION_NONE,
	ORLIX_TCTI_A64_KBUILD_SYSTEM_ACCESSOR_DIRECTION_READ,
	ORLIX_TCTI_A64_KBUILD_SYSTEM_ACCESSOR_DIRECTION_WRITE,
	ORLIX_TCTI_A64_KBUILD_SYSTEM_ACCESSOR_DIRECTION_EXECUTE,
};

enum orlix_tcti_a64_kbuild_system_accessor_disposition {
	ORLIX_TCTI_A64_KBUILD_SYSTEM_ACCESSOR_MAPPED,
	ORLIX_TCTI_A64_KBUILD_SYSTEM_ACCESSOR_RESERVED,
	ORLIX_TCTI_A64_KBUILD_SYSTEM_ACCESSOR_PRIVILEGED,
	ORLIX_TCTI_A64_KBUILD_SYSTEM_ACCESSOR_UNSUPPORTED,
	ORLIX_TCTI_A64_KBUILD_SYSTEM_ACCESSOR_AMBIGUOUS,
	ORLIX_TCTI_A64_KBUILD_SYSTEM_ACCESSOR_CONTRADICTORY,
	ORLIX_TCTI_A64_KBUILD_SYSTEM_ACCESSOR_INVALID,
};

struct orlix_tcti_a64_kbuild_system_accessor_metadata {
	const char *architecture;
	const char *build;
	const char *reference;
	const char *schema;
	const char *timestamp;
	const char *registers_sha256;
	uint32_t accessor_count;
	uint32_t mapped_count;
	uint32_t reserved_count;
	uint32_t privileged_count;
	uint32_t unsupported_count;
	uint32_t ambiguous_count;
	uint32_t contradictory_count;
	uint32_t invalid_count;
	uint32_t semantic_count;
	uint32_t source_access_semantics_count;
	uint32_t generic_leaf_semantics_count;
	uint32_t implemented_count;
	uint32_t architectural_rejection_count;
	uint32_t unimplemented_rejection_count;
	uint32_t concrete_selector_count;
	uint32_t symbolic_selector_count;
	uint32_t proof_not_observed_count;
	uint64_t reconciliation_identity;
};

struct orlix_tcti_a64_kbuild_system_accessor_row {
	uint32_t accessor_index;
	uint32_t encoding_index;
	const char *name;
	const char *variant_name;
	const char *generic_leaf;
	enum orlix_tcti_a64_kbuild_system_accessor_direction direction;
	enum orlix_tcti_a64_kbuild_system_accessor_disposition disposition;
	uint32_t selector_count;
	uint32_t condition_expression;
	uint32_t access_expression;
	uint32_t concrete_selector;
	uint32_t applicability;
	uint32_t semantics;
	uint32_t implementation;
	uint32_t proof_state;
	uint64_t selector_identity;
	uint64_t condition_identity;
	uint64_t access_identity;
	const char *decoder_owner;
	const char *execution_owner;
	const char *kunit_suite;
	const char *kunit_case;
	uint32_t accessor_source_offset;
	uint32_t accessor_source_length;
	uint32_t encoding_source_offset;
	uint32_t encoding_source_length;
	uint32_t condition_source_offset;
	uint32_t condition_source_length;
	uint32_t access_source_offset;
	uint32_t access_source_length;
};

enum orlix_tcti_a64_kbuild_generator_error {
	ORLIX_TCTI_A64_KBUILD_GENERATOR_OK,
	ORLIX_TCTI_A64_KBUILD_GENERATOR_INVALID_ARGUMENT,
	ORLIX_TCTI_A64_KBUILD_GENERATOR_METADATA_MISMATCH,
	ORLIX_TCTI_A64_KBUILD_GENERATOR_COUNT_MISMATCH,
	ORLIX_TCTI_A64_KBUILD_GENERATOR_BAD_ORDINAL,
	ORLIX_TCTI_A64_KBUILD_GENERATOR_BAD_SOURCE_ROW,
	ORLIX_TCTI_A64_KBUILD_GENERATOR_DUPLICATE_SOURCE,
	ORLIX_TCTI_A64_KBUILD_GENERATOR_CLASSIFICATION_MISMATCH,
	ORLIX_TCTI_A64_KBUILD_GENERATOR_BAD_CLASSIFICATION,
	ORLIX_TCTI_A64_KBUILD_GENERATOR_ACCESSOR_METADATA_MISMATCH,
	ORLIX_TCTI_A64_KBUILD_GENERATOR_ACCESSOR_COUNT_MISMATCH,
	ORLIX_TCTI_A64_KBUILD_GENERATOR_BAD_ACCESSOR_ROW,
	ORLIX_TCTI_A64_KBUILD_GENERATOR_DUPLICATE_ACCESSOR,
	ORLIX_TCTI_A64_KBUILD_GENERATOR_ACCESSOR_BLOCKER,
	ORLIX_TCTI_A64_KBUILD_GENERATOR_NO_MEMORY,
	ORLIX_TCTI_A64_KBUILD_GENERATOR_IO,
};

/*
 * Validation completes before the first output byte is written. UNCLASSIFIED
 * rows are valid inputs and are emitted as explicit completion blockers.
 */
enum orlix_tcti_a64_kbuild_generator_error orlix_tcti_a64_kbuild_generate_header(
	const struct orlix_tcti_a64_kbuild_metadata *metadata,
	const struct orlix_tcti_a64_kbuild_source_row *source,
	size_t source_count,
	const struct orlix_tcti_a64_kbuild_classification_row *classification,
	size_t classification_count,
	const struct orlix_tcti_a64_kbuild_system_accessor_metadata *accessor_metadata,
	const struct orlix_tcti_a64_kbuild_system_accessor_row *accessors,
	size_t accessor_count,
	FILE *output);

uint64_t orlix_tcti_a64_kbuild_system_accessor_identity(
	const struct orlix_tcti_a64_kbuild_system_accessor_row *accessors,
	size_t accessor_count);

const char *orlix_tcti_a64_kbuild_generator_error_name(
	enum orlix_tcti_a64_kbuild_generator_error error);

#endif
