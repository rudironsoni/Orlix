/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/errno.h>
#include <linux/bitops.h>
#include <linux/string.h>
#include <linux/unaligned.h>

#include <asm/ptrace.h>

#include "sve_state.h"

static bool tcti_sve_valid_vl(u16 vl_bytes)
{
	return vl_bytes >= TCTI_SVE_MIN_VL_BYTES &&
	       vl_bytes <= TCTI_SVE_MAX_VL_BYTES &&
	       !(vl_bytes % TCTI_SVE_MIN_VL_BYTES);
}

static bool tcti_sve_valid_element_bytes(u8 element_bytes)
{
	return element_bytes == 1 || element_bytes == 2 ||
	       element_bytes == 4 || element_bytes == 8;
}

bool tcti_sve_integer_binary_op_supports_element_bytes(
	enum tcti_sve_integer_binary_op op, u8 element_bytes)
{
	if (!tcti_sve_valid_element_bytes(element_bytes))
		return false;

	switch (op) {
	case TCTI_SVE_INTEGER_SDIV:
	case TCTI_SVE_INTEGER_SDIVR:
	case TCTI_SVE_INTEGER_UDIV:
	case TCTI_SVE_INTEGER_UDIVR:
		return element_bytes >= sizeof(u32);
	default:
		return true;
	}
}

static bool tcti_sve_predicate_lane_active(const struct tcti_sve_state *state,
					   u8 predicate, u16 offset)
{
	u16 bit = offset;

	return state->p[predicate][bit / 8] & BIT(bit % 8);
}

static u64 tcti_sve_read_element(const u8 *vector, u16 offset,
				  u8 element_bytes)
{
	u64 value = 0;
	u8 byte;

	for (byte = 0; byte < element_bytes; byte++)
		value |= (u64)vector[offset + byte] << (byte * 8);
	return value;
}

static void tcti_sve_write_element(u8 *vector, u16 offset, u8 element_bytes,
				   u64 value)
{
	u8 byte;

	for (byte = 0; byte < element_bytes; byte++)
		vector[offset + byte] = value >> (byte * 8);
}

static u64 tcti_sve_element_mask(u8 element_bytes)
{
	return element_bytes == sizeof(u64) ? U64_MAX :
		GENMASK_ULL(element_bytes * BITS_PER_BYTE - 1, 0);
}

static s64 tcti_sve_signed_element(u64 value, u8 element_bytes)
{
	return sign_extend64(value & tcti_sve_element_mask(element_bytes),
			     element_bytes * BITS_PER_BYTE - 1);
}

static int tcti_sve_binary_result(enum tcti_sve_integer_binary_op op,
				  u64 left, u64 right, u8 element_bytes,
				  u64 *result)
{
	u64 mask = tcti_sve_element_mask(element_bytes);
	s64 signed_left = tcti_sve_signed_element(left, element_bytes);
	s64 signed_right = tcti_sve_signed_element(right, element_bytes);
	u8 bits = element_bytes * BITS_PER_BYTE;

	left &= mask;
	right &= mask;
	switch (op) {
	case TCTI_SVE_INTEGER_ADD:
		*result = left + right;
		return 0;
	case TCTI_SVE_INTEGER_SUB:
		*result = left - right;
		return 0;
	case TCTI_SVE_INTEGER_SUBR:
		*result = right - left;
		return 0;
	case TCTI_SVE_INTEGER_SMAX:
		*result = signed_left > signed_right ? left : right;
		return 0;
	case TCTI_SVE_INTEGER_SMIN:
		*result = signed_left < signed_right ? left : right;
		return 0;
	case TCTI_SVE_INTEGER_SABD:
		*result = signed_left >= signed_right ? left - right : right - left;
		return 0;
	case TCTI_SVE_INTEGER_UMAX:
		*result = left > right ? left : right;
		return 0;
	case TCTI_SVE_INTEGER_UMIN:
		*result = left < right ? left : right;
		return 0;
	case TCTI_SVE_INTEGER_UABD:
		*result = left >= right ? left - right : right - left;
		return 0;
	case TCTI_SVE_INTEGER_MUL:
		*result = left * right;
		return 0;
	case TCTI_SVE_INTEGER_SMULH:
		*result = (u64)(((__int128)signed_left * signed_right) >> bits);
		return 0;
	case TCTI_SVE_INTEGER_UMULH:
		*result = (u64)(((unsigned __int128)left * right) >> bits);
		return 0;
	case TCTI_SVE_INTEGER_SDIV:
		if (!signed_right)
			*result = 0;
		else if (left == BIT_ULL(bits - 1) && right == mask)
			*result = left;
		else
			*result = signed_left / signed_right;
		return 0;
	case TCTI_SVE_INTEGER_SDIVR:
		if (!signed_left)
			*result = 0;
		else if (right == BIT_ULL(bits - 1) && left == mask)
			*result = right;
		else
			*result = signed_right / signed_left;
		return 0;
	case TCTI_SVE_INTEGER_UDIV:
		*result = right ? left / right : 0;
		return 0;
	case TCTI_SVE_INTEGER_UDIVR:
		*result = left ? right / left : 0;
		return 0;
	case TCTI_SVE_INTEGER_AND:
		*result = left & right;
		return 0;
	case TCTI_SVE_INTEGER_ORR:
		*result = left | right;
		return 0;
	case TCTI_SVE_INTEGER_EOR:
		*result = left ^ right;
		return 0;
	case TCTI_SVE_INTEGER_BIC:
		*result = left & ~right;
		return 0;
	}

	return -EINVAL;
}

