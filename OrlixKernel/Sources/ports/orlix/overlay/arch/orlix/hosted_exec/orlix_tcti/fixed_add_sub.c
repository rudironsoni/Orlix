// SPDX-License-Identifier: GPL-2.0-only
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>

#include "decode_aarch64.h"
#include "fixed_add_sub.h"

struct orlix_tcti_cpu_system_state {
	struct orlix_tcti_cpa_control cpa_control;
};

static DEFINE_SPINLOCK(orlix_tcti_cpu_system_state_lock);
static struct orlix_tcti_cpu_system_state orlix_tcti_cpu_system_state = {
	.cpa_control.feat_cpa = true,
};

void orlix_tcti_cpu_cpa_control_get(struct orlix_tcti_cpa_control *control)
{
	unsigned long flags;

	spin_lock_irqsave(&orlix_tcti_cpu_system_state_lock, flags);
	*control = orlix_tcti_cpu_system_state.cpa_control;
	spin_unlock_irqrestore(&orlix_tcti_cpu_system_state_lock, flags);
}

void orlix_tcti_cpu_cpa_control_set(
	const struct orlix_tcti_cpa_control *control)
{
	unsigned long flags;

	spin_lock_irqsave(&orlix_tcti_cpu_system_state_lock, flags);
	orlix_tcti_cpu_system_state.cpa_control = *control;
	spin_unlock_irqrestore(&orlix_tcti_cpu_system_state_lock, flags);
}

static u64 orlix_tcti_fixed_read_gpr_or_zero(const struct pt_regs *regs,
					       u8 reg, u8 access_size)
{
	u64 value = reg == 31 ? 0 : regs->regs[reg];

	return access_size == sizeof(u32) ? (u32)value : value;
}

static void orlix_tcti_fixed_write_gpr_or_zero(struct pt_regs *regs, u8 reg,
						u8 access_size, u64 value)
{
	if (reg != 31)
		regs->regs[reg] = access_size == sizeof(u32) ? (u32)value : value;
}

static u64 orlix_tcti_fixed_read_gpr_or_sp(const struct pt_regs *regs, u8 reg,
					     u8 access_size)
{
	u64 value = reg == 31 ? regs->sp : regs->regs[reg];

	return access_size == sizeof(u32) ? (u32)value : value;
}

static void orlix_tcti_fixed_write_gpr_or_sp(struct pt_regs *regs, u8 reg,
					       u8 access_size, u64 value)
{
	if (access_size == sizeof(u32))
		value = (u32)value;
	if (reg == 31)
		regs->sp = value;
	else
		regs->regs[reg] = value;
}

static void orlix_tcti_fixed_update_flags(struct pt_regs *regs, u64 left,
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

static int orlix_tcti_fixed_result(struct pt_regs *regs,
				    const struct orlix_tcti_decoded_instruction *decoded,
				    u64 left, u64 right, bool sp_allowed)
{
	u8 access_size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
	u64 result = decoded->subtract ? left - right : left + right;

	if (decoded->set_flags)
		orlix_tcti_fixed_update_flags(regs, left, right, result,
					     access_size, decoded->subtract);
	if (decoded->set_flags && decoded->rd == 31) {
		regs->pc += sizeof(u32);
		return 0;
	}
	if (decoded->set_flags || !sp_allowed)
		orlix_tcti_fixed_write_gpr_or_zero(regs, decoded->rd, access_size,
						   result);
	else
		orlix_tcti_fixed_write_gpr_or_sp(regs, decoded->rd, access_size,
						 result);
	regs->pc += sizeof(u32);
	return 0;
}

static u64 orlix_tcti_fixed_shift(u64 value,
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

static u64 orlix_tcti_fixed_extend(u64 value, u8 option)
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
	default: return value;
	}
}

static int orlix_tcti_fixed_carry(struct pt_regs *regs,
				  const struct orlix_tcti_decoded_instruction *decoded)
{
	u8 access_size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
	u64 left = orlix_tcti_fixed_read_gpr_or_zero(regs, decoded->rn, access_size);
	u64 right = orlix_tcti_fixed_read_gpr_or_zero(regs, decoded->rm, access_size);
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
		regs->pstate &= ~(PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT);
		regs->pstate |= flags;
	}
	if (!decoded->set_flags || decoded->rd != 31)
		orlix_tcti_fixed_write_gpr_or_zero(regs, decoded->rd, access_size,
						   result);
	regs->pc += sizeof(u32);
	return 0;
}

