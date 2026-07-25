/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * SVE state substrate tests only. These tests are not ISA proof and must not
 * back source-leaf proof or HWCAP promotion until the official pinned ASL,
 * production task lifecycle, and tcti_resume_user evidence are complete.
 */
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/unaligned.h>

#include <asm/ptrace.h>

#include "../sve_state.h"

static void tcti_sve_set_predicate_lane(struct tcti_sve_state *state, u8 pg,
					u16 lane, u8 element_bytes, bool active)
{
	u16 bit = lane * element_bytes;
	u8 *byte = &state->p[pg][bit / 8];

	if (active)
		*byte |= BIT(bit % 8);
	else
		*byte &= ~BIT(bit % 8);
}

static void tcti_sve_store_u32(u8 *vector, u16 offset, u32 value)
{
	vector[offset] = value;
	vector[offset + 1] = value >> 8;
	vector[offset + 2] = value >> 16;
	vector[offset + 3] = value >> 24;
}

static u32 tcti_sve_load_u32(const u8 *vector, u16 offset)
{
	return (u32)vector[offset] | ((u32)vector[offset + 1] << 8) |
	       ((u32)vector[offset + 2] << 16) |
	       ((u32)vector[offset + 3] << 24);
}

static void tcti_sve_sync_z_low_to_v(const struct tcti_sve_state *state,
				     unsigned long *user_simd, u8 reg)
{
	user_simd[reg * 2] = get_unaligned_le64(&state->z[reg][0]);
	user_simd[reg * 2 + 1] = get_unaligned_le64(&state->z[reg][8]);
}

static void tcti_sve_state_rejects_invalid_vector_lengths(struct kunit *test)
{
	static const u16 invalid[] = { 0, 15, 17, 255, 257 };
	struct tcti_sve_state state;
	unsigned long user_simd[64];
	size_t index;

	for (index = 0; index < ARRAY_SIZE(invalid); index++)
		KUNIT_EXPECT_EQ(test, -EINVAL,
			tcti_sve_state_reset(&state, user_simd, invalid[index]));
	for (index = TCTI_SVE_MIN_VL_BYTES;
	     index <= TCTI_SVE_MAX_VL_BYTES;
	     index += TCTI_SVE_MIN_VL_BYTES)
		KUNIT_EXPECT_EQ(test, 0,
			tcti_sve_state_reset(&state, user_simd, index));
}

static void tcti_sve_integer_binary_preserves_predicated_lanes(
	struct kunit *test)
{
	struct tcti_sve_state state;
	unsigned long user_simd[64];
	u16 lane;

	KUNIT_ASSERT_EQ(test, 0, tcti_sve_state_reset(&state, user_simd, 32));
	for (lane = 0; lane < 8; lane++) {
		tcti_sve_store_u32(state.z[1], lane * 4, lane + 10);
		tcti_sve_store_u32(state.z[2], lane * 4, lane + 100);
		tcti_sve_store_u32(state.z[0], lane * 4, 0xdecafbadU);
		tcti_sve_set_predicate_lane(&state, 0, lane, 4, lane & 1);
	}
	tcti_sve_sync_z_low_to_v(&state, user_simd, 0);
	tcti_sve_sync_z_low_to_v(&state, user_simd, 1);
	tcti_sve_sync_z_low_to_v(&state, user_simd, 2);

	KUNIT_ASSERT_EQ(test, 0, tcti_sve_predicated_integer_binary(&state,
		user_simd,
		TCTI_SVE_INTEGER_ADD, TCTI_SVE_PREDICATE_MERGING,
		0, 0, 1, 2, 4));
	for (lane = 0; lane < 8; lane++)
		KUNIT_EXPECT_EQ(test, lane & 1 ? lane + 110 : 0xdecafbadU,
			tcti_sve_load_u32(state.z[0], lane * 4));
}

