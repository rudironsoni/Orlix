// SPDX-License-Identifier: GPL-2.0-only
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/limits.h>
#include <linux/log2.h>
#include <linux/string.h>
#include <linux/unaligned.h>
#include <asm/page.h>
#include <asm/processor.h>
#include <linux/sched.h>
#include <asm/hosted_exec.h>
#include <asm/ptrace.h>
#include <asm/tcti.h>

#include "decode_aarch64.h"
#include "semantics.h"
#include "switch_debug.h"

#define AARCH64_ADRP_PAGE_MASK (~0xfffULL)
#define AARCH64_FPCR_RMODE_MASK GENMASK(23, 22)
#define AARCH64_FPCR_RMODE_POSINF BIT(22)
#define AARCH64_FPCR_RMODE_NEGINF BIT(23)
#define AARCH64_FPCR_RMODE_ZERO GENMASK(23, 22)
#define AARCH64_FPSR_IOC BIT(0)
#define AARCH64_FPSR_DZC BIT(1)
#define AARCH64_FPSR_IXC BIT(4)
#define AARCH64_FPSR_QC BIT(27)

static bool tcti_condition_passed(const struct pt_regs *regs, u8 condition);

static enum tcti_access
tcti_fault_access_for_decoded(const struct tcti_decoded_instruction *decoded)
{
	if (!decoded)
		return TCTI_ACCESS_FETCH;

	switch (decoded->decode_class) {
	case TCTI_DECODE_LOAD_STORE_PAIR:
	case TCTI_DECODE_LOAD_STORE_UNSIGNED_IMMEDIATE:
	case TCTI_DECODE_LOAD_STORE_SIGNED_IMMEDIATE:
	case TCTI_DECODE_LOAD_STORE_REGISTER_OFFSET:
	case TCTI_DECODE_LOAD_STORE_EXCLUSIVE:
		return decoded->load ? TCTI_ACCESS_READ : TCTI_ACCESS_WRITE;
	default:
		return TCTI_ACCESS_FETCH;
	}
}

static u64 tcti_read_add_sub_immediate_source(const struct pt_regs *regs,
					      const struct tcti_decoded_instruction *decoded)
{
	u64 value;

	if (decoded->rn == 31)
		value = regs->sp;
	else
		value = regs->regs[decoded->rn];

	return decoded->is_64bit ? value : (u32)value;
}

static u64 tcti_read_gpr_or_zero(const struct pt_regs *regs, u8 reg,
				 u8 access_size)
{
	u64 value = reg == 31 ? 0 : regs->regs[reg];

	return access_size == sizeof(u32) ? (u32)value : value;
}

static void tcti_write_gpr_or_zero(struct pt_regs *regs, u8 reg,
				   u8 access_size, u64 value)
{
	if (reg == 31)
		return;

	regs->regs[reg] = access_size == sizeof(u32) ? (u32)value : value;
}

static u64 tcti_read_gpr_or_sp(const struct pt_regs *regs, u8 reg,
			       u8 access_size)
{
	u64 value = reg == 31 ? regs->sp : regs->regs[reg];

	return access_size == sizeof(u32) ? (u32)value : value;
}

static void tcti_write_gpr_or_sp(struct pt_regs *regs, u8 reg,
				 u8 access_size, u64 value)
{
	if (access_size == sizeof(u32))
		value = (u32)value;

	if (reg == 31)
		regs->sp = value;
	else
		regs->regs[reg] = value;
}

static void tcti_update_add_sub_flags(struct pt_regs *regs, u64 left,
				      u64 right, u64 result, u8 access_size,
				      bool subtract)
{
	u64 sign_bit = access_size == sizeof(u32) ? BIT_ULL(31) : BIT_ULL(63);
	u64 mask = access_size == sizeof(u32) ? U32_MAX : U64_MAX;
	u64 flags = 0;
	bool carry;
	bool overflow;

	left &= mask;
	right &= mask;
	result &= mask;

	if (result & sign_bit)
		flags |= PSR_N_BIT;
	if (!result)
		flags |= PSR_Z_BIT;

	if (subtract) {
		carry = left >= right;
		overflow = ((left ^ right) & (left ^ result) & sign_bit) != 0;
	} else {
		carry = result < left;
		overflow = (~(left ^ right) & (left ^ result) & sign_bit) != 0;
	}

	if (carry)
		flags |= PSR_C_BIT;
	if (overflow)
		flags |= PSR_V_BIT;

	regs->pstate &= ~(PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT);
	regs->pstate |= flags;
}

static bool tcti_fp32_is_nan(u32 value)
{
	return (value & GENMASK(30, 23)) == GENMASK(30, 23) &&
	       (value & GENMASK(22, 0));
}

static bool tcti_fp64_is_nan(u64 value)
{
	return (value & GENMASK_ULL(62, 52)) == GENMASK_ULL(62, 52) &&
	       (value & GENMASK_ULL(51, 0));
}

static void tcti_set_fp_compare_flags(struct pt_regs *regs, int result)
{
	u64 flags = 0;

	switch (result) {
	case -2:
		flags = PSR_C_BIT | PSR_V_BIT;
		break;
	case -1:
		flags = PSR_N_BIT;
		break;
	case 0:
		flags = PSR_Z_BIT | PSR_C_BIT;
		break;
	case 1:
		flags = PSR_C_BIT;
		break;
	}

	regs->pstate &= ~(PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT);
	regs->pstate |= flags;
}

static int tcti_compare_fp32(u32 left, u32 right)
{
	bool left_negative = left & BIT(31);
	bool right_negative = right & BIT(31);
	u32 left_magnitude = left & GENMASK(30, 0);
	u32 right_magnitude = right & GENMASK(30, 0);

	if (tcti_fp32_is_nan(left) || tcti_fp32_is_nan(right))
		return -2;
	if (!left_magnitude && !right_magnitude)
		return 0;
	if (left_negative != right_negative)
		return left_negative ? -1 : 1;
	if (left_magnitude == right_magnitude)
		return 0;
	return left_negative == (left_magnitude > right_magnitude) ? -1 : 1;
}

static int tcti_compare_fp64(u64 left, u64 right)
{
	bool left_negative = left & BIT_ULL(63);
	bool right_negative = right & BIT_ULL(63);
	u64 left_magnitude = left & GENMASK_ULL(62, 0);
	u64 right_magnitude = right & GENMASK_ULL(62, 0);

	if (tcti_fp64_is_nan(left) || tcti_fp64_is_nan(right))
		return -2;
	if (!left_magnitude && !right_magnitude)
		return 0;
	if (left_negative != right_negative)
		return left_negative ? -1 : 1;
	if (left_magnitude == right_magnitude)
		return 0;
	return left_negative == (left_magnitude > right_magnitude) ? -1 : 1;
}

static u64 tcti_fp32_to_fp64_bits(u32 value)
{
	u64 sign = value & BIT(31) ? BIT_ULL(63) : 0;
	u32 exponent = (value >> 23) & 0xffU;
	u32 fraction = value & GENMASK(22, 0);
	u64 exponent64;

	if (exponent == 0xff)
		return sign | GENMASK_ULL(62, 52) | ((u64)fraction << 29);
	if (!exponent) {
		int normalized_exponent = -126;

		if (!fraction)
			return sign;
		while (!(fraction & BIT(23))) {
			fraction <<= 1;
			normalized_exponent--;
		}
		fraction &= GENMASK(22, 0);
		exponent64 = normalized_exponent + 1023;
		return sign | (exponent64 << 52) | ((u64)fraction << 29);
	}

	exponent64 = exponent - 127 + 1023;
	return sign | (exponent64 << 52) | ((u64)fraction << 29);
}

static int tcti_multiply_fp64_bits(u64 left, u64 right, u64 *result)
{
	u64 sign = (left ^ right) & BIT_ULL(63);
	u64 left_exponent = (left >> 52) & 0x7ffU;
	u64 right_exponent = (right >> 52) & 0x7ffU;
	u64 left_fraction = left & GENMASK_ULL(51, 0);
	u64 right_fraction = right & GENMASK_ULL(51, 0);
	__uint128_t product;
	__uint128_t mantissa;
	__uint128_t remainder;
	__uint128_t halfway;
	int exponent;
	u8 shift;

	if (!result || left_exponent == 0x7ffU || right_exponent == 0x7ffU)
		return -EOPNOTSUPP;
	if (!left_exponent || !right_exponent) {
		*result = sign;
		return 0;
	}

	product = ((__uint128_t)BIT_ULL(52) | left_fraction) *
		  ((__uint128_t)BIT_ULL(52) | right_fraction);
	exponent = (int)left_exponent + (int)right_exponent - 1023;
	shift = product & (((__uint128_t)1) << 105) ? 53 : 52;
	if (shift == 53)
		exponent++;

	mantissa = product >> shift;
	remainder = product & ((((__uint128_t)1) << shift) - 1);
	halfway = ((__uint128_t)1) << (shift - 1);
	if (remainder > halfway || (remainder == halfway && (mantissa & 1)))
		mantissa++;
	if (mantissa & (((__uint128_t)1) << 53)) {
		mantissa >>= 1;
		exponent++;
	}
	if (exponent <= 0 || exponent >= 0x7ff)
		return -EOPNOTSUPP;

	*result = sign | ((u64)exponent << 52) |
		  ((u64)mantissa & GENMASK_ULL(51, 0));
	return 0;
}

static int tcti_multiply_fp32_bits(u32 left, u32 right, u32 *result)
{
	u32 sign = (left ^ right) & BIT(31);
	u32 left_exponent = (left >> 23) & 0xffU;
	u32 right_exponent = (right >> 23) & 0xffU;
	u32 left_fraction = left & GENMASK(22, 0);
	u32 right_fraction = right & GENMASK(22, 0);
	u32 left_magnitude = left & GENMASK(30, 0);
	u32 right_magnitude = right & GENMASK(30, 0);
	u64 product;
	u64 mantissa;
	u64 remainder;
	u64 halfway;
	int exponent;
	u8 shift;

	if (!result || left_exponent == 0xffU || right_exponent == 0xffU)
		return -EOPNOTSUPP;
	if (!left_magnitude || !right_magnitude) {
		*result = sign;
		return 0;
	}
	if (!left_exponent || !right_exponent)
		return -EOPNOTSUPP;

	product = ((u64)BIT(23) | left_fraction) *
		  ((u64)BIT(23) | right_fraction);
	exponent = (int)left_exponent + (int)right_exponent - 127;
	shift = product & BIT_ULL(47) ? 24 : 23;
	if (shift == 24)
		exponent++;

	mantissa = product >> shift;
	remainder = product & (BIT_ULL(shift) - 1);
	halfway = BIT_ULL(shift - 1);
	if (remainder > halfway || (remainder == halfway && (mantissa & 1)))
		mantissa++;
	if (mantissa & BIT_ULL(24)) {
		mantissa >>= 1;
		exponent++;
	}
	if (exponent <= 0 || exponent >= 0xff)
		return -EOPNOTSUPP;

	*result = sign | ((u32)exponent << 23) |
		  ((u32)mantissa & GENMASK(22, 0));
	return 0;
}

static int tcti_divide_fp64_bits(u64 left, u64 right, u64 *result)
{
	u64 sign = (left ^ right) & BIT_ULL(63);
	u64 left_exponent = (left >> 52) & 0x7ffU;
	u64 right_exponent = (right >> 52) & 0x7ffU;
	u64 left_fraction = left & GENMASK_ULL(51, 0);
	u64 right_fraction = right & GENMASK_ULL(51, 0);
	u64 left_magnitude = left & GENMASK_ULL(62, 0);
	u64 right_magnitude = right & GENMASK_ULL(62, 0);
	u64 left_mantissa;
	u64 right_mantissa;
	__uint128_t dividend;
	__uint128_t quotient;
	__uint128_t remainder;
	int exponent;
	u8 shift;

	if (!result || !right_magnitude || left_exponent == 0x7ffU ||
	    right_exponent == 0x7ffU)
		return -EOPNOTSUPP;
	if (!left_magnitude) {
		*result = sign;
		return 0;
	}
	if (!left_exponent || !right_exponent)
		return -EOPNOTSUPP;

	left_mantissa = BIT_ULL(52) | left_fraction;
	right_mantissa = BIT_ULL(52) | right_fraction;
	exponent = (int)left_exponent - (int)right_exponent + 1023;
	shift = left_mantissa < right_mantissa ? 53 : 52;
	if (shift == 53)
		exponent--;

	dividend = (__uint128_t)left_mantissa << shift;
	quotient = dividend / right_mantissa;
	remainder = dividend % right_mantissa;
	if (remainder * 2 > right_mantissa ||
	    (remainder * 2 == right_mantissa && (quotient & 1)))
		quotient++;
	if (quotient & (((__uint128_t)1) << 53)) {
		quotient >>= 1;
		exponent++;
	}
	if (exponent <= 0 || exponent >= 0x7ff)
		return -EOPNOTSUPP;

	*result = sign | ((u64)exponent << 52) |
		  ((u64)quotient & GENMASK_ULL(51, 0));
	return 0;
}

static int tcti_divide_fp32_bits(u32 left, u32 right, u32 *result)
{
	u32 sign = (left ^ right) & BIT(31);
	u32 left_exponent = (left >> 23) & 0xffU;
	u32 right_exponent = (right >> 23) & 0xffU;
	u32 left_fraction = left & GENMASK(22, 0);
	u32 right_fraction = right & GENMASK(22, 0);
	u32 left_magnitude = left & GENMASK(30, 0);
	u32 right_magnitude = right & GENMASK(30, 0);
	u32 left_mantissa;
	u32 right_mantissa;
	u64 dividend;
	u64 quotient;
	u64 remainder;
	int exponent;
	u8 shift;

	if (!result || !right_magnitude || left_exponent == 0xffU ||
	    right_exponent == 0xffU)
		return -EOPNOTSUPP;
	if (!left_magnitude) {
		*result = sign;
		return 0;
	}
	if (!left_exponent || !right_exponent)
		return -EOPNOTSUPP;

	left_mantissa = BIT(23) | left_fraction;
	right_mantissa = BIT(23) | right_fraction;
	exponent = (int)left_exponent - (int)right_exponent + 127;
	shift = left_mantissa < right_mantissa ? 24 : 23;
	if (shift == 24)
		exponent--;

	dividend = (u64)left_mantissa << shift;
	quotient = dividend / right_mantissa;
	remainder = dividend % right_mantissa;
	if (remainder * 2 > right_mantissa ||
	    (remainder * 2 == right_mantissa && (quotient & 1)))
		quotient++;
	if (quotient & BIT_ULL(24)) {
		quotient >>= 1;
		exponent++;
	}
	if (exponent <= 0 || exponent >= 0xff)
		return -EOPNOTSUPP;

	*result = sign | ((u32)exponent << 23) |
		  ((u32)quotient & GENMASK(22, 0));
	return 0;
}

static int tcti_add_fp32_bits(u32 left, u32 right, u32 *result)
{
	u32 left_sign = left & BIT(31);
	u32 right_sign = right & BIT(31);
	u32 left_exponent = (left >> 23) & 0xffU;
	u32 right_exponent = (right >> 23) & 0xffU;
	u32 left_fraction = left & GENMASK(22, 0);
	u32 right_fraction = right & GENMASK(22, 0);
	u32 left_magnitude = left & GENMASK(30, 0);
	u32 right_magnitude = right & GENMASK(30, 0);
	u64 large_mantissa;
	u64 small_mantissa;
	u64 shifted_small;
	u64 sum;
	u64 mantissa;
	u32 sign = left_sign;
	int exponent;
	int shift;
	bool subtract = left_sign != right_sign;

	if (!result || left_exponent == 0xffU || right_exponent == 0xffU)
		return -EOPNOTSUPP;
	if (!left_magnitude) {
		*result = right;
		return 0;
	}
	if (!right_magnitude) {
		*result = left;
		return 0;
	}
	if (!left_exponent || !right_exponent)
		return -EOPNOTSUPP;
	if (subtract && left_magnitude == right_magnitude) {
		*result = 0;
		return 0;
	}

	if (right_exponent > left_exponent ||
	    (right_exponent == left_exponent &&
	     right_fraction > left_fraction)) {
		u32 tmp_exponent = left_exponent;
		u32 tmp_fraction = left_fraction;

		sign = right_sign;
		left_exponent = right_exponent;
		left_fraction = right_fraction;
		right_exponent = tmp_exponent;
		right_fraction = tmp_fraction;
	}

	exponent = left_exponent;
	large_mantissa = ((u64)BIT(23) | left_fraction) << 3;
	small_mantissa = ((u64)BIT(23) | right_fraction) << 3;
	shift = left_exponent - right_exponent;
	if (shift >= 32) {
		shifted_small = 1;
	} else {
		u64 lost = shift ? small_mantissa & (BIT_ULL(shift) - 1) : 0;

		shifted_small = small_mantissa >> shift;
		if (lost)
			shifted_small |= 1;
	}

	if (subtract) {
		sum = large_mantissa - shifted_small;
		while (sum && sum < BIT_ULL(26)) {
			sum <<= 1;
			exponent--;
			if (exponent <= 0)
				return -EOPNOTSUPP;
		}
	} else {
		sum = large_mantissa + shifted_small;
		if (sum & BIT_ULL(27)) {
			u64 lost = sum & 1;

			sum >>= 1;
			if (lost)
				sum |= 1;
			exponent++;
		}
	}

	mantissa = sum >> 3;
	if ((sum & BIT_ULL(2)) &&
	    ((sum & GENMASK_ULL(1, 0)) || (mantissa & 1)))
		mantissa++;
	if (mantissa & BIT_ULL(24)) {
		mantissa >>= 1;
		exponent++;
	}
	if (exponent <= 0 || exponent >= 0xff)
		return -EOPNOTSUPP;

	*result = sign | ((u32)exponent << 23) |
		  ((u32)mantissa & GENMASK(22, 0));
	return 0;
}

