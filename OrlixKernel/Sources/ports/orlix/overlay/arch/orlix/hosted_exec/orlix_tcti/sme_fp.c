/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/errno.h>
#include <linux/bitops.h>
#include <linux/string.h>
#include <linux/unaligned.h>
#include <linux/sched.h>

#include <asm/orlix_tcti.h>
#include <asm/ptrace.h>

#include "decode_aarch64.h"
#include "fixed_fp.h"
#include "sme_fp.h"
#include "sve_state.h"

#define ORLIX_TCTI_FPCR_RMODE_MASK GENMASK(23, 22)
#define ORLIX_TCTI_FPCR_AH BIT(26)
#define ORLIX_TCTI_FPCR_DN BIT(25)
#define ORLIX_TCTI_FPCR_FZ BIT(24)
#define ORLIX_TCTI_FPCR_FZ16 BIT(19)
#define ORLIX_TCTI_FPCR_FIZ BIT(0)
#define ORLIX_TCTI_FPSR_IOC BIT(0)
#define ORLIX_TCTI_FPSR_OFC BIT(2)
#define ORLIX_TCTI_FPSR_UFC BIT(3)
#define ORLIX_TCTI_FPSR_IXC BIT(4)
#define ORLIX_TCTI_FPSR_IDC BIT(7)

enum orlix_tcti_sme_bf16_operation {
	ORLIX_TCTI_SME_BF16_MAX,
	ORLIX_TCTI_SME_BF16_MIN,
	ORLIX_TCTI_SME_BF16_MAXNUM,
	ORLIX_TCTI_SME_BF16_MINNUM,
	ORLIX_TCTI_SME_BF16_SCALE,
};

/* Every SME MZ floating-size field is the Arm pseudocode `8 << UInt(size)`.
 * The source predicates reject size==0 for the floating (non-BF16) forms. */
static int orlix_tcti_sme_fp_element_bytes(u32 instruction)
{
	u8 size = (instruction >> 22) & 3U;

	if (!size)
		return -EINVAL;
	return 1U << (size + 3U);
}

static bool orlix_tcti_sme_bf16_nan(u16 value)
{
	return (value & 0x7f80U) == 0x7f80U && (value & 0x007fU);
}

static bool orlix_tcti_sme_bf16_snan(u16 value)
{
	return orlix_tcti_sme_bf16_nan(value) && !(value & 0x0040U);
}

static u16 orlix_tcti_sme_bf16_nan_result(u16 value, unsigned long fpcr,
	unsigned long *fpsr)
{
	if (orlix_tcti_sme_bf16_snan(value))
		*fpsr |= ORLIX_TCTI_FPSR_IOC;
	if (fpcr & ORLIX_TCTI_FPCR_DN)
		return 0x7fc0U;
	return value | 0x0040U;
}

/* Ordered comparison for finite non-zero BF16 encodings. */
static int orlix_tcti_sme_bf16_compare(u16 left, u16 right)
{
	u16 left_magnitude = left & 0x7fffU;
	u16 right_magnitude = right & 0x7fffU;

	if (left_magnitude == right_magnitude)
		return 0;
	if ((left ^ right) & 0x8000U)
		return left & 0x8000U ? -1 : 1;
	if (left & 0x8000U)
		return left_magnitude > right_magnitude ? -1 : 1;
	return left_magnitude > right_magnitude ? 1 : -1;
}

static u16 orlix_tcti_sme_bf16_minmax(u16 left, u16 right,
	enum orlix_tcti_sme_bf16_operation operation, unsigned long fpcr,
	unsigned long *fpsr)
{
	bool left_nan = orlix_tcti_sme_bf16_nan(left);
	bool right_nan = orlix_tcti_sme_bf16_nan(right);
	bool maximum = operation == ORLIX_TCTI_SME_BF16_MAX ||
		operation == ORLIX_TCTI_SME_BF16_MAXNUM;
	u16 numeric_nan_substitute = maximum ? 0xff80U : 0x7f80U;
	int comparison;

	if (operation == ORLIX_TCTI_SME_BF16_MAXNUM ||
	    operation == ORLIX_TCTI_SME_BF16_MINNUM) {
		if (left_nan && !orlix_tcti_sme_bf16_snan(left) && !right_nan)
			left = numeric_nan_substitute;
		else if (right_nan && !orlix_tcti_sme_bf16_snan(right) && !left_nan)
			right = numeric_nan_substitute;
		left_nan = orlix_tcti_sme_bf16_nan(left);
		right_nan = orlix_tcti_sme_bf16_nan(right);
	}
	if (left_nan)
		return orlix_tcti_sme_bf16_nan_result(left, fpcr, fpsr);
	if (right_nan)
		return orlix_tcti_sme_bf16_nan_result(right, fpcr, fpsr);
	if (!(left & 0x7fffU) && !(right & 0x7fffU))
		return maximum ? (left & right) : (left | right);
	comparison = orlix_tcti_sme_bf16_compare(left, right);
	if (comparison == 0)
		return right;
	return (comparison > 0) == maximum ? left : right;
}

static u16 orlix_tcti_sme_bf16_scale(u16 value, s16 scale,
	unsigned long fpcr, unsigned long *fpsr)
{
	u16 sign = value & 0x8000U;
	u16 fraction = value & 0x007fU;
	int exponent = (value >> 7) & 0xffU;
	int clamped;
	int output_exponent;
	u16 significand;
	unsigned int shift;
	u16 discarded;
	bool increment = false;

	if (orlix_tcti_sme_bf16_nan(value))
		return orlix_tcti_sme_bf16_nan_result(value, fpcr, fpsr);
	if (!exponent || exponent == 0xffU) {
		if (!exponent && fraction && (fpcr & ORLIX_TCTI_FPCR_FZ)) {
			*fpsr |= ORLIX_TCTI_FPSR_IDC;
			return sign;
		}
		return value;
	}
	clamped = scale;
	if (clamped < -8 - exponent)
		clamped = -8 - exponent;
	if (clamped > 263 - exponent)
		clamped = 263 - exponent;
	output_exponent = exponent + clamped;
	if (output_exponent >= 255) {
		*fpsr |= ORLIX_TCTI_FPSR_OFC | ORLIX_TCTI_FPSR_IXC;
		switch (fpcr & ORLIX_TCTI_FPCR_RMODE_MASK) {
		case BIT(22):
			return sign ? 0xff7fU : 0x7f80U;
		case BIT(23):
			return sign ? 0xff80U : 0x7f7fU;
		case GENMASK(23, 22):
			return sign | 0x7f7fU;
		default:
			return sign | 0x7f80U;
		}
	}
	if (output_exponent > 0)
		return sign | (output_exponent << 7) | fraction;
	significand = 0x80U | fraction;
	shift = 1U - output_exponent;
	discarded = significand & ((1U << shift) - 1U);
	significand >>= shift;
	if (discarded) {
		*fpsr |= ORLIX_TCTI_FPSR_UFC | ORLIX_TCTI_FPSR_IXC;
		switch (fpcr & ORLIX_TCTI_FPCR_RMODE_MASK) {
		case 0:
			increment = discarded > (1U << (shift - 1U)) ||
				(discarded == (1U << (shift - 1U)) && (significand & 1U));
			break;
		case BIT(22): increment = !sign; break;
		case BIT(23): increment = !!sign; break;
		default: break;
		}
		if (increment)
			significand++;
	}
	if (significand == 0x80U)
		return sign | 0x0080U;
	return sign | significand;
}

/* Arm FPScale{32,64}, expressed in integer form so host FP state cannot
 * affect the guest.  The clamp limits are FPClampScale's E/F-derived bounds.
 */