static void tcti_sve_integer_binary_zeroes_inactive_lanes(struct kunit *test)
{
	struct tcti_sve_state state;
	unsigned long user_simd[64];
	u16 lane;

	KUNIT_ASSERT_EQ(test, 0, tcti_sve_state_reset(&state, user_simd, 16));
	for (lane = 0; lane < 4; lane++) {
		tcti_sve_store_u32(state.z[1], lane * 4, 0xf0f00000U + lane);
		tcti_sve_store_u32(state.z[2], lane * 4, 0x0f0f0000U + lane);
		tcti_sve_store_u32(state.z[0], lane * 4, U32_MAX);
		tcti_sve_set_predicate_lane(&state, 3, lane, 4, lane != 2);
	}
	tcti_sve_sync_z_low_to_v(&state, user_simd, 0);
	tcti_sve_sync_z_low_to_v(&state, user_simd, 1);
	tcti_sve_sync_z_low_to_v(&state, user_simd, 2);

	KUNIT_ASSERT_EQ(test, 0, tcti_sve_predicated_integer_binary(&state,
		user_simd,
		TCTI_SVE_INTEGER_EOR, TCTI_SVE_PREDICATE_ZEROING,
		0, 3, 1, 2, 4));
	for (lane = 0; lane < 4; lane++)
		KUNIT_EXPECT_EQ(test, lane == 2 ? 0U :
			(0xf0f00000U + lane) ^ (0x0f0f0000U + lane),
			tcti_sve_load_u32(state.z[0], lane * 4));
}

static void tcti_sve_integer_binary_covers_all_integer_operations(
	struct kunit *test)
{
	static const enum tcti_sve_integer_binary_op operations[] = {
		TCTI_SVE_INTEGER_ADD, TCTI_SVE_INTEGER_SUB,
		TCTI_SVE_INTEGER_AND, TCTI_SVE_INTEGER_ORR,
		TCTI_SVE_INTEGER_EOR,
	};
	struct tcti_sve_state state;
	unsigned long user_simd[64];
	size_t index;

	for (index = 0; index < ARRAY_SIZE(operations); index++) {
		KUNIT_ASSERT_EQ(test, 0,
			tcti_sve_state_reset(&state, user_simd, 16));
		state.z[1][0] = 0x66;
		state.z[2][0] = 0x3c;
		tcti_sve_set_predicate_lane(&state, 0, 0, 1, true);
		tcti_sve_sync_z_low_to_v(&state, user_simd, 1);
		tcti_sve_sync_z_low_to_v(&state, user_simd, 2);
		KUNIT_ASSERT_EQ(test, 0, tcti_sve_predicated_integer_binary(&state,
			user_simd,
			operations[index], TCTI_SVE_PREDICATE_MERGING,
			0, 0, 1, 2, 1));
		switch (operations[index]) {
		case TCTI_SVE_INTEGER_ADD:
			KUNIT_EXPECT_EQ(test, 0xa2, state.z[0][0]);
			break;
		case TCTI_SVE_INTEGER_SUB:
			KUNIT_EXPECT_EQ(test, 0x2a, state.z[0][0]);
			break;
		case TCTI_SVE_INTEGER_AND:
			KUNIT_EXPECT_EQ(test, 0x24, state.z[0][0]);
			break;
		case TCTI_SVE_INTEGER_ORR:
			KUNIT_EXPECT_EQ(test, 0x7e, state.z[0][0]);
			break;
		case TCTI_SVE_INTEGER_EOR:
			KUNIT_EXPECT_EQ(test, 0x5a, state.z[0][0]);
			break;
		}
	}
}

static void tcti_sve_integer_binary_honors_destination_source_aliasing(
	struct kunit *test)
{
	struct tcti_sve_state state;
	unsigned long user_simd[64];
	u16 lane;

