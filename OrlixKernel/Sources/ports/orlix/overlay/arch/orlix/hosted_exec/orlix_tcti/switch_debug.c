// SPDX-License-Identifier: GPL-2.0-only
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/limits.h>
#include <linux/log2.h>
#include <linux/preempt.h>
#include <linux/random.h>
#include <linux/string.h>
#include <linux/unaligned.h>
#include <asm/page.h>
#include <asm/mte.h>
#include <asm/processor.h>
#include <linux/sched.h>
#include <asm/hosted_exec.h>
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>
#include <internal/asm/host_time.h>

#include "decode_aarch64.h"
#include "fixed_fp.h"
#include "semantics.h"
#include "system_accessor.h"
#include "sve_state.h"
#include "switch_debug.h"

#define AARCH64_ADRP_PAGE_MASK (~0xfffULL)
#define AARCH64_FPSR_IOC BIT(0)
#define AARCH64_FPSR_DZC BIT(1)
#define AARCH64_FPSR_IXC BIT(4)
#define AARCH64_FPSR_QC BIT(27)
#define AARCH64_FPCR_WRITABLE_MASK \
	(GENMASK(26, 22) | BIT(15) | GENMASK(12, 8))
#define AARCH64_FPSR_WRITABLE_MASK \
	(BIT(27) | BIT(7) | GENMASK(4, 0))
#define AARCH64_CTR_EL0_VALUE \
	(BIT_ULL(29) | BIT_ULL(28) | (4ULL << 16) | (3ULL << 14) | 4ULL)
#define AARCH64_DCZID_EL0_VALUE BIT_ULL(4)
#define AARCH64_CNTFRQ_EL0_VALUE 1000000000ULL
#define AARCH64_MTE_TAG_SHIFT 56U
#define AARCH64_MTE_TAG_MASK (0xfULL << AARCH64_MTE_TAG_SHIFT)
#define AARCH64_MTE_ADDRESS_MASK GENMASK_ULL(55, 0)
#define AARCH64_MTE_GRANULE_SIZE 16U

extern u64 orlix_tcti_native_fcvtzs_w_s(u64 value, u64 fractional_bits);
extern u64 orlix_tcti_native_fcvtzs_w_d(u64 value, u64 fractional_bits);
extern u64 orlix_tcti_native_fcvtzs_x_s(u64 value, u64 fractional_bits);
extern u64 orlix_tcti_native_fcvtzs_x_d(u64 value, u64 fractional_bits);
extern u64 orlix_tcti_native_fcvtzu_w_s(u64 value, u64 fractional_bits);
extern u64 orlix_tcti_native_fcvtzu_w_d(u64 value, u64 fractional_bits);
extern u64 orlix_tcti_native_fcvtzu_x_s(u64 value, u64 fractional_bits);
extern u64 orlix_tcti_native_fcvtzu_x_d(u64 value, u64 fractional_bits);

static bool orlix_tcti_condition_passed(const struct pt_regs *regs, u8 condition);

static int orlix_tcti_execute_sve_predicated_integer_binary(
	struct pt_regs *regs, const struct orlix_tcti_decoded_instruction *decoded)
{
	return orlix_tcti_sve_execute_predicated_integer_binary(
		&current->thread.user_sve, regs, current->thread.user_simd, false,
		decoded->sve_integer_binary_op, decoded->sve_predication,
		decoded->rd, decoded->sve_pg, decoded->rn, decoded->rm,
		decoded->sve_element_bytes);
}

static enum orlix_tcti_access
orlix_tcti_fault_access_for_decoded(const struct orlix_tcti_decoded_instruction *decoded)
{
	if (!decoded)
		return ORLIX_TCTI_ACCESS_FETCH;

	switch (decoded->decode_class) {
	case ORLIX_TCTI_DECODE_MOPS_COPY:
		return ORLIX_TCTI_ACCESS_WRITE;
	case ORLIX_TCTI_DECODE_LOAD_LITERAL:
		return ORLIX_TCTI_ACCESS_READ;
	case ORLIX_TCTI_DECODE_LOAD_STORE_PAIR:
	case ORLIX_TCTI_DECODE_LOAD_STORE_UNSIGNED_IMMEDIATE:
	case ORLIX_TCTI_DECODE_LOAD_STORE_SIGNED_IMMEDIATE:
	case ORLIX_TCTI_DECODE_LOAD_STORE_REGISTER_OFFSET:
	case ORLIX_TCTI_DECODE_LOAD_STORE_EXCLUSIVE:
	case ORLIX_TCTI_DECODE_LSE_ATOMIC:
		return decoded->load ? ORLIX_TCTI_ACCESS_READ : ORLIX_TCTI_ACCESS_WRITE;
	case ORLIX_TCTI_DECODE_MEMORY_TAGGING:
		return decoded->memory_tagging_op == ORLIX_TCTI_MTE_LDG ||
		       decoded->memory_tagging_op == ORLIX_TCTI_MTE_LDGM ?
			ORLIX_TCTI_ACCESS_READ : ORLIX_TCTI_ACCESS_WRITE;
	default:
		return ORLIX_TCTI_ACCESS_FETCH;
	}
}

static u64 orlix_tcti_read_add_sub_immediate_source(const struct pt_regs *regs,
					      const struct orlix_tcti_decoded_instruction *decoded)
{
	u64 value;

	if (decoded->rn == 31)
		value = regs->sp;
	else
		value = regs->regs[decoded->rn];

	return decoded->is_64bit ? value : (u32)value;
}

static u64 orlix_tcti_read_gpr_or_zero(const struct pt_regs *regs, u8 reg,
				 u8 access_size)
{
	u64 value = reg == 31 ? 0 : regs->regs[reg];

	switch (access_size) {
	case sizeof(u8):
		return (u8)value;
	case sizeof(u16):
		return (u16)value;
	case sizeof(u32):
		return (u32)value;
	default:
		return value;
	}
}

static void orlix_tcti_write_gpr_or_zero(struct pt_regs *regs, u8 reg,
				   u8 access_size, u64 value)
{
	if (reg == 31)
		return;

	regs->regs[reg] = access_size == sizeof(u32) ? (u32)value : value;
}

static u64 orlix_tcti_read_gpr_or_sp(const struct pt_regs *regs, u8 reg,
			       u8 access_size)
{
	u64 value = reg == 31 ? regs->sp : regs->regs[reg];

	return access_size == sizeof(u32) ? (u32)value : value;
}

static void orlix_tcti_write_gpr_or_sp(struct pt_regs *regs, u8 reg,
				 u8 access_size, u64 value)
{
	if (access_size == sizeof(u32))
		value = (u32)value;

	if (reg == 31)
		regs->sp = value;
	else
		regs->regs[reg] = value;
}

static u8 orlix_tcti_mte_logical_tag(u64 value)
{
	return (value >> AARCH64_MTE_TAG_SHIFT) & 0xfU;
}

static u64 orlix_tcti_mte_with_tag(u64 value, u8 tag)
{
	return (value & ~AARCH64_MTE_TAG_MASK) |
		((u64)(tag & 0xfU) << AARCH64_MTE_TAG_SHIFT);
}

static u8 orlix_tcti_mte_choose_tag(u8 start, u16 exclude)
{
	u8 tag = start & 0xfU;

	if (exclude == U16_MAX)
		return 0;
	while (exclude & BIT(tag))
		tag = (tag + 1) & 0xfU;
	return tag;
}

static int orlix_tcti_execute_memory_tagging(struct mm_struct *mm,
					struct pt_regs *regs,
					const struct orlix_tcti_decoded_instruction *decoded,
					unsigned long *fault_address)
{
	u64 base;
	u64 address;
	u64 source;
	u8 tag;
	u8 zero[2 * AARCH64_MTE_GRANULE_SIZE] = {};
	u16 exclude;
	int ret;

	switch (decoded->memory_tagging_op) {
	case ORLIX_TCTI_MTE_ADDG:
	case ORLIX_TCTI_MTE_SUBG:
		base = orlix_tcti_read_gpr_or_sp(regs, decoded->rn, sizeof(u64));
		if (decoded->rn == 31 && !IS_ALIGNED(base, 16)) {
			if (fault_address)
				*fault_address = base & AARCH64_MTE_ADDRESS_MASK;
			return -EFAULT;
		}
		tag = decoded->memory_tagging_op == ORLIX_TCTI_MTE_ADDG ?
			orlix_tcti_mte_logical_tag(base) + decoded->tag_offset :
			orlix_tcti_mte_logical_tag(base) - decoded->tag_offset;
		address = (base & ~AARCH64_MTE_ADDRESS_MASK) |
			((decoded->memory_tagging_op == ORLIX_TCTI_MTE_ADDG ?
			  (base & AARCH64_MTE_ADDRESS_MASK) + ((u64)decoded->imm6 << 4) :
			  (base & AARCH64_MTE_ADDRESS_MASK) - ((u64)decoded->imm6 << 4)) &
			 AARCH64_MTE_ADDRESS_MASK);
		orlix_tcti_write_gpr_or_sp(regs, decoded->rd, sizeof(u64),
					orlix_tcti_mte_with_tag(address, tag));
		regs->pc += sizeof(u32);
		return 0;
	case ORLIX_TCTI_MTE_IRG:
		base = orlix_tcti_read_gpr_or_sp(regs, decoded->rn, sizeof(u64));
		if (decoded->rn == 31 && !IS_ALIGNED(base, 16)) {
			if (fault_address)
				*fault_address = base & AARCH64_MTE_ADDRESS_MASK;
			return -EFAULT;
		}
		exclude = current->thread.user_mte_exclude_mask |
			(u16)orlix_tcti_read_gpr_or_zero(regs, decoded->rm, sizeof(u64));
		tag = orlix_tcti_mte_choose_tag(get_random_u32() & 0xfU, exclude);
		orlix_tcti_write_gpr_or_sp(regs, decoded->rd, sizeof(u64),
					orlix_tcti_mte_with_tag(base, tag));
		regs->pc += sizeof(u32);
		return 0;
	case ORLIX_TCTI_MTE_GMI:
		base = orlix_tcti_read_gpr_or_sp(regs, decoded->rn, sizeof(u64));
		if (decoded->rn == 31 && !IS_ALIGNED(base, 16)) {
			if (fault_address)
				*fault_address = base & AARCH64_MTE_ADDRESS_MASK;
			return -EFAULT;
		}
		source = orlix_tcti_read_gpr_or_zero(regs, decoded->rm, sizeof(u64));
		orlix_tcti_write_gpr_or_zero(regs, decoded->rd, sizeof(u64),
					 source | BIT_ULL(orlix_tcti_mte_logical_tag(base)));
		regs->pc += sizeof(u32);
		return 0;
	case ORLIX_TCTI_MTE_STZGM:
	case ORLIX_TCTI_MTE_STGM:
	case ORLIX_TCTI_MTE_LDGM:
		/* The pinned Arm pseudocode makes these instructions UNDEFINED at EL0. */
		return -EOPNOTSUPP;
	default:
		break;
	}

	if (!mm)
		return -EINVAL;
	base = orlix_tcti_read_gpr_or_sp(regs, decoded->rn, sizeof(u64));
	if (decoded->rn == 31 && !IS_ALIGNED(base, 16)) {
		if (fault_address)
			*fault_address = base & AARCH64_MTE_ADDRESS_MASK;
		return -EFAULT;
	}
	address = decoded->memory_index_mode == ORLIX_TCTI_MEMORY_INDEX_POST ?
		base : base + decoded->memory_offset;
	if (fault_address)
		*fault_address = address & AARCH64_MTE_ADDRESS_MASK;

	if (!IS_ALIGNED(address & AARCH64_MTE_ADDRESS_MASK,
			AARCH64_MTE_GRANULE_SIZE))
		return -EFAULT;

	if (decoded->memory_tagging_op == ORLIX_TCTI_MTE_LDG) {
		ret = orlix_mte_load_allocation_tag(mm, address, &tag);
		if (ret)
			return ret;
		if (decoded->rt != 31)
			regs->regs[decoded->rt] = orlix_tcti_mte_with_tag(
				regs->regs[decoded->rt], tag);
	} else {
		source = orlix_tcti_read_gpr_or_sp(regs, decoded->rt, sizeof(u64));
		tag = orlix_tcti_mte_logical_tag(source);
		if (decoded->memory_tagging_op == ORLIX_TCTI_MTE_STZG ||
		    decoded->memory_tagging_op == ORLIX_TCTI_MTE_STZ2G) {
			ret = orlix_tcti_write_user_data(mm,
					address & AARCH64_MTE_ADDRESS_MASK, zero,
					decoded->memory_tagging_op == ORLIX_TCTI_MTE_STZ2G ?
					2 * AARCH64_MTE_GRANULE_SIZE : AARCH64_MTE_GRANULE_SIZE);
			if (ret)
				return ret;
		}
		ret = orlix_mte_store_allocation_tags(mm, address, tag,
			(decoded->memory_tagging_op == ORLIX_TCTI_MTE_ST2G ||
			 decoded->memory_tagging_op == ORLIX_TCTI_MTE_STZ2G) ? 2 : 1);
		if (ret)
			return ret;
	}

	if (decoded->memory_index_mode != ORLIX_TCTI_MEMORY_INDEX_SIGNED_OFFSET)
		orlix_tcti_write_gpr_or_sp(regs, decoded->rn, sizeof(u64),
					base + decoded->memory_offset);
	regs->pc += sizeof(u32);
	return 0;
}

