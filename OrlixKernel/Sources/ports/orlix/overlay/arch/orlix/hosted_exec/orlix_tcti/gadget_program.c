// SPDX-License-Identifier: GPL-2.0-only
#include <asm/ptrace.h>
#include <linux/bitops.h>
#include <linux/bits.h>
#include <linux/errno.h>
#include <linux/limits.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/unaligned.h>

#include <asm/orlix_tcti.h>

#include "block_cache.h"
#include "crc32.h"
#include "decode_aarch64.h"
#include "gadget_program.h"
#include "native_capture.h"
#include "semantics.h"

#define ORLIX_TCTI_GADGET_DONE 1
#define ORLIX_TCTI_OP_64BIT BIT(0)
#define ORLIX_TCTI_OP_SUBTRACT BIT(1)
#define ORLIX_TCTI_OP_SET_FLAGS BIT(2)
#define ORLIX_TCTI_OP_LOAD BIT(3)
#define ORLIX_TCTI_OP_SIGN_EXT BIT(4)
#define ORLIX_TCTI_OP_REG BIT(5)
#define ORLIX_TCTI_OP_MOVK BIT(5)
#define ORLIX_TCTI_OP_CBNZ BIT(5)
#define ORLIX_TCTI_OP_LINK BIT(5)
#define ORLIX_TCTI_OP_MOVN BIT(6)
#define ORLIX_TCTI_OP_INVERT BIT(7)

#if defined(__has_attribute)
#if __has_attribute(musttail)
#define ORLIX_TCTI_MUSTTAIL __attribute__((musttail))
#endif
#endif
#ifndef ORLIX_TCTI_MUSTTAIL
#define ORLIX_TCTI_MUSTTAIL
#endif

struct orlix_tcti_exec {
	struct mm_struct *mm;
	struct pt_regs *regs;
	const struct orlix_tcti_gadget_word *cursor;
	const struct orlix_tcti_gadget_word *end;
	unsigned long *fault_address;
	struct orlix_tcti_native_capture *capture;
	u64 code_generation;
	bool authorize;
	bool *entry_valid;
	unsigned long *entry_pc;
	u32 *entry_instruction;
};

struct orlix_tcti_micro_op {
	u32 insn;
	u8 rd;
	u8 rn;
	u8 rt;
	u8 flags;
	u8 access_size;
	u8 result_size;
	u8 condition;
	s64 imm;
};

static int orlix_tcti_gadget_subs_b_cond(struct orlix_tcti_exec *e);

#ifdef CONFIG_ORLIX_TCTI_KUNIT_TEST
static void (*orlix_tcti_pre_authorized_test_hook)(void *);
static void *orlix_tcti_pre_authorized_test_hook_data;

void orlix_tcti_gadget_program_set_pre_authorized_test_hook(
		void (*hook)(void *), void *data)
{
	orlix_tcti_pre_authorized_test_hook = hook;
	orlix_tcti_pre_authorized_test_hook_data = data;
}

bool orlix_tcti_gadget_program_fuses_subs_b_cond(
	const struct orlix_tcti_gadget_word *program, size_t word_count)
{
	return program &&
	       word_count >= ORLIX_TCTI_PROGRAM_WORDS_FOR_INSTRUCTIONS(2) &&
	       program[0].value == (unsigned long)orlix_tcti_gadget_subs_b_cond;
}

static void orlix_tcti_gadget_program_run_pre_authorized_test_hook(void)
{
	void (*hook)(void *) = orlix_tcti_pre_authorized_test_hook;
	void *data = orlix_tcti_pre_authorized_test_hook_data;

	orlix_tcti_pre_authorized_test_hook = NULL;
	orlix_tcti_pre_authorized_test_hook_data = NULL;
	if (hook)
		hook(data);
}
#else
static void orlix_tcti_gadget_program_run_pre_authorized_test_hook(void)
{
}
#endif

static void orlix_tcti_micro_op_from_decoded(
	const struct orlix_tcti_decoded_instruction *decoded,
	struct orlix_tcti_micro_op *op)
{
	u64 imm;