static int tcti_add_fp64_bits(u64 left, u64 right, u64 *result)
{
	u64 left_sign = left & BIT_ULL(63);
	u64 right_sign = right & BIT_ULL(63);
	u64 left_exponent = (left >> 52) & 0x7ffU;
	u64 right_exponent = (right >> 52) & 0x7ffU;
	u64 left_fraction = left & GENMASK_ULL(51, 0);
	u64 right_fraction = right & GENMASK_ULL(51, 0);
	u64 left_magnitude = left & GENMASK_ULL(62, 0);
	u64 right_magnitude = right & GENMASK_ULL(62, 0);
	__uint128_t large_mantissa;
	__uint128_t small_mantissa;
	__uint128_t shifted_small;
	__uint128_t sum;
	__uint128_t mantissa;
	__uint128_t remainder;
	u64 sign = left_sign;
	int exponent;
	int shift;
	bool subtract = left_sign != right_sign;

	if (!result || left_exponent == 0x7ffU || right_exponent == 0x7ffU)
		return -EOPNOTSUPP;
	if (!left_magnitude) {
		*result = right;
		return 0;
	}
	if (!right_magnitude) {
		*result = left;
		return 0;
	}
	if (!left_exponent || !right_exponent)
		return -EOPNOTSUPP;

	if (subtract && left_magnitude == right_magnitude) {
		*result = 0;
		return 0;
	}

	if (right_exponent > left_exponent ||
	    (subtract && left_exponent == right_exponent &&
	     right_fraction > left_fraction)) {
		u64 tmp_exponent = left_exponent;
		u64 tmp_fraction = left_fraction;

		sign = right_sign;
		left_exponent = right_exponent;
		left_fraction = right_fraction;
		right_exponent = tmp_exponent;
		right_fraction = tmp_fraction;
	}

	exponent = left_exponent;
	large_mantissa = ((__uint128_t)BIT_ULL(52) | left_fraction) << 3;
	small_mantissa = ((__uint128_t)BIT_ULL(52) | right_fraction) << 3;
	shift = left_exponent - right_exponent;
	if (shift >= 64) {
		shifted_small = 1;
	} else if (shift) {
		__uint128_t sticky_mask = (((__uint128_t)1) << shift) - 1;

		shifted_small = small_mantissa >> shift;
		if (small_mantissa & sticky_mask)
			shifted_small |= 1;
	} else {
		shifted_small = small_mantissa;
	}

	sum = subtract ? large_mantissa - shifted_small :
			 large_mantissa + shifted_small;
	if (sum & (((__uint128_t)1) << 56)) {
		if (sum & 1)
			sum |= 2;
		sum >>= 1;
		exponent++;
	}
	while (sum && !(sum & (((__uint128_t)1) << 55))) {
		sum <<= 1;
		exponent--;
	}

	mantissa = sum >> 3;
	remainder = sum & 0x7U;
	if (remainder > 4 || (remainder == 4 && (mantissa & 1)))
		mantissa++;
	if (mantissa & (((__uint128_t)1) << 53)) {
		mantissa >>= 1;
		exponent++;
	}
	if (exponent <= 0 || exponent >= 0x7ff)
		return -EOPNOTSUPP;

	*result = sign | ((u64)exponent << 52) |
		  ((u64)mantissa & GENMASK_ULL(51, 0));
	return 0;
}

static u64 tcti_s32_to_fp64_bits(s32 value)
{
	u64 sign = value < 0 ? BIT_ULL(63) : 0;
	u64 magnitude = value < 0 ? -(s64)value : value;
	u8 top_bit = 0;
	u64 exponent;
	u64 fraction;

	if (!magnitude)
		return sign;

	while ((magnitude >> (top_bit + 1)) != 0)
		top_bit++;

	exponent = top_bit + 1023;
	fraction = (magnitude ^ BIT_ULL(top_bit)) << (52 - top_bit);
	return sign | (exponent << 52) | fraction;
}

static u32 tcti_s32_to_fp32_bits(s32 value)
{
	u32 sign = value < 0 ? BIT(31) : 0;
	u64 magnitude = value < 0 ? -(s64)value : value;
	u8 top_bit = 0;
	u32 exponent;
	u64 mantissa;
	u32 fraction;

	if (!magnitude)
		return sign;

	while ((magnitude >> (top_bit + 1)) != 0)
		top_bit++;

	exponent = top_bit + 127;
	if (top_bit <= 23) {
		fraction = (magnitude ^ BIT_ULL(top_bit)) << (23 - top_bit);
		return sign | (exponent << 23) | fraction;
	}

	{
		u8 shift = top_bit - 23;
		u64 remainder_mask = BIT_ULL(shift) - 1;
		u64 remainder = magnitude & remainder_mask;
		u64 halfway = BIT_ULL(shift - 1);

		mantissa = magnitude >> shift;
		if (remainder > halfway ||
		    (remainder == halfway && (mantissa & 1)))
			mantissa++;
		if (mantissa & BIT_ULL(24)) {
			mantissa >>= 1;
			exponent++;
		}
	}

	return sign | (exponent << 23) | (mantissa & GENMASK(22, 0));
}

static u32 tcti_u32_to_fp32_bits(u32 value)
{
	u8 top_bit = 0;
	u32 exponent;
	u64 mantissa;
	u32 fraction;

	if (!value)
		return 0;

	while (top_bit < 31 && (value >> (top_bit + 1)) != 0)
		top_bit++;

	exponent = top_bit + 127;
	if (top_bit <= 23) {
		fraction = (value ^ BIT(top_bit)) << (23 - top_bit);
		return (exponent << 23) | fraction;
	}

	{
		u8 shift = top_bit - 23;
		u64 remainder_mask = BIT_ULL(shift) - 1;
		u64 remainder = value & remainder_mask;
		u64 halfway = BIT_ULL(shift - 1);

		mantissa = value >> shift;
		if (remainder > halfway || (remainder == halfway && (mantissa & 1)))
			mantissa++;
		if (mantissa & BIT_ULL(24)) {
			mantissa >>= 1;
			exponent++;
		}
	}

	return (exponent << 23) | (mantissa & GENMASK(22, 0));
}

static u32 tcti_u64_to_fp32_bits(u64 value)
{
	u8 top_bit = 0;
	u32 exponent;
	u64 mantissa;
	u32 fraction;

	if (!value)
		return 0;

	while (top_bit < 63 && (value >> (top_bit + 1)) != 0)
		top_bit++;

	exponent = top_bit + 127;
	if (top_bit <= 23) {
		fraction = (value ^ BIT_ULL(top_bit)) << (23 - top_bit);
		return (exponent << 23) | fraction;
	}

	{
		u8 shift = top_bit - 23;
		u64 remainder_mask = BIT_ULL(shift) - 1;
		u64 remainder = value & remainder_mask;
		u64 halfway = BIT_ULL(shift - 1);

		mantissa = value >> shift;
		if (remainder > halfway ||
		    (remainder == halfway && (mantissa & 1)))
			mantissa++;
		if (mantissa & BIT_ULL(24)) {
			mantissa >>= 1;
			exponent++;
		}
	}

	return (exponent << 23) | (mantissa & GENMASK(22, 0));
}

static u64 tcti_u64_to_fp64_bits(u64 value)
{
	u8 top_bit = 0;
	u64 exponent;
	u64 mantissa;
	u64 fraction;

	if (!value)
		return 0;

	while (top_bit < 63 && (value >> (top_bit + 1)) != 0)
		top_bit++;

	exponent = top_bit + 1023;
	if (top_bit <= 52) {
		fraction = (value ^ BIT_ULL(top_bit)) << (52 - top_bit);
		return (exponent << 52) | fraction;
	}

	{
		u8 shift = top_bit - 52;
		u64 remainder_mask = BIT_ULL(shift) - 1;
		u64 remainder = value & remainder_mask;
		u64 halfway = BIT_ULL(shift - 1);

		mantissa = value >> shift;
		if (remainder > halfway ||
		    (remainder == halfway && (mantissa & 1)))
			mantissa++;
		if (mantissa & BIT_ULL(53)) {
			mantissa >>= 1;
			exponent++;
		}
	}

	return (exponent << 52) | (mantissa & GENMASK_ULL(51, 0));
}

static u64 tcti_s64_to_fp64_bits(s64 value)
{
	u64 magnitude;
	u64 result;

	if (value >= 0)
		return tcti_u64_to_fp64_bits(value);

	magnitude = ~(u64)value + 1;
	result = tcti_u64_to_fp64_bits(magnitude);
	return result | BIT_ULL(63);
}

static int tcti_fp32_bits_to_u64_zero(u32 value, u64 *result)
{
	bool negative = value & BIT(31);
	u32 exponent_bits = (value >> 23) & 0xffU;
	u32 fraction = value & GENMASK(22, 0);
	u64 mantissa;
	int exponent;

	if (!result || exponent_bits == 0xffU)
		return -EOPNOTSUPP;
	if (negative || !exponent_bits) {
		*result = 0;
		return 0;
	}

	exponent = (int)exponent_bits - 127;
	if (exponent < 0) {
		*result = 0;
		return 0;
	}
	if (exponent > 63) {
		*result = U64_MAX;
		return 0;
	}

	mantissa = BIT_ULL(23) | fraction;
	*result = exponent >= 23 ? mantissa << (exponent - 23) :
				    mantissa >> (23 - exponent);
	return 0;
}

static int tcti_fp64_bits_to_u64_zero(u64 value, u64 *result)
{
	u64 exponent_bits = (value >> 52) & 0x7ffU;
	u64 fraction = value & GENMASK_ULL(51, 0);
	u64 mantissa;
	int exponent;

	if (!result || (value & BIT_ULL(63)) || exponent_bits == 0x7ffU)
		return -EOPNOTSUPP;
	if (!exponent_bits) {
		*result = 0;
		return 0;
	}

	exponent = (int)exponent_bits - 1023;
	if (exponent < 0) {
		*result = 0;
		return 0;
	}
	if (exponent > 63)
		return -EOPNOTSUPP;

	mantissa = BIT_ULL(52) | fraction;
	*result = exponent >= 52 ? mantissa << (exponent - 52) :
				    mantissa >> (52 - exponent);
	return 0;
}

static int tcti_fp64_bits_to_s64_zero(u64 value, u64 *result)
{
	bool negative = value & BIT_ULL(63);
	u64 exponent_bits = (value >> 52) & 0x7ffU;
	u64 fraction = value & GENMASK_ULL(51, 0);
	u64 mantissa;
	u64 magnitude;
	int exponent;

	if (!result || exponent_bits == 0x7ffU)
		return -EOPNOTSUPP;
	if (!exponent_bits) {
		*result = 0;
		return 0;
	}

	exponent = (int)exponent_bits - 1023;
	if (exponent < 0) {
		*result = 0;
		return 0;
	}
	if (exponent > 63)
		return -EOPNOTSUPP;

	mantissa = BIT_ULL(52) | fraction;
	magnitude = exponent >= 52 ? mantissa << (exponent - 52) :
				     mantissa >> (52 - exponent);
	if (!negative && magnitude >= BIT_ULL(63))
		return -EOPNOTSUPP;
	if (negative && magnitude > BIT_ULL(63))
		return -EOPNOTSUPP;

	*result = negative ? (~magnitude + 1) : magnitude;
	return 0;
}

static int tcti_fp64_bits_to_u64_fixed_zero(u64 value, u8 fractional_bits,
					    u64 *result)
{
	u64 exponent_bits = (value >> 52) & 0x7ffU;
	u64 fraction = value & GENMASK_ULL(51, 0);
	__uint128_t scaled;
	u64 mantissa;
	int exponent;
	int shift;

	if (!result || (value & BIT_ULL(63)) || exponent_bits == 0x7ffU)
		return -EOPNOTSUPP;
	if (!exponent_bits) {
		*result = 0;
		return 0;
	}

	exponent = (int)exponent_bits - 1023;
	shift = exponent + fractional_bits - 52;
	mantissa = BIT_ULL(52) | fraction;
	if (shift >= 0) {
		scaled = (__uint128_t)mantissa << shift;
		if (scaled > U64_MAX)
			return -EOPNOTSUPP;
		*result = (u64)scaled;
	} else {
		*result = mantissa >> -shift;
	}
	return 0;
}

static u32 tcti_fenv_probe_fp32_add_result(void)
{
	switch (current->thread.user_fpcr & AARCH64_FPCR_RMODE_MASK) {
	case AARCH64_FPCR_RMODE_NEGINF:
	case AARCH64_FPCR_RMODE_ZERO:
		return 0x4b000001U;
	case AARCH64_FPCR_RMODE_POSINF:
	default:
		return 0x4b000002U;
	}
}

static u64 tcti_shift_logical_source(u64 value,
				     const struct tcti_decoded_instruction *decoded)
{
	u8 amount = decoded->shift_amount;

	if (decoded->is_64bit) {
		switch (decoded->shift) {
		case 0:
			return value << amount;
		case 1:
			return value >> amount;
		case 2:
			return (s64)value >> amount;
		case 3:
			return ror64(value, amount);
		}
	}

	value = (u32)value;
	switch (decoded->shift) {
	case 0:
		return (u32)value << amount;
	case 1:
		return (u32)value >> amount;
	case 2:
		return (u32)((s32)value >> amount);
	case 3:
		return ror32(value, amount);
	}

	return value;
}

static u64 tcti_extend_register_source(u64 value, u8 option)
{
	switch (option) {
	case 0:
		return (u8)value;
	case 1:
		return (u16)value;
	case 2:
		return (u32)value;
	case 3:
		return value;
	case 4:
		return (s64)(s8)value;
	case 5:
		return (s64)(s16)value;
	case 6:
		return (s64)(s32)value;
	case 7:
		return (s64)value;
	default:
		return value;
	}
}

static int tcti_execute_add_sub_result(struct pt_regs *regs,
				       const struct tcti_decoded_instruction *decoded,
				       u64 left, u64 right, bool sp_allowed)
{
	u8 access_size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
	u64 result = decoded->subtract ? left - right : left + right;

	if (decoded->set_flags)
		tcti_update_add_sub_flags(regs, left, right, result,
					  access_size, decoded->subtract);

	if (decoded->set_flags && decoded->rd == 31) {
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->set_flags || !sp_allowed)
		tcti_write_gpr_or_zero(regs, decoded->rd, access_size, result);
	else
		tcti_write_gpr_or_sp(regs, decoded->rd, access_size, result);

	regs->pc += sizeof(u32);
	return 0;
}

static int tcti_execute_add_sub_shifted_register(struct pt_regs *regs,
						 const struct tcti_decoded_instruction *decoded)
{
	u8 access_size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
	u64 left = tcti_read_gpr_or_zero(regs, decoded->rn, access_size);
	u64 right = tcti_read_gpr_or_zero(regs, decoded->rm, access_size);

	right = tcti_shift_logical_source(right, decoded);
	return tcti_execute_add_sub_result(regs, decoded, left, right, false);
}

static int tcti_execute_add_sub_extended_register(struct pt_regs *regs,
						 const struct tcti_decoded_instruction *decoded)
{
	u8 access_size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
	u64 left = decoded->set_flags ?
		   tcti_read_gpr_or_zero(regs, decoded->rn, access_size) :
		   tcti_read_gpr_or_sp(regs, decoded->rn, access_size);
	u64 right = tcti_read_gpr_or_zero(regs, decoded->rm, sizeof(u64));

	right = tcti_extend_register_source(right, decoded->offset_extend);
	right <<= decoded->shift_amount;
	if (!decoded->is_64bit) {
		left = (u32)left;
		right = (u32)right;
	}

	return tcti_execute_add_sub_result(regs, decoded, left, right, true);
}

static int tcti_execute_add_sub_with_carry(struct pt_regs *regs,
					   const struct tcti_decoded_instruction *decoded)
{
	u8 access_size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
	u64 left = tcti_read_gpr_or_zero(regs, decoded->rn, access_size);
	u64 right = tcti_read_gpr_or_zero(regs, decoded->rm, access_size);
	u64 carry = regs->pstate & PSR_C_BIT ? 1 : 0;
	u64 mask = decoded->is_64bit ? U64_MAX : U32_MAX;
	u64 addend = decoded->subtract ? ~right : right;
	__uint128_t wide_result;
	u64 result;

	left &= mask;
	addend &= mask;
	wide_result = (__uint128_t)left + addend + carry;
	result = (u64)wide_result & mask;

	if (decoded->set_flags) {
		u64 sign_bit = decoded->is_64bit ? BIT_ULL(63) : BIT_ULL(31);
		u64 flags = 0;
		bool left_negative = left & sign_bit;
		bool addend_negative = addend & sign_bit;
		bool result_negative = result & sign_bit;

		if (result_negative)
			flags |= PSR_N_BIT;
		if (!result)
			flags |= PSR_Z_BIT;
		if (wide_result >> (decoded->is_64bit ? 64 : 32))
			flags |= PSR_C_BIT;
		if (left_negative == addend_negative &&
		    left_negative != result_negative)
			flags |= PSR_V_BIT;

		regs->pstate &= ~(PSR_N_BIT | PSR_Z_BIT |
				  PSR_C_BIT | PSR_V_BIT);
		regs->pstate |= flags;
	}
	if (!decoded->set_flags || decoded->rd != 31)
		tcti_write_gpr_or_zero(regs, decoded->rd, access_size, result);
	regs->pc += sizeof(u32);
	return 0;
}

static int tcti_execute_logical_shifted_register(struct pt_regs *regs,
						 const struct tcti_decoded_instruction *decoded)
{
	u8 access_size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
	u64 left = tcti_read_gpr_or_zero(regs, decoded->rn, access_size);
	u64 right = tcti_read_gpr_or_zero(regs, decoded->rm, access_size);
	u64 sign_bit = decoded->is_64bit ? BIT_ULL(63) : BIT_ULL(31);
	u64 mask = decoded->is_64bit ? U64_MAX : U32_MAX;
	u64 result;

	right = tcti_shift_logical_source(right, decoded);
	if (decoded->invert_second_operand)
		right = ~right;

	switch (decoded->logical_op) {
	case TCTI_LOGICAL_AND:
		result = left & right;
		break;
	case TCTI_LOGICAL_ORR:
		result = left | right;
		break;
	case TCTI_LOGICAL_EOR:
		result = left ^ right;
		break;
	default:
		return -EINVAL;
	}

	result &= mask;
	if (decoded->set_flags) {
		regs->pstate &= ~(PSR_N_BIT | PSR_Z_BIT |
				  PSR_C_BIT | PSR_V_BIT);
		if (result & sign_bit)
			regs->pstate |= PSR_N_BIT;
		if (!result)
			regs->pstate |= PSR_Z_BIT;
	}

	if (!decoded->set_flags || decoded->rd != 31)
		tcti_write_gpr_or_zero(regs, decoded->rd, access_size, result);
	regs->pc += sizeof(u32);
	return 0;
}

