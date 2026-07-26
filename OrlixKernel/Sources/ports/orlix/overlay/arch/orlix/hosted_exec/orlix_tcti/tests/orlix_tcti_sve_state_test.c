/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * SVE state substrate tests only. These tests are not ISA proof and must not
 * back source-leaf proof or HWCAP promotion until the official pinned ASL,
 * production task lifecycle, and orlix_tcti_resume_user evidence are complete.
 */
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/unaligned.h>

#include <asm/ptrace.h>
#include <asm/processor.h>

#include "../sve_state.h"

static void orlix_tcti_sve_set_predicate_lane(struct orlix_tcti_sve_state *state, u8 pg,
					u16 lane, u8 element_bytes, bool active)
{
	u16 bit = lane * element_bytes;
	u8 *byte = &state->p[pg][bit / 8];

	if (active)
		*byte |= BIT(bit % 8);
	else
		*byte &= ~BIT(bit % 8);
}

static void orlix_tcti_sve_store_u32(u8 *vector, u16 offset, u32 value)
{
	vector[offset] = value;
	vector[offset + 1] = value >> 8;
	vector[offset + 2] = value >> 16;
	vector[offset + 3] = value >> 24;
}

static u32 orlix_tcti_sve_load_u32(const u8 *vector, u16 offset)
{
	return (u32)vector[offset] | ((u32)vector[offset + 1] << 8) |
	       ((u32)vector[offset + 2] << 16) |
	       ((u32)vector[offset + 3] << 24);
}

static void orlix_tcti_sve_store_element(u8 *vector, u16 offset,
				   u8 element_bytes, u64 value)
{
	u8 byte;

	for (byte = 0; byte < element_bytes; byte++)
		vector[offset + byte] = value >> (byte * BITS_PER_BYTE);
}

static u64 orlix_tcti_sve_load_element(const u8 *vector, u16 offset,
				  u8 element_bytes)
{
	u64 value = 0;
	u8 byte;

	for (byte = 0; byte < element_bytes; byte++)
		value |= (u64)vector[offset + byte] << (byte * BITS_PER_BYTE);
	return value;
}

static void orlix_tcti_sve_sync_z_low_to_v(const struct orlix_tcti_sve_state *state,
				     unsigned long *user_simd, u8 reg)
{
	user_simd[reg * 2] = get_unaligned_le64(&state->z[reg][0]);
	user_simd[reg * 2 + 1] = get_unaligned_le64(&state->z[reg][8]);
}

static void orlix_tcti_sve_state_rejects_invalid_vector_lengths(struct kunit *test)
{
	static const u16 invalid[] = { 0, 15, 17, 255, 257 };
	struct orlix_tcti_sve_state state;
	unsigned long user_simd[64];
	size_t index;

	for (index = 0; index < ARRAY_SIZE(invalid); index++)
		KUNIT_EXPECT_EQ(test, -EINVAL,
			orlix_tcti_sve_state_reset(&state, user_simd, invalid[index]));
	for (index = ORLIX_TCTI_SVE_MIN_VL_BYTES;
	     index <= ORLIX_TCTI_SVE_MAX_VL_BYTES;
	     index += ORLIX_TCTI_SVE_MIN_VL_BYTES)
		KUNIT_EXPECT_EQ(test, 0,
			orlix_tcti_sve_state_reset(&state, user_simd, index));
}

static void orlix_tcti_sve_state_reset_zeroes_full_scalable_register_file(
	struct kunit *test)
{
	struct orlix_tcti_sve_state state;
	unsigned long user_simd[64];

	memset(&state, 0x5a, sizeof(state));
	memset(user_simd, 0xa5, sizeof(user_simd));

	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_sve_state_reset(&state, user_simd, ORLIX_TCTI_SVE_MAX_VL_BYTES));
	KUNIT_EXPECT_TRUE(test, state.valid);
	KUNIT_EXPECT_EQ(test, (u16)ORLIX_TCTI_SVE_MAX_VL_BYTES, state.vl_bytes);
	KUNIT_EXPECT_PTR_EQ(test, NULL,
		memchr_inv(state.z, 0, sizeof(state.z)));
	KUNIT_EXPECT_PTR_EQ(test, NULL,
		memchr_inv(state.p, 0, sizeof(state.p)));
	KUNIT_EXPECT_PTR_EQ(test, NULL,
		memchr_inv(state.ffr, 0, sizeof(state.ffr)));
	KUNIT_EXPECT_PTR_EQ(test, NULL,
		memchr_inv(user_simd, 0, sizeof(user_simd)));
}

