// SPDX-License-Identifier: GPL-2.0-only
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <asm/ptrace.h>

#include "pointer_authentication.h"
#include "semantics.h"

/* Fixed-width PAUTH core owned by this production unit. */
/*
 * Pointer authentication belongs to the fixed OrlixTCTI semantic module so
 * both Kbuild and the app product compile adapter execute the same code.
 */
#define ORLIX_TCTI_PAUTH_LOW_ADDRESS_MASK GENMASK_ULL(47, 0)
#define ORLIX_TCTI_PAUTH_HIGH_PAC_MASK GENMASK_ULL(63, 56)
#define ORLIX_TCTI_PAUTH_LOW_PAC_MASK GENMASK_ULL(54, 48)

static u8 orlix_tcti_pauth_nibble(u64 value, unsigned int index)
{
	return (value >> (index * 4)) & 0xfU;
}

static u64 orlix_tcti_pauth_set_nibble(u64 value, unsigned int index, u8 cell)
{
	u64 shift = index * 4;

	return (value & ~(0xfULL << shift)) | ((u64)(cell & 0xfU) << shift);
}

static u8 orlix_tcti_pauth_rol4(u8 value, unsigned int amount)
{
	return ((value << amount) | (value >> (4 - amount))) & 0xfU;
}

static u64 orlix_tcti_pauth_permute(u64 input, const u8 permutation[16])
{
	u64 output = 0;
	unsigned int index;

	for (index = 0; index < 16; index++)
		output = orlix_tcti_pauth_set_nibble(output, index,
			orlix_tcti_pauth_nibble(input, permutation[index]));
	return output;
}

static u64 orlix_tcti_pauth_substitute(u64 input, const u8 table[16])
{
	u64 output = 0;
	unsigned int index;

	for (index = 0; index < 16; index++)
		output = orlix_tcti_pauth_set_nibble(output, index,
			table[orlix_tcti_pauth_nibble(input, index)]);
	return output;
}

static u64 orlix_tcti_pauth_cell_shuffle(u64 input)
{
	static const u8 permutation[16] = {
		13, 6, 11, 0, 7, 12, 1, 10, 8, 3, 14, 5, 2, 9, 4, 15,
	};

	return orlix_tcti_pauth_permute(input, permutation);
}

static u64 orlix_tcti_pauth_cell_inverse_shuffle(u64 input)
{
	static const u8 permutation[16] = {
		3, 6, 12, 9, 14, 11, 1, 4, 8, 13, 7, 2, 5, 0, 10, 15,
	};

	return orlix_tcti_pauth_permute(input, permutation);
}

static u64 orlix_tcti_pauth_sub(u64 input)
{
	static const u8 table[16] = {
		0xb, 0x6, 0x8, 0xf, 0xc, 0x0, 0x9, 0xe,
		0x3, 0x7, 0x4, 0x5, 0xd, 0x2, 0x1, 0xa,
	};

	return orlix_tcti_pauth_substitute(input, table);
}

static u64 orlix_tcti_pauth_inverse_sub(u64 input)
{
	static const u8 table[16] = {
		0x5, 0xe, 0xd, 0x8, 0xa, 0xb, 0x1, 0x9,
		0x2, 0x6, 0xf, 0x0, 0x4, 0xc, 0x7, 0x3,
	};

	return orlix_tcti_pauth_substitute(input, table);
}

static u64 orlix_tcti_pauth_mult(u64 input)
{
	u64 output = 0;
	unsigned int index;

	for (index = 0; index < 4; index++) {
		u8 c0 = orlix_tcti_pauth_nibble(input, index);
		u8 c1 = orlix_tcti_pauth_nibble(input, index + 4);
		u8 c2 = orlix_tcti_pauth_nibble(input, index + 8);
		u8 c3 = orlix_tcti_pauth_nibble(input, index + 12);
		u8 t0 = orlix_tcti_pauth_rol4(c2, 1) ^
			orlix_tcti_pauth_rol4(c1, 2) ^
			orlix_tcti_pauth_rol4(c0, 1);
		u8 t1 = orlix_tcti_pauth_rol4(c3, 1) ^
			orlix_tcti_pauth_rol4(c1, 1) ^
			orlix_tcti_pauth_rol4(c0, 2);
		u8 t2 = orlix_tcti_pauth_rol4(c3, 2) ^
			orlix_tcti_pauth_rol4(c2, 1) ^
			orlix_tcti_pauth_rol4(c0, 1);
		u8 t3 = orlix_tcti_pauth_rol4(c3, 1) ^
			orlix_tcti_pauth_rol4(c2, 2) ^
			orlix_tcti_pauth_rol4(c1, 1);

		output = orlix_tcti_pauth_set_nibble(output, index, t3);
		output = orlix_tcti_pauth_set_nibble(output, index + 4, t2);
		output = orlix_tcti_pauth_set_nibble(output, index + 8, t1);
		output = orlix_tcti_pauth_set_nibble(output, index + 12, t0);
	}
	return output;
}