static void tcti_sve_sync_v_to_z(struct tcti_sve_state *state,
				 unsigned long *user_simd, u8 reg)
{
	put_unaligned_le64(user_simd[reg * 2], &state->z[reg][0]);
	put_unaligned_le64(user_simd[reg * 2 + 1], &state->z[reg][8]);
}

static void tcti_sve_sync_z_to_v(const struct tcti_sve_state *state,
				 unsigned long *user_simd, u8 reg)
{
	user_simd[reg * 2] = get_unaligned_le64(&state->z[reg][0]);
	user_simd[reg * 2 + 1] = get_unaligned_le64(&state->z[reg][8]);
}

int tcti_sve_state_reset(struct tcti_sve_state *state,
			 unsigned long *user_simd, u16 vl_bytes)
{
	if (!state || !user_simd || !tcti_sve_valid_vl(vl_bytes))
		return -EINVAL;

	memset(state, 0, sizeof(*state));
	memset(user_simd, 0, TCTI_SVE_ZREG_COUNT * 2 * sizeof(*user_simd));
	state->vl_bytes = vl_bytes;
	state->valid = true;
	return 0;
}

int tcti_sve_state_copy(struct tcti_sve_state *destination,
			unsigned long *destination_simd,
			const struct tcti_sve_state *source,
			const unsigned long *source_simd)
{
	if (!destination || !destination_simd || !source || !source_simd ||
	    !source->valid || !tcti_sve_valid_vl(source->vl_bytes))
		return -EINVAL;

	memcpy(destination, source, sizeof(*destination));
	memcpy(destination_simd, source_simd,
	       TCTI_SVE_ZREG_COUNT * 2 * sizeof(*destination_simd));
	return 0;
}

int tcti_sve_predicated_integer_binary(struct tcti_sve_state *state,
				unsigned long *user_simd,
				enum tcti_sve_integer_binary_op op,
				enum tcti_sve_predication predication,
				u8 zd, u8 pg, u8 zn, u8 zm,
				u8 element_bytes)
{
	u16 offset;
	u64 ignored;
	int ret;

	if (!state || !user_simd || !state->valid ||
	    !tcti_sve_valid_vl(state->vl_bytes) ||
	    !tcti_sve_integer_binary_op_supports_element_bytes(op,
							 element_bytes) ||
	    state->vl_bytes % element_bytes || zd >= TCTI_SVE_ZREG_COUNT ||
	    zn >= TCTI_SVE_ZREG_COUNT || zm >= TCTI_SVE_ZREG_COUNT ||
	    pg >= TCTI_SVE_PREG_COUNT ||
	    (predication != TCTI_SVE_PREDICATE_MERGING &&
	     predication != TCTI_SVE_PREDICATE_ZEROING))
		return -EINVAL;

	/* Validate the operation before modifying any architected state. */
	ret = tcti_sve_binary_result(op, 0, 0, element_bytes, &ignored);
	if (ret)
		return ret;

	/* Vn aliases Zn[127:0], so synchronize every architectural operand. */
	tcti_sve_sync_v_to_z(state, user_simd, zd);
	if (zn != zd)
		tcti_sve_sync_v_to_z(state, user_simd, zn);
	if (zm != zd && zm != zn)
		tcti_sve_sync_v_to_z(state, user_simd, zm);

	for (offset = 0; offset < state->vl_bytes; offset += element_bytes) {
		u64 result;

		if (tcti_sve_predicate_lane_active(state, pg, offset)) {
			ret = tcti_sve_binary_result(op,
				tcti_sve_read_element(state->z[zn], offset,
						      element_bytes),
				tcti_sve_read_element(state->z[zm], offset,
						      element_bytes), element_bytes, &result);
			if (ret)
				return ret;
			tcti_sve_write_element(state->z[zd], offset, element_bytes,
					       result);
		} else if (predication == TCTI_SVE_PREDICATE_ZEROING) {
			memset(&state->z[zd][offset], 0, element_bytes);
		}
	}
	tcti_sve_sync_z_to_v(state, user_simd, zd);

	return 0;
}

int tcti_sve_execute_predicated_integer_binary(struct tcti_sve_state *state,
					struct pt_regs *regs,
					unsigned long *user_simd,
					bool available,
					enum tcti_sve_integer_binary_op op,
					enum tcti_sve_predication predication,
					u8 zd, u8 pg, u8 zn, u8 zm,
					u8 element_bytes)
{
	int ret;

	if (!regs)
		return -EINVAL;
	if (!available)
		return -EOPNOTSUPP;
	ret = tcti_sve_predicated_integer_binary(state, user_simd, op, predication,
						 zd, pg,
						 zn, zm, element_bytes);
	if (!ret)
		regs->pc += sizeof(u32);
	return ret;
}
