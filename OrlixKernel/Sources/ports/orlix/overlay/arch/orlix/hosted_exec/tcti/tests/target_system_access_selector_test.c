// SPDX-License-Identifier: GPL-2.0-only
#include "target_system_access_selector.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expression) do { \
	if (!(expression)) { \
		fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
		return -1; \
	} \
} while (0)

static int values_value_representation(void)
{
	struct tcti_system_access_value value = { 0 };

	CHECK(!tcti_system_access_value_parse("'11'", 4U, 2U, &value));
	CHECK(value.width == 2U);
	CHECK(value.value == 3U);
	CHECK(!tcti_system_access_value_parse("'0010'", 6U, 4U, &value));
	CHECK(value.width == 4U);
	CHECK(value.value == 2U);
	CHECK(tcti_system_access_value_parse("'010'", 5U, 4U, &value) == -EINVAL);
	CHECK(tcti_system_access_value_parse("010", 3U, 3U, &value) == -EINVAL);
	CHECK(tcti_system_access_value_parse("'012'", 5U, 3U, &value) == -EINVAL);
	CHECK(tcti_system_access_value_parse("'000000000'", 11U, 9U, &value) == -EINVAL);
	CHECK(tcti_system_access_value_parse(NULL, 0U, 1U, &value) == -EINVAL);
	CHECK(tcti_system_access_value_parse("'0'", 3U, 1U, NULL) == -EINVAL);
	return 0;
}

