/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_instruction_artifact.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FIXTURE_STRING_BYTES (TCTI_A64_INSTRUCTION_ARTIFACT_LEAF_COUNT * 16U + 16U)

static unsigned int failures;

#define EXPECT(expression) do { \
	if (!(expression)) { \
		fprintf(stderr, "%s:%d: expectation failed: %s\n", \
			__FILE__, __LINE__, #expression); \
		failures++; \
	} \
} while (0)

struct fixture {
	struct tcti_target_instruction_artifact artifact;
	struct tcti_target_instruction_artifact_leaf leaves[
		TCTI_A64_INSTRUCTION_ARTIFACT_LEAF_COUNT];
	struct tcti_target_instruction_artifact_operand operands[
		TCTI_A64_INSTRUCTION_ARTIFACT_LEAF_COUNT + 1U];
	uint8_t strings[FIXTURE_STRING_BYTES];
	uint8_t conditions[2048];
	size_t strings_used;
	size_t condition_length;
};

static uint32_t add_string(struct fixture *fixture, const char *text)
{
	size_t length = strlen(text) + 1U;
	uint32_t offset = fixture->strings_used;

	if (length > sizeof(fixture->strings) - fixture->strings_used)
		abort();
	memcpy(fixture->strings + fixture->strings_used, text, length);
	fixture->strings_used += length;
	return offset;
}

static void fixture_set_condition(struct fixture *fixture, const uint8_t *data,
				  size_t length)
{
	unsigned int index;

	if (length > sizeof(fixture->conditions) || length > UINT32_MAX)
		abort();
	memcpy(fixture->conditions, data, length);
	fixture->condition_length = length;
	fixture->artifact.condition_pool = fixture->conditions;
	fixture->artifact.condition_pool_size = length;
	for (index = 0; index < TCTI_A64_INSTRUCTION_ARTIFACT_LEAF_COUNT;
	     index++) {
		fixture->leaves[index].condition_offset = 0;
		fixture->leaves[index].condition_length = length;
		fixture->operands[index].condition_offset = 0;
		fixture->operands[index].condition_length = length;
	}
}

static void fixture_add_overlapping_operand(struct fixture *fixture,
	unsigned int leaf_index)
{
	unsigned int index;

	if (leaf_index + 1U >= TCTI_A64_INSTRUCTION_ARTIFACT_LEAF_COUNT)
		abort();
	memmove(&fixture->operands[leaf_index + 1U],
		&fixture->operands[leaf_index],
		(TCTI_A64_INSTRUCTION_ARTIFACT_LEAF_COUNT - leaf_index) *
			sizeof(*fixture->operands));
	fixture->operands[leaf_index + 1U] = fixture->operands[leaf_index];
	fixture->leaves[leaf_index].operand_count = 2;
	for (index = leaf_index + 1U;
	     index < TCTI_A64_INSTRUCTION_ARTIFACT_LEAF_COUNT; index++)
		fixture->leaves[index].operand_first++;
	fixture->artifact.operand_count++;
}

static void fixture_initialize(struct fixture *fixture)
{
	uint32_t mnemonic_offset;
	uint32_t operation_offset;
	uint32_t operand_name_offset;
	unsigned int index;

	memset(fixture, 0, sizeof(*fixture));
	mnemonic_offset = add_string(fixture, "mn");
	operation_offset = add_string(fixture, "op");
	operand_name_offset = add_string(fixture, "rd");
	for (index = 0; index < TCTI_A64_INSTRUCTION_ARTIFACT_LEAF_COUNT;
	     index++) {
		char name[16];

		(void)snprintf(name, sizeof(name), "leaf%u", index);
		fixture->leaves[index] =
			(struct tcti_target_instruction_artifact_leaf) {
				.name_offset = add_string(fixture, name),
				.mnemonic_offset = mnemonic_offset,
				.operation_offset = operation_offset,
				.encoding_mask = UINT32_MAX & ~UINT32_C(1),
				.encoding_pattern = 0,
				.condition_offset = 0,
				.condition_length = 11,
				.operand_first = index,
				.operand_count = 1,
			};
		fixture->operands[index] =
			(struct tcti_target_instruction_artifact_operand) {
				.name_offset = operand_name_offset,
				.leaf_index = index,
				.condition_offset = 0,
				.condition_length = 11,
				.variable_mask = 1,
				.start = 0,
				.width = 1,
			};
	}
	{
		static const uint8_t condition[] = {
			'T', 'C', 'N', 'D', 1, 1, 0, 0, 0, 1, 1,
		};

		memcpy(fixture->conditions, condition, sizeof(condition));
		fixture->condition_length = sizeof(condition);
	}
	fixture->artifact = (struct tcti_target_instruction_artifact) {
		.version = TCTI_A64_INSTRUCTION_ARTIFACT_VERSION,
		.architecture = "vFATAp1-A",
		.build = "818",
		.reference = "2026-06_rel",
		.schema = "2.9.5",
		.source_sha256 =
			"a1ad2c6538a47cd97d8762791ac5af88bce1d5f6aff096c9b77aef853e76acfe",
		.leaves = fixture->leaves,
		.leaf_count = TCTI_A64_INSTRUCTION_ARTIFACT_LEAF_COUNT,
		.operands = fixture->operands,
		.operand_count = TCTI_A64_INSTRUCTION_ARTIFACT_LEAF_COUNT,
		.string_pool = fixture->strings,
		.string_pool_size = fixture->strings_used,
		.condition_pool = fixture->conditions,
		.condition_pool_size = fixture->condition_length,
	};
}