	memset(op, 0, sizeof(*op));
	op->insn = decoded->instruction;
	op->rd = decoded->rd;
	op->rn = decoded->rn;
	op->rt = decoded->rt;
	op->condition = decoded->condition;
	op->access_size = decoded->access_size;
	op->result_size = decoded->result_size;
	if (decoded->is_64bit)
		op->flags |= ORLIX_TCTI_OP_64BIT;
	if (decoded->subtract)
		op->flags |= ORLIX_TCTI_OP_SUBTRACT;
	if (decoded->set_flags)
		op->flags |= ORLIX_TCTI_OP_SET_FLAGS;
	if (decoded->load)
		op->flags |= ORLIX_TCTI_OP_LOAD;
	if (decoded->sign_extend_load)
		op->flags |= ORLIX_TCTI_OP_SIGN_EXT;
	if (decoded->decode_class == ORLIX_TCTI_DECODE_ADD_SUB_IMMEDIATE)
		imm = (u64)decoded->imm12 << (decoded->shift ? 12 : 0);
	else if (decoded->decode_class ==
		 ORLIX_TCTI_DECODE_ADD_SUB_SHIFTED_REGISTER) {
		op->rt = decoded->rm;
		op->flags |= ORLIX_TCTI_OP_REG;
		imm = 0;
	} else if (decoded->decode_class ==
		 ORLIX_TCTI_DECODE_CONDITIONAL_BRANCH_IMMEDIATE)
		imm = (u64)decoded->branch_imm;
	else if (decoded->decode_class ==
		 ORLIX_TCTI_DECODE_COMPARE_BRANCH_IMMEDIATE) {
		if (decoded->nonzero)
			op->flags |= ORLIX_TCTI_OP_CBNZ;
		imm = (u64)decoded->branch_imm;
	} else if (decoded->decode_class ==
		 ORLIX_TCTI_DECODE_UNCONDITIONAL_BRANCH_IMMEDIATE) {
		if (decoded->link)
			op->flags |= ORLIX_TCTI_OP_LINK;
		imm = (u64)decoded->branch_imm;
	} else if (decoded->decode_class ==
		 ORLIX_TCTI_DECODE_MOVE_WIDE_IMMEDIATE) {
		op->access_size = decoded->halfword_shift;
		if (decoded->move_wide_op == ORLIX_TCTI_MOVE_WIDE_MOVK)
			op->flags |= ORLIX_TCTI_OP_MOVK;
		else if (decoded->move_wide_op == ORLIX_TCTI_MOVE_WIDE_MOVN)
			op->flags |= ORLIX_TCTI_OP_MOVN;
		imm = (u64)decoded->imm16 << decoded->halfword_shift;
	} else if (decoded->decode_class ==
		   ORLIX_TCTI_DECODE_LOGICAL_SHIFTED_REGISTER) {
		op->rt = decoded->rm;
		op->access_size = decoded->shift_amount;
		op->result_size = decoded->logical_op;
		op->condition = decoded->shift;
		if (decoded->invert_second_operand)
			op->flags |= ORLIX_TCTI_OP_INVERT;
		imm = 0;
	} else
		imm = (u64)decoded->memory_offset;
	op->imm = (s64)imm;
}

static void orlix_tcti_micro_op_pack(struct orlix_tcti_gadget_word *program,
			       size_t start, orlix_tcti_gadget_fn gadget,
			       const struct orlix_tcti_micro_op *op)
{
	program[start].value = (unsigned long)gadget;
	program[start + 1].value = (unsigned long)op->insn |
		((unsigned long)op->rd << 32) |
		((unsigned long)op->rn << 37) |
		((unsigned long)op->rt << 42) |
		((unsigned long)op->flags << 47);
	program[start + 2].value = (unsigned long)op->access_size |
		((unsigned long)op->result_size << 8) |
		((unsigned long)op->condition << 16);
	program[start + 3].value = (unsigned long)op->imm;
}

static void orlix_tcti_micro_op_unpack(const struct orlix_tcti_gadget_word *words,
				 struct orlix_tcti_micro_op *op)
{
	unsigned long packed = words[0].value;
	unsigned long meta = words[1].value;

	op->insn = (u32)packed;
	op->rd = (packed >> 32) & 0x1fU;
	op->rn = (packed >> 37) & 0x1fU;
	op->rt = (packed >> 42) & 0x1fU;
	op->flags = (packed >> 47) & 0xffU;
	op->access_size = meta & 0xffU;
	op->result_size = (meta >> 8) & 0xffU;
	op->condition = (meta >> 16) & 0xffU;
	op->imm = (s64)words[2].value;
}