static u64 orlix_tcti_sme_fp_scale(u64 value, s64 scale, u8 bytes,
	unsigned long fpcr, unsigned long *fpsr)
{
	u8 exponent_bits = bytes == sizeof(u16) ? 5U :
		(bytes == sizeof(u32) ? 8U : 11U);
	u8 fraction_bits = bytes == sizeof(u16) ? 10U :
		(bytes == sizeof(u32) ? 23U : 52U);
	u64 sign_mask = bytes == sizeof(u16) ? BIT_ULL(15) :
		(bytes == sizeof(u32) ? BIT_ULL(31) : BIT_ULL(63));
	u64 exponent_mask = ((1ULL << exponent_bits) - 1ULL) << fraction_bits;
	u64 fraction_mask = (1ULL << fraction_bits) - 1ULL;
	u64 sign = value & sign_mask;
	u64 fraction = value & fraction_mask;
	s64 exponent = (value & exponent_mask) >> fraction_bits;
	s64 max_exponent = (1LL << exponent_bits) - 1LL;
	s64 minimum = -(s64)(fraction_bits + 1U) - exponent;
	s64 maximum = max_exponent + (s64)(fraction_bits + 1U) - exponent;
	s64 output_exponent;
	u64 significand;
	u64 discarded;
	u8 shift;
	bool increment = false;

	if (exponent == max_exponent) {
		if (fraction) {
			if (!(fraction & BIT_ULL(fraction_bits - 1U)))
				*fpsr |= ORLIX_TCTI_FPSR_IOC;
			if (fpcr & ORLIX_TCTI_FPCR_DN)
				return sign | exponent_mask | BIT_ULL(fraction_bits - 1U);
			return value | BIT_ULL(fraction_bits - 1U);
		}
		return value;
	}
	if (!exponent && !fraction)
		return sign;
	if (!exponent && (fpcr & ORLIX_TCTI_FPCR_FZ)) {
		*fpsr |= ORLIX_TCTI_FPSR_IDC;
		return sign;
	}
	if (scale < minimum)
		scale = minimum;
	if (scale > maximum)
		scale = maximum;
	/* Normalize a denormal before applying the same rounded exponent path. */
	if (!exponent) {
		exponent = 1;
		while (!(fraction & BIT_ULL(fraction_bits))) {
			fraction <<= 1;
			exponent--;
		}
		fraction &= fraction_mask;
	}
	output_exponent = exponent + scale;
	if (output_exponent >= max_exponent) {
		*fpsr |= ORLIX_TCTI_FPSR_OFC | ORLIX_TCTI_FPSR_IXC;
		switch (fpcr & ORLIX_TCTI_FPCR_RMODE_MASK) {
		case BIT(22): return sign ? (sign | exponent_mask | fraction_mask) : exponent_mask;
		case BIT(23): return sign ? (sign | exponent_mask) : (exponent_mask - 1ULL);
		case GENMASK(23, 22): return sign | (exponent_mask - 1ULL);
		default: return sign | exponent_mask;
		}
	}
	if (output_exponent > 0)
		return sign | ((u64)output_exponent << fraction_bits) | fraction;
	significand = BIT_ULL(fraction_bits) | fraction;
	shift = 1U - output_exponent;
	if (shift >= 64U) {
		/* A normalized denormal input can underflow by more than a u64
		 * significand width.  It is below half the least subnormal. */
		*fpsr |= ORLIX_TCTI_FPSR_UFC | ORLIX_TCTI_FPSR_IXC;
		if ((fpcr & ORLIX_TCTI_FPCR_RMODE_MASK) == BIT(22) && !sign)
			return 1U;
		if ((fpcr & ORLIX_TCTI_FPCR_RMODE_MASK) == BIT(23) && sign)
			return sign | 1U;
		return sign;
	}
	discarded = significand & (BIT_ULL(shift) - 1ULL);
	significand >>= shift;
	if (discarded) {
		*fpsr |= ORLIX_TCTI_FPSR_UFC | ORLIX_TCTI_FPSR_IXC;
		switch (fpcr & ORLIX_TCTI_FPCR_RMODE_MASK) {
		case 0:
			increment = discarded > BIT_ULL(shift - 1U) ||
				(discarded == BIT_ULL(shift - 1U) && (significand & 1U));
			break;
		case BIT(22): increment = !sign; break;
		case BIT(23): increment = !!sign; break;
		default: break;
		}
		if (increment)
			significand++;
	}
	if (significand == BIT_ULL(fraction_bits))
		return sign | BIT_ULL(fraction_bits);
	return sign | significand;
}

/* Arm A64 FSCALE MZ.ZZ Execute pseudocode, 2026-06. */
static int orlix_tcti_sme_fp_scale_vectors(struct orlix_tcti_sve_state *sve,
					     u16 ordinal, u32 instruction)
{
	int element_bytes = orlix_tcti_sme_fp_element_bytes(instruction);
	u8 registers;
	u8 destination;
	u8 source;
	bool single_source;
	u8 snapshot[4][ORLIX_TCTI_SVE_MAX_VL_BYTES];
	u8 r;
	u16 offset;

	if (ordinal != 2008U && ordinal != 2026U && ordinal != 2046U &&
	    ordinal != 2065U)
		return -EOPNOTSUPP;
	if (element_bytes != sizeof(u16) && element_bytes != sizeof(u32) &&
	    element_bytes != sizeof(u64))
		return -EINVAL;
	registers = ordinal == 2026U || ordinal == 2065U ? 4U : 2U;
	single_source = ordinal == 2008U || ordinal == 2026U;
	destination = ((instruction >> (registers == 4U ? 2U : 1U)) &
		(0x10U / registers - 1U)) * registers;
	source = single_source ? ((instruction >> 16) & 0xfU) :
		((instruction >> (registers == 4U ? 18U : 17U)) &
		 (0x10U / registers - 1U)) * registers;
	if (!sve->valid || !sve->vl_bytes || sve->vl_bytes % element_bytes ||
	    destination + registers > ORLIX_TCTI_SVE_ZREG_COUNT ||
	    source + (single_source ? 1U : registers) > ORLIX_TCTI_SVE_ZREG_COUNT)
		return -EINVAL;
	for (r = 0; r < (single_source ? 1U : registers); r++)
		memcpy(snapshot[r], sve->z[source + r], sve->vl_bytes);
	for (r = 0; r < registers; r++)
		for (offset = 0; offset < sve->vl_bytes; offset += element_bytes) {
			u64 value;
			s64 scale;
			u64 result;

			if (element_bytes == sizeof(u16)) {
				value = get_unaligned_le16(&sve->z[destination + r][offset]);
				scale = (s16)get_unaligned_le16(&snapshot[single_source ? 0U : r][offset]);
				result = orlix_tcti_sme_fp_scale(value, scale, element_bytes,
					current->thread.user_fpcr, &current->thread.user_fpsr);
				put_unaligned_le16((u16)result,
					&sve->z[destination + r][offset]);
			} else if (element_bytes == sizeof(u32)) {
				value = get_unaligned_le32(&sve->z[destination + r][offset]);
				scale = (s32)get_unaligned_le32(&snapshot[single_source ? 0U : r][offset]);
				result = orlix_tcti_sme_fp_scale(value, scale, element_bytes,
					current->thread.user_fpcr, &current->thread.user_fpsr);
				put_unaligned_le32((u32)result,
					&sve->z[destination + r][offset]);
			} else {
				value = get_unaligned_le64(&sve->z[destination + r][offset]);
				scale = (s64)get_unaligned_le64(&snapshot[single_source ? 0U : r][offset]);
				result = orlix_tcti_sme_fp_scale(value, scale, element_bytes,
					current->thread.user_fpcr, &current->thread.user_fpsr);
				put_unaligned_le64(result, &sve->z[destination + r][offset]);
			}
		}
	return 0;
}

static int orlix_tcti_sme_bf16_vectors(struct orlix_tcti_sve_state *sve,
	u16 ordinal, u32 instruction)
{
	enum orlix_tcti_sme_bf16_operation operation;
	u8 destination;
	u8 source;
	u8 registers;
	bool single_source;
	u8 snapshot[4][ORLIX_TCTI_SVE_MAX_VL_BYTES];
	u8 r;
	u16 offset;

	switch (ordinal) {
	case 2001: case 2019: case 2037: case 2056: operation = ORLIX_TCTI_SME_BF16_MAX; break;
	case 2003: case 2021: case 2039: case 2058: operation = ORLIX_TCTI_SME_BF16_MIN; break;
	case 2005: case 2023: case 2041: case 2060: operation = ORLIX_TCTI_SME_BF16_MAXNUM; break;
	case 2007: case 2025: case 2043: case 2062: operation = ORLIX_TCTI_SME_BF16_MINNUM; break;
	case 2009: case 2027: case 2047: case 2066: operation = ORLIX_TCTI_SME_BF16_SCALE; break;
	default: return -EOPNOTSUPP;
	}
	registers = ordinal >= 2056 || (ordinal >= 2019 && ordinal <= 2027) ? 4U : 2U;
	single_source = ordinal < 2037;
	if (registers == 2U) {
		destination = ((instruction >> 1) & 0xfU) * 2U;
		source = single_source ? ((instruction >> 16) & 0xfU) :
			((instruction >> 17) & 0xfU) * 2U;
	} else {
		destination = ((instruction >> 2) & 7U) * 4U;
		source = single_source ? ((instruction >> 16) & 0xfU) :
			((instruction >> 18) & 7U) * 4U;
	}
	if (!sve->valid || !sve->vl_bytes || sve->vl_bytes % sizeof(u16) ||
	    sve->vl_bytes > ORLIX_TCTI_SVE_MAX_VL_BYTES ||
	    destination + registers > ORLIX_TCTI_SVE_ZREG_COUNT ||
	    source + registers > ORLIX_TCTI_SVE_ZREG_COUNT)
		return -EINVAL;
	for (r = 0; r < registers; r++)
		memcpy(snapshot[r], sve->z[single_source ? source : source + r],
		       sve->vl_bytes);
	for (r = 0; r < registers; r++)
		for (offset = 0; offset < sve->vl_bytes; offset += sizeof(u16)) {
			u16 left = get_unaligned_le16(&sve->z[destination + r][offset]);
			u16 right = get_unaligned_le16(&snapshot[r][offset]);
			u16 result = operation == ORLIX_TCTI_SME_BF16_SCALE ?
				orlix_tcti_sme_bf16_scale(left, (s16)right, current->thread.user_fpcr,
					&current->thread.user_fpsr) :
				orlix_tcti_sme_bf16_minmax(left, right, operation,
					current->thread.user_fpcr, &current->thread.user_fpsr);

			put_unaligned_le16(result, &sve->z[destination + r][offset]);
		}
	return 0;
}