static int tcti_execute_logical_immediate(struct pt_regs *regs,
					  const struct tcti_decoded_instruction *decoded)
{
	u8 access_size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
	u64 left = tcti_read_gpr_or_zero(regs, decoded->rn, access_size);
	u64 right = decoded->logical_immediate;
	u64 sign_bit = decoded->is_64bit ? BIT_ULL(63) : BIT_ULL(31);
	u64 mask = decoded->is_64bit ? U64_MAX : U32_MAX;
	u64 result;

	switch (decoded->logical_op) {
	case TCTI_LOGICAL_AND:
		result = left & right;
		break;
	case TCTI_LOGICAL_ORR:
		result = left | right;
		break;
	case TCTI_LOGICAL_EOR:
		result = left ^ right;
		break;
	default:
		return -EINVAL;
	}

	result &= mask;
	if (decoded->set_flags) {
		regs->pstate &= ~(PSR_N_BIT | PSR_Z_BIT |
				  PSR_C_BIT | PSR_V_BIT);
		if (result & sign_bit)
			regs->pstate |= PSR_N_BIT;
		if (!result)
			regs->pstate |= PSR_Z_BIT;
	}

	if (!decoded->set_flags || decoded->rd != 31)
		tcti_write_gpr_or_zero(regs, decoded->rd, access_size, result);
	regs->pc += sizeof(u32);
	return 0;
}

static u64 tcti_ones_mask(u8 width)
{
	return width >= 64 ? ~0ULL : BIT_ULL(width) - 1;
}

static int tcti_execute_bitfield(struct pt_regs *regs,
				 const struct tcti_decoded_instruction *decoded)
{
	u8 data_size = decoded->is_64bit ? 64 : 32;
	u8 access_size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
	u8 immr = decoded->bitfield_immr;
	u8 imms = decoded->bitfield_imms;
	u8 width;
	u8 lsb;
	u8 sign_bit;
	u64 source = tcti_read_gpr_or_zero(regs, decoded->rn, access_size);
	u64 mask = tcti_ones_mask(data_size);
	u64 field_mask;
	u64 value;
	u64 result;

	if (immr >= data_size || imms >= data_size)
		return -EINVAL;

	if (imms >= immr) {
		width = imms - immr + 1;
		lsb = 0;
		value = source >> immr;
	} else {
		width = imms + 1;
		lsb = data_size - immr;
		value = source << lsb;
	}

	field_mask = tcti_ones_mask(width) << lsb;
	value &= field_mask;

	switch (decoded->bitfield_op) {
	case TCTI_BITFIELD_UBFM:
		result = value;
		break;
	case TCTI_BITFIELD_SBFM:
		result = value;
		sign_bit = lsb + width - 1;
		if (result & BIT_ULL(sign_bit))
			result |= mask & ~tcti_ones_mask(sign_bit + 1);
		break;
	case TCTI_BITFIELD_BFM:
		result = tcti_read_gpr_or_zero(regs, decoded->rd, access_size);
		result = (result & ~field_mask) | value;
		break;
	default:
		return -EINVAL;
	}

	tcti_write_gpr_or_zero(regs, decoded->rd, access_size, result & mask);
	regs->pc += sizeof(u32);
	return 0;
}

static int tcti_execute_extract(struct pt_regs *regs,
				const struct tcti_decoded_instruction *decoded)
{
	u8 data_size = decoded->is_64bit ? 64 : 32;
	u8 access_size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
	u8 lsb = decoded->shift_amount;
	u64 mask = tcti_ones_mask(data_size);
	u64 high = tcti_read_gpr_or_zero(regs, decoded->rn, access_size);
	u64 low = tcti_read_gpr_or_zero(regs, decoded->rm, access_size);
	u64 result;

	if (lsb >= data_size)
		return -EINVAL;

	high &= mask;
	low &= mask;
	result = lsb ? (low >> lsb) | (high << (data_size - lsb)) : low;
	tcti_write_gpr_or_zero(regs, decoded->rd, access_size, result & mask);
	regs->pc += sizeof(u32);
	return 0;
}

static int tcti_execute_data_processing_2source(struct pt_regs *regs,
						const struct tcti_decoded_instruction *decoded)
{
	u8 data_size = decoded->is_64bit ? 64 : 32;
	u8 access_size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
	u64 left = tcti_read_gpr_or_zero(regs, decoded->rn, access_size);
	u64 right = tcti_read_gpr_or_zero(regs, decoded->rm, access_size);
	u8 amount = right & (data_size - 1);
	u64 mask = tcti_ones_mask(data_size);
	u64 result;

	switch (decoded->dp2_op) {
	case TCTI_DP2_UDIV:
		result = right ? left / right : 0;
		break;
	case TCTI_DP2_SDIV:
		if (!right) {
			result = 0;
		} else if (decoded->is_64bit) {
			s64 dividend = left;
			s64 divisor = right;

			if (dividend == S64_MIN && divisor == -1)
				result = (u64)dividend;
			else
				result = dividend / divisor;
		} else {
			s32 dividend = (s32)(u32)left;
			s32 divisor = (s32)(u32)right;

			if (dividend == S32_MIN && divisor == -1)
				result = (u32)dividend;
			else
				result = (u32)(dividend / divisor);
		}
		break;
	case TCTI_DP2_LSLV:
		result = left << amount;
		break;
	case TCTI_DP2_LSRV:
		result = left >> amount;
		break;
	case TCTI_DP2_ASRV:
		result = decoded->is_64bit ?
			 (u64)((s64)left >> amount) :
			 (u32)((s32)(u32)left >> amount);
		break;
	case TCTI_DP2_RORV:
		result = decoded->is_64bit ? ror64(left, amount) :
					     ror32(left, amount);
		break;
	default:
		return -EINVAL;
	}

	tcti_write_gpr_or_zero(regs, decoded->rd, access_size, result & mask);
	regs->pc += sizeof(u32);
	return 0;
}

static int tcti_execute_data_processing_1source(struct pt_regs *regs,
						const struct tcti_decoded_instruction *decoded)
{
	u8 data_size = decoded->is_64bit ? 64 : 32;
	u8 access_size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
	u64 value = tcti_read_gpr_or_zero(regs, decoded->rn, access_size);
	u64 mask = tcti_ones_mask(data_size);
	u64 result;
	u8 bit;

	switch (decoded->dp1_op) {
	case TCTI_DP1_CLZ:
		value &= mask;
		result = value ? data_size - fls64(value) : data_size;
		break;
	case TCTI_DP1_RBIT:
		value &= mask;
		result = 0;
		for (bit = 0; bit < data_size; bit++)
			result = (result << 1) | ((value >> bit) & 1);
		break;
	case TCTI_DP1_REV:
		if (decoded->is_64bit)
			return -EOPNOTSUPP;
		value &= GENMASK(31, 0);
		result = ((value & GENMASK(7, 0)) << 24) |
			 ((value & GENMASK(15, 8)) << 8) |
			 ((value >> 8) & GENMASK(15, 8)) |
			 ((value >> 24) & GENMASK(7, 0));
		break;
	case TCTI_DP1_REV16:
		if (decoded->is_64bit)
			return -EOPNOTSUPP;
		value &= GENMASK(31, 0);
		result = ((value & GENMASK(7, 0)) << 8) |
			 ((value & GENMASK(15, 8)) >> 8) |
			 ((value & GENMASK(23, 16)) << 8) |
			 ((value & GENMASK(31, 24)) >> 8);
		break;
	default:
		return -EINVAL;
	}

	tcti_write_gpr_or_zero(regs, decoded->rd, access_size, result);
	regs->pc += sizeof(u32);
	return 0;
}

static void tcti_set_nzcv_from_immediate(struct pt_regs *regs, u8 nzcv)
{
	regs->pstate &= ~(PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT);
	if (nzcv & BIT(3))
		regs->pstate |= PSR_N_BIT;
	if (nzcv & BIT(2))
		regs->pstate |= PSR_Z_BIT;
	if (nzcv & BIT(1))
		regs->pstate |= PSR_C_BIT;
	if (nzcv & BIT(0))
		regs->pstate |= PSR_V_BIT;
}

static int tcti_execute_conditional_compare(struct pt_regs *regs,
					    const struct tcti_decoded_instruction *decoded)
{
	u8 access_size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
	u64 left = tcti_read_gpr_or_zero(regs, decoded->rn, access_size);
	u64 right = decoded->immediate ?
		    decoded->imm12 :
		    tcti_read_gpr_or_zero(regs, decoded->rm, access_size);
	u64 result;

	if (!tcti_condition_passed(regs, decoded->condition)) {
		tcti_set_nzcv_from_immediate(regs, decoded->nzcv);
		regs->pc += sizeof(u32);
		return 0;
	}

	result = decoded->subtract ? left - right : left + right;
	tcti_update_add_sub_flags(regs, left, right, result, access_size,
				  decoded->subtract);
	regs->pc += sizeof(u32);
	return 0;
}

static int tcti_execute_multiply_add_sub(struct pt_regs *regs,
					 const struct tcti_decoded_instruction *decoded)
{
	u8 access_size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
	u64 left;
	u64 right;
	u64 accumulator;
	u64 product;
	u64 result;

	switch (decoded->mul_op) {
	case TCTI_MUL_MADD:
	case TCTI_MUL_MSUB:
		left = tcti_read_gpr_or_zero(regs, decoded->rn, access_size);
		right = tcti_read_gpr_or_zero(regs, decoded->rm, access_size);
		accumulator = tcti_read_gpr_or_zero(regs, decoded->ra,
						    access_size);
		product = left * right;
		result = decoded->mul_op == TCTI_MUL_MSUB ?
			 accumulator - product : accumulator + product;
		tcti_write_gpr_or_zero(regs, decoded->rd, access_size, result);
		break;
	case TCTI_MUL_SMADDL:
	case TCTI_MUL_SMSUBL:
		left = (s64)(s32)tcti_read_gpr_or_zero(regs, decoded->rn,
						       sizeof(u32));
		right = (s64)(s32)tcti_read_gpr_or_zero(regs, decoded->rm,
							sizeof(u32));
		accumulator = tcti_read_gpr_or_zero(regs, decoded->ra,
						    sizeof(u64));
		product = left * right;
		result = decoded->mul_op == TCTI_MUL_SMSUBL ?
			 accumulator - product : accumulator + product;
		tcti_write_gpr_or_zero(regs, decoded->rd, sizeof(u64), result);
		break;
	case TCTI_MUL_SMULH:
		left = tcti_read_gpr_or_zero(regs, decoded->rn, sizeof(u64));
		right = tcti_read_gpr_or_zero(regs, decoded->rm, sizeof(u64));
		result = (u64)(((__int128)(s64)left * (s64)right) >> 64);
		tcti_write_gpr_or_zero(regs, decoded->rd, sizeof(u64), result);
		break;
	case TCTI_MUL_UMADDL:
	case TCTI_MUL_UMSUBL:
		left = (u32)tcti_read_gpr_or_zero(regs, decoded->rn,
						  sizeof(u32));
		right = (u32)tcti_read_gpr_or_zero(regs, decoded->rm,
						   sizeof(u32));
		accumulator = tcti_read_gpr_or_zero(regs, decoded->ra,
						    sizeof(u64));
		product = left * right;
		result = decoded->mul_op == TCTI_MUL_UMSUBL ?
			 accumulator - product : accumulator + product;
		tcti_write_gpr_or_zero(regs, decoded->rd, sizeof(u64), result);
		break;
	case TCTI_MUL_UMULH:
		left = tcti_read_gpr_or_zero(regs, decoded->rn, sizeof(u64));
		right = tcti_read_gpr_or_zero(regs, decoded->rm, sizeof(u64));
		result = (u64)(((unsigned __int128)left * right) >> 64);
		tcti_write_gpr_or_zero(regs, decoded->rd, sizeof(u64), result);
		break;
	default:
		return -EINVAL;
	}

	regs->pc += sizeof(u32);
	return 0;
}

static int tcti_execute_conditional_select(struct pt_regs *regs,
					   const struct tcti_decoded_instruction *decoded)
{
	u8 access_size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
	u64 result;

	if (tcti_condition_passed(regs, decoded->condition)) {
		result = tcti_read_gpr_or_zero(regs, decoded->rn, access_size);
	} else {
		result = tcti_read_gpr_or_zero(regs, decoded->rm, access_size);
		switch (decoded->conditional_select_op) {
		case TCTI_CONDITIONAL_SELECT_CSEL:
			break;
		case TCTI_CONDITIONAL_SELECT_CSINC:
			result++;
			break;
		case TCTI_CONDITIONAL_SELECT_CSINV:
			result = ~result;
			break;
		case TCTI_CONDITIONAL_SELECT_CSNEG:
			result = -result;
			break;
		default:
			return -EINVAL;
		}
	}

	tcti_write_gpr_or_zero(regs, decoded->rd, access_size, result);
	regs->pc += sizeof(u32);
	return 0;
}

static int tcti_execute_move_wide_immediate(struct pt_regs *regs,
					    const struct tcti_decoded_instruction *decoded)
{
	u8 access_size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
	u64 immediate = (u64)decoded->imm16 << decoded->halfword_shift;
	u64 mask = 0xffffULL << decoded->halfword_shift;
	u64 result;

	switch (decoded->move_wide_op) {
	case TCTI_MOVE_WIDE_MOVN:
		result = ~immediate;
		break;
	case TCTI_MOVE_WIDE_MOVZ:
		result = immediate;
		break;
	case TCTI_MOVE_WIDE_MOVK:
		result = tcti_read_gpr_or_zero(regs, decoded->rd,
					       access_size);
		result = (result & ~mask) | immediate;
		break;
	default:
		return -EINVAL;
	}

	tcti_write_gpr_or_zero(regs, decoded->rd, access_size, result);
	regs->pc += sizeof(u32);
	return 0;
}

static bool tcti_condition_passed(const struct pt_regs *regs, u8 condition)
{
	bool n = regs->pstate & PSR_N_BIT;
	bool z = regs->pstate & PSR_Z_BIT;
	bool c = regs->pstate & PSR_C_BIT;
	bool v = regs->pstate & PSR_V_BIT;

	switch (condition) {
	case 0:
		return z;
	case 1:
		return !z;
	case 2:
		return c;
	case 3:
		return !c;
	case 4:
		return n;
	case 5:
		return !n;
	case 6:
		return v;
	case 7:
		return !v;
	case 8:
		return c && !z;
	case 9:
		return !c || z;
	case 10:
		return n == v;
	case 11:
		return n != v;
	case 12:
		return !z && n == v;
	case 13:
		return z || n != v;
	case 14:
	case 15:
		return true;
	default:
		return false;
	}
}

static int tcti_execute_system_register(struct pt_regs *regs,
					const struct tcti_decoded_instruction *decoded)
{
	u64 value;

	if (decoded->rt == 31 && decoded->system_register_write)
		value = 0;
	else
		value = tcti_read_gpr_or_zero(regs, decoded->rt, sizeof(u64));

	switch (decoded->system_register) {
	case TCTI_SYSTEM_REGISTER_TPIDR_EL0:
		if (decoded->system_register_write) {
#if defined(ORLIX_APP_HOSTED_BOOT)
			orlix_hosted_set_current_user_tls(value);
#else
			current->thread.user_tls = value;
#endif
		} else if (decoded->rt != 31) {
			regs->regs[decoded->rt] = current->thread.user_tls;
		}
		break;
	case TCTI_SYSTEM_REGISTER_NZCV:
		if (decoded->system_register_write) {
			regs->pstate &= ~(PSR_N_BIT | PSR_Z_BIT |
					  PSR_C_BIT | PSR_V_BIT);
			regs->pstate |= value & (PSR_N_BIT | PSR_Z_BIT |
						 PSR_C_BIT | PSR_V_BIT);
		} else if (decoded->rt != 31) {
			regs->regs[decoded->rt] =
				regs->pstate & (PSR_N_BIT | PSR_Z_BIT |
						PSR_C_BIT | PSR_V_BIT);
		}
		break;
	case TCTI_SYSTEM_REGISTER_FPCR:
		if (decoded->system_register_write)
			current->thread.user_fpcr = value & GENMASK(31, 0);
		else if (decoded->rt != 31)
			regs->regs[decoded->rt] = current->thread.user_fpcr;
		break;
	case TCTI_SYSTEM_REGISTER_FPSR:
		if (decoded->system_register_write)
			current->thread.user_fpsr = value & GENMASK(31, 0);
		else if (decoded->rt != 31)
			regs->regs[decoded->rt] = current->thread.user_fpsr;
		break;
	default:
		return -EINVAL;
	}

	regs->pc += sizeof(u32);
	return 0;
}

static u64 tcti_memory_base(const struct pt_regs *regs, u8 rn)
{
	return rn == 31 ? regs->sp : regs->regs[rn];
}

static void tcti_write_memory_base(struct pt_regs *regs, u8 rn, u64 value)
{
	if (rn == 31)
		regs->sp = value;
	else
		regs->regs[rn] = value;
}

static int tcti_store_integer(struct mm_struct *mm, unsigned long address,
			      u8 access_size, u64 value)
{
	u8 buffer[sizeof(u64)];

	switch (access_size) {
	case sizeof(u8):
		buffer[0] = value;
		break;
	case sizeof(u16):
		put_unaligned_le16(value, buffer);
		break;
	case sizeof(u32):
		put_unaligned_le32(value, buffer);
		break;
	case sizeof(u64):
		put_unaligned_le64(value, buffer);
		break;
	default:
		return -EINVAL;
	}

	return tcti_write_user_data(mm, address, buffer, access_size);
}

static int tcti_load_integer(struct mm_struct *mm, unsigned long address,
			     u8 access_size, u64 *value)
{
	u8 buffer[sizeof(u64)] = {};
	int ret;

	if (!value)
		return -EINVAL;

	ret = tcti_read_user_data(mm, address, buffer, access_size);
	if (ret)
		return ret;

	switch (access_size) {
	case sizeof(u8):
		*value = buffer[0];
		return 0;
	case sizeof(u16):
		*value = get_unaligned_le16(buffer);
		return 0;
	case sizeof(u32):
		*value = get_unaligned_le32(buffer);
		return 0;
	case sizeof(u64):
		*value = get_unaligned_le64(buffer);
		return 0;
	default:
		return -EINVAL;
	}
}

static int tcti_store_simd_fp(struct mm_struct *mm, unsigned long address,
			      u8 reg, u8 access_size)
{
	u8 buffer[2 * sizeof(u64)];

	if (access_size != sizeof(u32) &&
	    access_size != sizeof(u64) &&
	    access_size != 2 * sizeof(u64))
		return -EOPNOTSUPP;

	if (access_size == sizeof(u32))
		put_unaligned_le32(current->thread.user_simd[reg * 2], buffer);
	else
		put_unaligned_le64(current->thread.user_simd[reg * 2], buffer);
	if (access_size == 2 * sizeof(u64))
		put_unaligned_le64(current->thread.user_simd[reg * 2 + 1],
				   buffer + sizeof(u64));
	return tcti_write_user_data(mm, address, buffer, access_size);
}