	KUNIT_ASSERT_EQ(test, 0, tcti_sve_state_reset(&state, user_simd, 16));
	for (lane = 0; lane < 16; lane++) {
		state.z[7][lane] = lane;
		state.z[8][lane] = 0x80U + lane;
		tcti_sve_set_predicate_lane(&state, 1, lane, 1, true);
	}
	tcti_sve_sync_z_low_to_v(&state, user_simd, 7);
	tcti_sve_sync_z_low_to_v(&state, user_simd, 8);
	KUNIT_ASSERT_EQ(test, 0, tcti_sve_predicated_integer_binary(&state,
		user_simd,
		TCTI_SVE_INTEGER_ADD, TCTI_SVE_PREDICATE_MERGING,
		7, 1, 7, 8, 1));
	for (lane = 0; lane < 16; lane++)
		KUNIT_EXPECT_EQ(test, (u8)(0x80U + 2 * lane), state.z[7][lane]);
}

static void tcti_sve_execution_updates_advsimd_v_alias(struct kunit *test)
{
	struct tcti_sve_state state;
	unsigned long user_simd[64];
	struct pt_regs regs = { .pc = 0x2800 };

	KUNIT_ASSERT_EQ(test, 0, tcti_sve_state_reset(&state, user_simd, 16));
	state.z[1][0] = 3;
	state.z[2][0] = 9;
	tcti_sve_set_predicate_lane(&state, 0, 0, 1, true);
	tcti_sve_sync_z_low_to_v(&state, user_simd, 1);
	tcti_sve_sync_z_low_to_v(&state, user_simd, 2);
	KUNIT_ASSERT_EQ(test, 0,
		tcti_sve_execute_predicated_integer_binary(&state, &regs,
			user_simd, true, TCTI_SVE_INTEGER_ADD,
			TCTI_SVE_PREDICATE_MERGING, 0, 0, 1, 2, 1));
	KUNIT_EXPECT_EQ(test, 12, state.z[0][0]);
	KUNIT_EXPECT_EQ(test, 12UL, user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0UL, user_simd[1]);
}

static void tcti_sve_execution_reads_advsimd_v_alias(struct kunit *test)
{
	struct tcti_sve_state state;
	unsigned long user_simd[64];
	struct pt_regs regs = { .pc = 0x2c00 };

	KUNIT_ASSERT_EQ(test, 0, tcti_sve_state_reset(&state, user_simd, 16));
	state.z[1][0] = 0xfe;
	state.z[2][0] = 0xff;
	user_simd[1 * 2] = 5;
	user_simd[2 * 2] = 7;
	tcti_sve_set_predicate_lane(&state, 0, 0, 1, true);
	KUNIT_ASSERT_EQ(test, 0,
		tcti_sve_execute_predicated_integer_binary(&state, &regs,
			user_simd, true, TCTI_SVE_INTEGER_ADD,
			TCTI_SVE_PREDICATE_MERGING, 0, 0, 1, 2, 1));
	KUNIT_EXPECT_EQ(test, 12, state.z[0][0]);
	KUNIT_EXPECT_EQ(test, 12UL, user_simd[0]);
}

static void tcti_sve_invalid_operation_preserves_state_and_pc(
	struct kunit *test)
{
	struct tcti_sve_state state;
	struct tcti_sve_state before;
	unsigned long user_simd[64];
	struct pt_regs regs = { .pc = 0x1000, .pstate = 0xa0000000UL };

	KUNIT_ASSERT_EQ(test, 0, tcti_sve_state_reset(&state, user_simd, 16));
	memset(state.z, 0x5a, sizeof(state.z));
	before = state;
	KUNIT_EXPECT_EQ(test, -EINVAL,
		tcti_sve_execute_predicated_integer_binary(&state, &regs,
			user_simd,
			true,
			(enum tcti_sve_integer_binary_op)99,
			TCTI_SVE_PREDICATE_MERGING, 0, 0, 1, 2, 4));
	KUNIT_EXPECT_EQ(test, 0x1000UL, regs.pc);
	KUNIT_EXPECT_EQ(test, 0xa0000000UL, regs.pstate);
	KUNIT_EXPECT_EQ(test, 0, memcmp(&before, &state, sizeof(state)));
	KUNIT_EXPECT_EQ(test, -EINVAL,
		tcti_sve_execute_predicated_integer_binary(&state, &regs,
			user_simd,
			true,
			TCTI_SVE_INTEGER_ADD, TCTI_SVE_PREDICATE_MERGING,
			32, 0, 1, 2, 4));
	KUNIT_EXPECT_EQ(test, 0x1000UL, regs.pc);
	KUNIT_EXPECT_EQ(test, 0xa0000000UL, regs.pstate);
	KUNIT_EXPECT_EQ(test, 0, memcmp(&before, &state, sizeof(state)));
}