static u8 orlix_tcti_pauth_tweak_rotate(u8 input)
{
	return (((((input >> 0) ^ (input >> 1)) & 1U) << 3) |
		(((input >> 3) & 1U) << 2) |
		(((input >> 2) & 1U) << 1) |
		((input >> 1) & 1U));
}

static u8 orlix_tcti_pauth_tweak_inverse_rotate(u8 input)
{
	return ((((input >> 2) & 1U) << 3) |
		(((input >> 1) & 1U) << 2) |
		(((input >> 0) & 1U) << 1) |
		(((input >> 0) ^ (input >> 3)) & 1U));
}

static u64 orlix_tcti_pauth_tweak_shuffle(u64 input)
{
	static const u8 permutation[16] = {
		4, 5, 6, 7, 11, 2, 3, 8, 12, 13, 14, 15, 0, 1, 10, 9,
	};
	static const u16 rotate_mask = BIT(2) | BIT(4) | BIT(7) | BIT(11) |
		BIT(12) | BIT(14) | BIT(15);
	u64 output = 0;
	unsigned int index;

	for (index = 0; index < 16; index++) {
		u8 cell = orlix_tcti_pauth_nibble(input, permutation[index]);

		if (rotate_mask & BIT(index))
			cell = orlix_tcti_pauth_tweak_rotate(cell);
		output = orlix_tcti_pauth_set_nibble(output, index, cell);
	}
	return output;
}

static u64 orlix_tcti_pauth_tweak_inverse_shuffle(u64 input)
{
	static const u8 permutation[16] = {
		12, 13, 5, 6, 0, 1, 2, 3, 7, 15, 14, 4, 8, 9, 10, 11,
	};
	static const u16 rotate_mask = BIT(0) | BIT(6) | BIT(8) | BIT(9) |
		BIT(10) | BIT(11) | BIT(15);
	u64 output = 0;
	unsigned int index;

	for (index = 0; index < 16; index++) {
		u8 cell = orlix_tcti_pauth_nibble(input, permutation[index]);

		if (rotate_mask & BIT(index))
			cell = orlix_tcti_pauth_tweak_inverse_rotate(cell);
		output = orlix_tcti_pauth_set_nibble(output, index, cell);
	}
	return output;
}

u64 orlix_tcti_pauth_compute_qarma5(u64 data, u64 modifier,
				    const struct orlix_tcti_pauth_key *key)
{
	static const u64 round_constant[5] = {
		0x0000000000000000ULL, 0x13198a2e03707344ULL,
		0xa4093822299f31d0ULL, 0x082efa98ec4e6c89ULL,
		0x452821e638d01377ULL,
	};
	const u64 alpha = 0xc0ac29b7c97c50ddULL;
	u64 modified_key0;
	u64 running_modifier = modifier;
	u64 working = data ^ key->high;
	unsigned int round;

	modified_key0 = (key->high << 63) |
		((key->high >> 1) ^ (key->high >> 63));

	for (round = 0; round <= 4; round++) {
		working ^= key->low ^ running_modifier ^ round_constant[round];
		if (round > 0)
			working = orlix_tcti_pauth_mult(
				orlix_tcti_pauth_cell_shuffle(working));
		working = orlix_tcti_pauth_sub(working);
		running_modifier = orlix_tcti_pauth_tweak_shuffle(running_modifier);
	}

	working ^= modified_key0 ^ running_modifier;
	working = orlix_tcti_pauth_mult(orlix_tcti_pauth_cell_shuffle(working));
	working = orlix_tcti_pauth_sub(working);
	working = orlix_tcti_pauth_mult(orlix_tcti_pauth_cell_shuffle(working));
	working ^= key->low;
	working = orlix_tcti_pauth_cell_inverse_shuffle(working);
	working = orlix_tcti_pauth_inverse_sub(working);
	working = orlix_tcti_pauth_cell_inverse_shuffle(
		orlix_tcti_pauth_mult(working));
	working ^= key->high ^ running_modifier;

	for (round = 0; round <= 4; round++) {
		working = orlix_tcti_pauth_inverse_sub(working);
		if (round < 4)
			working = orlix_tcti_pauth_cell_inverse_shuffle(
				orlix_tcti_pauth_mult(working));
		running_modifier =
			orlix_tcti_pauth_tweak_inverse_shuffle(running_modifier);
		working ^= round_constant[4 - round] ^ key->low ^
			running_modifier ^ alpha;
	}

	return working ^ modified_key0;
}