static void orlix_tcti_sve_state_copy_preserves_full_scalable_context(
	struct kunit *test)
{
	struct orlix_tcti_sve_state source;
	struct orlix_tcti_sve_state destination;
	unsigned long source_simd[64];
	unsigned long destination_simd[64];

	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(&source, source_simd,
		ORLIX_TCTI_SVE_MAX_VL_BYTES));
	source.z[31][ORLIX_TCTI_SVE_MAX_VL_BYTES - 1] = 0x11;
	source.p[15][ORLIX_TCTI_SVE_PREG_MAX_BYTES - 1] = 0x22;
	source.ffr[ORLIX_TCTI_SVE_PREG_MAX_BYTES - 1] = 0x44;
	source_simd[63] = 0x8877665544332211UL;
	memset(&destination, 0xa5, sizeof(destination));
	memset(destination_simd, 0x5a, sizeof(destination_simd));

	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_copy(&destination,
		destination_simd, &source, source_simd));
	KUNIT_EXPECT_MEMEQ(test, &source, &destination, sizeof(source));
	KUNIT_EXPECT_MEMEQ(test, source_simd, destination_simd,
			  sizeof(source_simd));
}

static void orlix_tcti_sve_state_copy_rejects_invalid_source_without_writing(
	struct kunit *test)
{
	struct orlix_tcti_sve_state source;
	struct orlix_tcti_sve_state destination;
	struct orlix_tcti_sve_state destination_before;
	unsigned long source_simd[64];
	unsigned long destination_simd[64];
	unsigned long destination_simd_before[64];

	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(&source, source_simd,
		ORLIX_TCTI_SVE_DEFAULT_VL_BYTES));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(&destination,
		destination_simd, ORLIX_TCTI_SVE_DEFAULT_VL_BYTES));
	memset(&destination, 0xa5, sizeof(destination));
	memset(destination_simd, 0x5a, sizeof(destination_simd));
	destination_before = destination;
	memcpy(destination_simd_before, destination_simd,
	       sizeof(destination_simd_before));
	source.valid = false;

	KUNIT_EXPECT_EQ(test, -EINVAL, orlix_tcti_sve_state_copy(&destination,
		destination_simd, &source, source_simd));
	KUNIT_EXPECT_MEMEQ(test, &destination_before, &destination,
			  sizeof(destination));
	KUNIT_EXPECT_MEMEQ(test, destination_simd_before, destination_simd,
			  sizeof(destination_simd));
}

static void orlix_tcti_start_thread_resets_full_scalable_sve_context(
	struct kunit *test)
{
	struct pt_regs regs = {};

	memset(&current->thread.user_sve, 0x5a,
	       sizeof(current->thread.user_sve));
	memset(current->thread.user_simd, 0xa5,
	       sizeof(current->thread.user_simd));

	start_thread(&regs, 0x1000, 0x2000);

	KUNIT_EXPECT_TRUE(test, current->thread.user_sve.valid);
	KUNIT_EXPECT_EQ(test, (u16)ORLIX_TCTI_SVE_DEFAULT_VL_BYTES,
			current->thread.user_sve.vl_bytes);
	KUNIT_EXPECT_PTR_EQ(test, NULL,
		memchr_inv(current->thread.user_sve.z, 0,
			   sizeof(current->thread.user_sve.z)));
	KUNIT_EXPECT_PTR_EQ(test, NULL,
		memchr_inv(current->thread.user_sve.p, 0,
			   sizeof(current->thread.user_sve.p)));
	KUNIT_EXPECT_PTR_EQ(test, NULL,
		memchr_inv(current->thread.user_sve.ffr, 0,
			   sizeof(current->thread.user_sve.ffr)));
	KUNIT_EXPECT_PTR_EQ(test, NULL,
		memchr_inv(current->thread.user_simd, 0,
			   sizeof(current->thread.user_simd)));
}

