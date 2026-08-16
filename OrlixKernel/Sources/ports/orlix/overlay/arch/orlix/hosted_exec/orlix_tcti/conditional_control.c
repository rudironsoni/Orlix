/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/bitops.h>
#include <linux/errno.h>

#include "conditional_control.h"
#include "decode_aarch64.h"

static u64 conditional_gpr(const struct pt_regs *regs, u8 reg, u8 size)
{
	u64 value = reg == 31 ? 0 : regs->regs[reg];

	return size == sizeof(u32) ? (u32)value : value;
}

static void conditional_write_gpr(struct pt_regs *regs, u8 reg, u8 size,
				  u64 value)
{
	if (reg == 31)
		return;
	regs->regs[reg] = size == sizeof(u32) ? (u32)value : value;
}

static bool conditional_passed(const struct pt_regs *regs, u8 condition)
{
	bool n = regs->pstate & PSR_N_BIT;
	bool z = regs->pstate & PSR_Z_BIT;
	bool c = regs->pstate & PSR_C_BIT;
	bool v = regs->pstate & PSR_V_BIT;

	switch (condition) {
	case 0: return z;
	case 1: return !z;
	case 2: return c;
	case 3: return !c;
	case 4: return n;
	case 5: return !n;
	case 6: return v;
	case 7: return !v;
	case 8: return c && !z;
	case 9: return !c || z;
	case 10: return n == v;
	case 11: return n != v;
	case 12: return !z && n == v;
	case 13: return z || n != v;
	case 14: return true; /* AL */
	default: return false; /* NV */
	}
}

static void conditional_set_nzcv(struct pt_regs *regs, u8 nzcv)
{
	regs->pstate &= ~(PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT);
	if (nzcv & BIT(3)) regs->pstate |= PSR_N_BIT;
	if (nzcv & BIT(2)) regs->pstate |= PSR_Z_BIT;
	if (nzcv & BIT(1)) regs->pstate |= PSR_C_BIT;
	if (nzcv & BIT(0)) regs->pstate |= PSR_V_BIT;
}

static void conditional_add_sub_flags(struct pt_regs *regs, u64 left, u64 right,
				      u64 result, u8 size, bool subtract)
{
	u64 sign = size == sizeof(u32) ? BIT_ULL(31) : BIT_ULL(63);
	u64 mask = size == sizeof(u32) ? U32_MAX : U64_MAX;

	left &= mask;
	right &= mask;
	result &= mask;
	bool n = result & sign;
	bool z = !(result & mask);
	bool c = subtract ? left >= right : result < left;
	bool v = subtract ? ((left ^ right) & (left ^ result) & sign) :
		((~(left ^ right) & (left ^ result)) & sign);

	regs->pstate &= ~(PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT);
	if (n) regs->pstate |= PSR_N_BIT;
	if (z) regs->pstate |= PSR_Z_BIT;
	if (c) regs->pstate |= PSR_C_BIT;
	if (v) regs->pstate |= PSR_V_BIT;
}

bool orlix_tcti_conditional_control_decoded(
	const struct orlix_tcti_decoded_instruction *decoded)
{
	if (!decoded)
		return false;

	switch (decoded->decode_class) {
	case ORLIX_TCTI_DECODE_COMPARE_BRANCH_IMMEDIATE:
	case ORLIX_TCTI_DECODE_TEST_BRANCH_IMMEDIATE:
	case ORLIX_TCTI_DECODE_CONDITIONAL_BRANCH_IMMEDIATE:
	case ORLIX_TCTI_DECODE_CONDITIONAL_COMPARE:
	case ORLIX_TCTI_DECODE_CONDITIONAL_SELECT:
		return true;
	default:
		return false;
	}
}

int orlix_tcti_execute_conditional_control_semantics(
	struct pt_regs *regs, const struct orlix_tcti_decoded_instruction *decoded)
{
	u8 size;
	u64 left, right, result;

	if (!regs || !orlix_tcti_conditional_control_decoded(decoded))
		return -EINVAL;

	switch (decoded->decode_class) {
	case ORLIX_TCTI_DECODE_COMPARE_BRANCH_IMMEDIATE:
		size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
		left = conditional_gpr(regs, decoded->rt, size);
		regs->pc += ((!left) != decoded->nonzero) ? decoded->branch_imm : sizeof(u32);
		return 0;
	case ORLIX_TCTI_DECODE_TEST_BRANCH_IMMEDIATE:
		left = conditional_gpr(regs, decoded->rt, sizeof(u64));
		regs->pc += (!!(left & BIT_ULL(decoded->test_bit)) == decoded->nonzero) ?
			decoded->branch_imm : sizeof(u32);
		return 0;
	case ORLIX_TCTI_DECODE_CONDITIONAL_BRANCH_IMMEDIATE:
		regs->pc += conditional_passed(regs, decoded->condition) ?
			decoded->branch_imm : sizeof(u32);
		return 0;
	case ORLIX_TCTI_DECODE_CONDITIONAL_COMPARE:
		size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
		if (!conditional_passed(regs, decoded->condition)) {
			conditional_set_nzcv(regs, decoded->nzcv);
			regs->pc += sizeof(u32);
			return 0;
		}
		left = conditional_gpr(regs, decoded->rn, size);
		right = decoded->immediate ? decoded->imm12 :
			conditional_gpr(regs, decoded->rm, size);
		result = decoded->subtract ? left - right : left + right;
		conditional_add_sub_flags(regs, left, right, result, size,
					  decoded->subtract);
		regs->pc += sizeof(u32);
		return 0;
	case ORLIX_TCTI_DECODE_CONDITIONAL_SELECT:
		size = decoded->is_64bit ? sizeof(u64) : sizeof(u32);
		result = conditional_gpr(regs, conditional_passed(regs, decoded->condition) ?
			decoded->rn : decoded->rm, size);
		if (!conditional_passed(regs, decoded->condition)) {
			switch (decoded->conditional_select_op) {
			case ORLIX_TCTI_CONDITIONAL_SELECT_CSEL: break;
			case ORLIX_TCTI_CONDITIONAL_SELECT_CSINC: result++; break;
			case ORLIX_TCTI_CONDITIONAL_SELECT_CSINV: result = ~result; break;
			case ORLIX_TCTI_CONDITIONAL_SELECT_CSNEG: result = -result; break;
			default: return -EOPNOTSUPP;
			}
		}
		conditional_write_gpr(regs, decoded->rd, size, result);
		regs->pc += sizeof(u32);
		return 0;
	default:
		return -EOPNOTSUPP;
	}
}