u64 orlix_tcti_pauth_compute_qarma5_two_modifiers(
	u64 data, u64 modifier1, u64 modifier2,
	const struct orlix_tcti_pauth_key *key)
{
	u64 combined = ((modifier2 >> 5) & 0xffffffffULL) << 32;

	combined |= (modifier1 >> 4) & 0xffffffffULL;
	return orlix_tcti_pauth_compute_qarma5(data, combined, key);
}

static u64 orlix_tcti_pauth_canonicalize(u64 pointer, unsigned int select_bit)
{
	u64 result = pointer & ORLIX_TCTI_PAUTH_LOW_ADDRESS_MASK;

	if (pointer & BIT_ULL(select_bit))
		result |= ~ORLIX_TCTI_PAUTH_LOW_ADDRESS_MASK;
	return result;
}

u64 orlix_tcti_pauth_add(u64 pointer, u64 modifier, u64 modifier2,
			 bool use_modifier2,
			 const struct orlix_tcti_pauth_key *key)
{
	bool upper = pointer & BIT_ULL(63);
	u64 extended = orlix_tcti_pauth_canonicalize(pointer, 63);
	u64 pac = use_modifier2 ?
		orlix_tcti_pauth_compute_qarma5_two_modifiers(
			extended, modifier, modifier2, key) :
		orlix_tcti_pauth_compute_qarma5(extended, modifier, key);
	u64 extension = pointer >> ORLIX_TCTI_PAUTH_BOTTOM_BIT;

	if (extension != 0 && extension != GENMASK_ULL(15, 0))
		pac ^= BIT_ULL(62);

	return (pac & ORLIX_TCTI_PAUTH_HIGH_PAC_MASK) |
		(upper ? BIT_ULL(55) : 0) |
		(pac & ORLIX_TCTI_PAUTH_LOW_PAC_MASK) |
		(pointer & ORLIX_TCTI_PAUTH_LOW_ADDRESS_MASK);
}

u64 orlix_tcti_pauth_authenticate(u64 pointer, u64 modifier, u64 modifier2,
				  bool use_modifier2, bool key_b,
				  const struct orlix_tcti_pauth_key *key)
{
	u64 original = orlix_tcti_pauth_canonicalize(pointer, 55);
	u64 pac = use_modifier2 ?
		orlix_tcti_pauth_compute_qarma5_two_modifiers(
			original, modifier, modifier2, key) :
		orlix_tcti_pauth_compute_qarma5(original, modifier, key);

	if ((pac & ORLIX_TCTI_PAUTH_HIGH_PAC_MASK) ==
		(pointer & ORLIX_TCTI_PAUTH_HIGH_PAC_MASK) &&
	    (pac & ORLIX_TCTI_PAUTH_LOW_PAC_MASK) ==
		(pointer & ORLIX_TCTI_PAUTH_LOW_PAC_MASK))
		return original;

	/* Legacy FEAT_PAuth poisoning: key A writes 01, key B writes 10. */
	original &= ~GENMASK_ULL(62, 61);
	original |= (key_b ? 2ULL : 1ULL) << 61;
	return original;
}

u64 orlix_tcti_pauth_strip(u64 pointer)
{
	return orlix_tcti_pauth_canonicalize(pointer, 55);
}

void orlix_tcti_pauth_randomize_task(struct task_struct *task)
{
	if (!task)
		return;
	get_random_bytes(&task->thread.user_pauth,
			 offsetof(struct orlix_tcti_pauth_state, pacm));
	task->thread.user_pauth.pacm = false;
}

void orlix_tcti_pauth_copy_task(struct task_struct *destination,
				const struct task_struct *source)
{
	if (destination && source)
		destination->thread.user_pauth = source->thread.user_pauth;
}

void orlix_tcti_pauth_clear_task(struct task_struct *task)
{
	if (task)
		memset(&task->thread.user_pauth, 0,
		       sizeof(task->thread.user_pauth));
}

static const struct orlix_tcti_pauth_key *pauth_key(enum orlix_tcti_pauth_key_select key)
{
	switch (key) {
	case ORLIX_TCTI_PAUTH_KEY_APIA: return &current->thread.user_pauth.apia;
	case ORLIX_TCTI_PAUTH_KEY_APIB: return &current->thread.user_pauth.apib;
	case ORLIX_TCTI_PAUTH_KEY_APDA: return &current->thread.user_pauth.apda;
	case ORLIX_TCTI_PAUTH_KEY_APDB: return &current->thread.user_pauth.apdb;
	case ORLIX_TCTI_PAUTH_KEY_APGA: return &current->thread.user_pauth.apga;
	default: return NULL;
	}
}

static u64 pauth_gpr(const struct pt_regs *regs, u8 reg)
{
	return reg == 31 ? 0 : regs->regs[reg];
}