static u16 orlix_tcti_sme_bf16_round(u32 value, unsigned long fpcr)
{
	u32 low = value & 0xffffU;
	u32 high = value >> 16;
	bool increment;

	/* FPRoundBF{32}, with BFAdd_ZA's forced-default-NaN input FPCR. */
	if ((value & 0x7f800000U) == 0x7f800000U && (value & 0x007fffffU))
		return 0x7fc0U;
	switch (fpcr & ORLIX_TCTI_FPCR_RMODE_MASK) {
	case 0: /* nearest, ties to even */
		increment = low > 0x8000U || (low == 0x8000U && (high & 1U));
		break;
	case BIT(22): /* +infinity */
		increment = !(value & BIT(31)) && low;
		break;
	case BIT(23): /* -infinity */
		increment = !!(value & BIT(31)) && low;
		break;
	default: /* toward zero */
		increment = false;
		break;
	}
	return high + increment;
}

static int orlix_tcti_sme_bf16_add_sub(u8 *destination, const u8 *source,
	u16 bytes, bool subtract, unsigned long fpcr)
{
	u16 byte;

	for (byte = 0; byte < bytes; byte += 8U) {
		u64 left[2] = { 0, 0 };
		u64 right[2] = { 0, 0 };
		u64 result[2];
		unsigned long fpsr = 0;
		u8 lane;
		int ret;

		for (lane = 0; lane < 4U; lane++) {
			u32 lhs = (u32)get_unaligned_le16(destination + byte + lane * 2U) << 16;
			u32 rhs = (u32)get_unaligned_le16(source + byte + lane * 2U) << 16;

			if (subtract)
				rhs ^= BIT(31);
			if (lane < 2U) {
				left[0] |= (u64)lhs << (lane * 32U);
				right[0] |= (u64)rhs << (lane * 32U);
			} else {
				left[1] |= (u64)lhs << ((lane - 2U) * 32U);
				right[1] |= (u64)rhs << ((lane - 2U) * 32U);
			}
		}
		ret = orlix_tcti_native_simd_fp_three_same(ORLIX_TCTI_SIMD_ARITH_FADD,
			false, sizeof(u32), 16U, result, left, right, left,
			fpcr | ORLIX_TCTI_FPCR_DN, &fpsr);
		if (ret)
			return ret;
		current->thread.user_fpsr |= fpsr;
		for (lane = 0; lane < 4U; lane++) {
			u32 value = lane < 2U ? result[0] >> (lane * 32U) :
				result[1] >> ((lane - 2U) * 32U);

			put_unaligned_le16(orlix_tcti_sme_bf16_round(value, fpcr),
				destination + byte + lane * 2U);
		}
	}
	return 0;
}

/* FPAbsMax/FPAbsMin compare magnitudes but return op1_in or op2_in, not the
 * sign-cleared comparison value.  Preserve the selected original lane when
 * the native magnitude operation identifies it.  A synthesized NaN remains
 * the native helper's FPProcessNaNs result. */
static u64 orlix_tcti_sme_fp_restore_abs_selected_lanes(u64 selected,
	u64 left, u64 right, unsigned int element_bytes)
{
	u64 element_mask = element_bytes == sizeof(u16) ? 0xffffU :
		element_bytes == sizeof(u32) ? 0xffffffffU : ~0ULL;
	u64 sign_mask = BIT_ULL(element_bytes * 8U - 1U);
	unsigned int lane;
	u64 restored = 0;

	for (lane = 0; lane < sizeof(u64) / element_bytes; lane++) {
		unsigned int shift = lane * element_bytes * 8U;
		u64 chosen = (selected >> shift) & element_mask;
		u64 first = (left >> shift) & element_mask;
		u64 second = (right >> shift) & element_mask;
		u64 magnitude = chosen & ~sign_mask;

		if (magnitude == (first & ~sign_mask) &&
		    magnitude != (second & ~sign_mask))
			chosen = first;
		else if (magnitude == (second & ~sign_mask))
			chosen = second;
		restored |= chosen << shift;
	}
	return restored;
}


/*
 * SME2 multi-vector FMAX/FMIN use the same numerical operations as their
 * fixed AdvSIMD counterparts.  Run each 128-bit architectural slice through
 * the established FPCR/FPSR-aware helper so host FP state never becomes guest
 * state and accumulated exception flags remain architectural.
 */
static int orlix_tcti_sme_fp_minmax_vectors(
	struct orlix_tcti_sve_state *sve, u16 ordinal, u32 instruction)
{
	enum orlix_tcti_simd_vector_arithmetic_op operation;
	int element_bytes = orlix_tcti_sme_fp_element_bytes(instruction);
	u8 source;
	u8 destination;
	u8 registers;
	bool single_source;
	u8 snapshot[4][ORLIX_TCTI_SVE_MAX_VL_BYTES];
	u16 offset;
	u8 r;

	if (!sve->valid || !sve->vl_bytes || sve->vl_bytes % 16U ||
	    (element_bytes != sizeof(u16) && element_bytes != sizeof(u32) &&
	     element_bytes != sizeof(u64)))
		return -EINVAL;

	switch (ordinal) {
	case 2000: case 2018: case 2036: case 2055:
		operation = ORLIX_TCTI_SIMD_ARITH_FMAX;
		break;
	case 2002: case 2020: case 2038: case 2057:
		operation = ORLIX_TCTI_SIMD_ARITH_FMIN;
		break;
	case 2004: case 2022: case 2040: case 2059:
		operation = ORLIX_TCTI_SIMD_ARITH_FMAXNM;
		break;
	case 2006: case 2024: case 2042: case 2061:
		operation = ORLIX_TCTI_SIMD_ARITH_FMINNM;
		break;
	default:
		return -EOPNOTSUPP;
	}

	registers = (ordinal == 2018 || ordinal == 2020 || ordinal == 2022 ||
		     ordinal == 2024 || ordinal == 2055 || ordinal == 2057 ||
		     ordinal == 2059 || ordinal == 2061) ? 4U : 2U;
	single_source = ordinal < 2036U;
	/* Zdn is encoded in bits [4:1] or [4:2], then names its group base. */
	destination = ((instruction >> (registers == 4U ? 2U : 1U)) &
		(0x10U / registers - 1U)) * registers;
	source = single_source ? ((instruction >> 16) & 0xfU) :
		((instruction >> (registers == 4U ? 18U : 17U)) &
		 (0x10U / registers - 1U)) * registers;
	if (destination + registers > ORLIX_TCTI_SVE_ZREG_COUNT ||
	    source + (single_source ? 1U : registers) > ORLIX_TCTI_SVE_ZREG_COUNT)
		return -EINVAL;
	/* Vn is the low 128-bit architectural alias of Zn. */
	for (r = 0; r < registers; r++) {
		put_unaligned_le64(current->thread.user_simd[(destination + r) * 2],
			&sve->z[destination + r][0]);
		put_unaligned_le64(current->thread.user_simd[(destination + r) * 2 + 1],
			&sve->z[destination + r][8]);
	}
	for (r = 0; r < (single_source ? 1U : registers); r++)
		memcpy(snapshot[r], sve->z[source + r], sve->vl_bytes);