static int equation_and_group_normalization(void)
{
	static const struct tcti_system_access_slice equation_slices[] = {
		{ .start = 0U, .width = 2U },
		{ .start = 2U, .width = 2U },
	};
	static const struct tcti_system_access_slice collision_slices[] = {
		{ .start = 0U, .width = 2U },
		{ .start = 1U, .width = 2U },
	};
	static const struct tcti_system_access_source_value group_members[] = {
		{ .kind = TCTI_SYSTEM_ACCESS_SOURCE_VALUE,
		  .value = { .literal = "'01'", .length = 4U } },
		{ .kind = TCTI_SYSTEM_ACCESS_SOURCE_VALUE,
		  .value = { .literal = "'10'", .length = 4U } },
	};
	static const struct tcti_system_access_slice group_equation_slices[] = {
		{ .start = 0U, .width = 2U },
	};
	static const struct tcti_system_access_source_value fragmented_group_members[] = {
		{ .kind = TCTI_SYSTEM_ACCESS_SOURCE_VALUE,
		  .value = { .literal = "'0'", .length = 3U } },
		{ .kind = TCTI_SYSTEM_ACCESS_SOURCE_EQUATION,
		  .equation = { .identifier = "m", .identifier_length = 1U,
			.source_width = 2U, .slices = group_equation_slices,
			.slice_count = 1U } },
		{ .kind = TCTI_SYSTEM_ACCESS_SOURCE_VALUE,
		  .value = { .literal = "'1'", .length = 3U } },
	};
	static const struct tcti_system_access_source_value nested_member[] = {
		{ .kind = TCTI_SYSTEM_ACCESS_SOURCE_VALUE,
		  .value = { .literal = "'0'", .length = 3U } },
	};
	static const struct tcti_system_access_source_value nested_group_members[] = {
		{ .kind = TCTI_SYSTEM_ACCESS_SOURCE_GROUP,
		  .group = { .members = nested_member, .member_count = 1U } },
		{ .kind = TCTI_SYSTEM_ACCESS_SOURCE_VALUE,
		  .value = { .literal = "'000'", .length = 5U } },
	};
	struct tcti_system_access_source_value equation = {
		.kind = TCTI_SYSTEM_ACCESS_SOURCE_EQUATION,
		.equation = {
			.identifier = "m", .identifier_length = 1U,
			.source_width = 4U, .slices = equation_slices,
			.slice_count = 2U,
		},
	};
	struct tcti_system_access_source_value collision = {
		.kind = TCTI_SYSTEM_ACCESS_SOURCE_EQUATION,
		.equation = {
			.identifier = "m", .identifier_length = 1U,
			.source_width = 4U, .slices = collision_slices,
			.slice_count = 2U,
		},
	};
	struct tcti_system_access_source_value group = {
		.kind = TCTI_SYSTEM_ACCESS_SOURCE_GROUP,
		.group = { .members = group_members, .member_count = 2U },
	};
	struct tcti_system_access_source_value fragmented_group = {
		.kind = TCTI_SYSTEM_ACCESS_SOURCE_GROUP,
		.group = { .members = fragmented_group_members, .member_count = 3U },
	};
	struct tcti_system_access_source_value oversized_group = {
		.kind = TCTI_SYSTEM_ACCESS_SOURCE_GROUP,
		.group = { .members = group_members,
			.member_count = TCTI_SYSTEM_ACCESS_VALUE_MAX_GROUP_MEMBERS + 1U },
	};
	struct tcti_system_access_source_value nested_group = {
		.kind = TCTI_SYSTEM_ACCESS_SOURCE_GROUP,
		.group = { .members = nested_group_members, .member_count = 2U },
	};
	struct tcti_system_access_normalized_value members[8] = { { 0 } };
	struct tcti_system_access_value_scratch scratch = {
		.members = members, .member_capacity = 8U,
	};
	struct tcti_system_access_normalized_value normalized = { 0 };
	uint8_t width = 0;

	CHECK(!tcti_system_access_selector_field_width(
		TCTI_SYSTEM_ACCESS_SELECTOR_MRS,
		TCTI_SYSTEM_ACCESS_SELECTOR_FIELD_OP0, &width));
	CHECK(width == 2U);
	CHECK(!tcti_system_access_selector_field_width(
		TCTI_SYSTEM_ACCESS_SELECTOR_SYS,
		TCTI_SYSTEM_ACCESS_SELECTOR_FIELD_CRM, &width));
	CHECK(width == 4U);
	CHECK(tcti_system_access_selector_field_width(
		TCTI_SYSTEM_ACCESS_SELECTOR_SYS,
		TCTI_SYSTEM_ACCESS_SELECTOR_FIELD_OP0, &width) == -EINVAL);

	CHECK(!tcti_system_access_value_normalize(
		TCTI_SYSTEM_ACCESS_SELECTOR_MRS,
		TCTI_SYSTEM_ACCESS_SELECTOR_FIELD_CRM, &equation, &scratch,
		&normalized));
	CHECK(normalized.kind == TCTI_SYSTEM_ACCESS_NORMALIZED_EQUATION);
	CHECK(normalized.field == TCTI_SYSTEM_ACCESS_SELECTOR_FIELD_CRM);
	CHECK(normalized.width == 4U);
	CHECK(normalized.equation.identifier_length == 1U);
	CHECK(!memcmp(normalized.equation.identifier, "m", 1U));
	CHECK(normalized.equation.slice_count == 2U);

	CHECK(!tcti_system_access_value_normalize(
		TCTI_SYSTEM_ACCESS_SELECTOR_MRS,
		TCTI_SYSTEM_ACCESS_SELECTOR_FIELD_CRM, &group, &scratch,
		&normalized));
	CHECK(normalized.kind == TCTI_SYSTEM_ACCESS_NORMALIZED_GROUP);
	CHECK(normalized.group.first_member == 0U);
	CHECK(normalized.group.member_count == 2U);
	CHECK(scratch.member_count == 2U);
	CHECK(members[0].kind == TCTI_SYSTEM_ACCESS_NORMALIZED_VALUE);
	CHECK(members[0].value == 1U);
	CHECK(members[0].width == 2U);
	CHECK(members[0].lsb == 2U);
	CHECK(members[1].kind == TCTI_SYSTEM_ACCESS_NORMALIZED_VALUE);
	CHECK(members[1].value == 2U);
	CHECK(members[1].width == 2U);
	CHECK(members[1].lsb == 0U);

	CHECK(!tcti_system_access_value_normalize(
		TCTI_SYSTEM_ACCESS_SELECTOR_MRS,
		TCTI_SYSTEM_ACCESS_SELECTOR_FIELD_CRM, &fragmented_group, &scratch,
		&normalized));
	CHECK(normalized.group.first_member == 2U);
	CHECK(normalized.group.member_count == 3U);
	CHECK(scratch.member_count == 5U);
	CHECK(members[2].kind == TCTI_SYSTEM_ACCESS_NORMALIZED_VALUE);
	CHECK(members[2].value == 0U);
	CHECK(members[2].width == 1U);
	CHECK(members[2].lsb == 3U);
	CHECK(members[3].kind == TCTI_SYSTEM_ACCESS_NORMALIZED_EQUATION);
	CHECK(members[3].width == 2U);
	CHECK(members[3].lsb == 1U);
	CHECK(members[4].kind == TCTI_SYSTEM_ACCESS_NORMALIZED_VALUE);
	CHECK(members[4].value == 1U);
	CHECK(members[4].width == 1U);
	CHECK(members[4].lsb == 0U);

	CHECK(tcti_system_access_value_normalize(
		TCTI_SYSTEM_ACCESS_SELECTOR_MRS,
		TCTI_SYSTEM_ACCESS_SELECTOR_FIELD_CRM, &collision, &scratch,
		&normalized) == -EINVAL);
	CHECK(scratch.member_count == 5U);
	CHECK(tcti_system_access_value_normalize(
		TCTI_SYSTEM_ACCESS_SELECTOR_MRS,
		TCTI_SYSTEM_ACCESS_SELECTOR_FIELD_CRM, &nested_group, &scratch,
		&normalized) == -EINVAL);
	CHECK(scratch.member_count == 5U);
	CHECK(tcti_system_access_value_normalize(
		TCTI_SYSTEM_ACCESS_SELECTOR_MRS,
		TCTI_SYSTEM_ACCESS_SELECTOR_FIELD_CRM, &oversized_group, &scratch,
		&normalized) == -EINVAL);
	CHECK(scratch.member_count == 5U);
	scratch.member_count = scratch.member_capacity + 1U;
	CHECK(tcti_system_access_value_normalize(
		TCTI_SYSTEM_ACCESS_SELECTOR_MRS,
		TCTI_SYSTEM_ACCESS_SELECTOR_FIELD_CRM, &group, &scratch,
		&normalized) == -EINVAL);
	CHECK(scratch.member_count == scratch.member_capacity + 1U);
	return 0;
}

