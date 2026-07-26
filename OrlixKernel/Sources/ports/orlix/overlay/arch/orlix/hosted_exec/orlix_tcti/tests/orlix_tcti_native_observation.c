// SPDX-License-Identifier: GPL-2.0-only
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/string.h>

#include "orlix_tcti_native_observation.h"

#define ORLIX_TCTI_NATIVE_OBSERVATION_MAGIC 0x54434f42U

#define ORLIX_TCTI_NATIVE_OBSERVATION_RESULT BIT(0)
#define ORLIX_TCTI_NATIVE_OBSERVATION_REGS BIT(1)
#define ORLIX_TCTI_NATIVE_OBSERVATION_MEMORY BIT(2)

static bool orlix_tcti_native_observation_reason_valid(enum orlix_tcti_exit_reason reason)
{
	switch (reason) {
	case ORLIX_TCTI_EXIT_SYSCALL:
	case ORLIX_TCTI_EXIT_BREAKPOINT:
	case ORLIX_TCTI_EXIT_USER_FAULT:
	case ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION:
	case ORLIX_TCTI_EXIT_SIGNAL_POINT:
	case ORLIX_TCTI_EXIT_YIELD:
	case ORLIX_TCTI_EXIT_TASK_EXIT:
	case ORLIX_TCTI_EXIT_ALIGNMENT_FAULT:
		return true;
	}

	return false;
}

static bool orlix_tcti_native_observation_access_valid(enum orlix_tcti_access access)
{
	switch (access) {
	case ORLIX_TCTI_ACCESS_FETCH:
	case ORLIX_TCTI_ACCESS_READ:
	case ORLIX_TCTI_ACCESS_WRITE:
		return true;
	}

	return false;
}

static bool orlix_tcti_native_observation_result_valid(
		const struct orlix_tcti_result *result)
{
	return result &&
	       orlix_tcti_native_observation_reason_valid(result->reason) &&
	       orlix_tcti_native_observation_access_valid(result->fault_access);
}

static bool orlix_tcti_native_observation_result_equal(
		const struct orlix_tcti_result *left,
		const struct orlix_tcti_result *right)
{
	return left->reason == right->reason &&
	       left->status == right->status &&
	       left->fault_address == right->fault_address &&
	       left->fault_access == right->fault_access &&
	       left->pc == right->pc &&
	       left->instruction == right->instruction;
}

static bool orlix_tcti_native_observation_valid(
		const struct orlix_tcti_native_observation *observation)
{
	return observation &&
	       observation->magic == ORLIX_TCTI_NATIVE_OBSERVATION_MAGIC;
}

int orlix_tcti_native_observation_init(struct orlix_tcti_native_observation *observation,
				 const struct orlix_tcti_result *expected_result,
				 const struct pt_regs *expected_regs,
				 const void *expected_memory,
				 size_t memory_size)
{
	if (!observation)
		return -EINVAL;

	memset(observation, 0, sizeof(*observation));
	observation->state = ORLIX_TCTI_NATIVE_OBSERVATION_UNVERIFIED;
	if (!orlix_tcti_native_observation_result_valid(expected_result) ||
	    !expected_regs ||
	    memory_size > ORLIX_TCTI_NATIVE_OBSERVATION_MAX_MEMORY ||
	    (memory_size && !expected_memory))
		return -EINVAL;

	observation->magic = ORLIX_TCTI_NATIVE_OBSERVATION_MAGIC;
	observation->expected_mask = ORLIX_TCTI_NATIVE_OBSERVATION_RESULT |
				     ORLIX_TCTI_NATIVE_OBSERVATION_REGS;
	observation->expected_result = *expected_result;
	observation->expected_regs = *expected_regs;
	observation->memory_size = memory_size;
	if (memory_size) {
		memcpy(observation->expected_memory, expected_memory, memory_size);
		observation->expected_mask |= ORLIX_TCTI_NATIVE_OBSERVATION_MEMORY;
	}

	return 0;
}

int orlix_tcti_native_observation_add_result(
		struct orlix_tcti_native_observation *observation,
		const struct orlix_tcti_result *observed_result)
{
	if (!orlix_tcti_native_observation_valid(observation))
		return -EINVAL;
	observation->state = ORLIX_TCTI_NATIVE_OBSERVATION_UNVERIFIED;
	if (!orlix_tcti_native_observation_result_valid(observed_result))
		return -EINVAL;
	if (observation->observed_mask & ORLIX_TCTI_NATIVE_OBSERVATION_RESULT)
		return -EALREADY;

	observation->observed_result = *observed_result;
	observation->observed_mask |= ORLIX_TCTI_NATIVE_OBSERVATION_RESULT;
	return 0;
}

int orlix_tcti_native_observation_add_regs(
		struct orlix_tcti_native_observation *observation,
		const struct pt_regs *observed_regs)
{
	if (!orlix_tcti_native_observation_valid(observation))
		return -EINVAL;
	observation->state = ORLIX_TCTI_NATIVE_OBSERVATION_UNVERIFIED;
	if (!observed_regs)
		return -EINVAL;
	if (observation->observed_mask & ORLIX_TCTI_NATIVE_OBSERVATION_REGS)
		return -EALREADY;

	observation->observed_regs = *observed_regs;
	observation->observed_mask |= ORLIX_TCTI_NATIVE_OBSERVATION_REGS;
	return 0;
}

int orlix_tcti_native_observation_add_memory(
		struct orlix_tcti_native_observation *observation,
		const void *observed_memory, size_t memory_size)
{
	if (!orlix_tcti_native_observation_valid(observation))
		return -EINVAL;
	observation->state = ORLIX_TCTI_NATIVE_OBSERVATION_UNVERIFIED;
	if (!observed_memory || !observation->memory_size ||
	    memory_size != observation->memory_size)
		return -EINVAL;
	if (observation->observed_mask & ORLIX_TCTI_NATIVE_OBSERVATION_MEMORY)
		return -EALREADY;

	memcpy(observation->observed_memory, observed_memory, memory_size);
	observation->observed_mask |= ORLIX_TCTI_NATIVE_OBSERVATION_MEMORY;
	return 0;
}

int orlix_tcti_native_observation_compare(
		struct orlix_tcti_native_observation *observation)
{
	if (!orlix_tcti_native_observation_valid(observation))
		return -EINVAL;
	if (observation->observed_mask != observation->expected_mask) {
		observation->state = ORLIX_TCTI_NATIVE_OBSERVATION_INCOMPLETE;
		return -EINPROGRESS;
	}

	observation->state = ORLIX_TCTI_NATIVE_OBSERVATION_UNVERIFIED;
	if (!orlix_tcti_native_observation_result_equal(
			&observation->expected_result,
			&observation->observed_result)) {
		observation->state = ORLIX_TCTI_NATIVE_OBSERVATION_RESULT_MISMATCH;
		return -EBADE;
	}
	if (memcmp(&observation->expected_regs, &observation->observed_regs,
		   sizeof(observation->expected_regs))) {
		observation->state = ORLIX_TCTI_NATIVE_OBSERVATION_REGS_MISMATCH;
		return -EBADE;
	}
	if (observation->memory_size &&
	    memcmp(observation->expected_memory, observation->observed_memory,
		   observation->memory_size)) {
		observation->state = ORLIX_TCTI_NATIVE_OBSERVATION_MEMORY_MISMATCH;
		return -EBADE;
	}

	observation->state = ORLIX_TCTI_NATIVE_OBSERVATION_MATCH;
	return 0;
}
