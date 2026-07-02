// SPDX-License-Identifier: GPL-2.0-only
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/limits.h>
#include <linux/log2.h>
#include <linux/unaligned.h>
#include <asm/page.h>
#include <asm/processor.h>
#include <linux/sched.h>
#include <asm/ptrace.h>
#include <asm/tcti.h>

#include "decode_aarch64.h"
#include "semantics.h"
#include "switch_debug.h"

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
				       u64 left, u64 right)
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

	if (decoded->set_flags)
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
	u64 left = decoded->set_flags ?
		   tcti_read_gpr_or_zero(regs, decoded->rn, access_size) :
		   tcti_read_gpr_or_sp(regs, decoded->rn, access_size);
	u64 right = tcti_read_gpr_or_zero(regs, decoded->rm, access_size);

	right = tcti_shift_logical_source(right, decoded);
	return tcti_execute_add_sub_result(regs, decoded, left, right);
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

	return tcti_execute_add_sub_result(regs, decoded, left, right);
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
	result = lsb ? (high >> lsb) | (low << (data_size - lsb)) : high;
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

	switch (decoded->dp1_op) {
	case TCTI_DP1_CLZ:
		value &= mask;
		result = value ? data_size - fls64(value) : data_size;
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
		if (decoded->system_register_write)
			current->thread.user_tls = value;
		else if (decoded->rt != 31)
			regs->regs[decoded->rt] = current->thread.user_tls;
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
	u8 buffer[sizeof(u64)];

	if (access_size != sizeof(u64))
		return -EOPNOTSUPP;

	put_unaligned_le64(current->thread.user_simd[reg * 2], buffer);
	return tcti_write_user_data(mm, address, buffer, access_size);
}

static int tcti_load_simd_fp(struct mm_struct *mm, unsigned long address,
			     u8 reg, u8 access_size)
{
	u8 buffer[sizeof(u64)] = {};
	int ret;

	if (access_size != sizeof(u64))
		return -EOPNOTSUPP;

	ret = tcti_read_user_data(mm, address, buffer, access_size);
	if (ret)
		return ret;

	current->thread.user_simd[reg * 2] = get_unaligned_le64(buffer);
	current->thread.user_simd[reg * 2 + 1] = 0;
	current->thread.user_simd_valid = 1;
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
			ret = tcti_load_simd_fp(mm, address, decoded->rt,
						decoded->access_size);
			if (ret)
				return ret;
			ret = tcti_load_simd_fp(mm, address + decoded->access_size,
						decoded->rt2,
						decoded->access_size);
			if (ret)
				return ret;
		} else {
			ret = tcti_load_integer(mm, address, decoded->access_size,
						&first);
			if (ret)
				return ret;
			ret = tcti_load_integer(mm, address + decoded->access_size,
						decoded->access_size, &second);
			if (ret)
				return ret;
			tcti_write_gpr_or_zero(regs, decoded->rt,
					       decoded->access_size, first);
			tcti_write_gpr_or_zero(regs, decoded->rt2,
					       decoded->access_size, second);
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
		ret = tcti_load_integer(mm, address, decoded->access_size, &value);
		if (ret)
			return ret;
		value = tcti_extend_loaded_integer(value, decoded);
		tcti_write_gpr_or_zero(regs, decoded->rt,
				       decoded->result_size, value);
	} else {
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
		ret = tcti_load_integer(mm, address, decoded->access_size, &value);
		if (ret)
			return ret;
		value = tcti_extend_loaded_integer(value, decoded);
		tcti_write_gpr_or_zero(regs, decoded->rt,
				       decoded->result_size, value);
	} else {
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
				   (regs->pc & PAGE_MASK) : regs->pc;

			regs->regs[decoded->rd] =
				base + decoded->pc_relative_imm;
		}
		regs->pc += sizeof(u32);
		return 0;
	case TCTI_DECODE_ADD_SUB_IMMEDIATE:
		immediate = (u64)decoded->imm12 << (decoded->shift ? 12 : 0);
		source = tcti_read_add_sub_immediate_source(regs, decoded);
		return tcti_execute_add_sub_result(regs, decoded, source,
						   immediate);
	case TCTI_DECODE_ADD_SUB_SHIFTED_REGISTER:
		return tcti_execute_add_sub_shifted_register(regs, decoded);
	case TCTI_DECODE_ADD_SUB_EXTENDED_REGISTER:
		return tcti_execute_add_sub_extended_register(regs, decoded);
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
	default:
		return -EOPNOTSUPP;
	}
}

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