static int orlix_tcti_micro_op_take(struct orlix_tcti_exec *e,
			      struct orlix_tcti_micro_op *op)
{
	if (e->cursor + (ORLIX_TCTI_MICRO_OP_WORDS - 1) > e->end)
		return -EINVAL;
	orlix_tcti_micro_op_unpack(e->cursor, op);
	e->cursor += ORLIX_TCTI_MICRO_OP_WORDS - 1;
	return 0;
}

static int orlix_tcti_tail_next(struct orlix_tcti_exec *e)
{
	orlix_tcti_gadget_fn gadget;

	if (e->cursor >= e->end)
		return -EINVAL;
	gadget = (orlix_tcti_gadget_fn)e->cursor->value;
	e->cursor++;
	if (!gadget)
		return -EINVAL;
	ORLIX_TCTI_MUSTTAIL return gadget(e);
}

static void orlix_tcti_note_entry(struct orlix_tcti_exec *e, u32 insn)
{
	if (e->entry_valid && !*e->entry_valid && e->entry_pc &&
	    e->entry_instruction) {
		*e->entry_valid = true;
		*e->entry_pc = e->regs->pc;
		*e->entry_instruction = insn;
	}
}

static u64 orlix_tcti_read_reg_or_sp(const struct pt_regs *regs, u8 reg,
			       u8 access_size)
{
	u64 value = reg == 31 ? regs->sp : regs->regs[reg];

	return access_size == sizeof(u32) ? (u32)value : value;
}

static u64 orlix_tcti_read_reg_or_zr(const struct pt_regs *regs, u8 reg,
			       u8 access_size)
{
	u64 value = reg == 31 ? 0 : regs->regs[reg];

	return access_size == sizeof(u32) ? (u32)value : value;
}

static void orlix_tcti_write_reg_or_sp(struct pt_regs *regs, u8 reg,
				 u8 access_size, u64 value)
{
	if (reg == 31)
		regs->sp = access_size == sizeof(u32) ? (u32)value : value;
	else
		regs->regs[reg] = access_size == sizeof(u32) ? (u32)value : value;
}

static void orlix_tcti_write_reg_or_zr(struct pt_regs *regs, u8 reg,
				 u8 access_size, u64 value)
{
	if (reg == 31)
		return;
	regs->regs[reg] = access_size == sizeof(u32) ? (u32)value : value;
}

static void orlix_tcti_set_add_sub_flags(struct pt_regs *regs, u64 left, u64 right,
				   u64 result, u8 access_size, bool subtract)
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

static bool orlix_tcti_native_condition_passed(const struct pt_regs *regs,
					 u8 condition)
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

static int orlix_tcti_gadget_halt(struct orlix_tcti_exec *e)
{
	(void)e;
	return ORLIX_TCTI_GADGET_DONE;
}

static int orlix_tcti_gadget_execute_decoded(struct orlix_tcti_exec *e)
{
	struct orlix_tcti_micro_op op;
	struct orlix_tcti_decoded_instruction decoded;
	int ret;

	ret = orlix_tcti_micro_op_take(e, &op);
	if (ret)
		return ret;
	orlix_tcti_note_entry(e, op.insn);
	decoded = orlix_tcti_decode_aarch64(op.insn);
	orlix_tcti_native_capture_before_decoded(e->capture, e->mm, e->regs,
					   &decoded);
	ret = orlix_tcti_execute_decoded_semantics(e->mm, e->regs, &decoded,
					     e->fault_address);
	if (ret) {
		orlix_tcti_native_capture_fault(e->capture, &decoded,
					  e->fault_address ? *e->fault_address : 0,
					  ret);
		return ret;
	}
	orlix_tcti_native_capture_after_decoded(e->capture, e->mm, e->regs,
					  &decoded);
	ORLIX_TCTI_MUSTTAIL return orlix_tcti_tail_next(e);
}

static int orlix_tcti_gadget_execute_crc32(struct orlix_tcti_exec *e)
{
	struct orlix_tcti_micro_op op;
	struct orlix_tcti_decoded_instruction decoded;
	int ret;

	ret = orlix_tcti_micro_op_take(e, &op);
	if (ret)
		return ret;
	orlix_tcti_note_entry(e, op.insn);
	decoded = orlix_tcti_decode_aarch64(op.insn);
	ret = orlix_tcti_execute_crc32(e->regs, &decoded);
	if (ret)
		return ret;
	ORLIX_TCTI_MUSTTAIL return orlix_tcti_tail_next(e);
}