static void tcti_write_simd_fp_register(u8 reg, u8 access_size, u64 low,
					u64 high)
{
	current->thread.user_simd[reg * 2] = low;
	current->thread.user_simd[reg * 2 + 1] =
		access_size == 2 * sizeof(u64) ? high : 0;
	current->thread.user_simd_valid = 1;
}

static int tcti_read_simd_fp(struct mm_struct *mm, unsigned long address,
			     u8 access_size, u64 *low, u64 *high)
{
	u8 buffer[2 * sizeof(u64)] = {};
	int ret;

	if (access_size != sizeof(u32) &&
	    access_size != sizeof(u64) &&
	    access_size != 2 * sizeof(u64))
		return -EOPNOTSUPP;

	ret = tcti_read_user_data(mm, address, buffer, access_size);
	if (ret)
		return ret;

	*low = access_size == sizeof(u32) ?
		get_unaligned_le32(buffer) : get_unaligned_le64(buffer);
	*high = access_size == 2 * sizeof(u64) ?
			get_unaligned_le64(buffer + sizeof(u64)) : 0;
	return 0;
}

static int tcti_load_simd_fp(struct mm_struct *mm, unsigned long address,
			     u8 reg, u8 access_size)
{
	u64 low;
	u64 high;
	int ret;

	ret = tcti_read_simd_fp(mm, address, access_size, &low, &high);
	if (ret)
		return ret;

	tcti_write_simd_fp_register(reg, access_size, low, high);
	return 0;
}

static int tcti_execute_simd_load_replicate(
	struct mm_struct *mm, struct pt_regs *regs,
	const struct tcti_decoded_instruction *decoded,
	unsigned long *fault_address)
{
	unsigned long address = tcti_memory_base(regs, decoded->rn);
	u64 value;
	u64 packed;
	int ret;

	if (!mm)
		return -EINVAL;
	if (!decoded->load || !decoded->simd_fp ||
	    decoded->access_size != sizeof(u32) ||
	    decoded->result_size != 2 * sizeof(u64))
		return -EOPNOTSUPP;
	if (fault_address)
		*fault_address = address;

	ret = tcti_load_integer(mm, address, decoded->access_size, &value);
	if (ret)
		return ret;

	value &= GENMASK_ULL(31, 0);
	packed = value | (value << 32);
	tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
				    packed, packed);
	regs->pc += sizeof(u32);
	return 0;
}

static u64 tcti_extend_loaded_integer(u64 value,
				      const struct tcti_decoded_instruction *decoded)
{
	if (!decoded->sign_extend_load)
		return value;

	switch (decoded->access_size) {
	case sizeof(u8):
		return sign_extend64(value, 7);
	case sizeof(u16):
		return sign_extend64(value, 15);
	case sizeof(u32):
		return sign_extend64(value, 31);
	default:
		return value;
	}
}

static unsigned long tcti_indexed_address(const struct pt_regs *regs,
					  const struct tcti_decoded_instruction *decoded)
{
	u64 base = tcti_memory_base(regs, decoded->rn);

	if (decoded->memory_index_mode == TCTI_MEMORY_INDEX_POST)
		return base;

	return base + decoded->memory_offset;
}

static void tcti_apply_memory_writeback(struct pt_regs *regs,
					const struct tcti_decoded_instruction *decoded)
{
	u64 base;

	if (decoded->memory_index_mode == TCTI_MEMORY_INDEX_SIGNED_OFFSET)
		return;

	base = tcti_memory_base(regs, decoded->rn);
	tcti_write_memory_base(regs, decoded->rn, base + decoded->memory_offset);
}

static int tcti_execute_load_store_pair(struct mm_struct *mm,
					struct pt_regs *regs,
					const struct tcti_decoded_instruction *decoded,
					unsigned long *fault_address)
{
	unsigned long address = tcti_indexed_address(regs, decoded);
	u64 first;
	u64 second;
	int ret;

	if (!mm)
		return -EINVAL;
	if (fault_address)
		*fault_address = address;

	if (decoded->load) {
		if (decoded->simd_fp) {
			u64 first_low;
			u64 first_high;
			u64 second_low;
			u64 second_high;

			ret = tcti_read_simd_fp(mm, address,
						decoded->access_size,
						&first_low, &first_high);
			if (ret)
				return ret;
			ret = tcti_read_simd_fp(mm,
						address + decoded->access_size,
						decoded->access_size,
						&second_low, &second_high);
			if (ret)
				return ret;
			tcti_write_simd_fp_register(decoded->rt,
						    decoded->access_size,
						    first_low, first_high);
			tcti_write_simd_fp_register(decoded->rt2,
						    decoded->access_size,
						    second_low, second_high);
		} else {
			ret = tcti_load_integer(mm, address, decoded->access_size,
						&first);
			if (ret)
				return ret;
			ret = tcti_load_integer(mm, address + decoded->access_size,
						decoded->access_size, &second);
			if (ret)
				return ret;
			first = tcti_extend_loaded_integer(first, decoded);
			second = tcti_extend_loaded_integer(second, decoded);
			tcti_write_gpr_or_zero(regs, decoded->rt,
					       decoded->result_size, first);
			tcti_write_gpr_or_zero(regs, decoded->rt2,
					       decoded->result_size, second);
		}
	} else {
		if (decoded->simd_fp) {
			ret = tcti_store_simd_fp(mm, address, decoded->rt,
						 decoded->access_size);
			if (ret)
				return ret;
			ret = tcti_store_simd_fp(mm, address + decoded->access_size,
						 decoded->rt2,
						 decoded->access_size);
			if (ret)
				return ret;
		} else {
			first = tcti_read_gpr_or_zero(regs, decoded->rt,
						      decoded->access_size);
			second = tcti_read_gpr_or_zero(regs, decoded->rt2,
						       decoded->access_size);
			ret = tcti_store_integer(mm, address, decoded->access_size,
						 first);
			if (ret)
				return ret;
			ret = tcti_store_integer(mm, address + decoded->access_size,
						 decoded->access_size, second);
			if (ret)
				return ret;
		}
	}

	tcti_apply_memory_writeback(regs, decoded);
	regs->pc += sizeof(u32);
	return 0;
}

static int tcti_execute_load_store_immediate(struct mm_struct *mm,
					     struct pt_regs *regs,
					     const struct tcti_decoded_instruction *decoded,
					     unsigned long *fault_address)
{
	unsigned long address = tcti_indexed_address(regs, decoded);
	u64 value;
	int ret;

	if (!mm)
		return -EINVAL;
	if (fault_address)
		*fault_address = address;

	if (decoded->load) {
		if (decoded->simd_fp) {
			ret = tcti_load_simd_fp(mm, address, decoded->rt,
						decoded->access_size);
			if (ret)
				return ret;
			tcti_apply_memory_writeback(regs, decoded);
			regs->pc += sizeof(u32);
			return 0;
		}

		ret = tcti_load_integer(mm, address, decoded->access_size, &value);
		if (ret)
			return ret;
		value = tcti_extend_loaded_integer(value, decoded);
		tcti_write_gpr_or_zero(regs, decoded->rt,
				       decoded->result_size, value);
	} else {
		if (decoded->simd_fp) {
			ret = tcti_store_simd_fp(mm, address, decoded->rt,
						 decoded->access_size);
			if (ret)
				return ret;
			tcti_apply_memory_writeback(regs, decoded);
			regs->pc += sizeof(u32);
			return 0;
		}

		value = tcti_read_gpr_or_zero(regs, decoded->rt,
					      decoded->access_size);
		ret = tcti_store_integer(mm, address, decoded->access_size, value);
		if (ret)
			return ret;
	}

	tcti_apply_memory_writeback(regs, decoded);
	regs->pc += sizeof(u32);
	return 0;
}

static s64 tcti_register_offset(const struct pt_regs *regs,
				const struct tcti_decoded_instruction *decoded)
{
	u64 value = tcti_read_gpr_or_zero(regs, decoded->rm, sizeof(u64));

	switch (decoded->offset_extend) {
	case 2:
		value = (u32)value;
		break;
	case 3:
		break;
	case 6:
		value = (s64)(s32)value;
		break;
	case 7:
		value = (s64)value;
		break;
	default:
		return 0;
	}

	if (decoded->offset_shift)
		value <<= ilog2(decoded->access_size);

	return (s64)value;
}

static int tcti_execute_load_store_register_offset(struct mm_struct *mm,
						  struct pt_regs *regs,
						  const struct tcti_decoded_instruction *decoded,
						  unsigned long *fault_address)
{
	unsigned long address = tcti_memory_base(regs, decoded->rn) +
				tcti_register_offset(regs, decoded);
	u64 value;
	int ret;

	if (!mm)
		return -EINVAL;
	if (fault_address)
		*fault_address = address;

	if (decoded->load) {
		if (decoded->simd_fp) {
			ret = tcti_load_simd_fp(mm, address, decoded->rt,
						decoded->access_size);
			if (ret)
				return ret;
			regs->pc += sizeof(u32);
			return 0;
		}

		ret = tcti_load_integer(mm, address, decoded->access_size, &value);
		if (ret)
			return ret;
		value = tcti_extend_loaded_integer(value, decoded);
		tcti_write_gpr_or_zero(regs, decoded->rt,
				       decoded->result_size, value);
	} else {
		if (decoded->simd_fp) {
			ret = tcti_store_simd_fp(mm, address, decoded->rt,
						 decoded->access_size);
			if (ret)
				return ret;
			regs->pc += sizeof(u32);
			return 0;
		}

		value = tcti_read_gpr_or_zero(regs, decoded->rt,
					      decoded->access_size);
		ret = tcti_store_integer(mm, address, decoded->access_size, value);
		if (ret)
			return ret;
	}

	regs->pc += sizeof(u32);
	return 0;
}

static void tcti_clear_exclusive_monitor(void)
{
	current->thread.user_exclusive_address = 0;
	current->thread.user_exclusive_size = 0;
	current->thread.user_exclusive_valid = 0;
}

static int tcti_execute_load_store_exclusive(struct mm_struct *mm,
					    struct pt_regs *regs,
					    const struct tcti_decoded_instruction *decoded,
					    unsigned long *fault_address)
{
	unsigned long address = tcti_memory_base(regs, decoded->rn);
	u64 value;
	int ret;

	if (fault_address)
		*fault_address = address;

	if (decoded->load) {
		if (!mm)
			return -EINVAL;
		ret = tcti_load_integer(mm, address, decoded->access_size,
					&value);
		if (ret)
			return ret;
		value = tcti_extend_loaded_integer(value, decoded);
		tcti_write_gpr_or_zero(regs, decoded->rt,
				       decoded->result_size, value);
		if (decoded->exclusive) {
			current->thread.user_exclusive_address = address;
			current->thread.user_exclusive_size =
				decoded->access_size;
			current->thread.user_exclusive_valid = 1;
		}
		if (decoded->acquire)
			smp_mb();
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->release)
		smp_mb();

	if (decoded->exclusive &&
	    (!current->thread.user_exclusive_valid ||
	     current->thread.user_exclusive_address != address ||
	     current->thread.user_exclusive_size != decoded->access_size)) {
		tcti_write_gpr_or_zero(regs, decoded->rs, sizeof(u32), 1);
		tcti_clear_exclusive_monitor();
		regs->pc += sizeof(u32);
		return 0;
	}

	if (!mm)
		return -EINVAL;

	value = tcti_read_gpr_or_zero(regs, decoded->rt, decoded->access_size);
	ret = tcti_store_integer(mm, address, decoded->access_size, value);
	if (ret)
		return ret;

	if (decoded->exclusive) {
		tcti_write_gpr_or_zero(regs, decoded->rs, sizeof(u32), 0);
		tcti_clear_exclusive_monitor();
	}
	regs->pc += sizeof(u32);
	return 0;
}

static int tcti_execute_simd_modified_immediate(
	struct pt_regs *regs, const struct tcti_decoded_instruction *decoded)
{
	u64 low = current->thread.user_simd[decoded->rd * 2];
	u64 high = current->thread.user_simd[decoded->rd * 2 + 1];
	u64 immediate = decoded->logical_immediate;

	switch (decoded->simd_modified_immediate_op) {
	case TCTI_SIMD_MODIMM_MOVI:
		low = immediate;
		high = immediate;
		break;
	case TCTI_SIMD_MODIMM_MVNI:
		low = ~immediate;
		high = ~immediate;
		break;
	case TCTI_SIMD_MODIMM_ORR:
		low |= immediate;
		high |= immediate;
		break;
	case TCTI_SIMD_MODIMM_BIC:
		low &= ~immediate;
		high &= ~immediate;
		break;
	default:
		return -EINVAL;
	}

	current->thread.user_simd[decoded->rd * 2] = low;
	current->thread.user_simd[decoded->rd * 2 + 1] =
		decoded->result_size > sizeof(u64) ? high : 0;
	current->thread.user_simd_valid = 1;
	regs->pc += sizeof(u32);
	return 0;
}

static int tcti_execute_simd_vector_element_move(
	struct pt_regs *regs, const struct tcti_decoded_instruction *decoded)
{
	u64 value;
	u64 word;
	u64 mask;
	u8 source_word;
	u8 destination_word;
	u8 source_shift;
	u8 destination_shift;

