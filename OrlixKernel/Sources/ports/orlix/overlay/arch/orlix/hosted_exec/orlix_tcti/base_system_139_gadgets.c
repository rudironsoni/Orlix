/* SPDX-License-Identifier: GPL-2.0-only */
#include <asm/orlix_tcti.h>
#include <asm/ptrace.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/string.h>

#include "base_system_139_gadgets.h"
#include "system_accessor.h"

static bool base_system_139_ordinal(u16 ordinal)
{
	switch (ordinal) {
	case 2236U ... 2244U:
	case 2250U ... 2255U:
	case 2264U:
	case 2266U ... 2280U:
	case 2285U ... 2287U:
	case 2344U:
		return true;
	default:
		return false;
	}
}

bool orlix_tcti_is_base_system_139_gadget(
	const struct orlix_tcti_decoded_instruction *decoded)
{
	return decoded && base_system_139_ordinal(decoded->source_ordinal);
}

static int execute_event(struct pt_regs *regs,
	const struct orlix_tcti_decoded_instruction *decoded)
{
	switch (decoded->event_op) {
	case ORLIX_TCTI_EVENT_NOP:
		regs->pc += sizeof(u32);
		return 0;
	case ORLIX_TCTI_EVENT_YIELD:
		regs->pc += sizeof(u32);
		return -EAGAIN;
	case ORLIX_TCTI_EVENT_SEV:
		orlix_tcti_event_sev();
		regs->pc += sizeof(u32);
		return 0;
	case ORLIX_TCTI_EVENT_SEVL:
		orlix_tcti_event_sevl();
		regs->pc += sizeof(u32);
		return 0;
	case ORLIX_TCTI_EVENT_WFE:
	case ORLIX_TCTI_EVENT_WFET:
		regs->pc += sizeof(u32);
		return orlix_tcti_event_wfe_consumed(NULL) ? 0 : -EWOULDBLOCK;
	case ORLIX_TCTI_EVENT_WFI:
	case ORLIX_TCTI_EVENT_WFIT:
		regs->pc += sizeof(u32);
		return -EINPROGRESS;
	default:
		return -EINVAL;
	}
}

static int execute_pstate_flags(struct pt_regs *regs,
	const struct orlix_tcti_decoded_instruction *decoded)
{
	u64 nzcv = regs->pstate & (PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT);
	bool z = nzcv & PSR_Z_BIT;
	bool c = nzcv & PSR_C_BIT;
	bool v = nzcv & PSR_V_BIT;

	switch (decoded->pstate_flag_op) {
	case ORLIX_TCTI_PSTATE_FLAG_CFINV:
		nzcv ^= PSR_C_BIT;
		break;
	case ORLIX_TCTI_PSTATE_FLAG_XAFLAG:
		nzcv = (!c && !z ? PSR_N_BIT : 0) |
			(z && c ? PSR_Z_BIT : 0) |
			(c || z ? PSR_C_BIT : 0) |
			(!c && z ? PSR_V_BIT : 0);
		break;
	case ORLIX_TCTI_PSTATE_FLAG_AXFLAG:
		nzcv = (z || v ? PSR_Z_BIT : 0) |
			(c && !v ? PSR_C_BIT : 0);
		break;
	default:
		return -EINVAL;
	}
	regs->pstate &= ~(PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT);
	regs->pstate |= nzcv;
	regs->pc += sizeof(u32);
	return 0;
}

int orlix_tcti_gadget_execute_base_system_139(struct mm_struct *mm,
	struct pt_regs *regs, const struct orlix_tcti_gadget_word **cursor,
	unsigned long *fault_address)
{
	struct orlix_tcti_decoded_instruction decoded;

	(void)mm;
	(void)fault_address;
	memcpy(&decoded, *cursor, sizeof(decoded));
	*cursor += ORLIX_TCTI_DECODED_INSTRUCTION_WORDS;
	if (!orlix_tcti_is_base_system_139_gadget(&decoded))
		return -EINVAL;
	switch (decoded.decode_class) {
	case ORLIX_TCTI_DECODE_EVENT:
		return execute_event(regs, &decoded);
	case ORLIX_TCTI_DECODE_HINT:
		regs->pc += sizeof(u32);
		return decoded.hint_imm >= 1U && decoded.hint_imm <= 3U ? -EAGAIN : 0;
	case ORLIX_TCTI_DECODE_PSTATE_FLAG:
		return execute_pstate_flags(regs, &decoded);
	case ORLIX_TCTI_DECODE_FEATURE_HINT:
		/* SB is selected by barrier_gadgets; named hints preserve EL0 state. */
		if (decoded.feature_hint_op == ORLIX_TCTI_FEATURE_HINT_SB)
			return -EINVAL;
		regs->pc += sizeof(u32);
		return 0;
	case ORLIX_TCTI_DECODE_SYSTEM_REGISTER:
		return orlix_tcti_execute_system_register(regs, &decoded);
	case ORLIX_TCTI_DECODE_SYSTEM_INSTRUCTION:
		/* The generated partition admits no implemented SYS/SYSL operation. */
		return -EOPNOTSUPP;
	case ORLIX_TCTI_DECODE_SYSTEM_PSTATE_IMMEDIATE:
	case ORLIX_TCTI_DECODE_FEATURE_UNAVAILABLE:
		return -EOPNOTSUPP;
	default:
		return -EINVAL;
	}
}