	for (r = 0; r < registers; r++) {
		for (offset = 0; offset < sve->vl_bytes; offset += 16U) {
			u64 result[2];
			u64 left[2];
			u64 right[2];
			int ret;

			left[0] = get_unaligned_le64(&sve->z[destination + r][offset]);
			left[1] = get_unaligned_le64(&sve->z[destination + r][offset + 8]);
			right[0] = get_unaligned_le64(&snapshot[single_source ? 0U : r][offset]);
			right[1] = get_unaligned_le64(&snapshot[single_source ? 0U : r][offset + 8]);
			if (element_bytes == sizeof(u16))
				ret = orlix_tcti_native_simd_fp16_three_same(operation,
					false, true, result, left, right, left,
					current->thread.user_fpcr, &current->thread.user_fpsr);
			else
				ret = orlix_tcti_native_simd_fp_three_same(operation, false,
					element_bytes, 16U, result, left, right, left,
					current->thread.user_fpcr, &current->thread.user_fpsr);
			if (ret)
				return ret;
			result[0] = orlix_tcti_sme_fp_restore_abs_selected_lanes(result[0],
				get_unaligned_le64(&sve->z[destination + r][offset]),
				get_unaligned_le64(&snapshot[r][offset]), element_bytes);
			result[1] = orlix_tcti_sme_fp_restore_abs_selected_lanes(result[1],
				get_unaligned_le64(&sve->z[destination + r][offset + 8U]),
				get_unaligned_le64(&snapshot[r][offset + 8U]), element_bytes);
			put_unaligned_le64(result[0], &sve->z[destination + r][offset]);
			put_unaligned_le64(result[1], &sve->z[destination + r][offset + 8]);
		}
		current->thread.user_simd[(destination + r) * 2] =
			get_unaligned_le64(&sve->z[destination + r][0]);
		current->thread.user_simd[(destination + r) * 2 + 1] =
			get_unaligned_le64(&sve->z[destination + r][8]);
		current->thread.user_simd_valid = 1;
	}
	return 0;
}

/* Arm A64 fadd_za_zw/fsub_za_zw Execute pseudocode, 2026-06 release. */
static int orlix_tcti_sme_fp_za_add_sub(struct pt_regs *regs,
	struct orlix_tcti_sme_state *sme, struct orlix_tcti_sve_state *sve,
	u16 ordinal, u32 instruction)
{
	enum orlix_tcti_simd_vector_arithmetic_op operation;
	u8 nreg;
	bool bf16;
	bool subtract;
	u8 esize;
	u8 m;
	u8 rv;
	u8 offset;
	u16 vector_bytes;
	u16 vectors;
	u16 vstride;
	u16 vec;
	u8 r;

	bf16 = ordinal == 1951 || ordinal == 1953 || ordinal == 1991 || ordinal == 1993;
	subtract = ordinal == 1947 || ordinal == 1952 || ordinal == 1953 ||
		ordinal == 1987 || ordinal == 1992 || ordinal == 1993;
	switch (ordinal) {
	case 1946: case 1986: case 1950: case 1990: case 1951: case 1991:
		operation = ORLIX_TCTI_SIMD_ARITH_FADD;
		break;
	case 1947: case 1987: case 1952: case 1953: case 1992: case 1993:
		operation = ORLIX_TCTI_SIMD_ARITH_FSUB;
		break;
	default:
		return -EOPNOTSUPP;
	}
	nreg = (ordinal >= 1986U && ordinal <= 1993U) ? 4U : 2U;
	esize = (ordinal == 1950 || ordinal == 1952 || ordinal == 1990 ||
		ordinal == 1992 || bf16) ? sizeof(u16) :
		((instruction & BIT(22)) ? sizeof(u64) : sizeof(u32));
	/* Zm is [9:6] for 2-register forms and [9:7] for 4-register forms;
	 * Rv is [14:13]. */
	m = ((instruction >> (nreg == 4U ? 7U : 6U)) &
		(0x10U / nreg - 1U)) * nreg;
	rv = (instruction >> 13) & 3U;
	offset = instruction & 7U;
	vector_bytes = sve->vl_bytes;
	vectors = vector_bytes / 8U;
	if (!sme->za || !sme->za_enabled || !sve->valid ||
	    vector_bytes != sme->svl_bytes || !vectors || vectors % nreg ||
	    m + nreg > ORLIX_TCTI_SVE_ZREG_COUNT || sme->za_bytes !=
	    (size_t)vector_bytes * vector_bytes)
		return -EINVAL;
	vstride = vectors / nreg;
	vec = ((u32)regs->regs[8U + rv] + offset) % vstride;
	for (r = 0; r < nreg; r++, vec += vstride) {
		u16 byte;
		u8 *za_vector = sme->za + (size_t)vec * vector_bytes;

		for (byte = 0; byte < vector_bytes; byte += 16U) {
			u64 result[2];
			u64 left[2] = {
				get_unaligned_le64(za_vector + byte),
				get_unaligned_le64(za_vector + byte + 8U),
			};
			u64 right[2] = {
				get_unaligned_le64(&sve->z[m + r][byte]),
				get_unaligned_le64(&sve->z[m + r][byte + 8U]),
			};
			int ret;

			if (bf16)
				ret = orlix_tcti_sme_bf16_add_sub(za_vector + byte,
					&sve->z[m + r][byte], 16U, subtract,
					current->thread.user_fpcr);
			else if (esize == sizeof(u16))
				ret = orlix_tcti_native_simd_fp16_three_same(operation,
					false, true, result, left, right, left,
					current->thread.user_fpcr,
					&current->thread.user_fpsr);
			else
				ret = orlix_tcti_native_simd_fp_three_same(operation,
					false, esize, 16U, result, left, right, left,
					current->thread.user_fpcr,
					&current->thread.user_fpsr);
			if (ret)
				return ret;
			/* The BF16 helper writes the ZA slice directly.  Its result is
			 * not represented by the native FP result buffer. */
			if (!bf16) {
				put_unaligned_le64(result[0], za_vector + byte);
				put_unaligned_le64(result[1], za_vector + byte + 8U);
			}
		}
	}
	return 0;
}

static u32 orlix_tcti_sme_fp16_to_fp32(u16 value)
{
	u32 sign = (u32)(value & 0x8000U) << 16;
	u32 fraction = value & 0x03ffU;
	u32 exponent = (value >> 10) & 0x1fU;

	if (!exponent) {
		if (!fraction)
			return sign;
		exponent = 113U;
		while (!(fraction & 0x0400U)) {
			fraction <<= 1;
			exponent--;
		}
		return sign | (exponent << 23) | ((fraction & 0x03ffU) << 13);
	}
	if (exponent == 0x1fU)
		return sign | 0x7f800000U | (fraction << 13);
	return sign | ((exponent + 112U) << 23) | (fraction << 13);
}

/* Arm A64 BFMLAL/FMLAL/BFMLSL/FMLSL ZA.ZZW Execute pseudocode, 2026-06. */
static int orlix_tcti_sme_fp_long_matrix(struct pt_regs *regs,
	struct orlix_tcti_sme_state *sme, struct orlix_tcti_sve_state *sve,
	u16 ordinal, u32 instruction)
{
	enum orlix_tcti_simd_vector_arithmetic_op operation;
	u8 source_group = (instruction & 3U) * 4U;
	u8 source = (instruction >> 16) & 0x1fU;
	u8 rv = (instruction >> 13) & 3U;
	u8 offset = (instruction >> 7) & 3U;
	u16 vector_bytes = sve->vl_bytes;
	u16 vectors = vector_bytes;
	u16 vstride;
	u16 vec;
	u8 r;
	bool bf16;

	switch (ordinal) {
	case 1960U:
		bf16 = true;
		operation = ORLIX_TCTI_SIMD_ARITH_FMLA;
		break;
	case 1961U:
		bf16 = false;
		operation = ORLIX_TCTI_SIMD_ARITH_FMLA;
		break;
	case 1962U:
		bf16 = true;
		operation = ORLIX_TCTI_SIMD_ARITH_FMLS;
		break;
	case 1963U:
		bf16 = false;
		operation = ORLIX_TCTI_SIMD_ARITH_FMLS;
		break;
	default:
		return -EOPNOTSUPP;
	}
	if (!sme->za || !sme->za_enabled || !sve->valid ||
	    vector_bytes != sme->svl_bytes || vector_bytes % 16U ||
	    source_group + 4U > ORLIX_TCTI_SVE_ZREG_COUNT ||
	    source >= ORLIX_TCTI_SVE_ZREG_COUNT || vectors % 4U ||
	    sme->za_bytes != (size_t)vector_bytes * vector_bytes)
		return -EINVAL;
	vstride = vectors / 4U;
	vec = ((u32)regs->regs[8U + rv] + offset) % vstride;
	for (r = 0; r < 4U; r++) {
		u8 parity;

		for (parity = 0; parity < 2U; parity++) {
			u8 *za_vector = sme->za +
				(size_t)(vec + r * vstride + parity) * vector_bytes;
			u16 byte;

			for (byte = 0; byte < vector_bytes; byte += 16U) {
				u64 result[2];
				u64 left[2] = {
					get_unaligned_le64(za_vector + byte),
					get_unaligned_le64(za_vector + byte + 8U),
				};
				u64 multiplicand[2] = { 0, 0 };
				u64 multiplier[2] = { 0, 0 };
				u8 lane;
				int ret;

				for (lane = 0; lane < 4U; lane++) {
					u16 first = get_unaligned_le16(
						&sve->z[source_group + r][byte +
							(lane * 2U + parity) * sizeof(u16)]);
					u16 second = get_unaligned_le16(&sve->z[source][byte +
						(lane * 2U + parity) * sizeof(u16)]);
					u32 first32 = bf16 ? (u32)first << 16 :
						orlix_tcti_sme_fp16_to_fp32(first);
					u32 second32 = bf16 ? (u32)second << 16 :
						orlix_tcti_sme_fp16_to_fp32(second);

					if (lane < 2U) {
						multiplicand[0] |= (u64)first32 << (lane * 32U);
						multiplier[0] |= (u64)second32 << (lane * 32U);
					} else {
						multiplicand[1] |= (u64)first32 <<
							((lane - 2U) * 32U);
						multiplier[1] |= (u64)second32 <<
							((lane - 2U) * 32U);
					}
				}
				ret = orlix_tcti_native_simd_fp_three_same(operation, false,
					sizeof(u32), 16U, result, multiplicand, multiplier,
					left, current->thread.user_fpcr,
					&current->thread.user_fpsr);
				if (ret)
					return ret;
				put_unaligned_le64(result[0], za_vector + byte);
				put_unaligned_le64(result[1], za_vector + byte + 8U);
			}
		}
	}
	return 0;
}