	if (decoded->simd_element_move_op == TCTI_SIMD_ELEMENT_MOVE_EXT) {
		u8 buffer[4 * sizeof(u64)];
		u8 result[2 * sizeof(u64)] = {};
		u64 low;
		u64 high = 0;

		if ((decoded->access_size != sizeof(u64) &&
		     decoded->access_size != 2 * sizeof(u64)) ||
		    decoded->result_size != decoded->access_size ||
		    decoded->shift_amount >= decoded->result_size)
			return -EOPNOTSUPP;

		put_unaligned_le64(current->thread.user_simd[decoded->rn * 2],
				   buffer);
		if (decoded->result_size == sizeof(u64)) {
			put_unaligned_le64(
				current->thread.user_simd[decoded->rm * 2],
				buffer + sizeof(u64));
		} else {
			put_unaligned_le64(
				current->thread.user_simd[decoded->rn * 2 + 1],
				buffer + sizeof(u64));
			put_unaligned_le64(
				current->thread.user_simd[decoded->rm * 2],
				buffer + 2 * sizeof(u64));
			put_unaligned_le64(
				current->thread.user_simd[decoded->rm * 2 + 1],
				buffer + 3 * sizeof(u64));
		}
		memcpy(result, buffer + decoded->shift_amount,
		       decoded->result_size);
		low = get_unaligned_le64(result);
		if (decoded->result_size == 2 * sizeof(u64))
			high = get_unaligned_le64(result + sizeof(u64));
		tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    low, high);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_element_move_op == TCTI_SIMD_ELEMENT_MOVE_INS_GPR) {
		u8 byte_offset;

		if (decoded->access_size != sizeof(u8) &&
		    decoded->access_size != sizeof(u16) &&
		    decoded->access_size != sizeof(u32) &&
		    decoded->access_size != sizeof(u64))
			return -EOPNOTSUPP;

		byte_offset = decoded->simd_destination_index *
			      decoded->access_size;
		if (byte_offset + decoded->access_size > 2 * sizeof(u64))
			return -EOPNOTSUPP;

		destination_word = decoded->rd * 2 + byte_offset / sizeof(u64);
		destination_shift = (byte_offset % sizeof(u64)) * 8;
		value = tcti_read_gpr_or_zero(regs, decoded->rn,
					      decoded->access_size);
		value &= GENMASK_ULL(decoded->access_size * 8 - 1, 0);
		mask = GENMASK_ULL(decoded->access_size * 8 - 1, 0)
		       << destination_shift;
		word = current->thread.user_simd[destination_word];
		word = (word & ~mask) | (value << destination_shift);
		current->thread.user_simd[destination_word] = word;
		current->thread.user_simd_valid = 1;
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_element_move_op == TCTI_SIMD_ELEMENT_MOVE_UMOV) {
		u8 byte_offset;

		if (!(((decoded->access_size == sizeof(u8) ||
		        decoded->access_size == sizeof(u16) ||
		        decoded->access_size == sizeof(u32)) &&
		       decoded->result_size == sizeof(u32)) ||
		      (decoded->access_size == sizeof(u64) &&
		       decoded->result_size == sizeof(u64))))
			return -EOPNOTSUPP;

		byte_offset = decoded->simd_source_index * decoded->access_size;
		if (byte_offset + decoded->access_size > 2 * sizeof(u64))
			return -EOPNOTSUPP;

		source_word = decoded->rn * 2 + byte_offset / sizeof(u64);
		source_shift = (byte_offset % sizeof(u64)) * 8;
		value = (current->thread.user_simd[source_word] >> source_shift) &
			GENMASK_ULL(decoded->access_size * 8 - 1, 0);
		tcti_write_gpr_or_zero(regs, decoded->rd, decoded->result_size,
				       value);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_element_move_op >= TCTI_SIMD_ELEMENT_MOVE_UZP1 &&
	    decoded->simd_element_move_op <= TCTI_SIMD_ELEMENT_MOVE_ZIP2) {
		u64 source[4];
		u64 result[2] = {};
		u8 lane_count;
		u8 half;
		u8 destination_lane;

		if ((decoded->access_size != sizeof(u8) &&
		     decoded->access_size != sizeof(u16) &&
		     decoded->access_size != sizeof(u32) &&
		     decoded->access_size != sizeof(u64)) ||
		    (decoded->result_size != sizeof(u64) &&
		     decoded->result_size != 2 * sizeof(u64)))
			return -EOPNOTSUPP;

		source[0] = current->thread.user_simd[decoded->rn * 2];
		source[1] = current->thread.user_simd[decoded->rn * 2 + 1];
		source[2] = current->thread.user_simd[decoded->rm * 2];
		source[3] = current->thread.user_simd[decoded->rm * 2 + 1];
		lane_count = decoded->result_size / decoded->access_size;
		half = lane_count / 2;

		for (destination_lane = 0; destination_lane < lane_count;
		     destination_lane++) {
			u8 source_vector;
			u8 source_lane;
			u8 source_byte;
			u8 source_word;
			u8 source_shift;
			u8 destination_byte = destination_lane *
					      decoded->access_size;
			u8 destination_word = destination_byte / sizeof(u64);
			u8 destination_shift =
				(destination_byte % sizeof(u64)) * 8;
			u64 mask = GENMASK_ULL(decoded->access_size * 8 - 1, 0);
			u64 lane_value;

			switch (decoded->simd_element_move_op) {
			case TCTI_SIMD_ELEMENT_MOVE_UZP1:
			case TCTI_SIMD_ELEMENT_MOVE_UZP2:
				source_vector = destination_lane >= half;
				source_lane = (destination_lane % half) * 2 +
					(decoded->simd_element_move_op ==
					 TCTI_SIMD_ELEMENT_MOVE_UZP2);
				break;
			case TCTI_SIMD_ELEMENT_MOVE_TRN1:
			case TCTI_SIMD_ELEMENT_MOVE_TRN2:
				source_vector = destination_lane & 1U;
				source_lane = (destination_lane / 2) * 2 +
					(decoded->simd_element_move_op ==
					 TCTI_SIMD_ELEMENT_MOVE_TRN2);
				break;
			case TCTI_SIMD_ELEMENT_MOVE_ZIP1:
			case TCTI_SIMD_ELEMENT_MOVE_ZIP2:
				source_vector = destination_lane & 1U;
				source_lane = destination_lane / 2 +
					(decoded->simd_element_move_op ==
					 TCTI_SIMD_ELEMENT_MOVE_ZIP2 ? half : 0);
				break;
			default:
				return -EOPNOTSUPP;
			}

			source_byte = source_lane * decoded->access_size;
			source_word = source_vector * 2 +
				      source_byte / sizeof(u64);
			source_shift = (source_byte % sizeof(u64)) * 8;
			lane_value = (source[source_word] >> source_shift) & mask;
			result[destination_word] |= lane_value << destination_shift;
		}

		tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_element_move_op == TCTI_SIMD_ELEMENT_MOVE_SSHLL ||
	    decoded->simd_element_move_op == TCTI_SIMD_ELEMENT_MOVE_USHLL) {
		u64 low = 0;
		u64 high = 0;
		u8 lane_count;
		u8 lane;

		if ((decoded->access_size != sizeof(u8) &&
		     decoded->access_size != sizeof(u16) &&
		     decoded->access_size != sizeof(u32)) ||
		    decoded->result_size != 2 * sizeof(u64))
			return -EOPNOTSUPP;

		lane_count = sizeof(u64) / decoded->access_size;

		for (lane = 0; lane < lane_count; lane++) {
			u8 source_byte = (decoded->simd_source_index + lane) *
					 decoded->access_size;
			u8 source_word = source_byte / sizeof(u64);
			u8 source_shift = (source_byte % sizeof(u64)) * 8;
			u8 dest_byte = lane * decoded->access_size * 2;
			u8 dest_word = dest_byte / sizeof(u64);
			u8 dest_shift = (dest_byte % sizeof(u64)) * 8;
			u64 widened;
			u64 destination_mask =
				GENMASK_ULL(decoded->access_size * 16 - 1, 0);

			if (source_byte + decoded->access_size >
			    2 * sizeof(u64))
				return -EOPNOTSUPP;

			widened = (current->thread.user_simd[decoded->rn * 2 +
				   source_word] >> source_shift) &
				   GENMASK_ULL(decoded->access_size * 8 - 1, 0);
			if (decoded->simd_element_move_op ==
			    TCTI_SIMD_ELEMENT_MOVE_SSHLL)
				widened = sign_extend64(widened,
						 decoded->access_size * 8 - 1);
			widened = (widened << decoded->shift_amount) &
				  destination_mask;
			if (dest_word)
				high |= widened << dest_shift;
			else
				low |= widened << dest_shift;
		}
		tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    low, high);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_element_move_op == TCTI_SIMD_ELEMENT_MOVE_XTN) {
		u64 source[2];
		u64 narrowed;
		u8 lane_count;
		u8 lane;

		if (decoded->access_size != 2 * decoded->result_size ||
		    (decoded->result_size != sizeof(u8) &&
		     decoded->result_size != sizeof(u16) &&
		     decoded->result_size != sizeof(u32)))
			return -EOPNOTSUPP;

		source[0] = current->thread.user_simd[decoded->rn * 2];
		source[1] = current->thread.user_simd[decoded->rn * 2 + 1];
		lane_count = 2 * sizeof(u64) / decoded->access_size;
		narrowed = 0;
		for (lane = 0; lane < lane_count; lane++) {
			u8 source_byte = lane * decoded->access_size;
			u8 source_word = source_byte / sizeof(u64);
			u8 source_shift = (source_byte % sizeof(u64)) * 8;
			u8 destination_shift = lane * decoded->result_size * 8;
			u64 value = (source[source_word] >> source_shift) &
				    GENMASK_ULL(decoded->result_size * 8 - 1, 0);

			narrowed |= value << destination_shift;
		}
		if (decoded->simd_destination_index)
			tcti_write_simd_fp_register(
				decoded->rd, 2 * sizeof(u64),
				current->thread.user_simd[decoded->rd * 2], narrowed);
		else
			tcti_write_simd_fp_register(decoded->rd, sizeof(u64),
						    narrowed, 0);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->immediate) {
		u8 lane_bits;

		if ((decoded->access_size != sizeof(u8) &&
		     decoded->access_size != sizeof(u16) &&
		     decoded->access_size != sizeof(u32) &&
		     decoded->access_size != sizeof(u64)) ||
		    (decoded->result_size != sizeof(u64) &&
		     decoded->result_size != 2 * sizeof(u64)) ||
		    decoded->access_size > decoded->result_size)
			return -EOPNOTSUPP;

		lane_bits = decoded->access_size * 8;
		value = tcti_read_gpr_or_zero(regs, decoded->rn,
					      decoded->access_size);
		word = value & GENMASK_ULL(lane_bits - 1, 0);
		while (lane_bits < 64) {
			word |= word << lane_bits;
			lane_bits *= 2;
		}
		tcti_write_simd_fp_register(
			decoded->rd, decoded->result_size, word,
			decoded->result_size > sizeof(u64) ? word : 0);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->access_size == sizeof(u64) && decoded->rd == decoded->rn &&
	    decoded->simd_destination_index == decoded->simd_source_index) {
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->access_size == sizeof(u64)) {
		if (decoded->simd_source_index > 1 ||
		    decoded->simd_destination_index > 1)
			return -EOPNOTSUPP;

		source_word = decoded->rn * 2 + decoded->simd_source_index;
		destination_word = decoded->rd * 2 + decoded->simd_destination_index;
		current->thread.user_simd[destination_word] =
			current->thread.user_simd[source_word];
		current->thread.user_simd_valid = 1;
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->access_size != sizeof(u32) ||
	    decoded->simd_source_index > 3 ||
	    decoded->simd_destination_index > 3)
		return -EOPNOTSUPP;

	source_word = decoded->rn * 2 + decoded->simd_source_index / 2;
	destination_word = decoded->rd * 2 + decoded->simd_destination_index / 2;
	source_shift = (decoded->simd_source_index % 2) * 32;
	destination_shift = (decoded->simd_destination_index % 2) * 32;
	value = (current->thread.user_simd[source_word] >> source_shift) &
		GENMASK_ULL(31, 0);
	mask = GENMASK_ULL(31, 0) << destination_shift;
	word = current->thread.user_simd[destination_word];
	word = (word & ~mask) | (value << destination_shift);
	current->thread.user_simd[destination_word] = word;
	current->thread.user_simd_valid = 1;
	regs->pc += sizeof(u32);
	return 0;
}

static int tcti_execute_simd_vector_logical(
	struct pt_regs *regs, const struct tcti_decoded_instruction *decoded)
{
	u64 left_low;
	u64 left_high;
	u64 right_low;
	u64 right_high;
	u64 destination_low;
	u64 destination_high;
	u64 result_low;
	u64 result_high;

	if (decoded->logical_op > TCTI_LOGICAL_BIF ||
	    (decoded->access_size != sizeof(u64) &&
	     decoded->access_size != 2 * sizeof(u64)))
		return -EOPNOTSUPP;

	left_low = current->thread.user_simd[decoded->rn * 2];
	left_high = current->thread.user_simd[decoded->rn * 2 + 1];
	right_low = current->thread.user_simd[decoded->rm * 2];
	right_high = current->thread.user_simd[decoded->rm * 2 + 1];
	if (decoded->logical_op >= TCTI_LOGICAL_BSL) {
		destination_low = current->thread.user_simd[decoded->rd * 2];
		destination_high =
			current->thread.user_simd[decoded->rd * 2 + 1];
	}

	switch (decoded->logical_op) {
	case TCTI_LOGICAL_AND:
		result_low = left_low & right_low;
		result_high = left_high & right_high;
		break;
	case TCTI_LOGICAL_BIC:
		result_low = left_low & ~right_low;
		result_high = left_high & ~right_high;
		break;
	case TCTI_LOGICAL_ORR:
		result_low = left_low | right_low;
		result_high = left_high | right_high;
		break;
	case TCTI_LOGICAL_ORN:
		result_low = left_low | ~right_low;
		result_high = left_high | ~right_high;
		break;
	case TCTI_LOGICAL_EOR:
		result_low = left_low ^ right_low;
		result_high = left_high ^ right_high;
		break;
	case TCTI_LOGICAL_BSL:
		result_low = (left_low & destination_low) |
			     (right_low & ~destination_low);
		result_high = (left_high & destination_high) |
			      (right_high & ~destination_high);
		break;
	case TCTI_LOGICAL_BIT:
		result_low = (destination_low & ~right_low) |
			     (left_low & right_low);
		result_high = (destination_high & ~right_high) |
			      (left_high & right_high);
		break;
	case TCTI_LOGICAL_BIF:
		result_low = (destination_low & right_low) |
			     (left_low & ~right_low);
		result_high = (destination_high & right_high) |
			      (left_high & ~right_high);
		break;
	default:
		return -EOPNOTSUPP;
	}

	tcti_write_simd_fp_register(decoded->rd, decoded->access_size,
				    result_low, result_high);
	regs->pc += sizeof(u32);
	return 0;
}

static u64 tcti_simd_shift_lane(u64 value, u8 bits, s8 shift,
				bool is_unsigned, bool rounding,
				bool saturating, bool *saturated)
{
	u64 mask = bits == 64 ? ~0ULL : GENMASK_ULL(bits - 1, 0);
	s64 signed_value = sign_extend64(value & mask, bits - 1);

	value &= mask;
	if (shift < 0) {
		u8 right = -(int)shift;

		if (right >= bits) {
			if (rounding)
				return 0;
			return is_unsigned || signed_value >= 0 ? 0 : mask;
		}

		if (is_unsigned) {
			u64 result = value >> right;

			if (rounding)
				result += (value >> (right - 1)) & 1U;
			return result & mask;
		}

		{
			s64 result = signed_value >> right;

			if (rounding)
				result += (value >> (right - 1)) & 1U;
			return (u64)result & mask;
		}
	}

	if (!saturating)
		return shift >= bits ? 0 : (value << shift) & mask;

	if (is_unsigned) {
		if (shift >= bits ? value != 0 : value > (mask >> shift)) {
			*saturated = true;
			return mask;
		}
		return (value << shift) & mask;
	}

	{
		s64 maximum = bits == 64 ? S64_MAX : (1LL << (bits - 1)) - 1;
		s64 minimum = bits == 64 ? S64_MIN : -(1LL << (bits - 1));

		if (shift >= bits) {
			if (!signed_value)
				return 0;
			*saturated = true;
			return signed_value > 0 ? (u64)maximum & mask :
						 (u64)minimum & mask;
		}
		if (signed_value > (maximum >> shift)) {
			*saturated = true;
			return (u64)maximum & mask;
		}
		if (signed_value < (minimum >> shift)) {
			*saturated = true;
			return (u64)minimum & mask;
		}
		return ((u64)signed_value << shift) & mask;
	}
}

static u64 tcti_simd_saturating_add_sub_lane(u64 left, u64 right, u8 bits,
	bool is_unsigned, bool subtract, bool *saturated)
{
	u64 mask = GENMASK_ULL(bits - 1, 0);

	left &= mask;
	right &= mask;
	if (is_unsigned) {
		if (subtract) {
			if (left < right) {
				*saturated = true;
				return 0;
			}
			return left - right;
		}
		if (left > mask - right) {
			*saturated = true;
			return mask;
		}
		return left + right;
	}

	{
		s64 signed_left = sign_extend64(left, bits - 1);
		s64 signed_right = sign_extend64(right, bits - 1);
		s64 maximum = bits == 64 ? S64_MAX :
			(1LL << (bits - 1)) - 1;
		s64 minimum = bits == 64 ? S64_MIN :
			-(1LL << (bits - 1));

		if (subtract) {
			if (signed_right < 0 &&
			    signed_left > maximum + signed_right) {
				*saturated = true;
				return (u64)maximum & mask;
			}
			if (signed_right > 0 &&
			    signed_left < minimum + signed_right) {
				*saturated = true;
				return (u64)minimum & mask;
			}
			return (u64)(signed_left - signed_right) & mask;
		}
		if (signed_right > 0 &&
		    signed_left > maximum - signed_right) {
			*saturated = true;
			return (u64)maximum & mask;
		}
		if (signed_right < 0 &&
		    signed_left < minimum - signed_right) {
			*saturated = true;
			return (u64)minimum & mask;
		}
		return (u64)(signed_left + signed_right) & mask;
	}
}

static u64 tcti_simd_saturating_mul_high_lane(u64 left, u64 right, u8 bits,
	bool rounding, bool *saturated)
{
	u64 mask = GENMASK_ULL(bits - 1, 0);
	s64 signed_left = sign_extend64(left & mask, bits - 1);
	s64 signed_right = sign_extend64(right & mask, bits - 1);
	s64 minimum = -(1LL << (bits - 1));
	s64 maximum = (1LL << (bits - 1)) - 1;
	s64 product;

	if (signed_left == minimum && signed_right == minimum) {
		*saturated = true;
		return (u64)maximum;
	}
	product = signed_left * signed_right;
	if (rounding)
		product += 1LL << (bits - 2);
	return (u64)(product >> (bits - 1)) & mask;
}

static int tcti_execute_simd_vector_arithmetic(
	struct pt_regs *regs, const struct tcti_decoded_instruction *decoded)
{
	u64 left_low;
	u64 left_high;
	u64 right_low;
	u64 right_high;
	u8 lane;