static void pauth_write_gpr(struct pt_regs *regs, u8 reg, u64 value)
{
	if (reg != 31)
		regs->regs[reg] = value;
}

static u64 pauth_modifier(const struct pt_regs *regs,
			  const struct orlix_tcti_decoded_instruction *decoded)
{
	if (decoded->pauth_modifier_zero)
		return 0;
	if (decoded->pauth_modifier_is_sp)
		return regs->sp;
	return pauth_gpr(regs, decoded->pauth_modifier_reg_is_rm ? decoded->rm : decoded->rn);
}

static u64 pauth_modifier2(const struct pt_regs *regs,
			   const struct orlix_tcti_decoded_instruction *decoded)
{
	if (decoded->pauth_modifier2_is_pc)
		return decoded->pauth_modifier2_pc_relative ?
			regs->pc - ((u64)decoded->imm16 << 2) : regs->pc;
	return pauth_gpr(regs, decoded->rm);
}

int orlix_tcti_execute_pointer_authentication(
	struct mm_struct *mm, struct pt_regs *regs,
	const struct orlix_tcti_decoded_instruction *decoded,
	unsigned long *fault_address)
{
	const struct orlix_tcti_pauth_key *key;
	u64 modifier, modifier2 = 0, pointer, result;
	bool key_b, use_modifier2;

	if (!mm || !regs || !decoded)
		return -EINVAL;
	if (!user_mode(regs))
		return -EOPNOTSUPP;
	key = pauth_key(decoded->pauth_key);
	if (!key)
		return -EINVAL;
	key_b = decoded->pauth_key == ORLIX_TCTI_PAUTH_KEY_APIB ||
		decoded->pauth_key == ORLIX_TCTI_PAUTH_KEY_APDB;
	use_modifier2 = decoded->pauth_use_modifier2 ||
		(decoded->pauth_pacm_modifier2 && current->thread.user_pauth.pacm);
	modifier = pauth_modifier(regs, decoded);
	if (use_modifier2)
		modifier2 = pauth_modifier2(regs, decoded);

	switch (decoded->pauth_op) {
	case ORLIX_TCTI_PAUTH_ADD:
		pointer = pauth_gpr(regs, decoded->rd);
		result = orlix_tcti_pauth_add(pointer, modifier, modifier2, use_modifier2, key);
		pauth_write_gpr(regs, decoded->rd, result);
		break;
	case ORLIX_TCTI_PAUTH_AUTHENTICATE:
		pointer = pauth_gpr(regs, decoded->rd);
		result = orlix_tcti_pauth_authenticate(pointer, modifier, modifier2,
			use_modifier2, key_b, key);
		pauth_write_gpr(regs, decoded->rd, result);
		break;
	case ORLIX_TCTI_PAUTH_STRIP:
		pauth_write_gpr(regs, decoded->rd, orlix_tcti_pauth_strip(pauth_gpr(regs, decoded->rd)));
		break;
	case ORLIX_TCTI_PAUTH_GENERIC:
		result = orlix_tcti_pauth_compute_qarma5(pauth_gpr(regs, decoded->rn), modifier, key);
		pauth_write_gpr(regs, decoded->rd, result & GENMASK_ULL(63, 32));
		break;
	case ORLIX_TCTI_PAUTH_BRANCH:
		pointer = decoded->pauth_return ? regs->regs[30] : pauth_gpr(regs, decoded->rn);
		result = orlix_tcti_pauth_authenticate(pointer, modifier, modifier2,
			use_modifier2, key_b, key);
		if (decoded->link)
			regs->regs[30] = regs->pc + sizeof(u32);
		regs->pc = result;
		return 0;
	case ORLIX_TCTI_PAUTH_LOAD: {
		u64 address = decoded->rn == 31 ? regs->sp : regs->regs[decoded->rn];
		u64 value;
		int ret;

		address = orlix_tcti_pauth_authenticate(address, 0, 0, false, key_b, key);
		address += decoded->memory_offset;
		ret = orlix_tcti_read_user_data(mm, address, &value, sizeof(value));
		if (ret) {
			if (fault_address)
				*fault_address = address;
			return ret;
		}
		pauth_write_gpr(regs, decoded->rt, value);
		if (decoded->memory_index_mode == ORLIX_TCTI_MEMORY_INDEX_PRE) {
			if (decoded->rn == 31)
				regs->sp = address;
			else
				regs->regs[decoded->rn] = address;
		}
		break;
	}
	case ORLIX_TCTI_PAUTH_SET_PACM:
		current->thread.user_pauth.pacm = true;
		break;
	default:
		return -EOPNOTSUPP;
	}
	regs->pc += sizeof(u32);
	return 0;
}