static int orlix_tcti_sme_bf16_mul_add(u8 *destination, const u8 *left_source,
	const u8 *right_source, u16 bytes, bool subtract, unsigned long fpcr)
{
	u16 byte;

	for (byte = 0; byte < bytes; byte += 8U) {
		u64 accumulator[2] = { 0, 0 };
		u64 left[2] = { 0, 0 };
		u64 right[2] = { 0, 0 };
		u64 result[2];
		unsigned long fpsr = 0;
		u8 lane;
		int ret;

		for (lane = 0; lane < 4U; lane++) {
			u32 sum = (u32)get_unaligned_le16(destination + byte + lane * 2U) << 16;
			u32 first = (u32)get_unaligned_le16(left_source + byte + lane * 2U) << 16;
			u32 second = (u32)get_unaligned_le16(right_source + byte + lane * 2U) << 16;

			if (lane < 2U) {
				accumulator[0] |= (u64)sum << (lane * 32U);
				left[0] |= (u64)first << (lane * 32U);
				right[0] |= (u64)second << (lane * 32U);
			} else {
				accumulator[1] |= (u64)sum << ((lane - 2U) * 32U);
				left[1] |= (u64)first << ((lane - 2U) * 32U);
				right[1] |= (u64)second << ((lane - 2U) * 32U);
			}
		}
		ret = orlix_tcti_native_simd_fp_three_same(
			subtract ? ORLIX_TCTI_SIMD_ARITH_FMLS : ORLIX_TCTI_SIMD_ARITH_FMLA,
			false, sizeof(u32), 16U, result, left, right, accumulator,
			fpcr | ORLIX_TCTI_FPCR_DN, &fpsr);
		if (ret)
			return ret;
		current->thread.user_fpsr |= fpsr;
		for (lane = 0; lane < 4U; lane++) {
			u32 value = lane < 2U ? result[0] >> (lane * 32U) :
				result[1] >> ((lane - 2U) * 32U);

			put_unaligned_le16(orlix_tcti_sme_bf16_round(value, fpcr),
				destination + byte + lane * 2U);
		}
	}
	return 0;
}

/* Arm A64 FMLA/BFMLA/FMLS/BFMLS ZA.ZZW Execute pseudocode, 2026-06. */
static int orlix_tcti_sme_fp_matrix_mul(struct pt_regs *regs,
	struct orlix_tcti_sme_state *sme, struct orlix_tcti_sve_state *sve,
	u16 ordinal, u32 instruction)
{
	u8 first = (instruction & 7U) * 4U;
	u8 second = ((instruction >> 18) & 7U) * 4U;
	u8 rv = (instruction >> 13) & 3U;
	u8 offset = (instruction >> 7) & 7U;
	u16 vector_bytes = sve->vl_bytes;
	u16 vectors = vector_bytes / 8U;
	u16 vstride;
	u16 vec;
	u8 r;
	bool bf16;
	bool subtract;

	switch (ordinal) {
	case 1973U: bf16 = false; subtract = false; break;
	case 1974U: bf16 = true; subtract = false; break;
	case 1975U: bf16 = false; subtract = true; break;
	case 1976U: bf16 = true; subtract = true; break;
	default: return -EOPNOTSUPP;
	}
	if (!sme->za || !sme->za_enabled || !sve->valid ||
	    vector_bytes != sme->svl_bytes || vector_bytes % 16U ||
	    !vectors || vectors % 4U || first + 4U > ORLIX_TCTI_SVE_ZREG_COUNT ||
	    second + 4U > ORLIX_TCTI_SVE_ZREG_COUNT ||
	    sme->za_bytes != (size_t)vector_bytes * vector_bytes)
		return -EINVAL;
	vstride = vectors / 4U;
	vec = ((u32)regs->regs[8U + rv] + offset) % vstride;
	for (r = 0; r < 4U; r++, vec += vstride) {
		u8 *za_vector = sme->za + (size_t)vec * vector_bytes;
		u16 byte;

		for (byte = 0; byte < vector_bytes; byte += 16U) {
			int ret;

			if (bf16)
				ret = orlix_tcti_sme_bf16_mul_add(za_vector + byte,
					&sve->z[first + r][byte], &sve->z[second + r][byte],
					16U, subtract, current->thread.user_fpcr);
			else {
				u64 result[2];
				u64 accumulator[2] = {
					get_unaligned_le64(za_vector + byte),
					get_unaligned_le64(za_vector + byte + 8U),
				};
				u64 left[2] = {
					get_unaligned_le64(&sve->z[first + r][byte]),
					get_unaligned_le64(&sve->z[first + r][byte + 8U]),
				};
				u64 right[2] = {
					get_unaligned_le64(&sve->z[second + r][byte]),
					get_unaligned_le64(&sve->z[second + r][byte + 8U]),
				};

				ret = orlix_tcti_native_simd_fp16_three_same(
					subtract ? ORLIX_TCTI_SIMD_ARITH_FMLS :
					ORLIX_TCTI_SIMD_ARITH_FMLA, false, true, result, left,
					right, accumulator, current->thread.user_fpcr,
					&current->thread.user_fpsr);
				if (!ret) {
					put_unaligned_le64(result[0], za_vector + byte);
					put_unaligned_le64(result[1], za_vector + byte + 8U);
				}
			}
			if (ret)
				return ret;
		}
	}
	return 0;
}

/* Arm A64 FDOT/BFDOT ZA.ZZW Execute pseudocode, 2026-06. */
static int orlix_tcti_sme_fp_matrix_dot(struct pt_regs *regs,
	struct orlix_tcti_sme_state *sme, struct orlix_tcti_sve_state *sve,
	u16 ordinal, u32 instruction)
{
	u8 first = (instruction & 7U) * 4U;
	u8 second = ((instruction >> 18) & 7U) * 4U;
	u8 rv = (instruction >> 13) & 3U;
	u8 offset = (instruction >> 7) & 7U;
	u16 vector_bytes = sve->vl_bytes;
	u16 vectors = vector_bytes / 8U;
	u16 vstride;
	u16 vec;
	u8 r;
	bool bf16;

	if (ordinal == 1969U)
		bf16 = false;
	else if (ordinal == 1970U)
		bf16 = true;
	else
		return -EOPNOTSUPP;
	if (!sme->za || !sme->za_enabled || !sve->valid ||
	    vector_bytes != sme->svl_bytes || vector_bytes % 16U ||
	    !vectors || vectors % 4U || first + 4U > ORLIX_TCTI_SVE_ZREG_COUNT ||
	    second + 4U > ORLIX_TCTI_SVE_ZREG_COUNT ||
	    sme->za_bytes != (size_t)vector_bytes * vector_bytes)
		return -EINVAL;
	vstride = vectors / 4U;
	vec = ((u32)regs->regs[8U + rv] + offset) % vstride;
	for (r = 0; r < 4U; r++, vec += vstride) {
		u8 *za_vector = sme->za + (size_t)vec * vector_bytes;
		u16 byte;

		for (byte = 0; byte < vector_bytes; byte += 16U) {
			u64 accumulator[2] = {
				get_unaligned_le64(za_vector + byte),
				get_unaligned_le64(za_vector + byte + 8U),
			};
			u64 first_even[2] = { 0, 0 };
			u64 second_even[2] = { 0, 0 };
			u64 first_odd[2] = { 0, 0 };
			u64 second_odd[2] = { 0, 0 };
			u64 result[2];
			u8 lane;
			int ret;

			for (lane = 0; lane < 4U; lane++) {
				u16 a0 = get_unaligned_le16(&sve->z[first + r][byte + lane * 4U]);
				u16 b0 = get_unaligned_le16(&sve->z[second + r][byte + lane * 4U]);
				u16 a1 = get_unaligned_le16(&sve->z[first + r][byte + lane * 4U + 2U]);
				u16 b1 = get_unaligned_le16(&sve->z[second + r][byte + lane * 4U + 2U]);
				u32 a0_32 = bf16 ? (u32)a0 << 16 : orlix_tcti_sme_fp16_to_fp32(a0);
				u32 b0_32 = bf16 ? (u32)b0 << 16 : orlix_tcti_sme_fp16_to_fp32(b0);
				u32 a1_32 = bf16 ? (u32)a1 << 16 : orlix_tcti_sme_fp16_to_fp32(a1);
				u32 b1_32 = bf16 ? (u32)b1 << 16 : orlix_tcti_sme_fp16_to_fp32(b1);
				u8 half = lane / 2U;
				u8 shift = (lane % 2U) * 32U;

				first_even[half] |= (u64)a0_32 << shift;
				second_even[half] |= (u64)b0_32 << shift;
				first_odd[half] |= (u64)a1_32 << shift;
				second_odd[half] |= (u64)b1_32 << shift;
			}
			ret = orlix_tcti_native_simd_fp_three_same(
				ORLIX_TCTI_SIMD_ARITH_FMLA, false, sizeof(u32), 16U,
				result, first_even, second_even, accumulator,
				current->thread.user_fpcr, &current->thread.user_fpsr);
			if (ret)
				return ret;
			ret = orlix_tcti_native_simd_fp_three_same(
				ORLIX_TCTI_SIMD_ARITH_FMLA, false, sizeof(u32), 16U,
				result, first_odd, second_odd, result,
				current->thread.user_fpcr, &current->thread.user_fpsr);
			if (ret)
				return ret;
			put_unaligned_le64(result[0], za_vector + byte);
			put_unaligned_le64(result[1], za_vector + byte + 8U);
		}
	}
	return 0;
}