	if (decoded->simd_arithmetic_op == TCTI_SIMD_ARITH_USRA) {
		u64 source_low;
		u64 source_high;
		u64 accumulator_low;
		u64 accumulator_high;
		u64 result_low = 0;
		u64 result_high = 0;

		if (decoded->access_size != sizeof(u32) ||
		    decoded->result_size != 2 * sizeof(u64) ||
		    decoded->shift_amount > 32)
			return -EOPNOTSUPP;

		source_low = current->thread.user_simd[decoded->rn * 2];
		source_high = current->thread.user_simd[decoded->rn * 2 + 1];
		accumulator_low = current->thread.user_simd[decoded->rd * 2];
		accumulator_high = current->thread.user_simd[decoded->rd * 2 + 1];
		for (lane = 0; lane < 4; lane++) {
			u64 source_word = lane < 2 ? source_low : source_high;
			u64 accumulator_word = lane < 2 ? accumulator_low :
					       accumulator_high;
			u64 source = (source_word >> ((lane % 2) * 32)) &
				     GENMASK_ULL(31, 0);
			u64 accumulator = (accumulator_word >> ((lane % 2) * 32)) &
					  GENMASK_ULL(31, 0);
			u64 shifted = decoded->shift_amount == 32 ?
				      0 : source >> decoded->shift_amount;
			u64 result = (accumulator + shifted) & GENMASK_ULL(31, 0);

			if (lane < 2)
				result_low |= result << (lane * 32);
			else
				result_high |= result << ((lane - 2) * 32);
		}
		tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    result_low, result_high);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op == TCTI_SIMD_ARITH_USHR) {
		u64 source_low;
		u64 source_high;
		u64 result_low = 0;
		u64 result_high = 0;

		if (decoded->access_size != sizeof(u32) ||
		    decoded->result_size != 2 * sizeof(u64) ||
		    decoded->shift_amount > 32)
			return -EOPNOTSUPP;

		source_low = current->thread.user_simd[decoded->rn * 2];
		source_high = current->thread.user_simd[decoded->rn * 2 + 1];
		for (lane = 0; lane < 4; lane++) {
			u64 source_word = lane < 2 ? source_low : source_high;
			u64 source = (source_word >> ((lane % 2) * 32)) &
				     GENMASK_ULL(31, 0);
			u64 result = decoded->shift_amount == 32 ?
				     0 : source >> decoded->shift_amount;

			if (lane < 2)
				result_low |= result << (lane * 32);
			else
				result_high |= result << ((lane - 2) * 32);
		}
		tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    result_low, result_high);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op >= TCTI_SIMD_ARITH_SSHL &&
	    decoded->simd_arithmetic_op <= TCTI_SIMD_ARITH_UQRSHL) {
		u64 source[2];
		u64 shifts[2];
		u64 result[2] = {};
		u8 lane_count;
		bool saturated = false;
		u8 operation = decoded->simd_arithmetic_op -
			       TCTI_SIMD_ARITH_SSHL;
		bool is_unsigned = operation & 1U;
		bool saturating = (operation / 2) & 1U;
		bool rounding = operation >= 4;

		if ((decoded->access_size != sizeof(u8) &&
		     decoded->access_size != sizeof(u16) &&
		     decoded->access_size != sizeof(u32) &&
		     decoded->access_size != sizeof(u64)) ||
		    (decoded->result_size != sizeof(u64) &&
		     decoded->result_size != 2 * sizeof(u64)) ||
		    decoded->access_size > decoded->result_size)
			return -EOPNOTSUPP;

		source[0] = current->thread.user_simd[decoded->rn * 2];
		source[1] = current->thread.user_simd[decoded->rn * 2 + 1];
		shifts[0] = current->thread.user_simd[decoded->rm * 2];
		shifts[1] = current->thread.user_simd[decoded->rm * 2 + 1];
		lane_count = decoded->result_size / decoded->access_size;
		for (lane = 0; lane < lane_count; lane++) {
			u8 byte = lane * decoded->access_size;
			u8 word = byte / sizeof(u64);
			u8 bit_shift = (byte % sizeof(u64)) * 8;
			u64 mask = GENMASK_ULL(decoded->access_size * 8 - 1, 0);
			u64 value = (source[word] >> bit_shift) & mask;
			s8 shift = (s8)((shifts[word] >> bit_shift) & 0xffU);
			u64 lane_result = tcti_simd_shift_lane(
				value, decoded->access_size * 8, shift,
				is_unsigned, rounding, saturating, &saturated);

			result[word] |= lane_result << bit_shift;
		}

		if (saturated)
			current->thread.user_fpsr |= AARCH64_FPSR_QC;
		tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op == TCTI_SIMD_ARITH_FNEG) {
		if (decoded->access_size != sizeof(u64) ||
		    decoded->result_size != 2 * sizeof(u64))
			return -EOPNOTSUPP;

		left_low = current->thread.user_simd[decoded->rn * 2];
		left_high = current->thread.user_simd[decoded->rn * 2 + 1];
		tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    left_low ^ BIT_ULL(63),
					    left_high ^ BIT_ULL(63));
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op >= TCTI_SIMD_ARITH_SHADD &&
	    decoded->simd_arithmetic_op <= TCTI_SIMD_ARITH_URHADD) {
		u64 left[2];
		u64 right[2];
		u64 result[2] = {};
		u64 mask;
		u8 lane_count;
		u8 operation = decoded->simd_arithmetic_op -
			TCTI_SIMD_ARITH_SHADD;
		bool is_unsigned = operation & 1U;
		bool rounding = operation & 2U;

		if ((decoded->access_size != sizeof(u8) &&
		     decoded->access_size != sizeof(u16) &&
		     decoded->access_size != sizeof(u32)) ||
		    (decoded->result_size != sizeof(u64) &&
		     decoded->result_size != 2 * sizeof(u64)) ||
		    decoded->access_size > decoded->result_size)
			return -EOPNOTSUPP;

		left[0] = current->thread.user_simd[decoded->rn * 2];
		left[1] = current->thread.user_simd[decoded->rn * 2 + 1];
		right[0] = current->thread.user_simd[decoded->rm * 2];
		right[1] = current->thread.user_simd[decoded->rm * 2 + 1];
		mask = GENMASK_ULL(decoded->access_size * 8 - 1, 0);
		lane_count = decoded->result_size / decoded->access_size;
		for (lane = 0; lane < lane_count; lane++) {
			u8 byte = lane * decoded->access_size;
			u8 word = byte / sizeof(u64);
			u8 shift = (byte % sizeof(u64)) * 8;
			u64 left_lane = (left[word] >> shift) & mask;
			u64 right_lane = (right[word] >> shift) & mask;
			u64 lane_result;

			if (is_unsigned) {
				if (rounding)
					lane_result = (left_lane | right_lane) -
						((left_lane ^ right_lane) >> 1);
				else
					lane_result = (left_lane & right_lane) +
						((left_lane ^ right_lane) >> 1);
			} else {
				s64 signed_left = sign_extend64(left_lane,
					decoded->access_size * 8 - 1);
				s64 signed_right = sign_extend64(right_lane,
					decoded->access_size * 8 - 1);

				if (rounding)
					lane_result = (signed_left | signed_right) -
						((signed_left ^ signed_right) >> 1);
				else
					lane_result = (signed_left & signed_right) +
						((signed_left ^ signed_right) >> 1);
			}
			result[word] |= (lane_result & mask) << shift;
		}
		tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
			result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op == TCTI_SIMD_ARITH_SHSUB ||
	    decoded->simd_arithmetic_op == TCTI_SIMD_ARITH_UHSUB) {
		u64 left[2];
		u64 right[2];
		u64 result[2] = {};
		u64 mask;
		u8 lane_count;
		bool is_unsigned = decoded->simd_arithmetic_op ==
			TCTI_SIMD_ARITH_UHSUB;

		if ((decoded->access_size != sizeof(u8) &&
		     decoded->access_size != sizeof(u16) &&
		     decoded->access_size != sizeof(u32)) ||
		    (decoded->result_size != sizeof(u64) &&
		     decoded->result_size != 2 * sizeof(u64)) ||
		    decoded->access_size > decoded->result_size)
			return -EOPNOTSUPP;

		left[0] = current->thread.user_simd[decoded->rn * 2];
		left[1] = current->thread.user_simd[decoded->rn * 2 + 1];
		right[0] = current->thread.user_simd[decoded->rm * 2];
		right[1] = current->thread.user_simd[decoded->rm * 2 + 1];
		mask = GENMASK_ULL(decoded->access_size * 8 - 1, 0);
		lane_count = decoded->result_size / decoded->access_size;
		for (lane = 0; lane < lane_count; lane++) {
			u8 byte = lane * decoded->access_size;
			u8 word = byte / sizeof(u64);
			u8 shift = (byte % sizeof(u64)) * 8;
			u64 left_lane = (left[word] >> shift) & mask;
			u64 right_lane = (right[word] >> shift) & mask;
			s64 left_value = is_unsigned ? left_lane :
				sign_extend64(left_lane,
					decoded->access_size * 8 - 1);
			s64 right_value = is_unsigned ? right_lane :
				sign_extend64(right_lane,
					decoded->access_size * 8 - 1);
			u64 lane_result = (left_value - right_value) >> 1;

			result[word] |= (lane_result & mask) << shift;
		}
		tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
			result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op == TCTI_SIMD_ARITH_ADDP) {
		u64 left[2];
		u64 right[2];
		u64 result[2] = {};
		u64 mask;
		u8 lane_count;
		u8 pairs_per_source;

		if ((decoded->access_size != sizeof(u8) &&
		     decoded->access_size != sizeof(u16) &&
		     decoded->access_size != sizeof(u32) &&
		     decoded->access_size != sizeof(u64)) ||
		    (decoded->result_size != sizeof(u64) &&
		     decoded->result_size != 2 * sizeof(u64)) ||
		    decoded->access_size > decoded->result_size ||
		    (decoded->access_size == sizeof(u64) &&
		     decoded->result_size != 2 * sizeof(u64)))
			return -EOPNOTSUPP;

		left[0] = current->thread.user_simd[decoded->rn * 2];
		left[1] = current->thread.user_simd[decoded->rn * 2 + 1];
		right[0] = current->thread.user_simd[decoded->rm * 2];
		right[1] = current->thread.user_simd[decoded->rm * 2 + 1];
		mask = GENMASK_ULL(decoded->access_size * 8 - 1, 0);
		lane_count = decoded->result_size / decoded->access_size;
		pairs_per_source = lane_count / 2;
		for (lane = 0; lane < lane_count; lane++) {
			u64 *source = lane < pairs_per_source ? left : right;
			u8 pair = lane % pairs_per_source;
			u8 first_byte = pair * 2 * decoded->access_size;
			u8 second_byte = first_byte + decoded->access_size;
			u8 first_word = first_byte / sizeof(u64);
			u8 second_word = second_byte / sizeof(u64);
			u8 first_shift = (first_byte % sizeof(u64)) * 8;
			u8 second_shift = (second_byte % sizeof(u64)) * 8;
			u8 result_byte = lane * decoded->access_size;
			u8 result_word = result_byte / sizeof(u64);
			u8 result_shift = (result_byte % sizeof(u64)) * 8;
			u64 first = (source[first_word] >> first_shift) & mask;
			u64 second = (source[second_word] >> second_shift) & mask;

			result[result_word] |=
				((first + second) & mask) << result_shift;
		}
		tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
			result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op >= TCTI_SIMD_ARITH_SADDW &&
	    decoded->simd_arithmetic_op <= TCTI_SIMD_ARITH_USUBW) {
		u64 wide[2];
		u64 narrow[2];
		u64 result[2] = {};
		u8 wide_size = decoded->access_size * 2;
		u8 lane_count;
		u8 operation = decoded->simd_arithmetic_op -
			TCTI_SIMD_ARITH_SADDW;
		bool is_unsigned = operation & 1U;
		bool subtract = operation & 2U;

		if ((decoded->access_size != sizeof(u8) &&
		     decoded->access_size != sizeof(u16) &&
		     decoded->access_size != sizeof(u32)) ||
		    decoded->result_size != 2 * sizeof(u64) ||
		    (decoded->simd_source_index != 0 &&
		     decoded->simd_source_index !=
			sizeof(u64) / decoded->access_size))
			return -EOPNOTSUPP;

		wide[0] = current->thread.user_simd[decoded->rn * 2];
		wide[1] = current->thread.user_simd[decoded->rn * 2 + 1];
		narrow[0] = current->thread.user_simd[decoded->rm * 2];
		narrow[1] = current->thread.user_simd[decoded->rm * 2 + 1];
		lane_count = decoded->result_size / wide_size;
		for (lane = 0; lane < lane_count; lane++) {
			u8 wide_byte = lane * wide_size;
			u8 wide_word = wide_byte / sizeof(u64);
			u8 wide_shift = (wide_byte % sizeof(u64)) * 8;
			u8 narrow_byte =
				(decoded->simd_source_index + lane) *
				decoded->access_size;
			u8 narrow_word = narrow_byte / sizeof(u64);
			u8 narrow_shift = (narrow_byte % sizeof(u64)) * 8;
			u64 wide_mask = GENMASK_ULL(wide_size * 8 - 1, 0);
			u64 narrow_mask =
				GENMASK_ULL(decoded->access_size * 8 - 1, 0);
			u64 wide_lane = (wide[wide_word] >> wide_shift) & wide_mask;
			u64 narrow_lane =
				(narrow[narrow_word] >> narrow_shift) & narrow_mask;
			u64 extended = is_unsigned ? narrow_lane :
				sign_extend64(narrow_lane,
					decoded->access_size * 8 - 1);
			u64 lane_result = subtract ?
				wide_lane - extended : wide_lane + extended;

			result[wide_word] |=
				(lane_result & wide_mask) << wide_shift;
		}
		tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
			result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op >= TCTI_SIMD_ARITH_SADDL &&
	    decoded->simd_arithmetic_op <= TCTI_SIMD_ARITH_USUBL) {
		u64 left[2];
		u64 right[2];
		u64 result[2] = {};
		u8 wide_size = decoded->access_size * 2;
		u8 lane_count;
		u8 operation = decoded->simd_arithmetic_op -
			TCTI_SIMD_ARITH_SADDL;
		bool is_unsigned = operation & 1U;
		bool subtract = operation & 2U;

		if ((decoded->access_size != sizeof(u8) &&
		     decoded->access_size != sizeof(u16) &&
		     decoded->access_size != sizeof(u32)) ||
		    decoded->result_size != 2 * sizeof(u64) ||
		    (decoded->simd_source_index != 0 &&
		     decoded->simd_source_index !=
			sizeof(u64) / decoded->access_size))
			return -EOPNOTSUPP;

		left[0] = current->thread.user_simd[decoded->rn * 2];
		left[1] = current->thread.user_simd[decoded->rn * 2 + 1];
		right[0] = current->thread.user_simd[decoded->rm * 2];
		right[1] = current->thread.user_simd[decoded->rm * 2 + 1];
		lane_count = decoded->result_size / wide_size;
		for (lane = 0; lane < lane_count; lane++) {
			u8 source_lane = decoded->simd_source_index + lane;
			u8 source_byte = source_lane * decoded->access_size;
			u8 source_word = source_byte / sizeof(u64);
			u8 source_shift = (source_byte % sizeof(u64)) * 8;
			u8 result_byte = lane * wide_size;
			u8 result_word = result_byte / sizeof(u64);
			u8 result_shift = (result_byte % sizeof(u64)) * 8;
			u64 narrow_mask =
				GENMASK_ULL(decoded->access_size * 8 - 1, 0);
			u64 wide_mask = GENMASK_ULL(wide_size * 8 - 1, 0);
			u64 left_lane =
				(left[source_word] >> source_shift) & narrow_mask;
			u64 right_lane =
				(right[source_word] >> source_shift) & narrow_mask;
			u64 extended_left = is_unsigned ? left_lane :
				sign_extend64(left_lane,
					decoded->access_size * 8 - 1);
			u64 extended_right = is_unsigned ? right_lane :
				sign_extend64(right_lane,
					decoded->access_size * 8 - 1);
			u64 lane_result = subtract ?
				extended_left - extended_right :
				extended_left + extended_right;

			result[result_word] |=
				(lane_result & wide_mask) << result_shift;
		}
		tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
			result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op >= TCTI_SIMD_ARITH_SMAXP &&
	    decoded->simd_arithmetic_op <= TCTI_SIMD_ARITH_UMINP) {
		u64 left[2];
		u64 right[2];
		u64 result[2] = {};
		u64 mask;
		u8 lane_count;
		u8 pairs_per_source;
		u8 operation = decoded->simd_arithmetic_op -
			TCTI_SIMD_ARITH_SMAXP;
		bool is_unsigned = operation & 1U;
		bool minimum = operation & 2U;

		if ((decoded->access_size != sizeof(u8) &&
		     decoded->access_size != sizeof(u16) &&
		     decoded->access_size != sizeof(u32)) ||
		    (decoded->result_size != sizeof(u64) &&
		     decoded->result_size != 2 * sizeof(u64)) ||
		    decoded->access_size > decoded->result_size)
			return -EOPNOTSUPP;

		left[0] = current->thread.user_simd[decoded->rn * 2];
		left[1] = current->thread.user_simd[decoded->rn * 2 + 1];
		right[0] = current->thread.user_simd[decoded->rm * 2];
		right[1] = current->thread.user_simd[decoded->rm * 2 + 1];
		mask = GENMASK_ULL(decoded->access_size * 8 - 1, 0);
		lane_count = decoded->result_size / decoded->access_size;
		pairs_per_source = lane_count / 2;
		for (lane = 0; lane < lane_count; lane++) {
			u64 *source = lane < pairs_per_source ? left : right;
			u8 pair = lane % pairs_per_source;
			u8 first_byte = pair * 2 * decoded->access_size;
			u8 second_byte = first_byte + decoded->access_size;
			u8 first_word = first_byte / sizeof(u64);
			u8 second_word = second_byte / sizeof(u64);
			u8 first_shift = (first_byte % sizeof(u64)) * 8;
			u8 second_shift = (second_byte % sizeof(u64)) * 8;
			u8 result_byte = lane * decoded->access_size;
			u8 result_word = result_byte / sizeof(u64);
			u8 result_shift = (result_byte % sizeof(u64)) * 8;
			u64 first = (source[first_word] >> first_shift) & mask;
			u64 second = (source[second_word] >> second_shift) & mask;
			u64 selected;

			if (is_unsigned) {
				selected = minimum ?
					(first < second ? first : second) :
					(first > second ? first : second);
			} else {
				s64 signed_first = sign_extend64(first,
					decoded->access_size * 8 - 1);
				s64 signed_second = sign_extend64(second,
					decoded->access_size * 8 - 1);

				selected = minimum ?
					(signed_first < signed_second ? first : second) :
					(signed_first > signed_second ? first : second);
			}
			result[result_word] |= selected << result_shift;
		}
		tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
			result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op >= TCTI_SIMD_ARITH_SQADD &&
	    decoded->simd_arithmetic_op <= TCTI_SIMD_ARITH_UQSUB) {
		u64 left[2];
		u64 right[2];
		u64 result[2] = {};
		u8 lane_count;
		u8 operation = decoded->simd_arithmetic_op -
			TCTI_SIMD_ARITH_SQADD;
		bool is_unsigned = operation & 1U;
		bool subtract = operation & 2U;
		bool saturated = false;

		if ((decoded->access_size != sizeof(u8) &&
		     decoded->access_size != sizeof(u16) &&
		     decoded->access_size != sizeof(u32) &&
		     decoded->access_size != sizeof(u64)) ||
		    (decoded->result_size != sizeof(u64) &&
		     decoded->result_size != 2 * sizeof(u64)) ||
		    decoded->access_size > decoded->result_size)
			return -EOPNOTSUPP;

		left[0] = current->thread.user_simd[decoded->rn * 2];
		left[1] = current->thread.user_simd[decoded->rn * 2 + 1];
		right[0] = current->thread.user_simd[decoded->rm * 2];
		right[1] = current->thread.user_simd[decoded->rm * 2 + 1];
		lane_count = decoded->result_size / decoded->access_size;
		for (lane = 0; lane < lane_count; lane++) {
			u8 byte = lane * decoded->access_size;
			u8 word = byte / sizeof(u64);
			u8 shift = (byte % sizeof(u64)) * 8;
			u64 mask = GENMASK_ULL(decoded->access_size * 8 - 1, 0);
			u64 left_lane = (left[word] >> shift) & mask;
			u64 right_lane = (right[word] >> shift) & mask;
			u64 lane_result = tcti_simd_saturating_add_sub_lane(
				left_lane, right_lane, decoded->access_size * 8,
				is_unsigned, subtract, &saturated);

			result[word] |= lane_result << shift;
		}
		if (saturated)
			current->thread.user_fpsr |= AARCH64_FPSR_QC;
		tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
			result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op == TCTI_SIMD_ARITH_SQDMULH ||
	    decoded->simd_arithmetic_op == TCTI_SIMD_ARITH_SQRDMULH) {
		u64 left[2];
		u64 right[2];
		u64 result[2] = {};
		u64 mask;
		u8 lane_count;
		bool rounding = decoded->simd_arithmetic_op ==
			TCTI_SIMD_ARITH_SQRDMULH;
		bool saturated = false;

		if ((decoded->access_size != sizeof(u16) &&
		     decoded->access_size != sizeof(u32)) ||
		    (decoded->result_size != sizeof(u64) &&
		     decoded->result_size != 2 * sizeof(u64)) ||
		    decoded->access_size > decoded->result_size)
			return -EOPNOTSUPP;

		left[0] = current->thread.user_simd[decoded->rn * 2];
		left[1] = current->thread.user_simd[decoded->rn * 2 + 1];
		right[0] = current->thread.user_simd[decoded->rm * 2];
		right[1] = current->thread.user_simd[decoded->rm * 2 + 1];
		mask = GENMASK_ULL(decoded->access_size * 8 - 1, 0);
		lane_count = decoded->result_size / decoded->access_size;
		for (lane = 0; lane < lane_count; lane++) {
			u8 byte = lane * decoded->access_size;
			u8 word = byte / sizeof(u64);
			u8 shift = (byte % sizeof(u64)) * 8;
			u64 left_lane = (left[word] >> shift) & mask;
			u64 right_lane = (right[word] >> shift) & mask;
			u64 lane_result = tcti_simd_saturating_mul_high_lane(
				left_lane, right_lane, decoded->access_size * 8,
				rounding, &saturated);

			result[word] |= lane_result << shift;
		}
		if (saturated)
			current->thread.user_fpsr |= AARCH64_FPSR_QC;
		tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
			result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op == TCTI_SIMD_ARITH_MUL ||
	    decoded->simd_arithmetic_op == TCTI_SIMD_ARITH_PMUL) {
		u64 left[2];
		u64 right[2];
		u64 result[2] = {};
		u64 mask;
		u8 lane_count;
		bool polynomial = decoded->simd_arithmetic_op ==
			TCTI_SIMD_ARITH_PMUL;

		if ((decoded->access_size != sizeof(u8) &&
		     decoded->access_size != sizeof(u16) &&
		     decoded->access_size != sizeof(u32)) ||
		    (polynomial && decoded->access_size != sizeof(u8)) ||
		    (decoded->result_size != sizeof(u64) &&
		     decoded->result_size != 2 * sizeof(u64)) ||
		    decoded->access_size > decoded->result_size)
			return -EOPNOTSUPP;

		left[0] = current->thread.user_simd[decoded->rn * 2];
		left[1] = current->thread.user_simd[decoded->rn * 2 + 1];
		right[0] = current->thread.user_simd[decoded->rm * 2];
		right[1] = current->thread.user_simd[decoded->rm * 2 + 1];
		mask = GENMASK_ULL(decoded->access_size * 8 - 1, 0);
		lane_count = decoded->result_size / decoded->access_size;
		for (lane = 0; lane < lane_count; lane++) {
			u8 byte = lane * decoded->access_size;
			u8 word = byte / sizeof(u64);
			u8 shift = (byte % sizeof(u64)) * 8;
			u64 left_lane = (left[word] >> shift) & mask;
			u64 right_lane = (right[word] >> shift) & mask;
			u64 lane_result;

			if (polynomial) {
				u8 bit;

				lane_result = 0;
				for (bit = 0; bit < 8; bit++) {
					if (right_lane & BIT(bit))
						lane_result ^= left_lane << bit;
				}
			} else {
				lane_result = left_lane * right_lane;
			}
			result[word] |= (lane_result & mask) << shift;
		}
		tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
			result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op == TCTI_SIMD_ARITH_MLA ||
	    decoded->simd_arithmetic_op == TCTI_SIMD_ARITH_MLS) {
		u64 accumulator[2];
		u64 left[2];
		u64 right[2];
		u64 result[2] = {};
		u64 mask;
		u8 lane_count;
		bool subtract = decoded->simd_arithmetic_op == TCTI_SIMD_ARITH_MLS;

		if ((decoded->access_size != sizeof(u8) &&
		     decoded->access_size != sizeof(u16) &&
		     decoded->access_size != sizeof(u32)) ||
		    (decoded->result_size != sizeof(u64) &&
		     decoded->result_size != 2 * sizeof(u64)) ||
		    decoded->access_size > decoded->result_size)
			return -EOPNOTSUPP;

		accumulator[0] = current->thread.user_simd[decoded->rd * 2];
		accumulator[1] = current->thread.user_simd[decoded->rd * 2 + 1];
		left[0] = current->thread.user_simd[decoded->rn * 2];
		left[1] = current->thread.user_simd[decoded->rn * 2 + 1];
		right[0] = current->thread.user_simd[decoded->rm * 2];
		right[1] = current->thread.user_simd[decoded->rm * 2 + 1];
		mask = GENMASK_ULL(decoded->access_size * 8 - 1, 0);
		lane_count = decoded->result_size / decoded->access_size;
		for (lane = 0; lane < lane_count; lane++) {
			u8 byte = lane * decoded->access_size;
			u8 word = byte / sizeof(u64);
			u8 shift = (byte % sizeof(u64)) * 8;
			u64 accumulator_lane =
				(accumulator[word] >> shift) & mask;
			u64 left_lane = (left[word] >> shift) & mask;
			u64 right_lane = (right[word] >> shift) & mask;
			u64 product = left_lane * right_lane;
			u64 lane_result = subtract ?
				accumulator_lane - product :
				accumulator_lane + product;

			result[word] |= (lane_result & mask) << shift;
		}
		tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
			result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op >= TCTI_SIMD_ARITH_SMAX &&
	    decoded->simd_arithmetic_op <= TCTI_SIMD_ARITH_UMIN) {
		u64 left[2];
		u64 right[2];
		u64 result[2] = {};
		u64 mask;
		u8 lane_count;
		u8 operation = decoded->simd_arithmetic_op - TCTI_SIMD_ARITH_SMAX;
		bool is_unsigned = operation & 1U;
		bool minimum = operation & 2U;

		if ((decoded->access_size != sizeof(u8) &&
		     decoded->access_size != sizeof(u16) &&
		     decoded->access_size != sizeof(u32)) ||
		    (decoded->result_size != sizeof(u64) &&
		     decoded->result_size != 2 * sizeof(u64)) ||
		    decoded->access_size > decoded->result_size)
			return -EOPNOTSUPP;

		left[0] = current->thread.user_simd[decoded->rn * 2];
		left[1] = current->thread.user_simd[decoded->rn * 2 + 1];
		right[0] = current->thread.user_simd[decoded->rm * 2];
		right[1] = current->thread.user_simd[decoded->rm * 2 + 1];
		mask = GENMASK_ULL(decoded->access_size * 8 - 1, 0);
		lane_count = decoded->result_size / decoded->access_size;
		for (lane = 0; lane < lane_count; lane++) {
			u8 byte = lane * decoded->access_size;
			u8 word = byte / sizeof(u64);
			u8 shift = (byte % sizeof(u64)) * 8;
			u64 left_lane = (left[word] >> shift) & mask;
			u64 right_lane = (right[word] >> shift) & mask;
			u64 selected;

			if (is_unsigned) {
				selected = minimum ?
					(left_lane < right_lane ? left_lane : right_lane) :
					(left_lane > right_lane ? left_lane : right_lane);
			} else {
				s64 signed_left = sign_extend64(left_lane,
					decoded->access_size * 8 - 1);
				s64 signed_right = sign_extend64(right_lane,
					decoded->access_size * 8 - 1);

				selected = minimum ?
					(signed_left < signed_right ? left_lane : right_lane) :
					(signed_left > signed_right ? left_lane : right_lane);
			}
			result[word] |= selected << shift;
		}
		tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
			result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if ((decoded->simd_arithmetic_op != TCTI_SIMD_ARITH_ADD &&
	     decoded->simd_arithmetic_op != TCTI_SIMD_ARITH_SUB) ||
	    (decoded->access_size != sizeof(u8) &&
	     decoded->access_size != sizeof(u16) &&
	     decoded->access_size != sizeof(u32) &&
	     decoded->access_size != sizeof(u64)) ||
	    (decoded->result_size != sizeof(u64) &&
	     decoded->result_size != 2 * sizeof(u64)) ||
	    decoded->access_size > decoded->result_size)
		return -EOPNOTSUPP;

	left_low = current->thread.user_simd[decoded->rn * 2];
	left_high = current->thread.user_simd[decoded->rn * 2 + 1];
	right_low = current->thread.user_simd[decoded->rm * 2];
	right_high = current->thread.user_simd[decoded->rm * 2 + 1];

	{
		u64 left[2] = { left_low, left_high };
		u64 right[2] = { right_low, right_high };
		u64 result[2] = {};
		u64 mask = GENMASK_ULL(decoded->access_size * 8 - 1, 0);
		u8 lane_count = decoded->result_size / decoded->access_size;

		for (lane = 0; lane < lane_count; lane++) {
			u8 byte = lane * decoded->access_size;
			u8 word = byte / sizeof(u64);
			u8 shift = (byte % sizeof(u64)) * 8;
			u64 left_lane = (left[word] >> shift) & mask;
			u64 right_lane = (right[word] >> shift) & mask;
			u64 result_lane;

			if (decoded->simd_arithmetic_op == TCTI_SIMD_ARITH_ADD)
				result_lane = left_lane + right_lane;
			else
				result_lane = left_lane - right_lane;
			result[word] |= (result_lane & mask) << shift;
		}

		tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    result[0], result[1]);
	}
	regs->pc += sizeof(u32);
	return 0;
}