static int mrs_msr_packing(void)
{
	struct tcti_system_access_mrs_msr_fields fields = {
		.op0 = 3U, .op1 = 3U, .crn = 2U, .crm = 4U, .op2 = 0U,
	};
	uint16_t selector = 0;

	CHECK(!tcti_system_access_mrs_msr_pack(&fields, &selector));
	CHECK(selector == 0x5920U); /* RNDR */
	fields.op2 = 1U;
	CHECK(!tcti_system_access_mrs_msr_pack(&fields, &selector));
	CHECK(selector == 0x5921U); /* RNDRRS */
	fields.op0 = 2U;
	fields.op1 = 0U;
	fields.crn = 0U;
	fields.crm = 0U;
	fields.op2 = 0U;
	CHECK(!tcti_system_access_mrs_msr_pack(&fields, &selector));
	CHECK(selector == 0U);
	fields.op0 = 3U;
	fields.op1 = 7U;
	fields.crn = 15U;
	fields.crm = 15U;
	fields.op2 = 7U;
	CHECK(!tcti_system_access_mrs_msr_pack(&fields, &selector));
	CHECK(selector == 0x7fffU);
	fields.op0 = 1U;
	CHECK(tcti_system_access_mrs_msr_pack(&fields, &selector) == -EINVAL);
	fields.op0 = 4U;
	CHECK(tcti_system_access_mrs_msr_pack(&fields, &selector) == -EINVAL);
	fields.op0 = 3U;
	fields.op1 = 8U;
	CHECK(tcti_system_access_mrs_msr_pack(&fields, &selector) == -EINVAL);
	CHECK(tcti_system_access_mrs_msr_pack(NULL, &selector) == -EINVAL);
	CHECK(tcti_system_access_mrs_msr_pack(&fields, NULL) == -EINVAL);
	return 0;
}

static int sys_sysl_packing(void)
{
	struct tcti_system_access_sys_sysl_fields fields = {
		.op1 = 0U, .crn = 0U, .crm = 0U, .op2 = 0U,
	};
	uint16_t selector = 0;

	CHECK(!tcti_system_access_sys_sysl_pack(&fields, &selector));
	CHECK(selector == 0U);
	fields.op1 = 7U;
	fields.crn = 15U;
	fields.crm = 15U;
	fields.op2 = 7U;
	CHECK(!tcti_system_access_sys_sysl_pack(&fields, &selector));
	CHECK(selector == 0x3fffU);
	fields.op1 = 8U;
	CHECK(tcti_system_access_sys_sysl_pack(&fields, &selector) == -EINVAL);
	fields.op1 = 7U;
	fields.crn = 16U;
	CHECK(tcti_system_access_sys_sysl_pack(&fields, &selector) == -EINVAL);
	CHECK(tcti_system_access_sys_sysl_pack(NULL, &selector) == -EINVAL);
	CHECK(tcti_system_access_sys_sysl_pack(&fields, NULL) == -EINVAL);
	return 0;
}

struct enumeration_state {
	unsigned int count;
	uint16_t previous;
	uint16_t stop_at;
	int stop_code;
};