/* Arm A64 FAMAX/FAMIN MZ.ZZW Execute pseudocode, 2026-06.
 *
 * FPAbs{Max,Min} clears the sign before the numerical comparison.  The
 * established fixed-FP helper supplies FPProcessNaNs and the guest FPCR/FPSR
 * behaviour; feeding it the sign-cleared operands gives the required
 * non-NaN result without depending on host floating point state.
 */
static int orlix_tcti_sme_fp_abs_minmax_vectors(
	struct orlix_tcti_sve_state *sve, u16 ordinal, u32 instruction)
{
	enum orlix_tcti_simd_vector_arithmetic_op operation;
	int element_bytes = orlix_tcti_sme_fp_element_bytes(instruction);
	u8 registers;
	u8 destination;
	u8 source;
	u8 snapshot[4][ORLIX_TCTI_SVE_MAX_VL_BYTES];
	u8 r;
	u16 offset;
	unsigned long fpcr;

	switch (ordinal) {
	case 2044U: case 2063U:
		operation = ORLIX_TCTI_SIMD_ARITH_FMAX;
		break;
	case 2045U: case 2064U:
		operation = ORLIX_TCTI_SIMD_ARITH_FMIN;
		break;
	default:
		return -EOPNOTSUPP;
	}
	registers = ordinal >= 2063U ? 4U : 2U;
	destination = ((instruction >> (registers == 4U ? 2U : 1U)) &
		(0x10U / registers - 1U)) * registers;
	source = ((instruction >> (registers == 4U ? 18U : 17U)) &
		(0x10U / registers - 1U)) * registers;
	if (!sve->valid || !sve->vl_bytes || sve->vl_bytes % 16U ||
	    (element_bytes != sizeof(u16) && element_bytes != sizeof(u32) &&
	     element_bytes != sizeof(u64)) ||
	    destination + registers > ORLIX_TCTI_SVE_ZREG_COUNT ||
	    source + registers > ORLIX_TCTI_SVE_ZREG_COUNT)
		return -EINVAL;
	for (r = 0; r < registers; r++)
		memcpy(snapshot[r], sve->z[source + r], sve->vl_bytes);
	for (r = 0; r < registers; r++) {
		for (offset = 0; offset < sve->vl_bytes; offset += 16U) {
			u64 left[2] = {
				get_unaligned_le64(&sve->z[destination + r][offset]),
				get_unaligned_le64(&sve->z[destination + r][offset + 8U]),
			};
			u64 right[2] = {
				get_unaligned_le64(&snapshot[r][offset]),
				get_unaligned_le64(&snapshot[r][offset + 8U]),
			};
			u64 result[2];
			int ret;

			if (element_bytes == sizeof(u32)) {
				left[0] &= 0x7fffffff7fffffffULL;
				left[1] &= 0x7fffffff7fffffffULL;
				right[0] &= 0x7fffffff7fffffffULL;
				right[1] &= 0x7fffffff7fffffffULL;
			} else if (element_bytes == sizeof(u64)) {
				left[0] &= ~BIT_ULL(63);
				left[1] &= ~BIT_ULL(63);
				right[0] &= ~BIT_ULL(63);
				right[1] &= ~BIT_ULL(63);
			} else {
				left[0] &= 0x7fff7fff7fff7fffULL;
				left[1] &= 0x7fff7fff7fff7fffULL;
				right[0] &= 0x7fff7fff7fff7fffULL;
				right[1] &= 0x7fff7fff7fff7fffULL;
			}
			/* FPAbs{Max,Min} clears AH/FIZ/FZ/FZ16 before FPUnpack. */
			fpcr = current->thread.user_fpcr & ~(ORLIX_TCTI_FPCR_AH |
				ORLIX_TCTI_FPCR_FIZ | ORLIX_TCTI_FPCR_FZ |
				ORLIX_TCTI_FPCR_FZ16);
			if (element_bytes == sizeof(u16))
				ret = orlix_tcti_native_simd_fp16_three_same(operation,
					false, true, result, left, right, left, fpcr,
					&current->thread.user_fpsr);
			else
				ret = orlix_tcti_native_simd_fp_three_same(operation, false,
					element_bytes, 16U, result, left, right, left, fpcr,
					&current->thread.user_fpsr);
			if (ret)
				return ret;
			put_unaligned_le64(result[0], &sve->z[destination + r][offset]);
			put_unaligned_le64(result[1], &sve->z[destination + r][offset + 8U]);
		}
	}
	return 0;
}

/* Arm A64 FCLAMP/BFCLAMP MZ.ZZ Execute pseudocode, 2026-06. */
static int orlix_tcti_sme_fp_clamp_vectors(struct orlix_tcti_sve_state *sve,
					     u16 ordinal, u32 instruction)
{
	bool bf16 = ordinal == 2071U || ordinal == 2075U;
	u8 registers;
	u8 destination;
	/* FCLAMP's scalar Zn field is bits [9:5]; Zm is bits [20:16]. */
	u8 lower = (instruction >> 5) & 0x1fU;
	u8 upper = (instruction >> 16) & 0x1fU;
	int element_bytes = bf16 ? sizeof(u16) :
		orlix_tcti_sme_fp_element_bytes(instruction);
	u8 destination_snapshot[4][ORLIX_TCTI_SVE_MAX_VL_BYTES];
	u8 r;
	u16 offset;

	if (ordinal != 2070U && ordinal != 2071U && ordinal != 2074U &&
	    ordinal != 2075U)
		return -EOPNOTSUPP;
	registers = ordinal >= 2074U ? 4U : 2U;
	destination = ((instruction >> (registers == 4U ? 2U : 1U)) &
		(0x10U / registers - 1U)) * registers;
	if (!sve->valid || !sve->vl_bytes || sve->vl_bytes % 16U ||
	    (element_bytes != sizeof(u16) && element_bytes != sizeof(u32) &&
	     element_bytes != sizeof(u64)) || destination + registers >
	    ORLIX_TCTI_SVE_ZREG_COUNT || lower >= ORLIX_TCTI_SVE_ZREG_COUNT ||
	    upper >= ORLIX_TCTI_SVE_ZREG_COUNT)
		return -EINVAL;
	for (r = 0; r < registers; r++)
		memcpy(destination_snapshot[r], sve->z[destination + r], sve->vl_bytes);
	for (r = 0; r < registers; r++) {
		for (offset = 0; offset < sve->vl_bytes; offset += 16U) {
			if (bf16) {
				u16 lane;

				for (lane = 0; lane < 16U; lane += sizeof(u16)) {
					u16 first = get_unaligned_le16(&sve->z[lower][offset + lane]);
					u16 second = get_unaligned_le16(&sve->z[upper][offset + lane]);
					u16 third = get_unaligned_le16(&destination_snapshot[r][offset + lane]);
					u16 maximum = orlix_tcti_sme_bf16_minmax(first, third,
						ORLIX_TCTI_SME_BF16_MAXNUM,
						current->thread.user_fpcr, &current->thread.user_fpsr);

					put_unaligned_le16(orlix_tcti_sme_bf16_minmax(maximum, second,
						ORLIX_TCTI_SME_BF16_MINNUM,
						current->thread.user_fpcr, &current->thread.user_fpsr),
						&sve->z[destination + r][offset + lane]);
				}
			} else {
				u64 first[2] = { get_unaligned_le64(&sve->z[lower][offset]),
					get_unaligned_le64(&sve->z[lower][offset + 8U]) };
				u64 second[2] = { get_unaligned_le64(&sve->z[upper][offset]),
					get_unaligned_le64(&sve->z[upper][offset + 8U]) };
				u64 third[2] = { get_unaligned_le64(&destination_snapshot[r][offset]),
					get_unaligned_le64(&destination_snapshot[r][offset + 8U]) };
				u64 maximum[2], result[2];
				int ret;

				if (element_bytes == sizeof(u16))
					ret = orlix_tcti_native_simd_fp16_three_same(
						ORLIX_TCTI_SIMD_ARITH_FMAXNM, false, true,
						maximum, first, third, first,
						current->thread.user_fpcr,
						&current->thread.user_fpsr);
				else
					ret = orlix_tcti_native_simd_fp_three_same(
						ORLIX_TCTI_SIMD_ARITH_FMAXNM, false,
						element_bytes, 16U, maximum, first, third, first,
						current->thread.user_fpcr,
						&current->thread.user_fpsr);
				if (ret)
					return ret;
				if (element_bytes == sizeof(u16))
					ret = orlix_tcti_native_simd_fp16_three_same(
						ORLIX_TCTI_SIMD_ARITH_FMINNM, false, true,
						result, maximum, second, maximum,
						current->thread.user_fpcr,
						&current->thread.user_fpsr);
				else
					ret = orlix_tcti_native_simd_fp_three_same(
						ORLIX_TCTI_SIMD_ARITH_FMINNM, false,
						element_bytes, 16U, result, maximum, second,
						maximum, current->thread.user_fpcr,
						&current->thread.user_fpsr);
				if (ret)
					return ret;
				put_unaligned_le64(result[0], &sve->z[destination + r][offset]);
				put_unaligned_le64(result[1], &sve->z[destination + r][offset + 8U]);
			}
		}
	}
	return 0;
}

