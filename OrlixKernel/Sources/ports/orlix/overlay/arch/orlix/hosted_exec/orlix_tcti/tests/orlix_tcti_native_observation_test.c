// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/errno.h>
#include <linux/string.h>

#include "orlix_tcti_native_observation.h"

static const struct orlix_tcti_result orlix_tcti_native_proof_result = {
	.reason = ORLIX_TCTI_EXIT_SYSCALL,
	.status = 17,
	.fault_address = 0x12340000UL,
	.fault_access = ORLIX_TCTI_ACCESS_READ,
	.pc = 0x4000UL,
	.instruction = 0xd4000001U,
};

static void orlix_tcti_native_proof_seed_regs(struct pt_regs *regs)
{
	memset(regs, 0, sizeof(*regs));
	regs->regs[0] = 0x0123456789abcdefULL;
	regs->regs[30] = 0xfedcba9876543210ULL;
	regs->sp = 0x10000UL;
	regs->pc = 0x4000UL;
	regs->pstate = PSR_MODE_EL0t | PSR_Z_BIT;
}

static void orlix_tcti_native_observation_accepts_complete_exact_observation(
		struct kunit *test)
{
	const u8 memory[] = { 0x00, 0xff, 0x41, 0x00 };
	struct orlix_tcti_native_observation observation;
	struct pt_regs regs;

	orlix_tcti_native_proof_seed_regs(&regs);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_native_observation_init(&observation,
					     &orlix_tcti_native_proof_result,
					     &regs, memory, sizeof(memory)));
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_native_observation_add_regs(&observation, &regs));
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_native_observation_add_result(&observation,
						  &orlix_tcti_native_proof_result));
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_native_observation_add_memory(&observation, memory,
						   sizeof(memory)));
	KUNIT_EXPECT_EQ(test, 0,
			orlix_tcti_native_observation_compare(&observation));
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_NATIVE_OBSERVATION_MATCH,
			observation.state);
}

static void orlix_tcti_native_observation_rejects_invalid_duplicate_and_incomplete(
		struct kunit *test)
{
	const u8 memory[] = { 1, 2, 3 };
	struct orlix_tcti_native_observation observation = {};
	struct orlix_tcti_result invalid = orlix_tcti_native_proof_result;
	struct pt_regs regs;

	orlix_tcti_native_proof_seed_regs(&regs);
	invalid.reason = ORLIX_TCTI_EXIT_ALIGNMENT_FAULT + 1;
	KUNIT_EXPECT_EQ(test, -EINVAL,
		orlix_tcti_native_observation_init(&observation, &invalid, &regs,
					     memory, sizeof(memory)));
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_NATIVE_OBSERVATION_UNVERIFIED,
			observation.state);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_native_observation_init(&observation,
					     &orlix_tcti_native_proof_result,
					     &regs, memory, sizeof(memory)));
	KUNIT_EXPECT_EQ(test, -EINPROGRESS,
			orlix_tcti_native_observation_compare(&observation));
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_NATIVE_OBSERVATION_INCOMPLETE,
			observation.state);
	KUNIT_EXPECT_NE(test, ORLIX_TCTI_NATIVE_OBSERVATION_MATCH,
			observation.state);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_native_observation_add_result(&observation,
						  &orlix_tcti_native_proof_result));
	KUNIT_EXPECT_EQ(test, -EALREADY,
		orlix_tcti_native_observation_add_result(&observation,
						  &orlix_tcti_native_proof_result));
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_NATIVE_OBSERVATION_UNVERIFIED,
			observation.state);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_native_observation_add_regs(&observation, &regs));
	KUNIT_EXPECT_EQ(test, -EINVAL,
		orlix_tcti_native_observation_add_memory(&observation, memory,
						   sizeof(memory) - 1));
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_native_observation_add_memory(&observation, memory,
						   sizeof(memory)));
	KUNIT_EXPECT_EQ(test, -EALREADY,
		orlix_tcti_native_observation_add_memory(&observation, memory,
						   sizeof(memory)));
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_NATIVE_OBSERVATION_UNVERIFIED,
			observation.state);
}

static void orlix_tcti_native_observation_rejects_invalid_observed_access(
		struct kunit *test)
{
	struct orlix_tcti_native_observation observation;
	struct orlix_tcti_result invalid = orlix_tcti_native_proof_result;
	struct pt_regs regs;