static void orlix_tcti_update_add_sub_flags(struct pt_regs *regs, u64 left,
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

static bool orlix_tcti_fp32_is_nan(u32 value)
{
	return (value & GENMASK(30, 23)) == GENMASK(30, 23) &&
	       (value & GENMASK(22, 0));
}

static bool orlix_tcti_fp16_is_nan(u16 value)
{
	return (value & GENMASK(14, 10)) == GENMASK(14, 10) &&
	       (value & GENMASK(9, 0));
}

static bool orlix_tcti_fp64_is_nan(u64 value)
{
	return (value & GENMASK_ULL(62, 52)) == GENMASK_ULL(62, 52) &&
	       (value & GENMASK_ULL(51, 0));
}

static bool orlix_tcti_fp32_is_signaling_nan(u32 value)
{
	return orlix_tcti_fp32_is_nan(value) && !(value & BIT(22));
}

static bool orlix_tcti_fp16_is_signaling_nan(u16 value)
{
	return orlix_tcti_fp16_is_nan(value) && !(value & BIT(9));
}

static bool orlix_tcti_fp64_is_signaling_nan(u64 value)
{
	return orlix_tcti_fp64_is_nan(value) && !(value & BIT_ULL(51));
}

static void orlix_tcti_set_fp_compare_flags(struct pt_regs *regs, int result)
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

static int orlix_tcti_compare_fp32(u32 left, u32 right)
{
	bool left_negative = left & BIT(31);
	bool right_negative = right & BIT(31);
	u32 left_magnitude = left & GENMASK(30, 0);
	u32 right_magnitude = right & GENMASK(30, 0);

	if (orlix_tcti_fp32_is_nan(left) || orlix_tcti_fp32_is_nan(right))
		return -2;
	if (!left_magnitude && !right_magnitude)
		return 0;
	if (left_negative != right_negative)
		return left_negative ? -1 : 1;
	if (left_magnitude == right_magnitude)
		return 0;
	return left_negative == (left_magnitude > right_magnitude) ? -1 : 1;
}

static int orlix_tcti_compare_fp16(u16 left, u16 right)
{
	bool left_negative = left & BIT(15);
	bool right_negative = right & BIT(15);
	u16 left_magnitude = left & GENMASK(14, 0);
	u16 right_magnitude = right & GENMASK(14, 0);

	if (orlix_tcti_fp16_is_nan(left) || orlix_tcti_fp16_is_nan(right))
		return -2;
	if (!left_magnitude && !right_magnitude)
		return 0;
	if (left_negative != right_negative)
		return left_negative ? -1 : 1;
	if (left_magnitude == right_magnitude)
		return 0;
	return left_negative == (left_magnitude > right_magnitude) ? -1 : 1;
}

static int orlix_tcti_compare_fp64(u64 left, u64 right)
{
	bool left_negative = left & BIT_ULL(63);
	bool right_negative = right & BIT_ULL(63);
	u64 left_magnitude = left & GENMASK_ULL(62, 0);
	u64 right_magnitude = right & GENMASK_ULL(62, 0);

	if (orlix_tcti_fp64_is_nan(left) || orlix_tcti_fp64_is_nan(right))
		return -2;
	if (!left_magnitude && !right_magnitude)
		return 0;
	if (left_negative != right_negative)
		return left_negative ? -1 : 1;
	if (left_magnitude == right_magnitude)
		return 0;
	return left_negative == (left_magnitude > right_magnitude) ? -1 : 1;
}

static u64 orlix_tcti_s32_to_fp64_bits(s32 value)
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

static u32 orlix_tcti_s32_to_fp32_bits(s32 value)
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

static u32 orlix_tcti_u32_to_fp32_bits(u32 value)
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

static u32 orlix_tcti_u64_to_fp32_bits(u64 value)
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

static u32 orlix_tcti_s64_to_fp32_bits(s64 value)
{
	u64 magnitude = value < 0 ? 0 - (u64)value : (u64)value;
	u32 result = orlix_tcti_u64_to_fp32_bits(magnitude);

	return value < 0 ? result | BIT(31) : result;
}

static u64 orlix_tcti_u64_to_fp64_bits(u64 value)
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

static u64 orlix_tcti_s64_to_fp64_bits(s64 value)
{
	u64 magnitude;
	u64 result;

	if (value >= 0)
		return orlix_tcti_u64_to_fp64_bits(value);

	magnitude = ~(u64)value + 1;
	result = orlix_tcti_u64_to_fp64_bits(magnitude);
	return result | BIT_ULL(63);
}

static int orlix_tcti_fp32_bits_to_u64_zero(u32 value, u64 *result)
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

static int orlix_tcti_fp64_bits_to_u64_zero(u64 value, u64 *result)
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

static int orlix_tcti_fp32_bits_to_s32_zero(u32 value, u64 *result)
{
	bool negative = value & BIT(31);
	u32 exponent_bits = (value >> 23) & 0xffU;
	u32 fraction = value & GENMASK(22, 0);
	u64 mantissa;
	u64 magnitude;
	int exponent;

	if (!result || exponent_bits == 0xffU)
		return -EOPNOTSUPP;
	if (!exponent_bits) {
		*result = 0;
		return 0;
	}

	exponent = (int)exponent_bits - 127;
	if (exponent < 0) {
		*result = 0;
		return 0;
	}
	if (exponent > 31)
		return -EOPNOTSUPP;

	mantissa = BIT_ULL(23) | fraction;
	magnitude = exponent >= 23 ? mantissa << (exponent - 23) :
				     mantissa >> (23 - exponent);
	if (!negative && magnitude >= BIT_ULL(31))
		return -EOPNOTSUPP;
	if (negative && magnitude > BIT_ULL(31))
		return -EOPNOTSUPP;

	*result = (u32)(negative ? ~magnitude + 1 : magnitude);
	return 0;
}

static int orlix_tcti_fp64_bits_to_s64_zero(u64 value, u64 *result)
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

static u64 orlix_tcti_shift_logical_source(u64 value,
				     const struct orlix_tcti_decoded_instruction *decoded)
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

static u64 orlix_tcti_extend_register_source(u64 value, u8 option)
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

static int orlix_tcti_execute_add_sub_result(struct pt_regs *regs,
				       const struct orlix_tcti_decoded_instruction *decoded,
				       u64 left, u64 right, bool sp_allowed)
{
	u8 access_size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
	u64 result = decoded->subtract ? left - right : left + right;

	if (decoded->set_flags)
		orlix_tcti_update_add_sub_flags(regs, left, right, result,
					  access_size, decoded->subtract);

	if (decoded->set_flags && decoded->rd == 31) {
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->set_flags || !sp_allowed)
		orlix_tcti_write_gpr_or_zero(regs, decoded->rd, access_size, result);
	else
		orlix_tcti_write_gpr_or_sp(regs, decoded->rd, access_size, result);

	regs->pc += sizeof(u32);
	return 0;
}

static int orlix_tcti_execute_min_max_immediate(
	struct pt_regs *regs,
	const struct orlix_tcti_decoded_instruction *decoded)
{
	u8 access_size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
	u64 source = orlix_tcti_read_gpr_or_zero(regs, decoded->rn, access_size);
	u64 result;

	if (decoded->is_64bit) {
		s64 signed_source = (s64)source;
		s64 signed_immediate = (s8)decoded->min_max_immediate;
		u64 unsigned_immediate = decoded->min_max_immediate;

		switch (decoded->min_max_immediate_op) {
		case ORLIX_TCTI_MIN_MAX_IMMEDIATE_SMAX:
			result = signed_source > signed_immediate ? source :
				(u64)signed_immediate;
			break;
		case ORLIX_TCTI_MIN_MAX_IMMEDIATE_UMAX:
			result = source > unsigned_immediate ? source :
				unsigned_immediate;
			break;
		case ORLIX_TCTI_MIN_MAX_IMMEDIATE_SMIN:
			result = signed_source < signed_immediate ? source :
				(u64)signed_immediate;
			break;
		case ORLIX_TCTI_MIN_MAX_IMMEDIATE_UMIN:
			result = source < unsigned_immediate ? source :
				unsigned_immediate;
			break;
		default:
			return -EINVAL;
		}
	} else {
		u32 source32 = source;
		s32 signed_source = source32;
		s32 signed_immediate = (s8)decoded->min_max_immediate;
		u32 unsigned_immediate = decoded->min_max_immediate;

		switch (decoded->min_max_immediate_op) {
		case ORLIX_TCTI_MIN_MAX_IMMEDIATE_SMAX:
			result = signed_source > signed_immediate ? source32 :
				(u32)signed_immediate;
			break;
		case ORLIX_TCTI_MIN_MAX_IMMEDIATE_UMAX:
			result = source32 > unsigned_immediate ? source32 :
				unsigned_immediate;
			break;
		case ORLIX_TCTI_MIN_MAX_IMMEDIATE_SMIN:
			result = signed_source < signed_immediate ? source32 :
				(u32)signed_immediate;
			break;
		case ORLIX_TCTI_MIN_MAX_IMMEDIATE_UMIN:
			result = source32 < unsigned_immediate ? source32 :
				unsigned_immediate;
			break;
		default:
			return -EINVAL;
		}
	}

	orlix_tcti_write_gpr_or_zero(regs, decoded->rd, access_size, result);
	regs->pc += sizeof(u32);
	return 0;
}

static int orlix_tcti_execute_add_sub_shifted_register(struct pt_regs *regs,
						 const struct orlix_tcti_decoded_instruction *decoded)
{
	u8 access_size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
	u64 left = orlix_tcti_read_gpr_or_zero(regs, decoded->rn, access_size);
	u64 right = orlix_tcti_read_gpr_or_zero(regs, decoded->rm, access_size);

	right = orlix_tcti_shift_logical_source(right, decoded);
	return orlix_tcti_execute_add_sub_result(regs, decoded, left, right, false);
}

static int orlix_tcti_execute_add_sub_extended_register(struct pt_regs *regs,
						 const struct orlix_tcti_decoded_instruction *decoded)
{
	u8 access_size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
	u64 left = orlix_tcti_read_gpr_or_sp(regs, decoded->rn, access_size);
	u64 right = orlix_tcti_read_gpr_or_zero(regs, decoded->rm, sizeof(u64));

	right = orlix_tcti_extend_register_source(right, decoded->offset_extend);
	right <<= decoded->shift_amount;
	if (!decoded->is_64bit) {
		left = (u32)left;
		right = (u32)right;
	}

	return orlix_tcti_execute_add_sub_result(regs, decoded, left, right, true);
}

static int orlix_tcti_execute_add_sub_with_carry(struct pt_regs *regs,
					   const struct orlix_tcti_decoded_instruction *decoded)
{
	u8 access_size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
	u64 left = orlix_tcti_read_gpr_or_zero(regs, decoded->rn, access_size);
	u64 right = orlix_tcti_read_gpr_or_zero(regs, decoded->rm, access_size);
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
		orlix_tcti_write_gpr_or_zero(regs, decoded->rd, access_size, result);
	regs->pc += sizeof(u32);
	return 0;
}

static int orlix_tcti_execute_logical_shifted_register(struct pt_regs *regs,
						 const struct orlix_tcti_decoded_instruction *decoded)
{
	u8 access_size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
	u64 left = orlix_tcti_read_gpr_or_zero(regs, decoded->rn, access_size);
	u64 right = orlix_tcti_read_gpr_or_zero(regs, decoded->rm, access_size);
	u64 sign_bit = decoded->is_64bit ? BIT_ULL(63) : BIT_ULL(31);
	u64 mask = decoded->is_64bit ? U64_MAX : U32_MAX;
	u64 result;

	right = orlix_tcti_shift_logical_source(right, decoded);
	if (decoded->invert_second_operand)
		right = ~right;

	switch (decoded->logical_op) {
	case ORLIX_TCTI_LOGICAL_AND:
		result = left & right;
		break;
	case ORLIX_TCTI_LOGICAL_ORR:
		result = left | right;
		break;
	case ORLIX_TCTI_LOGICAL_EOR:
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
		orlix_tcti_write_gpr_or_zero(regs, decoded->rd, access_size, result);
	regs->pc += sizeof(u32);
	return 0;
}

static int orlix_tcti_execute_logical_immediate(struct pt_regs *regs,
					  const struct orlix_tcti_decoded_instruction *decoded)
{
	u8 access_size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
	u64 left = orlix_tcti_read_gpr_or_zero(regs, decoded->rn, access_size);
	u64 right = decoded->logical_immediate;
	u64 sign_bit = decoded->is_64bit ? BIT_ULL(63) : BIT_ULL(31);
	u64 mask = decoded->is_64bit ? U64_MAX : U32_MAX;
	u64 result;

	switch (decoded->logical_op) {
	case ORLIX_TCTI_LOGICAL_AND:
		result = left & right;
		break;
	case ORLIX_TCTI_LOGICAL_ORR:
		result = left | right;
		break;
	case ORLIX_TCTI_LOGICAL_EOR:
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
		orlix_tcti_write_gpr_or_zero(regs, decoded->rd, access_size, result);
	regs->pc += sizeof(u32);
	return 0;
}

static u64 orlix_tcti_ones_mask(u8 width)
{
	return width >= 64 ? ~0ULL : BIT_ULL(width) - 1;
}

static int orlix_tcti_execute_bitfield(struct pt_regs *regs,
				 const struct orlix_tcti_decoded_instruction *decoded)
{
	u8 data_size = decoded->is_64bit ? 64 : 32;
	u8 access_size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
	u8 immr = decoded->bitfield_immr;
	u8 imms = decoded->bitfield_imms;
	u8 width;
	u8 lsb;
	u8 sign_bit;
	u64 source = orlix_tcti_read_gpr_or_zero(regs, decoded->rn, access_size);
	u64 mask = orlix_tcti_ones_mask(data_size);
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

	field_mask = orlix_tcti_ones_mask(width) << lsb;
	value &= field_mask;

	switch (decoded->bitfield_op) {
	case ORLIX_TCTI_BITFIELD_UBFM:
		result = value;
		break;
	case ORLIX_TCTI_BITFIELD_SBFM:
		result = value;
		sign_bit = lsb + width - 1;
		if (result & BIT_ULL(sign_bit))
			result |= mask & ~orlix_tcti_ones_mask(sign_bit + 1);
		break;
	case ORLIX_TCTI_BITFIELD_BFM:
		result = orlix_tcti_read_gpr_or_zero(regs, decoded->rd, access_size);
		result = (result & ~field_mask) | value;
		break;
	default:
		return -EINVAL;
	}

	orlix_tcti_write_gpr_or_zero(regs, decoded->rd, access_size, result & mask);
	regs->pc += sizeof(u32);
	return 0;
}

static int orlix_tcti_execute_extract(struct pt_regs *regs,
				const struct orlix_tcti_decoded_instruction *decoded)
{
	u8 data_size = decoded->is_64bit ? 64 : 32;
	u8 access_size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
	u8 lsb = decoded->shift_amount;
	u64 mask = orlix_tcti_ones_mask(data_size);
	u64 high = orlix_tcti_read_gpr_or_zero(regs, decoded->rn, access_size);
	u64 low = orlix_tcti_read_gpr_or_zero(regs, decoded->rm, access_size);
	u64 result;

	if (lsb >= data_size)
		return -EINVAL;

	high &= mask;
	low &= mask;
	result = lsb ? (low >> lsb) | (high << (data_size - lsb)) : low;
	orlix_tcti_write_gpr_or_zero(regs, decoded->rd, access_size, result & mask);
	regs->pc += sizeof(u32);
	return 0;
}

static u32 orlix_tcti_crc32_update(u32 accumulator, u64 value, u8 byte_count,
			     u32 polynomial)
{
	u8 byte;
	u8 bit;

	for (byte = 0; byte < byte_count; byte++) {
		accumulator ^= (u8)(value >> (byte * 8));
		for (bit = 0; bit < 8; bit++)
			accumulator = (accumulator >> 1) ^
				(accumulator & 1 ? polynomial : 0);
	}

	return accumulator;
}

static int orlix_tcti_execute_data_processing_2source(struct pt_regs *regs,
						 const struct orlix_tcti_decoded_instruction *decoded)
{
	u8 data_size = decoded->is_64bit ? 64 : 32;
	u8 access_size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
	u64 left;
	u64 right;
	u8 amount;
	u64 mask;
	u64 result;

	if (decoded->dp2_op == ORLIX_TCTI_DP2_CRC32 ||
	    decoded->dp2_op == ORLIX_TCTI_DP2_CRC32C) {
		u32 accumulator = orlix_tcti_read_gpr_or_zero(
			regs, decoded->rn, sizeof(u32));
		u64 value = orlix_tcti_read_gpr_or_zero(
			regs, decoded->rm, decoded->access_size);
		u32 polynomial = decoded->dp2_op == ORLIX_TCTI_DP2_CRC32C ?
			0x82f63b78U : 0xedb88320U;
		u32 result = orlix_tcti_crc32_update(accumulator, value,
			decoded->access_size, polynomial);

		orlix_tcti_write_gpr_or_zero(regs, decoded->rd, sizeof(u32), result);
		regs->pc += sizeof(u32);
		return 0;
	}

	left = orlix_tcti_read_gpr_or_zero(regs, decoded->rn, access_size);
	right = orlix_tcti_read_gpr_or_zero(regs, decoded->rm, access_size);
	amount = right & (data_size - 1);
	mask = orlix_tcti_ones_mask(data_size);

	switch (decoded->dp2_op) {
	case ORLIX_TCTI_DP2_UDIV:
		result = right ? left / right : 0;
		break;
	case ORLIX_TCTI_DP2_SDIV:
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
	case ORLIX_TCTI_DP2_LSLV:
		result = left << amount;
		break;
	case ORLIX_TCTI_DP2_LSRV:
		result = left >> amount;
		break;
	case ORLIX_TCTI_DP2_ASRV:
		result = decoded->is_64bit ?
			 (u64)((s64)left >> amount) :
			 (u32)((s32)(u32)left >> amount);
		break;
	case ORLIX_TCTI_DP2_RORV:
		result = decoded->is_64bit ? ror64(left, amount) :
					     ror32(left, amount);
		break;
	case ORLIX_TCTI_DP2_SMAX:
		result = decoded->is_64bit ?
			((s64)left > (s64)right ? left : right) :
			((s32)(u32)left > (s32)(u32)right ? left : right);
		break;
	case ORLIX_TCTI_DP2_UMAX:
		result = left > right ? left : right;
		break;
	case ORLIX_TCTI_DP2_SMIN:
		result = decoded->is_64bit ?
			((s64)left < (s64)right ? left : right) :
			((s32)(u32)left < (s32)(u32)right ? left : right);
		break;
	case ORLIX_TCTI_DP2_UMIN:
		result = left < right ? left : right;
		break;
	default:
		return -EINVAL;
	}

	orlix_tcti_write_gpr_or_zero(regs, decoded->rd, access_size, result & mask);
	regs->pc += sizeof(u32);
	return 0;
}

static int orlix_tcti_execute_data_processing_1source(struct pt_regs *regs,
						const struct orlix_tcti_decoded_instruction *decoded)
{
	u8 data_size = decoded->is_64bit ? 64 : 32;
	u8 access_size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
	u64 value = orlix_tcti_read_gpr_or_zero(regs, decoded->rn, access_size);
	u64 mask = orlix_tcti_ones_mask(data_size);
	u64 result;
	u8 bit;

	switch (decoded->dp1_op) {
	case ORLIX_TCTI_DP1_CLZ:
		value &= mask;
		result = value ? data_size - fls64(value) : data_size;
		break;
	case ORLIX_TCTI_DP1_CLS:
		value &= mask;
		if (value & BIT_ULL(data_size - 1))
			value = ~value & mask;
		result = value ? data_size - fls64(value) - 1 : data_size - 1;
		break;
	case ORLIX_TCTI_DP1_CTZ:
		value &= mask;
		result = value ? (decoded->is_64bit ? __ffs64(value) :
			__ffs((u32)value)) : data_size;
		break;
	case ORLIX_TCTI_DP1_CNT:
		result = decoded->is_64bit ? hweight64(value) : hweight32(value);
		break;
	case ORLIX_TCTI_DP1_ABS:
		value &= mask;
		result = value & BIT_ULL(data_size - 1) ? (-value) & mask : value;
		break;
	case ORLIX_TCTI_DP1_RBIT:
		value &= mask;
		result = 0;
		for (bit = 0; bit < data_size; bit++)
			result = (result << 1) | ((value >> bit) & 1);
		break;
	case ORLIX_TCTI_DP1_REV:
		result = decoded->is_64bit ? __builtin_bswap64(value) :
						 __builtin_bswap32((u32)value);
		break;
	case ORLIX_TCTI_DP1_REV32:
		result = ((u64)__builtin_bswap32((u32)(value >> 32)) << 32) |
			 __builtin_bswap32((u32)value);
		break;
	case ORLIX_TCTI_DP1_REV16:
		result = (((value & 0x00ff00ff00ff00ffULL) << 8) |
			  ((value & 0xff00ff00ff00ff00ULL) >> 8)) & mask;
		break;
	default:
		return -EINVAL;
	}

	orlix_tcti_write_gpr_or_zero(regs, decoded->rd, access_size, result);
	regs->pc += sizeof(u32);
	return 0;
}

static void orlix_tcti_set_nzcv_from_immediate(struct pt_regs *regs, u8 nzcv)
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

static int orlix_tcti_execute_conditional_compare(struct pt_regs *regs,
					    const struct orlix_tcti_decoded_instruction *decoded)
{
	u8 access_size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
	u64 left = orlix_tcti_read_gpr_or_zero(regs, decoded->rn, access_size);
	u64 right = decoded->immediate ?
		    decoded->imm12 :
		    orlix_tcti_read_gpr_or_zero(regs, decoded->rm, access_size);
	u64 result;

	if (!orlix_tcti_condition_passed(regs, decoded->condition)) {
		orlix_tcti_set_nzcv_from_immediate(regs, decoded->nzcv);
		regs->pc += sizeof(u32);
		return 0;
	}

	result = decoded->subtract ? left - right : left + right;
	orlix_tcti_update_add_sub_flags(regs, left, right, result, access_size,
				  decoded->subtract);
	regs->pc += sizeof(u32);
	return 0;
}

static int orlix_tcti_execute_multiply_add_sub(struct pt_regs *regs,
					 const struct orlix_tcti_decoded_instruction *decoded)
{
	u8 access_size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
	u64 left;
	u64 right;
	u64 accumulator;
	u64 product;
	u64 result;

	switch (decoded->mul_op) {
	case ORLIX_TCTI_MUL_MADD:
	case ORLIX_TCTI_MUL_MSUB:
		left = orlix_tcti_read_gpr_or_zero(regs, decoded->rn, access_size);
		right = orlix_tcti_read_gpr_or_zero(regs, decoded->rm, access_size);
		accumulator = orlix_tcti_read_gpr_or_zero(regs, decoded->ra,
						    access_size);
		product = left * right;
		result = decoded->mul_op == ORLIX_TCTI_MUL_MSUB ?
			 accumulator - product : accumulator + product;
		orlix_tcti_write_gpr_or_zero(regs, decoded->rd, access_size, result);
		break;
	case ORLIX_TCTI_MUL_SMADDL:
	case ORLIX_TCTI_MUL_SMSUBL:
		left = (s64)(s32)orlix_tcti_read_gpr_or_zero(regs, decoded->rn,
						       sizeof(u32));
		right = (s64)(s32)orlix_tcti_read_gpr_or_zero(regs, decoded->rm,
							sizeof(u32));
		accumulator = orlix_tcti_read_gpr_or_zero(regs, decoded->ra,
						    sizeof(u64));
		product = left * right;
		result = decoded->mul_op == ORLIX_TCTI_MUL_SMSUBL ?
			 accumulator - product : accumulator + product;
		orlix_tcti_write_gpr_or_zero(regs, decoded->rd, sizeof(u64), result);
		break;
	case ORLIX_TCTI_MUL_SMULH:
		left = orlix_tcti_read_gpr_or_zero(regs, decoded->rn, sizeof(u64));
		right = orlix_tcti_read_gpr_or_zero(regs, decoded->rm, sizeof(u64));
		result = (u64)(((__int128)(s64)left * (s64)right) >> 64);
		orlix_tcti_write_gpr_or_zero(regs, decoded->rd, sizeof(u64), result);
		break;
	case ORLIX_TCTI_MUL_UMADDL:
	case ORLIX_TCTI_MUL_UMSUBL:
		left = (u32)orlix_tcti_read_gpr_or_zero(regs, decoded->rn,
						  sizeof(u32));
		right = (u32)orlix_tcti_read_gpr_or_zero(regs, decoded->rm,
						   sizeof(u32));
		accumulator = orlix_tcti_read_gpr_or_zero(regs, decoded->ra,
						    sizeof(u64));
		product = left * right;
		result = decoded->mul_op == ORLIX_TCTI_MUL_UMSUBL ?
			 accumulator - product : accumulator + product;
		orlix_tcti_write_gpr_or_zero(regs, decoded->rd, sizeof(u64), result);
		break;
	case ORLIX_TCTI_MUL_UMULH:
		left = orlix_tcti_read_gpr_or_zero(regs, decoded->rn, sizeof(u64));
		right = orlix_tcti_read_gpr_or_zero(regs, decoded->rm, sizeof(u64));
		result = (u64)(((unsigned __int128)left * right) >> 64);
		orlix_tcti_write_gpr_or_zero(regs, decoded->rd, sizeof(u64), result);
		break;
	default:
		return -EINVAL;
	}

	regs->pc += sizeof(u32);
	return 0;
}

static int orlix_tcti_execute_conditional_select(struct pt_regs *regs,
					   const struct orlix_tcti_decoded_instruction *decoded)
{
	u8 access_size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
	u64 result;

	if (orlix_tcti_condition_passed(regs, decoded->condition)) {
		result = orlix_tcti_read_gpr_or_zero(regs, decoded->rn, access_size);
	} else {
		result = orlix_tcti_read_gpr_or_zero(regs, decoded->rm, access_size);
		switch (decoded->conditional_select_op) {
		case ORLIX_TCTI_CONDITIONAL_SELECT_CSEL:
			break;
		case ORLIX_TCTI_CONDITIONAL_SELECT_CSINC:
			result++;
			break;
		case ORLIX_TCTI_CONDITIONAL_SELECT_CSINV:
			result = ~result;
			break;
		case ORLIX_TCTI_CONDITIONAL_SELECT_CSNEG:
			result = -result;
			break;
		default:
			return -EINVAL;
		}
	}

	orlix_tcti_write_gpr_or_zero(regs, decoded->rd, access_size, result);
	regs->pc += sizeof(u32);
	return 0;
}

static int orlix_tcti_execute_move_wide_immediate(struct pt_regs *regs,
					    const struct orlix_tcti_decoded_instruction *decoded)
{
	u8 access_size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
	u64 immediate = (u64)decoded->imm16 << decoded->halfword_shift;
	u64 mask = 0xffffULL << decoded->halfword_shift;
	u64 result;

	switch (decoded->move_wide_op) {
	case ORLIX_TCTI_MOVE_WIDE_MOVN:
		result = ~immediate;
		break;
	case ORLIX_TCTI_MOVE_WIDE_MOVZ:
		result = immediate;
		break;
	case ORLIX_TCTI_MOVE_WIDE_MOVK:
		result = orlix_tcti_read_gpr_or_zero(regs, decoded->rd,
					       access_size);
		result = (result & ~mask) | immediate;
		break;
	default:
		return -EINVAL;
	}

	orlix_tcti_write_gpr_or_zero(regs, decoded->rd, access_size, result);
	regs->pc += sizeof(u32);
	return 0;
}

static bool orlix_tcti_condition_passed(const struct pt_regs *regs, u8 condition)
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

static void orlix_tcti_clear_exclusive_monitor(void)
{
	current->thread.user_exclusive_address = 0;
	current->thread.user_exclusive_value = 0;
	current->thread.user_exclusive_value2 = 0;
	current->thread.user_exclusive_pfn = 0;
	current->thread.user_exclusive_generation = 0;
	current->thread.user_exclusive_mapping_generation = 0;
	current->thread.user_exclusive_size = 0;
	current->thread.user_exclusive_valid = 0;
}

static u64 orlix_tcti_memory_base(const struct pt_regs *regs, u8 rn)
{
	return rn == 31 ? regs->sp : regs->regs[rn];
}

static void orlix_tcti_write_memory_base(struct pt_regs *regs, u8 rn, u64 value)
{
	if (rn == 31)
		regs->sp = value;
	else
		regs->regs[rn] = value;
}

static int orlix_tcti_encode_integer(u8 *buffer, u8 access_size, u64 value)
{
	switch (access_size) {
	case sizeof(u8):
		buffer[0] = value;
		return 0;
	case sizeof(u16):
		put_unaligned_le16(value, buffer);
		return 0;
	case sizeof(u32):
		put_unaligned_le32(value, buffer);
		return 0;
	case sizeof(u64):
		put_unaligned_le64(value, buffer);
		return 0;
	default:
		return -EINVAL;
	}
}

static int orlix_tcti_decode_integer(const u8 *buffer, u8 access_size, u64 *value)
{
	if (!buffer || !value)
		return -EINVAL;

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

static int orlix_tcti_store_integer(struct mm_struct *mm, unsigned long address,
			      u8 access_size, u64 value)
{
	u8 buffer[sizeof(u64)];
	int ret;

	ret = orlix_tcti_encode_integer(buffer, access_size, value);
	if (ret)
		return ret;
	ret = orlix_mte_check_access(mm, address, access_size, true);
	if (ret)
		return ret;

	ret = orlix_tcti_write_user_data(mm, address, buffer, access_size);
	if (!ret)
		orlix_tcti_clear_exclusive_monitor();
	return ret;
}

static int orlix_tcti_load_integer(struct mm_struct *mm, unsigned long address,
			     u8 access_size, u64 *value)
{
	u8 buffer[sizeof(u64)] = {};
	int ret;

	if (!value)
		return -EINVAL;
	ret = orlix_mte_check_access(mm, address, access_size, false);
	if (ret)
		return ret;

	ret = orlix_tcti_read_user_data(mm, address, buffer, access_size);
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

/*
 * DDI0602 2026-06 CPY* permits an implementation-defined stage size.
 * OrlixTCTI selects option B with zero-byte prologue and main stages, then
 * completes the epilogue one guest byte at a time. This never substitutes a
 * host bulk-memory operation: each completed byte writes architectural state
 * before the next guest access, preserving partial completion on fault.
 *
 * All sixteen option encodings are legal EL0 forms. Their privilege and
 * non-temporal descriptors retain the current guest-memory permission path;
 * no host tag operation is synthesized because TCTI does not advertise MTE.
 */
static int orlix_tcti_execute_mops_copy(struct mm_struct *mm,
					struct pt_regs *regs,
					const struct orlix_tcti_decoded_instruction *decoded,
					unsigned long *fault_address)
{
	u64 remaining;
	u64 source;
	u64 destination;
	bool backward;
	int ret;

	if (!mm)
		return -EINVAL;
	if (decoded->rd == 31 || decoded->rm == 31 || decoded->rn == 31 ||
	    decoded->rd == decoded->rm || decoded->rd == decoded->rn ||
	    decoded->rm == decoded->rn)
		return -EOPNOTSUPP;

	destination = regs->regs[decoded->rd];
	source = regs->regs[decoded->rm];
	remaining = regs->regs[decoded->rn];
	if (decoded->mops_copy_stage == ORLIX_TCTI_MOPS_COPY_PROLOGUE) {
		if (remaining & BIT_ULL(63))
			remaining = S64_MAX;
		backward = !decoded->mops_forward_only && source < destination;
		regs->regs[decoded->rn] = remaining;
		regs->pstate &= ~(PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT);
		if (backward)
			regs->pstate |= PSR_N_BIT;
		regs->pstate |= PSR_C_BIT;
		regs->pc += sizeof(u32);
		return 0;
	}

	if (!(regs->pstate & PSR_C_BIT) ||
	    (regs->pstate & (PSR_Z_BIT | PSR_V_BIT)) ||
	    (decoded->mops_forward_only && (regs->pstate & PSR_N_BIT)))
		return -EOPNOTSUPP;
	backward = !decoded->mops_forward_only && (regs->pstate & PSR_N_BIT);
	if (decoded->mops_copy_stage == ORLIX_TCTI_MOPS_COPY_MAIN) {
		regs->pc += sizeof(u32);
		return 0;
	}

	while (remaining) {
		u64 value;
		unsigned long read_address = source;
		unsigned long write_address = destination;

		if (backward) {
			read_address--;
			write_address--;
		}
		if (fault_address)
			*fault_address = read_address;
		ret = orlix_tcti_load_integer(mm, read_address, sizeof(u8), &value);
		if (ret)
			return ret;
		if (fault_address)
			*fault_address = write_address;
		ret = orlix_tcti_store_integer(mm, write_address, sizeof(u8), value);
		if (ret)
			return ret;
		source = backward ? source - 1 : source + 1;
		destination = backward ? destination - 1 : destination + 1;
		remaining--;
		regs->regs[decoded->rm] = source;
		regs->regs[decoded->rd] = destination;
		regs->regs[decoded->rn] = remaining;
	}
	regs->pc += sizeof(u32);
	return 0;
}

static int orlix_tcti_store_simd_fp(struct mm_struct *mm, unsigned long address,
			      u8 reg, u8 access_size)
{
	u8 buffer[2 * sizeof(u64)];
	int ret;

	if (access_size != sizeof(u8) &&
	    access_size != sizeof(u16) &&
	    access_size != sizeof(u32) &&
	    access_size != sizeof(u64) &&
	    access_size != 2 * sizeof(u64))
		return -EOPNOTSUPP;

	if (access_size == sizeof(u8))
		buffer[0] = current->thread.user_simd[reg * 2];
	else if (access_size == sizeof(u16))
		put_unaligned_le16(current->thread.user_simd[reg * 2], buffer);
	else if (access_size == sizeof(u32))
		put_unaligned_le32(current->thread.user_simd[reg * 2], buffer);
	else
		put_unaligned_le64(current->thread.user_simd[reg * 2], buffer);
	if (access_size == 2 * sizeof(u64))
		put_unaligned_le64(current->thread.user_simd[reg * 2 + 1],
				   buffer + sizeof(u64));
	ret = orlix_mte_check_access(mm, address, access_size, true);
	if (ret)
		return ret;
	ret = orlix_tcti_write_user_data(mm, address, buffer, access_size);
	if (!ret)
		orlix_tcti_clear_exclusive_monitor();
	return ret;
}

static void orlix_tcti_write_simd_fp_register(u8 reg, u8 access_size, u64 low,
					u64 high)
{
	current->thread.user_simd[reg * 2] = low;
	current->thread.user_simd[reg * 2 + 1] =
		access_size == 2 * sizeof(u64) ? high : 0;
	current->thread.user_simd_valid = 1;
}

static int orlix_tcti_read_simd_fp(struct mm_struct *mm, unsigned long address,
			     u8 access_size, u64 *low, u64 *high)
{
	u8 buffer[2 * sizeof(u64)] = {};
	int ret;

	if (access_size != sizeof(u8) &&
	    access_size != sizeof(u16) &&
	    access_size != sizeof(u32) &&
	    access_size != sizeof(u64) &&
	    access_size != 2 * sizeof(u64))
		return -EOPNOTSUPP;
	ret = orlix_mte_check_access(mm, address, access_size, false);
	if (ret)
		return ret;

	ret = orlix_tcti_read_user_data(mm, address, buffer, access_size);
	if (ret)
		return ret;

	if (access_size == sizeof(u8))
		*low = buffer[0];
	else if (access_size == sizeof(u16))
		*low = get_unaligned_le16(buffer);
	else if (access_size == sizeof(u32))
		*low = get_unaligned_le32(buffer);
	else
		*low = get_unaligned_le64(buffer);
	*high = access_size == 2 * sizeof(u64) ?
			get_unaligned_le64(buffer + sizeof(u64)) : 0;
	return 0;
}

static int orlix_tcti_load_simd_fp(struct mm_struct *mm, unsigned long address,
			     u8 reg, u8 access_size)
{
	u64 low;
	u64 high;
	int ret;

	ret = orlix_tcti_read_simd_fp(mm, address, access_size, &low, &high);
	if (ret)
		return ret;

	orlix_tcti_write_simd_fp_register(reg, access_size, low, high);
	return 0;
}

static u64 orlix_tcti_simd_lane_mask(u8 access_size)
{
	if (access_size == sizeof(u64))
		return U64_MAX;
	return GENMASK_ULL(access_size * 8 - 1, 0);
}

static u64 orlix_tcti_read_simd_lane(u8 reg, u8 lane, u8 access_size)
{
	u8 byte_offset = lane * access_size;
	u8 word = byte_offset / sizeof(u64);
	u8 shift = (byte_offset % sizeof(u64)) * 8;

	return (current->thread.user_simd[reg * 2 + word] >> shift) &
		orlix_tcti_simd_lane_mask(access_size);
}

static void orlix_tcti_write_simd_lane(u8 reg, u8 lane, u8 access_size,
				 u64 value)
{
	u8 byte_offset = lane * access_size;
	u8 word = byte_offset / sizeof(u64);
	u8 shift = (byte_offset % sizeof(u64)) * 8;
	u64 mask = orlix_tcti_simd_lane_mask(access_size) << shift;
	unsigned long *destination =
		&current->thread.user_simd[reg * 2 + word];

	*destination = (*destination & ~mask) | ((value << shift) & mask);
	current->thread.user_simd_valid = 1;
}

static u64 orlix_tcti_replicate_simd_element(u64 value, u8 access_size)
{
	u8 element_bits = access_size * 8;
	u64 element = value & orlix_tcti_simd_lane_mask(access_size);
	u64 packed = 0;
	u8 shift;

	for (shift = 0; shift < 64; shift += element_bits)
		packed |= element << shift;
	return packed;
}

static int orlix_tcti_simd_indexed_operand(
	const struct orlix_tcti_decoded_instruction *decoded, u64 result[2])
{
	u8 lane_count;
	u8 byte_offset;
	u8 word;
	u8 shift;
	u64 element;

	if (!decoded->simd_indexed || !result ||
	    (decoded->access_size != sizeof(u16) &&
	     decoded->access_size != sizeof(u32) &&
	     decoded->access_size != sizeof(u64)))
		return -EINVAL;
	lane_count = 2 * sizeof(u64) / decoded->access_size;
	if (decoded->simd_source_index >= lane_count)
		return -EINVAL;

	byte_offset = decoded->simd_source_index * decoded->access_size;
	word = byte_offset / sizeof(u64);
	shift = (byte_offset % sizeof(u64)) * 8;
	element = current->thread.user_simd[decoded->rm * 2 + word] >> shift;
	result[0] = orlix_tcti_replicate_simd_element(element,
						decoded->access_size);
	result[1] = result[0];
	return 0;
}

static int orlix_tcti_execute_simd_single_structure(
	struct mm_struct *mm, struct pt_regs *regs,
	const struct orlix_tcti_decoded_instruction *decoded,
	unsigned long *fault_address)
{
	unsigned long address = orlix_tcti_memory_base(regs, decoded->rn);
	u64 increment;
	u8 index;
	int ret;

	if (!mm)
		return -EINVAL;
	if (!decoded->simd_fp || !decoded->simd_structure_count ||
	    decoded->simd_structure_count > 4 ||
	    (decoded->access_size != sizeof(u8) &&
	     decoded->access_size != sizeof(u16) &&
	     decoded->access_size != sizeof(u32) &&
	     decoded->access_size != sizeof(u64)) ||
	    (decoded->simd_replicate && !decoded->load))
		return -EOPNOTSUPP;

	for (index = 0; index < decoded->simd_structure_count; index++) {
		u8 reg = (decoded->rd + index) & 0x1fU;
		u64 value;

		if (fault_address)
			*fault_address = address + index * decoded->access_size;
		if (decoded->load) {
			ret = orlix_tcti_load_integer(mm,
				address + index * decoded->access_size,
				decoded->access_size, &value);
			if (ret)
				return ret;
			if (decoded->simd_replicate) {
				u64 packed = orlix_tcti_replicate_simd_element(
					value, decoded->access_size);

				orlix_tcti_write_simd_fp_register(reg,
					decoded->result_size, packed, packed);
			} else {
				orlix_tcti_write_simd_lane(reg, decoded->simd_lane_index,
					decoded->access_size, value);
			}
		} else {
			value = orlix_tcti_read_simd_lane(reg,
				decoded->simd_lane_index, decoded->access_size);
			ret = orlix_tcti_store_integer(mm,
				address + index * decoded->access_size,
				decoded->access_size, value);
			if (ret)
				return ret;
		}
	}

	if (decoded->memory_index_mode == ORLIX_TCTI_MEMORY_INDEX_POST) {
		increment = decoded->rm == 31 ?
			decoded->simd_structure_count * decoded->access_size :
			orlix_tcti_read_gpr_or_zero(regs, decoded->rm, sizeof(u64));
		orlix_tcti_write_memory_base(regs, decoded->rn, address + increment);
	}
	regs->pc += sizeof(u32);
	return 0;
}

static int orlix_tcti_execute_simd_multiple_structure(
	struct mm_struct *mm, struct pt_regs *regs,
	const struct orlix_tcti_decoded_instruction *decoded,
	unsigned long *fault_address)
{
	unsigned long address = orlix_tcti_memory_base(regs, decoded->rn);
	u8 lanes;
	u8 index;
	u8 lane;
	int ret;

	if (!mm || !decoded->simd_fp || !decoded->simd_structure_count ||
	    decoded->simd_structure_count > 4 ||
	    (decoded->result_size != sizeof(u64) &&
	     decoded->result_size != 2 * sizeof(u64)) ||
	    (decoded->access_size != sizeof(u8) &&
	     decoded->access_size != sizeof(u16) &&
	     decoded->access_size != sizeof(u32) &&
	     decoded->access_size != sizeof(u64)) ||
	    decoded->result_size % decoded->access_size)
		return -EOPNOTSUPP;

	if (!decoded->simd_interleaved) {
		for (index = 0; index < decoded->simd_structure_count; index++) {
			u8 reg = (decoded->rd + index) & 0x1fU;
			unsigned long element_address =
				address + index * decoded->result_size;

			if (fault_address)
				*fault_address = element_address;
			if (decoded->load)
				ret = orlix_tcti_load_simd_fp(mm, element_address, reg,
							decoded->result_size);
			else
				ret = orlix_tcti_store_simd_fp(mm, element_address, reg,
							 decoded->result_size);
			if (ret)
				return ret;
		}
	} else {
		lanes = decoded->result_size / decoded->access_size;
		if (decoded->load && !decoded->simd_q) {
			for (index = 0; index < decoded->simd_structure_count;
			     index++)
				current->thread.user_simd[
					((decoded->rd + index) & 0x1fU) * 2 + 1] = 0;
		}

		for (lane = 0; lane < lanes; lane++) {
			for (index = 0; index < decoded->simd_structure_count;
			     index++) {
				u8 reg = (decoded->rd + index) & 0x1fU;
				u64 value;
				unsigned long element_address = address +
					(lane * decoded->simd_structure_count + index) *
					decoded->access_size;

				if (fault_address)
					*fault_address = element_address;
				if (decoded->load) {
					ret = orlix_tcti_load_integer(mm, element_address,
							decoded->access_size, &value);
					if (!ret)
						orlix_tcti_write_simd_lane(reg, lane,
								     decoded->access_size,
								     value);
				} else {
					value = orlix_tcti_read_simd_lane(reg, lane,
								    decoded->access_size);
					ret = orlix_tcti_store_integer(mm, element_address,
							 decoded->access_size, value);
				}
				if (ret)
					return ret;
			}
		}
	}

	if (decoded->memory_index_mode == ORLIX_TCTI_MEMORY_INDEX_POST) {
		u64 increment = decoded->rm == 31 ?
			decoded->simd_structure_count * decoded->result_size :
			orlix_tcti_read_gpr_or_zero(regs, decoded->rm, sizeof(u64));

		orlix_tcti_write_memory_base(regs, decoded->rn, address + increment);
	}
	regs->pc += sizeof(u32);
	return 0;
}

static u64 orlix_tcti_extend_loaded_integer(u64 value,
				      const struct orlix_tcti_decoded_instruction *decoded)
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

static unsigned long orlix_tcti_indexed_address(const struct pt_regs *regs,
					  const struct orlix_tcti_decoded_instruction *decoded)
{
	u64 base = orlix_tcti_memory_base(regs, decoded->rn);

	if (decoded->memory_index_mode == ORLIX_TCTI_MEMORY_INDEX_POST)
		return base;

	return base + decoded->memory_offset;
}

static void orlix_tcti_apply_memory_writeback(struct pt_regs *regs,
					const struct orlix_tcti_decoded_instruction *decoded)
{
	u64 base;

	if (decoded->memory_index_mode == ORLIX_TCTI_MEMORY_INDEX_SIGNED_OFFSET)
		return;

	base = orlix_tcti_memory_base(regs, decoded->rn);
	orlix_tcti_write_memory_base(regs, decoded->rn, base + decoded->memory_offset);
}

static int orlix_tcti_check_memory_sp_alignment(
	const struct pt_regs *regs,
	const struct orlix_tcti_decoded_instruction *decoded,
	unsigned long *fault_address)
{
	if (decoded->rn != 31 || IS_ALIGNED(regs->sp, 16))
		return 0;
	if (fault_address)
		*fault_address = regs->sp;
	return -EFAULT;
}

static int orlix_tcti_execute_load_literal(struct mm_struct *mm,
				     struct pt_regs *regs,
				     const struct orlix_tcti_decoded_instruction *decoded,
				     unsigned long *fault_address)
{
	unsigned long address = regs->pc + decoded->memory_offset;
	u64 value;
	int ret;

	if (decoded->prefetch) {
		regs->pc += sizeof(u32);
		return 0;
	}
	if (!mm)
		return -EINVAL;
	if (fault_address)
		*fault_address = address;

	if (decoded->simd_fp) {
		ret = orlix_tcti_load_simd_fp(mm, address, decoded->rt,
					decoded->access_size);
		if (ret)
			return ret;
	} else {
		ret = orlix_tcti_load_integer(mm, address, decoded->access_size, &value);
		if (ret)
			return ret;
		value = orlix_tcti_extend_loaded_integer(value, decoded);
		orlix_tcti_write_gpr_or_zero(regs, decoded->rt,
				       decoded->result_size, value);
	}

	regs->pc += sizeof(u32);
	return 0;
}

static int orlix_tcti_execute_load_store_pair(struct mm_struct *mm,
					struct pt_regs *regs,
					const struct orlix_tcti_decoded_instruction *decoded,
					unsigned long *fault_address)
{
	unsigned long address = orlix_tcti_indexed_address(regs, decoded);
	u64 first;
	u64 second;
	int ret;

	ret = orlix_tcti_check_memory_sp_alignment(regs, decoded, fault_address);
	if (ret)
		return ret;
	if (!mm)
		return -EINVAL;
	if (fault_address)
		*fault_address = address;
	if (decoded->memory_tag_store_pair) {
		u8 pair[2 * sizeof(u64)];

		if (decoded->load || decoded->simd_fp ||
		    decoded->access_size != sizeof(u64))
			return -EOPNOTSUPP;
		if (!IS_ALIGNED(address, 2 * sizeof(u64)))
			return -EFAULT;
		put_unaligned_le64(orlix_tcti_read_gpr_or_zero(
			regs, decoded->rt, sizeof(u64)), pair);
		put_unaligned_le64(orlix_tcti_read_gpr_or_zero(
			regs, decoded->rt2, sizeof(u64)), pair + sizeof(u64));
		ret = orlix_tcti_store_tagged_pair(mm, address, pair,
						 sizeof(pair));
		if (ret)
			return ret;
		orlix_tcti_clear_exclusive_monitor();
		orlix_tcti_apply_memory_writeback(regs, decoded);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->load) {
		if (decoded->simd_fp) {
			u64 first_low;
			u64 first_high;
			u64 second_low;
			u64 second_high;

			ret = orlix_tcti_read_simd_fp(mm, address,
						decoded->access_size,
						&first_low, &first_high);
			if (ret)
				return ret;
			if (fault_address)
				*fault_address = address + decoded->access_size;
			ret = orlix_tcti_read_simd_fp(mm,
						address + decoded->access_size,
						decoded->access_size,
						&second_low, &second_high);
			if (ret)
				return ret;
			orlix_tcti_write_simd_fp_register(decoded->rt,
						    decoded->access_size,
						    first_low, first_high);
			orlix_tcti_write_simd_fp_register(decoded->rt2,
						    decoded->access_size,
						    second_low, second_high);
		} else {
			ret = orlix_tcti_load_integer(mm, address, decoded->access_size,
						&first);
			if (ret)
				return ret;
			if (fault_address)
				*fault_address = address + decoded->access_size;
			ret = orlix_tcti_load_integer(mm, address + decoded->access_size,
						decoded->access_size, &second);
			if (ret)
				return ret;
			first = orlix_tcti_extend_loaded_integer(first, decoded);
			second = orlix_tcti_extend_loaded_integer(second, decoded);
			orlix_tcti_write_gpr_or_zero(regs, decoded->rt,
					       decoded->result_size, first);
			orlix_tcti_write_gpr_or_zero(regs, decoded->rt2,
					       decoded->result_size, second);
		}
	} else {
		if (decoded->simd_fp) {
			ret = orlix_tcti_store_simd_fp(mm, address, decoded->rt,
						 decoded->access_size);
			if (ret)
				return ret;
			if (fault_address)
				*fault_address = address + decoded->access_size;
			ret = orlix_tcti_store_simd_fp(mm, address + decoded->access_size,
						 decoded->rt2,
						 decoded->access_size);
			if (ret)
				return ret;
		} else {
			first = orlix_tcti_read_gpr_or_zero(regs, decoded->rt,
						      decoded->access_size);
			second = orlix_tcti_read_gpr_or_zero(regs, decoded->rt2,
						       decoded->access_size);
			ret = orlix_tcti_store_integer(mm, address, decoded->access_size,
						 first);
			if (ret)
				return ret;
			if (fault_address)
				*fault_address = address + decoded->access_size;
			ret = orlix_tcti_store_integer(mm, address + decoded->access_size,
						 decoded->access_size, second);
			if (ret)
				return ret;
		}
	}

	orlix_tcti_apply_memory_writeback(regs, decoded);
	regs->pc += sizeof(u32);
	return 0;
}

static int orlix_tcti_execute_load_store_immediate(struct mm_struct *mm,
					     struct pt_regs *regs,
					     const struct orlix_tcti_decoded_instruction *decoded,
					     unsigned long *fault_address)
{
	unsigned long address = orlix_tcti_indexed_address(regs, decoded);
	u64 value;
	int ret;

	ret = orlix_tcti_check_memory_sp_alignment(regs, decoded, fault_address);
	if (ret)
		return ret;
	if (decoded->prefetch) {
		regs->pc += sizeof(u32);
		return 0;
	}
	if (!mm)
		return -EINVAL;
	if (fault_address)
		*fault_address = address;

	if (decoded->load) {
		if (decoded->simd_fp) {
			ret = orlix_tcti_load_simd_fp(mm, address, decoded->rt,
						decoded->access_size);
			if (ret)
				return ret;
			orlix_tcti_apply_memory_writeback(regs, decoded);
			regs->pc += sizeof(u32);
			return 0;
		}

		ret = orlix_tcti_load_integer(mm, address, decoded->access_size, &value);
		if (ret)
			return ret;
		value = orlix_tcti_extend_loaded_integer(value, decoded);
		orlix_tcti_write_gpr_or_zero(regs, decoded->rt,
				       decoded->result_size, value);
	} else {
		if (decoded->simd_fp) {
			ret = orlix_tcti_store_simd_fp(mm, address, decoded->rt,
						 decoded->access_size);
			if (ret)
				return ret;
			orlix_tcti_apply_memory_writeback(regs, decoded);
			regs->pc += sizeof(u32);
			return 0;
		}

		value = orlix_tcti_read_gpr_or_zero(regs, decoded->rt,
					      decoded->access_size);
		ret = orlix_tcti_store_integer(mm, address, decoded->access_size, value);
		if (ret)
			return ret;
	}

	orlix_tcti_apply_memory_writeback(regs, decoded);
	regs->pc += sizeof(u32);
	return 0;
}

static s64 orlix_tcti_register_offset(const struct pt_regs *regs,
				const struct orlix_tcti_decoded_instruction *decoded)
{
	u64 value = orlix_tcti_read_gpr_or_zero(regs, decoded->rm, sizeof(u64));

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

static int orlix_tcti_execute_load_store_register_offset(struct mm_struct *mm,
						  struct pt_regs *regs,
						  const struct orlix_tcti_decoded_instruction *decoded,
						  unsigned long *fault_address)
{
	unsigned long address = orlix_tcti_memory_base(regs, decoded->rn) +
				orlix_tcti_register_offset(regs, decoded);
	u64 value;
	int ret;

	ret = orlix_tcti_check_memory_sp_alignment(regs, decoded, fault_address);
	if (ret)
		return ret;
	if (decoded->prefetch) {
		regs->pc += sizeof(u32);
		return 0;
	}
	if (!mm)
		return -EINVAL;
	if (fault_address)
		*fault_address = address;

	if (decoded->load) {
		if (decoded->simd_fp) {
			ret = orlix_tcti_load_simd_fp(mm, address, decoded->rt,
						decoded->access_size);
			if (ret)
				return ret;
			regs->pc += sizeof(u32);
			return 0;
		}

		ret = orlix_tcti_load_integer(mm, address, decoded->access_size, &value);
		if (ret)
			return ret;
		value = orlix_tcti_extend_loaded_integer(value, decoded);
		orlix_tcti_write_gpr_or_zero(regs, decoded->rt,
				       decoded->result_size, value);
	} else {
		if (decoded->simd_fp) {
			ret = orlix_tcti_store_simd_fp(mm, address, decoded->rt,
						 decoded->access_size);
			if (ret)
				return ret;
			regs->pc += sizeof(u32);
			return 0;
		}

		value = orlix_tcti_read_gpr_or_zero(regs, decoded->rt,
					      decoded->access_size);
		ret = orlix_tcti_store_integer(mm, address, decoded->access_size, value);
		if (ret)
			return ret;
	}

	regs->pc += sizeof(u32);
	return 0;
}

static int orlix_tcti_execute_gcs_store(struct mm_struct *mm,
				 struct pt_regs *regs,
				 const struct orlix_tcti_decoded_instruction *decoded,
				 unsigned long *fault_address)
{
	unsigned long address = orlix_tcti_memory_base(regs, decoded->rn);
	u64 value;
	u8 buffer[sizeof(u64)];
	int ret;

	ret = orlix_tcti_check_memory_sp_alignment(regs, decoded, fault_address);
	if (ret)
		return ret;
	if (!mm)
		return -EINVAL;
	if (fault_address)
		*fault_address = address;
	value = orlix_tcti_read_gpr_or_zero(regs, decoded->rt, sizeof(value));
	put_unaligned_le64(value, buffer);
	ret = orlix_tcti_write_gcs_user_data(mm, address, buffer, sizeof(buffer));
	if (ret)
		return ret;
	orlix_tcti_clear_exclusive_monitor();
	regs->pc += sizeof(u32);
	return 0;
}

static int orlix_tcti_execute_load_store_exclusive(struct mm_struct *mm,
					    struct pt_regs *regs,
					    const struct orlix_tcti_decoded_instruction *decoded,
					    unsigned long *fault_address)
{
	unsigned long address = orlix_tcti_memory_base(regs, decoded->rn);
	u8 loaded[2 * sizeof(u64)] = {};
	u8 desired[2 * sizeof(u64)] = {};
	u8 total_size = decoded->access_size * (decoded->pair ? 2 : 1);
	unsigned long reservation_pfn = 0;
	u64 reservation_generation = 0;
	u64 reservation_mapping_generation = 0;
	u64 value = 0;
	u64 value2 = 0;
	bool exchanged;
	int ret;

	if (fault_address)
		*fault_address = address;
	if (!IS_ALIGNED(address, total_size))
		return -EFAULT;

	if (decoded->load) {
		if (!mm)
			return -EINVAL;
		ret = orlix_tcti_load_exclusive_user_data(mm, address, loaded,
						    total_size, &reservation_pfn,
						    &reservation_generation,
						    &reservation_mapping_generation);
		if (ret)
			return ret;
		ret = orlix_tcti_decode_integer(loaded, decoded->access_size, &value);
		if (ret)
			return ret;
		if (decoded->pair) {
			ret = orlix_tcti_decode_integer(loaded + decoded->access_size,
						  decoded->access_size, &value2);
			if (ret)
				return ret;
		}
		value = orlix_tcti_extend_loaded_integer(value, decoded);
		orlix_tcti_write_gpr_or_zero(regs, decoded->rt,
				       decoded->result_size, value);
		if (decoded->pair)
			orlix_tcti_write_gpr_or_zero(regs, decoded->rt2,
					       decoded->result_size, value2);
		if (decoded->exclusive) {
			current->thread.user_exclusive_address = address;
			current->thread.user_exclusive_value = value;
			current->thread.user_exclusive_value2 = value2;
			current->thread.user_exclusive_pfn = reservation_pfn;
			current->thread.user_exclusive_generation =
				reservation_generation;
			current->thread.user_exclusive_mapping_generation =
				reservation_mapping_generation;
			current->thread.user_exclusive_size = total_size;
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
	     current->thread.user_exclusive_size != total_size)) {
		orlix_tcti_write_gpr_or_zero(regs, decoded->rs, sizeof(u32), 1);
		orlix_tcti_clear_exclusive_monitor();
		regs->pc += sizeof(u32);
		return 0;
	}

	if (!mm)
		return -EINVAL;

	value = orlix_tcti_read_gpr_or_zero(regs, decoded->rt, decoded->access_size);
	if (decoded->exclusive) {
		if (decoded->pair)
			value2 = orlix_tcti_read_gpr_or_zero(regs, decoded->rt2,
						      decoded->access_size);
		ret = orlix_tcti_encode_integer(desired, decoded->access_size,
					  value);
		if (!ret && decoded->pair)
			ret = orlix_tcti_encode_integer(desired + decoded->access_size,
						  decoded->access_size, value2);
		if (!ret)
			ret = orlix_tcti_store_exclusive_user_data(
				mm, address, desired, total_size,
				current->thread.user_exclusive_pfn,
				current->thread.user_exclusive_generation,
				current->thread.user_exclusive_mapping_generation,
				&exchanged);
		orlix_tcti_clear_exclusive_monitor();
		if (ret)
			return ret;
		orlix_tcti_write_gpr_or_zero(regs, decoded->rs, sizeof(u32),
				       exchanged ? 0 : 1);
	} else {
		ret = orlix_tcti_store_integer(mm, address, decoded->access_size,
					 value);
		if (ret)
			return ret;
	}
	regs->pc += sizeof(u32);
	return 0;
}

static int orlix_tcti_lse_memory_order(const struct orlix_tcti_decoded_instruction *decoded,
				 enum orlix_tcti_atomic_memory_order *order)
{
	if (!decoded || !order)
		return -EINVAL;

	if (decoded->acquire)
		*order = decoded->release ? ORLIX_TCTI_ATOMIC_MEMORY_ACQ_REL :
			 ORLIX_TCTI_ATOMIC_MEMORY_ACQUIRE;
	else
		*order = decoded->release ? ORLIX_TCTI_ATOMIC_MEMORY_RELEASE :
			 ORLIX_TCTI_ATOMIC_MEMORY_RELAXED;
	return 0;
}

static int orlix_tcti_lse_memory_operation(enum orlix_tcti_lse_atomic_op lse_op,
				     enum orlix_tcti_atomic_memory_operation *operation)
{
	if (!operation)
		return -EINVAL;

	switch (lse_op) {
	case ORLIX_TCTI_LSE_ATOMIC_CAS:
		*operation = ORLIX_TCTI_ATOMIC_MEMORY_CAS;
		return 0;
	case ORLIX_TCTI_LSE_ATOMIC_SWP:
		*operation = ORLIX_TCTI_ATOMIC_MEMORY_SWP;
		return 0;
	case ORLIX_TCTI_LSE_ATOMIC_ADD:
		*operation = ORLIX_TCTI_ATOMIC_MEMORY_ADD;
		return 0;
	case ORLIX_TCTI_LSE_ATOMIC_CLR:
		*operation = ORLIX_TCTI_ATOMIC_MEMORY_CLR;
		return 0;
	case ORLIX_TCTI_LSE_ATOMIC_EOR:
		*operation = ORLIX_TCTI_ATOMIC_MEMORY_EOR;
		return 0;
	case ORLIX_TCTI_LSE_ATOMIC_SET:
		*operation = ORLIX_TCTI_ATOMIC_MEMORY_SET;
		return 0;
	case ORLIX_TCTI_LSE_ATOMIC_SMAX:
		*operation = ORLIX_TCTI_ATOMIC_MEMORY_SMAX;
		return 0;
	case ORLIX_TCTI_LSE_ATOMIC_SMIN:
		*operation = ORLIX_TCTI_ATOMIC_MEMORY_SMIN;
		return 0;
	case ORLIX_TCTI_LSE_ATOMIC_UMAX:
		*operation = ORLIX_TCTI_ATOMIC_MEMORY_UMAX;
		return 0;
	case ORLIX_TCTI_LSE_ATOMIC_UMIN:
		*operation = ORLIX_TCTI_ATOMIC_MEMORY_UMIN;
		return 0;
	default:
		return -EINVAL;
	}
}

static bool
orlix_tcti_memory_alignment_fault(
	const struct orlix_tcti_decoded_instruction *decoded,
	unsigned long address)
{
	u8 size;

	if (!decoded)
		return false;
	if (decoded->decode_class == ORLIX_TCTI_DECODE_MEMORY_TAGGING) {
		if (decoded->rn == 31 && !IS_ALIGNED(address, 16))
			return true;
		return (decoded->memory_tagging_op == ORLIX_TCTI_MTE_STZG ||
			decoded->memory_tagging_op == ORLIX_TCTI_MTE_STZ2G) &&
			!IS_ALIGNED(address, 16);
	}
	if (decoded->decode_class != ORLIX_TCTI_DECODE_LSE_ATOMIC)
		return false;
	if (decoded->rn == 31 && !IS_ALIGNED(address, 16))
		return true;
	size = decoded->access_size * (decoded->pair ? 2 : 1);
	return size && !IS_ALIGNED(address, size);
}

static int orlix_tcti_execute_lse_atomic(struct mm_struct *mm,
				   struct pt_regs *regs,
				   const struct orlix_tcti_decoded_instruction *decoded,
				   unsigned long *fault_address)
{
	enum orlix_tcti_atomic_memory_operation operation;
	enum orlix_tcti_atomic_memory_order order;
	unsigned long address;
	u8 expected[2 * sizeof(u64)] = {};
	u8 operand[2 * sizeof(u64)] = {};
	u8 old_value[2 * sizeof(u64)] = {};
	u8 total_size;
	u64 source;
	u64 source2;
	u64 old;
	u64 old2;
	bool exchanged;
	int ret;

	if (!mm || !regs || !decoded)
		return -EINVAL;

	address = orlix_tcti_memory_base(regs, decoded->rn);
	if (fault_address)
		*fault_address = address;
	if (decoded->access_size != sizeof(u8) &&
	    decoded->access_size != sizeof(u16) &&
	    decoded->access_size != sizeof(u32) &&
	    decoded->access_size != sizeof(u64))
		return -EINVAL;
	if (decoded->pair && !decoded->lse128 &&
	    ((decoded->lse_atomic_op == ORLIX_TCTI_LSE_ATOMIC_CAS &&
	      decoded->access_size < sizeof(u32)) || decoded->rs > 30 ||
	     decoded->rt > 30))
		return -EINVAL;

	total_size = decoded->access_size * (decoded->pair ? 2 : 1);
	if (orlix_tcti_memory_alignment_fault(decoded, address))
		return -EFAULT;

	ret = orlix_tcti_lse_memory_order(decoded, &order);
	if (ret)
		return ret;
	ret = orlix_tcti_lse_memory_operation(decoded->lse_atomic_op, &operation);
	if (ret)
		return ret;

	source = orlix_tcti_read_gpr_or_zero(regs,
				       decoded->lse128 ? decoded->rt : decoded->rs,
				       decoded->access_size);
	ret = orlix_tcti_encode_integer(operand, decoded->access_size,
				  decoded->lse_atomic_op == ORLIX_TCTI_LSE_ATOMIC_CAS ?
				  orlix_tcti_read_gpr_or_zero(regs, decoded->rt,
						decoded->access_size) : source);
	if (ret)
		return ret;
	if (operation == ORLIX_TCTI_ATOMIC_MEMORY_CAS) {
		ret = orlix_tcti_encode_integer(expected, decoded->access_size, source);
		if (ret)
			return ret;
	}
	if (decoded->pair) {
		/* Snapshot both source lanes before any returned old-value writeback. */
		source2 = orlix_tcti_read_gpr_or_zero(regs,
					decoded->lse128 ? decoded->rt2 : decoded->rs + 1,
					decoded->access_size);
		ret = orlix_tcti_encode_integer(operation == ORLIX_TCTI_ATOMIC_MEMORY_CAS ?
					  expected + decoded->access_size :
					  operand + decoded->access_size,
					  decoded->access_size, source2);
		if (ret)
			return ret;
		if (operation == ORLIX_TCTI_ATOMIC_MEMORY_CAS) {
			ret = orlix_tcti_encode_integer(operand + decoded->access_size,
						  decoded->access_size,
						  orlix_tcti_read_gpr_or_zero(regs, decoded->rt + 1,
								       decoded->access_size));
			if (ret)
				return ret;
		}
	}

	ret = orlix_tcti_atomic_user_data(mm, address, operation, order,
				    operation == ORLIX_TCTI_ATOMIC_MEMORY_CAS ? expected : NULL,
				    operand, old_value, total_size, &exchanged);
	if (ret)
		return ret;
	if (operation != ORLIX_TCTI_ATOMIC_MEMORY_CAS || exchanged)
		orlix_tcti_clear_exclusive_monitor();
	ret = orlix_tcti_decode_integer(old_value, decoded->access_size, &old);
	if (ret)
		return ret;
	if (decoded->pair) {
		ret = orlix_tcti_decode_integer(old_value + decoded->access_size,
					  decoded->access_size, &old2);
		if (ret)
			return ret;
	}

	if (decoded->lse128) {
		orlix_tcti_write_gpr_or_zero(regs, decoded->rt, decoded->access_size, old);
		orlix_tcti_write_gpr_or_zero(regs, decoded->rt2, decoded->access_size, old2);
	} else if (operation == ORLIX_TCTI_ATOMIC_MEMORY_CAS) {
		orlix_tcti_write_gpr_or_zero(regs, decoded->rs, decoded->access_size, old);
		if (decoded->pair)
			orlix_tcti_write_gpr_or_zero(regs, decoded->rs + 1,
					       decoded->access_size, old2);
	} else {
		orlix_tcti_write_gpr_or_zero(regs, decoded->rt, decoded->access_size, old);
		if (decoded->pair)
			orlix_tcti_write_gpr_or_zero(regs, decoded->rt2,
					       decoded->access_size, old2);
	}
	regs->pc += sizeof(u32);
	return 0;
}

static int orlix_tcti_execute_simd_modified_immediate(
	struct pt_regs *regs, const struct orlix_tcti_decoded_instruction *decoded)
{
	u64 low = current->thread.user_simd[decoded->rd * 2];
	u64 high = current->thread.user_simd[decoded->rd * 2 + 1];
	u64 immediate = decoded->logical_immediate;

	switch (decoded->simd_modified_immediate_op) {
	case ORLIX_TCTI_SIMD_MODIMM_MOVI:
		low = immediate;
		high = immediate;
		break;
	case ORLIX_TCTI_SIMD_MODIMM_MVNI:
		low = ~immediate;
		high = ~immediate;
		break;
	case ORLIX_TCTI_SIMD_MODIMM_ORR:
		low |= immediate;
		high |= immediate;
		break;
	case ORLIX_TCTI_SIMD_MODIMM_BIC:
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

static int orlix_tcti_execute_fp_scalar_immediate(
	struct pt_regs *regs, const struct orlix_tcti_decoded_instruction *decoded)
{
	orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
				    decoded->logical_immediate, 0);
	regs->pc += sizeof(u32);
	return 0;
}

static int orlix_tcti_execute_simd_vector_element_move(
	struct pt_regs *regs, const struct orlix_tcti_decoded_instruction *decoded)
{
	u64 value;
	u64 word;
	u64 mask;
	u8 source_word;
	u8 destination_word;
	u8 source_shift;
	u8 destination_shift;

	if (decoded->simd_element_move_op == ORLIX_TCTI_SIMD_ELEMENT_MOVE_EXT) {
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
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    low, high);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_element_move_op == ORLIX_TCTI_SIMD_ELEMENT_MOVE_INS_GPR) {
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
		value = orlix_tcti_read_gpr_or_zero(regs, decoded->rn,
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

	if (decoded->simd_element_move_op == ORLIX_TCTI_SIMD_ELEMENT_MOVE_UMOV) {
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
		orlix_tcti_write_gpr_or_zero(regs, decoded->rd, decoded->result_size,
				       value);
		regs->pc += sizeof(u32);
		return 0;
	}
	if (decoded->simd_element_move_op == ORLIX_TCTI_SIMD_ELEMENT_MOVE_SMOV) {
		u8 byte_offset;
		u8 sign_bit;

		if (decoded->result_size == sizeof(u32) &&
		    decoded->access_size != sizeof(u8) &&
		    decoded->access_size != sizeof(u16))
			return -EOPNOTSUPP;
		if (decoded->result_size == sizeof(u64) &&
		    decoded->access_size != sizeof(u8) &&
		    decoded->access_size != sizeof(u16) &&
		    decoded->access_size != sizeof(u32))
			return -EOPNOTSUPP;
		if (decoded->result_size != sizeof(u32) &&
		    decoded->result_size != sizeof(u64))
			return -EOPNOTSUPP;

		byte_offset = decoded->simd_source_index *
			      decoded->access_size;
		if (byte_offset + decoded->access_size > 2 * sizeof(u64))
			return -EOPNOTSUPP;

		source_word = decoded->rn * 2 + byte_offset / sizeof(u64);
		source_shift = (byte_offset % sizeof(u64)) * 8;
		value = (current->thread.user_simd[source_word] >> source_shift) &
			GENMASK_ULL(decoded->access_size * 8 - 1, 0);
		sign_bit = decoded->access_size * 8 - 1;
		value = sign_extend64(value, sign_bit);
		orlix_tcti_write_gpr_or_zero(regs, decoded->rd,
				       decoded->result_size, value);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_element_move_op == ORLIX_TCTI_SIMD_ELEMENT_MOVE_DUP &&
	    decoded->simd_scalar && !decoded->immediate) {
		u8 byte_offset;

		if (decoded->access_size != decoded->result_size ||
		    (decoded->access_size != sizeof(u8) &&
		     decoded->access_size != sizeof(u16) &&
		     decoded->access_size != sizeof(u32) &&
		     decoded->access_size != sizeof(u64)))
			return -EOPNOTSUPP;
		byte_offset = decoded->simd_source_index * decoded->access_size;
		if (byte_offset + decoded->access_size > 2 * sizeof(u64))
			return -EOPNOTSUPP;
		source_word = decoded->rn * 2 + byte_offset / sizeof(u64);
		source_shift = (byte_offset % sizeof(u64)) * 8;
		value = (current->thread.user_simd[source_word] >> source_shift) &
			GENMASK_ULL(decoded->access_size * 8 - 1, 0);
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
			value, 0);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_element_move_op >= ORLIX_TCTI_SIMD_ELEMENT_MOVE_UZP1 &&
	    decoded->simd_element_move_op <= ORLIX_TCTI_SIMD_ELEMENT_MOVE_ZIP2) {
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
			case ORLIX_TCTI_SIMD_ELEMENT_MOVE_UZP1:
			case ORLIX_TCTI_SIMD_ELEMENT_MOVE_UZP2:
				source_vector = destination_lane >= half;
				source_lane = (destination_lane % half) * 2 +
					(decoded->simd_element_move_op ==
					 ORLIX_TCTI_SIMD_ELEMENT_MOVE_UZP2);
				break;
			case ORLIX_TCTI_SIMD_ELEMENT_MOVE_TRN1:
			case ORLIX_TCTI_SIMD_ELEMENT_MOVE_TRN2:
				source_vector = destination_lane & 1U;
				source_lane = (destination_lane / 2) * 2 +
					(decoded->simd_element_move_op ==
					 ORLIX_TCTI_SIMD_ELEMENT_MOVE_TRN2);
				break;
			case ORLIX_TCTI_SIMD_ELEMENT_MOVE_ZIP1:
			case ORLIX_TCTI_SIMD_ELEMENT_MOVE_ZIP2:
				source_vector = destination_lane & 1U;
				source_lane = destination_lane / 2 +
					(decoded->simd_element_move_op ==
					 ORLIX_TCTI_SIMD_ELEMENT_MOVE_ZIP2 ? half : 0);
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

		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_element_move_op == ORLIX_TCTI_SIMD_ELEMENT_MOVE_SSHLL ||
	    decoded->simd_element_move_op == ORLIX_TCTI_SIMD_ELEMENT_MOVE_USHLL) {
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
			    ORLIX_TCTI_SIMD_ELEMENT_MOVE_SSHLL)
				widened = sign_extend64(widened,
						 decoded->access_size * 8 - 1);
			widened = (widened << decoded->shift_amount) &
				  destination_mask;
			if (dest_word)
				high |= widened << dest_shift;
			else
				low |= widened << dest_shift;
		}
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    low, high);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_element_move_op == ORLIX_TCTI_SIMD_ELEMENT_MOVE_XTN) {
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
			orlix_tcti_write_simd_fp_register(
				decoded->rd, 2 * sizeof(u64),
				current->thread.user_simd[decoded->rd * 2], narrowed);
		else
			orlix_tcti_write_simd_fp_register(decoded->rd, sizeof(u64),
						    narrowed, 0);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_element_move_op == ORLIX_TCTI_SIMD_ELEMENT_MOVE_DUP &&
	    !decoded->simd_scalar && !decoded->immediate) {
		u8 byte_offset;
		u8 lane_bits;
		u64 high_word;

		if ((decoded->access_size != sizeof(u8) &&
		     decoded->access_size != sizeof(u16) &&
		     decoded->access_size != sizeof(u32) &&
		     decoded->access_size != sizeof(u64)) ||
		    (decoded->result_size != sizeof(u64) &&
		     decoded->result_size != 2 * sizeof(u64)) ||
		    decoded->access_size > decoded->result_size)
			return -EOPNOTSUPP;

		byte_offset = decoded->simd_source_index *
			      decoded->access_size;
		if (byte_offset + decoded->access_size > 2 * sizeof(u64))
			return -EOPNOTSUPP;

		source_word = decoded->rn * 2 + byte_offset / sizeof(u64);
		source_shift = (byte_offset % sizeof(u64)) * 8;
		lane_bits = decoded->access_size * 8;
		word = (current->thread.user_simd[source_word] >> source_shift) &
		       GENMASK_ULL(lane_bits - 1, 0);
		while (lane_bits < 64) {
			word |= word << lane_bits;
			lane_bits *= 2;
		}
		high_word = decoded->result_size > sizeof(u64) ? word : 0;
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    word, high_word);
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
		value = orlix_tcti_read_gpr_or_zero(regs, decoded->rn,
					      decoded->access_size);
		word = value & GENMASK_ULL(lane_bits - 1, 0);
		while (lane_bits < 64) {
			word |= word << lane_bits;
			lane_bits *= 2;
		}
		orlix_tcti_write_simd_fp_register(
			decoded->rd, decoded->result_size, word,
			decoded->result_size > sizeof(u64) ? word : 0);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->access_size == sizeof(u64) && decoded->rd == decoded->rn &&
	    decoded->simd_destination_index == decoded->simd_source_index) {
		current->thread.user_simd_valid = 1;
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_element_move_op ==
	    ORLIX_TCTI_SIMD_ELEMENT_MOVE_INS_ELEMENT) {
		u8 lane_count;
		u8 source_byte;
		u8 destination_byte;

		if (decoded->access_size != sizeof(u8) &&
		    decoded->access_size != sizeof(u16) &&
		    decoded->access_size != sizeof(u32) &&
		    decoded->access_size != sizeof(u64))
			return -EOPNOTSUPP;
		lane_count = 2 * sizeof(u64) / decoded->access_size;
		if (decoded->simd_source_index >= lane_count ||
		    decoded->simd_destination_index >= lane_count)
			return -EOPNOTSUPP;

		source_byte = decoded->simd_source_index * decoded->access_size;
		destination_byte = decoded->simd_destination_index *
				   decoded->access_size;
		source_word = decoded->rn * 2 + source_byte / sizeof(u64);
		destination_word = decoded->rd * 2 +
				   destination_byte / sizeof(u64);
		source_shift = (source_byte % sizeof(u64)) * 8;
		destination_shift = (destination_byte % sizeof(u64)) * 8;
		mask = GENMASK_ULL(decoded->access_size * 8 - 1, 0);
		value = (current->thread.user_simd[source_word] >> source_shift) &
			mask;
		word = current->thread.user_simd[destination_word];
		word = (word & ~(mask << destination_shift)) |
			(value << destination_shift);
		current->thread.user_simd[destination_word] = word;
		current->thread.user_simd_valid = 1;
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

static int orlix_tcti_execute_simd_table_lookup(
	struct pt_regs *regs, const struct orlix_tcti_decoded_instruction *decoded)
{
	u8 table[4 * 2 * sizeof(u64)];
	u8 indexes[2 * sizeof(u64)];
	u8 original[2 * sizeof(u64)];
	u8 result[2 * sizeof(u64)] = {};
	u8 result_size = decoded->simd_q ? 2 * sizeof(u64) : sizeof(u64);
	u8 table_size;
	u8 i;

	if (decoded->simd_table_count < 1 || decoded->simd_table_count > 4 ||
	    decoded->simd_table_lookup_op > ORLIX_TCTI_SIMD_TABLE_LOOKUP_TBX ||
	    decoded->result_size != result_size)
		return -EOPNOTSUPP;

	for (i = 0; i < decoded->simd_table_count; i++) {
		u8 reg = (decoded->rn + i) & 0x1fU;

		put_unaligned_le64(current->thread.user_simd[reg * 2],
				   table + i * 2 * sizeof(u64));
		put_unaligned_le64(current->thread.user_simd[reg * 2 + 1],
				   table + i * 2 * sizeof(u64) + sizeof(u64));
	}
	put_unaligned_le64(current->thread.user_simd[decoded->rm * 2], indexes);
	put_unaligned_le64(current->thread.user_simd[decoded->rm * 2 + 1],
			   indexes + sizeof(u64));
	put_unaligned_le64(current->thread.user_simd[decoded->rd * 2], original);
	put_unaligned_le64(current->thread.user_simd[decoded->rd * 2 + 1],
			   original + sizeof(u64));
	table_size = decoded->simd_table_count * 2 * sizeof(u64);

	for (i = 0; i < result_size; i++) {
		if (indexes[i] < table_size)
			result[i] = table[indexes[i]];
		else if (decoded->simd_table_lookup_op ==
			 ORLIX_TCTI_SIMD_TABLE_LOOKUP_TBX)
			result[i] = original[i];
	}

	orlix_tcti_write_simd_fp_register(decoded->rd, result_size,
				    get_unaligned_le64(result),
				    result_size == 2 * sizeof(u64) ?
					get_unaligned_le64(result + sizeof(u64)) : 0);
	regs->pc += sizeof(u32);
	return 0;
}

static int orlix_tcti_execute_simd_vector_logical(
	struct pt_regs *regs, const struct orlix_tcti_decoded_instruction *decoded)
{
	u64 left_low;
	u64 left_high;
	u64 right_low;
	u64 right_high;
	u64 destination_low;
	u64 destination_high;
	u64 result_low;
	u64 result_high;

	if (decoded->logical_op > ORLIX_TCTI_LOGICAL_BIF ||
	    (decoded->access_size != sizeof(u64) &&
	     decoded->access_size != 2 * sizeof(u64)))
		return -EOPNOTSUPP;

	left_low = current->thread.user_simd[decoded->rn * 2];
	left_high = current->thread.user_simd[decoded->rn * 2 + 1];
	right_low = current->thread.user_simd[decoded->rm * 2];
	right_high = current->thread.user_simd[decoded->rm * 2 + 1];
	if (decoded->logical_op >= ORLIX_TCTI_LOGICAL_BSL) {
		destination_low = current->thread.user_simd[decoded->rd * 2];
		destination_high =
			current->thread.user_simd[decoded->rd * 2 + 1];
	}

	switch (decoded->logical_op) {
	case ORLIX_TCTI_LOGICAL_AND:
		result_low = left_low & right_low;
		result_high = left_high & right_high;
		break;
	case ORLIX_TCTI_LOGICAL_BIC:
		result_low = left_low & ~right_low;
		result_high = left_high & ~right_high;
		break;
	case ORLIX_TCTI_LOGICAL_ORR:
		result_low = left_low | right_low;
		result_high = left_high | right_high;
		break;
	case ORLIX_TCTI_LOGICAL_ORN:
		result_low = left_low | ~right_low;
		result_high = left_high | ~right_high;
		break;
	case ORLIX_TCTI_LOGICAL_EOR:
		result_low = left_low ^ right_low;
		result_high = left_high ^ right_high;
		break;
	case ORLIX_TCTI_LOGICAL_BSL:
		result_low = (left_low & destination_low) |
			     (right_low & ~destination_low);
		result_high = (left_high & destination_high) |
			      (right_high & ~destination_high);
		break;
	case ORLIX_TCTI_LOGICAL_BIT:
		result_low = (destination_low & ~right_low) |
			     (left_low & right_low);
		result_high = (destination_high & ~right_high) |
			      (left_high & right_high);
		break;
	case ORLIX_TCTI_LOGICAL_BIF:
		result_low = (destination_low & right_low) |
			     (left_low & ~right_low);
		result_high = (destination_high & right_high) |
			      (left_high & ~right_high);
		break;
	default:
		return -EOPNOTSUPP;
	}

	orlix_tcti_write_simd_fp_register(decoded->rd, decoded->access_size,
				    result_low, result_high);
	regs->pc += sizeof(u32);
	return 0;
}

static u64 orlix_tcti_simd_shift_lane(u64 value, u8 bits, s8 shift,
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

static u64 orlix_tcti_simd_saturating_add_sub_lane(u64 left, u64 right, u8 bits,
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

static u64 orlix_tcti_simd_mixed_saturating_add_lane(u64 destination, u64 source,
					       u8 bits,
					       bool unsigned_destination,
					       bool *saturated)
{
	u64 mask = GENMASK_ULL(bits - 1, 0);
	__int128 result;
	__int128 minimum;
	__int128 maximum;

	if (unsigned_destination) {
		result = (__int128)(destination & mask) +
			 sign_extend64(source & mask, bits - 1);
		minimum = 0;
		maximum = mask;
	} else {
		result = (__int128)sign_extend64(destination & mask, bits - 1) +
			 (source & mask);
		minimum = -((__int128)1 << (bits - 1));
		maximum = ((__int128)1 << (bits - 1)) - 1;
	}
	if (result < minimum) {
		*saturated = true;
		result = minimum;
	} else if (result > maximum) {
		*saturated = true;
		result = maximum;
	}
	return (u64)result & mask;
}

static u64 orlix_tcti_simd_saturating_mul_high_lane(u64 left, u64 right, u8 bits,
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

static u64 orlix_tcti_simd_saturating_double_mul_long_lane(u64 left, u64 right,
						      u64 accumulator,
						      u8 source_bits,
						      bool accumulate,
						      bool subtract,
						      bool *saturated)
{
	u8 result_bits = source_bits * 2;
	u64 source_mask = GENMASK_ULL(source_bits - 1, 0);
	u64 result_mask = GENMASK_ULL(result_bits - 1, 0);
	__int128 result;
	__int128 minimum;
	__int128 maximum;

	result = (__int128)sign_extend64(left & source_mask, source_bits - 1) *
		 sign_extend64(right & source_mask, source_bits - 1) * 2;
	if (accumulate) {
		__int128 signed_accumulator =
			sign_extend64(accumulator & result_mask,
				      result_bits - 1);

		result = subtract ? signed_accumulator - result :
				    signed_accumulator + result;
	}
	minimum = -((__int128)1 << (result_bits - 1));
	maximum = ((__int128)1 << (result_bits - 1)) - 1;
	if (result < minimum) {
		*saturated = true;
		result = minimum;
	} else if (result > maximum) {
		*saturated = true;
		result = maximum;
	}
	return (u64)result & result_mask;
}

static u8 orlix_tcti_aes_rotate_left(u8 value, u8 amount)
{
	return (value << amount) | (value >> (8 - amount));
}

static u8 orlix_tcti_aes_gf_multiply(u8 left, u8 right)
{
	u8 result = 0;
	u8 bit;

	for (bit = 0; bit < 8; bit++) {
		bool high = left & BIT(7);

		if (right & BIT(0))
			result ^= left;
		left <<= 1;
		if (high)
			left ^= 0x1bU;
		right >>= 1;
	}
	return result;
}

static u8 orlix_tcti_aes_gf_inverse(u8 value)
{
	u8 result = 1;
	u8 base = value;
	u8 exponent = 254;

	if (!value)
		return 0;
	while (exponent) {
		if (exponent & 1U)
			result = orlix_tcti_aes_gf_multiply(result, base);
		base = orlix_tcti_aes_gf_multiply(base, base);
		exponent >>= 1;
	}
	return result;
}

static u8 orlix_tcti_aes_substitute(u8 value)
{
	u8 inverse = orlix_tcti_aes_gf_inverse(value);

	return inverse ^ orlix_tcti_aes_rotate_left(inverse, 1) ^
	       orlix_tcti_aes_rotate_left(inverse, 2) ^
	       orlix_tcti_aes_rotate_left(inverse, 3) ^
	       orlix_tcti_aes_rotate_left(inverse, 4) ^ 0x63U;
}

static u8 orlix_tcti_aes_inverse_substitute(u8 value)
{
	u8 inverse_affine = orlix_tcti_aes_rotate_left(value, 1) ^
				    orlix_tcti_aes_rotate_left(value, 3) ^
				    orlix_tcti_aes_rotate_left(value, 6) ^ 0x05U;

	return orlix_tcti_aes_gf_inverse(inverse_affine);
}

static void orlix_tcti_aes_shift_rows(u8 state[16], bool inverse)
{
	u8 source[16];
	u8 row;
	u8 column;

	memcpy(source, state, sizeof(source));
	for (row = 0; row < 4; row++) {
		for (column = 0; column < 4; column++) {
			u8 source_column = inverse ?
				(column + 4 - row) % 4 : (column + row) % 4;

			state[row + column * 4] =
				source[row + source_column * 4];
		}
	}
}

static void orlix_tcti_aes_mix_columns(u8 state[16], bool inverse)
{
	u8 column;

	for (column = 0; column < 4; column++) {
		u8 *value = &state[column * 4];
		u8 source[4] = { value[0], value[1], value[2], value[3] };

		if (inverse) {
			value[0] = orlix_tcti_aes_gf_multiply(source[0], 0x0e) ^
				orlix_tcti_aes_gf_multiply(source[1], 0x0b) ^
				orlix_tcti_aes_gf_multiply(source[2], 0x0d) ^
				orlix_tcti_aes_gf_multiply(source[3], 0x09);
			value[1] = orlix_tcti_aes_gf_multiply(source[0], 0x09) ^
				orlix_tcti_aes_gf_multiply(source[1], 0x0e) ^
				orlix_tcti_aes_gf_multiply(source[2], 0x0b) ^
				orlix_tcti_aes_gf_multiply(source[3], 0x0d);
			value[2] = orlix_tcti_aes_gf_multiply(source[0], 0x0d) ^
				orlix_tcti_aes_gf_multiply(source[1], 0x09) ^
				orlix_tcti_aes_gf_multiply(source[2], 0x0e) ^
				orlix_tcti_aes_gf_multiply(source[3], 0x0b);
			value[3] = orlix_tcti_aes_gf_multiply(source[0], 0x0b) ^
				orlix_tcti_aes_gf_multiply(source[1], 0x0d) ^
				orlix_tcti_aes_gf_multiply(source[2], 0x09) ^
				orlix_tcti_aes_gf_multiply(source[3], 0x0e);
		} else {
			value[0] = orlix_tcti_aes_gf_multiply(source[0], 2) ^
				orlix_tcti_aes_gf_multiply(source[1], 3) ^
				source[2] ^ source[3];
			value[1] = source[0] ^
				orlix_tcti_aes_gf_multiply(source[1], 2) ^
				orlix_tcti_aes_gf_multiply(source[2], 3) ^ source[3];
			value[2] = source[0] ^ source[1] ^
				orlix_tcti_aes_gf_multiply(source[2], 2) ^
				orlix_tcti_aes_gf_multiply(source[3], 3);
			value[3] = orlix_tcti_aes_gf_multiply(source[0], 3) ^
				source[1] ^ source[2] ^
				orlix_tcti_aes_gf_multiply(source[3], 2);
		}
	}
}

static u32 orlix_tcti_sha256_sigma0(u32 value)
{
	return ror32(value, 2) ^ ror32(value, 13) ^ ror32(value, 22);
}

static u32 orlix_tcti_sha256_sigma1(u32 value)
{
	return ror32(value, 6) ^ ror32(value, 11) ^ ror32(value, 25);
}

static u32 orlix_tcti_sha256_schedule_sigma0(u32 value)
{
	return ror32(value, 7) ^ ror32(value, 18) ^ (value >> 3);
}

static u32 orlix_tcti_sha256_schedule_sigma1(u32 value)
{
	return ror32(value, 17) ^ ror32(value, 19) ^ (value >> 10);
}

static u64 orlix_tcti_sha512_sigma0(u64 value)
{
	return ror64(value, 28) ^ ror64(value, 34) ^ ror64(value, 39);
}

static u64 orlix_tcti_sha512_sigma1(u64 value)
{
	return ror64(value, 14) ^ ror64(value, 18) ^ ror64(value, 41);
}

static u64 orlix_tcti_sha512_schedule_sigma0(u64 value)
{
	return ror64(value, 1) ^ ror64(value, 8) ^ (value >> 7);
}

static u64 orlix_tcti_sha512_schedule_sigma1(u64 value)
{
	return ror64(value, 19) ^ ror64(value, 61) ^ (value >> 6);
}

/* GB/T 32907-2016 S-box, matching upstream Linux crypto/sm4.c. */
static const u8 orlix_tcti_sm4_sbox[256] = {
	0xd6, 0x90, 0xe9, 0xfe, 0xcc, 0xe1, 0x3d, 0xb7,
	0x16, 0xb6, 0x14, 0xc2, 0x28, 0xfb, 0x2c, 0x05,
	0x2b, 0x67, 0x9a, 0x76, 0x2a, 0xbe, 0x04, 0xc3,
	0xaa, 0x44, 0x13, 0x26, 0x49, 0x86, 0x06, 0x99,
	0x9c, 0x42, 0x50, 0xf4, 0x91, 0xef, 0x98, 0x7a,
	0x33, 0x54, 0x0b, 0x43, 0xed, 0xcf, 0xac, 0x62,
	0xe4, 0xb3, 0x1c, 0xa9, 0xc9, 0x08, 0xe8, 0x95,
	0x80, 0xdf, 0x94, 0xfa, 0x75, 0x8f, 0x3f, 0xa6,
	0x47, 0x07, 0xa7, 0xfc, 0xf3, 0x73, 0x17, 0xba,
	0x83, 0x59, 0x3c, 0x19, 0xe6, 0x85, 0x4f, 0xa8,
	0x68, 0x6b, 0x81, 0xb2, 0x71, 0x64, 0xda, 0x8b,
	0xf8, 0xeb, 0x0f, 0x4b, 0x70, 0x56, 0x9d, 0x35,
	0x1e, 0x24, 0x0e, 0x5e, 0x63, 0x58, 0xd1, 0xa2,
	0x25, 0x22, 0x7c, 0x3b, 0x01, 0x21, 0x78, 0x87,
	0xd4, 0x00, 0x46, 0x57, 0x9f, 0xd3, 0x27, 0x52,
	0x4c, 0x36, 0x02, 0xe7, 0xa0, 0xc4, 0xc8, 0x9e,
	0xea, 0xbf, 0x8a, 0xd2, 0x40, 0xc7, 0x38, 0xb5,
	0xa3, 0xf7, 0xf2, 0xce, 0xf9, 0x61, 0x15, 0xa1,
	0xe0, 0xae, 0x5d, 0xa4, 0x9b, 0x34, 0x1a, 0x55,
	0xad, 0x93, 0x32, 0x30, 0xf5, 0x8c, 0xb1, 0xe3,
	0x1d, 0xf6, 0xe2, 0x2e, 0x82, 0x66, 0xca, 0x60,
	0xc0, 0x29, 0x23, 0xab, 0x0d, 0x53, 0x4e, 0x6f,
	0xd5, 0xdb, 0x37, 0x45, 0xde, 0xfd, 0x8e, 0x2f,
	0x03, 0xff, 0x6a, 0x72, 0x6d, 0x6c, 0x5b, 0x51,
	0x8d, 0x1b, 0xaf, 0x92, 0xbb, 0xdd, 0xbc, 0x7f,
	0x11, 0xd9, 0x5c, 0x41, 0x1f, 0x10, 0x5a, 0xd8,
	0x0a, 0xc1, 0x31, 0x88, 0xa5, 0xcd, 0x7b, 0xbd,
	0x2d, 0x74, 0xd0, 0x12, 0xb8, 0xe5, 0xb4, 0xb0,
	0x89, 0x69, 0x97, 0x4a, 0x0c, 0x96, 0x77, 0x7e,
	0x65, 0xb9, 0xf1, 0x09, 0xc5, 0x6e, 0xc6, 0x84,
	0x18, 0xf0, 0x7d, 0xec, 0x3a, 0xdc, 0x4d, 0x20,
	0x79, 0xee, 0x5f, 0x3e, 0xd7, 0xcb, 0x39, 0x48,
};

static u32 orlix_tcti_sm4_substitute(u32 value)
{
	return (u32)orlix_tcti_sm4_sbox[value & 0xffU] |
	       (u32)orlix_tcti_sm4_sbox[(value >> 8) & 0xffU] << 8 |
	       (u32)orlix_tcti_sm4_sbox[(value >> 16) & 0xffU] << 16 |
	       (u32)orlix_tcti_sm4_sbox[value >> 24] << 24;
}

static int orlix_tcti_execute_simd_vector_arithmetic(
	struct pt_regs *regs, const struct orlix_tcti_decoded_instruction *decoded)
{
	u64 left_low;
	u64 left_high;
	u64 right_low;
	u64 right_high;
	u8 lane;

	if (decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_EOR3 ||
	    decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_BCAX) {
		u64 result_low;
		u64 result_high;

		if (decoded->access_size != 2 * sizeof(u64) ||
		    decoded->result_size != 2 * sizeof(u64) ||
		    decoded->simd_scalar)
			return -EOPNOTSUPP;
		if (decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_EOR3) {
			result_low = current->thread.user_simd[decoded->rn * 2] ^
				current->thread.user_simd[decoded->rm * 2] ^
				current->thread.user_simd[decoded->ra * 2];
			result_high = current->thread.user_simd[decoded->rn * 2 + 1] ^
				current->thread.user_simd[decoded->rm * 2 + 1] ^
				current->thread.user_simd[decoded->ra * 2 + 1];
		} else {
			result_low = current->thread.user_simd[decoded->rn * 2] ^
				(current->thread.user_simd[decoded->rm * 2] &
				 ~current->thread.user_simd[decoded->ra * 2]);
			result_high = current->thread.user_simd[decoded->rn * 2 + 1] ^
				(current->thread.user_simd[decoded->rm * 2 + 1] &
				 ~current->thread.user_simd[decoded->ra * 2 + 1]);
		}
		orlix_tcti_write_simd_fp_register(decoded->rd, 2 * sizeof(u64),
					    result_low, result_high);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_RAX1 ||
	    decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_XAR) {
		u64 result_low;
		u64 result_high;

		if (decoded->access_size != 2 * sizeof(u64) ||
		    decoded->result_size != 2 * sizeof(u64) ||
		    decoded->simd_scalar)
			return -EOPNOTSUPP;
		if (decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_RAX1) {
			result_low = current->thread.user_simd[decoded->rn * 2] ^
				ror64(current->thread.user_simd[decoded->rm * 2], 1);
			result_high = current->thread.user_simd[decoded->rn * 2 + 1] ^
				ror64(current->thread.user_simd[decoded->rm * 2 + 1], 1);
		} else {
			result_low = ror64(
				current->thread.user_simd[decoded->rn * 2] ^
				current->thread.user_simd[decoded->rm * 2],
				decoded->shift_amount);
			result_high = ror64(
				current->thread.user_simd[decoded->rn * 2 + 1] ^
				current->thread.user_simd[decoded->rm * 2 + 1],
				decoded->shift_amount);
		}
		orlix_tcti_write_simd_fp_register(decoded->rd, 2 * sizeof(u64),
					    result_low, result_high);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op >= ORLIX_TCTI_SIMD_ARITH_AESE &&
	    decoded->simd_arithmetic_op <= ORLIX_TCTI_SIMD_ARITH_AESIMC) {
		u8 state[16];
		u8 source[16];
		u8 index;

		if (decoded->access_size != 2 * sizeof(u64) ||
		    decoded->result_size != 2 * sizeof(u64))
			return -EOPNOTSUPP;

		put_unaligned_le64(current->thread.user_simd[decoded->rd * 2],
				   state);
		put_unaligned_le64(current->thread.user_simd[decoded->rd * 2 + 1],
				   state + sizeof(u64));
		put_unaligned_le64(current->thread.user_simd[decoded->rn * 2],
				   source);
		put_unaligned_le64(current->thread.user_simd[decoded->rn * 2 + 1],
				   source + sizeof(u64));

		switch (decoded->simd_arithmetic_op) {
		case ORLIX_TCTI_SIMD_ARITH_AESE:
			for (index = 0; index < sizeof(state); index++)
				state[index] =
					orlix_tcti_aes_substitute(state[index] ^ source[index]);
			orlix_tcti_aes_shift_rows(state, false);
			break;
		case ORLIX_TCTI_SIMD_ARITH_AESD:
			for (index = 0; index < sizeof(state); index++)
				state[index] = orlix_tcti_aes_inverse_substitute(
					state[index] ^ source[index]);
			orlix_tcti_aes_shift_rows(state, true);
			break;
		case ORLIX_TCTI_SIMD_ARITH_AESMC:
			memcpy(state, source, sizeof(state));
			orlix_tcti_aes_mix_columns(state, false);
			break;
		case ORLIX_TCTI_SIMD_ARITH_AESIMC:
			memcpy(state, source, sizeof(state));
			orlix_tcti_aes_mix_columns(state, true);
			break;
		default:
			return -EOPNOTSUPP;
		}

		orlix_tcti_write_simd_fp_register(decoded->rd, 2 * sizeof(u64),
					    get_unaligned_le64(state),
					    get_unaligned_le64(
						    state + sizeof(u64)));
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op >= ORLIX_TCTI_SIMD_ARITH_SHA1C &&
	    decoded->simd_arithmetic_op <= ORLIX_TCTI_SIMD_ARITH_SHA1SU1) {
		u64 destination[2] = {
			current->thread.user_simd[decoded->rd * 2],
			current->thread.user_simd[decoded->rd * 2 + 1],
		};
		u64 source_n[2] = {
			current->thread.user_simd[decoded->rn * 2],
			current->thread.user_simd[decoded->rn * 2 + 1],
		};
		u32 d[4] = {
			(u32)destination[0], destination[0] >> 32,
			(u32)destination[1], destination[1] >> 32,
		};
		u32 n[4] = {
			(u32)source_n[0], source_n[0] >> 32,
			(u32)source_n[1], source_n[1] >> 32,
		};
		u32 result[4] = {};

		if (decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SHA1H) {
			if (decoded->access_size != sizeof(u32) ||
			    decoded->result_size != sizeof(u32) ||
			    !decoded->simd_scalar)
				return -EOPNOTSUPP;
			orlix_tcti_write_simd_fp_register(decoded->rd, sizeof(u32),
						    ror32(n[0], 2), 0);
		} else if (decoded->simd_arithmetic_op ==
			   ORLIX_TCTI_SIMD_ARITH_SHA1SU1) {
			u32 temporary[4] = {
				d[0] ^ n[1], d[1] ^ n[2], d[2] ^ n[3], d[3],
			};

			if (decoded->access_size != 2 * sizeof(u64) ||
			    decoded->result_size != 2 * sizeof(u64) ||
			    decoded->simd_scalar)
				return -EOPNOTSUPP;
			result[0] = rol32(temporary[0], 1);
			result[1] = rol32(temporary[1], 1);
			result[2] = rol32(temporary[2], 1);
			result[3] = rol32(temporary[3], 1) ^
				    rol32(temporary[0], 2);
			orlix_tcti_write_simd_fp_register(
				decoded->rd, 2 * sizeof(u64),
				(u64)result[0] | (u64)result[1] << 32,
				(u64)result[2] | (u64)result[3] << 32);
		} else {
			u64 source_m[2] = {
				current->thread.user_simd[decoded->rm * 2],
				current->thread.user_simd[decoded->rm * 2 + 1],
			};
			u32 m[4] = {
				(u32)source_m[0], source_m[0] >> 32,
				(u32)source_m[1], source_m[1] >> 32,
			};

			if (decoded->access_size != 2 * sizeof(u64) ||
			    decoded->result_size != 2 * sizeof(u64) ||
			    decoded->simd_scalar)
				return -EOPNOTSUPP;
			if (decoded->simd_arithmetic_op ==
			    ORLIX_TCTI_SIMD_ARITH_SHA1SU0) {
				result[0] = d[0] ^ d[2] ^ m[0];
				result[1] = d[1] ^ d[3] ^ m[1];
				result[2] = n[0] ^ d[2] ^ m[2];
				result[3] = n[1] ^ d[3] ^ m[3];
			} else {
				u32 a = d[0];
				u32 b = d[1];
				u32 c = d[2];
				u32 value_d = d[3];
				u32 e = n[0];
				u8 round;

				for (round = 0; round < 4; round++) {
					u32 function;
					u32 temporary;

					if (decoded->simd_arithmetic_op ==
					    ORLIX_TCTI_SIMD_ARITH_SHA1C)
						function = (b & c) | (~b & value_d);
					else if (decoded->simd_arithmetic_op ==
						 ORLIX_TCTI_SIMD_ARITH_SHA1M)
						function = (b & c) | (b & value_d) |
							   (c & value_d);
					else
						function = b ^ c ^ value_d;
					temporary = rol32(a, 5) + function + e +
						    m[round];
					e = value_d;
					value_d = c;
					c = ror32(b, 2);
					b = a;
					a = temporary;
				}
				result[0] = a;
				result[1] = b;
				result[2] = c;
				result[3] = value_d;
			}
			orlix_tcti_write_simd_fp_register(
				decoded->rd, 2 * sizeof(u64),
				(u64)result[0] | (u64)result[1] << 32,
				(u64)result[2] | (u64)result[3] << 32);
		}

		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op >= ORLIX_TCTI_SIMD_ARITH_SHA256H &&
	    decoded->simd_arithmetic_op <= ORLIX_TCTI_SIMD_ARITH_SHA256SU0) {
		u64 destination[2] = {
			current->thread.user_simd[decoded->rd * 2],
			current->thread.user_simd[decoded->rd * 2 + 1],
		};
		u64 source_n[2] = {
			current->thread.user_simd[decoded->rn * 2],
			current->thread.user_simd[decoded->rn * 2 + 1],
		};
		u32 d[4] = {
			(u32)destination[0], destination[0] >> 32,
			(u32)destination[1], destination[1] >> 32,
		};
		u32 n[4] = {
			(u32)source_n[0], source_n[0] >> 32,
			(u32)source_n[1], source_n[1] >> 32,
		};
		u32 result[4];

		if (decoded->access_size != 2 * sizeof(u64) ||
		    decoded->result_size != 2 * sizeof(u64) ||
		    decoded->simd_scalar)
			return -EOPNOTSUPP;

		if (decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SHA256SU0) {
			result[0] = d[0] + orlix_tcti_sha256_schedule_sigma0(d[1]);
			result[1] = d[1] + orlix_tcti_sha256_schedule_sigma0(d[2]);
			result[2] = d[2] + orlix_tcti_sha256_schedule_sigma0(d[3]);
			result[3] = d[3] + orlix_tcti_sha256_schedule_sigma0(n[0]);
		} else {
			u64 source_m[2] = {
				current->thread.user_simd[decoded->rm * 2],
				current->thread.user_simd[decoded->rm * 2 + 1],
			};
			u32 m[4] = {
				(u32)source_m[0], source_m[0] >> 32,
				(u32)source_m[1], source_m[1] >> 32,
			};

			if (decoded->simd_arithmetic_op ==
			    ORLIX_TCTI_SIMD_ARITH_SHA256SU1) {
				result[0] = d[0] +
					orlix_tcti_sha256_schedule_sigma1(m[2]) + n[1];
				result[1] = d[1] +
					orlix_tcti_sha256_schedule_sigma1(m[3]) + n[2];
				result[2] = d[2] +
					orlix_tcti_sha256_schedule_sigma1(result[0]) + n[3];
				result[3] = d[3] +
					orlix_tcti_sha256_schedule_sigma1(result[1]) + m[0];
			} else {
				u32 a;
				u32 b;
				u32 c;
				u32 value_d;
				u32 e;
				u32 f;
				u32 g;
				u32 h;
				u8 round;

				if (decoded->simd_arithmetic_op ==
				    ORLIX_TCTI_SIMD_ARITH_SHA256H) {
					a = d[0];
					b = d[1];
					c = d[2];
					value_d = d[3];
					e = n[0];
					f = n[1];
					g = n[2];
					h = n[3];
				} else {
					a = n[0];
					b = n[1];
					c = n[2];
					value_d = n[3];
					e = d[0];
					f = d[1];
					g = d[2];
					h = d[3];
				}
				for (round = 0; round < 4; round++) {
					u32 temporary1 = h + orlix_tcti_sha256_sigma1(e) +
						((e & f) ^ (~e & g)) + m[round];
					u32 temporary2 = orlix_tcti_sha256_sigma0(a) +
						((a & b) ^ (a & c) ^ (b & c));

					h = g;
					g = f;
					f = e;
					e = value_d + temporary1;
					value_d = c;
					c = b;
					b = a;
					a = temporary1 + temporary2;
				}
				if (decoded->simd_arithmetic_op ==
				    ORLIX_TCTI_SIMD_ARITH_SHA256H) {
					result[0] = a;
					result[1] = b;
					result[2] = c;
					result[3] = value_d;
				} else {
					result[0] = e;
					result[1] = f;
					result[2] = g;
					result[3] = h;
				}
			}
		}

		orlix_tcti_write_simd_fp_register(
			decoded->rd, 2 * sizeof(u64),
			(u64)result[0] | (u64)result[1] << 32,
			(u64)result[2] | (u64)result[3] << 32);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op >= ORLIX_TCTI_SIMD_ARITH_SHA512H &&
	    decoded->simd_arithmetic_op <= ORLIX_TCTI_SIMD_ARITH_SHA512SU0) {
		u64 d[2] = {
			current->thread.user_simd[decoded->rd * 2],
			current->thread.user_simd[decoded->rd * 2 + 1],
		};
		u64 n[2] = {
			current->thread.user_simd[decoded->rn * 2],
			current->thread.user_simd[decoded->rn * 2 + 1],
		};
		u64 result[2];

		if (decoded->access_size != 2 * sizeof(u64) ||
		    decoded->result_size != 2 * sizeof(u64) ||
		    decoded->simd_scalar)
			return -EOPNOTSUPP;

		if (decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SHA512SU0) {
			result[0] = d[0] + orlix_tcti_sha512_schedule_sigma0(d[1]);
			result[1] = d[1] + orlix_tcti_sha512_schedule_sigma0(n[0]);
		} else {
			u64 m[2] = {
				current->thread.user_simd[decoded->rm * 2],
				current->thread.user_simd[decoded->rm * 2 + 1],
			};

			if (decoded->simd_arithmetic_op ==
			    ORLIX_TCTI_SIMD_ARITH_SHA512SU1) {
				result[0] = d[0] +
					orlix_tcti_sha512_schedule_sigma1(n[0]) + m[0];
				result[1] = d[1] +
					orlix_tcti_sha512_schedule_sigma1(n[1]) + m[1];
			} else if (decoded->simd_arithmetic_op ==
				   ORLIX_TCTI_SIMD_ARITH_SHA512H) {
				u64 temporary_high =
					((m[1] & n[0]) ^ (~m[1] & n[1])) +
					orlix_tcti_sha512_sigma1(m[1]) + d[1];
				u64 temporary = temporary_high + m[0];

				result[0] =
					((temporary & m[1]) ^ (~temporary & n[0])) +
					orlix_tcti_sha512_sigma1(temporary) + d[0];
				result[1] = temporary_high;
			} else {
				result[1] =
					((n[0] & m[1]) ^ (n[0] & m[0]) ^
					 (m[1] & m[0])) +
					orlix_tcti_sha512_sigma0(m[0]) + d[1];
				result[0] =
					((result[1] & m[0]) ^
					 (result[1] & m[1]) ^ (m[0] & m[1])) +
					orlix_tcti_sha512_sigma0(result[1]) + d[0];
			}
		}

		orlix_tcti_write_simd_fp_register(decoded->rd, 2 * sizeof(u64),
					    result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SM4E ||
	    decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SM4EKEY) {
		u64 source_n[2] = {
			current->thread.user_simd[decoded->rn * 2],
			current->thread.user_simd[decoded->rn * 2 + 1],
		};
		u32 n[4] = {
			(u32)source_n[0], source_n[0] >> 32,
			(u32)source_n[1], source_n[1] >> 32,
		};
		u32 state[4];
		u32 round_key[4];
		u8 round;

		if (decoded->access_size != 2 * sizeof(u64) ||
		    decoded->result_size != 2 * sizeof(u64) ||
		    decoded->simd_scalar)
			return -EOPNOTSUPP;

		if (decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SM4E) {
			u64 destination[2] = {
				current->thread.user_simd[decoded->rd * 2],
				current->thread.user_simd[decoded->rd * 2 + 1],
			};

			state[0] = destination[0];
			state[1] = destination[0] >> 32;
			state[2] = destination[1];
			state[3] = destination[1] >> 32;
			memcpy(round_key, n, sizeof(round_key));
		} else {
			u64 source_m[2] = {
				current->thread.user_simd[decoded->rm * 2],
				current->thread.user_simd[decoded->rm * 2 + 1],
			};

			memcpy(state, n, sizeof(state));
			round_key[0] = source_m[0];
			round_key[1] = source_m[0] >> 32;
			round_key[2] = source_m[1];
			round_key[3] = source_m[1] >> 32;
		}

		for (round = 0; round < 4; round++) {
			u32 transformed = orlix_tcti_sm4_substitute(
				state[1] ^ state[2] ^ state[3] ^ round_key[round]);
			u32 next;

			if (decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SM4E)
				transformed ^= rol32(transformed, 2) ^
					       rol32(transformed, 10) ^
					       rol32(transformed, 18) ^
					       rol32(transformed, 24);
			else
				transformed ^= rol32(transformed, 13) ^
					       rol32(transformed, 23);
			next = state[0] ^ transformed;
			state[0] = state[1];
			state[1] = state[2];
			state[2] = state[3];
			state[3] = next;
		}

		orlix_tcti_write_simd_fp_register(
			decoded->rd, 2 * sizeof(u64),
			(u64)state[0] | (u64)state[1] << 32,
			(u64)state[2] | (u64)state[3] << 32);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op >= ORLIX_TCTI_SIMD_ARITH_SM3SS1 &&
	    decoded->simd_arithmetic_op <= ORLIX_TCTI_SIMD_ARITH_SM3PARTW2) {
		u64 source_n[2] = {
			current->thread.user_simd[decoded->rn * 2],
			current->thread.user_simd[decoded->rn * 2 + 1],
		};
		u64 source_m[2] = {
			current->thread.user_simd[decoded->rm * 2],
			current->thread.user_simd[decoded->rm * 2 + 1],
		};
		u32 n[4] = {
			(u32)source_n[0], source_n[0] >> 32,
			(u32)source_n[1], source_n[1] >> 32,
		};
		u32 m[4] = {
			(u32)source_m[0], source_m[0] >> 32,
			(u32)source_m[1], source_m[1] >> 32,
		};
		u32 result[4] = {};

		if (decoded->access_size != 2 * sizeof(u64) ||
		    decoded->result_size != 2 * sizeof(u64) ||
		    decoded->simd_scalar)
			return -EOPNOTSUPP;

		if (decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SM3SS1) {
			u64 source_a[2] = {
				current->thread.user_simd[decoded->ra * 2],
				current->thread.user_simd[decoded->ra * 2 + 1],
			};

			result[3] = rol32(rol32(n[3], 12) + m[3] +
					  (u32)(source_a[1] >> 32), 7);
		} else {
			u64 destination[2] = {
				current->thread.user_simd[decoded->rd * 2],
				current->thread.user_simd[decoded->rd * 2 + 1],
			};
			u32 d[4] = {
				(u32)destination[0], destination[0] >> 32,
				(u32)destination[1], destination[1] >> 32,
			};

			if (decoded->simd_arithmetic_op >=
				    ORLIX_TCTI_SIMD_ARITH_SM3TT1A &&
			    decoded->simd_arithmetic_op <=
				    ORLIX_TCTI_SIMD_ARITH_SM3TT2B) {
				u8 operation = decoded->simd_arithmetic_op -
					       ORLIX_TCTI_SIMD_ARITH_SM3TT1A;
				u32 temporary;

				if (decoded->shift_amount > 3)
					return -EOPNOTSUPP;
				if (operation == 0 || operation == 2)
					temporary = d[3] ^ d[2] ^ d[1];
				else if (operation == 1)
					temporary = (d[3] & d[2]) ^
						    (d[3] & d[1]) ^ (d[2] & d[1]);
				else
					temporary = (d[3] & d[2]) ^ (~d[3] & d[1]);
				temporary += d[0] + m[decoded->shift_amount];
				result[0] = d[1];
				if (operation < 2) {
					temporary += n[3] ^ rol32(d[3], 12);
					result[1] = rol32(d[2], 9);
				} else {
					temporary += n[3];
					temporary ^= rol32(temporary, 9) ^
						     rol32(temporary, 17);
					result[1] = rol32(d[2], 19);
				}
				result[2] = d[3];
				result[3] = temporary;
			} else if (decoded->simd_arithmetic_op ==
				   ORLIX_TCTI_SIMD_ARITH_SM3PARTW1) {
				u32 temporary = d[0] ^ n[0] ^ rol32(m[1], 15);

				result[0] = temporary ^ rol32(temporary, 15) ^
					    rol32(temporary, 23);
				temporary = d[1] ^ n[1] ^ rol32(m[2], 15);
				result[1] = temporary ^ rol32(temporary, 15) ^
					    rol32(temporary, 23);
				temporary = d[2] ^ n[2] ^ rol32(m[3], 15);
				result[2] = temporary ^ rol32(temporary, 15) ^
					    rol32(temporary, 23);
				temporary = d[3] ^ n[3] ^ rol32(result[0], 15);
				result[3] = temporary ^ rol32(temporary, 15) ^
					    rol32(temporary, 23);
			} else {
				u32 temporary = n[0] ^ rol32(m[0], 7);

				result[0] = d[0] ^ temporary;
				result[1] = d[1] ^ n[1] ^ rol32(m[1], 7);
				result[2] = d[2] ^ n[2] ^ rol32(m[2], 7);
				result[3] = d[3] ^ n[3] ^ rol32(m[3], 7) ^
					    rol32(temporary, 15) ^
					    rol32(temporary, 30) ^
					    rol32(temporary, 6);
			}
		}

		orlix_tcti_write_simd_fp_register(
			decoded->rd, 2 * sizeof(u64),
			(u64)result[0] | (u64)result[1] << 32,
			(u64)result[2] | (u64)result[3] << 32);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->immediate &&
	    (decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SQSHL ||
	     decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_UQSHL ||
	     decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SQSHLU)) {
		u64 source[2];
		u64 result[2] = {};
		u8 lane_count;
		u8 lane_bits;
		u64 mask;
		bool saturated = false;

		if ((decoded->access_size != sizeof(u8) &&
		     decoded->access_size != sizeof(u16) &&
		     decoded->access_size != sizeof(u32) &&
		     decoded->access_size != sizeof(u64)) ||
		    (decoded->simd_scalar &&
		     decoded->result_size != decoded->access_size) ||
		    (!decoded->simd_scalar &&
		     decoded->result_size != sizeof(u64) &&
		     decoded->result_size != 2 * sizeof(u64)))
			return -EOPNOTSUPP;

		lane_bits = decoded->access_size * 8;
		if (decoded->shift_amount >= lane_bits)
			return -EOPNOTSUPP;
		source[0] = current->thread.user_simd[decoded->rn * 2];
		source[1] = current->thread.user_simd[decoded->rn * 2 + 1];
		lane_count = decoded->simd_scalar ? 1 :
			decoded->result_size / decoded->access_size;
		mask = GENMASK_ULL(lane_bits - 1, 0);
		for (lane = 0; lane < lane_count; lane++) {
			u8 byte = lane * decoded->access_size;
			u8 word = byte / sizeof(u64);
			u8 shift = (byte % sizeof(u64)) * 8;
			u64 value = (source[word] >> shift) & mask;
			u64 lane_result;

			if (decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_UQSHL) {
				if (value > (mask >> decoded->shift_amount)) {
					lane_result = mask;
					saturated = true;
				} else {
					lane_result = value << decoded->shift_amount;
				}
			} else {
				s64 signed_value = sign_extend64(value, lane_bits - 1);

				if (decoded->simd_arithmetic_op ==
				    ORLIX_TCTI_SIMD_ARITH_SQSHLU) {
					if (signed_value < 0) {
						lane_result = 0;
						saturated = true;
					} else if ((u64)signed_value >
						   (mask >> decoded->shift_amount)) {
						lane_result = mask;
						saturated = true;
					} else {
						lane_result = (u64)signed_value <<
							decoded->shift_amount;
					}
				} else {
					s64 minimum = lane_bits == 64 ? S64_MIN :
						-(1LL << (lane_bits - 1));
					s64 maximum = lane_bits == 64 ? S64_MAX :
						(1LL << (lane_bits - 1)) - 1;

					if (signed_value >
					    (maximum >> decoded->shift_amount)) {
						lane_result = (u64)maximum & mask;
						saturated = true;
					} else if (signed_value <
						   (minimum >> decoded->shift_amount)) {
						lane_result = (u64)minimum & mask;
						saturated = true;
					} else {
						lane_result = (u64)signed_value <<
							decoded->shift_amount;
					}
				}
			}
			result[word] |= (lane_result & mask) << shift;
		}

		if (saturated)
			current->thread.user_fpsr |= AARCH64_FPSR_QC;
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SHL ||
	    decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SLI ||
	    decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SRI) {
		u64 source[2];
		u64 destination[2];
		u64 result[2] = {};
		u8 lane_count;
		u8 lane_bits;
		u64 mask;
		bool right_insert =
			decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SRI;

		if ((decoded->access_size != sizeof(u8) &&
		     decoded->access_size != sizeof(u16) &&
		     decoded->access_size != sizeof(u32) &&
		     decoded->access_size != sizeof(u64)) ||
		    (decoded->result_size != sizeof(u64) &&
		     decoded->result_size != 2 * sizeof(u64)) ||
		    (decoded->simd_scalar &&
		     (decoded->access_size != sizeof(u64) ||
		      decoded->result_size != sizeof(u64))) ||
		    (!decoded->simd_scalar && decoded->access_size == sizeof(u64) &&
		     decoded->result_size != 2 * sizeof(u64)))
			return -EOPNOTSUPP;

		lane_bits = decoded->access_size * 8;
		if ((!right_insert && decoded->shift_amount >= lane_bits) ||
		    (right_insert && (decoded->shift_amount == 0 ||
				     decoded->shift_amount > lane_bits)))
			return -EOPNOTSUPP;

		source[0] = current->thread.user_simd[decoded->rn * 2];
		source[1] = current->thread.user_simd[decoded->rn * 2 + 1];
		destination[0] = current->thread.user_simd[decoded->rd * 2];
		destination[1] = current->thread.user_simd[decoded->rd * 2 + 1];
		lane_count = decoded->result_size / decoded->access_size;
		mask = GENMASK_ULL(lane_bits - 1, 0);
		for (lane = 0; lane < lane_count; lane++) {
			u8 byte = lane * decoded->access_size;
			u8 word = byte / sizeof(u64);
			u8 shift = (byte % sizeof(u64)) * 8;
			u64 value = (source[word] >> shift) & mask;
			u64 original = (destination[word] >> shift) & mask;
			u64 lane_result;

			if (right_insert) {
				u64 preserved = decoded->shift_amount == lane_bits ?
					mask : mask & ~GENMASK_ULL(
						lane_bits - decoded->shift_amount - 1, 0);
				u64 shifted = decoded->shift_amount == lane_bits ? 0 :
					value >> decoded->shift_amount;

				lane_result = (original & preserved) | shifted;
			} else {
				u64 preserved = decoded->shift_amount == 0 ? 0 :
					GENMASK_ULL(decoded->shift_amount - 1, 0);
				u64 shifted = value << decoded->shift_amount;

				lane_result = shifted;
				if (decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SLI)
					lane_result |= original & preserved;
			}
			result[word] |= (lane_result & mask) << shift;
		}

		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SSHR ||
	    decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_USHR ||
	    decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SSRA ||
	    decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_USRA ||
	    decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SRSHR ||
	    decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_URSHR ||
	    decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SRSRA ||
	    decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_URSRA) {
		u64 source[2];
		u64 accumulator[2];
		u64 result[2] = {};
		u8 lane_count;
		u8 lane_bits;
		u64 mask;
		bool signed_shift =
			decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SSHR ||
			decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SSRA ||
			decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SRSHR ||
			decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SRSRA;
		bool rounding =
			decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SRSHR ||
			decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_URSHR ||
			decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SRSRA ||
			decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_URSRA;
		bool accumulate =
			decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SSRA ||
			decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_USRA ||
			decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SRSRA ||
			decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_URSRA;

		if ((decoded->access_size != sizeof(u8) &&
		     decoded->access_size != sizeof(u16) &&
		     decoded->access_size != sizeof(u32) &&
		     decoded->access_size != sizeof(u64)) ||
		    (decoded->result_size != sizeof(u64) &&
		     decoded->result_size != 2 * sizeof(u64)) ||
		    decoded->shift_amount == 0 ||
		    decoded->shift_amount > decoded->access_size * 8 ||
		    (decoded->simd_scalar &&
		     (decoded->access_size != sizeof(u64) ||
		      decoded->result_size != sizeof(u64))) ||
		    (!decoded->simd_scalar && decoded->access_size == sizeof(u64) &&
		     decoded->result_size != 2 * sizeof(u64)))
			return -EOPNOTSUPP;

		source[0] = current->thread.user_simd[decoded->rn * 2];
		source[1] = current->thread.user_simd[decoded->rn * 2 + 1];
		accumulator[0] = current->thread.user_simd[decoded->rd * 2];
		accumulator[1] = current->thread.user_simd[decoded->rd * 2 + 1];
		lane_bits = decoded->access_size * 8;
		lane_count = decoded->result_size / decoded->access_size;
		mask = GENMASK_ULL(lane_bits - 1, 0);
		for (lane = 0; lane < lane_count; lane++) {
			u8 byte = lane * decoded->access_size;
			u8 word = byte / sizeof(u64);
			u8 shift = (byte % sizeof(u64)) * 8;
			u64 value = (source[word] >> shift) & mask;
			u64 shifted;

			if (signed_shift) {
				s64 signed_value = sign_extend64(value, lane_bits - 1);
				s64 signed_result = decoded->shift_amount == lane_bits ?
					(signed_value < 0 ? -1 : 0) :
					signed_value >> decoded->shift_amount;

				if (rounding)
					signed_result +=
						(value >> (decoded->shift_amount - 1)) & 1U;
				shifted = (u64)signed_result & mask;
			} else {
				shifted = decoded->shift_amount == lane_bits ? 0 :
					value >> decoded->shift_amount;
				if (rounding)
					shifted +=
						(value >> (decoded->shift_amount - 1)) & 1U;
			}

			if (accumulate)
				shifted += (accumulator[word] >> shift) & mask;
			result[word] |= (shifted & mask) << shift;
		}

		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op >= ORLIX_TCTI_SIMD_ARITH_SABDL &&
	    decoded->simd_arithmetic_op <= ORLIX_TCTI_SIMD_ARITH_UABAL) {
		u64 accumulator[2];
		u64 left[2];
		u64 right[2];
		u64 result[2] = {};
		u8 wide_size = decoded->access_size * 2;
		u8 lane_count;
		u8 operation = decoded->simd_arithmetic_op - ORLIX_TCTI_SIMD_ARITH_SABDL;
		bool is_unsigned = operation & 1U;
		bool accumulate = operation & 2U;
		u64 narrow_mask;
		u64 wide_mask;

		if ((decoded->access_size != sizeof(u8) &&
		     decoded->access_size != sizeof(u16) &&
		     decoded->access_size != sizeof(u32)) ||
		    decoded->result_size != 2 * sizeof(u64) ||
		    (decoded->simd_source_index != 0 &&
		     decoded->simd_source_index !=
			sizeof(u64) / decoded->access_size))
			return -EOPNOTSUPP;

		accumulator[0] = current->thread.user_simd[decoded->rd * 2];
		accumulator[1] = current->thread.user_simd[decoded->rd * 2 + 1];
		left[0] = current->thread.user_simd[decoded->rn * 2];
		left[1] = current->thread.user_simd[decoded->rn * 2 + 1];
		right[0] = current->thread.user_simd[decoded->rm * 2];
		right[1] = current->thread.user_simd[decoded->rm * 2 + 1];
		lane_count = decoded->result_size / wide_size;
		narrow_mask = GENMASK_ULL(decoded->access_size * 8 - 1, 0);
		wide_mask = GENMASK_ULL(wide_size * 8 - 1, 0);
		for (lane = 0; lane < lane_count; lane++) {
			u8 source_lane = decoded->simd_source_index + lane;
			u8 source_byte = source_lane * decoded->access_size;
			u8 source_word = source_byte / sizeof(u64);
			u8 source_shift = (source_byte % sizeof(u64)) * 8;
			u8 result_byte = lane * wide_size;
			u8 result_word = result_byte / sizeof(u64);
			u8 result_shift = (result_byte % sizeof(u64)) * 8;
			u64 left_lane = (left[source_word] >> source_shift) &
				narrow_mask;
			u64 right_lane = (right[source_word] >> source_shift) &
				narrow_mask;
			u64 difference;

			if (is_unsigned) {
				difference = left_lane >= right_lane ?
					left_lane - right_lane : right_lane - left_lane;
			} else {
				s64 signed_left = sign_extend64(
					left_lane, decoded->access_size * 8 - 1);
				s64 signed_right = sign_extend64(
					right_lane, decoded->access_size * 8 - 1);

				difference = signed_left >= signed_right ?
					(u64)(signed_left - signed_right) :
					(u64)(signed_right - signed_left);
			}

			if (accumulate) {
				u64 accumulator_lane =
					(accumulator[result_word] >> result_shift) &
					wide_mask;

				difference += accumulator_lane;
			}
			result[result_word] |=
				(difference & wide_mask) << result_shift;
		}

		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op >= ORLIX_TCTI_SIMD_ARITH_ADDHN &&
	    decoded->simd_arithmetic_op <= ORLIX_TCTI_SIMD_ARITH_RSUBHN) {
		u64 left[2];
		u64 right[2];
		u64 narrowed = 0;
		u8 narrow_size = decoded->access_size / 2;
		u8 lane_count;
		u8 operation = decoded->simd_arithmetic_op - ORLIX_TCTI_SIMD_ARITH_ADDHN;
		bool subtract = operation & 1U;
		bool rounding = operation & 2U;
		u64 wide_mask;
		u64 narrow_mask;

		if ((decoded->access_size != sizeof(u16) &&
		     decoded->access_size != sizeof(u32) &&
		     decoded->access_size != sizeof(u64)) ||
		    (decoded->result_size != sizeof(u64) &&
		     decoded->result_size != 2 * sizeof(u64)) ||
		    decoded->simd_destination_index !=
			(decoded->result_size == 2 * sizeof(u64)))
			return -EOPNOTSUPP;

		left[0] = current->thread.user_simd[decoded->rn * 2];
		left[1] = current->thread.user_simd[decoded->rn * 2 + 1];
		right[0] = current->thread.user_simd[decoded->rm * 2];
		right[1] = current->thread.user_simd[decoded->rm * 2 + 1];
		wide_mask = GENMASK_ULL(decoded->access_size * 8 - 1, 0);
		narrow_mask = GENMASK_ULL(narrow_size * 8 - 1, 0);
		lane_count = 2 * sizeof(u64) / decoded->access_size;
		for (lane = 0; lane < lane_count; lane++) {
			u8 source_byte = lane * decoded->access_size;
			u8 source_word = source_byte / sizeof(u64);
			u8 source_shift = (source_byte % sizeof(u64)) * 8;
			u64 left_lane = (left[source_word] >> source_shift) &
				wide_mask;
			u64 right_lane = (right[source_word] >> source_shift) &
				wide_mask;
			u64 result = subtract ? left_lane - right_lane :
				left_lane + right_lane;

			if (rounding)
				result += BIT_ULL(narrow_size * 8 - 1);
			result = (result >> (narrow_size * 8)) & narrow_mask;
			narrowed |= result << (lane * narrow_size * 8);
		}

		if (decoded->simd_destination_index)
			orlix_tcti_write_simd_fp_register(
				decoded->rd, 2 * sizeof(u64),
				current->thread.user_simd[decoded->rd * 2], narrowed);
		else
			orlix_tcti_write_simd_fp_register(decoded->rd, sizeof(u64),
						    narrowed, 0);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op >= ORLIX_TCTI_SIMD_ARITH_SQXTN &&
	    decoded->simd_arithmetic_op <= ORLIX_TCTI_SIMD_ARITH_SQXTUN) {
		u64 source[2];
		u64 narrowed = 0;
		u8 narrow_size = decoded->access_size / 2;
		u8 lane_count;
		u8 narrow_bits;
		u64 source_mask;
		u64 result_mask;
		bool saturated = false;

		if ((decoded->access_size != sizeof(u16) &&
		     decoded->access_size != sizeof(u32) &&
		     decoded->access_size != sizeof(u64)) ||
		    (decoded->simd_scalar &&
		     (decoded->result_size != narrow_size ||
		      decoded->simd_destination_index)) ||
		    (!decoded->simd_scalar &&
		     decoded->result_size != sizeof(u64) &&
		     decoded->result_size != 2 * sizeof(u64)) ||
		    (!decoded->simd_scalar && decoded->simd_destination_index !=
			(decoded->result_size == 2 * sizeof(u64))))
			return -EOPNOTSUPP;

		source[0] = current->thread.user_simd[decoded->rn * 2];
		source[1] = current->thread.user_simd[decoded->rn * 2 + 1];
		lane_count = decoded->simd_scalar ? 1 :
			2 * sizeof(u64) / decoded->access_size;
		narrow_bits = narrow_size * 8;
		source_mask = GENMASK_ULL(decoded->access_size * 8 - 1, 0);
		result_mask = GENMASK_ULL(narrow_bits - 1, 0);
		for (lane = 0; lane < lane_count; lane++) {
			u8 source_byte = lane * decoded->access_size;
			u8 source_word = source_byte / sizeof(u64);
			u8 source_shift = (source_byte % sizeof(u64)) * 8;
			u64 value = (source[source_word] >> source_shift) & source_mask;
			u64 result;

			if (decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_UQXTN) {
				if (value > result_mask) {
					result = result_mask;
					saturated = true;
				} else {
					result = value;
				}
			} else {
				s64 signed_value = sign_extend64(
					value, decoded->access_size * 8 - 1);
				s64 minimum = decoded->simd_arithmetic_op ==
					ORLIX_TCTI_SIMD_ARITH_SQXTN ?
					-(s64)BIT_ULL(narrow_bits - 1) : 0;
				s64 maximum = decoded->simd_arithmetic_op ==
					ORLIX_TCTI_SIMD_ARITH_SQXTN ?
					BIT_ULL(narrow_bits - 1) - 1 : result_mask;

				if (signed_value < minimum) {
					result = (u64)minimum & result_mask;
					saturated = true;
				} else if (signed_value > maximum) {
					result = (u64)maximum;
					saturated = true;
				} else {
					result = (u64)signed_value & result_mask;
				}
			}

			narrowed |= result << (lane * narrow_bits);
		}

		if (saturated)
			current->thread.user_fpsr |= AARCH64_FPSR_QC;
		if (decoded->simd_scalar)
			orlix_tcti_write_simd_fp_register(decoded->rd,
				decoded->result_size, narrowed, 0);
		else if (decoded->simd_destination_index)
			orlix_tcti_write_simd_fp_register(
				decoded->rd, 2 * sizeof(u64),
				current->thread.user_simd[decoded->rd * 2], narrowed);
		else
			orlix_tcti_write_simd_fp_register(decoded->rd, sizeof(u64),
						    narrowed, 0);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op >= ORLIX_TCTI_SIMD_ARITH_SHRN &&
	    decoded->simd_arithmetic_op <= ORLIX_TCTI_SIMD_ARITH_SQRSHRUN) {
		u64 source[2];
		u64 narrowed = 0;
		u8 narrow_size = decoded->access_size / 2;
		u8 lane_count;
		u8 narrow_bits;
		u64 source_mask;
		u64 result_mask;
		bool signed_source =
			decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SQSHRN ||
			decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SQRSHRN ||
			decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SQSHRUN ||
			decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SQRSHRUN;
		bool unsigned_result =
			decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_UQSHRN ||
			decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_UQRSHRN ||
			decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SQSHRUN ||
			decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SQRSHRUN;
		bool rounding =
			decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_RSHRN ||
			decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SQRSHRN ||
			decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_UQRSHRN ||
			decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SQRSHRUN;
		bool saturating =
			decoded->simd_arithmetic_op != ORLIX_TCTI_SIMD_ARITH_SHRN &&
			decoded->simd_arithmetic_op != ORLIX_TCTI_SIMD_ARITH_RSHRN;
		bool saturated = false;

		if ((decoded->access_size != sizeof(u16) &&
		     decoded->access_size != sizeof(u32) &&
		     decoded->access_size != sizeof(u64)) ||
		    (decoded->simd_scalar &&
		     (decoded->result_size != narrow_size ||
		      decoded->simd_destination_index)) ||
		    (!decoded->simd_scalar &&
		     ((decoded->result_size != sizeof(u64) &&
		       decoded->result_size != 2 * sizeof(u64)) ||
		      decoded->simd_destination_index !=
			(decoded->result_size == 2 * sizeof(u64)))) ||
		    decoded->shift_amount == 0 ||
		    decoded->shift_amount > decoded->access_size * 4)
			return -EOPNOTSUPP;

		source[0] = current->thread.user_simd[decoded->rn * 2];
		source[1] = current->thread.user_simd[decoded->rn * 2 + 1];
		lane_count = decoded->simd_scalar ? 1 :
					       2 * sizeof(u64) / decoded->access_size;
		narrow_bits = narrow_size * 8;
		source_mask = GENMASK_ULL(decoded->access_size * 8 - 1, 0);
		result_mask = GENMASK_ULL(narrow_bits - 1, 0);
		for (lane = 0; lane < lane_count; lane++) {
			u8 source_byte = lane * decoded->access_size;
			u8 source_word = source_byte / sizeof(u64);
			u8 source_shift = (source_byte % sizeof(u64)) * 8;
			u64 value = (source[source_word] >> source_shift) & source_mask;
			u64 result;

			if (signed_source) {
				s64 shifted = sign_extend64(
					value, decoded->access_size * 8 - 1) >>
					decoded->shift_amount;

				if (rounding)
					shifted += (value >>
						    (decoded->shift_amount - 1)) & 1U;
				if (unsigned_result) {
					if (shifted < 0) {
						result = 0;
						saturated = true;
					} else if ((u64)shifted > result_mask) {
						result = result_mask;
						saturated = true;
					} else {
						result = shifted;
					}
				} else {
					s64 minimum = -(s64)BIT_ULL(narrow_bits - 1);
					s64 maximum = BIT_ULL(narrow_bits - 1) - 1;

					if (shifted < minimum) {
						result = (u64)minimum & result_mask;
						saturated = true;
					} else if (shifted > maximum) {
						result = maximum;
						saturated = true;
					} else {
						result = (u64)shifted & result_mask;
					}
				}
			} else {
				u64 shifted = value >> decoded->shift_amount;

				if (rounding)
					shifted += (value >>
						    (decoded->shift_amount - 1)) & 1U;
				if (saturating && shifted > result_mask) {
					result = result_mask;
					saturated = true;
				} else {
					result = shifted & result_mask;
				}
			}

			narrowed |= result << (lane * narrow_bits);
		}

		if (saturated)
			current->thread.user_fpsr |= AARCH64_FPSR_QC;
		if (decoded->simd_scalar)
			orlix_tcti_write_simd_fp_register(decoded->rd,
						    decoded->result_size,
						    narrowed, 0);
		else if (decoded->simd_destination_index)
			orlix_tcti_write_simd_fp_register(
				decoded->rd, 2 * sizeof(u64),
				current->thread.user_simd[decoded->rd * 2], narrowed);
		else
			orlix_tcti_write_simd_fp_register(decoded->rd, sizeof(u64),
						    narrowed, 0);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SUQADD ||
	    decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_USQADD) {
		u64 destination[2];
		u64 source[2];
		u64 result[2] = {};
		u8 lane_count;
		bool unsigned_destination = decoded->simd_arithmetic_op ==
					    ORLIX_TCTI_SIMD_ARITH_USQADD;
		bool saturated = false;

		if ((decoded->simd_scalar &&
		     decoded->result_size != decoded->access_size) ||
		    (!decoded->simd_scalar &&
		     decoded->result_size != sizeof(u64) &&
		     decoded->result_size != 2 * sizeof(u64)) ||
		    decoded->access_size > decoded->result_size)
			return -EOPNOTSUPP;
		destination[0] = current->thread.user_simd[decoded->rd * 2];
		destination[1] = current->thread.user_simd[decoded->rd * 2 + 1];
		source[0] = current->thread.user_simd[decoded->rn * 2];
		source[1] = current->thread.user_simd[decoded->rn * 2 + 1];
		lane_count = decoded->simd_scalar ?
			     1 : decoded->result_size / decoded->access_size;
		for (lane = 0; lane < lane_count; lane++) {
			u8 byte = lane * decoded->access_size;
			u8 word = byte / sizeof(u64);
			u8 shift = (byte % sizeof(u64)) * 8;
			u64 mask = GENMASK_ULL(decoded->access_size * 8 - 1, 0);
			u64 destination_lane = (destination[word] >> shift) & mask;
			u64 source_lane = (source[word] >> shift) & mask;
			u64 lane_result = orlix_tcti_simd_mixed_saturating_add_lane(
				destination_lane, source_lane,
				decoded->access_size * 8, unsigned_destination,
				&saturated);

			result[word] |= lane_result << shift;
		}
		if (saturated)
			current->thread.user_fpsr |= AARCH64_FPSR_QC;
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}
	if ((!decoded->simd_scalar &&
	     (decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_FRECPE ||
	      decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_FRSQRTE ||
	      decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_FCVTXN)) ||
	    decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_FNEG ||
	    (decoded->simd_arithmetic_op >= ORLIX_TCTI_SIMD_ARITH_FABS &&
	     decoded->simd_arithmetic_op <= ORLIX_TCTI_SIMD_ARITH_URSQRTE)) {
		u64 source[2];
		u64 accumulator[2];
		u64 result[2] = {};
		int ret;

		source[0] = current->thread.user_simd[decoded->rn * 2];
		source[1] = current->thread.user_simd[decoded->rn * 2 + 1];
		accumulator[0] = current->thread.user_simd[decoded->rd * 2];
		accumulator[1] = current->thread.user_simd[decoded->rd * 2 + 1];
		if (decoded->simd_scalar)
			ret = orlix_tcti_native_simd_fp_scalar_unary(
				decoded->simd_arithmetic_op, decoded->access_size,
				decoded->result_size, result, source,
				current->thread.user_fpcr,
				&current->thread.user_fpsr);
		else
			ret = orlix_tcti_native_simd_fp_two_register(
				decoded->simd_arithmetic_op, decoded->access_size,
				decoded->result_size, decoded->simd_source_index,
				decoded->simd_destination_index, result, source,
				accumulator, current->thread.user_fpcr,
				&current->thread.user_fpsr);
		if (ret)
			return ret;
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
			result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_FRECPE ||
	    decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_FRECPX ||
	    decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_FRSQRTE ||
	    decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_FCVTXN) {
		u64 source[2];
		u64 result[2] = {};
		int ret;

		source[0] = current->thread.user_simd[decoded->rn * 2];
		source[1] = current->thread.user_simd[decoded->rn * 2 + 1];
		ret = orlix_tcti_native_simd_fp_scalar_unary(
			decoded->simd_arithmetic_op, decoded->access_size,
			decoded->result_size, result, source,
			current->thread.user_fpcr, &current->thread.user_fpsr);
		if (ret)
			return ret;
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}
	if (decoded->simd_arithmetic_op >= ORLIX_TCTI_SIMD_ARITH_FABD &&
	    decoded->simd_arithmetic_op <= ORLIX_TCTI_SIMD_ARITH_FSUB) {
		u64 left[2];
		u64 right[2];
		u64 accumulator[2];
		u64 result[2] = {};
		int ret;

		left[0] = current->thread.user_simd[decoded->rn * 2];
		left[1] = current->thread.user_simd[decoded->rn * 2 + 1];
		if (decoded->simd_indexed) {
			ret = orlix_tcti_simd_indexed_operand(decoded, right);
			if (ret)
				return ret;
		} else {
			right[0] = current->thread.user_simd[decoded->rm * 2];
			right[1] = current->thread.user_simd[decoded->rm * 2 + 1];
		}
		accumulator[0] = current->thread.user_simd[decoded->rd * 2];
		accumulator[1] = current->thread.user_simd[decoded->rd * 2 + 1];
		if (decoded->access_size == sizeof(u16))
			ret = orlix_tcti_native_simd_fp16_three_same(
				decoded->simd_arithmetic_op, decoded->simd_scalar,
				decoded->simd_q, result, left, right, accumulator,
				current->thread.user_fpcr, &current->thread.user_fpsr);
		else
			ret = orlix_tcti_native_simd_fp_three_same(
				decoded->simd_arithmetic_op, decoded->simd_scalar,
				decoded->access_size, decoded->result_size, result, left,
				right, accumulator, current->thread.user_fpcr,
				&current->thread.user_fpsr);
		if (ret)
			return ret;
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}
	if (decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SADDLP ||
	    decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_UADDLP ||
	    decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SADALP ||
	    decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_UADALP) {
		u64 source[2];
		u64 accumulator[2];
		u64 result[2] = {};
		u8 result_access_size = 2 * decoded->access_size;
		u8 lane_count;
		bool is_unsigned = decoded->simd_arithmetic_op ==
				   ORLIX_TCTI_SIMD_ARITH_UADDLP ||
				   decoded->simd_arithmetic_op ==
				   ORLIX_TCTI_SIMD_ARITH_UADALP;
		bool accumulate = decoded->simd_arithmetic_op ==
				  ORLIX_TCTI_SIMD_ARITH_SADALP ||
				  decoded->simd_arithmetic_op ==
				  ORLIX_TCTI_SIMD_ARITH_UADALP;

		if (decoded->access_size > sizeof(u32) ||
		    (decoded->result_size != sizeof(u64) &&
		     decoded->result_size != 2 * sizeof(u64)))
			return -EOPNOTSUPP;
		source[0] = current->thread.user_simd[decoded->rn * 2];
		source[1] = current->thread.user_simd[decoded->rn * 2 + 1];
		accumulator[0] = current->thread.user_simd[decoded->rd * 2];
		accumulator[1] = current->thread.user_simd[decoded->rd * 2 + 1];
		lane_count = decoded->result_size / result_access_size;
		for (lane = 0; lane < lane_count; lane++) {
			u8 source_byte = 2 * lane * decoded->access_size;
			u8 source_word = source_byte / sizeof(u64);
			u8 source_shift = (source_byte % sizeof(u64)) * 8;
			u8 destination_byte = lane * result_access_size;
			u8 destination_word = destination_byte / sizeof(u64);
			u8 destination_shift =
				(destination_byte % sizeof(u64)) * 8;
			u64 source_mask = orlix_tcti_simd_lane_mask(decoded->access_size);
			u64 result_mask = orlix_tcti_simd_lane_mask(result_access_size);
			u64 left = (source[source_word] >> source_shift) & source_mask;
			u64 right = (source[source_word] >>
				     (source_shift + decoded->access_size * 8)) &
				source_mask;
			u64 lane_result;

			if (is_unsigned)
				lane_result = left + right;
			else
				lane_result =
					(s64)sign_extend64(left,
							  decoded->access_size * 8 - 1) +
					(s64)sign_extend64(right,
							  decoded->access_size * 8 - 1);
			if (accumulate)
				lane_result += (accumulator[destination_word] >>
						destination_shift) & result_mask;
			result[destination_word] |=
				(lane_result & result_mask) << destination_shift;
		}
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op >= ORLIX_TCTI_SIMD_ARITH_ABS &&
	    decoded->simd_arithmetic_op <= ORLIX_TCTI_SIMD_ARITH_RBIT) {
		u64 source[2];
		u64 result[2] = {};
		u8 operation = decoded->simd_arithmetic_op - ORLIX_TCTI_SIMD_ARITH_ABS;
		bool saturated = false;

		if ((decoded->simd_scalar &&
		     decoded->result_size != decoded->access_size) ||
		    (!decoded->simd_scalar &&
		     decoded->result_size != sizeof(u64) &&
		     decoded->result_size != 2 * sizeof(u64)))
			return -EOPNOTSUPP;

		source[0] = current->thread.user_simd[decoded->rn * 2];
		source[1] = current->thread.user_simd[decoded->rn * 2 + 1];
		if (operation >= ORLIX_TCTI_SIMD_ARITH_REV64 - ORLIX_TCTI_SIMD_ARITH_ABS) {
			u8 byte;

			for (byte = 0; byte < decoded->result_size; byte++) {
				u8 source_word = byte / sizeof(u64);
				u8 source_shift = (byte % sizeof(u64)) * 8;
				u8 value = source[source_word] >> source_shift;
				u8 destination_byte = byte;

				switch (decoded->simd_arithmetic_op) {
				case ORLIX_TCTI_SIMD_ARITH_REV64:
				case ORLIX_TCTI_SIMD_ARITH_REV32: {
					u8 group_size = decoded->simd_arithmetic_op ==
						ORLIX_TCTI_SIMD_ARITH_REV64 ? 8 : 4;
					u8 group_offset = byte & (group_size - 1);
					u8 element_offset = group_offset %
						decoded->access_size;

					destination_byte = (byte & ~(group_size - 1)) +
						group_size - decoded->access_size -
						(group_offset / decoded->access_size) *
						decoded->access_size + element_offset;
					break;
				}
				case ORLIX_TCTI_SIMD_ARITH_REV16:
					destination_byte = byte ^ 1U;
					break;
				case ORLIX_TCTI_SIMD_ARITH_CNT:
					value = hweight8(value);
					break;
				case ORLIX_TCTI_SIMD_ARITH_NOT:
					value = ~value;
					break;
				default: {
					u8 reversed = 0;
					u8 bit;

					for (bit = 0; bit < 8; bit++)
						reversed |= ((value >> bit) & 1U) <<
							(7U - bit);
					value = reversed;
					break;
				}
				}

				result[destination_byte / sizeof(u64)] |=
					(u64)value <<
					((destination_byte % sizeof(u64)) * 8);
			}
		} else {
			u8 lane_count;
			u8 bits;
			u64 mask;

			if (decoded->access_size != sizeof(u8) &&
			    decoded->access_size != sizeof(u16) &&
			    decoded->access_size != sizeof(u32) &&
			    decoded->access_size != sizeof(u64))
				return -EOPNOTSUPP;

			bits = decoded->access_size * 8;
			mask = GENMASK_ULL(bits - 1, 0);
			lane_count = decoded->simd_scalar ? 1 :
				decoded->result_size / decoded->access_size;
			for (lane = 0; lane < lane_count; lane++) {
				u8 byte = lane * decoded->access_size;
				u8 word = byte / sizeof(u64);
				u8 shift = (byte % sizeof(u64)) * 8;
				u64 value = (source[word] >> shift) & mask;
				u64 lane_result = 0;

				switch (decoded->simd_arithmetic_op) {
				case ORLIX_TCTI_SIMD_ARITH_ABS:
					lane_result = value & BIT_ULL(bits - 1) ?
						(-value & mask) : value;
					break;
				case ORLIX_TCTI_SIMD_ARITH_NEG:
					lane_result = -value & mask;
					break;
				case ORLIX_TCTI_SIMD_ARITH_SQABS:
					if (value == BIT_ULL(bits - 1)) {
						lane_result = BIT_ULL(bits - 1) - 1;
						saturated = true;
					} else {
						lane_result = value & BIT_ULL(bits - 1) ?
							(-value & mask) : value;
					}
					break;
				case ORLIX_TCTI_SIMD_ARITH_SQNEG:
					if (value == BIT_ULL(bits - 1)) {
						lane_result = BIT_ULL(bits - 1) - 1;
						saturated = true;
					} else {
						lane_result = -value & mask;
					}
					break;
				case ORLIX_TCTI_SIMD_ARITH_CLS: {
					bool sign = value & BIT_ULL(bits - 1);
					int bit;

					for (bit = bits - 2; bit >= 0; bit--) {
						if (!!(value & BIT_ULL(bit)) != sign)
							break;
						lane_result++;
					}
					break;
				}
				default: {
					int bit;

					for (bit = bits - 1; bit >= 0; bit--) {
						if (value & BIT_ULL(bit))
							break;
						lane_result++;
					}
					break;
				}
				}

				result[word] |= (lane_result & mask) << shift;
			}
		}

		if (saturated)
			current->thread.user_fpsr |= AARCH64_FPSR_QC;
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op >= ORLIX_TCTI_SIMD_ARITH_SSHL &&
	    decoded->simd_arithmetic_op <= ORLIX_TCTI_SIMD_ARITH_UQRSHL) {
		u64 source[2];
		u64 shifts[2];
		u64 result[2] = {};
		u8 lane_count;
		bool saturated = false;
		u8 operation = decoded->simd_arithmetic_op -
			       ORLIX_TCTI_SIMD_ARITH_SSHL;
		bool is_unsigned = operation & 1U;
		bool saturating = (operation / 2) & 1U;
		bool rounding = operation >= 4;

		if ((decoded->access_size != sizeof(u8) &&
		     decoded->access_size != sizeof(u16) &&
		     decoded->access_size != sizeof(u32) &&
		     decoded->access_size != sizeof(u64)) ||
		    (decoded->simd_scalar &&
		     decoded->result_size != decoded->access_size) ||
		    (!decoded->simd_scalar &&
		     decoded->result_size != sizeof(u64) &&
		     decoded->result_size != 2 * sizeof(u64)) ||
		    (!decoded->simd_scalar &&
		     decoded->access_size > decoded->result_size))
			return -EOPNOTSUPP;

		source[0] = current->thread.user_simd[decoded->rn * 2];
		source[1] = current->thread.user_simd[decoded->rn * 2 + 1];
		shifts[0] = current->thread.user_simd[decoded->rm * 2];
		shifts[1] = current->thread.user_simd[decoded->rm * 2 + 1];
		lane_count = decoded->simd_scalar ? 1 :
			decoded->result_size / decoded->access_size;
		for (lane = 0; lane < lane_count; lane++) {
			u8 byte = lane * decoded->access_size;
			u8 word = byte / sizeof(u64);
			u8 bit_shift = (byte % sizeof(u64)) * 8;
			u64 mask = GENMASK_ULL(decoded->access_size * 8 - 1, 0);
			u64 value = (source[word] >> bit_shift) & mask;
			s8 shift = (s8)((shifts[word] >> bit_shift) & 0xffU);
			u64 lane_result = orlix_tcti_simd_shift_lane(
				value, decoded->access_size * 8, shift,
				is_unsigned, rounding, saturating, &saturated);

			result[word] |= lane_result << bit_shift;
		}

		if (saturated)
			current->thread.user_fpsr |= AARCH64_FPSR_QC;
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op >= ORLIX_TCTI_SIMD_ARITH_SHADD &&
	    decoded->simd_arithmetic_op <= ORLIX_TCTI_SIMD_ARITH_URHADD) {
		u64 left[2];
		u64 right[2];
		u64 result[2] = {};
		u64 mask;
		u8 lane_count;
		u8 operation = decoded->simd_arithmetic_op -
			ORLIX_TCTI_SIMD_ARITH_SHADD;
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
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
			result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SHSUB ||
	    decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_UHSUB) {
		u64 left[2];
		u64 right[2];
		u64 result[2] = {};
		u64 mask;
		u8 lane_count;
		bool is_unsigned = decoded->simd_arithmetic_op ==
			ORLIX_TCTI_SIMD_ARITH_UHSUB;

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
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
			result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_ADDP) {
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
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
			result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op >= ORLIX_TCTI_SIMD_ARITH_SADDW &&
	    decoded->simd_arithmetic_op <= ORLIX_TCTI_SIMD_ARITH_USUBW) {
		u64 wide[2];
		u64 narrow[2];
		u64 result[2] = {};
		u8 wide_size = decoded->access_size * 2;
		u8 lane_count;
		u8 operation = decoded->simd_arithmetic_op -
			ORLIX_TCTI_SIMD_ARITH_SADDW;
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
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
			result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op >= ORLIX_TCTI_SIMD_ARITH_SADDL &&
	    decoded->simd_arithmetic_op <= ORLIX_TCTI_SIMD_ARITH_USUBL) {
		u64 left[2];
		u64 right[2];
		u64 result[2] = {};
		u8 wide_size = decoded->access_size * 2;
		u8 lane_count;
		u8 operation = decoded->simd_arithmetic_op -
			ORLIX_TCTI_SIMD_ARITH_SADDL;
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
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
			result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op >= ORLIX_TCTI_SIMD_ARITH_SMAXP &&
	    decoded->simd_arithmetic_op <= ORLIX_TCTI_SIMD_ARITH_UMINP) {
		u64 left[2];
		u64 right[2];
		u64 result[2] = {};
		u64 mask;
		u8 lane_count;
		u8 pairs_per_source;
		u8 operation = decoded->simd_arithmetic_op -
			ORLIX_TCTI_SIMD_ARITH_SMAXP;
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
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
			result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op >= ORLIX_TCTI_SIMD_ARITH_SQADD &&
	    decoded->simd_arithmetic_op <= ORLIX_TCTI_SIMD_ARITH_UQSUB) {
		u64 left[2];
		u64 right[2];
		u64 result[2] = {};
		u8 lane_count;
		u8 operation = decoded->simd_arithmetic_op -
			ORLIX_TCTI_SIMD_ARITH_SQADD;
		bool is_unsigned = operation & 1U;
		bool subtract = operation & 2U;
		bool saturated = false;

		if ((decoded->access_size != sizeof(u8) &&
		     decoded->access_size != sizeof(u16) &&
		     decoded->access_size != sizeof(u32) &&
		     decoded->access_size != sizeof(u64)) ||
		    (decoded->simd_scalar &&
		     decoded->result_size != decoded->access_size) ||
		    (!decoded->simd_scalar &&
		     decoded->result_size != sizeof(u64) &&
		     decoded->result_size != 2 * sizeof(u64)) ||
		    decoded->access_size > decoded->result_size)
			return -EOPNOTSUPP;

		left[0] = current->thread.user_simd[decoded->rn * 2];
		left[1] = current->thread.user_simd[decoded->rn * 2 + 1];
		right[0] = current->thread.user_simd[decoded->rm * 2];
		right[1] = current->thread.user_simd[decoded->rm * 2 + 1];
		lane_count = decoded->simd_scalar ? 1 :
			decoded->result_size / decoded->access_size;
		for (lane = 0; lane < lane_count; lane++) {
			u8 byte = lane * decoded->access_size;
			u8 word = byte / sizeof(u64);
			u8 shift = (byte % sizeof(u64)) * 8;
			u64 mask = GENMASK_ULL(decoded->access_size * 8 - 1, 0);
			u64 left_lane = (left[word] >> shift) & mask;
			u64 right_lane = (right[word] >> shift) & mask;
			u64 lane_result = orlix_tcti_simd_saturating_add_sub_lane(
				left_lane, right_lane, decoded->access_size * 8,
				is_unsigned, subtract, &saturated);

			result[word] |= lane_result << shift;
		}
		if (saturated)
			current->thread.user_fpsr |= AARCH64_FPSR_QC;
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
			result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SQDMULH ||
	    decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SQRDMULH) {
		u64 left[2];
		u64 right[2];
		u64 result[2] = {};
		u64 mask;
		u8 lane_count;
		bool rounding = decoded->simd_arithmetic_op ==
			ORLIX_TCTI_SIMD_ARITH_SQRDMULH;
		bool saturated = false;
		int ret;

		if ((decoded->access_size != sizeof(u16) &&
		     decoded->access_size != sizeof(u32)) ||
		    (decoded->simd_scalar &&
		     decoded->result_size != decoded->access_size) ||
		    (!decoded->simd_scalar &&
		     decoded->result_size != sizeof(u64) &&
		     decoded->result_size != 2 * sizeof(u64)) ||
		    decoded->access_size > decoded->result_size)
			return -EOPNOTSUPP;

		left[0] = current->thread.user_simd[decoded->rn * 2];
		left[1] = current->thread.user_simd[decoded->rn * 2 + 1];
		if (decoded->simd_indexed) {
			ret = orlix_tcti_simd_indexed_operand(decoded, right);
			if (ret)
				return ret;
		} else {
			right[0] = current->thread.user_simd[decoded->rm * 2];
			right[1] = current->thread.user_simd[decoded->rm * 2 + 1];
		}
		mask = GENMASK_ULL(decoded->access_size * 8 - 1, 0);
		lane_count = decoded->simd_scalar ? 1 :
			decoded->result_size / decoded->access_size;
		for (lane = 0; lane < lane_count; lane++) {
			u8 byte = lane * decoded->access_size;
			u8 word = byte / sizeof(u64);
			u8 shift = (byte % sizeof(u64)) * 8;
			u64 left_lane = (left[word] >> shift) & mask;
			u64 right_lane = (right[word] >> shift) & mask;
			u64 lane_result = orlix_tcti_simd_saturating_mul_high_lane(
				left_lane, right_lane, decoded->access_size * 8,
				rounding, &saturated);

			result[word] |= lane_result << shift;
		}
		if (saturated)
			current->thread.user_fpsr |= AARCH64_FPSR_QC;
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
			result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op >= ORLIX_TCTI_SIMD_ARITH_SQDMULL &&
	    decoded->simd_arithmetic_op <= ORLIX_TCTI_SIMD_ARITH_SQDMLSL) {
		u64 left;
		u64 right;
		u64 accumulator[2];
		u64 result[2] = {};
		u8 result_size = decoded->access_size * 2;
		u8 lane_count;
		u8 lane;
		u64 source_mask;
		u64 result_mask;
		bool accumulate;
		bool subtract;
		bool saturated = false;
		int ret;

		if ((decoded->access_size != sizeof(u16) &&
		     decoded->access_size != sizeof(u32)) ||
		    (decoded->simd_scalar &&
		     decoded->result_size != result_size) ||
		    (!decoded->simd_scalar &&
		     (decoded->result_size != 2 * sizeof(u64) ||
		      (!decoded->simd_indexed &&
		       decoded->simd_source_index > 1))))
			return -EOPNOTSUPP;

		if (decoded->simd_indexed) {
			u64 indexed[2];

			ret = orlix_tcti_simd_indexed_operand(decoded, indexed);
			if (ret)
				return ret;
			left = current->thread.user_simd[decoded->rn * 2 +
				(decoded->simd_scalar ? 0 : !!decoded->simd_q)];
			right = indexed[0];
		} else {
			left = current->thread.user_simd[
				decoded->rn * 2 +
				(decoded->simd_scalar ? 0 :
				 decoded->simd_source_index)];
			right = current->thread.user_simd[
				decoded->rm * 2 +
				(decoded->simd_scalar ? 0 :
				 decoded->simd_source_index)];
		}
		accumulator[0] = current->thread.user_simd[decoded->rd * 2];
		accumulator[1] = current->thread.user_simd[decoded->rd * 2 + 1];
		accumulate = decoded->simd_arithmetic_op !=
			     ORLIX_TCTI_SIMD_ARITH_SQDMULL;
		subtract = decoded->simd_arithmetic_op ==
			   ORLIX_TCTI_SIMD_ARITH_SQDMLSL;
		lane_count = decoded->simd_scalar ? 1 :
			     2 * sizeof(u64) / result_size;
		source_mask = GENMASK_ULL(decoded->access_size * 8 - 1, 0);
		result_mask = GENMASK_ULL(result_size * 8 - 1, 0);
		for (lane = 0; lane < lane_count; lane++) {
			u8 source_shift = lane * decoded->access_size * 8;
			u8 result_byte = lane * result_size;
			u8 result_word = result_byte / sizeof(u64);
			u8 result_shift = (result_byte % sizeof(u64)) * 8;
			u64 left_lane = (left >> source_shift) & source_mask;
			u64 right_lane = (right >> source_shift) & source_mask;
			u64 accumulator_lane =
				(accumulator[result_word] >> result_shift) &
				result_mask;
			u64 lane_result;

			lane_result = orlix_tcti_simd_saturating_double_mul_long_lane(
				left_lane, right_lane, accumulator_lane,
				decoded->access_size * 8, accumulate, subtract,
				&saturated);
			result[result_word] |= lane_result << result_shift;
		}
		if (saturated)
			current->thread.user_fpsr |= AARCH64_FPSR_QC;
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op >= ORLIX_TCTI_SIMD_ARITH_SMULL &&
	    decoded->simd_arithmetic_op <= ORLIX_TCTI_SIMD_ARITH_UMLSL) {
		u64 left;
		u64 right;
		u64 result[2] = {};
		u8 result_size = decoded->access_size * 2;
		u8 lane_count;
		u8 lane;
		u64 source_mask;
		u64 result_mask;
		bool signed_multiply =
			decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SMULL ||
			decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SMLAL ||
			decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SMLSL;
		bool accumulate =
			decoded->simd_arithmetic_op != ORLIX_TCTI_SIMD_ARITH_SMULL &&
			decoded->simd_arithmetic_op != ORLIX_TCTI_SIMD_ARITH_UMULL;
		bool subtract =
			decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SMLSL ||
			decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_UMLSL;
		int ret;

		if ((decoded->access_size != sizeof(u8) &&
		     decoded->access_size != sizeof(u16) &&
		     decoded->access_size != sizeof(u32)) ||
		    decoded->result_size != 2 * sizeof(u64) ||
		    (!decoded->simd_indexed && decoded->simd_source_index > 1))
			return -EOPNOTSUPP;

		if (decoded->simd_indexed) {
			u64 indexed[2];

			ret = orlix_tcti_simd_indexed_operand(decoded, indexed);
			if (ret)
				return ret;
			left = current->thread.user_simd[decoded->rn * 2 +
				!!decoded->simd_q];
			right = indexed[0];
		} else {
			left = current->thread.user_simd[decoded->rn * 2 +
				decoded->simd_source_index];
			right = current->thread.user_simd[decoded->rm * 2 +
				decoded->simd_source_index];
		}
		lane_count = 2 * sizeof(u64) / result_size;
		source_mask = GENMASK_ULL(decoded->access_size * 8 - 1, 0);
		result_mask = GENMASK_ULL(result_size * 8 - 1, 0);
		for (lane = 0; lane < lane_count; lane++) {
			u8 source_shift = lane * decoded->access_size * 8;
			u8 result_byte = lane * result_size;
			u8 result_word = result_byte / sizeof(u64);
			u8 result_shift = (result_byte % sizeof(u64)) * 8;
			u64 left_lane = (left >> source_shift) & source_mask;
			u64 right_lane = (right >> source_shift) & source_mask;
			u64 product;
			u64 lane_result;

			if (signed_multiply)
				product = (u64)(sign_extend64(
					left_lane,
					decoded->access_size * 8 - 1) *
					sign_extend64(
					right_lane,
					decoded->access_size * 8 - 1));
			else
				product = left_lane * right_lane;

			if (accumulate) {
				u64 accumulator =
					(current->thread.user_simd[
						decoded->rd * 2 + result_word] >>
					 result_shift) & result_mask;

				lane_result = subtract ? accumulator - product :
							 accumulator + product;
			} else {
				lane_result = product;
			}
			result[result_word] |=
				(lane_result & result_mask) << result_shift;
		}

		orlix_tcti_write_simd_fp_register(decoded->rd, 2 * sizeof(u64),
					    result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_PMULL) {
		u64 left;
		u64 right;
		u64 result[2] = {};

		if ((decoded->access_size != sizeof(u8) &&
		     decoded->access_size != sizeof(u64)) ||
		    decoded->result_size != 2 * sizeof(u64) ||
		    decoded->simd_source_index > 1)
			return -EOPNOTSUPP;

		left = current->thread.user_simd[decoded->rn * 2 +
						 decoded->simd_source_index];
		right = current->thread.user_simd[decoded->rm * 2 +
						  decoded->simd_source_index];
		if (decoded->access_size == sizeof(u8)) {
			u8 lane;

			for (lane = 0; lane < 8; lane++) {
				u16 left_lane = (left >> (lane * 8)) & 0xffU;
				u16 right_lane = (right >> (lane * 8)) & 0xffU;
				u16 lane_result = 0;
				u8 bit;

				for (bit = 0; bit < 8; bit++) {
					if (right_lane & BIT(bit))
						lane_result ^= left_lane << bit;
				}
				result[lane / 4] |=
					(u64)lane_result << ((lane % 4) * 16);
			}
		} else {
			u8 bit;

			for (bit = 0; bit < 64; bit++) {
				if (!(right & BIT_ULL(bit)))
					continue;
				result[0] ^= left << bit;
				if (bit)
					result[1] ^= left >> (64 - bit);
			}
		}

		orlix_tcti_write_simd_fp_register(decoded->rd, 2 * sizeof(u64),
					    result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_MUL ||
	    decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_PMUL) {
		u64 left[2];
		u64 right[2];
		u64 result[2] = {};
		u64 mask;
		u8 lane_count;
		bool polynomial = decoded->simd_arithmetic_op ==
			ORLIX_TCTI_SIMD_ARITH_PMUL;
		int ret;

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
		if (decoded->simd_indexed) {
			ret = orlix_tcti_simd_indexed_operand(decoded, right);
			if (ret)
				return ret;
		} else {
			right[0] = current->thread.user_simd[decoded->rm * 2];
			right[1] = current->thread.user_simd[decoded->rm * 2 + 1];
		}
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
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
			result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_MLA ||
	    decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_MLS) {
		u64 accumulator[2];
		u64 left[2];
		u64 right[2];
		u64 result[2] = {};
		u64 mask;
		u8 lane_count;
		bool subtract = decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_MLS;
		int ret;

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
		if (decoded->simd_indexed) {
			ret = orlix_tcti_simd_indexed_operand(decoded, right);
			if (ret)
				return ret;
		} else {
			right[0] = current->thread.user_simd[decoded->rm * 2];
			right[1] = current->thread.user_simd[decoded->rm * 2 + 1];
		}
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
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
			result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op >= ORLIX_TCTI_SIMD_ARITH_SMAX &&
	    decoded->simd_arithmetic_op <= ORLIX_TCTI_SIMD_ARITH_UMIN) {
		u64 left[2];
		u64 right[2];
		u64 result[2] = {};
		u64 mask;
		u8 lane_count;
		u8 operation = decoded->simd_arithmetic_op - ORLIX_TCTI_SIMD_ARITH_SMAX;
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
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
			result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_arithmetic_op >= ORLIX_TCTI_SIMD_ARITH_SABD &&
	    decoded->simd_arithmetic_op <= ORLIX_TCTI_SIMD_ARITH_UABA) {
		u64 accumulator[2];
		u64 left[2];
		u64 right[2];
		u64 result[2] = {};
		u64 mask;
		u8 lane_count;
		u8 operation = decoded->simd_arithmetic_op - ORLIX_TCTI_SIMD_ARITH_SABD;
		bool is_unsigned = operation & 1U;
		bool accumulate = operation & 2U;

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
			u64 difference;

			if (is_unsigned) {
				difference = left_lane > right_lane ?
					left_lane - right_lane : right_lane - left_lane;
			} else {
				s64 signed_left = sign_extend64(left_lane,
					decoded->access_size * 8 - 1);
				s64 signed_right = sign_extend64(right_lane,
					decoded->access_size * 8 - 1);
				s64 signed_difference = signed_left - signed_right;

				difference = signed_difference < 0 ?
					-signed_difference : signed_difference;
			}
			if (accumulate)
				difference += accumulator_lane;
			result[word] |= (difference & mask) << shift;
		}
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
			result[0], result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if ((decoded->simd_arithmetic_op != ORLIX_TCTI_SIMD_ARITH_ADD &&
	     decoded->simd_arithmetic_op != ORLIX_TCTI_SIMD_ARITH_SUB) ||
	    (decoded->access_size != sizeof(u8) &&
	     decoded->access_size != sizeof(u16) &&
	     decoded->access_size != sizeof(u32) &&
	     decoded->access_size != sizeof(u64)) ||
	    (decoded->simd_scalar &&
	     (decoded->access_size != sizeof(u64) ||
	      decoded->result_size != sizeof(u64))) ||
	    (!decoded->simd_scalar &&
	     decoded->result_size != sizeof(u64) &&
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
		u8 lane_count = decoded->simd_scalar ? 1 :
			decoded->result_size / decoded->access_size;

		for (lane = 0; lane < lane_count; lane++) {
			u8 byte = lane * decoded->access_size;
			u8 word = byte / sizeof(u64);
			u8 shift = (byte % sizeof(u64)) * 8;
			u64 left_lane = (left[word] >> shift) & mask;
			u64 right_lane = (right[word] >> shift) & mask;
			u64 result_lane;

			if (decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_ADD)
				result_lane = left_lane + right_lane;
			else
				result_lane = left_lane - right_lane;
			result[word] |= (result_lane & mask) << shift;
		}

		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    result[0], result[1]);
	}
	regs->pc += sizeof(u32);
	return 0;
}

static int orlix_tcti_execute_simd_vector_compare(
	struct pt_regs *regs, const struct orlix_tcti_decoded_instruction *decoded)
{
	u64 low = 0;
	u64 high = 0;
	u8 lane;

	if (decoded->immediate &&
	    decoded->simd_compare_op >= ORLIX_TCTI_SIMD_COMPARE_CMEQ &&
	    decoded->simd_compare_op <= ORLIX_TCTI_SIMD_COMPARE_CMLT) {
		if ((decoded->access_size != sizeof(u8) &&
		     decoded->access_size != sizeof(u16) &&
		     decoded->access_size != sizeof(u32) &&
		     decoded->access_size != sizeof(u64)) ||
		    (decoded->simd_scalar &&
		     (decoded->access_size != sizeof(u64) ||
		      decoded->result_size != sizeof(u64))) ||
		    (!decoded->simd_scalar &&
		     decoded->result_size != sizeof(u64) &&
		     decoded->result_size != 2 * sizeof(u64)) ||
		    decoded->access_size > decoded->result_size)
			return -EOPNOTSUPP;

		for (lane = 0; lane < (decoded->simd_scalar ? 1 :
		     decoded->result_size / decoded->access_size); lane++) {
			u8 lane_bits = decoded->access_size * 8;
			u8 byte_offset = lane * decoded->access_size;
			u8 word = byte_offset / sizeof(u64);
			u8 shift = (byte_offset % sizeof(u64)) * 8;
			u64 mask = GENMASK_ULL(lane_bits - 1, 0);
			u64 value =
				(current->thread.user_simd[decoded->rn * 2 + word] >>
				 shift) & mask;
			bool negative = value & BIT_ULL(lane_bits - 1);
			bool zero = value == 0;
			bool matches;

			switch (decoded->simd_compare_op) {
			case ORLIX_TCTI_SIMD_COMPARE_CMGT:
				matches = !negative && !zero;
				break;
			case ORLIX_TCTI_SIMD_COMPARE_CMGE:
				matches = !negative;
				break;
			case ORLIX_TCTI_SIMD_COMPARE_CMEQ:
				matches = zero;
				break;
			case ORLIX_TCTI_SIMD_COMPARE_CMLE:
				matches = negative || zero;
				break;
			default:
				matches = negative;
				break;
			}

			if (word == 0)
				low |= (matches ? mask : 0) << shift;
			else
				high |= (matches ? mask : 0) << shift;
		}

		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    low, high);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (!decoded->immediate &&
	    decoded->simd_compare_op >= ORLIX_TCTI_SIMD_COMPARE_CMEQ &&
	    decoded->simd_compare_op <= ORLIX_TCTI_SIMD_COMPARE_CMHS) {
		if ((decoded->access_size != sizeof(u8) &&
		     decoded->access_size != sizeof(u16) &&
		     decoded->access_size != sizeof(u32) &&
		     decoded->access_size != sizeof(u64)) ||
		    (decoded->simd_scalar &&
		     (decoded->access_size != sizeof(u64) ||
		      decoded->result_size != sizeof(u64))) ||
		    (!decoded->simd_scalar &&
		     decoded->result_size != sizeof(u64) &&
		     decoded->result_size != 2 * sizeof(u64)) ||
		    decoded->access_size > decoded->result_size)
			return -EOPNOTSUPP;

		for (lane = 0; lane < (decoded->simd_scalar ? 1 :
		     decoded->result_size / decoded->access_size); lane++) {
			u8 lane_bits = decoded->access_size * 8;
			u8 byte_offset = lane * decoded->access_size;
			u8 word = byte_offset / sizeof(u64);
			u8 shift = (byte_offset % sizeof(u64)) * 8;
			u64 mask = GENMASK_ULL(lane_bits - 1, 0);
			u64 left =
				(current->thread.user_simd[decoded->rn * 2 + word] >>
				 shift) & mask;
			u64 right =
				(current->thread.user_simd[decoded->rm * 2 + word] >>
				 shift) & mask;
			bool matches;

			switch (decoded->simd_compare_op) {
			case ORLIX_TCTI_SIMD_COMPARE_CMTST:
				matches = (left & right) != 0;
				break;
			case ORLIX_TCTI_SIMD_COMPARE_CMEQ:
				matches = left == right;
				break;
			case ORLIX_TCTI_SIMD_COMPARE_CMGT:
				matches = sign_extend64(left, lane_bits - 1) >
					sign_extend64(right, lane_bits - 1);
				break;
			case ORLIX_TCTI_SIMD_COMPARE_CMHI:
				matches = left > right;
				break;
			case ORLIX_TCTI_SIMD_COMPARE_CMGE:
				matches = sign_extend64(left, lane_bits - 1) >=
					sign_extend64(right, lane_bits - 1);
				break;
			case ORLIX_TCTI_SIMD_COMPARE_CMHS:
				matches = left >= right;
				break;
			default:
				return -EOPNOTSUPP;
			}
			if (word)
				high |= (matches ? mask : 0) << shift;
			else
				low |= (matches ? mask : 0) << shift;
		}
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
			low, high);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_compare_op == ORLIX_TCTI_SIMD_COMPARE_CMHI) {
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
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    left_low > right_low ? ~0ULL : 0,
					    left_high > right_high ? ~0ULL : 0);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_compare_op != ORLIX_TCTI_SIMD_COMPARE_CMEQ ||
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

	orlix_tcti_write_simd_fp_register(decoded->rd, 2 * sizeof(u64), low, high);
	regs->pc += sizeof(u32);
	return 0;
}

static int orlix_tcti_execute_simd_vector_reduction(
	struct pt_regs *regs, const struct orlix_tcti_decoded_instruction *decoded)
{
	u32 result = 0;
	u8 lane_bits;
	u8 lane;
	u8 lane_count;

	if (decoded->simd_reduction_op >= ORLIX_TCTI_SIMD_REDUCTION_FADDP &&
	    decoded->simd_reduction_op <= ORLIX_TCTI_SIMD_REDUCTION_FMINV) {
		u64 source[2];
		u64 fp_result[2] = {};
		int ret;

		source[0] = current->thread.user_simd[decoded->rn * 2];
		source[1] = current->thread.user_simd[decoded->rn * 2 + 1];
		ret = orlix_tcti_native_simd_fp_reduction(
			decoded->simd_reduction_op, decoded->access_size,
			fp_result, source, current->thread.user_fpcr,
			&current->thread.user_fpsr);
		if (ret)
			return ret;
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    fp_result[0], fp_result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->simd_reduction_op == ORLIX_TCTI_SIMD_REDUCTION_ADDP &&
	    decoded->access_size == sizeof(u64) &&
	    decoded->result_size == sizeof(u64)) {
		u64 low = current->thread.user_simd[decoded->rn * 2];
		u64 high = current->thread.user_simd[decoded->rn * 2 + 1];

		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    low + high, 0);
		regs->pc += sizeof(u32);
		return 0;
	}

	if ((decoded->simd_reduction_op == ORLIX_TCTI_SIMD_REDUCTION_SADDLV ||
	     decoded->simd_reduction_op == ORLIX_TCTI_SIMD_REDUCTION_UADDLV) &&
	    (decoded->access_size == sizeof(u8) ||
	     decoded->access_size == sizeof(u16) ||
	     decoded->access_size == sizeof(u32)) &&
	    decoded->result_size == 2 * decoded->access_size) {
		u64 sum = 0;
		u64 result_mask = decoded->result_size == sizeof(u64) ? U64_MAX :
			GENMASK_ULL(decoded->result_size * 8 - 1, 0);

		lane_bits = decoded->access_size * 8;
		lane_count = (decoded->simd_q ? 2 * sizeof(u64) : sizeof(u64)) /
			     decoded->access_size;
		for (lane = 0; lane < lane_count; lane++) {
			u8 byte_offset = lane * decoded->access_size;
			u8 word = byte_offset / sizeof(u64);
			u8 shift = (byte_offset % sizeof(u64)) * 8;
			u32 value =
				(current->thread.user_simd[decoded->rn * 2 + word] >>
				 shift) & GENMASK(lane_bits - 1, 0);

			if (decoded->simd_reduction_op == ORLIX_TCTI_SIMD_REDUCTION_SADDLV)
				sum += (s64)sign_extend32(value, lane_bits - 1);
			else
				sum += value;
		}
		orlix_tcti_write_simd_fp_register(decoded->rd, 2 * sizeof(u64),
					    sum & result_mask, 0);
		regs->pc += sizeof(u32);
		return 0;
	}

	if ((decoded->simd_reduction_op == ORLIX_TCTI_SIMD_REDUCTION_UMAXV ||
	     decoded->simd_reduction_op == ORLIX_TCTI_SIMD_REDUCTION_UMINV ||
	     decoded->simd_reduction_op == ORLIX_TCTI_SIMD_REDUCTION_SMAXV ||
	     decoded->simd_reduction_op == ORLIX_TCTI_SIMD_REDUCTION_SMINV ||
	     decoded->simd_reduction_op == ORLIX_TCTI_SIMD_REDUCTION_ADDV) &&
	    (decoded->access_size == sizeof(u8) ||
	     decoded->access_size == sizeof(u16) ||
	     decoded->access_size == sizeof(u32)) &&
	    decoded->result_size == decoded->access_size) {
		lane_bits = decoded->access_size * 8;
		lane_count = (decoded->simd_q ? 2 * sizeof(u64) : sizeof(u64)) /
			     decoded->access_size;
	} else {
		return -EOPNOTSUPP;
	}

	for (lane = 0; lane < lane_count; lane++) {
		u8 byte_offset = lane * decoded->access_size;
		u8 word = byte_offset / sizeof(u64);
		u8 shift = (byte_offset % sizeof(u64)) * 8;
		u32 value = (current->thread.user_simd[decoded->rn * 2 + word] >>
			     shift) & GENMASK(lane_bits - 1, 0);

		switch (decoded->simd_reduction_op) {
		case ORLIX_TCTI_SIMD_REDUCTION_UMAXV:
			if (lane == 0 || value > result)
				result = value;
			break;
		case ORLIX_TCTI_SIMD_REDUCTION_UMINV:
			if (lane == 0 || value < result)
				result = value;
			break;
		case ORLIX_TCTI_SIMD_REDUCTION_SMAXV:
			if (lane == 0 ||
			    sign_extend32(value, lane_bits - 1) >
			    sign_extend32(result, lane_bits - 1))
				result = value;
			break;
		case ORLIX_TCTI_SIMD_REDUCTION_SMINV:
			if (lane == 0 ||
			    sign_extend32(value, lane_bits - 1) <
			    sign_extend32(result, lane_bits - 1))
				result = value;
			break;
		case ORLIX_TCTI_SIMD_REDUCTION_ADDV:
			result = (result + value) & GENMASK(lane_bits - 1, 0);
			break;
		default:
			return -EOPNOTSUPP;
		}
	}

	orlix_tcti_write_simd_fp_register(decoded->rd, 2 * sizeof(u64), result, 0);
	regs->pc += sizeof(u32);
	return 0;
}

static int orlix_tcti_execute_fp_scalar_move(
	struct pt_regs *regs, const struct orlix_tcti_decoded_instruction *decoded)
{
	u64 value;

	if ((decoded->access_size != sizeof(u32) &&
	     decoded->access_size != sizeof(u64)) ||
	    decoded->result_size != decoded->access_size)
		return -EOPNOTSUPP;

	switch (decoded->fp_move_op) {
	case ORLIX_TCTI_FP_MOVE_SIMD_TO_GPR:
		value = current->thread.user_simd[decoded->rn * 2];
		orlix_tcti_write_gpr_or_zero(regs, decoded->rd,
				       decoded->access_size, value);
		break;
	case ORLIX_TCTI_FP_MOVE_GPR_TO_SIMD:
		value = orlix_tcti_read_gpr_or_zero(regs, decoded->rn,
					      decoded->access_size);
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->access_size,
					    value, 0);
		break;
	case ORLIX_TCTI_FP_MOVE_SIMD_HIGH_TO_GPR:
		value = current->thread.user_simd[decoded->rn * 2 + 1];
		orlix_tcti_write_gpr_or_zero(regs, decoded->rd, sizeof(u64), value);
		break;
	case ORLIX_TCTI_FP_MOVE_GPR_TO_SIMD_HIGH:
		value = orlix_tcti_read_gpr_or_zero(regs, decoded->rn, sizeof(u64));
		orlix_tcti_write_simd_lane(decoded->rd, 1, sizeof(u64), value);
		break;
	case ORLIX_TCTI_FP_MOVE_REGISTER:
		value = current->thread.user_simd[decoded->rn * 2];
		if (decoded->access_size == sizeof(u32))
			value = (u32)value;
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->access_size,
					    value, 0);
		break;
	default:
		return -EINVAL;
	}

	regs->pc += sizeof(u32);
	return 0;
}

static int orlix_tcti_execute_fp_round(const struct orlix_tcti_decoded_instruction *decoded,
				 u64 value, u64 *result)
{
	u64 host_fpcr;
	u64 host_fpsr;
	u64 guest_fpsr;
	if (!result || decoded->result_size != decoded->access_size ||
	    (decoded->access_size != sizeof(u16) &&
	     decoded->access_size != sizeof(u32) &&
	     decoded->access_size != sizeof(u64)))
		return -EOPNOTSUPP;

	preempt_disable();
	asm volatile(
		"mrs %0, fpcr\n"
		"mrs %1, fpsr\n"
		"msr fpcr, %2\n"
		"msr fpsr, %3\n"
		"isb\n"
		: "=&r" (host_fpcr), "=&r" (host_fpsr)
		: "r" (current->thread.user_fpcr),
		  "r" (current->thread.user_fpsr)
		: "memory");

	if (decoded->access_size == sizeof(u16)) {
		u16 result16;

#define ORLIX_TCTI_EXECUTE_FRINT_H(instruction) \
	({ \
		asm volatile( \
			"fmov h0, %w1\n" \
			instruction " h0, h0\n" \
			"fmov %w0, h0\n" \
			: "=r" (result16) \
			: "r" ((u32)value) \
			: "v0", "memory"); \
	})

		switch (decoded->fp1_op) {
		case ORLIX_TCTI_FP1_FRINTN:
			ORLIX_TCTI_EXECUTE_FRINT_H("frintn");
			break;
		case ORLIX_TCTI_FP1_FRINTP:
			ORLIX_TCTI_EXECUTE_FRINT_H("frintp");
			break;
		case ORLIX_TCTI_FP1_FRINTM:
			ORLIX_TCTI_EXECUTE_FRINT_H("frintm");
			break;
		case ORLIX_TCTI_FP1_FRINTZ:
			ORLIX_TCTI_EXECUTE_FRINT_H("frintz");
			break;
		case ORLIX_TCTI_FP1_FRINTA:
			ORLIX_TCTI_EXECUTE_FRINT_H("frinta");
			break;
		case ORLIX_TCTI_FP1_FRINTX:
			ORLIX_TCTI_EXECUTE_FRINT_H("frintx");
			break;
		case ORLIX_TCTI_FP1_FRINTI:
			ORLIX_TCTI_EXECUTE_FRINT_H("frinti");
			break;
		default:
			goto unsupported;
		}
#undef ORLIX_TCTI_EXECUTE_FRINT_H
		*result = result16;
	} else if (decoded->access_size == sizeof(u32)) {
		u32 result32;

#define ORLIX_TCTI_EXECUTE_FRINT_S(instruction) \
	({ \
		asm volatile( \
			"fmov s0, %w1\n" \
			instruction " s0, s0\n" \
			"fmov %w0, s0\n" \
			: "=r" (result32) \
			: "r" ((u32)value) \
			: "v0", "memory"); \
	})

		switch (decoded->fp1_op) {
		case ORLIX_TCTI_FP1_FRINTN:
			ORLIX_TCTI_EXECUTE_FRINT_S("frintn");
			break;
		case ORLIX_TCTI_FP1_FRINTP:
			ORLIX_TCTI_EXECUTE_FRINT_S("frintp");
			break;
		case ORLIX_TCTI_FP1_FRINTM:
			ORLIX_TCTI_EXECUTE_FRINT_S("frintm");
			break;
		case ORLIX_TCTI_FP1_FRINTZ:
			ORLIX_TCTI_EXECUTE_FRINT_S("frintz");
			break;
		case ORLIX_TCTI_FP1_FRINTA:
			ORLIX_TCTI_EXECUTE_FRINT_S("frinta");
			break;
		case ORLIX_TCTI_FP1_FRINTX:
			ORLIX_TCTI_EXECUTE_FRINT_S("frintx");
			break;
		case ORLIX_TCTI_FP1_FRINTI:
			ORLIX_TCTI_EXECUTE_FRINT_S("frinti");
			break;
		default:
			goto unsupported;
		}
#undef ORLIX_TCTI_EXECUTE_FRINT_S
		*result = result32;
	} else {
#define ORLIX_TCTI_EXECUTE_FRINT_D(instruction) \
	({ \
		asm volatile( \
			"fmov d0, %1\n" \
			instruction " d0, d0\n" \
			"fmov %0, d0\n" \
			: "=r" (*result) \
			: "r" (value) \
			: "v0", "memory"); \
	})

		switch (decoded->fp1_op) {
		case ORLIX_TCTI_FP1_FRINTN:
			ORLIX_TCTI_EXECUTE_FRINT_D("frintn");
			break;
		case ORLIX_TCTI_FP1_FRINTP:
			ORLIX_TCTI_EXECUTE_FRINT_D("frintp");
			break;
		case ORLIX_TCTI_FP1_FRINTM:
			ORLIX_TCTI_EXECUTE_FRINT_D("frintm");
			break;
		case ORLIX_TCTI_FP1_FRINTZ:
			ORLIX_TCTI_EXECUTE_FRINT_D("frintz");
			break;
		case ORLIX_TCTI_FP1_FRINTA:
			ORLIX_TCTI_EXECUTE_FRINT_D("frinta");
			break;
		case ORLIX_TCTI_FP1_FRINTX:
			ORLIX_TCTI_EXECUTE_FRINT_D("frintx");
			break;
		case ORLIX_TCTI_FP1_FRINTI:
			ORLIX_TCTI_EXECUTE_FRINT_D("frinti");
			break;
		default:
			goto unsupported;
		}
#undef ORLIX_TCTI_EXECUTE_FRINT_D
	}

	asm volatile(
		"mrs %0, fpsr\n"
		"msr fpcr, %1\n"
		"msr fpsr, %2\n"
		"isb\n"
		: "=&r" (guest_fpsr)
		: "r" (host_fpcr), "r" (host_fpsr)
		: "memory");
	current->thread.user_fpsr = guest_fpsr;
	preempt_enable();
	return 0;

unsupported:
	asm volatile(
		"msr fpcr, %0\n"
		"msr fpsr, %1\n"
		"isb\n"
		:
		: "r" (host_fpcr), "r" (host_fpsr)
		: "memory");
	preempt_enable();
	return -EOPNOTSUPP;
}

static int orlix_tcti_execute_fp_scalar_1source(
	struct pt_regs *regs, const struct orlix_tcti_decoded_instruction *decoded)
{
	u64 value;

	switch (decoded->fp1_op) {
	case ORLIX_TCTI_FP1_FABS:
		if (decoded->access_size == sizeof(u16)) {
			value = current->thread.user_simd[decoded->rn * 2] &
				GENMASK(14, 0);
		} else if (decoded->access_size == sizeof(u32)) {
			value = current->thread.user_simd[decoded->rn * 2] &
				GENMASK(30, 0);
		} else if (decoded->access_size == sizeof(u64)) {
			value = current->thread.user_simd[decoded->rn * 2] &
				GENMASK_ULL(62, 0);
		} else {
			return -EOPNOTSUPP;
		}
		break;
	case ORLIX_TCTI_FP1_FCVT:
	case ORLIX_TCTI_FP1_FSQRT: {
		u64 source[2];
		u64 result[2] = {};
		int ret;

		source[0] = current->thread.user_simd[decoded->rn * 2];
		source[1] = current->thread.user_simd[decoded->rn * 2 + 1];
		ret = orlix_tcti_native_fp_one_source(
			decoded->fp1_op, decoded->access_size,
			decoded->result_size, result, source,
			current->thread.user_fpcr,
			&current->thread.user_fpsr);
		if (ret)
			return ret;
		value = result[0];
		break;
	}
	case ORLIX_TCTI_FP1_FNEG:
		if (decoded->access_size == sizeof(u16)) {
			value = (u16)current->thread.user_simd[decoded->rn * 2];
			value ^= BIT(15);
		} else if (decoded->access_size == sizeof(u32)) {
			value = (u32)current->thread.user_simd[decoded->rn * 2];
			value ^= BIT(31);
		} else if (decoded->access_size == sizeof(u64)) {
			value = current->thread.user_simd[decoded->rn * 2] ^
				BIT_ULL(63);
		} else {
			return -EOPNOTSUPP;
		}
		break;
	case ORLIX_TCTI_FP1_FRINTN:
	case ORLIX_TCTI_FP1_FRINTP:
	case ORLIX_TCTI_FP1_FRINTM:
	case ORLIX_TCTI_FP1_FRINTZ:
	case ORLIX_TCTI_FP1_FRINTA:
	case ORLIX_TCTI_FP1_FRINTX:
	case ORLIX_TCTI_FP1_FRINTI:
		if (orlix_tcti_execute_fp_round(decoded,
				current->thread.user_simd[decoded->rn * 2], &value))
			return -EOPNOTSUPP;
		break;
	default:
		return -EINVAL;
	}

	orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size, value, 0);
	regs->pc += sizeof(u32);
	return 0;
}

static int orlix_tcti_execute_fp_scalar_2source(
	struct pt_regs *regs, const struct orlix_tcti_decoded_instruction *decoded)
{
	u64 left = current->thread.user_simd[decoded->rn * 2];
	u64 right = current->thread.user_simd[decoded->rm * 2];
	u64 result;
	u64 host_fpcr;
	u64 host_fpsr;
	u64 guest_fpsr;

	if (decoded->result_size == decoded->access_size &&
	    (decoded->access_size == sizeof(u16) ||
	     decoded->access_size == sizeof(u32) ||
	     decoded->access_size == sizeof(u64))) {
		preempt_disable();
		asm volatile(
			"mrs %0, fpcr\n"
			"mrs %1, fpsr\n"
			"msr fpcr, %2\n"
			"msr fpsr, %3\n"
			"isb\n"
			: "=&r" (host_fpcr), "=&r" (host_fpsr)
			: "r" (current->thread.user_fpcr),
			  "r" (current->thread.user_fpsr)
			: "memory");
		if (decoded->access_size == sizeof(u16)) {
			u16 result16;

#define ORLIX_TCTI_EXECUTE_FP2_H(instruction) \
			({ \
				asm volatile( \
					"fmov h0, %w1\n" \
					"fmov h1, %w2\n" \
					instruction " h0, h0, h1\n" \
					"fmov %w0, h0\n" \
					: "=r" (result16) \
					: "r" ((u32)left), "r" ((u32)right) \
					: "v0", "v1", "memory"); \
			})

			switch (decoded->fp2_op) {
			case ORLIX_TCTI_FP2_FDIV:
				ORLIX_TCTI_EXECUTE_FP2_H("fdiv");
				break;
			case ORLIX_TCTI_FP2_FADD:
				ORLIX_TCTI_EXECUTE_FP2_H("fadd");
				break;
			case ORLIX_TCTI_FP2_FSUB:
				ORLIX_TCTI_EXECUTE_FP2_H("fsub");
				break;
			case ORLIX_TCTI_FP2_FMUL:
				ORLIX_TCTI_EXECUTE_FP2_H("fmul");
				break;
			case ORLIX_TCTI_FP2_FMAX:
				ORLIX_TCTI_EXECUTE_FP2_H("fmax");
				break;
			case ORLIX_TCTI_FP2_FMIN:
				ORLIX_TCTI_EXECUTE_FP2_H("fmin");
				break;
			case ORLIX_TCTI_FP2_FMAXNM:
				ORLIX_TCTI_EXECUTE_FP2_H("fmaxnm");
				break;
			case ORLIX_TCTI_FP2_FMINNM:
				ORLIX_TCTI_EXECUTE_FP2_H("fminnm");
				break;
			case ORLIX_TCTI_FP2_FNMUL:
				ORLIX_TCTI_EXECUTE_FP2_H("fnmul");
				break;
			default:
				goto restore_host_fp_state;
			}
#undef ORLIX_TCTI_EXECUTE_FP2_H
			result = result16;
		} else if (decoded->access_size == sizeof(u32)) {
			u32 result32;

#define ORLIX_TCTI_EXECUTE_FP2_S(instruction) \
			({ \
				asm volatile( \
					"fmov s0, %w1\n" \
					"fmov s1, %w2\n" \
					instruction " s0, s0, s1\n" \
					"fmov %w0, s0\n" \
					: "=r" (result32) \
					: "r" ((u32)left), "r" ((u32)right) \
					: "v0", "v1", "memory"); \
			})

			switch (decoded->fp2_op) {
			case ORLIX_TCTI_FP2_FDIV:
				ORLIX_TCTI_EXECUTE_FP2_S("fdiv");
				break;
			case ORLIX_TCTI_FP2_FADD:
				ORLIX_TCTI_EXECUTE_FP2_S("fadd");
				break;
			case ORLIX_TCTI_FP2_FSUB:
				ORLIX_TCTI_EXECUTE_FP2_S("fsub");
				break;
			case ORLIX_TCTI_FP2_FMUL:
				ORLIX_TCTI_EXECUTE_FP2_S("fmul");
				break;
			case ORLIX_TCTI_FP2_FMAX:
				ORLIX_TCTI_EXECUTE_FP2_S("fmax");
				break;
			case ORLIX_TCTI_FP2_FMIN:
				ORLIX_TCTI_EXECUTE_FP2_S("fmin");
				break;
			case ORLIX_TCTI_FP2_FMAXNM:
				ORLIX_TCTI_EXECUTE_FP2_S("fmaxnm");
				break;
			case ORLIX_TCTI_FP2_FMINNM:
				ORLIX_TCTI_EXECUTE_FP2_S("fminnm");
				break;
			case ORLIX_TCTI_FP2_FNMUL:
				ORLIX_TCTI_EXECUTE_FP2_S("fnmul");
				break;
			default:
				goto restore_host_fp_state;
			}
#undef ORLIX_TCTI_EXECUTE_FP2_S
			result = result32;
		} else {
#define ORLIX_TCTI_EXECUTE_FP2_D(instruction) \
			({ \
				asm volatile( \
					"fmov d0, %1\n" \
					"fmov d1, %2\n" \
					instruction " d0, d0, d1\n" \
					"fmov %0, d0\n" \
					: "=r" (result) \
					: "r" (left), "r" (right) \
					: "v0", "v1", "memory"); \
			})

			switch (decoded->fp2_op) {
			case ORLIX_TCTI_FP2_FDIV:
				ORLIX_TCTI_EXECUTE_FP2_D("fdiv");
				break;
			case ORLIX_TCTI_FP2_FADD:
				ORLIX_TCTI_EXECUTE_FP2_D("fadd");
				break;
			case ORLIX_TCTI_FP2_FSUB:
				ORLIX_TCTI_EXECUTE_FP2_D("fsub");
				break;
			case ORLIX_TCTI_FP2_FMUL:
				ORLIX_TCTI_EXECUTE_FP2_D("fmul");
				break;
			case ORLIX_TCTI_FP2_FMAX:
				ORLIX_TCTI_EXECUTE_FP2_D("fmax");
				break;
			case ORLIX_TCTI_FP2_FMIN:
				ORLIX_TCTI_EXECUTE_FP2_D("fmin");
				break;
			case ORLIX_TCTI_FP2_FMAXNM:
				ORLIX_TCTI_EXECUTE_FP2_D("fmaxnm");
				break;
			case ORLIX_TCTI_FP2_FMINNM:
				ORLIX_TCTI_EXECUTE_FP2_D("fminnm");
				break;
			case ORLIX_TCTI_FP2_FNMUL:
				ORLIX_TCTI_EXECUTE_FP2_D("fnmul");
				break;
			default:
				goto restore_host_fp_state;
			}
#undef ORLIX_TCTI_EXECUTE_FP2_D
		}
		asm volatile("mrs %0, fpsr\n" : "=r" (guest_fpsr));
		current->thread.user_fpsr = guest_fpsr;
		asm volatile(
			"msr fpcr, %0\n"
			"msr fpsr, %1\n"
			"isb\n"
			:
			: "r" (host_fpcr), "r" (host_fpsr)
			: "memory");
		preempt_enable();
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->access_size,
					    result, 0);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->fp2_op == ORLIX_TCTI_FP2_FMUL) {
		u64 vector_left[2];
		u64 vector_right[2];
		u64 vector_result[2] = {};
		int ret;

		if (decoded->access_size != sizeof(u64) ||
		    decoded->result_size != 2 * sizeof(u64))
			return -EOPNOTSUPP;
		vector_left[0] = left;
		vector_left[1] = current->thread.user_simd[decoded->rn * 2 + 1];
		vector_right[0] = right;
		vector_right[1] = current->thread.user_simd[decoded->rm * 2 + 1];
		ret = orlix_tcti_native_simd_fp_three_same(
			ORLIX_TCTI_SIMD_ARITH_FMUL, false, decoded->access_size,
			decoded->result_size, vector_result, vector_left, vector_right,
			vector_left, current->thread.user_fpcr,
			&current->thread.user_fpsr);
		if (ret)
			return ret;
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    vector_result[0], vector_result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	return -EOPNOTSUPP;

restore_host_fp_state:
	asm volatile(
		"msr fpcr, %0\n"
		"msr fpsr, %1\n"
		"isb\n"
		:
		: "r" (host_fpcr), "r" (host_fpsr)
		: "memory");
	preempt_enable();
	return -EINVAL;
}

static int orlix_tcti_execute_fp_scalar_3source(
	struct pt_regs *regs, const struct orlix_tcti_decoded_instruction *decoded)
{
	u64 left = current->thread.user_simd[decoded->rn * 2];
	u64 right = current->thread.user_simd[decoded->rm * 2];
	u64 addend = current->thread.user_simd[decoded->ra * 2];
	u64 host_fpcr;
	u64 host_fpsr;
	u64 guest_fpsr;
	u64 result = 0;

	if (decoded->result_size != decoded->access_size ||
	    (decoded->access_size != sizeof(u16) &&
	     decoded->access_size != sizeof(u32) &&
	     decoded->access_size != sizeof(u64)) ||
	    decoded->fp3_op > ORLIX_TCTI_FP3_FNMSUB)
		return -EOPNOTSUPP;

	preempt_disable();
	asm volatile(
		"mrs %0, fpcr\n"
		"mrs %1, fpsr\n"
		"msr fpcr, %2\n"
		"msr fpsr, %3\n"
		"isb\n"
		: "=&r" (host_fpcr), "=&r" (host_fpsr)
		: "r" (current->thread.user_fpcr),
		  "r" (current->thread.user_fpsr)
		: "memory");

	if (decoded->access_size == sizeof(u16)) {
		u16 result16;

#define ORLIX_TCTI_EXECUTE_FP3_H(instruction) \
		({ \
			asm volatile( \
				"fmov h0, %w1\n" \
				"fmov h1, %w2\n" \
				"fmov h2, %w3\n" \
				instruction " h0, h0, h1, h2\n" \
				"fmov %w0, h0\n" \
				: "=r" (result16) \
				: "r" ((u32)left), "r" ((u32)right), \
				  "r" ((u32)addend) \
				: "v0", "v1", "v2", "memory"); \
		})

		switch (decoded->fp3_op) {
		case ORLIX_TCTI_FP3_FMADD:
			ORLIX_TCTI_EXECUTE_FP3_H("fmadd");
			break;
		case ORLIX_TCTI_FP3_FMSUB:
			ORLIX_TCTI_EXECUTE_FP3_H("fmsub");
			break;
		case ORLIX_TCTI_FP3_FNMADD:
			ORLIX_TCTI_EXECUTE_FP3_H("fnmadd");
			break;
		case ORLIX_TCTI_FP3_FNMSUB:
			ORLIX_TCTI_EXECUTE_FP3_H("fnmsub");
			break;
		default:
			return -EOPNOTSUPP;
		}
#undef ORLIX_TCTI_EXECUTE_FP3_H
		result = result16;
	} else if (decoded->access_size == sizeof(u32)) {
		u32 result32;

#define ORLIX_TCTI_EXECUTE_FP3_S(instruction) \
		asm volatile( \
			"fmov s0, %w1\n" \
			"fmov s1, %w2\n" \
			"fmov s2, %w3\n" \
			instruction " s0, s0, s1, s2\n" \
			"fmov %w0, s0\n" \
			: "=r" (result32) \
			: "r" ((u32)left), "r" ((u32)right), \
			  "r" ((u32)addend) \
			: "v0", "v1", "v2", "memory")

		switch (decoded->fp3_op) {
		case ORLIX_TCTI_FP3_FMADD:
			ORLIX_TCTI_EXECUTE_FP3_S("fmadd");
			break;
		case ORLIX_TCTI_FP3_FMSUB:
			ORLIX_TCTI_EXECUTE_FP3_S("fmsub");
			break;
		case ORLIX_TCTI_FP3_FNMADD:
			ORLIX_TCTI_EXECUTE_FP3_S("fnmadd");
			break;
		case ORLIX_TCTI_FP3_FNMSUB:
			ORLIX_TCTI_EXECUTE_FP3_S("fnmsub");
			break;
		}
#undef ORLIX_TCTI_EXECUTE_FP3_S
		result = result32;
	} else {
#define ORLIX_TCTI_EXECUTE_FP3_D(instruction) \
		asm volatile( \
			"fmov d0, %1\n" \
			"fmov d1, %2\n" \
			"fmov d2, %3\n" \
			instruction " d0, d0, d1, d2\n" \
			"fmov %0, d0\n" \
			: "=r" (result) \
			: "r" (left), "r" (right), "r" (addend) \
			: "v0", "v1", "v2", "memory")

		switch (decoded->fp3_op) {
		case ORLIX_TCTI_FP3_FMADD:
			ORLIX_TCTI_EXECUTE_FP3_D("fmadd");
			break;
		case ORLIX_TCTI_FP3_FMSUB:
			ORLIX_TCTI_EXECUTE_FP3_D("fmsub");
			break;
		case ORLIX_TCTI_FP3_FNMADD:
			ORLIX_TCTI_EXECUTE_FP3_D("fnmadd");
			break;
		case ORLIX_TCTI_FP3_FNMSUB:
			ORLIX_TCTI_EXECUTE_FP3_D("fnmsub");
			break;
		}
#undef ORLIX_TCTI_EXECUTE_FP3_D
	}

	asm volatile(
		"mrs %0, fpsr\n"
		"msr fpcr, %1\n"
		"msr fpsr, %2\n"
		"isb\n"
		: "=&r" (guest_fpsr)
		: "r" (host_fpcr), "r" (host_fpsr)
		: "memory");
	current->thread.user_fpsr = guest_fpsr;
	preempt_enable();

	orlix_tcti_write_simd_fp_register(decoded->rd, decoded->access_size,
				    result, 0);
	regs->pc += sizeof(u32);
	return 0;
}

static int orlix_tcti_execute_fp_scalar_compare(
	struct pt_regs *regs, const struct orlix_tcti_decoded_instruction *decoded)
{
	u64 left = current->thread.user_simd[decoded->rn * 2];
	u64 right = decoded->immediate ?
			    0 : current->thread.user_simd[decoded->rm * 2];
	bool invalid_operation;
	int result;

	if (decoded->fp_conditional &&
	    !orlix_tcti_condition_passed(regs, decoded->condition)) {
		orlix_tcti_set_nzcv_from_immediate(regs, decoded->nzcv);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->access_size == sizeof(u16)) {
		result = orlix_tcti_compare_fp16(left, right);
		invalid_operation =
			(decoded->fp_signal_all_nans &&
			 (orlix_tcti_fp16_is_nan(left) || orlix_tcti_fp16_is_nan(right))) ||
			orlix_tcti_fp16_is_signaling_nan(left) ||
			orlix_tcti_fp16_is_signaling_nan(right);
	} else if (decoded->access_size == sizeof(u32)) {
		result = orlix_tcti_compare_fp32(left, right);
		invalid_operation =
			(decoded->fp_signal_all_nans &&
			 (orlix_tcti_fp32_is_nan(left) || orlix_tcti_fp32_is_nan(right))) ||
			orlix_tcti_fp32_is_signaling_nan(left) ||
			orlix_tcti_fp32_is_signaling_nan(right);
	} else if (decoded->access_size == sizeof(u64)) {
		result = orlix_tcti_compare_fp64(left, right);
		invalid_operation =
			(decoded->fp_signal_all_nans &&
			 (orlix_tcti_fp64_is_nan(left) || orlix_tcti_fp64_is_nan(right))) ||
			orlix_tcti_fp64_is_signaling_nan(left) ||
			orlix_tcti_fp64_is_signaling_nan(right);
	} else {
		return -EOPNOTSUPP;
	}

	if (invalid_operation)
		current->thread.user_fpsr |= AARCH64_FPSR_IOC;
	orlix_tcti_set_fp_compare_flags(regs, result);
	regs->pc += sizeof(u32);
	return 0;
}

static int orlix_tcti_execute_fp_conditional_select(
	struct pt_regs *regs, const struct orlix_tcti_decoded_instruction *decoded)
{
	u64 result;

	if (decoded->access_size != sizeof(u16) &&
	    decoded->access_size != sizeof(u32) &&
	    decoded->access_size != sizeof(u64))
		return -EOPNOTSUPP;

	result = orlix_tcti_condition_passed(regs, decoded->condition) ?
		 current->thread.user_simd[decoded->rn * 2] :
		 current->thread.user_simd[decoded->rm * 2];
	if (decoded->access_size == sizeof(u16))
		result = (u16)result;
	else if (decoded->access_size == sizeof(u32))
		result = (u32)result;

	orlix_tcti_write_simd_fp_register(decoded->rd, decoded->access_size,
				    result, 0);
	regs->pc += sizeof(u32);
	return 0;
}

static u64 orlix_tcti_execute_native_fcvt_fixed(
	const struct orlix_tcti_decoded_instruction *decoded, u64 value)
{
	bool unsigned_conversion =
		decoded->fp_int_op == ORLIX_TCTI_FP_INT_FCVTZU_FIXED;
	bool double_source = decoded->access_size == sizeof(u64);
	bool wide_result = decoded->result_size == sizeof(u64);

	if (unsigned_conversion) {
		if (wide_result)
			return double_source ?
				orlix_tcti_native_fcvtzu_x_d(value, decoded->shift_amount) :
				orlix_tcti_native_fcvtzu_x_s(value, decoded->shift_amount);
		return double_source ?
			orlix_tcti_native_fcvtzu_w_d(value, decoded->shift_amount) :
			orlix_tcti_native_fcvtzu_w_s(value, decoded->shift_amount);
	}

	if (wide_result)
		return double_source ?
			orlix_tcti_native_fcvtzs_x_d(value, decoded->shift_amount) :
			orlix_tcti_native_fcvtzs_x_s(value, decoded->shift_amount);
	return double_source ?
		orlix_tcti_native_fcvtzs_w_d(value, decoded->shift_amount) :
		orlix_tcti_native_fcvtzs_w_s(value, decoded->shift_amount);
}

static int orlix_tcti_execute_fp_fixed_convert(
	struct pt_regs *regs, const struct orlix_tcti_decoded_instruction *decoded)
{
	bool int_to_fp = decoded->fp_int_op == ORLIX_TCTI_FP_INT_SCVTF_FIXED ||
		decoded->fp_int_op == ORLIX_TCTI_FP_INT_UCVTF_FIXED;
	u64 host_fpcr;
	u64 host_fpsr;
	u64 guest_fpsr;
	u64 value = int_to_fp ? orlix_tcti_read_gpr_or_zero(regs, decoded->rn,
		decoded->access_size) :
		current->thread.user_simd[decoded->rn * 2];
	u64 result;

	preempt_disable();
	asm volatile(
		"mrs %0, fpcr\n"
		"mrs %1, fpsr\n"
		"msr fpcr, %2\n"
		"msr fpsr, %3\n"
		"isb\n"
		: "=&r" (host_fpcr), "=&r" (host_fpsr)
		: "r" (current->thread.user_fpcr),
		  "r" (current->thread.user_fpsr)
		: "memory");

	if (int_to_fp)
		result = orlix_tcti_native_gpr_to_fp_fixed(decoded->fp_int_op,
			decoded->access_size, decoded->result_size, value,
			decoded->shift_amount);
	else
		result = orlix_tcti_execute_native_fcvt_fixed(decoded, value);

	asm volatile(
		"mrs %0, fpsr\n"
		"msr fpcr, %1\n"
		"msr fpsr, %2\n"
		"isb\n"
		: "=&r" (guest_fpsr)
		: "r" (host_fpcr), "r" (host_fpsr)
		: "memory");
	current->thread.user_fpsr = guest_fpsr;
	preempt_enable();

	if (int_to_fp)
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
			result, 0);
	else
		orlix_tcti_write_gpr_or_zero(regs, decoded->rd, decoded->result_size,
			result);
	regs->pc += sizeof(u32);
	return 0;
}

static int orlix_tcti_execute_fp_int_convert(
	struct pt_regs *regs, const struct orlix_tcti_decoded_instruction *decoded)
{
	u64 value;
	u64 result;
	u64 host_fpcr;
	u64 host_fpsr;
	unsigned long guest_fpsr;
	u64 vector_source[2];
	u64 vector_result[2];
	unsigned long native_fpsr;

	if (decoded->fp_int_op >= ORLIX_TCTI_FP_INT_FCVTZS_FIXED &&
	    decoded->fp_int_op <= ORLIX_TCTI_FP_INT_UCVTF_FIXED)
		return orlix_tcti_execute_fp_fixed_convert(regs, decoded);

	if (decoded->fp_int_op >= ORLIX_TCTI_FP_INT_FCVTZS_FIXED_SIMD &&
	    decoded->fp_int_op <= ORLIX_TCTI_FP_INT_UCVTF_FIXED_SIMD) {
		vector_source[0] = current->thread.user_simd[decoded->rn * 2];
		vector_source[1] = current->thread.user_simd[decoded->rn * 2 + 1];
		guest_fpsr = current->thread.user_fpsr;
		if (orlix_tcti_native_fixed_simd_fp_convert(decoded->fp_int_op,
			decoded->simd_scalar, decoded->simd_q,
			decoded->access_size, decoded->shift_amount,
			vector_result, vector_source, current->thread.user_fpcr,
			&guest_fpsr))
			return -EOPNOTSUPP;
		current->thread.user_fpsr = guest_fpsr;
		orlix_tcti_write_simd_fp_register(decoded->rd,
			decoded->simd_scalar ? decoded->access_size :
			(decoded->simd_q ? 2 * sizeof(u64) : sizeof(u64)),
			vector_result[0], vector_result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->fp_int_op >= ORLIX_TCTI_FP_INT_FCVTNS_SIMD &&
	    decoded->fp_int_op <= ORLIX_TCTI_FP_INT_SCVTF_SIMD) {
		vector_source[0] = current->thread.user_simd[decoded->rn * 2];
		vector_source[1] = current->thread.user_simd[decoded->rn * 2 + 1];
		native_fpsr = current->thread.user_fpsr;
		if (orlix_tcti_native_simd_fp_convert(
				decoded->fp_int_op, decoded->simd_scalar,
				decoded->simd_q, decoded->access_size,
				vector_result, vector_source,
				current->thread.user_fpcr, &native_fpsr))
			return -EOPNOTSUPP;
		current->thread.user_fpsr = native_fpsr;
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    vector_result[0], vector_result[1]);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->fp_int_op >= ORLIX_TCTI_FP_INT_FCVTNS &&
	    decoded->fp_int_op <= ORLIX_TCTI_FP_INT_FCVTAU) {
		value = current->thread.user_simd[decoded->rn * 2];
		native_fpsr = current->thread.user_fpsr;
		if (orlix_tcti_native_fp_to_gpr(decoded->fp_int_op,
					   decoded->access_size,
					   decoded->result_size, value, &result,
					   current->thread.user_fpcr,
					   &native_fpsr))
			return -EOPNOTSUPP;
		current->thread.user_fpsr = native_fpsr;
		orlix_tcti_write_gpr_or_zero(regs, decoded->rd,
				       decoded->result_size, result);
		regs->pc += sizeof(u32);
		return 0;
	}

	if ((decoded->fp_int_op == ORLIX_TCTI_FP_INT_FCVTZS ||
	     decoded->fp_int_op == ORLIX_TCTI_FP_INT_FCVTZU) &&
	    (decoded->access_size == sizeof(u32) ||
	     decoded->access_size == sizeof(u64)) &&
	    (decoded->result_size == sizeof(u32) ||
	     decoded->result_size == sizeof(u64))) {
		value = current->thread.user_simd[decoded->rn * 2];
		preempt_disable();
		asm volatile(
			"mrs %0, fpcr\n"
			"mrs %1, fpsr\n"
			"msr fpcr, %2\n"
			"msr fpsr, %3\n"
			"isb\n"
			: "=&r" (host_fpcr), "=&r" (host_fpsr)
			: "r" (current->thread.user_fpcr),
			  "r" (current->thread.user_fpsr)
			: "memory");
		if (decoded->result_size == sizeof(u32)) {
			u32 result32;

#define ORLIX_TCTI_EXECUTE_FCVT_W(instruction, source, input) \
			({ \
				asm volatile( \
					"fmov " source "0, " input "\n" \
					instruction " %w0, " source "0\n" \
					: "=r" (result32) \
					: "r" (value) \
					: "v0", "memory"); \
			})

			if (decoded->fp_int_op == ORLIX_TCTI_FP_INT_FCVTZS) {
				if (decoded->access_size == sizeof(u32))
					ORLIX_TCTI_EXECUTE_FCVT_W("fcvtzs", "s", "%w1");
				else
					ORLIX_TCTI_EXECUTE_FCVT_W("fcvtzs", "d", "%1");
			} else if (decoded->access_size == sizeof(u32)) {
				ORLIX_TCTI_EXECUTE_FCVT_W("fcvtzu", "s", "%w1");
			} else {
				ORLIX_TCTI_EXECUTE_FCVT_W("fcvtzu", "d", "%1");
			}
#undef ORLIX_TCTI_EXECUTE_FCVT_W
			result = result32;
		} else {
#define ORLIX_TCTI_EXECUTE_FCVT_X(instruction, source, input) \
			({ \
				asm volatile( \
					"fmov " source "0, " input "\n" \
					instruction " %0, " source "0\n" \
					: "=r" (result) \
					: "r" (value) \
					: "v0", "memory"); \
			})

			if (decoded->fp_int_op == ORLIX_TCTI_FP_INT_FCVTZS) {
				if (decoded->access_size == sizeof(u32))
					ORLIX_TCTI_EXECUTE_FCVT_X("fcvtzs", "s", "%w1");
				else
					ORLIX_TCTI_EXECUTE_FCVT_X("fcvtzs", "d", "%1");
			} else if (decoded->access_size == sizeof(u32)) {
				ORLIX_TCTI_EXECUTE_FCVT_X("fcvtzu", "s", "%w1");
			} else {
				ORLIX_TCTI_EXECUTE_FCVT_X("fcvtzu", "d", "%1");
			}
#undef ORLIX_TCTI_EXECUTE_FCVT_X
		}
		asm volatile("mrs %0, fpsr\n" : "=r" (guest_fpsr));
		current->thread.user_fpsr = guest_fpsr;
		asm volatile(
			"msr fpcr, %0\n"
			"msr fpsr, %1\n"
			"isb\n"
			:
			: "r" (host_fpcr), "r" (host_fpsr)
			: "memory");
		preempt_enable();
		orlix_tcti_write_gpr_or_zero(regs, decoded->rd,
					decoded->result_size, result);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->fp_int_op == ORLIX_TCTI_FP_INT_SCVTF &&
	    decoded->access_size == sizeof(u32)) {
		value = orlix_tcti_read_gpr_or_zero(regs, decoded->rn, sizeof(u32));
		if (decoded->result_size == sizeof(u32)) {
			orlix_tcti_write_simd_fp_register(
				decoded->rd, decoded->result_size,
				orlix_tcti_s32_to_fp32_bits((s32)value), 0);
			regs->pc += sizeof(u32);
			return 0;
		}
		if (decoded->result_size != sizeof(u64))
			return -EOPNOTSUPP;
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    orlix_tcti_s32_to_fp64_bits((s32)value),
					    0);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->fp_int_op == ORLIX_TCTI_FP_INT_SCVTF &&
	    decoded->access_size == sizeof(u64)) {
		value = orlix_tcti_read_gpr_or_zero(regs, decoded->rn, sizeof(u64));
		if (decoded->result_size == sizeof(u32))
			result = orlix_tcti_s64_to_fp32_bits((s64)value);
		else if (decoded->result_size == sizeof(u64))
			result = orlix_tcti_s64_to_fp64_bits((s64)value);
		else
			return -EOPNOTSUPP;
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    result, 0);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->fp_int_op == ORLIX_TCTI_FP_INT_UCVTF &&
	    decoded->result_size == sizeof(u32)) {
		value = orlix_tcti_read_gpr_or_zero(regs, decoded->rn,
					      decoded->access_size);
		if (decoded->access_size == sizeof(u64)) {
			orlix_tcti_write_simd_fp_register(
				decoded->rd, decoded->result_size,
				orlix_tcti_u64_to_fp32_bits(value), 0);
			regs->pc += sizeof(u32);
			return 0;
		}
		if (decoded->access_size != sizeof(u32))
			return -EOPNOTSUPP;
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    orlix_tcti_u32_to_fp32_bits((u32)value),
					    0);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->fp_int_op == ORLIX_TCTI_FP_INT_UCVTF &&
	    decoded->result_size == sizeof(u64)) {
		value = orlix_tcti_read_gpr_or_zero(regs, decoded->rn,
					      decoded->access_size);
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    orlix_tcti_u64_to_fp64_bits(value), 0);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->fp_int_op == ORLIX_TCTI_FP_INT_FCVTZS &&
	    decoded->access_size == sizeof(u64)) {
		value = current->thread.user_simd[decoded->rn * 2];
		if (orlix_tcti_fp64_bits_to_s64_zero(value, &result))
			return -EOPNOTSUPP;
		orlix_tcti_write_gpr_or_zero(regs, decoded->rd, decoded->result_size,
				       result);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->fp_int_op == ORLIX_TCTI_FP_INT_FCVTZU &&
	    decoded->access_size == sizeof(u32)) {
		value = current->thread.user_simd[decoded->rn * 2] & GENMASK(31, 0);
		if (orlix_tcti_fp32_bits_to_u64_zero((u32)value, &result))
			return -EOPNOTSUPP;
		if (decoded->result_size == sizeof(u32)) {
			if (result > GENMASK_ULL(31, 0))
				result = GENMASK_ULL(31, 0);
		} else if (decoded->result_size != sizeof(u64)) {
			return -EOPNOTSUPP;
		}
		orlix_tcti_write_gpr_or_zero(regs, decoded->rd, decoded->result_size,
				       result);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->fp_int_op == ORLIX_TCTI_FP_INT_FCVTZU &&
	    decoded->access_size == sizeof(u64)) {
		value = current->thread.user_simd[decoded->rn * 2];
		if (orlix_tcti_fp64_bits_to_u64_zero(value, &result))
			return -EOPNOTSUPP;
		if (decoded->result_size == sizeof(u32)) {
			if (result > GENMASK_ULL(31, 0))
				result = GENMASK_ULL(31, 0);
		} else if (decoded->result_size != sizeof(u64)) {
			return -EOPNOTSUPP;
		}
		orlix_tcti_write_gpr_or_zero(regs, decoded->rd, decoded->result_size,
				       result);
		regs->pc += sizeof(u32);
		return 0;
	}

	if ((decoded->fp_int_op == ORLIX_TCTI_FP_INT_FCVTZS_SIMD ||
	     decoded->fp_int_op == ORLIX_TCTI_FP_INT_FCVTZU_SIMD) &&
	    decoded->result_size == decoded->access_size) {
		value = current->thread.user_simd[decoded->rn * 2];
		if (decoded->access_size == sizeof(u32)) {
			if (decoded->fp_int_op == ORLIX_TCTI_FP_INT_FCVTZS_SIMD) {
				if (orlix_tcti_fp32_bits_to_s32_zero((u32)value, &result))
					return -EOPNOTSUPP;
			} else if (orlix_tcti_fp32_bits_to_u64_zero((u32)value,
							      &result)) {
				return -EOPNOTSUPP;
			}
		} else if (decoded->access_size == sizeof(u64)) {
			if (decoded->fp_int_op == ORLIX_TCTI_FP_INT_FCVTZS_SIMD) {
				if (orlix_tcti_fp64_bits_to_s64_zero(value, &result))
					return -EOPNOTSUPP;
			} else if (orlix_tcti_fp64_bits_to_u64_zero(value, &result)) {
				return -EOPNOTSUPP;
			}
		} else {
			return -EOPNOTSUPP;
		}
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    result, 0);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->fp_int_op == ORLIX_TCTI_FP_INT_UCVTF_SIMD &&
	    decoded->access_size == sizeof(u64) &&
	    decoded->result_size == sizeof(u64)) {
		value = current->thread.user_simd[decoded->rn * 2];
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    orlix_tcti_u64_to_fp64_bits(value), 0);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->fp_int_op == ORLIX_TCTI_FP_INT_UCVTF_SIMD &&
	    decoded->access_size == sizeof(u64) &&
	    decoded->result_size == 2 * sizeof(u64)) {
		value = current->thread.user_simd[decoded->rn * 2];
		result = current->thread.user_simd[decoded->rn * 2 + 1];
		orlix_tcti_write_simd_fp_register(decoded->rd, decoded->result_size,
					    orlix_tcti_u64_to_fp64_bits(value),
					    orlix_tcti_u64_to_fp64_bits(result));
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->fp_int_op == ORLIX_TCTI_FP_INT_SCVTF_SIMD &&
	    decoded->access_size == sizeof(u64) &&
	    decoded->result_size == sizeof(u64)) {
		value = current->thread.user_simd[decoded->rn * 2];
		orlix_tcti_write_simd_fp_register(decoded->rd, sizeof(u64),
					    orlix_tcti_s64_to_fp64_bits((s64)value), 0);
		regs->pc += sizeof(u32);
		return 0;
	}

	if (decoded->fp_int_op != ORLIX_TCTI_FP_INT_SCVTF)
		return -EOPNOTSUPP;

	return -EINVAL;
}

