/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_NATIVE_OBSERVATION_H
#define ORLIX_TCTI_NATIVE_OBSERVATION_H

#include <linux/types.h>

#include <asm/orlix_tcti.h>

#include "target_native_proof_contract.h"

struct task_struct;
struct pt_regs;
struct mm_struct;

/*
 * Capture selection is registry-owned.  Callers can name only the opaque
 * token issued for their KUnit case and the canonical source/obligation row.
 * They never provide observed state, expected state, or a result comparator.
 */
struct orlix_tcti_native_capture_session;

int orlix_tcti_native_capture_begin(const void *case_token, u32 source_ordinal,
				    u32 obligation,
				    struct orlix_tcti_native_capture_session **session);
int orlix_tcti_native_capture_resume(
	struct orlix_tcti_native_capture_session *session,
	struct task_struct *task, struct pt_regs *regs, struct mm_struct *mm,
	struct orlix_tcti_result *result);
int orlix_tcti_native_capture_take_wire(
	struct orlix_tcti_native_capture_session *session,
	struct orlix_tcti_native_wire_record *record);
void orlix_tcti_native_capture_destroy(
	struct orlix_tcti_native_capture_session *session);

#endif /* ORLIX_TCTI_NATIVE_OBSERVATION_H */