/*
 * The MZ conversion encodings carry both multi-vector bases in their low
 * fields.  Keep the source group snapshot separate: MZ groups may overlap,
 * and architectural reads must precede every write in the group.
 */
static int orlix_tcti_sme_fp_convert_vectors(
	struct orlix_tcti_sve_state *sve, u16 ordinal, u32 instruction)
{
	enum orlix_tcti_fp_int_convert_op convert;
	enum orlix_tcti_simd_vector_arithmetic_op round;
	u8 destination;
	u8 source;
	u8 registers;
	u8 snapshot[4][ORLIX_TCTI_SVE_MAX_VL_BYTES];
	u8 r;
	u16 offset;
	bool integer_convert = false;
	bool rounding = false;

	switch (ordinal) {
	case 2095: case 2120:
		convert = ORLIX_TCTI_FP_INT_FCVTZS_SIMD; integer_convert = true; break;
	case 2096: case 2121:
		convert = ORLIX_TCTI_FP_INT_FCVTZU_SIMD; integer_convert = true; break;
	case 2097: case 2122:
		convert = ORLIX_TCTI_FP_INT_SCVTF_SIMD; integer_convert = true; break;
	case 2098: case 2123:
		convert = ORLIX_TCTI_FP_INT_UCVTF_SIMD; integer_convert = true; break;
	case 2114: case 2138: round = ORLIX_TCTI_SIMD_ARITH_FRINTN; rounding = true; break;
	case 2115: case 2139: round = ORLIX_TCTI_SIMD_ARITH_FRINTP; rounding = true; break;
	case 2116: case 2140: round = ORLIX_TCTI_SIMD_ARITH_FRINTM; rounding = true; break;
	case 2117: case 2141: round = ORLIX_TCTI_SIMD_ARITH_FRINTA; rounding = true; break;
	default:
		return -EOPNOTSUPP;
	}
	registers = ordinal >= 2120U ? 4U : 2U;
	/* The 4-register forms shorten each group field by one bit: Zd is
	 * [4:2] and Zn is [9:7], rather than the 2-register [4:1]/[9:6]. */
	destination = ((instruction >> (registers == 4U ? 2U : 1U)) &
		(0x10U / registers - 1U)) * registers;
	source = ((instruction >> (registers == 4U ? 7U : 6U)) &
		(0x10U / registers - 1U)) * registers;
	if (!sve->valid || !sve->vl_bytes || sve->vl_bytes % 16U ||
	    sve->vl_bytes > ORLIX_TCTI_SVE_MAX_VL_BYTES ||
	    destination + registers > ORLIX_TCTI_SVE_ZREG_COUNT ||
	    source + registers > ORLIX_TCTI_SVE_ZREG_COUNT)
		return -EINVAL;
	for (r = 0; r < registers; r++)
		memcpy(snapshot[r], sve->z[source + r], sve->vl_bytes);
	for (r = 0; r < registers; r++) {
		for (offset = 0; offset < sve->vl_bytes; offset += 16U) {
			u64 input[2] = {
				get_unaligned_le64(&snapshot[r][offset]),
				get_unaligned_le64(&snapshot[r][offset + 8U]),
			};
			u64 result[2];
			int ret;

			if (integer_convert)
				ret = orlix_tcti_native_simd_fp_convert(convert, false, true,
					sizeof(u32), result, input,
					current->thread.user_fpcr, &current->thread.user_fpsr);
			else if (rounding)
				ret = orlix_tcti_native_simd_fp_two_register(round,
					sizeof(u32), sizeof(u32), 0, 0, result, input, input,
					current->thread.user_fpcr, &current->thread.user_fpsr);
			else
				return -EOPNOTSUPP;
			if (ret)
				return ret;
			put_unaligned_le64(result[0], &sve->z[destination + r][offset]);
			put_unaligned_le64(result[1], &sve->z[destination + r][offset + 8U]);
		}
	}
	return 0;
}

/* FCVT/FCVTL use the FPCR/FPSR-aware scalar primitive lane by lane. */
static int orlix_tcti_sme_fp_width_convert_vectors(
	struct orlix_tcti_sve_state *sve, u16 ordinal, u32 instruction)
{
	u8 destination;
	u8 source;
	u8 snapshot[ORLIX_TCTI_SVE_MAX_VL_BYTES];
	u16 offset;

	if (ordinal != 2118U && ordinal != 2119U)
		return -EOPNOTSUPP;
	destination = ((instruction >> 1) & 0xfU) * 2U;
	source = (instruction >> 5) & 0x1fU;
	if (!sve->valid || !sve->vl_bytes || sve->vl_bytes % sizeof(u16) ||
	    sve->vl_bytes > ORLIX_TCTI_SVE_MAX_VL_BYTES ||
	    destination + 2U > ORLIX_TCTI_SVE_ZREG_COUNT ||
	    source >= ORLIX_TCTI_SVE_ZREG_COUNT)
		return -EINVAL;
	memcpy(snapshot, sve->z[source], sve->vl_bytes);
	if (ordinal == 2118U) {
		/* FCVT widens every half-precision lane in Zn across the concatenated
		 * two-vector destination { Zd, Zd + 1 }. */
		for (offset = 0; offset < sve->vl_bytes; offset += sizeof(u16)) {
			u64 input[2] = { 0, 0 };
			u64 output[2] = { 0, 0 };
			u16 destination_byte = offset * 2U;
			int ret;

			input[0] = get_unaligned_le16(&snapshot[offset]);
			ret = orlix_tcti_native_fp_one_source(ORLIX_TCTI_FP1_FCVT,
				sizeof(u16), sizeof(u32), output, input,
				current->thread.user_fpcr, &current->thread.user_fpsr);
			if (ret)
				return ret;
			put_unaligned_le32((u32)output[0],
				&sve->z[destination + destination_byte / sve->vl_bytes]
					[destination_byte % sve->vl_bytes]);
		}
	} else {
		/* FCVTL de-interleaves the even and odd half-precision lanes into
		 * Zd and Zd+1 respectively. */
		for (offset = 0; offset < sve->vl_bytes; offset += sizeof(u32)) {
			u64 input[2] = { 0, 0 };
			u64 output[2] = { 0, 0 };
			int ret;

			input[0] = get_unaligned_le16(&snapshot[offset]);
			ret = orlix_tcti_native_fp_one_source(ORLIX_TCTI_FP1_FCVT,
				sizeof(u16), sizeof(u32), output, input,
				current->thread.user_fpcr, &current->thread.user_fpsr);
			if (ret)
				return ret;
			put_unaligned_le32((u32)output[0],
				&sve->z[destination][offset]);
			input[0] = get_unaligned_le16(&snapshot[offset + sizeof(u16)]);
			ret = orlix_tcti_native_fp_one_source(ORLIX_TCTI_FP1_FCVT,
				sizeof(u16), sizeof(u32), output, input,
				current->thread.user_fpcr, &current->thread.user_fpsr);
			if (ret)
				return ret;
			put_unaligned_le32((u32)output[0],
				&sve->z[destination + 1U][offset]);
		}
	}
	return 0;
}

