/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_EXECUTION_SLICE_MAP_H
#define ORLIX_TCTI_TARGET_EXECUTION_SLICE_MAP_H

#ifdef __KERNEL__
#include <linux/stddef.h>
#include <linux/types.h>
#else
#include <stddef.h>
#include <stdint.h>
typedef uint32_t u32;
#endif

#define ORLIX_TCTI_EXECUTION_SLICE_MAP_VERSION 1U
#define ORLIX_TCTI_EXECUTION_SLICE_MAP_LEAF_COUNT 4350U
#define ORLIX_TCTI_EXECUTION_SLICE_MAP_MAX_FAMILY_COUNT 64U

struct orlix_tcti_execution_slice_map_source {
	u32 version;
	const char *architecture;
	const char *build;
	const char *reference;
	const char *schema;
	const char *sha256;
	size_t length;
};

struct orlix_tcti_execution_slice_map_counts {
	size_t leaf_count;
	size_t family_count;
};

struct orlix_tcti_execution_slice_family {
	const char *stable_id;
	u32 issue_id;
	u32 declared_member_count;
};

struct orlix_tcti_execution_slice_member {
	u32 ordinal;
	const char *source_name;
	const char *condition_tcnd_hex;
	u32 family_index;
};

/*
 * This artifact assigns issue ownership only.  It deliberately contains no
 * architectural classification, proof, coverage, or runtime capability state.
 */
struct orlix_tcti_execution_slice_map {
	struct orlix_tcti_execution_slice_map_source source;
	struct orlix_tcti_execution_slice_map_counts counts;
	const struct orlix_tcti_execution_slice_family *families;
	const struct orlix_tcti_execution_slice_member *members;
};

enum orlix_tcti_execution_slice_map_error {
	ORLIX_TCTI_EXECUTION_SLICE_MAP_VALID = 0,
	ORLIX_TCTI_EXECUTION_SLICE_MAP_INVALID_ARGUMENT,
	ORLIX_TCTI_EXECUTION_SLICE_MAP_PROVENANCE_MISMATCH,
	ORLIX_TCTI_EXECUTION_SLICE_MAP_COUNT_MISMATCH,
	ORLIX_TCTI_EXECUTION_SLICE_MAP_FAMILY_INVALID,
	ORLIX_TCTI_EXECUTION_SLICE_MAP_FAMILY_DUPLICATE,
	ORLIX_TCTI_EXECUTION_SLICE_MAP_ORDINAL_MISMATCH,
	ORLIX_TCTI_EXECUTION_SLICE_MAP_SOURCE_NAME_MISMATCH,
	ORLIX_TCTI_EXECUTION_SLICE_MAP_CONDITION_MISMATCH,
	ORLIX_TCTI_EXECUTION_SLICE_MAP_UNKNOWN_FAMILY,
	ORLIX_TCTI_EXECUTION_SLICE_MAP_MEMBERSHIP_MISMATCH,
};

struct orlix_tcti_execution_slice_map_validation_result {
	enum orlix_tcti_execution_slice_map_error error;
	u32 member_index;
	u32 family_index;
};

int orlix_tcti_execution_slice_map_validate(
	const struct orlix_tcti_execution_slice_map *map,
	struct orlix_tcti_execution_slice_map_validation_result *result);

const struct orlix_tcti_execution_slice_map *
orlix_tcti_execution_slice_map_canonical(void);

const char *orlix_tcti_execution_slice_map_error_name(
	enum orlix_tcti_execution_slice_map_error error);

#endif