int orlix_tcti_execute_flag_manipulation_semantics(
	struct pt_regs *regs,
	const struct orlix_tcti_decoded_instruction *decoded)
{
	unsigned long flags;
	u64 source;
	u8 width;

	if (!regs || !decoded ||
	    decoded->decode_class != ORLIX_TCTI_DECODE_FLAG_MANIPULATION)
		return -EINVAL;
	if ((regs->pstate & PSR_MODE_MASK) != PSR_MODE_EL0t)
		return -EOPNOTSUPP;

	source = orlix_tcti_read_gpr_or_zero(regs, decoded->rn, sizeof(u64));
	switch (decoded->flag_manipulation_op) {
	case ORLIX_TCTI_FLAG_MANIPULATION_RMIF: {
		unsigned long mask = (unsigned long)(decoded->nzcv & 0xfU) << 28;

		flags = (unsigned long)(ror64(source, decoded->imm6) & 0xfU) << 28;
		regs->pstate = (regs->pstate & ~mask) | (flags & mask);
		break;
	}
	case ORLIX_TCTI_FLAG_MANIPULATION_SETF8:
		width = 8;
		break;
	case ORLIX_TCTI_FLAG_MANIPULATION_SETF16:
		width = 16;
		break;
	default:
		return -EINVAL;
	}

	if (decoded->flag_manipulation_op != ORLIX_TCTI_FLAG_MANIPULATION_RMIF) {
		u64 value = source & (BIT_ULL(width) - 1);
		bool sign = value & BIT_ULL(width - 1);
		bool extension = source & BIT_ULL(width);

		flags = (sign ? PSR_N_BIT : 0) |
			(!value ? PSR_Z_BIT : 0) |
			(sign != extension ? PSR_V_BIT : 0);
		regs->pstate = (regs->pstate &
			~(PSR_N_BIT | PSR_Z_BIT | PSR_V_BIT)) | flags;
	}

	regs->pc += sizeof(u32);
	return 0;
}

