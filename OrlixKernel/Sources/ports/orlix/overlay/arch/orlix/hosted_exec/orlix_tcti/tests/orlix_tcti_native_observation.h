/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_NATIVE_OBSERVATION_H
#define ORLIX_TCTI_NATIVE_OBSERVATION_H

#include <linux/types.h>

#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>

#define ORLIX_TCTI_NATIVE_OBSERVATION_MAX_MEMORY 128U

enum orlix_tcti_native_observation_state {
	ORLIX_TCTI_NATIVE_OBSERVATION_UNVERIFIED,
	ORLIX_TCTI_NATIVE_OBSERVATION_INCOMPLETE,
	ORLIX_TCTI_NATIVE_OBSERVATION_MATCH,
	ORLIX_TCTI_NATIVE_OBSERVATION_RESULT_MISMATCH,
	ORLIX_TCTI_NATIVE_OBSERVATION_REGS_MISMATCH,
	ORLIX_TCTI_NATIVE_OBSERVATION_MEMORY_MISMATCH,
};

/*
 * This KUnit-only comparator checks observations from one OrlixTCTI execution.  It
 * compares the structured exit, the complete byte representation of struct
 * pt_regs, and an optional bounded memory snapshot.  struct pt_regs does not
 * contain SIMD, floating-point, SVE, or SME state, and a memory snapshot does
 * not establish atomicity or ordering.  Owning tests must provide separate
 * observations for those obligations.
 *
 * There is no generic pass operation.  A match is produced only by comparing
 * every required observation to its expected value.
 */
struct orlix_tcti_native_observation {
	u32 magic;
	u8 expected_mask;
	u8 observed_mask;
	enum orlix_tcti_native_observation_state state;
	struct orlix_tcti_result expected_result;
	struct orlix_tcti_result observed_result;
	struct pt_regs expected_regs;
	struct pt_regs observed_regs;
	size_t memory_size;
	u8 expected_memory[ORLIX_TCTI_NATIVE_OBSERVATION_MAX_MEMORY];
	u8 observed_memory[ORLIX_TCTI_NATIVE_OBSERVATION_MAX_MEMORY];
};

int orlix_tcti_native_observation_init(struct orlix_tcti_native_observation *observation,
				 const struct orlix_tcti_result *expected_result,
				 const struct pt_regs *expected_regs,
				 const void *expected_memory,
				 size_t memory_size);
int orlix_tcti_native_observation_add_result(
		struct orlix_tcti_native_observation *observation,
		const struct orlix_tcti_result *observed_result);
int orlix_tcti_native_observation_add_regs(
		struct orlix_tcti_native_observation *observation,
		const struct pt_regs *observed_regs);
int orlix_tcti_native_observation_add_memory(
		struct orlix_tcti_native_observation *observation,
		const void *observed_memory, size_t memory_size);
int orlix_tcti_native_observation_compare(
		struct orlix_tcti_native_observation *observation);

#endif /* ORLIX_TCTI_NATIVE_OBSERVATION_H */