static void expect_error(const struct fixture *fixture,
	enum tcti_target_instruction_artifact_validation_error expected)
{
	struct tcti_target_instruction_artifact_validation_result result;

	EXPECT(tcti_target_instruction_artifact_validate(&fixture->artifact,
							&result) == -1);
	EXPECT(result.error == expected);
}

static void test_valid_complete_artifact(void)
{
	struct fixture fixture;
	struct tcti_target_instruction_artifact_validation_result result;

	fixture_initialize(&fixture);
	EXPECT(tcti_target_instruction_artifact_validate(&fixture.artifact,
							&result) == 0);
	EXPECT(result.error == TCTI_TARGET_INSTRUCTION_ARTIFACT_VALID);
	EXPECT(result.leaf_index == UINT32_MAX);
	EXPECT(result.operand_index == UINT32_MAX);
}

static void test_header_and_provenance_rejections(void)
{
	struct fixture fixture;

	fixture_initialize(&fixture);
	fixture.artifact.version++;
	expect_error(&fixture,
		TCTI_TARGET_INSTRUCTION_ARTIFACT_VERSION_MISMATCH);
	fixture_initialize(&fixture);
	fixture.artifact.architecture = "v9-A";
	expect_error(&fixture,
		TCTI_TARGET_INSTRUCTION_ARTIFACT_PROVENANCE_MISMATCH);
	fixture_initialize(&fixture);
	fixture.artifact.source_sha256 = "invalid";
	expect_error(&fixture,
		TCTI_TARGET_INSTRUCTION_ARTIFACT_PROVENANCE_MISMATCH);
	fixture_initialize(&fixture);
	fixture.artifact.leaf_count--;
	expect_error(&fixture,
		TCTI_TARGET_INSTRUCTION_ARTIFACT_COUNT_MISMATCH);
	fixture_initialize(&fixture);
	fixture.artifact.operands = NULL;
	expect_error(&fixture, TCTI_TARGET_INSTRUCTION_ARTIFACT_POOL_INVALID);
}

static void test_string_and_condition_rejections(void)
{
	struct fixture fixture;

	fixture_initialize(&fixture);
	fixture.leaves[17].name_offset = UINT32_MAX;
	expect_error(&fixture, TCTI_TARGET_INSTRUCTION_ARTIFACT_STRING_INVALID);
	fixture_initialize(&fixture);
	fixture.artifact.string_pool_size = 1;
	expect_error(&fixture, TCTI_TARGET_INSTRUCTION_ARTIFACT_STRING_INVALID);
	fixture_initialize(&fixture);
	fixture.leaves[17].name_offset++;
	expect_error(&fixture, TCTI_TARGET_INSTRUCTION_ARTIFACT_STRING_INVALID);
	fixture_initialize(&fixture);
	fixture.leaves[17].condition_length = 0;
	expect_error(&fixture,
		TCTI_TARGET_INSTRUCTION_ARTIFACT_CONDITION_INVALID);
	fixture_initialize(&fixture);
	fixture.conditions[0] = 'X';
	expect_error(&fixture,
		TCTI_TARGET_INSTRUCTION_ARTIFACT_CONDITION_INVALID);
	fixture_initialize(&fixture);
	fixture.operands[17].condition_offset = UINT32_MAX;
	expect_error(&fixture,
		TCTI_TARGET_INSTRUCTION_ARTIFACT_CONDITION_INVALID);
}