int orlix_tcti_execute_decoded_semantics(struct mm_struct *mm,
				   struct pt_regs *regs,
				 const struct orlix_tcti_decoded_instruction *decoded,
				   unsigned long *fault_address)
{
	u64 immediate;
	u64 source;

	if (!regs || !decoded)
		return -EINVAL;

	switch (decoded->decode_class) {
	case ORLIX_TCTI_DECODE_FLAG_MANIPULATION:
		return orlix_tcti_execute_flag_manipulation_semantics(regs, decoded);
	case ORLIX_TCTI_DECODE_SVE_PREDICATED_INTEGER_BINARY:
		return orlix_tcti_execute_sve_predicated_integer_binary(regs, decoded);
	case ORLIX_TCTI_DECODE_MOPS_COPY:
		return orlix_tcti_execute_mops_copy(mm, regs, decoded, fault_address);
	case ORLIX_TCTI_DECODE_HINT:
		regs->pc += sizeof(u32);
		return decoded->hint_imm >= 1 && decoded->hint_imm <= 3 ?
			-EAGAIN : 0;
	case ORLIX_TCTI_DECODE_BARRIER:
		/*
		 * DSB <imm2>nXS is a distinct FEAT_XS completion contract. Do not
		 * collapse it into the ordinary host fence until that guest-visible
		 * ordering semantics has authoritative implementation and proof.
		 */
		if (decoded->barrier_nxs)
			return -EOPNOTSUPP;
		__atomic_thread_fence(__ATOMIC_SEQ_CST);
		regs->pc += sizeof(u32);
		return 0;
	case ORLIX_TCTI_DECODE_CACHE_MAINTENANCE:
		/*
		 * The accepted EL0 cache-maintenance leaves remain unproved until
		 * their official translation, permission, and fault semantics are
		 * implemented from the pinned Arm ASL.
		 */
		return -EOPNOTSUPP;
	case ORLIX_TCTI_DECODE_PC_RELATIVE_ADDRESS:
		if (decoded->rd != 31) {
			u64 base = decoded->page_relative ?
				   (regs->pc & AARCH64_ADRP_PAGE_MASK) :
				   regs->pc;

			regs->regs[decoded->rd] =
				base + decoded->pc_relative_imm;
		}
		regs->pc += sizeof(u32);
		return 0;
	case ORLIX_TCTI_DECODE_ADD_SUB_IMMEDIATE:
		immediate = (u64)decoded->imm12 << (decoded->shift ? 12 : 0);
		source = orlix_tcti_read_add_sub_immediate_source(regs, decoded);
		return orlix_tcti_execute_add_sub_result(regs, decoded, source,
						   immediate, true);
	case ORLIX_TCTI_DECODE_MEMORY_TAGGING:
		return orlix_tcti_execute_memory_tagging(mm, regs, decoded,
						 fault_address);
	case ORLIX_TCTI_DECODE_MIN_MAX_IMMEDIATE:
		return orlix_tcti_execute_min_max_immediate(regs, decoded);
	case ORLIX_TCTI_DECODE_ADD_SUB_SHIFTED_REGISTER:
		return orlix_tcti_execute_add_sub_shifted_register(regs, decoded);
	case ORLIX_TCTI_DECODE_ADD_SUB_EXTENDED_REGISTER:
		return orlix_tcti_execute_add_sub_extended_register(regs, decoded);
	case ORLIX_TCTI_DECODE_ADD_SUB_WITH_CARRY:
		return orlix_tcti_execute_add_sub_with_carry(regs, decoded);
	case ORLIX_TCTI_DECODE_UNCONDITIONAL_BRANCH_IMMEDIATE:
		if (decoded->link)
			regs->regs[30] = regs->pc + sizeof(u32);
		regs->pc += decoded->branch_imm;
		return 0;
	case ORLIX_TCTI_DECODE_UNCONDITIONAL_BRANCH_REGISTER:
		source = orlix_tcti_read_gpr_or_zero(regs, decoded->rn, sizeof(u64));
		if (decoded->branch_register_op == ORLIX_TCTI_BRANCH_REGISTER_BLR)
			regs->regs[30] = regs->pc + sizeof(u32);
		regs->pc = source;
		return 0;
	case ORLIX_TCTI_DECODE_COMPARE_BRANCH_IMMEDIATE:
		source = orlix_tcti_read_gpr_or_zero(regs, decoded->rt,
				decoded->is_64bit ?
				sizeof(u64) : sizeof(u32));
		if ((!source) != decoded->nonzero)
			regs->pc += decoded->branch_imm;
		else
			regs->pc += sizeof(u32);
		return 0;
	case ORLIX_TCTI_DECODE_COMPARE_BRANCH_EXTENSION: {
		u64 left = orlix_tcti_read_gpr_or_zero(regs, decoded->rt,
				decoded->compare_branch_access_size);
		u64 right = decoded->compare_branch_immediate ? decoded->imm6 :
			orlix_tcti_read_gpr_or_zero(regs, decoded->rm,
				decoded->compare_branch_access_size);
		bool taken;

		switch (decoded->compare_branch_condition) {
		case ORLIX_TCTI_COMPARE_BRANCH_GT:
			taken = decoded->compare_branch_access_size == sizeof(u8) ?
				(s8)left > (s8)right :
				decoded->compare_branch_access_size == sizeof(u16) ?
				(s16)left > (s16)right :
				decoded->compare_branch_access_size == sizeof(u32) ?
				(s32)left > (s32)right : (s64)left > (s64)right;
			break;
		case ORLIX_TCTI_COMPARE_BRANCH_GE:
			taken = decoded->compare_branch_access_size == sizeof(u8) ?
				(s8)left >= (s8)right :
				decoded->compare_branch_access_size == sizeof(u16) ?
				(s16)left >= (s16)right :
				decoded->compare_branch_access_size == sizeof(u32) ?
				(s32)left >= (s32)right : (s64)left >= (s64)right;
			break;
		case ORLIX_TCTI_COMPARE_BRANCH_HI:
			taken = left > right;
			break;
		case ORLIX_TCTI_COMPARE_BRANCH_HS:
			taken = left >= right;
			break;
		case ORLIX_TCTI_COMPARE_BRANCH_EQ:
			taken = left == right;
			break;
		case ORLIX_TCTI_COMPARE_BRANCH_NE:
			taken = left != right;
			break;
		case ORLIX_TCTI_COMPARE_BRANCH_LT:
			taken = decoded->compare_branch_access_size == sizeof(u32) ?
				(s32)left < (s32)right : (s64)left < (s64)right;
			break;
		case ORLIX_TCTI_COMPARE_BRANCH_LO:
			taken = left < right;
			break;
		default:
			return -EOPNOTSUPP;
		}
		regs->pc += taken ? decoded->branch_imm : sizeof(u32);
		return 0;
	}
	case ORLIX_TCTI_DECODE_TEST_BRANCH_IMMEDIATE:
		source = orlix_tcti_read_gpr_or_zero(regs, decoded->rt, sizeof(u64));
		if (!!(source & BIT_ULL(decoded->test_bit)) ==
		    decoded->nonzero)
			regs->pc += decoded->branch_imm;
		else
			regs->pc += sizeof(u32);
		return 0;
	case ORLIX_TCTI_DECODE_CONDITIONAL_BRANCH_IMMEDIATE:
		if (orlix_tcti_condition_passed(regs, decoded->condition))
			regs->pc += decoded->branch_imm;
		else
			regs->pc += sizeof(u32);
		return 0;
	case ORLIX_TCTI_DECODE_CONDITIONAL_COMPARE:
		return orlix_tcti_execute_conditional_compare(regs, decoded);
	case ORLIX_TCTI_DECODE_CONDITIONAL_SELECT:
		return orlix_tcti_execute_conditional_select(regs, decoded);
	case ORLIX_TCTI_DECODE_LOAD_LITERAL:
		return orlix_tcti_execute_load_literal(mm, regs, decoded,
						 fault_address);
	case ORLIX_TCTI_DECODE_LOAD_STORE_PAIR:
		return orlix_tcti_execute_load_store_pair(mm, regs, decoded,
						    fault_address);
	case ORLIX_TCTI_DECODE_LOAD_STORE_UNSIGNED_IMMEDIATE:
	case ORLIX_TCTI_DECODE_LOAD_STORE_SIGNED_IMMEDIATE:
		return orlix_tcti_execute_load_store_immediate(mm, regs, decoded,
							 fault_address);
	case ORLIX_TCTI_DECODE_LOAD_STORE_REGISTER_OFFSET:
		return orlix_tcti_execute_load_store_register_offset(mm, regs, decoded,
							       fault_address);
	case ORLIX_TCTI_DECODE_GCS_STORE:
		return orlix_tcti_execute_gcs_store(mm, regs, decoded, fault_address);
	case ORLIX_TCTI_DECODE_LOGICAL_SHIFTED_REGISTER:
		return orlix_tcti_execute_logical_shifted_register(regs, decoded);
	case ORLIX_TCTI_DECODE_LOGICAL_IMMEDIATE:
		return orlix_tcti_execute_logical_immediate(regs, decoded);
	case ORLIX_TCTI_DECODE_BITFIELD:
		return orlix_tcti_execute_bitfield(regs, decoded);
	case ORLIX_TCTI_DECODE_EXTRACT:
		return orlix_tcti_execute_extract(regs, decoded);
	case ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE:
		return orlix_tcti_execute_data_processing_1source(regs, decoded);
	case ORLIX_TCTI_DECODE_DATA_PROCESSING_2SOURCE:
		return orlix_tcti_execute_data_processing_2source(regs, decoded);
	case ORLIX_TCTI_DECODE_MULTIPLY_ADD_SUB:
		return orlix_tcti_execute_multiply_add_sub(regs, decoded);
	case ORLIX_TCTI_DECODE_MOVE_WIDE_IMMEDIATE:
		return orlix_tcti_execute_move_wide_immediate(regs, decoded);
	case ORLIX_TCTI_DECODE_SYSTEM_REGISTER:
		return orlix_tcti_execute_system_register(regs, decoded);
	case ORLIX_TCTI_DECODE_SME_PSTATE_IMMEDIATE:
		return -EOPNOTSUPP;
	case ORLIX_TCTI_DECODE_EXCLUSIVE_MONITOR_CLEAR:
		orlix_tcti_clear_exclusive_monitor();
		regs->pc += sizeof(u32);
		return 0;
	case ORLIX_TCTI_DECODE_LOAD_STORE_EXCLUSIVE:
		return orlix_tcti_execute_load_store_exclusive(mm, regs, decoded,
							 fault_address);
	case ORLIX_TCTI_DECODE_LSE_ATOMIC:
		return orlix_tcti_execute_lse_atomic(mm, regs, decoded, fault_address);
	case ORLIX_TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE:
		return orlix_tcti_execute_simd_modified_immediate(regs, decoded);
	case ORLIX_TCTI_DECODE_FP_SCALAR_IMMEDIATE:
		return orlix_tcti_execute_fp_scalar_immediate(regs, decoded);
	case ORLIX_TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE:
		return orlix_tcti_execute_simd_vector_element_move(regs, decoded);
	case ORLIX_TCTI_DECODE_SIMD_TABLE_LOOKUP:
		return orlix_tcti_execute_simd_table_lookup(regs, decoded);
	case ORLIX_TCTI_DECODE_SIMD_VECTOR_LOGICAL:
		return orlix_tcti_execute_simd_vector_logical(regs, decoded);
	case ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC:
		return orlix_tcti_execute_simd_vector_arithmetic(regs, decoded);
	case ORLIX_TCTI_DECODE_SIMD_VECTOR_COMPARE:
		return orlix_tcti_execute_simd_vector_compare(regs, decoded);
	case ORLIX_TCTI_DECODE_SIMD_VECTOR_REDUCTION:
		return orlix_tcti_execute_simd_vector_reduction(regs, decoded);
	case ORLIX_TCTI_DECODE_SIMD_LOAD_STORE_SINGLE_STRUCTURE:
	case ORLIX_TCTI_DECODE_SIMD_LOAD_REPLICATE:
		return orlix_tcti_execute_simd_single_structure(mm, regs, decoded,
							 fault_address);
	case ORLIX_TCTI_DECODE_SIMD_LOAD_STORE_MULTIPLE_STRUCTURE:
		return orlix_tcti_execute_simd_multiple_structure(mm, regs, decoded,
							   fault_address);
	case ORLIX_TCTI_DECODE_FP_SCALAR_MOVE:
		return orlix_tcti_execute_fp_scalar_move(regs, decoded);
	case ORLIX_TCTI_DECODE_FP_SCALAR_1SOURCE:
		return orlix_tcti_execute_fp_scalar_1source(regs, decoded);
	case ORLIX_TCTI_DECODE_FP_SCALAR_2SOURCE:
		return orlix_tcti_execute_fp_scalar_2source(regs, decoded);
	case ORLIX_TCTI_DECODE_FP_SCALAR_3SOURCE:
		return orlix_tcti_execute_fp_scalar_3source(regs, decoded);
	case ORLIX_TCTI_DECODE_FP_SCALAR_COMPARE:
		return orlix_tcti_execute_fp_scalar_compare(regs, decoded);
	case ORLIX_TCTI_DECODE_FP_CONDITIONAL_SELECT:
		return orlix_tcti_execute_fp_conditional_select(regs, decoded);
	case ORLIX_TCTI_DECODE_FP_INT_CONVERT:
		return orlix_tcti_execute_fp_int_convert(regs, decoded);
	default:
		return -EOPNOTSUPP;
	}
}