static int orlix_tcti_gadget_execute_flag_manipulation(struct orlix_tcti_exec *e)
{
	struct orlix_tcti_micro_op op;
	struct orlix_tcti_decoded_instruction decoded;
	int ret;

	ret = orlix_tcti_micro_op_take(e, &op);
	if (ret)
		return ret;
	orlix_tcti_note_entry(e, op.insn);
	decoded = orlix_tcti_decode_aarch64(op.insn);
	ret = orlix_tcti_execute_flag_manipulation_semantics(e->regs, &decoded);
	if (ret)
		return ret;
	ORLIX_TCTI_MUSTTAIL return orlix_tcti_tail_next(e);
}

static int orlix_tcti_exec_add_sub_imm(struct orlix_tcti_exec *e)
{
	struct orlix_tcti_micro_op op;
	u8 access_size;
	u64 left;
	u64 right;
	u64 result;
	int ret;

	ret = orlix_tcti_micro_op_take(e, &op);
	if (ret)
		return ret;
	orlix_tcti_note_entry(e, op.insn);
	access_size = (op.flags & ORLIX_TCTI_OP_64BIT) ? sizeof(u64) : sizeof(u32);
	if (op.flags & ORLIX_TCTI_OP_REG) {
		left = orlix_tcti_read_reg_or_zr(e->regs, op.rn, access_size);
		right = orlix_tcti_read_reg_or_zr(e->regs, op.rt, access_size);
	} else {
		left = orlix_tcti_read_reg_or_sp(e->regs, op.rn, access_size);
		right = (u64)op.imm;
		if (access_size == sizeof(u32))
			right = (u32)right;
	}
	result = (op.flags & ORLIX_TCTI_OP_SUBTRACT) ? left - right : left + right;
	if (op.flags & ORLIX_TCTI_OP_SET_FLAGS)
		orlix_tcti_set_add_sub_flags(e->regs, left, right, result,
				       access_size,
				       op.flags & ORLIX_TCTI_OP_SUBTRACT);
	if ((op.flags & ORLIX_TCTI_OP_SET_FLAGS) && op.rd == 31) {
		e->regs->pc += sizeof(u32);
		return 0;
	}
	if (op.flags & ORLIX_TCTI_OP_SET_FLAGS)
		orlix_tcti_write_reg_or_zr(e->regs, op.rd, access_size, result);
	else
		orlix_tcti_write_reg_or_sp(e->regs, op.rd, access_size, result);
	e->regs->pc += sizeof(u32);
	return 0;
}

static int orlix_tcti_gadget_add_sub_imm(struct orlix_tcti_exec *e)
{
	int ret = orlix_tcti_exec_add_sub_imm(e);

	if (ret)
		return ret;
	ORLIX_TCTI_MUSTTAIL return orlix_tcti_tail_next(e);
}

static int orlix_tcti_exec_b_cond(struct orlix_tcti_exec *e)
{
	struct orlix_tcti_micro_op op;
	int ret;

	ret = orlix_tcti_micro_op_take(e, &op);
	if (ret)
		return ret;
	orlix_tcti_note_entry(e, op.insn);
	if (orlix_tcti_native_condition_passed(e->regs, op.condition))
		e->regs->pc += op.imm;
	else
		e->regs->pc += sizeof(u32);
	return 0;
}

static int orlix_tcti_gadget_b_cond(struct orlix_tcti_exec *e)
{
	int ret = orlix_tcti_exec_b_cond(e);

	if (ret)
		return ret;
	ORLIX_TCTI_MUSTTAIL return orlix_tcti_tail_next(e);
}

static int orlix_tcti_gadget_move_wide(struct orlix_tcti_exec *e)
{
	struct orlix_tcti_micro_op op;
	u8 access_size;
	u64 result;
	int ret;

	ret = orlix_tcti_micro_op_take(e, &op);
	if (ret)
		return ret;
	orlix_tcti_note_entry(e, op.insn);
	access_size = (op.flags & ORLIX_TCTI_OP_64BIT) ? sizeof(u64) : sizeof(u32);
	if (op.flags & ORLIX_TCTI_OP_MOVK) {
		u64 mask = 0xffffULL << op.access_size;

		result = orlix_tcti_read_reg_or_zr(e->regs, op.rd, access_size);
		result = (result & ~mask) | (u64)op.imm;
	} else if (op.flags & ORLIX_TCTI_OP_MOVN)
		result = ~(u64)op.imm;
	else
		result = (u64)op.imm;
	orlix_tcti_write_reg_or_zr(e->regs, op.rd, access_size, result);
	e->regs->pc += sizeof(u32);
	ORLIX_TCTI_MUSTTAIL return orlix_tcti_tail_next(e);
}