static int observe_selector(uint16_t selector, void *context)
{
	struct enumeration_state *state = context;

	if (state->count && selector != (uint16_t)(state->previous + 1U))
		return -ERANGE;
	state->previous = selector;
	state->count++;
	if (selector == state->stop_at)
		return state->stop_code;
	return 0;
}

static int selector_enumeration(void)
{
	struct enumeration_state state;
	uint8_t width = 0;

	CHECK(!tcti_system_access_selector_width(TCTI_SYSTEM_ACCESS_SELECTOR_MRS,
						  &width));
	CHECK(width == TCTI_SYSTEM_ACCESS_MRS_MSR_SELECTOR_WIDTH);
	CHECK(!tcti_system_access_selector_width(TCTI_SYSTEM_ACCESS_SELECTOR_MSR,
						  &width));
	CHECK(width == TCTI_SYSTEM_ACCESS_MRS_MSR_SELECTOR_WIDTH);
	CHECK(!tcti_system_access_selector_width(TCTI_SYSTEM_ACCESS_SELECTOR_SYS,
						  &width));
	CHECK(width == TCTI_SYSTEM_ACCESS_SYS_SYSL_SELECTOR_WIDTH);
	CHECK(!tcti_system_access_selector_width(TCTI_SYSTEM_ACCESS_SELECTOR_SYSL,
						  &width));
	CHECK(width == TCTI_SYSTEM_ACCESS_SYS_SYSL_SELECTOR_WIDTH);
	CHECK(tcti_system_access_selector_width((enum tcti_system_access_selector_form)99,
						 &width) == -EINVAL);
	CHECK(tcti_system_access_selector_width(TCTI_SYSTEM_ACCESS_SELECTOR_MRS,
						  NULL) == -EINVAL);

	memset(&state, 0, sizeof(state));
	state.stop_at = UINT16_MAX;
	CHECK(!tcti_system_access_selector_enumerate(
		TCTI_SYSTEM_ACCESS_SELECTOR_MRS, observe_selector, &state));
	CHECK(state.count == TCTI_SYSTEM_ACCESS_MRS_MSR_SELECTOR_COUNT);
	CHECK(state.previous == 0x7fffU);

	memset(&state, 0, sizeof(state));
	state.stop_at = UINT16_MAX;
	CHECK(!tcti_system_access_selector_enumerate(
		TCTI_SYSTEM_ACCESS_SELECTOR_MSR, observe_selector, &state));
	CHECK(state.count == TCTI_SYSTEM_ACCESS_MRS_MSR_SELECTOR_COUNT);
	CHECK(state.previous == 0x7fffU);

	memset(&state, 0, sizeof(state));
	state.stop_at = UINT16_MAX;
	CHECK(!tcti_system_access_selector_enumerate(
		TCTI_SYSTEM_ACCESS_SELECTOR_SYS, observe_selector, &state));
	CHECK(state.count == TCTI_SYSTEM_ACCESS_SYS_SYSL_SELECTOR_COUNT);
	CHECK(state.previous == 0x3fffU);

	memset(&state, 0, sizeof(state));
	state.stop_at = UINT16_MAX;
	CHECK(!tcti_system_access_selector_enumerate(
		TCTI_SYSTEM_ACCESS_SELECTOR_SYSL, observe_selector, &state));
	CHECK(state.count == TCTI_SYSTEM_ACCESS_SYS_SYSL_SELECTOR_COUNT);
	CHECK(state.previous == 0x3fffU);

	memset(&state, 0, sizeof(state));
	state.stop_at = 7U;
	state.stop_code = -EINTR;
	CHECK(tcti_system_access_selector_enumerate(
		TCTI_SYSTEM_ACCESS_SELECTOR_MRS, observe_selector, &state) == -EINTR);
	CHECK(state.count == 8U);
	CHECK(state.previous == 7U);
	CHECK(tcti_system_access_selector_enumerate(
		(enum tcti_system_access_selector_form)99, observe_selector, &state) == -EINVAL);
	CHECK(tcti_system_access_selector_enumerate(
		TCTI_SYSTEM_ACCESS_SELECTOR_MRS, NULL, &state) == -EINVAL);
	return 0;
}

int main(void)
{
	CHECK(!values_value_representation());
	CHECK(!equation_and_group_normalization());
	CHECK(!mrs_msr_packing());
	CHECK(!sys_sysl_packing());
	CHECK(!selector_enumeration());
	puts("target system access selector tests: passed");
	return 0;
}