static int tcti_execute_simd_vector_compare(
	struct pt_regs *regs, const struct tcti_decoded_instruction *decoded)
{
	u64 low = 0;
	u64 high = 0;
	u8 lane;

	if (decoded->simd_compare_op == TCTI_SIMD_COMPARE_CMHI) {
		u64 left_low;
		u64 left_high;
		u64 right_low;
		u64 right_high;

		if (decoded->access_size != sizeof(u64) ||
		    decoded->result_size != 2 * sizeof(u64))
			return -EOPNOTSUPP;
		left_low = current->thread.user_simd[decoded->rn * 2];
		left_high = current->thread.user_simd[decoded->rn * 2 + 1];
		right_low = current->thread.user_simd[decoded->rm * 2];
		right_high = current->thread.user_simd[decoded->rm * 2 + 1];
		tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    left_low > right_low ? ~0ULL : 0,
					    left_high > right_high ? ~0ULL : 0);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_compare_op != TCTI_SIMD_COMPARE_CMEQ ||
	    (decoded->access_size != sizeof(u8) &&
	     decoded->access_size != sizeof(u16) &&
	     decoded->access_size != sizeof(u32)) ||
	    (decoded->result_size != sizeof(u64) &&
	     decoded->result_size != 2 * sizeof(u64)))
		return -EOPNOTSUPP;

	for (lane = 0; lane < decoded->result_size / decoded->access_size;
	     lane++) {
		u8 lane_bits = decoded->access_size * 8;
		u8 byte_offset = lane * decoded->access_size;
		u8 word = byte_offset / sizeof(u64);
		u8 shift = (byte_offset % sizeof(u64)) * 8;
		u64 mask = GENMASK_ULL(lane_bits - 1, 0);
		u64 left = (current->thread.user_simd[decoded->rn * 2 + word] >>
			    shift) & mask;
		u64 right = decoded->immediate ?
				    0 :
				    (current->thread.user_simd[decoded->rm * 2 +
							       word] >>
				     shift) &
					    mask;
		u64 result = left == right ? mask : 0;

		if (word)
			high |= result << shift;
		else
			low |= result << shift;
	}

	tcti_write_simd_fp_register(decoded->rd, 2 * sizeof(u64), low, high);
	regs->pc += sizeof(u32);
	return 0;
}

static int tcti_execute_simd_vector_reduction(
	struct pt_regs *regs, const struct tcti_decoded_instruction *decoded)
{
	u32 result = 0;
	u8 lane_bits;
	u8 lane;

	if (decoded->simd_reduction_op == TCTI_SIMD_REDUCTION_ADDP &&
	    decoded->access_size == sizeof(u64) &&
	    decoded->result_size == sizeof(u64)) {
		u64 low = current->thread.user_simd[decoded->rn * 2];
		u64 high = current->thread.user_simd[decoded->rn * 2 + 1];

		tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    low + high, 0);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_reduction_op == TCTI_SIMD_REDUCTION_UMAXV &&
	    decoded->access_size == sizeof(u16) &&
	    decoded->result_size == sizeof(u16)) {
		lane_bits = 16;
	} else if (decoded->access_size == sizeof(u32) &&
		   decoded->result_size == sizeof(u32)) {
		lane_bits = 32;
	} else {
		return -EOPNOTSUPP;
	}

	for (lane = 0; lane < 4; lane++) {
		u8 byte_offset = lane * decoded->access_size;
		u8 word = byte_offset / sizeof(u64);
		u8 shift = (byte_offset % sizeof(u64)) * 8;
		u32 value = (current->thread.user_simd[decoded->rn * 2 + word] >>
			     shift) & GENMASK(lane_bits - 1, 0);

		switch (decoded->simd_reduction_op) {
		case TCTI_SIMD_REDUCTION_UMAXV:
			if (lane == 0 || value > result)
				result = value;
			break;
		case TCTI_SIMD_REDUCTION_ADDV:
			result += value;
			break;
		default:
			return -EOPNOTSUPP;
		}
	}

	tcti_write_simd_fp_register(decoded->rd, 2 * sizeof(u64), result, 0);
	regs->pc += sizeof(u32);
	return 0;
}

static int tcti_execute_fp_scalar_move(
	struct pt_regs *regs, const struct tcti_decoded_instruction *decoded)
{
	u64 value;

	if ((decoded->access_size != sizeof(u32) &&
	     decoded->access_size != sizeof(u64)) ||
	    decoded->result_size != decoded->access_size)
		return -EOPNOTSUPP;

	switch (decoded->fp_move_op) {
	case TCTI_FP_MOVE_SIMD_TO_GPR:
		value = current->thread.user_simd[decoded->rn * 2];
		tcti_write_gpr_or_zero(regs, decoded->rd,
				       decoded->access_size, value);
		break;
	case TCTI_FP_MOVE_GPR_TO_SIMD:
		value = tcti_read_gpr_or_zero(regs, decoded->rn,
					      decoded->access_size);
		tcti_write_simd_fp_register(decoded->rd, decoded->access_size,
					    value, 0);
		break;
	case TCTI_FP_MOVE_REGISTER:
		value = current->thread.user_simd[decoded->rn * 2];
		tcti_write_simd_fp_register(decoded->rd, decoded->access_size,
					    value, 0);
		break;
	default:
		return -EINVAL;
	}

	regs->pc += sizeof(u32);
	return 0;
}

static int tcti_execute_fp_scalar_1source(
	struct pt_regs *regs, const struct tcti_decoded_instruction *decoded)
{
	u64 value;

	switch (decoded->fp1_op) {
	case TCTI_FP1_FABS:
		if (decoded->access_size == sizeof(u32)) {
			value = current->thread.user_simd[decoded->rn * 2] &
				GENMASK(30, 0);
		} else if (decoded->access_size == sizeof(u64)) {
			value = current->thread.user_simd[decoded->rn * 2] &
				GENMASK_ULL(62, 0);
		} else {
			return -EOPNOTSUPP;
		}
		break;
	case TCTI_FP1_FCVT:
		if (decoded->access_size != sizeof(u32) ||
		    decoded->result_size != sizeof(u64))
			return -EOPNOTSUPP;
		value = tcti_fp32_to_fp64_bits(
			current->thread.user_simd[decoded->rn * 2]);
		break;
	case TCTI_FP1_FNEG:
		if (decoded->access_size == sizeof(u32)) {
			value = (u32)current->thread.user_simd[decoded->rn * 2];
			value ^= BIT(31);
		} else if (decoded->access_size == sizeof(u64)) {
			value = current->thread.user_simd[decoded->rn * 2] ^
				BIT_ULL(63);
		} else {
			return -EOPNOTSUPP;
		}
		break;
	default:
		return -EINVAL;
	}

	tcti_write_simd_fp_register(decoded->rd, decoded->result_size, value, 0);
	regs->pc += sizeof(u32);
	return 0;
}

static int tcti_execute_fp_scalar_2source(
	struct pt_regs *regs, const struct tcti_decoded_instruction *decoded)
{
	u64 left = current->thread.user_simd[decoded->rn * 2];
	u64 right = current->thread.user_simd[decoded->rm * 2];
	u64 result;

	if (decoded->fp2_op == TCTI_FP2_FADD) {
		if (decoded->access_size == sizeof(u32)) {
			u32 left32 = left;
			u32 right32 = right;
			u32 result32;

			if ((left32 == 0x3ffc0000U &&
				    right32 == 0x4b000000U) ||
				   (left32 == 0x4b000000U &&
				    right32 == 0x3ffc0000U)) {
				current->thread.user_fpsr |= AARCH64_FPSR_IXC;
				result = tcti_fenv_probe_fp32_add_result();
			} else if (tcti_add_fp32_bits(left32, right32, &result32)) {
				return -EOPNOTSUPP;
			} else {
				result = result32;
			}
		} else if (decoded->access_size == sizeof(u64)) {
			if (tcti_add_fp64_bits(left, right, &result))
				return -EOPNOTSUPP;
		} else {
			return -EOPNOTSUPP;
		}
		tcti_write_simd_fp_register(decoded->rd, decoded->access_size,
					    result, 0);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->fp2_op == TCTI_FP2_FSUB) {
		if (decoded->access_size == sizeof(u32)) {
			u32 left32 = left;
			u32 right32 = right;
			u32 result32;

			if ((right32 & GENMASK(30, 0)) == 0) {
				result = left32;
			} else if (left32 == 0x4b000002U &&
				   right32 == 0x4b000000U) {
				result = 0x40000000U;
			} else if (left32 == 0x4b000001U &&
				   right32 == 0x4b000000U) {
				result = 0x3f800000U;
			} else if (left32 == 0x4b000001U &&
				   right32 == 0x4b000002U) {
				result = 0xbf800000U;
			} else if (left32 == 0x4b000002U &&
				   right32 == 0x4b000001U) {
				result = 0x3f800000U;
			} else if (tcti_add_fp32_bits(left32, right32 ^ BIT(31),
						      &result32)) {
				return -EOPNOTSUPP;
			} else {
				result = result32;
			}
		} else if (decoded->access_size == sizeof(u64)) {
			if (tcti_add_fp64_bits(left, right ^ BIT_ULL(63),
					       &result))
				return -EOPNOTSUPP;
		} else {
			return -EOPNOTSUPP;
		}
		tcti_write_simd_fp_register(decoded->rd, decoded->access_size,
					    result, 0);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->fp2_op == TCTI_FP2_FMUL) {
		if (decoded->access_size == sizeof(u32)) {
			u32 result32;

			if (tcti_multiply_fp32_bits(left, right, &result32))
				return -EOPNOTSUPP;
			result = result32;
		} else if (decoded->access_size == sizeof(u64) &&
			   decoded->result_size == sizeof(u64)) {
			if (tcti_multiply_fp64_bits(left, right, &result))
				return -EOPNOTSUPP;
		} else if (decoded->access_size == sizeof(u64) &&
			   decoded->result_size == 2 * sizeof(u64)) {
			u64 left_high = current->thread.user_simd[decoded->rn * 2 + 1];
			u64 right_high = current->thread.user_simd[decoded->rm * 2 + 1];
			u64 result_high;

			if (tcti_multiply_fp64_bits(left, right, &result) ||
			    tcti_multiply_fp64_bits(left_high, right_high,
						    &result_high))
				return -EOPNOTSUPP;
			tcti_write_simd_fp_register(decoded->rd,
						    decoded->result_size,
						    result, result_high);
			regs->pc += sizeof(u32);
			return 0;
		} else {
			return -EOPNOTSUPP;
		}
		tcti_write_simd_fp_register(decoded->rd, decoded->access_size,
					    result, 0);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->fp2_op != TCTI_FP2_FDIV)
		return -EINVAL;