/* The narrow MZ2 forms consume a two-register source group and either replace
 * the low narrow half or append it above the existing narrow lanes. */
static int orlix_tcti_sme_fp_narrow_vectors(
	struct orlix_tcti_sve_state *sve, u16 ordinal, u32 instruction)
{
	u8 destination = instruction & 0x1fU;
	u8 source = ((instruction >> 6) & 0xfU) * 2U;
	u8 snapshot[2][ORLIX_TCTI_SVE_MAX_VL_BYTES];
	u16 offset;
	bool append;

	if (ordinal < 2091U || ordinal > 2094U || !sve->valid ||
	    !sve->vl_bytes || sve->vl_bytes % 16U ||
	    sve->vl_bytes > ORLIX_TCTI_SVE_MAX_VL_BYTES ||
	    destination >= ORLIX_TCTI_SVE_ZREG_COUNT ||
	    source + 2U > ORLIX_TCTI_SVE_ZREG_COUNT)
		return -EOPNOTSUPP;
	append = ordinal == 2093U || ordinal == 2094U;
	memcpy(snapshot[0], sve->z[source], sve->vl_bytes);
	memcpy(snapshot[1], sve->z[source + 1U], sve->vl_bytes);
	if (ordinal == 2091U || ordinal == 2093U) {
		for (offset = 0; offset < sve->vl_bytes / sizeof(u32); offset++) {
			u16 first = orlix_tcti_sme_bf16_round(
				get_unaligned_le32(&snapshot[0][offset * sizeof(u32)]),
				current->thread.user_fpcr);
			u16 second = orlix_tcti_sme_bf16_round(
				get_unaligned_le32(&snapshot[1][offset * sizeof(u32)]),
				current->thread.user_fpcr);
			u16 first_lane = append ? offset * 2U : offset;
			u16 second_lane = append ? offset * 2U + 1U :
				offset + sve->vl_bytes / sizeof(u32);

			put_unaligned_le16(first,
				&sve->z[destination][first_lane * sizeof(u16)]);
			put_unaligned_le16(second,
				&sve->z[destination][second_lane * sizeof(u16)]);
		}
	} else {
		for (offset = 0; offset < sve->vl_bytes / sizeof(u64); offset++) {
			u64 input[2] = { 0, 0 };
			u64 output[2] = { 0, 0 };
			u16 first_lane = append ? offset * 2U : offset;
			u16 second_lane = append ? offset * 2U + 1U :
				offset + sve->vl_bytes / sizeof(u64);
			int ret;

			input[0] = get_unaligned_le64(&snapshot[0][offset * sizeof(u64)]);
			ret = orlix_tcti_native_fp_one_source(ORLIX_TCTI_FP1_FCVT,
				sizeof(u64), sizeof(u32), output, input,
				current->thread.user_fpcr, &current->thread.user_fpsr);
			if (ret)
				return ret;
			put_unaligned_le32((u32)output[0],
				&sve->z[destination][first_lane * sizeof(u32)]);
			input[0] = get_unaligned_le64(&snapshot[1][offset * sizeof(u64)]);
			ret = orlix_tcti_native_fp_one_source(ORLIX_TCTI_FP1_FCVT,
				sizeof(u64), sizeof(u32), output, input,
				current->thread.user_fpcr, &current->thread.user_fpsr);
			if (ret)
				return ret;
			put_unaligned_le32((u32)output[0],
				&sve->z[destination][second_lane * sizeof(u32)]);
		}
	}
	return 0;
}

int orlix_tcti_execute_sme_pstate(struct pt_regs *regs,
	const struct orlix_tcti_decoded_instruction *decoded)
{
	struct orlix_tcti_sme_state *sme = &current->thread.user_sme;
	int ret;

	if (!regs || !decoded)
		return -EINVAL;
	if (decoded->sme_pstate_operation == ORLIX_TCTI_SME_PSTATE_SMSTART) {
		/* First use gives the task an architecturally zeroed ZA/ZT0 backing. */
		if (!current->thread.user_sve.valid) {
			ret = orlix_tcti_sve_state_reset(&current->thread.user_sve,
				current->thread.user_simd,
				ORLIX_TCTI_SVE_DEFAULT_VL_BYTES);
			if (ret)
				return ret;
		}
		if (!sme->valid || (decoded->sme_za && !sme->za_enabled)) {
			ret = orlix_tcti_sme_state_reset(sme,
				ORLIX_TCTI_SVE_DEFAULT_VL_BYTES,
				decoded->sme_streaming_mode, decoded->sme_za, false);
			if (ret)
				return ret;
		} else {
			sme->streaming_mode |= decoded->sme_streaming_mode;
		}
	} else if (decoded->sme_pstate_operation == ORLIX_TCTI_SME_PSTATE_SMSTOP) {
		if (!sme->valid)
			return -EINVAL;
		if (decoded->sme_streaming_mode)
			sme->streaming_mode = false;
		if (decoded->sme_za)
			sme->za_enabled = false;
	} else {
		return -EINVAL;
	}
	regs->pc += sizeof(u32);
	return 0;
}

int orlix_tcti_fixed_execute_sme_fp(struct pt_regs *regs,
	const struct orlix_tcti_decoded_instruction *decoded)
{
	struct orlix_tcti_sme_state *sme = &current->thread.user_sme;
	int ret;

	if (!regs || !decoded || !sme->valid || !sme->streaming_mode)
		return -EINVAL;

	ret = orlix_tcti_sme_fp_za_add_sub(regs, sme, &current->thread.user_sve,
		decoded->sme_fp_source_ordinal, decoded->instruction);
	if (ret == -EOPNOTSUPP)
		ret = orlix_tcti_sme_fp_long_matrix(regs, sme,
			&current->thread.user_sve, decoded->sme_fp_source_ordinal,
			decoded->instruction);
	if (ret == -EOPNOTSUPP)
		ret = orlix_tcti_sme_fp_matrix_mul(regs, sme,
			&current->thread.user_sve, decoded->sme_fp_source_ordinal,
			decoded->instruction);
	if (ret == -EOPNOTSUPP)
		ret = orlix_tcti_sme_fp_matrix_dot(regs, sme,
			&current->thread.user_sve, decoded->sme_fp_source_ordinal,
			decoded->instruction);
	if (ret == -EOPNOTSUPP)
		ret = orlix_tcti_sme_bf16_vectors(&current->thread.user_sve,
			decoded->sme_fp_source_ordinal, decoded->instruction);
	if (ret == -EOPNOTSUPP)
		ret = orlix_tcti_sme_fp_minmax_vectors(&current->thread.user_sve,
			decoded->sme_fp_source_ordinal, decoded->instruction);
	if (ret == -EOPNOTSUPP)
		ret = orlix_tcti_sme_fp_abs_minmax_vectors(&current->thread.user_sve,
			decoded->sme_fp_source_ordinal, decoded->instruction);
	if (ret == -EOPNOTSUPP)
		ret = orlix_tcti_sme_fp_clamp_vectors(&current->thread.user_sve,
			decoded->sme_fp_source_ordinal, decoded->instruction);
	if (ret == -EOPNOTSUPP)
		ret = orlix_tcti_sme_fp_scale_vectors(&current->thread.user_sve,
			decoded->sme_fp_source_ordinal, decoded->instruction);
	if (ret == -EOPNOTSUPP)
		ret = orlix_tcti_sme_fp_convert_vectors(&current->thread.user_sve,
			decoded->sme_fp_source_ordinal, decoded->instruction);
	if (ret == -EOPNOTSUPP)
		ret = orlix_tcti_sme_fp_width_convert_vectors(&current->thread.user_sve,
			decoded->sme_fp_source_ordinal, decoded->instruction);
	if (ret == -EOPNOTSUPP)
		ret = orlix_tcti_sme_fp_narrow_vectors(&current->thread.user_sve,
			decoded->sme_fp_source_ordinal, decoded->instruction);
	if (ret)
		return ret;
	regs->pc += sizeof(u32);
	return 0;
}

/* Independent semantic-oracle entry retained for focused unit vectors. */
int orlix_tcti_execute_sme_fp(struct pt_regs *regs,
	const struct orlix_tcti_decoded_instruction *decoded)
{
	return orlix_tcti_fixed_execute_sme_fp(regs, decoded);
}