static u64 orlix_tcti_shift_logical_value(u64 value, u8 shift, u8 amount,
				     bool is_64bit)
{
	if (is_64bit) {
		switch (shift) {
		case 0:
			return value << amount;
		case 1:
			return value >> amount;
		case 2:
			return (u64)((s64)value >> amount);
		case 3:
			return ror64(value, amount);
		}
		return value;
	}
	value = (u32)value;
	switch (shift) {
	case 0:
		return (u32)(value << amount);
	case 1:
		return (u32)value >> amount;
	case 2:
		return (u32)((s32)value >> amount);
	case 3:
		return ror32(value, amount);
	}
	return value;
}

static int orlix_tcti_gadget_logical_shifted_register(struct orlix_tcti_exec *e)
{
	struct orlix_tcti_micro_op op;
	u8 access_size;
	u64 left;
	u64 right;
	u64 result;
	u64 mask;
	u64 sign_bit;
	int ret;

	ret = orlix_tcti_micro_op_take(e, &op);
	if (ret)
		return ret;
	orlix_tcti_note_entry(e, op.insn);
	access_size = (op.flags & ORLIX_TCTI_OP_64BIT) ? sizeof(u64) : sizeof(u32);
	mask = access_size == sizeof(u32) ? U32_MAX : U64_MAX;
	sign_bit = access_size == sizeof(u32) ? BIT_ULL(31) : BIT_ULL(63);
	left = orlix_tcti_read_reg_or_zr(e->regs, op.rn, access_size);
	right = orlix_tcti_shift_logical_value(
		orlix_tcti_read_reg_or_zr(e->regs, op.rt, access_size),
		op.condition, op.access_size, access_size == sizeof(u64));
	if (op.flags & ORLIX_TCTI_OP_INVERT)
		right = ~right;
	switch (op.result_size) {
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
	if (op.flags & ORLIX_TCTI_OP_SET_FLAGS) {
		e->regs->pstate &= ~(PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT);
		if (result & sign_bit)
			e->regs->pstate |= PSR_N_BIT;
		if (!result)
			e->regs->pstate |= PSR_Z_BIT;
	}
	if (!(op.flags & ORLIX_TCTI_OP_SET_FLAGS) || op.rd != 31)
		orlix_tcti_write_reg_or_zr(e->regs, op.rd, access_size, result);
	e->regs->pc += sizeof(u32);
	ORLIX_TCTI_MUSTTAIL return orlix_tcti_tail_next(e);
}

static int orlix_tcti_gadget_cbz(struct orlix_tcti_exec *e)
{
	struct orlix_tcti_micro_op op;
	u8 access_size;
	u64 value;
	int ret;

	ret = orlix_tcti_micro_op_take(e, &op);
	if (ret)
		return ret;
	orlix_tcti_note_entry(e, op.insn);
	access_size = (op.flags & ORLIX_TCTI_OP_64BIT) ? sizeof(u64) : sizeof(u32);
	value = orlix_tcti_read_reg_or_zr(e->regs, op.rt, access_size);
	if ((!value) != !!(op.flags & ORLIX_TCTI_OP_CBNZ))
		e->regs->pc += op.imm;
	else
		e->regs->pc += sizeof(u32);
	ORLIX_TCTI_MUSTTAIL return orlix_tcti_tail_next(e);
}

static int orlix_tcti_gadget_b_imm(struct orlix_tcti_exec *e)
{
	struct orlix_tcti_micro_op op;
	int ret;

	ret = orlix_tcti_micro_op_take(e, &op);
	if (ret)
		return ret;
	orlix_tcti_note_entry(e, op.insn);
	if (op.flags & ORLIX_TCTI_OP_LINK)
		e->regs->regs[30] = e->regs->pc + sizeof(u32);
	e->regs->pc += op.imm;
	ORLIX_TCTI_MUSTTAIL return orlix_tcti_tail_next(e);
}

static int orlix_tcti_gadget_ldr_str_unsigned(struct orlix_tcti_exec *e)
{
	struct orlix_tcti_micro_op op;
	unsigned long address;
	u8 buffer[sizeof(u64)] = {};
	u64 value;
	int ret;

	ret = orlix_tcti_micro_op_take(e, &op);
	if (ret)
		return ret;
	orlix_tcti_note_entry(e, op.insn);
	if (e->authorize &&
	    e->code_generation != orlix_tcti_code_generation(e->mm))
		return -ESTALE;
	if (!e->mm)
		return -EINVAL;
	address = orlix_tcti_read_reg_or_sp(e->regs, op.rn, sizeof(u64)) +
		  (unsigned long)op.imm;
	if (e->fault_address)
		*e->fault_address = address;
	if (op.flags & ORLIX_TCTI_OP_LOAD) {
		ret = orlix_tcti_read_user_data(e->mm, address, buffer,
					  op.access_size);
		if (ret)
			return ret;
		switch (op.access_size) {
		case sizeof(u8):
			value = buffer[0];
			break;
		case sizeof(u16):
			value = get_unaligned_le16(buffer);
			break;
		case sizeof(u32):
			value = get_unaligned_le32(buffer);
			break;
		case sizeof(u64):
			value = get_unaligned_le64(buffer);
			break;
		default:
			return -EINVAL;
		}
		if (op.flags & ORLIX_TCTI_OP_SIGN_EXT) {
			if (op.access_size == sizeof(u8))
				value = (u64)(s64)(s8)value;
			else if (op.access_size == sizeof(u16))
				value = (u64)(s64)(s16)value;
			else if (op.access_size == sizeof(u32))
				value = (u64)(s64)(s32)value;
		}
		orlix_tcti_write_reg_or_zr(e->regs, op.rt, op.result_size, value);
	} else {
		value = orlix_tcti_read_reg_or_zr(e->regs, op.rt, op.access_size);
		switch (op.access_size) {
		case sizeof(u8):
			buffer[0] = (u8)value;
			break;
		case sizeof(u16):
			put_unaligned_le16((u16)value, buffer);
			break;
		case sizeof(u32):
			put_unaligned_le32((u32)value, buffer);
			break;
		case sizeof(u64):
			put_unaligned_le64(value, buffer);
			break;
		default:
			return -EINVAL;
		}
		ret = orlix_tcti_write_user_data(e->mm, address, buffer,
					   op.access_size);
		if (ret)
			return ret;
		if (current) {
			current->thread.user_exclusive_address = 0;
			current->thread.user_exclusive_value = 0;
			current->thread.user_exclusive_value2 = 0;
			current->thread.user_exclusive_pfn = 0;
		}
	}
	e->regs->pc += sizeof(u32);
	ORLIX_TCTI_MUSTTAIL return orlix_tcti_tail_next(e);
}

static int orlix_tcti_gadget_subs_b_cond(struct orlix_tcti_exec *e)
{
	int ret;

	ret = orlix_tcti_exec_add_sub_imm(e);
	if (ret)
		return ret;
	if (e->cursor >= e->end)
		return -EINVAL;
	e->cursor++;
	ret = orlix_tcti_exec_b_cond(e);
	if (ret)
		return ret;
	ORLIX_TCTI_MUSTTAIL return orlix_tcti_tail_next(e);
}

static orlix_tcti_gadget_fn orlix_tcti_gadget_for_decoded(
	const struct orlix_tcti_decoded_instruction *decoded)
{
	if (decoded->decode_class == ORLIX_TCTI_DECODE_ADD_SUB_IMMEDIATE)
		return orlix_tcti_gadget_add_sub_imm;
	if (decoded->decode_class == ORLIX_TCTI_DECODE_ADD_SUB_SHIFTED_REGISTER &&
	    !decoded->shift_amount)
		return orlix_tcti_gadget_add_sub_imm;
	if (decoded->decode_class == ORLIX_TCTI_DECODE_CONDITIONAL_BRANCH_IMMEDIATE)
		return orlix_tcti_gadget_b_cond;
	if (decoded->decode_class == ORLIX_TCTI_DECODE_COMPARE_BRANCH_IMMEDIATE)
		return orlix_tcti_gadget_cbz;
	if (decoded->decode_class ==
	    ORLIX_TCTI_DECODE_UNCONDITIONAL_BRANCH_IMMEDIATE)
		return orlix_tcti_gadget_b_imm;
	if (decoded->decode_class == ORLIX_TCTI_DECODE_MOVE_WIDE_IMMEDIATE)
		return orlix_tcti_gadget_move_wide;
	if (decoded->decode_class == ORLIX_TCTI_DECODE_LOGICAL_SHIFTED_REGISTER)
		return orlix_tcti_gadget_logical_shifted_register;
	if (decoded->decode_class ==
		    ORLIX_TCTI_DECODE_LOAD_STORE_UNSIGNED_IMMEDIATE &&
	    !decoded->simd_fp && !decoded->prefetch)
		return orlix_tcti_gadget_ldr_str_unsigned;
	if (decoded->decode_class == ORLIX_TCTI_DECODE_FLAG_MANIPULATION)
		return orlix_tcti_gadget_execute_flag_manipulation;
	if (decoded->decode_class == ORLIX_TCTI_DECODE_DATA_PROCESSING_2SOURCE &&
	    (decoded->dp2_op == ORLIX_TCTI_DP2_CRC32 ||
	     decoded->dp2_op == ORLIX_TCTI_DP2_CRC32C))
		return orlix_tcti_gadget_execute_crc32;
	return orlix_tcti_gadget_execute_decoded;
}

enum orlix_tcti_gadget_program_kind orlix_tcti_gadget_program_first_kind(
	const struct orlix_tcti_gadget_word *program, size_t word_count)
{
	if (!program || word_count < ORLIX_TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS)
		return ORLIX_TCTI_GADGET_PROGRAM_GENERIC;
	return program[0].value == (unsigned long)orlix_tcti_gadget_execute_crc32 ?
		ORLIX_TCTI_GADGET_PROGRAM_CRC32 : ORLIX_TCTI_GADGET_PROGRAM_GENERIC;
}

u32 orlix_tcti_program_instruction_at(
	const struct orlix_tcti_gadget_word *program, size_t word_count,
	u32 index)
{
	size_t offset = (size_t)index * ORLIX_TCTI_MICRO_OP_WORDS;

	if (!program || offset + 1 >= word_count)
		return 0;
	return (u32)program[offset + 1].value;
}

int orlix_tcti_lower_decoded_instruction(
	const struct orlix_tcti_decoded_instruction *decoded,
	struct orlix_tcti_gadget_word *program,
	size_t capacity,
	size_t *word_count)
{
	int ret;

	if (!decoded || !program || !word_count)
		return -EINVAL;
	if (decoded->decode_class == ORLIX_TCTI_DECODE_UNSUPPORTED ||
	    decoded->decode_class == ORLIX_TCTI_DECODE_SVC ||
	    decoded->decode_class == ORLIX_TCTI_DECODE_BRK ||
	    decoded->decode_class == ORLIX_TCTI_DECODE_HLT)
		return -EOPNOTSUPP;

	*word_count = 0;
	ret = orlix_tcti_append_decoded_instruction(decoded, program, capacity,
					      word_count);
	if (ret)
		return ret;

	return 0;
}

int orlix_tcti_append_decoded_instruction(
	const struct orlix_tcti_decoded_instruction *decoded,
	struct orlix_tcti_gadget_word *program,
	size_t capacity,
	size_t *word_count)
{
	struct orlix_tcti_micro_op op;
	orlix_tcti_gadget_fn gadget;
	size_t start;
	size_t words;

	if (!decoded || !program || !word_count)
		return -EINVAL;
	if (decoded->decode_class == ORLIX_TCTI_DECODE_UNSUPPORTED ||
	    decoded->decode_class == ORLIX_TCTI_DECODE_SVC ||
	    decoded->decode_class == ORLIX_TCTI_DECODE_BRK)
		return -EOPNOTSUPP;

	orlix_tcti_micro_op_from_decoded(decoded, &op);
	gadget = orlix_tcti_gadget_for_decoded(decoded);
	start = *word_count ? *word_count - 1 : 0;
	if (*word_count >= ORLIX_TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS &&
	    decoded->decode_class ==
		    ORLIX_TCTI_DECODE_CONDITIONAL_BRANCH_IMMEDIATE &&
	    program[start - ORLIX_TCTI_MICRO_OP_WORDS].value ==
		    (unsigned long)orlix_tcti_gadget_add_sub_imm) {
		struct orlix_tcti_micro_op prev;

		orlix_tcti_micro_op_unpack(
			&program[start - ORLIX_TCTI_MICRO_OP_WORDS + 1], &prev);
		if (prev.flags & ORLIX_TCTI_OP_SET_FLAGS)
			program[start - ORLIX_TCTI_MICRO_OP_WORDS].value =
				(unsigned long)orlix_tcti_gadget_subs_b_cond;
	}

	words = start + ORLIX_TCTI_MICRO_OP_WORDS + 1;
	if (capacity < words)
		return -ENOSPC;
	orlix_tcti_micro_op_pack(program, start, gadget, &op);
	program[start + ORLIX_TCTI_MICRO_OP_WORDS].value =
		(unsigned long)orlix_tcti_gadget_halt;
	*word_count = words;
	return 0;
}

static int orlix_tcti_execute_gadget_program_checked(
	struct mm_struct *mm, struct pt_regs *regs,
	const struct orlix_tcti_gadget_word *program, size_t word_count,
	unsigned long *fault_address, bool authorize, u64 code_generation,
	bool *entry_valid, unsigned long *entry_pc, u32 *entry_instruction,
	struct orlix_tcti_native_capture *capture)
{
	struct orlix_tcti_exec exec = {
		.mm = mm,
		.regs = regs,
		.cursor = program,
		.end = program + word_count,
		.fault_address = fault_address,
		.capture = capture,
		.code_generation = code_generation,
		.authorize = authorize,
		.entry_valid = entry_valid,
		.entry_pc = entry_pc,
		.entry_instruction = entry_instruction,
	};
	orlix_tcti_gadget_fn gadget;
	int ret;

	if (!regs || !program || !word_count)
		return -EINVAL;
	if (authorize && !mm)
		return -EINVAL;
	if (authorize) {
		orlix_tcti_gadget_program_run_pre_authorized_test_hook();
		if (code_generation != orlix_tcti_code_generation(mm))
			return -ESTALE;
	}
	gadget = (orlix_tcti_gadget_fn)exec.cursor->value;
	exec.cursor++;
	if (!gadget)
		return -EINVAL;
	ret = gadget(&exec);
	if (ret == ORLIX_TCTI_GADGET_DONE)
		return 0;
	return ret;
}

int orlix_tcti_execute_gadget_program(struct mm_struct *mm, struct pt_regs *regs,
				const struct orlix_tcti_gadget_word *program,
				size_t word_count,
				unsigned long *fault_address)
{
	return orlix_tcti_execute_gadget_program_checked(mm, regs, program, word_count,
						  fault_address, false, 0,
					  NULL, NULL, NULL, NULL);
}

int orlix_tcti_execute_gadget_program_authorized(
	struct mm_struct *mm, struct pt_regs *regs,
	const struct orlix_tcti_gadget_word *program, size_t word_count,
	unsigned long *fault_address, u64 code_generation)
{
	return orlix_tcti_execute_gadget_program_checked(
		mm, regs, program, word_count, fault_address, true,
		code_generation, NULL, NULL, NULL, NULL);
}

int orlix_tcti_execute_gadget_program_authorized_observed(
	struct mm_struct *mm, struct pt_regs *regs,
	const struct orlix_tcti_gadget_word *program, size_t word_count,
	unsigned long *fault_address, u64 code_generation, bool *entry_valid,
	unsigned long *entry_pc, u32 *entry_instruction)
{
	if (!entry_valid || !entry_pc || !entry_instruction)
		return -EINVAL;
	return orlix_tcti_execute_gadget_program_checked(
		mm, regs, program, word_count, fault_address, true,
		code_generation, entry_valid, entry_pc, entry_instruction, NULL);
}

int orlix_tcti_execute_gadget_program_authorized_captured(
	struct mm_struct *mm, struct pt_regs *regs,
	const struct orlix_tcti_gadget_word *program, size_t word_count,
	unsigned long *fault_address, u64 code_generation, bool *entry_valid,
	unsigned long *entry_pc, u32 *entry_instruction,
	struct orlix_tcti_native_capture *capture)
{
	if (!entry_valid || !entry_pc || !entry_instruction || !capture)
		return -EINVAL;
	return orlix_tcti_execute_gadget_program_checked(
		mm, regs, program, word_count, fault_address, true, code_generation,
		entry_valid, entry_pc, entry_instruction, capture);
}
