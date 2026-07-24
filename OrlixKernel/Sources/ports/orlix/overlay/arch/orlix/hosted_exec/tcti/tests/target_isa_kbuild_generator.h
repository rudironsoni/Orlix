/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_ISA_KBUILD_GENERATOR_H
#define ORLIX_TCTI_TARGET_ISA_KBUILD_GENERATOR_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define TCTI_A64_KBUILD_SOURCE_COUNT 4350U
#define TCTI_A64_KBUILD_ARCHITECTURE "vFATAp1-A"
#define TCTI_A64_KBUILD_BUILD "818"
#define TCTI_A64_KBUILD_REFERENCE "2026-06_rel"
#define TCTI_A64_KBUILD_SCHEMA "2.9.5"
#define TCTI_A64_KBUILD_INSTRUCTIONS_SHA256 \
	"a1ad2c6538a47cd97d8762791ac5af88bce1d5f6aff096c9b77aef853e76acfe"

enum tcti_a64_kbuild_classification {
	TCTI_A64_KBUILD_UNCLASSIFIED,
	TCTI_A64_KBUILD_REQUIRED,
	TCTI_A64_KBUILD_NON_EL0,
	TCTI_A64_KBUILD_ARCHITECTURALLY_UNDEFINED,
	TCTI_A64_KBUILD_ALIAS_OR_DUPLICATE,
};

enum tcti_a64_kbuild_relation {
	TCTI_A64_KBUILD_RELATION_NONE,
	TCTI_A64_KBUILD_RELATION_ALIAS,
	TCTI_A64_KBUILD_RELATION_DUPLICATE,
};

struct tcti_a64_kbuild_metadata {
	const char *architecture;
	const char *build;
	const char *reference;
	const char *schema;
	const char *instructions_sha256;
	uint32_t source_count;
};

struct tcti_a64_kbuild_source_row {
	uint32_t ordinal;
	const char *name;
	const char *mnemonic;
	const char *operation;
	uint32_t mask;
	uint32_t pattern;
	const char *condition_tcnd_hex;
};

struct tcti_a64_kbuild_classification_row {
	const char *name;
	enum tcti_a64_kbuild_classification classification;
	enum tcti_a64_kbuild_relation relation;
	const char *canonical;
	const char *evidence;
	const char *proof;
};

enum tcti_a64_kbuild_generator_error {
	TCTI_A64_KBUILD_GENERATOR_OK,
	TCTI_A64_KBUILD_GENERATOR_INVALID_ARGUMENT,
	TCTI_A64_KBUILD_GENERATOR_METADATA_MISMATCH,
	TCTI_A64_KBUILD_GENERATOR_COUNT_MISMATCH,
	TCTI_A64_KBUILD_GENERATOR_BAD_ORDINAL,
	TCTI_A64_KBUILD_GENERATOR_BAD_SOURCE_ROW,
	TCTI_A64_KBUILD_GENERATOR_DUPLICATE_SOURCE,
	TCTI_A64_KBUILD_GENERATOR_CLASSIFICATION_MISMATCH,
	TCTI_A64_KBUILD_GENERATOR_BAD_CLASSIFICATION,
	TCTI_A64_KBUILD_GENERATOR_NO_MEMORY,
	TCTI_A64_KBUILD_GENERATOR_IO,
};

/*
 * Validation completes before the first output byte is written. UNCLASSIFIED
 * rows are valid inputs and are emitted as explicit completion blockers.
 */
enum tcti_a64_kbuild_generator_error tcti_a64_kbuild_generate_header(
	const struct tcti_a64_kbuild_metadata *metadata,
	const struct tcti_a64_kbuild_source_row *source,
	size_t source_count,
	const struct tcti_a64_kbuild_classification_row *classification,
	size_t classification_count,
	FILE *output);

const char *tcti_a64_kbuild_generator_error_name(
	enum tcti_a64_kbuild_generator_error error);

#endif