static void orlix_tcti_sve_integer_binary_preserves_predicated_lanes(
	struct kunit *test)
{
	struct orlix_tcti_sve_state state;
	unsigned long user_simd[64];
	u16 lane;

	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(&state, user_simd,
		ORLIX_TCTI_SVE_MAX_VL_BYTES));
	for (lane = 0; lane < ORLIX_TCTI_SVE_MAX_VL_BYTES / sizeof(u32); lane++) {
		orlix_tcti_sve_store_u32(state.z[1], lane * 4, lane + 10);
		orlix_tcti_sve_store_u32(state.z[2], lane * 4, lane + 100);
		orlix_tcti_sve_store_u32(state.z[0], lane * 4, 0xdecafbadU);
		orlix_tcti_sve_set_predicate_lane(&state, 0, lane, 4, lane & 1);
	}
	orlix_tcti_sve_sync_z_low_to_v(&state, user_simd, 0);
	orlix_tcti_sve_sync_z_low_to_v(&state, user_simd, 1);
	orlix_tcti_sve_sync_z_low_to_v(&state, user_simd, 2);

	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_predicated_integer_binary(&state,
		user_simd,
		ORLIX_TCTI_SVE_INTEGER_ADD, ORLIX_TCTI_SVE_PREDICATE_MERGING,
		0, 0, 1, 2, 4));
	for (lane = 0; lane < ORLIX_TCTI_SVE_MAX_VL_BYTES / sizeof(u32); lane++)
		KUNIT_EXPECT_EQ(test, lane & 1 ? lane + 110 : 0xdecafbadU,
			orlix_tcti_sve_load_u32(state.z[0], lane * 4));
}

static void orlix_tcti_sve_integer_binary_zeroes_inactive_lanes(struct kunit *test)
{
	struct orlix_tcti_sve_state state;
	unsigned long user_simd[64];
	u16 lane;

	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(&state, user_simd, 16));
	for (lane = 0; lane < 4; lane++) {
		orlix_tcti_sve_store_u32(state.z[1], lane * 4, 0xf0f00000U + lane);
		orlix_tcti_sve_store_u32(state.z[2], lane * 4, 0x0f0f0000U + lane);
		orlix_tcti_sve_store_u32(state.z[0], lane * 4, U32_MAX);
		orlix_tcti_sve_set_predicate_lane(&state, 3, lane, 4, lane != 2);
	}
	orlix_tcti_sve_sync_z_low_to_v(&state, user_simd, 0);
	orlix_tcti_sve_sync_z_low_to_v(&state, user_simd, 1);
	orlix_tcti_sve_sync_z_low_to_v(&state, user_simd, 2);

	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_predicated_integer_binary(&state,
		user_simd,
		ORLIX_TCTI_SVE_INTEGER_EOR, ORLIX_TCTI_SVE_PREDICATE_ZEROING,
		0, 3, 1, 2, 4));
	for (lane = 0; lane < 4; lane++)
		KUNIT_EXPECT_EQ(test, lane == 2 ? 0U :
			(0xf0f00000U + lane) ^ (0x0f0f0000U + lane),
			orlix_tcti_sve_load_u32(state.z[0], lane * 4));
}

static void orlix_tcti_sve_integer_binary_covers_all_integer_operations(
	struct kunit *test)
{
	static const enum orlix_tcti_sve_integer_binary_op operations[] = {
		ORLIX_TCTI_SVE_INTEGER_ADD, ORLIX_TCTI_SVE_INTEGER_SUB,
		ORLIX_TCTI_SVE_INTEGER_SUBR, ORLIX_TCTI_SVE_INTEGER_SMAX,
		ORLIX_TCTI_SVE_INTEGER_SMIN, ORLIX_TCTI_SVE_INTEGER_SABD,
		ORLIX_TCTI_SVE_INTEGER_UMAX, ORLIX_TCTI_SVE_INTEGER_UMIN,
		ORLIX_TCTI_SVE_INTEGER_UABD, ORLIX_TCTI_SVE_INTEGER_MUL,
		ORLIX_TCTI_SVE_INTEGER_SMULH, ORLIX_TCTI_SVE_INTEGER_UMULH,
		ORLIX_TCTI_SVE_INTEGER_AND, ORLIX_TCTI_SVE_INTEGER_ORR,
		ORLIX_TCTI_SVE_INTEGER_EOR, ORLIX_TCTI_SVE_INTEGER_BIC,
	};
	struct orlix_tcti_sve_state state;
	unsigned long user_simd[64];
	size_t index;