	orlix_tcti_native_proof_seed_regs(&regs);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_native_observation_init(&observation,
					     &orlix_tcti_native_proof_result,
					     &regs, NULL, 0));
	invalid.fault_access = ORLIX_TCTI_ACCESS_WRITE + 1;
	KUNIT_EXPECT_EQ(test, -EINVAL,
		orlix_tcti_native_observation_add_result(&observation, &invalid));
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_NATIVE_OBSERVATION_UNVERIFIED,
			observation.state);
}

static void orlix_tcti_native_observation_accepts_no_memory(struct kunit *test)
{
	struct orlix_tcti_native_observation observation;
	struct pt_regs regs;

	orlix_tcti_native_proof_seed_regs(&regs);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_native_observation_init(&observation,
					     &orlix_tcti_native_proof_result,
					     &regs, NULL, 0));
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_native_observation_add_result(&observation,
						  &orlix_tcti_native_proof_result));
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_native_observation_add_regs(&observation, &regs));
	KUNIT_EXPECT_EQ(test, 0,
			orlix_tcti_native_observation_compare(&observation));
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_NATIVE_OBSERVATION_MATCH,
			observation.state);
}

static void orlix_tcti_native_observation_reports_structured_mismatches(
		struct kunit *test)
{
	const u8 expected_memory[] = { 1, 2, 3, 4 };
	const u8 observed_memory[] = { 1, 2, 3, 5 };
	struct orlix_tcti_native_observation observation;
	struct orlix_tcti_result result = orlix_tcti_native_proof_result;
	struct pt_regs regs;
	int ret;

	orlix_tcti_native_proof_seed_regs(&regs);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_native_observation_init(&observation,
					     &orlix_tcti_native_proof_result,
					     &regs, expected_memory,
					     sizeof(expected_memory)));
	result.instruction++;
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_native_observation_add_result(&observation, &result));
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_native_observation_add_regs(&observation, &regs));
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_native_observation_add_memory(&observation,
						   expected_memory,
						   sizeof(expected_memory)));
	ret = orlix_tcti_native_observation_compare(&observation);
	KUNIT_EXPECT_EQ(test, -EBADE, ret);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_NATIVE_OBSERVATION_RESULT_MISMATCH,
			observation.state);

	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_native_observation_init(&observation,
					     &orlix_tcti_native_proof_result,
					     &regs, expected_memory,
					     sizeof(expected_memory)));
	regs.regs[29] = 1;
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_native_observation_add_result(&observation,
						  &orlix_tcti_native_proof_result));
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_native_observation_add_regs(&observation, &regs));
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_native_observation_add_memory(&observation,
						   expected_memory,
						   sizeof(expected_memory)));
	ret = orlix_tcti_native_observation_compare(&observation);
	KUNIT_EXPECT_EQ(test, -EBADE, ret);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_NATIVE_OBSERVATION_REGS_MISMATCH,
			observation.state);

	orlix_tcti_native_proof_seed_regs(&regs);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_native_observation_init(&observation,
					     &orlix_tcti_native_proof_result,
					     &regs, expected_memory,
					     sizeof(expected_memory)));
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_native_observation_add_result(&observation,
						  &orlix_tcti_native_proof_result));
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_native_observation_add_regs(&observation, &regs));
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_native_observation_add_memory(&observation,
						   observed_memory,
						   sizeof(observed_memory)));
	ret = orlix_tcti_native_observation_compare(&observation);
	KUNIT_EXPECT_EQ(test, -EBADE, ret);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_NATIVE_OBSERVATION_MEMORY_MISMATCH,
			observation.state);
}

static struct kunit_case orlix_tcti_native_observation_test_cases[] = {
	KUNIT_CASE(orlix_tcti_native_observation_accepts_complete_exact_observation),
	KUNIT_CASE(
		orlix_tcti_native_observation_rejects_invalid_duplicate_and_incomplete),
	KUNIT_CASE(orlix_tcti_native_observation_rejects_invalid_observed_access),
	KUNIT_CASE(orlix_tcti_native_observation_accepts_no_memory),
	KUNIT_CASE(orlix_tcti_native_observation_reports_structured_mismatches),
	{}
};

static struct kunit_suite orlix_tcti_native_observation_test_suite = {
	.name = "orlix-tcti-native-observation",
	.test_cases = orlix_tcti_native_observation_test_cases,
};
kunit_test_suite(orlix_tcti_native_observation_test_suite);

MODULE_LICENSE("GPL");