static void test_condition_bytecode_validation(void)
{
	static const uint8_t text[] = {
		'T', 'C', 'N', 'D', 1, 2, 0, 0, 0, 5, 0, 0, 0, 1, 'x',
	};
	static const uint8_t set[] = {
		'T', 'C', 'N', 'D', 1, 5, 0, 0, 0, 10, 0, 0, 0, 1,
		1, 0, 0, 0, 1, 1,
	};
	static const uint8_t not[] = {
		'T', 'C', 'N', 'D', 1, 6, 0, 0, 0, 6,
		1, 0, 0, 0, 1, 1,
	};
	static const uint8_t binary[] = {
		'T', 'C', 'N', 'D', 1, 7, 0, 0, 0, 12,
		1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 0,
	};
	static const uint8_t unknown[] = {
		'T', 'C', 'N', 'D', 1, 0xff, 0, 0, 0, 0,
	};
	static const uint8_t bad_length[] = {
		'T', 'C', 'N', 'D', 1, 1, 0, 0, 0, 2, 1,
	};
	static const uint8_t bad_bool[] = {
		'T', 'C', 'N', 'D', 1, 1, 0, 0, 0, 1, 2,
	};
	static const uint8_t set_count[] = {
		'T', 'C', 'N', 'D', 1, 5, 0, 0, 0, 10, 0, 0, 0, 2,
		1, 0, 0, 0, 1, 1,
	};
	static const uint8_t not_trailing[] = {
		'T', 'C', 'N', 'D', 1, 6, 0, 0, 0, 7,
		1, 0, 0, 0, 1, 1, 0,
	};
	struct fixture fixture;
	struct tcti_target_instruction_artifact_validation_result result;
	unsigned int tag;

	fixture_initialize(&fixture);
	for (tag = 2; tag <= 4; tag++) {
		uint8_t copy[sizeof(text)];

		memcpy(copy, text, sizeof(copy));
		copy[5] = tag;
		fixture_set_condition(&fixture, copy, sizeof(copy));
		EXPECT(tcti_target_instruction_artifact_validate(&fixture.artifact,
								&result) == 0);
	}
	fixture_set_condition(&fixture, set, sizeof(set));
	EXPECT(tcti_target_instruction_artifact_validate(&fixture.artifact,
							&result) == 0);
	fixture_set_condition(&fixture, not, sizeof(not));
	EXPECT(tcti_target_instruction_artifact_validate(&fixture.artifact,
							&result) == 0);
	for (tag = 7; tag <= 11; tag++) {
		uint8_t copy[sizeof(binary)];

		memcpy(copy, binary, sizeof(copy));
		copy[5] = tag;
		fixture_set_condition(&fixture, copy, sizeof(copy));
		EXPECT(tcti_target_instruction_artifact_validate(&fixture.artifact,
								&result) == 0);
	}
	fixture_set_condition(&fixture, unknown, sizeof(unknown));
	expect_error(&fixture,
		TCTI_TARGET_INSTRUCTION_ARTIFACT_CONDITION_INVALID);
	fixture_set_condition(&fixture, bad_length, sizeof(bad_length));
	expect_error(&fixture,
		TCTI_TARGET_INSTRUCTION_ARTIFACT_CONDITION_INVALID);
	fixture_set_condition(&fixture, bad_bool, sizeof(bad_bool));
	expect_error(&fixture,
		TCTI_TARGET_INSTRUCTION_ARTIFACT_CONDITION_INVALID);
	fixture_set_condition(&fixture, set_count, sizeof(set_count));
	expect_error(&fixture,
		TCTI_TARGET_INSTRUCTION_ARTIFACT_CONDITION_INVALID);
	fixture_set_condition(&fixture, not_trailing, sizeof(not_trailing));
	expect_error(&fixture,
		TCTI_TARGET_INSTRUCTION_ARTIFACT_CONDITION_INVALID);
	fixture_initialize(&fixture);
	fixture.artifact.condition_pool_size--;
	expect_error(&fixture,
		TCTI_TARGET_INSTRUCTION_ARTIFACT_CONDITION_INVALID);
}

static void test_condition_bytecode_depth_limit(void)
{
	uint8_t condition[2048] = { 'T', 'C', 'N', 'D', 1, 1, 0, 0, 0, 1, 1 };
	struct fixture fixture;
	struct tcti_target_instruction_artifact_validation_result result;
	size_t length = 11;
	unsigned int nesting;

	fixture_initialize(&fixture);
	/* 255 nested NOT records leave the BOOL at depth 255, still valid. */
	for (nesting = 0; nesting < 255; nesting++) {
		size_t payload_length = length - 5U;

		memmove(condition + 10U, condition + 5U, payload_length);
		condition[5] = 6;
		condition[6] = (uint8_t)(payload_length >> 24);
		condition[7] = (uint8_t)(payload_length >> 16);
		condition[8] = (uint8_t)(payload_length >> 8);
		condition[9] = (uint8_t)payload_length;
		length += 5U;
	}
	fixture_set_condition(&fixture, condition, length);
	EXPECT(tcti_target_instruction_artifact_validate(&fixture.artifact,
							&result) == 0);

	/* One additional NOT places the BOOL at depth 256 and must fail closed. */
	memmove(condition + 10U, condition + 5U, length - 5U);
	condition[5] = 6;
	condition[6] = (uint8_t)((length - 5U) >> 24);
	condition[7] = (uint8_t)((length - 5U) >> 16);
	condition[8] = (uint8_t)((length - 5U) >> 8);
	condition[9] = (uint8_t)(length - 5U);
	fixture_set_condition(&fixture, condition, length + 5U);
	expect_error(&fixture,
		TCTI_TARGET_INSTRUCTION_ARTIFACT_CONDITION_INVALID);
}