	for (index = 0; index < ARRAY_SIZE(operations); index++) {
		KUNIT_ASSERT_EQ(test, 0,
			orlix_tcti_sve_state_reset(&state, user_simd, 16));
		state.z[1][0] = 0x66;
		state.z[2][0] = 0x3c;
		orlix_tcti_sve_set_predicate_lane(&state, 0, 0, 1, true);
		orlix_tcti_sve_sync_z_low_to_v(&state, user_simd, 1);
		orlix_tcti_sve_sync_z_low_to_v(&state, user_simd, 2);
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_predicated_integer_binary(&state,
			user_simd,
			operations[index], ORLIX_TCTI_SVE_PREDICATE_MERGING,
			0, 0, 1, 2, 1));
		switch (operations[index]) {
		case ORLIX_TCTI_SVE_INTEGER_ADD:
			KUNIT_EXPECT_EQ(test, 0xa2, state.z[0][0]);
			break;
		case ORLIX_TCTI_SVE_INTEGER_SUB:
			KUNIT_EXPECT_EQ(test, 0x2a, state.z[0][0]);
			break;
		case ORLIX_TCTI_SVE_INTEGER_SUBR:
			KUNIT_EXPECT_EQ(test, 0xd6, state.z[0][0]);
			break;
		case ORLIX_TCTI_SVE_INTEGER_SMAX:
		case ORLIX_TCTI_SVE_INTEGER_UMAX:
			KUNIT_EXPECT_EQ(test, 0x66, state.z[0][0]);
			break;
		case ORLIX_TCTI_SVE_INTEGER_SMIN:
		case ORLIX_TCTI_SVE_INTEGER_UMIN:
			KUNIT_EXPECT_EQ(test, 0x3c, state.z[0][0]);
			break;
		case ORLIX_TCTI_SVE_INTEGER_SABD:
		case ORLIX_TCTI_SVE_INTEGER_UABD:
			KUNIT_EXPECT_EQ(test, 0x2a, state.z[0][0]);
			break;
		case ORLIX_TCTI_SVE_INTEGER_MUL:
			KUNIT_EXPECT_EQ(test, 0xe8, state.z[0][0]);
			break;
		case ORLIX_TCTI_SVE_INTEGER_SMULH:
		case ORLIX_TCTI_SVE_INTEGER_UMULH:
			KUNIT_EXPECT_EQ(test, 0x17, state.z[0][0]);
			break;
		case ORLIX_TCTI_SVE_INTEGER_SDIV:
		case ORLIX_TCTI_SVE_INTEGER_UDIV:
			KUNIT_EXPECT_EQ(test, 1, state.z[0][0]);
			break;
		case ORLIX_TCTI_SVE_INTEGER_SDIVR:
		case ORLIX_TCTI_SVE_INTEGER_UDIVR:
			KUNIT_EXPECT_EQ(test, 0, state.z[0][0]);
			break;
		case ORLIX_TCTI_SVE_INTEGER_AND:
			KUNIT_EXPECT_EQ(test, 0x24, state.z[0][0]);
			break;
		case ORLIX_TCTI_SVE_INTEGER_ORR:
			KUNIT_EXPECT_EQ(test, 0x7e, state.z[0][0]);
			break;
		case ORLIX_TCTI_SVE_INTEGER_EOR:
			KUNIT_EXPECT_EQ(test, 0x5a, state.z[0][0]);
			break;
		case ORLIX_TCTI_SVE_INTEGER_BIC:
			KUNIT_EXPECT_EQ(test, 0x42, state.z[0][0]);
			break;
		}
	}
}

static void orlix_tcti_sve_integer_binary_proves_signed_and_high_edges(
	struct kunit *test)
{
	static const u8 element_bytes[] = { 1, 2, 4, 8 };
	struct orlix_tcti_sve_state state;
	unsigned long user_simd[64];
	size_t index;

