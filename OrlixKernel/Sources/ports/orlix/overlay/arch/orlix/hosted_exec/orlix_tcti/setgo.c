/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * AARCHMRS 2026-06 provides the SETGO* encodings and feature conditions but
 * omits their execute bodies. This is a non-executable implementation draft,
 * retained for future source-backed work only. It receives no semantic, proof,
 * or runtime-capability credit, and gadget_program.c rejects SETGO before this
 * code can affect guest state.
 *
 * This fixed production handler is deliberately outside switch_debug.  Linux
 * owns every data access and allocation-tag transaction: no host memcpy,
 * page-table policy, tag storage, or fault model is recreated here.
 */
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/smp.h>

#include <asm/mte.h>
#include <asm/orlix_tcti.h>
#include <asm/processor.h>
#include <asm/ptrace.h>

#include "decode_aarch64.h"
#include "setgo.h"

#define ORLIX_TCTI_SETGO_STAGE_BYTES ORLIX_MTE_GRANULE_SIZE

static bool orlix_tcti_setgo_is_epilogue(enum orlix_tcti_setgo_op op)
{
	return op >= ORLIX_TCTI_SETGO_E;
}

static bool orlix_tcti_setgo_is_prologue(enum orlix_tcti_setgo_op op)
{
	return op <= ORLIX_TCTI_SETGO_PTN;
}

static bool orlix_tcti_setgo_is_nontemporal(enum orlix_tcti_setgo_op op)
{
	return op & BIT(1);
}

static void orlix_tcti_setgo_publish_progress(struct pt_regs *regs,
	const struct orlix_tcti_decoded_instruction *decoded,
	unsigned long destination, unsigned long remaining)
{
	if (orlix_tcti_setgo_is_nontemporal(decoded->setgo_op)) {
		smp_store_release(&regs->regs[decoded->rd], destination);
		smp_store_release(&regs->regs[decoded->rn], remaining);
		return;
	}
	WRITE_ONCE(regs->regs[decoded->rd], destination);
	WRITE_ONCE(regs->regs[decoded->rn], remaining);
}

int orlix_tcti_execute_setgo(struct mm_struct *mm, struct pt_regs *regs,
	const struct orlix_tcti_decoded_instruction *decoded,
	unsigned long *fault_address)
{
	u64 destination;
	u64 remaining;
	u8 value;
	u8 allocation_tag;
	bool prologue_flags_pending;
	int ret;

	if (!regs || !decoded ||
	    decoded->decode_class != ORLIX_TCTI_DECODE_SET_GO ||
	    decoded->setgo_op > ORLIX_TCTI_SETGO_ETN)
		return -EINVAL;
	if (!mm)
		return -EINVAL;
	/* [UNVERIFIED] Draft constrained-operand handling pending official semantics. */
	if (decoded->rd == 31 || decoded->rn == 31 ||
	    decoded->rd == decoded->rn || decoded->rd == decoded->rs ||
	    decoded->rn == decoded->rs)
		return -EOPNOTSUPP;

	destination = regs->regs[decoded->rd];
	remaining = regs->regs[decoded->rn];
	if (!IS_ALIGNED(orlix_mte_untagged_address(destination),
			ORLIX_MTE_GRANULE_SIZE) ||
	    !IS_ALIGNED(remaining, ORLIX_MTE_GRANULE_SIZE)) {
		if (fault_address)
			*fault_address = destination;
		return -EFAULT;
	}
	if (!orlix_tcti_setgo_is_prologue(decoded->setgo_op) && remaining &&
	    !(regs->pstate & PSR_C_BIT))
		return -EOPNOTSUPP;

	value = regs->regs[decoded->rs];
	allocation_tag = (destination & ORLIX_MTE_TAG_MASK) >> ORLIX_MTE_TAG_SHIFT;
	prologue_flags_pending = orlix_tcti_setgo_is_prologue(decoded->setgo_op);
	do {
		unsigned long stage_start = destination;
		unsigned long count = min_t(unsigned long, remaining,
			ORLIX_TCTI_SETGO_STAGE_BYTES);
		unsigned long done = 0;

		while (done < count) {
			ret = orlix_tcti_write_user_data(mm, destination, &value,
						  sizeof(value));
			if (ret) {
				if (fault_address)
					*fault_address = destination;
				return ret;
			}
			destination++;
			remaining--;
			done++;
			if (prologue_flags_pending) {
				WRITE_ONCE(regs->pstate, (regs->pstate &
					~(PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT)) |
					PSR_C_BIT);
				prologue_flags_pending = false;
			}
			orlix_tcti_setgo_publish_progress(regs, decoded, destination,
							 remaining);
		}
		if (done) {
			ret = orlix_mte_store_allocation_tag(mm, stage_start,
				allocation_tag);
			if (ret) {
				if (fault_address)
					*fault_address = stage_start;
				return ret;
			}
		}
	} while (orlix_tcti_setgo_is_epilogue(decoded->setgo_op) && remaining);

	regs->pc += sizeof(u32);
	return 0;
}