#if IS_ENABLED(CONFIG_ORLIX_TCTI_DEBUG_SWITCH)
int orlix_tcti_switch_debug_execute_decoded(struct mm_struct *mm,
				      struct pt_regs *regs,
				      const struct orlix_tcti_decoded_instruction *decoded,
				      unsigned long *fault_address)
{
	return orlix_tcti_execute_decoded_semantics(mm, regs, decoded,
					      fault_address);
}

struct orlix_tcti_result orlix_tcti_switch_debug_resume_user(struct task_struct *task,
						 struct pt_regs *regs,
						 struct mm_struct *mm)
{
	struct orlix_tcti_result result = {
		.reason = ORLIX_TCTI_EXIT_TASK_EXIT,
		.status = -EINVAL,
	};
	struct orlix_tcti_decoded_instruction decoded;
	unsigned long fault_address;
	int ret;

	if (!task || !regs || !mm)
		return result;

	for (;;) {
		u32 instruction = 0;

		ret = orlix_tcti_fetch_instruction(mm, regs->pc, &instruction);
		if (ret) {
			result.reason = ret == -EFAULT &&
				!IS_ALIGNED(regs->pc, sizeof(u32)) ?
				ORLIX_TCTI_EXIT_ALIGNMENT_FAULT : ORLIX_TCTI_EXIT_USER_FAULT;
			result.status = ret;
			result.fault_address = regs->pc;
			result.fault_access = ORLIX_TCTI_ACCESS_FETCH;
			result.pc = regs->pc;
			result.instruction = instruction;
			return result;
		}

		decoded = orlix_tcti_decode_aarch64(instruction);
		if (decoded.decode_class == ORLIX_TCTI_DECODE_BRK) {
			result.reason = ORLIX_TCTI_EXIT_BREAKPOINT;
			result.status = decoded.imm16;
			result.pc = regs->pc;
			result.instruction = instruction;
			return result;
		}
		if (decoded.decode_class == ORLIX_TCTI_DECODE_SVC) {
			result.reason = ORLIX_TCTI_EXIT_SYSCALL;
			result.status = 0;
			result.pc = regs->pc;
			result.instruction = instruction;
			return result;
		}
		if (decoded.decode_class == ORLIX_TCTI_DECODE_HLT) {
			result.reason = ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION;
			result.status = -EOPNOTSUPP;
			result.pc = regs->pc;
			result.instruction = instruction;
			return result;
		}

		fault_address = regs->pc;
		ret = orlix_tcti_switch_debug_execute_decoded(mm, regs, &decoded,
							&fault_address);
		if (!ret)
			continue;

		if (ret == -EFAULT || ret == -EACCES || ret == -EHWPOISON) {
			result.reason =
				ret == -EHWPOISON ? ORLIX_TCTI_EXIT_MTE_TAG_FAULT :
				ret == -EFAULT &&
				orlix_tcti_memory_alignment_fault(&decoded,
							    fault_address) ?
				ORLIX_TCTI_EXIT_ALIGNMENT_FAULT :
				ORLIX_TCTI_EXIT_USER_FAULT;
			result.status = ret;
			result.fault_address = fault_address;
			result.fault_access = orlix_tcti_fault_access_for_decoded(&decoded);
			result.pc = regs->pc;
			result.instruction = instruction;
			return result;
		}

		result.reason = ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION;
		result.status = -ENOSYS;
		result.pc = regs->pc;
		result.instruction = instruction;
		return result;
	}
}
#endif