	for (index = 0; index < ARRAY_SIZE(element_bytes); index++) {
		u8 bytes = element_bytes[index];
		u8 bits = bytes * BITS_PER_BYTE;
		u64 mask = bytes == sizeof(u64) ? U64_MAX :
			GENMASK_ULL(bits - 1, 0);
		u64 minimum = BIT_ULL(bits - 1);

		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(&state, user_simd,
			ORLIX_TCTI_SVE_MAX_VL_BYTES));
		orlix_tcti_sve_store_element(state.z[1], 0, bytes, minimum);
		orlix_tcti_sve_store_element(state.z[2], 0, bytes, mask);
		orlix_tcti_sve_set_predicate_lane(&state, 0, 0, bytes, true);
		orlix_tcti_sve_sync_z_low_to_v(&state, user_simd, 1);
		orlix_tcti_sve_sync_z_low_to_v(&state, user_simd, 2);
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_predicated_integer_binary(&state,
			user_simd, ORLIX_TCTI_SVE_INTEGER_SMAX,
			ORLIX_TCTI_SVE_PREDICATE_MERGING, 0, 0, 1, 2, bytes));
		KUNIT_EXPECT_EQ(test, mask, orlix_tcti_sve_load_element(state.z[0], 0,
			bytes));

		orlix_tcti_sve_store_element(state.z[1], 0, bytes, mask - 1);
		orlix_tcti_sve_store_element(state.z[2], 0, bytes, 3);
		orlix_tcti_sve_sync_z_low_to_v(&state, user_simd, 1);
		orlix_tcti_sve_sync_z_low_to_v(&state, user_simd, 2);
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_predicated_integer_binary(&state,
			user_simd, ORLIX_TCTI_SVE_INTEGER_SMULH,
			ORLIX_TCTI_SVE_PREDICATE_MERGING, 0, 0, 1, 2, bytes));
		KUNIT_EXPECT_EQ(test, mask, orlix_tcti_sve_load_element(state.z[0], 0,
			bytes));
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_predicated_integer_binary(&state,
			user_simd, ORLIX_TCTI_SVE_INTEGER_UMULH,
			ORLIX_TCTI_SVE_PREDICATE_MERGING, 0, 0, 1, 2, bytes));
		KUNIT_EXPECT_EQ(test, 2, orlix_tcti_sve_load_element(state.z[0], 0,
			bytes));

		if (bytes < 4)
			continue;
		orlix_tcti_sve_store_element(state.z[1], 0, bytes, minimum);
		orlix_tcti_sve_store_element(state.z[2], 0, bytes, mask);
		orlix_tcti_sve_sync_z_low_to_v(&state, user_simd, 1);
		orlix_tcti_sve_sync_z_low_to_v(&state, user_simd, 2);
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_predicated_integer_binary(&state,
			user_simd, ORLIX_TCTI_SVE_INTEGER_SDIV,
			ORLIX_TCTI_SVE_PREDICATE_MERGING, 0, 0, 1, 2, bytes));
		KUNIT_EXPECT_EQ(test, minimum, orlix_tcti_sve_load_element(state.z[0], 0,
			bytes));
		orlix_tcti_sve_store_element(state.z[2], 0, bytes, 0);
		orlix_tcti_sve_sync_z_low_to_v(&state, user_simd, 2);
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_predicated_integer_binary(&state,
			user_simd, ORLIX_TCTI_SVE_INTEGER_SDIV,
			ORLIX_TCTI_SVE_PREDICATE_MERGING, 0, 0, 1, 2, bytes));
		KUNIT_EXPECT_EQ(test, 0, orlix_tcti_sve_load_element(state.z[0], 0,
			bytes));

		orlix_tcti_sve_store_element(state.z[1], 0, bytes, mask);
		orlix_tcti_sve_store_element(state.z[2], 0, bytes, minimum);
		orlix_tcti_sve_sync_z_low_to_v(&state, user_simd, 1);
		orlix_tcti_sve_sync_z_low_to_v(&state, user_simd, 2);
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_predicated_integer_binary(&state,
			user_simd, ORLIX_TCTI_SVE_INTEGER_SDIVR,
			ORLIX_TCTI_SVE_PREDICATE_MERGING, 0, 0, 1, 2, bytes));
		KUNIT_EXPECT_EQ(test, minimum, orlix_tcti_sve_load_element(state.z[0], 0,
			bytes));
		orlix_tcti_sve_store_element(state.z[1], 0, bytes, 0);
		orlix_tcti_sve_store_element(state.z[2], 0, bytes, 7);
		orlix_tcti_sve_sync_z_low_to_v(&state, user_simd, 1);
		orlix_tcti_sve_sync_z_low_to_v(&state, user_simd, 2);
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_predicated_integer_binary(&state,
			user_simd, ORLIX_TCTI_SVE_INTEGER_SDIVR,
			ORLIX_TCTI_SVE_PREDICATE_MERGING, 0, 0, 1, 2, bytes));
		KUNIT_EXPECT_EQ(test, 0, orlix_tcti_sve_load_element(state.z[0], 0,
			bytes));

		orlix_tcti_sve_store_element(state.z[1], 0, bytes, 7);
		orlix_tcti_sve_store_element(state.z[2], 0, bytes, 0);
		orlix_tcti_sve_sync_z_low_to_v(&state, user_simd, 1);
		orlix_tcti_sve_sync_z_low_to_v(&state, user_simd, 2);
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_predicated_integer_binary(&state,
			user_simd, ORLIX_TCTI_SVE_INTEGER_UDIV,
			ORLIX_TCTI_SVE_PREDICATE_MERGING, 0, 0, 1, 2, bytes));
		KUNIT_EXPECT_EQ(test, 0, orlix_tcti_sve_load_element(state.z[0], 0,
			bytes));
		orlix_tcti_sve_store_element(state.z[1], 0, bytes, 0);
		orlix_tcti_sve_store_element(state.z[2], 0, bytes, 7);
		orlix_tcti_sve_sync_z_low_to_v(&state, user_simd, 1);
		orlix_tcti_sve_sync_z_low_to_v(&state, user_simd, 2);
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_predicated_integer_binary(&state,
			user_simd, ORLIX_TCTI_SVE_INTEGER_UDIVR,
			ORLIX_TCTI_SVE_PREDICATE_MERGING, 0, 0, 1, 2, bytes));
		KUNIT_EXPECT_EQ(test, 0, orlix_tcti_sve_load_element(state.z[0], 0,
			bytes));
	}
}

