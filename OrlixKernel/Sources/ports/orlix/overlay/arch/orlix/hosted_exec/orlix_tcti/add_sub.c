// SPDX-License-Identifier: GPL-2.0-only
#include <linux/bitops.h>
#include <linux/errno.h>
#include <asm/ptrace.h>

#include "add_sub.h"
#include "decode_aarch64.h"

#define ORLIX_TCTI_NZCV (PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT)

static u64 orlix_tcti_add_sub_mask(bool wide)
{
	return wide ? U64_MAX : U32_MAX;
}

static u64 orlix_tcti_add_sub_read_zero(const struct pt_regs *regs, u8 reg,
					 bool wide)
{
	u64 value = reg == 31 ? 0 : regs->regs[reg];

	return value & orlix_tcti_add_sub_mask(wide);
}

static u64 orlix_tcti_add_sub_read_sp(const struct pt_regs *regs, u8 reg,
					   bool wide)
{
	u64 value = reg == 31 ? regs->sp : regs->regs[reg];

	return value & orlix_tcti_add_sub_mask(wide);
}

static void orlix_tcti_add_sub_write_zero(struct pt_regs *regs, u8 reg,
					   bool wide, u64 value)
{
	if (reg != 31)
		regs->regs[reg] = value & orlix_tcti_add_sub_mask(wide);
}

static void orlix_tcti_add_sub_write_sp(struct pt_regs *regs, u8 reg,
					 bool wide, u64 value)
{
	value &= orlix_tcti_add_sub_mask(wide);
	if (reg == 31)
		regs->sp = value;
	else
		regs->regs[reg] = value;
}

static u64 orlix_tcti_add_sub_extend(u64 value, u8 option)
{
	switch (option) {
	case 0: return (u8)value;
	case 1: return (u16)value;
	case 2: return (u32)value;
	case 3: return value;
	case 4: return (s64)(s8)value;
	case 5: return (s64)(s16)value;
	case 6: return (s64)(s32)value;
	case 7: return (s64)value;
	default: return 0;
	}
}

static u64 orlix_tcti_add_sub_shift(u64 value, bool wide, u8 shift, u8 amount)
{
	u64 mask = orlix_tcti_add_sub_mask(wide);

	value &= mask;
	switch (shift) {
	case 0: return (value << amount) & mask;
	case 1: return value >> amount;
	case 2: return wide ? (u64)((s64)value >> amount) :
		(u64)((s32)value >> amount) & mask;
	default: return 0;
	}
}

static void orlix_tcti_add_sub_flags(struct pt_regs *regs, bool wide,
					  u64 left, u64 addend, u64 result,
					  __uint128_t total)
{
	u64 sign = wide ? BIT_ULL(63) : BIT_ULL(31);
	u64 flags = 0;

	if (result & sign)
		flags |= PSR_N_BIT;
	if (!result)
		flags |= PSR_Z_BIT;
	if (total >> (wide ? 64 : 32))
		flags |= PSR_C_BIT;
	if (!!(left & sign) == !!(addend & sign) &&
	    !!(left & sign) != !!(result & sign))
		flags |= PSR_V_BIT;
	regs->pstate = (regs->pstate & ~ORLIX_TCTI_NZCV) | flags;
}

static int orlix_tcti_add_sub_commit(struct pt_regs *regs,
		const struct orlix_tcti_decoded_instruction *decoded, bool sp_form,
		u64 left, u64 right, bool carry_in)
{
	bool wide = decoded->is_64bit;
	u64 mask = orlix_tcti_add_sub_mask(wide);
	u64 addend = decoded->subtract ? ~right : right;
	__uint128_t total;
	u64 result;

	left &= mask;
	addend &= mask;
	total = (__uint128_t)left + addend + carry_in;
	result = (u64)total & mask;
	if (decoded->set_flags)
		orlix_tcti_add_sub_flags(regs, wide, left, addend, result, total);
	if (sp_form && !decoded->set_flags)
		orlix_tcti_add_sub_write_sp(regs, decoded->rd, wide, result);
	else
		orlix_tcti_add_sub_write_zero(regs, decoded->rd, wide, result);
	regs->pc += sizeof(u32);
	return 0;
}

int orlix_tcti_execute_add_sub(struct pt_regs *regs,
	const struct orlix_tcti_decoded_instruction *decoded)
{
	u64 left, right;

	if (!regs || !decoded)
		return -EINVAL;
	switch (decoded->decode_class) {
	case ORLIX_TCTI_DECODE_ADD_SUB_IMMEDIATE:
		left = decoded->set_flags ?
			orlix_tcti_add_sub_read_zero(regs, decoded->rn, decoded->is_64bit) :
			orlix_tcti_add_sub_read_sp(regs, decoded->rn, decoded->is_64bit);
		right = (u64)decoded->imm12 << (decoded->shift ? 12 : 0);
		return orlix_tcti_add_sub_commit(regs, decoded, true, left, right, false);
	case ORLIX_TCTI_DECODE_ADD_SUB_SHIFTED_REGISTER:
		left = orlix_tcti_add_sub_read_zero(regs, decoded->rn, decoded->is_64bit);
		right = orlix_tcti_add_sub_shift(
			orlix_tcti_add_sub_read_zero(regs, decoded->rm, decoded->is_64bit),
			decoded->is_64bit, decoded->shift, decoded->shift_amount);
		return orlix_tcti_add_sub_commit(regs, decoded, false, left, right, false);
	case ORLIX_TCTI_DECODE_ADD_SUB_EXTENDED_REGISTER:
		left = decoded->set_flags ?
			orlix_tcti_add_sub_read_zero(regs, decoded->rn, decoded->is_64bit) :
			orlix_tcti_add_sub_read_sp(regs, decoded->rn, decoded->is_64bit);
		right = orlix_tcti_add_sub_extend(
			orlix_tcti_add_sub_read_zero(regs, decoded->rm, true),
			decoded->offset_extend) << decoded->shift_amount;
		return orlix_tcti_add_sub_commit(regs, decoded, true, left, right, false);
	case ORLIX_TCTI_DECODE_ADD_SUB_WITH_CARRY:
		left = orlix_tcti_add_sub_read_zero(regs, decoded->rn, decoded->is_64bit);
		right = orlix_tcti_add_sub_read_zero(regs, decoded->rm, decoded->is_64bit);
		return orlix_tcti_add_sub_commit(regs, decoded, false, left, right,
			!!(regs->pstate & PSR_C_BIT));
	default:
		return -EOPNOTSUPP;
	}
}
