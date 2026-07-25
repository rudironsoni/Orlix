// SPDX-License-Identifier: GPL-2.0-only
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/string.h>

#include "tcti_native_observation.h"

#define TCTI_NATIVE_OBSERVATION_MAGIC 0x54434f42U

#define TCTI_NATIVE_OBSERVATION_RESULT BIT(0)
#define TCTI_NATIVE_OBSERVATION_REGS BIT(1)
#define TCTI_NATIVE_OBSERVATION_MEMORY BIT(2)

static bool tcti_native_observation_reason_valid(enum tcti_exit_reason reason)
{
	switch (reason) {
	case TCTI_EXIT_SYSCALL:
	case TCTI_EXIT_BREAKPOINT:
	case TCTI_EXIT_USER_FAULT:
	case TCTI_EXIT_UNSUPPORTED_INSTRUCTION:
	case TCTI_EXIT_SIGNAL_POINT:
	case TCTI_EXIT_YIELD:
	case TCTI_EXIT_TASK_EXIT:
	case TCTI_EXIT_ALIGNMENT_FAULT:
		return true;
	}

	return false;
}

static bool tcti_native_observation_access_valid(enum tcti_access access)
{
	switch (access) {
	case TCTI_ACCESS_FETCH:
	case TCTI_ACCESS_READ:
	case TCTI_ACCESS_WRITE:
		return true;
	}

	return false;
}

static bool tcti_native_observation_result_valid(
		const struct tcti_result *result)
{
	return result &&
	       tcti_native_observation_reason_valid(result->reason) &&
	       tcti_native_observation_access_valid(result->fault_access);
}

static bool tcti_native_observation_result_equal(
		const struct tcti_result *left,
		const struct tcti_result *right)
{
	return left->reason == right->reason &&
	       left->status == right->status &&
	       left->fault_address == right->fault_address &&
	       left->fault_access == right->fault_access &&
	       left->pc == right->pc &&
	       left->instruction == right->instruction;
}

static bool tcti_native_observation_valid(
		const struct tcti_native_observation *observation)
{
	return observation &&
	       observation->magic == TCTI_NATIVE_OBSERVATION_MAGIC;
}

int tcti_native_observation_init(struct tcti_native_observation *observation,
				 const struct tcti_result *expected_result,
				 const struct pt_regs *expected_regs,
				 const void *expected_memory,
				 size_t memory_size)
{
	if (!observation)
		return -EINVAL;

	memset(observation, 0, sizeof(*observation));
	observation->state = TCTI_NATIVE_OBSERVATION_UNVERIFIED;
	if (!tcti_native_observation_result_valid(expected_result) ||
	    !expected_regs ||
	    memory_size > TCTI_NATIVE_OBSERVATION_MAX_MEMORY ||
	    (memory_size && !expected_memory))
		return -EINVAL;

	observation->magic = TCTI_NATIVE_OBSERVATION_MAGIC;
	observation->expected_mask = TCTI_NATIVE_OBSERVATION_RESULT |
				     TCTI_NATIVE_OBSERVATION_REGS;
	observation->expected_result = *expected_result;
	observation->expected_regs = *expected_regs;
	observation->memory_size = memory_size;
	if (memory_size) {
		memcpy(observation->expected_memory, expected_memory, memory_size);
		observation->expected_mask |= TCTI_NATIVE_OBSERVATION_MEMORY;
	}

	return 0;
}

int tcti_native_observation_add_result(
		struct tcti_native_observation *observation,
		const struct tcti_result *observed_result)
{
	if (!tcti_native_observation_valid(observation))
		return -EINVAL;
	observation->state = TCTI_NATIVE_OBSERVATION_UNVERIFIED;
	if (!tcti_native_observation_result_valid(observed_result))
		return -EINVAL;
	if (observation->observed_mask & TCTI_NATIVE_OBSERVATION_RESULT)
		return -EALREADY;

	observation->observed_result = *observed_result;
	observation->observed_mask |= TCTI_NATIVE_OBSERVATION_RESULT;
	return 0;
}

int tcti_native_observation_add_regs(
		struct tcti_native_observation *observation,
		const struct pt_regs *observed_regs)
{
	if (!tcti_native_observation_valid(observation))
		return -EINVAL;
	observation->state = TCTI_NATIVE_OBSERVATION_UNVERIFIED;
	if (!observed_regs)
		return -EINVAL;
	if (observation->observed_mask & TCTI_NATIVE_OBSERVATION_REGS)
		return -EALREADY;

	observation->observed_regs = *observed_regs;
	observation->observed_mask |= TCTI_NATIVE_OBSERVATION_REGS;
	return 0;
}

int tcti_native_observation_add_memory(
		struct tcti_native_observation *observation,
		const void *observed_memory, size_t memory_size)
{
	if (!tcti_native_observation_valid(observation))
		return -EINVAL;
	observation->state = TCTI_NATIVE_OBSERVATION_UNVERIFIED;
	if (!observed_memory || !observation->memory_size ||
	    memory_size != observation->memory_size)
		return -EINVAL;
	if (observation->observed_mask & TCTI_NATIVE_OBSERVATION_MEMORY)
		return -EALREADY;

	memcpy(observation->observed_memory, observed_memory, memory_size);
	observation->observed_mask |= TCTI_NATIVE_OBSERVATION_MEMORY;
	return 0;
}

int tcti_native_observation_compare(
		struct tcti_native_observation *observation)
{
	if (!tcti_native_observation_valid(observation))
		return -EINVAL;
	if (observation->observed_mask != observation->expected_mask) {
		observation->state = TCTI_NATIVE_OBSERVATION_INCOMPLETE;
		return -EINPROGRESS;
	}

	observation->state = TCTI_NATIVE_OBSERVATION_UNVERIFIED;
	if (!tcti_native_observation_result_equal(
			&observation->expected_result,
			&observation->observed_result)) {
		observation->state = TCTI_NATIVE_OBSERVATION_RESULT_MISMATCH;
		return -EBADE;
	}
	if (memcmp(&observation->expected_regs, &observation->observed_regs,
		   sizeof(observation->expected_regs))) {
		observation->state = TCTI_NATIVE_OBSERVATION_REGS_MISMATCH;
		return -EBADE;
	}
	if (observation->memory_size &&
	    memcmp(observation->expected_memory, observation->observed_memory,
		   observation->memory_size)) {
		observation->state = TCTI_NATIVE_OBSERVATION_MEMORY_MISMATCH;
		return -EBADE;
	}

	observation->state = TCTI_NATIVE_OBSERVATION_MATCH;
	return 0;
}