static void orlix_tcti_sve_integer_binary_honors_destination_source_aliasing(
	struct kunit *test)
{
	struct orlix_tcti_sve_state state;
	unsigned long user_simd[64];
	u16 lane;

	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(&state, user_simd, 16));
	for (lane = 0; lane < 16; lane++) {
		state.z[7][lane] = lane;
		state.z[8][lane] = 0x80U + lane;
		orlix_tcti_sve_set_predicate_lane(&state, 1, lane, 1, true);
	}
	orlix_tcti_sve_sync_z_low_to_v(&state, user_simd, 7);
	orlix_tcti_sve_sync_z_low_to_v(&state, user_simd, 8);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_predicated_integer_binary(&state,
		user_simd,
		ORLIX_TCTI_SVE_INTEGER_ADD, ORLIX_TCTI_SVE_PREDICATE_MERGING,
		7, 1, 7, 8, 1));
	for (lane = 0; lane < 16; lane++)
		KUNIT_EXPECT_EQ(test, (u8)(0x80U + 2 * lane), state.z[7][lane]);
}

static void orlix_tcti_sve_execution_updates_advsimd_v_alias(struct kunit *test)
{
	struct orlix_tcti_sve_state state;
	unsigned long user_simd[64];
	struct pt_regs regs = { .pc = 0x2800 };

	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(&state, user_simd, 16));
	state.z[1][0] = 3;
	state.z[2][0] = 9;
	orlix_tcti_sve_set_predicate_lane(&state, 0, 0, 1, true);
	orlix_tcti_sve_sync_z_low_to_v(&state, user_simd, 1);
	orlix_tcti_sve_sync_z_low_to_v(&state, user_simd, 2);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_sve_execute_predicated_integer_binary(&state, &regs,
			user_simd, true, ORLIX_TCTI_SVE_INTEGER_ADD,
			ORLIX_TCTI_SVE_PREDICATE_MERGING, 0, 0, 1, 2, 1));
	KUNIT_EXPECT_EQ(test, 12, state.z[0][0]);
	KUNIT_EXPECT_EQ(test, 12UL, user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0UL, user_simd[1]);
}

