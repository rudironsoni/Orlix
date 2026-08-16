// SPDX-License-Identifier: GPL-2.0-only
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/types.h>

#include "decode_aarch64.h"
#include "fixed_integer.h"

static u64 read_gpr_or_zero(const struct pt_regs *regs, u8 reg, u8 access_size)
{
	u64 value = reg == 31 ? 0 : regs->regs[reg];

	return access_size == sizeof(u32) ? (u32)value : value;
}

static void write_gpr_or_zero(struct pt_regs *regs, u8 reg, u8 access_size,
			      u64 value)
{
	if (reg != 31)
		regs->regs[reg] = access_size == sizeof(u32) ? (u32)value : value;
}

static u64 ones_mask(u8 width)
{
	return width == 64 ? U64_MAX : BIT_ULL(width) - 1;
}

static int execute_bitfield(struct pt_regs *regs,
			 const struct orlix_tcti_decoded_instruction *decoded)
{
	u8 data_size = decoded->is_64bit ? 64 : 32;
	u8 access_size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
	u8 immr = decoded->bitfield_immr;
	u8 imms = decoded->bitfield_imms;
	u8 width, lsb, sign_bit;
	u64 source = read_gpr_or_zero(regs, decoded->rn, access_size);
	u64 mask = ones_mask(data_size);
	u64 field_mask, value, result;

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
	field_mask = ones_mask(width) << lsb;
	value &= field_mask;

	switch (decoded->bitfield_op) {
	case ORLIX_TCTI_BITFIELD_UBFM:
		result = value;
		break;
	case ORLIX_TCTI_BITFIELD_SBFM:
		result = value;
		sign_bit = lsb + width - 1;
		if (result & BIT_ULL(sign_bit))
			result |= mask & ~ones_mask(sign_bit + 1);
		break;
	case ORLIX_TCTI_BITFIELD_BFM:
		result = read_gpr_or_zero(regs, decoded->rd, access_size);
		result = (result & ~field_mask) | value;
		break;
	default:
		return -EINVAL;
	}

	write_gpr_or_zero(regs, decoded->rd, access_size, result & mask);
	regs->pc += sizeof(u32);
	return 0;
}

static int execute_extract(struct pt_regs *regs,
			   const struct orlix_tcti_decoded_instruction *decoded)
{
	u8 data_size = decoded->is_64bit ? 64 : 32;
	u8 access_size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
	u8 lsb = decoded->shift_amount;
	u64 mask = ones_mask(data_size);
	u64 high = read_gpr_or_zero(regs, decoded->rn, access_size) & mask;
	u64 low = read_gpr_or_zero(regs, decoded->rm, access_size) & mask;
	u64 result;

	if (lsb >= data_size)
		return -EINVAL;
	result = lsb ? (low >> lsb) | (high << (data_size - lsb)) : low;
	write_gpr_or_zero(regs, decoded->rd, access_size, result & mask);
	regs->pc += sizeof(u32);
	return 0;
}

static int execute_dp1(struct pt_regs *regs,
		       const struct orlix_tcti_decoded_instruction *decoded)
{
	u8 data_size = decoded->is_64bit ? 64 : 32;
	u8 access_size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
	u64 value = read_gpr_or_zero(regs, decoded->rn, access_size);
	u64 mask = ones_mask(data_size);
	u64 result;
	u8 bit;

	value &= mask;
	switch (decoded->dp1_op) {
	case ORLIX_TCTI_DP1_CLZ:
		result = value ? data_size - fls64(value) : data_size;
		break;
	case ORLIX_TCTI_DP1_CLS:
		if (value & BIT_ULL(data_size - 1))
			value = ~value & mask;
		result = value ? data_size - fls64(value) - 1 : data_size - 1;
		break;
	case ORLIX_TCTI_DP1_RBIT:
		result = 0;
		for (bit = 0; bit < data_size; bit++)
			result = (result << 1) | ((value >> bit) & 1);
		break;
	case ORLIX_TCTI_DP1_REV:
		result = decoded->is_64bit ? __builtin_bswap64(value) :
			__builtin_bswap32((u32)value);
		break;
	case ORLIX_TCTI_DP1_REV32:
		if (!decoded->is_64bit)
			return -EINVAL;
		result = ((u64)__builtin_bswap32((u32)(value >> 32)) << 32) |
			__builtin_bswap32((u32)value);
		break;
	case ORLIX_TCTI_DP1_REV16:
		result = ((value & 0x00ff00ff00ff00ffULL) << 8) |
			((value & 0xff00ff00ff00ff00ULL) >> 8);
		break;
	default:
		return -EOPNOTSUPP;
	}

	write_gpr_or_zero(regs, decoded->rd, access_size, result & mask);
	regs->pc += sizeof(u32);
	return 0;
}

int orlix_tcti_fixed_integer_execute(struct pt_regs *regs,
		const struct orlix_tcti_decoded_instruction *decoded)
{
	if (!regs || !decoded)
		return -EINVAL;
	switch (decoded->decode_class) {
	case ORLIX_TCTI_DECODE_BITFIELD:
		return execute_bitfield(regs, decoded);
	case ORLIX_TCTI_DECODE_EXTRACT:
		return execute_extract(regs, decoded);
	case ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE:
		return execute_dp1(regs, decoded);
	default:
		return -EOPNOTSUPP;
	}
}