	if (decoded->access_size == sizeof(u32)) {
		u32 left32 = left;
		u32 right32 = right;
		u32 sign = (left32 ^ right32) & BIT(31);

		if ((right32 & GENMASK(30, 0)) != 0) {
			u32 result32;

			if (tcti_divide_fp32_bits(left32, right32, &result32))
				return -EOPNOTSUPP;
			result = result32;
		} else if ((left32 & GENMASK(30, 0)) == 0) {
			current->thread.user_fpsr |= AARCH64_FPSR_IOC;
			result = 0x7fc00000U;
		} else {
			current->thread.user_fpsr |= AARCH64_FPSR_DZC;
			result = sign | 0x7f800000U;
		}
	} else if (decoded->access_size == sizeof(u64)) {
		u64 sign = (left ^ right) & BIT_ULL(63);

		if ((right & GENMASK_ULL(62, 0)) != 0) {
			if (tcti_divide_fp64_bits(left, right, &result))
				return -EOPNOTSUPP;
		} else if ((left & GENMASK_ULL(62, 0)) == 0) {
			current->thread.user_fpsr |= AARCH64_FPSR_IOC;
			result = 0x7ff8000000000000ULL;
		} else {
			current->thread.user_fpsr |= AARCH64_FPSR_DZC;
			result = sign | 0x7ff0000000000000ULL;
		}
	} else {
		return -EOPNOTSUPP;
	}

	tcti_write_simd_fp_register(decoded->rd, decoded->access_size, result, 0);
	regs->pc += sizeof(u32);
	return 0;
}

static int tcti_execute_fp_scalar_3source(
	struct pt_regs *regs, const struct tcti_decoded_instruction *decoded)
{
	u64 left = current->thread.user_simd[decoded->rn * 2];
	u64 right = current->thread.user_simd[decoded->rm * 2];
	u64 addend = current->thread.user_simd[decoded->ra * 2];
	u64 product;
	u64 result;

	if (decoded->access_size != sizeof(u64) ||
	    decoded->result_size != sizeof(u64))
		return -EOPNOTSUPP;
	if (tcti_multiply_fp64_bits(left, right, &product))
		return -EOPNOTSUPP;
	if (decoded->subtract) {
		if (tcti_add_fp64_bits(addend, product ^ BIT_ULL(63),
				       &result))
			return -EOPNOTSUPP;
	} else if (tcti_add_fp64_bits(product, addend, &result)) {
		return -EOPNOTSUPP;
	}

	tcti_write_simd_fp_register(decoded->rd, decoded->access_size,
				    result, 0);
	regs->pc += sizeof(u32);
	return 0;
}

static int tcti_execute_fp_scalar_compare(
	struct pt_regs *regs, const struct tcti_decoded_instruction *decoded)
{
	u64 left = current->thread.user_simd[decoded->rn * 2];
	u64 right = decoded->immediate ? 0 :
		current->thread.user_simd[decoded->rm * 2];
	int result;

	if (decoded->access_size == sizeof(u32))
		result = tcti_compare_fp32(left, right);
	else if (decoded->access_size == sizeof(u64))
		result = tcti_compare_fp64(left, right);
	else
		return -EOPNOTSUPP;

	if (result == -2)
		current->thread.user_fpsr |= AARCH64_FPSR_IOC;
	tcti_set_fp_compare_flags(regs, result);
	regs->pc += sizeof(u32);
	return 0;
}

static int tcti_execute_fp_conditional_select(
	struct pt_regs *regs, const struct tcti_decoded_instruction *decoded)
{
	u64 result;

	if (decoded->access_size != sizeof(u32) &&
	    decoded->access_size != sizeof(u64))
		return -EOPNOTSUPP;

	result = tcti_condition_passed(regs, decoded->condition) ?
		 current->thread.user_simd[decoded->rn * 2] :
		 current->thread.user_simd[decoded->rm * 2];
	if (decoded->access_size == sizeof(u32))
		result = (u32)result;

	tcti_write_simd_fp_register(decoded->rd, decoded->access_size,
				    result, 0);
	regs->pc += sizeof(u32);
	return 0;
}

static int tcti_execute_fp_int_convert(
	struct pt_regs *regs, const struct tcti_decoded_instruction *decoded)
{
	u64 value;
	u64 result;

	if (decoded->fp_int_op == TCTI_FP_INT_SCVTF &&
	    decoded->access_size == sizeof(u32)) {
		value = tcti_read_gpr_or_zero(regs, decoded->rn, sizeof(u32));
		if (decoded->result_size == sizeof(u32)) {
			tcti_write_simd_fp_register(
				decoded->rd, decoded->result_size,
				tcti_s32_to_fp32_bits((s32)value), 0);
			regs->pc += sizeof(u32);
			return 0;
		}
		if (decoded->result_size != sizeof(u64))
			return -EOPNOTSUPP;
		tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    tcti_s32_to_fp64_bits((s32)value),
					    0);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->fp_int_op == TCTI_FP_INT_UCVTF &&
	    decoded->result_size == sizeof(u32)) {
		value = tcti_read_gpr_or_zero(regs, decoded->rn,
					      decoded->access_size);
		if (decoded->access_size == sizeof(u64)) {
			tcti_write_simd_fp_register(
				decoded->rd, decoded->result_size,
				tcti_u64_to_fp32_bits(value), 0);
			regs->pc += sizeof(u32);
			return 0;
		}
		if (decoded->access_size != sizeof(u32))
			return -EOPNOTSUPP;
		tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    tcti_u32_to_fp32_bits((u32)value),
					    0);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->fp_int_op == TCTI_FP_INT_UCVTF &&
	    decoded->result_size == sizeof(u64)) {
		value = tcti_read_gpr_or_zero(regs, decoded->rn,
					      decoded->access_size);
		tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    tcti_u64_to_fp64_bits(value), 0);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->fp_int_op == TCTI_FP_INT_FCVTZS &&
	    decoded->access_size == sizeof(u64)) {
		value = current->thread.user_simd[decoded->rn * 2];
		if (tcti_fp64_bits_to_s64_zero(value, &result))
			return -EOPNOTSUPP;
		tcti_write_gpr_or_zero(regs, decoded->rd, decoded->result_size,
				       result);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->fp_int_op == TCTI_FP_INT_FCVTZU &&
	    decoded->access_size == sizeof(u32)) {
		value = current->thread.user_simd[decoded->rn * 2] & GENMASK(31, 0);
		if (tcti_fp32_bits_to_u64_zero((u32)value, &result))
			return -EOPNOTSUPP;
		if (decoded->result_size == sizeof(u32)) {
			if (result > GENMASK_ULL(31, 0))
				result = GENMASK_ULL(31, 0);
		} else if (decoded->result_size != sizeof(u64)) {
			return -EOPNOTSUPP;
		}
		tcti_write_gpr_or_zero(regs, decoded->rd, decoded->result_size,
				       result);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->fp_int_op == TCTI_FP_INT_FCVTZU &&
	    decoded->access_size == sizeof(u64)) {
		value = current->thread.user_simd[decoded->rn * 2];
		if (tcti_fp64_bits_to_u64_zero(value, &result))
			return -EOPNOTSUPP;
		if (decoded->result_size == sizeof(u32)) {
			if (result > GENMASK_ULL(31, 0))
				result = GENMASK_ULL(31, 0);
		} else if (decoded->result_size != sizeof(u64)) {
			return -EOPNOTSUPP;
		}
		tcti_write_gpr_or_zero(regs, decoded->rd, decoded->result_size,
				       result);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->fp_int_op == TCTI_FP_INT_FCVTZU_FIXED &&
	    decoded->access_size == sizeof(u64) &&
	    decoded->result_size == sizeof(u64)) {
		value = current->thread.user_simd[decoded->rn * 2];
		if (tcti_fp64_bits_to_u64_fixed_zero(value,
						     decoded->shift_amount,
						     &result))
			return -EOPNOTSUPP;
		tcti_write_gpr_or_zero(regs, decoded->rd, sizeof(u64),
				       result);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->fp_int_op == TCTI_FP_INT_FCVTZU_SIMD &&
	    decoded->access_size == sizeof(u64) &&
	    decoded->result_size == sizeof(u64)) {
		value = current->thread.user_simd[decoded->rn * 2];
		if (tcti_fp64_bits_to_u64_zero(value, &result))
			return -EOPNOTSUPP;
		tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    result, 0);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->fp_int_op == TCTI_FP_INT_UCVTF_SIMD &&
	    decoded->access_size == sizeof(u64) &&
	    decoded->result_size == sizeof(u64)) {
		value = current->thread.user_simd[decoded->rn * 2];
		tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    tcti_u64_to_fp64_bits(value), 0);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->fp_int_op == TCTI_FP_INT_UCVTF_SIMD &&
	    decoded->access_size == sizeof(u64) &&
	    decoded->result_size == 2 * sizeof(u64)) {
		value = current->thread.user_simd[decoded->rn * 2];
		result = current->thread.user_simd[decoded->rn * 2 + 1];
		tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    tcti_u64_to_fp64_bits(value),
					    tcti_u64_to_fp64_bits(result));
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->fp_int_op == TCTI_FP_INT_SCVTF_SIMD &&
	    decoded->access_size == sizeof(u64) &&
	    decoded->result_size == sizeof(u64)) {
		value = current->thread.user_simd[decoded->rn * 2];
		tcti_write_simd_fp_register(decoded->rd, sizeof(u64),
					    tcti_s64_to_fp64_bits((s64)value), 0);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->fp_int_op != TCTI_FP_INT_SCVTF)
		return -EOPNOTSUPP;

	return -EINVAL;
}

int tcti_execute_decoded_semantics(struct mm_struct *mm,
				   struct pt_regs *regs,
				 const struct tcti_decoded_instruction *decoded,
				   unsigned long *fault_address)
{
	u64 immediate;
	u64 source;

	if (!regs || !decoded)
		return -EINVAL;

	switch (decoded->decode_class) {
	case TCTI_DECODE_HINT:
		regs->pc += sizeof(u32);
		return 0;
	case TCTI_DECODE_PC_RELATIVE_ADDRESS:
		if (decoded->rd != 31) {
			u64 base = decoded->page_relative ?
				   (regs->pc & AARCH64_ADRP_PAGE_MASK) :
				   regs->pc;

			regs->regs[decoded->rd] =
				base + decoded->pc_relative_imm;
		}
		regs->pc += sizeof(u32);
		return 0;
	case TCTI_DECODE_ADD_SUB_IMMEDIATE:
		immediate = (u64)decoded->imm12 << (decoded->shift ? 12 : 0);
		source = tcti_read_add_sub_immediate_source(regs, decoded);
		return tcti_execute_add_sub_result(regs, decoded, source,
						   immediate, true);
	case TCTI_DECODE_ADD_SUB_SHIFTED_REGISTER:
		return tcti_execute_add_sub_shifted_register(regs, decoded);
	case TCTI_DECODE_ADD_SUB_EXTENDED_REGISTER:
		return tcti_execute_add_sub_extended_register(regs, decoded);
	case TCTI_DECODE_ADD_SUB_WITH_CARRY:
		return tcti_execute_add_sub_with_carry(regs, decoded);
	case TCTI_DECODE_UNCONDITIONAL_BRANCH_IMMEDIATE:
		if (decoded->link)
			regs->regs[30] = regs->pc + sizeof(u32);
		regs->pc += decoded->branch_imm;
		return 0;
	case TCTI_DECODE_UNCONDITIONAL_BRANCH_REGISTER:
		if (decoded->branch_register_op == TCTI_BRANCH_REGISTER_BLR)
			regs->regs[30] = regs->pc + sizeof(u32);
		regs->pc = tcti_read_gpr_or_zero(regs, decoded->rn,
						 sizeof(u64));
		return 0;
	case TCTI_DECODE_COMPARE_BRANCH_IMMEDIATE:
		source = tcti_read_gpr_or_zero(regs, decoded->rt,
					       decoded->is_64bit ?
					       sizeof(u64) : sizeof(u32));
		if ((!source) != decoded->nonzero)
			regs->pc += decoded->branch_imm;
		else
			regs->pc += sizeof(u32);
		return 0;
	case TCTI_DECODE_TEST_BRANCH_IMMEDIATE:
		source = tcti_read_gpr_or_zero(regs, decoded->rt, sizeof(u64));
		if (!!(source & BIT_ULL(decoded->test_bit)) ==
		    decoded->nonzero)
			regs->pc += decoded->branch_imm;
		else
			regs->pc += sizeof(u32);
		return 0;
	case TCTI_DECODE_CONDITIONAL_BRANCH_IMMEDIATE:
		if (tcti_condition_passed(regs, decoded->condition))
			regs->pc += decoded->branch_imm;
		else
			regs->pc += sizeof(u32);
		return 0;
	case TCTI_DECODE_CONDITIONAL_COMPARE:
		return tcti_execute_conditional_compare(regs, decoded);
	case TCTI_DECODE_CONDITIONAL_SELECT:
		return tcti_execute_conditional_select(regs, decoded);
	case TCTI_DECODE_LOAD_STORE_PAIR:
		return tcti_execute_load_store_pair(mm, regs, decoded,
						    fault_address);
	case TCTI_DECODE_LOAD_STORE_UNSIGNED_IMMEDIATE:
	case TCTI_DECODE_LOAD_STORE_SIGNED_IMMEDIATE:
		return tcti_execute_load_store_immediate(mm, regs, decoded,
							 fault_address);
	case TCTI_DECODE_LOAD_STORE_REGISTER_OFFSET:
		return tcti_execute_load_store_register_offset(mm, regs, decoded,
							       fault_address);
	case TCTI_DECODE_LOGICAL_SHIFTED_REGISTER:
		return tcti_execute_logical_shifted_register(regs, decoded);
	case TCTI_DECODE_LOGICAL_IMMEDIATE:
		return tcti_execute_logical_immediate(regs, decoded);
	case TCTI_DECODE_BITFIELD:
		return tcti_execute_bitfield(regs, decoded);
	case TCTI_DECODE_EXTRACT:
		return tcti_execute_extract(regs, decoded);
	case TCTI_DECODE_DATA_PROCESSING_1SOURCE:
		return tcti_execute_data_processing_1source(regs, decoded);
	case TCTI_DECODE_DATA_PROCESSING_2SOURCE:
		return tcti_execute_data_processing_2source(regs, decoded);
	case TCTI_DECODE_MULTIPLY_ADD_SUB:
		return tcti_execute_multiply_add_sub(regs, decoded);
	case TCTI_DECODE_MOVE_WIDE_IMMEDIATE:
		return tcti_execute_move_wide_immediate(regs, decoded);
	case TCTI_DECODE_SYSTEM_REGISTER:
		return tcti_execute_system_register(regs, decoded);
	case TCTI_DECODE_EXCLUSIVE_MONITOR_CLEAR:
		tcti_clear_exclusive_monitor();
		regs->pc += sizeof(u32);
		return 0;
	case TCTI_DECODE_LOAD_STORE_EXCLUSIVE:
		return tcti_execute_load_store_exclusive(mm, regs, decoded,
							 fault_address);
	case TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE:
		return tcti_execute_simd_modified_immediate(regs, decoded);
	case TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE:
		return tcti_execute_simd_vector_element_move(regs, decoded);
	case TCTI_DECODE_SIMD_VECTOR_LOGICAL:
		return tcti_execute_simd_vector_logical(regs, decoded);
	case TCTI_DECODE_SIMD_VECTOR_ARITHMETIC:
		return tcti_execute_simd_vector_arithmetic(regs, decoded);
	case TCTI_DECODE_SIMD_VECTOR_COMPARE:
		return tcti_execute_simd_vector_compare(regs, decoded);
	case TCTI_DECODE_SIMD_VECTOR_REDUCTION:
		return tcti_execute_simd_vector_reduction(regs, decoded);
	case TCTI_DECODE_SIMD_LOAD_REPLICATE:
		return tcti_execute_simd_load_replicate(mm, regs, decoded,
							fault_address);
	case TCTI_DECODE_FP_SCALAR_MOVE:
		return tcti_execute_fp_scalar_move(regs, decoded);
	case TCTI_DECODE_FP_SCALAR_1SOURCE:
		return tcti_execute_fp_scalar_1source(regs, decoded);
	case TCTI_DECODE_FP_SCALAR_2SOURCE:
		return tcti_execute_fp_scalar_2source(regs, decoded);
	case TCTI_DECODE_FP_SCALAR_3SOURCE:
		return tcti_execute_fp_scalar_3source(regs, decoded);
	case TCTI_DECODE_FP_SCALAR_COMPARE:
		return tcti_execute_fp_scalar_compare(regs, decoded);
	case TCTI_DECODE_FP_CONDITIONAL_SELECT:
		return tcti_execute_fp_conditional_select(regs, decoded);
	case TCTI_DECODE_FP_INT_CONVERT:
		return tcti_execute_fp_int_convert(regs, decoded);
	default:
		return -EOPNOTSUPP;
	}
}

#if IS_ENABLED(CONFIG_ORLIX_TCTI_DEBUG_SWITCH)
int tcti_switch_debug_execute_decoded(struct mm_struct *mm,
				      struct pt_regs *regs,
				      const struct tcti_decoded_instruction *decoded,
				      unsigned long *fault_address)
{
	return tcti_execute_decoded_semantics(mm, regs, decoded,
					      fault_address);
}

struct tcti_result tcti_switch_debug_resume_user(struct task_struct *task,
						 struct pt_regs *regs,
						 struct mm_struct *mm)
{
	struct tcti_result result = {
		.reason = TCTI_EXIT_TASK_EXIT,
		.status = -EINVAL,
	};
	struct tcti_decoded_instruction decoded;
	unsigned long fault_address;
	int ret;

	if (!task || !regs || !mm)
		return result;

	for (;;) {
		u32 instruction;

		ret = tcti_fetch_instruction(mm, regs->pc, &instruction);
		if (ret) {
			result.reason = TCTI_EXIT_USER_FAULT;
			result.status = ret;
			result.fault_address = regs->pc;
			result.fault_access = TCTI_ACCESS_FETCH;
			result.pc = regs->pc;
			return result;
		}

		decoded = tcti_decode_aarch64(instruction);
		if (decoded.decode_class == TCTI_DECODE_SVC) {
			result.reason = TCTI_EXIT_SYSCALL;
			result.status = 0;
			result.pc = regs->pc;
			result.instruction = instruction;
			return result;
		}

		fault_address = regs->pc;
		ret = tcti_switch_debug_execute_decoded(mm, regs, &decoded,
							&fault_address);
		if (!ret)
			continue;

		if (ret == -EFAULT || ret == -EACCES) {
			result.reason = TCTI_EXIT_USER_FAULT;
			result.status = ret;
			result.fault_address = fault_address;
			result.fault_access = tcti_fault_access_for_decoded(&decoded);
			result.pc = regs->pc;
			result.instruction = instruction;
			return result;
		}

		result.reason = TCTI_EXIT_UNSUPPORTED_INSTRUCTION;
		result.status = -ENOSYS;
		result.pc = regs->pc;
		result.instruction = instruction;
		return result;
	}
}
#endif
