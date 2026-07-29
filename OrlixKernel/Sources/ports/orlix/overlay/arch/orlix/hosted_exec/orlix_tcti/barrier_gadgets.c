/* SPDX-License-Identifier: GPL-2.0-only */
#include <asm/orlix_tcti.h>
#include <asm/ptrace.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/string.h>

#include "barrier_gadgets.h"
#include "decode_aarch64.h"

/*
 * Fixed AArch64 entries, following the immutable-entrypoint pattern observed
 * in OpenMinis ish-arm64 control.S/gadgets.h at de124dd66124a15239cea1465164f74980ada245.
 * They are OrlixTCTI-owned templates, not imported upstream runtime code.
 */
static int orlix_tcti_native_dsb(u8 option)
{
	switch (option) {
	case 1: asm volatile("dsb oshld" : : : "memory"); break;
	case 2: asm volatile("dsb oshst" : : : "memory"); break;
	case 3: asm volatile("dsb osh" : : : "memory"); break;
	case 5: asm volatile("dsb nshld" : : : "memory"); break;
	case 6: asm volatile("dsb nshst" : : : "memory"); break;
	case 7: asm volatile("dsb nsh" : : : "memory"); break;
	case 9: asm volatile("dsb ishld" : : : "memory"); break;
	case 10: asm volatile("dsb ishst" : : : "memory"); break;
	case 11: asm volatile("dsb ish" : : : "memory"); break;
	case 13: asm volatile("dsb ld" : : : "memory"); break;
	case 14: asm volatile("dsb st" : : : "memory"); break;
	case 15: asm volatile("dsb sy" : : : "memory"); break;
	default: return -EINVAL;
	}
	return 0;
}

static int orlix_tcti_native_dmb(u8 option)
{
	switch (option) {
	case 1: asm volatile("dmb oshld" : : : "memory"); break;
	case 2: asm volatile("dmb oshst" : : : "memory"); break;
	case 3: asm volatile("dmb osh" : : : "memory"); break;
	case 5: asm volatile("dmb nshld" : : : "memory"); break;
	case 6: asm volatile("dmb nshst" : : : "memory"); break;
	case 7: asm volatile("dmb nsh" : : : "memory"); break;
	case 9: asm volatile("dmb ishld" : : : "memory"); break;
	case 10: asm volatile("dmb ishst" : : : "memory"); break;
	case 11: asm volatile("dmb ish" : : : "memory"); break;
	case 13: asm volatile("dmb ld" : : : "memory"); break;
	case 14: asm volatile("dmb st" : : : "memory"); break;
	case 15: asm volatile("dmb sy" : : : "memory"); break;
	default: return -EINVAL;
	}
	return 0;
}

static int orlix_tcti_native_dsb_nxs(u8 option)
{
	switch (option) {
	case 2: asm volatile(".inst 0xd503323f" : : : "memory"); return 0;
	case 6: asm volatile(".inst 0xd503363f" : : : "memory"); return 0;
	case 10: asm volatile(".inst 0xd5033a3f" : : : "memory"); return 0;
	case 14: asm volatile(".inst 0xd5033e3f" : : : "memory"); return 0;
	default: return -EINVAL;
	}
}

int orlix_tcti_native_barrier_execute(u8 op, u8 option, bool nxs)
{
	if (nxs)
		return op == 0U ? orlix_tcti_native_dsb_nxs(option) : -EINVAL;
	if (op == 0U)
		return orlix_tcti_native_dsb(option);
	if (op == 1U)
		return orlix_tcti_native_dmb(option);
	if (op == 2U && option == 15U) {
		asm volatile("isb" : : : "memory");
		return 0;
	}
	return -EINVAL;
}

void orlix_tcti_native_csdb(void)
{
	asm volatile(".inst 0xd503229f" : : : "memory");
}

bool orlix_tcti_is_native_barrier_gadget(
	const struct orlix_tcti_decoded_instruction *decoded)
{
	return decoded && (decoded->decode_class == ORLIX_TCTI_DECODE_BARRIER ||
		decoded->decode_class == ORLIX_TCTI_DECODE_SPECULATION_BARRIER ||
		decoded->decode_class == ORLIX_TCTI_DECODE_EXCLUSIVE_MONITOR_CLEAR ||
		(decoded->decode_class == ORLIX_TCTI_DECODE_FEATURE_HINT &&
		 decoded->feature_hint_op == ORLIX_TCTI_FEATURE_HINT_SB));
}

int orlix_tcti_gadget_execute_native_barrier(struct mm_struct *mm,
	struct pt_regs *regs, const struct orlix_tcti_gadget_word **cursor,
	unsigned long *fault_address)
{
	struct orlix_tcti_decoded_instruction decoded;

	(void)fault_address;
	memcpy(&decoded, *cursor, sizeof(decoded));
	*cursor += ORLIX_TCTI_DECODED_INSTRUCTION_WORDS;
	switch (decoded.decode_class) {
	case ORLIX_TCTI_DECODE_BARRIER:
		if (!mm || orlix_tcti_native_barrier_execute(decoded.barrier_op,
			decoded.barrier_option, decoded.barrier_nxs))
			return -EOPNOTSUPP;
		break;
	case ORLIX_TCTI_DECODE_SPECULATION_BARRIER:
		orlix_tcti_native_csdb();
		break;
	case ORLIX_TCTI_DECODE_FEATURE_HINT:
		asm volatile(".inst 0xd50330ff" : : : "memory");
		break;
	case ORLIX_TCTI_DECODE_EXCLUSIVE_MONITOR_CLEAR:
		current->thread.user_exclusive_address = 0;
		current->thread.user_exclusive_value = 0;
		current->thread.user_exclusive_value2 = 0;
		current->thread.user_exclusive_pfn = 0;
		current->thread.user_exclusive_generation = 0;
		current->thread.user_exclusive_mapping_generation = 0;
		current->thread.user_exclusive_size = 0;
		current->thread.user_exclusive_valid = 0;
		asm volatile(".inst 0xd503305f" : : : "memory");
		break;
	default:
		return -EINVAL;
	}
	regs->pc += sizeof(u32);
	return 0;
}
