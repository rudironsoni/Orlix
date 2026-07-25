/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_NATIVE_OBSERVATION_H
#define ORLIX_TCTI_NATIVE_OBSERVATION_H

#include <linux/types.h>

#include <asm/ptrace.h>
#include <asm/tcti.h>

#define TCTI_NATIVE_OBSERVATION_MAX_MEMORY 128U

enum tcti_native_observation_state {
	TCTI_NATIVE_OBSERVATION_UNVERIFIED,
	TCTI_NATIVE_OBSERVATION_INCOMPLETE,
	TCTI_NATIVE_OBSERVATION_MATCH,
	TCTI_NATIVE_OBSERVATION_RESULT_MISMATCH,
	TCTI_NATIVE_OBSERVATION_REGS_MISMATCH,
	TCTI_NATIVE_OBSERVATION_MEMORY_MISMATCH,
};

/*
 * This KUnit-only comparator checks observations from one TCTI execution.  It
 * compares the structured exit, the complete byte representation of struct
 * pt_regs, and an optional bounded memory snapshot.  struct pt_regs does not
 * contain SIMD, floating-point, SVE, or SME state, and a memory snapshot does
 * not establish atomicity or ordering.  Owning tests must provide separate
 * observations for those obligations.
 *
 * There is no generic pass operation.  A match is produced only by comparing
 * every required observation to its expected value.
 */
struct tcti_native_observation {
	u32 magic;
	u8 expected_mask;
	u8 observed_mask;
	enum tcti_native_observation_state state;
	struct tcti_result expected_result;
	struct tcti_result observed_result;
	struct pt_regs expected_regs;
	struct pt_regs observed_regs;
	size_t memory_size;
	u8 expected_memory[TCTI_NATIVE_OBSERVATION_MAX_MEMORY];
	u8 observed_memory[TCTI_NATIVE_OBSERVATION_MAX_MEMORY];
};

int tcti_native_observation_init(struct tcti_native_observation *observation,
				 const struct tcti_result *expected_result,
				 const struct pt_regs *expected_regs,
				 const void *expected_memory,
				 size_t memory_size);
int tcti_native_observation_add_result(
		struct tcti_native_observation *observation,
		const struct tcti_result *observed_result);
int tcti_native_observation_add_regs(
		struct tcti_native_observation *observation,
		const struct pt_regs *observed_regs);
int tcti_native_observation_add_memory(
		struct tcti_native_observation *observation,
		const void *observed_memory, size_t memory_size);
int tcti_native_observation_compare(
		struct tcti_native_observation *observation);

#endif /* ORLIX_TCTI_NATIVE_OBSERVATION_H */