static void test_leaf_and_span_rejections(void)
{
	struct fixture fixture;

	fixture_initialize(&fixture);
	fixture.leaves[17].encoding_pattern = UINT32_C(1);
	expect_error(&fixture, TCTI_TARGET_INSTRUCTION_ARTIFACT_LEAF_INVALID);
	fixture_initialize(&fixture);
	fixture.leaves[17].operand_first++;
	expect_error(&fixture, TCTI_TARGET_INSTRUCTION_ARTIFACT_SPAN_INVALID);
	fixture_initialize(&fixture);
	fixture.leaves[17].operand_count = UINT32_MAX;
	expect_error(&fixture, TCTI_TARGET_INSTRUCTION_ARTIFACT_SPAN_INVALID);
	fixture_initialize(&fixture);
	fixture.artifact.operand_count--;
	expect_error(&fixture, TCTI_TARGET_INSTRUCTION_ARTIFACT_SPAN_INVALID);
	fixture_initialize(&fixture);
	fixture.leaves[17].name_offset = fixture.leaves[16].name_offset;
	expect_error(&fixture, TCTI_TARGET_INSTRUCTION_ARTIFACT_LEAF_INVALID);
}

static void test_operand_rejections(void)
{
	struct fixture fixture;

	fixture_initialize(&fixture);
	fixture.operands[17].leaf_index = 0;
	expect_error(&fixture, TCTI_TARGET_INSTRUCTION_ARTIFACT_OPERAND_INVALID);
	fixture_initialize(&fixture);
	fixture.operands[17].name_offset = UINT32_MAX;
	expect_error(&fixture, TCTI_TARGET_INSTRUCTION_ARTIFACT_OPERAND_INVALID);
	fixture_initialize(&fixture);
	fixture.operands[17].variable_mask = 0;
	expect_error(&fixture, TCTI_TARGET_INSTRUCTION_ARTIFACT_OPERAND_INVALID);
	fixture_initialize(&fixture);
	fixture.operands[17].width = 0;
	expect_error(&fixture, TCTI_TARGET_INSTRUCTION_ARTIFACT_OPERAND_INVALID);
	fixture_initialize(&fixture);
	fixture.operands[17].start = 32;
	expect_error(&fixture, TCTI_TARGET_INSTRUCTION_ARTIFACT_OPERAND_INVALID);
	fixture_initialize(&fixture);
	fixture.operands[17].variable_mask = 2;
	expect_error(&fixture, TCTI_TARGET_INSTRUCTION_ARTIFACT_OPERAND_INVALID);
	fixture_initialize(&fixture);
	fixture.operands[17].variable_mask = 1;
	fixture.leaves[17].encoding_mask |= 1;
	expect_error(&fixture, TCTI_TARGET_INSTRUCTION_ARTIFACT_OPERAND_INVALID);
	fixture_initialize(&fixture);
	fixture_add_overlapping_operand(&fixture, 17);
	expect_error(&fixture, TCTI_TARGET_INSTRUCTION_ARTIFACT_OPERAND_INVALID);
	fixture_initialize(&fixture);
	memcpy(fixture.conditions + fixture.condition_length,
	       fixture.conditions, fixture.condition_length);
	fixture.artifact.condition_pool_size += fixture.condition_length;
	fixture.operands[17].condition_offset = fixture.condition_length;
	expect_error(&fixture, TCTI_TARGET_INSTRUCTION_ARTIFACT_OPERAND_INVALID);
}

int main(void)
{
	struct tcti_target_instruction_artifact_validation_result result;

	EXPECT(tcti_target_instruction_artifact_validate(NULL, &result) == -1);
	EXPECT(result.error == TCTI_TARGET_INSTRUCTION_ARTIFACT_INVALID_ARGUMENT);
	test_valid_complete_artifact();
	test_header_and_provenance_rejections();
	test_string_and_condition_rejections();
	test_condition_bytecode_validation();
	test_condition_bytecode_depth_limit();
	test_leaf_and_span_rejections();
	test_operand_rejections();
	if (failures) {
		fprintf(stderr, "%u target instruction artifact test(s) failed\n",
			failures);
		return EXIT_FAILURE;
	}
	puts("target instruction artifact tests: 1 passed");
	return EXIT_SUCCESS;
}