static void orlix_tcti_sve_execution_reads_advsimd_v_alias(struct kunit *test)
{
	struct orlix_tcti_sve_state state;
	unsigned long user_simd[64];
	struct pt_regs regs = { .pc = 0x2c00 };

	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(&state, user_simd, 16));
	state.z[1][0] = 0xfe;
	state.z[2][0] = 0xff;
	user_simd[1 * 2] = 5;
	user_simd[2 * 2] = 7;
	orlix_tcti_sve_set_predicate_lane(&state, 0, 0, 1, true);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_sve_execute_predicated_integer_binary(&state, &regs,
			user_simd, true, ORLIX_TCTI_SVE_INTEGER_ADD,
			ORLIX_TCTI_SVE_PREDICATE_MERGING, 0, 0, 1, 2, 1));
	KUNIT_EXPECT_EQ(test, 12, state.z[0][0]);
	KUNIT_EXPECT_EQ(test, 12UL, user_simd[0]);
}

static void orlix_tcti_sve_invalid_operation_preserves_state_and_pc(
	struct kunit *test)
{
	struct orlix_tcti_sve_state state;
	struct orlix_tcti_sve_state before;
	unsigned long user_simd[64];
	unsigned long simd_before[64];
	struct pt_regs regs = { .pc = 0x1000, .pstate = 0xa0000000UL };
	static const enum orlix_tcti_sve_integer_binary_op division_ops[] = {
		ORLIX_TCTI_SVE_INTEGER_SDIV, ORLIX_TCTI_SVE_INTEGER_SDIVR,
		ORLIX_TCTI_SVE_INTEGER_UDIV, ORLIX_TCTI_SVE_INTEGER_UDIVR,
	};
	static const u8 illegal_sizes[] = { 1, 2 };
	size_t operation_index;
	size_t size_index;

	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(&state, user_simd, 16));
	memset(state.z, 0x5a, sizeof(state.z));
	before = state;
	KUNIT_EXPECT_EQ(test, -EINVAL,
		orlix_tcti_sve_execute_predicated_integer_binary(&state, &regs,
			user_simd,
			true,
			(enum orlix_tcti_sve_integer_binary_op)99,
			ORLIX_TCTI_SVE_PREDICATE_MERGING, 0, 0, 1, 2, 4));
	KUNIT_EXPECT_EQ(test, 0x1000UL, regs.pc);
	KUNIT_EXPECT_EQ(test, 0xa0000000UL, regs.pstate);
	KUNIT_EXPECT_EQ(test, 0, memcmp(&before, &state, sizeof(state)));
	KUNIT_EXPECT_EQ(test, -EINVAL,
		orlix_tcti_sve_execute_predicated_integer_binary(&state, &regs,
			user_simd,
			true,
			ORLIX_TCTI_SVE_INTEGER_ADD, ORLIX_TCTI_SVE_PREDICATE_MERGING,
			32, 0, 1, 2, 4));
	KUNIT_EXPECT_EQ(test, 0x1000UL, regs.pc);
	KUNIT_EXPECT_EQ(test, 0xa0000000UL, regs.pstate);
	KUNIT_EXPECT_EQ(test, 0, memcmp(&before, &state, sizeof(state)));
	memcpy(simd_before, user_simd, sizeof(simd_before));
	for (operation_index = 0;
	     operation_index < ARRAY_SIZE(division_ops); operation_index++) {
		for (size_index = 0; size_index < ARRAY_SIZE(illegal_sizes);
		     size_index++) {
			KUNIT_EXPECT_EQ(test, -EINVAL,
				orlix_tcti_sve_execute_predicated_integer_binary(&state, &regs,
					user_simd, true, division_ops[operation_index],
					ORLIX_TCTI_SVE_PREDICATE_MERGING, 0, 0, 1, 2,
					illegal_sizes[size_index]));
			KUNIT_EXPECT_EQ(test, 0x1000UL, regs.pc);
			KUNIT_EXPECT_EQ(test, 0xa0000000UL, regs.pstate);
			KUNIT_EXPECT_MEMEQ(test, &before, &state, sizeof(state));
			KUNIT_EXPECT_MEMEQ(test, simd_before, user_simd,
					  sizeof(simd_before));
		}
	}
}