static int orlix_tcti_fixed_pointer_checked(struct pt_regs *regs,
					     const struct orlix_tcti_decoded_instruction *decoded)
{
	struct orlix_tcti_cpa_control control;
	struct orlix_tcti_pointer_add_observation *observation =
		&current->thread.user_cpa_add_observation;
	u64 base;
	u64 offset;
	u64 result;

	memset(observation, 0, sizeof(*observation));
	orlix_tcti_cpu_cpa_control_get(&control);
	if (!control.feat_cpa)
		return -EOPNOTSUPP;
	base = orlix_tcti_fixed_read_gpr_or_sp(regs, decoded->rn, sizeof(u64));
	offset = orlix_tcti_fixed_read_gpr_or_zero(regs, decoded->rm, sizeof(u64));
	offset <<= decoded->shift_amount;
	result = decoded->subtract ? base - offset : base + offset;
	observation->base = base;
	observation->arithmetic_result = result;
	observation->previous_detection =
		!!(base & BIT_ULL(55)) != !!(base & BIT_ULL(54));
	observation->cpta_detected =
		((result ^ base) & GENMASK_ULL(63, 56)) != 0 ||
		observation->previous_detection;
	observation->effective_cpta = control.feat_cpa2 &&
		control.sctlr2_el1_enabled && control.sctlr2_el1_cpta0;
	observation->poisoned = observation->cpta_detected &&
		observation->effective_cpta;
	if (observation->poisoned) {
		result &= GENMASK_ULL(53, 0);
		result |= base & GENMASK_ULL(63, 55);
		if (!(base & BIT_ULL(55)))
			result |= BIT_ULL(54);
	}
	observation->result = result;
	observation->valid = true;
	if (decoded->rd == 31)
		regs->sp = result;
	else
		regs->regs[decoded->rd] = result;
	regs->pc += sizeof(u32);
	return 0;
}

int orlix_tcti_fixed_execute_add_sub(struct pt_regs *regs,
	const struct orlix_tcti_decoded_instruction *decoded)
{
	u8 access_size;
	u64 left;
	u64 right;

	if (!regs || !decoded)
		return -EINVAL;
	access_size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
	switch (decoded->decode_class) {
	case ORLIX_TCTI_DECODE_ADD_SUB_IMMEDIATE:
		left = orlix_tcti_fixed_read_gpr_or_sp(regs, decoded->rn, access_size);
		right = (u64)decoded->imm12 << (decoded->shift ? 12 : 0);
		return orlix_tcti_fixed_result(regs, decoded, left, right, true);
	case ORLIX_TCTI_DECODE_ADD_SUB_SHIFTED_REGISTER:
		left = orlix_tcti_fixed_read_gpr_or_zero(regs, decoded->rn, access_size);
		right = orlix_tcti_fixed_read_gpr_or_zero(regs, decoded->rm, access_size);
		return orlix_tcti_fixed_result(regs, decoded, left,
					      orlix_tcti_fixed_shift(right, decoded), false);
	case ORLIX_TCTI_DECODE_ADD_SUB_EXTENDED_REGISTER:
		left = orlix_tcti_fixed_read_gpr_or_sp(regs, decoded->rn, access_size);
		right = orlix_tcti_fixed_read_gpr_or_zero(regs, decoded->rm, sizeof(u64));
		right = orlix_tcti_fixed_extend(right, decoded->offset_extend);
		right <<= decoded->shift_amount;
		if (!decoded->is_64bit) {
			left = (u32)left;
			right = (u32)right;
		}
		return orlix_tcti_fixed_result(regs, decoded, left, right, true);
	case ORLIX_TCTI_DECODE_ADD_SUB_WITH_CARRY:
		return orlix_tcti_fixed_carry(regs, decoded);
	case ORLIX_TCTI_DECODE_ADD_SUB_POINTER_CHECKED:
		return orlix_tcti_fixed_pointer_checked(regs, decoded);
	default:
		return -EOPNOTSUPP;
	}
}