static void tcti_sve_unavailable_extension_preserves_state_and_pc(
	struct kunit *test)
{
	struct tcti_sve_state state;
	struct tcti_sve_state before;
	unsigned long user_simd[64];
	struct pt_regs regs = { .pc = 0x1800, .pstate = 0x60000000UL };

	KUNIT_ASSERT_EQ(test, 0, tcti_sve_state_reset(&state, user_simd, 16));
	before = state;
	KUNIT_EXPECT_EQ(test, -EOPNOTSUPP,
		tcti_sve_execute_predicated_integer_binary(&state, &regs,
			user_simd,
			false,
			TCTI_SVE_INTEGER_ADD, TCTI_SVE_PREDICATE_MERGING,
			0, 0, 1, 2, 1));
	KUNIT_EXPECT_EQ(test, 0x1800UL, regs.pc);
	KUNIT_EXPECT_EQ(test, 0x60000000UL, regs.pstate);
	KUNIT_EXPECT_EQ(test, 0, memcmp(&before, &state, sizeof(state)));
}

static void tcti_sve_valid_operation_advances_pc_once(struct kunit *test)
{
	struct tcti_sve_state state;
	unsigned long user_simd[64];
	struct pt_regs regs = { .pc = 0x2000, .pstate = 0x90000000UL };

	KUNIT_ASSERT_EQ(test, 0, tcti_sve_state_reset(&state, user_simd, 16));
	state.z[1][0] = 4;
	state.z[2][0] = 5;
	tcti_sve_set_predicate_lane(&state, 0, 0, 1, true);
	tcti_sve_sync_z_low_to_v(&state, user_simd, 1);
	tcti_sve_sync_z_low_to_v(&state, user_simd, 2);
	KUNIT_EXPECT_EQ(test, 0,
		tcti_sve_execute_predicated_integer_binary(&state, &regs,
			user_simd,
			true,
			TCTI_SVE_INTEGER_ADD, TCTI_SVE_PREDICATE_MERGING,
			0, 0, 1, 2, 1));
	KUNIT_EXPECT_EQ(test, 9, state.z[0][0]);
	KUNIT_EXPECT_EQ(test, 0x2004UL, regs.pc);
	KUNIT_EXPECT_EQ(test, 0x90000000UL, regs.pstate);
}

static struct kunit_case tcti_sve_state_test_cases[] = {
	KUNIT_CASE(tcti_sve_state_rejects_invalid_vector_lengths),
	KUNIT_CASE(tcti_sve_integer_binary_preserves_predicated_lanes),
	KUNIT_CASE(tcti_sve_integer_binary_zeroes_inactive_lanes),
	KUNIT_CASE(tcti_sve_integer_binary_covers_all_integer_operations),
	KUNIT_CASE(tcti_sve_integer_binary_honors_destination_source_aliasing),
	KUNIT_CASE(tcti_sve_execution_updates_advsimd_v_alias),
	KUNIT_CASE(tcti_sve_execution_reads_advsimd_v_alias),
	KUNIT_CASE(tcti_sve_invalid_operation_preserves_state_and_pc),
	KUNIT_CASE(tcti_sve_unavailable_extension_preserves_state_and_pc),
	KUNIT_CASE(tcti_sve_valid_operation_advances_pc_once),
	{}
};

struct kunit_suite tcti_sve_state_test_suite = {
	.name = "orlix-tcti-sve-state",
	.test_cases = tcti_sve_state_test_cases,
};

kunit_test_suite(tcti_sve_state_test_suite);

MODULE_LICENSE("GPL");
