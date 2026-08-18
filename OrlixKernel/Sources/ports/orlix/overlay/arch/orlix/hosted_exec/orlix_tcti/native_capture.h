/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_NATIVE_CAPTURE_H
#define ORLIX_TCTI_NATIVE_CAPTURE_H

#include <asm/orlix_tcti.h>

struct mm_struct;
struct pt_regs;
struct task_struct;
struct orlix_tcti_decoded_instruction;

/*
 * This is deliberately a producer-side sink.  It carries no expected state
 * and the executor never learns about proof contracts or ingestion.
 */
struct orlix_tcti_native_capture;
struct orlix_tcti_native_capture_ops {
	void (*before_decoded)(struct orlix_tcti_native_capture *capture,
		struct mm_struct *mm, const struct pt_regs *regs,
		const struct orlix_tcti_decoded_instruction *decoded);
	void (*after_decoded)(struct orlix_tcti_native_capture *capture,
		struct mm_struct *mm, const struct pt_regs *regs,
		const struct orlix_tcti_decoded_instruction *decoded);
	void (*fault)(struct orlix_tcti_native_capture *capture,
		const struct orlix_tcti_decoded_instruction *decoded,
		unsigned long address, int status);
	void (*exit)(struct orlix_tcti_native_capture *capture,
		const struct orlix_tcti_result *result,
		const struct pt_regs *regs);
	void (*finalize)(struct orlix_tcti_native_capture *capture,
		const struct orlix_tcti_result *result, const struct pt_regs *regs);
};

struct orlix_tcti_native_capture {
	const struct orlix_tcti_native_capture_ops *ops;
	u32 target_execution_count;
	bool target_seen;
	bool finalized;
};

static inline void orlix_tcti_native_capture_before_decoded(
	struct orlix_tcti_native_capture *capture, struct mm_struct *mm,
	const struct pt_regs *regs,
	const struct orlix_tcti_decoded_instruction *decoded)
{
	if (capture && capture->ops && capture->ops->before_decoded)
		capture->ops->before_decoded(capture, mm, regs, decoded);
}

static inline void orlix_tcti_native_capture_after_decoded(
	struct orlix_tcti_native_capture *capture, struct mm_struct *mm,
	const struct pt_regs *regs,
	const struct orlix_tcti_decoded_instruction *decoded)
{
	if (capture && capture->ops && capture->ops->after_decoded)
		capture->ops->after_decoded(capture, mm, regs, decoded);
}

static inline void orlix_tcti_native_capture_fault(
	struct orlix_tcti_native_capture *capture,
	const struct orlix_tcti_decoded_instruction *decoded,
	unsigned long address, int status)
{
	if (capture && capture->ops && capture->ops->fault)
		capture->ops->fault(capture, decoded, address, status);
}

static inline void orlix_tcti_native_capture_exit(
	struct orlix_tcti_native_capture *capture,
	const struct orlix_tcti_result *result, const struct pt_regs *regs)
{
	if (capture && capture->ops && capture->ops->exit)
		capture->ops->exit(capture, result, regs);
}

static inline void orlix_tcti_native_capture_execution_succeeded(
	struct orlix_tcti_native_capture *capture)
{
	/* Generic capture events are observational. They cannot credit a proof. */
	(void)capture;
}

static inline void orlix_tcti_native_capture_finalize(
	struct orlix_tcti_native_capture *capture,
	const struct orlix_tcti_result *result, const struct pt_regs *regs)
{
	if (capture && capture->ops && capture->ops->finalize)
		capture->ops->finalize(capture, result, regs);
}

struct orlix_tcti_native_capture *orlix_tcti_native_capture_claim_resume(
	struct task_struct *task);

/* Only engine.c can present its private execution-boundary evidence. */
bool orlix_tcti_native_capture_engine_evidence_valid(const void *evidence);
void orlix_tcti_native_capture_complete_successful_gadget(
	struct orlix_tcti_native_capture *capture, const void *evidence);
void orlix_tcti_native_capture_complete_fault_observation(
	struct orlix_tcti_native_capture *capture, const void *evidence);

#endif /* ORLIX_TCTI_NATIVE_CAPTURE_H */
