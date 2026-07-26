/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_execution_slice_map.h"

#include "target_instruction_artifact.h"

#ifdef __KERNEL__
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/string.h>
#else
#include <errno.h>
#include <string.h>
#define ARRAY_SIZE(values) (sizeof(values) / sizeof((values)[0]))
#endif

#ifndef ORLIX_TCTI_EXECUTION_SLICE_MAP_DEF
#define ORLIX_TCTI_EXECUTION_SLICE_MAP_DEF \
	"../isa/target_execution_slice_map.def"
#endif

#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_SOURCE(...)
#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_COUNTS(...)
#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_FAMILY(symbol, ...) \
	ORLIX_TCTI_EXECUTION_SLICE_FAMILY_##symbol,
#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_MEMBER(...)
enum orlix_tcti_execution_slice_family_index {
#include ORLIX_TCTI_EXECUTION_SLICE_MAP_DEF
	ORLIX_TCTI_EXECUTION_SLICE_FAMILY_COUNT
};
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_SOURCE
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_COUNTS
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_FAMILY
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_MEMBER

#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_SOURCE(...)
#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_COUNTS(...)
#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_FAMILY(symbol, id, issue, count) \
	[ORLIX_TCTI_EXECUTION_SLICE_FAMILY_##symbol] = { id, issue, count },
#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_MEMBER(...)
static const struct orlix_tcti_execution_slice_family canonical_families[] = {
#include ORLIX_TCTI_EXECUTION_SLICE_MAP_DEF
};
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_SOURCE
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_COUNTS
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_FAMILY
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_MEMBER

#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_SOURCE(...)
#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_COUNTS(...)
#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_FAMILY(...)
#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_MEMBER(i, name, condition, family) \
	[i] = { i, name, condition, \
		 ORLIX_TCTI_EXECUTION_SLICE_FAMILY_##family },
static const struct orlix_tcti_execution_slice_member canonical_members[] = {
#include ORLIX_TCTI_EXECUTION_SLICE_MAP_DEF
};
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_SOURCE
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_COUNTS
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_FAMILY
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_MEMBER

static const struct orlix_tcti_execution_slice_map canonical_map = {
#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_SOURCE(v, a, b, r, s, h, l) \
	.source = { v, a, b, r, s, h, l },
#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_COUNTS(leaves, families) \
	.counts = { leaves, families },
#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_FAMILY(...)
#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_MEMBER(...)
#include ORLIX_TCTI_EXECUTION_SLICE_MAP_DEF
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_SOURCE
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_COUNTS
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_FAMILY
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_MEMBER
	.families = canonical_families,
	.members = canonical_members,
};

struct orlix_tcti_execution_slice_source_row {
	const char *name;
	const char *condition_tcnd_hex;
};

#define ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(i, name, mnemonic, operation, mask, \
	pattern, condition, offset, length) \
	[i] = { name, condition },
static const struct orlix_tcti_execution_slice_source_row source_rows[] = {
#include "../isa/source_manifest.def"
};
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW

static const struct orlix_tcti_execution_slice_map_source source_manifest = {
#define ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(a, b, r, s, h, rows, date, l) \
	ORLIX_TCTI_EXECUTION_SLICE_MAP_VERSION, a, b, r, s, h, l
#define ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(...)
#include "../isa/source_manifest.def"
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW
};

static int fail(
	struct orlix_tcti_execution_slice_map_validation_result *result,
	enum orlix_tcti_execution_slice_map_error error, u32 member, u32 family)
{
	if (result) {
		result->error = error;
		result->member_index = member;
		result->family_index = family;
	}
	return -EINVAL;
}

static int source_matches(
	const struct orlix_tcti_execution_slice_map_source *source,
	const struct orlix_tcti_target_instruction_artifact *instructions)
{
	return instructions &&
		source->version == source_manifest.version &&
		source->architecture && source->build && source->reference &&
		source->schema && source->sha256 &&
		!strcmp(source->architecture, source_manifest.architecture) &&
		!strcmp(source->build, source_manifest.build) &&
		!strcmp(source->reference, source_manifest.reference) &&
		!strcmp(source->schema, source_manifest.schema) &&
		!strcmp(source->sha256, source_manifest.sha256) &&
		source->length == source_manifest.length &&
		!strcmp(source->architecture, instructions->architecture) &&
		!strcmp(source->build, instructions->build) &&
		!strcmp(source->reference, instructions->reference) &&
		!strcmp(source->schema, instructions->schema) &&
		!strcmp(source->sha256, instructions->source_sha256);
}

static int valid_stable_id(const char *stable_id)
{
	size_t index;

	if (!stable_id || !stable_id[0])
		return 0;
	for (index = 0; stable_id[index]; index++) {
		char value = stable_id[index];

		if ((value < 'a' || value > 'z') &&
		    (value < '0' || value > '9') && value != '-')
			return 0;
		if (value == '-' && (!index || !stable_id[index + 1U] ||
				     stable_id[index + 1U] == '-'))
			return 0;
	}
	return 1;
}

static int hex_nibble(char value, u32 *nibble)
{
	if (value >= '0' && value <= '9') {
		*nibble = (u32)(value - '0');
		return 0;
	}
	if (value >= 'a' && value <= 'f') {
		*nibble = (u32)(value - 'a' + 10);
		return 0;
	}
	return -EINVAL;
}

static int condition_matches(
	const char *hex,
	const struct orlix_tcti_target_instruction_artifact_leaf *leaf,
	const struct orlix_tcti_target_instruction_artifact *instructions)
{
	size_t index;

	if (!hex || leaf->condition_offset > instructions->condition_pool_size ||
	    leaf->condition_length >
		    instructions->condition_pool_size - leaf->condition_offset ||
	    strlen(hex) != (size_t)leaf->condition_length * 2U)
		return 0;
	for (index = 0; index < leaf->condition_length; index++) {
		u32 high;
		u32 low;

		if (hex_nibble(hex[index * 2U], &high) ||
		    hex_nibble(hex[index * 2U + 1U], &low) ||
		    instructions->condition_pool[leaf->condition_offset + index] !=
			    (u8)((high << 4) | low))
			return 0;
	}
	return 1;
}

int orlix_tcti_execution_slice_map_validate(
	const struct orlix_tcti_execution_slice_map *map,
	struct orlix_tcti_execution_slice_map_validation_result *result)
{
	const struct orlix_tcti_target_instruction_artifact *instructions =
		orlix_tcti_target_instruction_artifact_canonical();
	struct orlix_tcti_target_instruction_artifact_validation_result
		instruction_result;
	u32 actual_counts[ORLIX_TCTI_EXECUTION_SLICE_MAP_MAX_FAMILY_COUNT] = { 0 };
	size_t family_index;
	size_t member_index;

	if (result)
		*result = (struct orlix_tcti_execution_slice_map_validation_result) {
			.error = ORLIX_TCTI_EXECUTION_SLICE_MAP_VALID,
		};
	if (!map || !map->families || !map->members)
		return fail(result, ORLIX_TCTI_EXECUTION_SLICE_MAP_INVALID_ARGUMENT,
			    0, 0);
	if (!instructions || orlix_tcti_target_instruction_artifact_validate(
				     instructions, &instruction_result) ||
	    !source_matches(&map->source, instructions))
		return fail(result,
			    ORLIX_TCTI_EXECUTION_SLICE_MAP_PROVENANCE_MISMATCH,
			    0, 0);
	if (map->counts.leaf_count != ORLIX_TCTI_EXECUTION_SLICE_MAP_LEAF_COUNT ||
	    map->counts.family_count != ARRAY_SIZE(canonical_families) ||
	    map->counts.family_count !=
		    (size_t)ORLIX_TCTI_EXECUTION_SLICE_FAMILY_COUNT ||
	    map->counts.family_count == 0 ||
	    map->counts.family_count >
		    ORLIX_TCTI_EXECUTION_SLICE_MAP_MAX_FAMILY_COUNT ||
	    ARRAY_SIZE(canonical_members) !=
		    ORLIX_TCTI_EXECUTION_SLICE_MAP_LEAF_COUNT ||
	    ARRAY_SIZE(source_rows) != ORLIX_TCTI_EXECUTION_SLICE_MAP_LEAF_COUNT)
		return fail(result, ORLIX_TCTI_EXECUTION_SLICE_MAP_COUNT_MISMATCH,
			    0, 0);

	/* The family plane is capped at 64, so this bounded prepass is constant. */
	for (family_index = 0; family_index < map->counts.family_count;
	     family_index++) {
		const struct orlix_tcti_execution_slice_family *family =
			&map->families[family_index];
		const struct orlix_tcti_execution_slice_family *canonical =
			&canonical_families[family_index];
		size_t previous;

		if (!valid_stable_id(family->stable_id) || !family->issue_id ||
		    !family->declared_member_count)
			return fail(result,
				    ORLIX_TCTI_EXECUTION_SLICE_MAP_FAMILY_INVALID,
				    0, (u32)family_index);
		for (previous = 0; previous < family_index; previous++) {
			if (family->issue_id == map->families[previous].issue_id ||
			    !strcmp(family->stable_id,
				    map->families[previous].stable_id))
				return fail(result,
					    ORLIX_TCTI_EXECUTION_SLICE_MAP_FAMILY_DUPLICATE,
					    0, (u32)family_index);
		}
		if (strcmp(family->stable_id, canonical->stable_id) ||
		    family->issue_id != canonical->issue_id)
			return fail(result,
				    ORLIX_TCTI_EXECUTION_SLICE_MAP_FAMILY_INVALID,
				    0, (u32)family_index);
	}

	for (member_index = 0; member_index < map->counts.leaf_count;
	     member_index++) {
		const struct orlix_tcti_execution_slice_member *member =
			&map->members[member_index];
		const struct orlix_tcti_target_instruction_artifact_leaf *leaf =
			&instructions->leaves[member_index];
		const char *instruction_name =
			(const char *)instructions->string_pool + leaf->name_offset;

		if (member->ordinal != member_index)
			return fail(result,
				    ORLIX_TCTI_EXECUTION_SLICE_MAP_ORDINAL_MISMATCH,
				    (u32)member_index, member->family_index);
		if (!member->source_name ||
		    strcmp(member->source_name, source_rows[member_index].name) ||
		    strcmp(member->source_name, instruction_name))
			return fail(result,
				    ORLIX_TCTI_EXECUTION_SLICE_MAP_SOURCE_NAME_MISMATCH,
				    (u32)member_index, member->family_index);
		if (!member->condition_tcnd_hex ||
		    strcmp(member->condition_tcnd_hex,
			   source_rows[member_index].condition_tcnd_hex) ||
		    !condition_matches(member->condition_tcnd_hex, leaf,
				       instructions))
			return fail(result,
				    ORLIX_TCTI_EXECUTION_SLICE_MAP_CONDITION_MISMATCH,
				    (u32)member_index, member->family_index);
		if (member->family_index >= map->counts.family_count)
			return fail(result,
				    ORLIX_TCTI_EXECUTION_SLICE_MAP_UNKNOWN_FAMILY,
				    (u32)member_index, member->family_index);
		if (member->family_index !=
		    canonical_members[member_index].family_index)
			return fail(result,
				    ORLIX_TCTI_EXECUTION_SLICE_MAP_MEMBERSHIP_MISMATCH,
				    (u32)member_index, member->family_index);
		actual_counts[member->family_index]++;
	}

	for (family_index = 0; family_index < map->counts.family_count;
	     family_index++) {
		if (actual_counts[family_index] !=
		    map->families[family_index].declared_member_count)
			return fail(result,
				    ORLIX_TCTI_EXECUTION_SLICE_MAP_COUNT_MISMATCH,
				    ORLIX_TCTI_EXECUTION_SLICE_MAP_LEAF_COUNT,
				    (u32)family_index);
	}
	return 0;
}

const struct orlix_tcti_execution_slice_map *
orlix_tcti_execution_slice_map_canonical(void)
{
	return &canonical_map;
}

const char *orlix_tcti_execution_slice_map_error_name(
	enum orlix_tcti_execution_slice_map_error error)
{
	switch (error) {
	case ORLIX_TCTI_EXECUTION_SLICE_MAP_VALID:
		return "valid";
	case ORLIX_TCTI_EXECUTION_SLICE_MAP_INVALID_ARGUMENT:
		return "invalid argument";
	case ORLIX_TCTI_EXECUTION_SLICE_MAP_PROVENANCE_MISMATCH:
		return "provenance mismatch";
	case ORLIX_TCTI_EXECUTION_SLICE_MAP_COUNT_MISMATCH:
		return "count mismatch";
	case ORLIX_TCTI_EXECUTION_SLICE_MAP_FAMILY_INVALID:
		return "family invalid";
	case ORLIX_TCTI_EXECUTION_SLICE_MAP_FAMILY_DUPLICATE:
		return "family duplicate";
	case ORLIX_TCTI_EXECUTION_SLICE_MAP_ORDINAL_MISMATCH:
		return "ordinal mismatch";
	case ORLIX_TCTI_EXECUTION_SLICE_MAP_SOURCE_NAME_MISMATCH:
		return "source name mismatch";
	case ORLIX_TCTI_EXECUTION_SLICE_MAP_CONDITION_MISMATCH:
		return "condition mismatch";
	case ORLIX_TCTI_EXECUTION_SLICE_MAP_UNKNOWN_FAMILY:
		return "unknown family";
	case ORLIX_TCTI_EXECUTION_SLICE_MAP_MEMBERSHIP_MISMATCH:
		return "membership mismatch";
	}
	return "unknown";
}