static void orlix_tcti_sve_unavailable_extension_preserves_state_and_pc(
	struct kunit *test)
{
	struct orlix_tcti_sve_state state;
	struct orlix_tcti_sve_state before;
	unsigned long user_simd[64];
	struct pt_regs regs = { .pc = 0x1800, .pstate = 0x60000000UL };

	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(&state, user_simd, 16));
	before = state;
	KUNIT_EXPECT_EQ(test, -EOPNOTSUPP,
		orlix_tcti_sve_execute_predicated_integer_binary(&state, &regs,
			user_simd,
			false,
			ORLIX_TCTI_SVE_INTEGER_ADD, ORLIX_TCTI_SVE_PREDICATE_MERGING,
			0, 0, 1, 2, 1));
	KUNIT_EXPECT_EQ(test, 0x1800UL, regs.pc);
	KUNIT_EXPECT_EQ(test, 0x60000000UL, regs.pstate);
	KUNIT_EXPECT_EQ(test, 0, memcmp(&before, &state, sizeof(state)));
}

static void orlix_tcti_sve_valid_operation_advances_pc_once(struct kunit *test)
{
	struct orlix_tcti_sve_state state;
	unsigned long user_simd[64];
	struct pt_regs regs = { .pc = 0x2000, .pstate = 0x90000000UL };

	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(&state, user_simd, 16));
	state.z[1][0] = 4;
	state.z[2][0] = 5;
	orlix_tcti_sve_set_predicate_lane(&state, 0, 0, 1, true);
	orlix_tcti_sve_sync_z_low_to_v(&state, user_simd, 1);
	orlix_tcti_sve_sync_z_low_to_v(&state, user_simd, 2);
	KUNIT_EXPECT_EQ(test, 0,
		orlix_tcti_sve_execute_predicated_integer_binary(&state, &regs,
			user_simd,
			true,
			ORLIX_TCTI_SVE_INTEGER_ADD, ORLIX_TCTI_SVE_PREDICATE_MERGING,
			0, 0, 1, 2, 1));
	KUNIT_EXPECT_EQ(test, 9, state.z[0][0]);
	KUNIT_EXPECT_EQ(test, 0x2004UL, regs.pc);
	KUNIT_EXPECT_EQ(test, 0x90000000UL, regs.pstate);
}

static struct kunit_case orlix_tcti_sve_state_test_cases[] = {
	KUNIT_CASE(orlix_tcti_sve_state_rejects_invalid_vector_lengths),
	KUNIT_CASE(orlix_tcti_sve_state_reset_zeroes_full_scalable_register_file),
	KUNIT_CASE(orlix_tcti_sve_state_copy_preserves_full_scalable_context),
	KUNIT_CASE(orlix_tcti_sve_state_copy_rejects_invalid_source_without_writing),
	KUNIT_CASE(orlix_tcti_start_thread_resets_full_scalable_sve_context),
	KUNIT_CASE(orlix_tcti_sve_integer_binary_preserves_predicated_lanes),
	KUNIT_CASE(orlix_tcti_sve_integer_binary_zeroes_inactive_lanes),
	KUNIT_CASE(orlix_tcti_sve_integer_binary_covers_all_integer_operations),
	KUNIT_CASE(orlix_tcti_sve_integer_binary_proves_signed_and_high_edges),
	KUNIT_CASE(orlix_tcti_sve_integer_binary_honors_destination_source_aliasing),
	KUNIT_CASE(orlix_tcti_sve_execution_updates_advsimd_v_alias),
	KUNIT_CASE(orlix_tcti_sve_execution_reads_advsimd_v_alias),
	KUNIT_CASE(orlix_tcti_sve_invalid_operation_preserves_state_and_pc),
	KUNIT_CASE(orlix_tcti_sve_unavailable_extension_preserves_state_and_pc),
	KUNIT_CASE(orlix_tcti_sve_valid_operation_advances_pc_once),
	{}
};

struct kunit_suite orlix_tcti_sve_state_test_suite = {
	.name = "orlix-tcti-sve-state",
	.test_cases = orlix_tcti_sve_state_test_cases,
};

kunit_test_suite(orlix_tcti_sve_state_test_suite);

MODULE_LICENSE("GPL");
