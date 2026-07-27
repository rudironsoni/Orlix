// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>

#include "../tls_repair.h"

#define ORLIX_TEST_USER_BASE 0x100000000UL
#define ORLIX_TEST_USER_LIMIT 0x200000000UL
#define ORLIX_TEST_ACTIVE_TLS 0x1002bff80UL

static struct orlix_host_user_tls_repair_request
orlix_tcti_tls_repair_request(void)
{
	struct orlix_host_user_tls_repair_request request = {
		.user_base = ORLIX_TEST_USER_BASE,
		.user_limit = ORLIX_TEST_USER_LIMIT,
		.fault_address = 0x300000000UL,
		.installed_host_tls = 0x1007UL,
		.live_host_tls = 0x1005UL,
		.active_user_tls = ORLIX_TEST_ACTIVE_TLS,
		.faulting_instruction = 0xf85f02c9U,
	};

	return request;
}

static void orlix_tcti_tls_repair_rejects_missing_unsigned_current_state(
	struct kunit *test)
{
	struct orlix_host_user_tls_repair_request request =
		orlix_tcti_tls_repair_request();
	struct orlix_host_user_tls_repair_decision decision = {};

	request.faulting_instruction = 0xf9400d09U;
	request.fault_address = 0x3018UL;
	request.regs[8] = 0x3000UL;

	KUNIT_EXPECT_EQ(test, 0,
		orlix_tcti_decide_user_tls_repair(&request, &decision));
}

static void orlix_tcti_tls_repair_rebases_from_live_host_tls(struct kunit *test)
{
	struct orlix_host_user_tls_repair_request request =
		orlix_tcti_tls_repair_request();
	struct orlix_host_user_tls_repair_decision decision = {};

	request.regs[22] = request.live_host_tls - 0x78UL;
	request.fault_address = request.regs[22] - 0x10UL;

	KUNIT_ASSERT_EQ(test, 1,
		orlix_tcti_decide_user_tls_repair(&request, &decision));
	KUNIT_EXPECT_EQ(test, 22U, decision.register_index);
	KUNIT_EXPECT_EQ(test, ORLIX_TEST_ACTIVE_TLS - 0x78UL,
			decision.register_value);
	KUNIT_EXPECT_EQ(test, ORLIX_TEST_ACTIVE_TLS, decision.user_tls);
}

static void orlix_tcti_tls_repair_rejects_missing_signed_current_state(
	struct kunit *test)
{
	struct orlix_host_user_tls_repair_request request =
		orlix_tcti_tls_repair_request();
	struct orlix_host_user_tls_repair_decision decision = {};

	request.regs[22] = 0x3000UL;
	request.fault_address = request.regs[22] - 0x10UL;

	KUNIT_EXPECT_EQ(test, 0,
		orlix_tcti_decide_user_tls_repair(&request, &decision));
}

static void orlix_tcti_tls_repair_rejects_invalid_user_tls(struct kunit *test)
{
	struct orlix_host_user_tls_repair_request request =
		orlix_tcti_tls_repair_request();
	struct orlix_host_user_tls_repair_decision decision = {};

	request.active_user_tls = ORLIX_TEST_ACTIVE_TLS - 2UL;
	KUNIT_EXPECT_EQ(test, 0,
		orlix_tcti_decide_user_tls_repair(&request, &decision));

	request.active_user_tls = 0;
	KUNIT_EXPECT_EQ(test, 0,
		orlix_tcti_decide_user_tls_repair(&request, &decision));
}

static void orlix_tcti_tls_repair_rejects_non_memory_decode(struct kunit *test)
{
	struct orlix_host_user_tls_repair_request request =
		orlix_tcti_tls_repair_request();
	struct orlix_host_user_tls_repair_decision decision = {};

	request.faulting_instruction = 0xd503201fU;

	KUNIT_EXPECT_EQ(test, 0,
		orlix_tcti_decide_user_tls_repair(&request, &decision));
}

static void orlix_tcti_tls_repair_rejects_unmatched_fault_address(
	struct kunit *test)
{
	struct orlix_host_user_tls_repair_request request =
		orlix_tcti_tls_repair_request();
	struct orlix_host_user_tls_repair_decision decision = {};

	request.regs[22] = request.live_host_tls - 0x78UL;
	request.fault_address = request.regs[22] - 0x18UL;

	KUNIT_EXPECT_EQ(test, 0,
		orlix_tcti_decide_user_tls_repair(&request, &decision));
}

static void orlix_tcti_tls_repair_rejects_repaired_address_outside_user(
	struct kunit *test)
{
	struct orlix_host_user_tls_repair_request request =
		orlix_tcti_tls_repair_request();
	struct orlix_host_user_tls_repair_decision decision = {};

	request.active_user_tls = ORLIX_TEST_USER_BASE;
	request.regs[22] = request.live_host_tls;
	request.fault_address = request.regs[22] - 0x10UL;

	KUNIT_EXPECT_EQ(test, 0,
		orlix_tcti_decide_user_tls_repair(&request, &decision));
}

static struct kunit_case orlix_tcti_tls_repair_test_cases[] = {
	KUNIT_CASE(orlix_tcti_tls_repair_rebases_from_live_host_tls),
	KUNIT_CASE(orlix_tcti_tls_repair_rejects_missing_unsigned_current_state),
	KUNIT_CASE(orlix_tcti_tls_repair_rejects_missing_signed_current_state),
	KUNIT_CASE(orlix_tcti_tls_repair_rejects_invalid_user_tls),
	KUNIT_CASE(orlix_tcti_tls_repair_rejects_non_memory_decode),
	KUNIT_CASE(orlix_tcti_tls_repair_rejects_unmatched_fault_address),
	KUNIT_CASE(orlix_tcti_tls_repair_rejects_repaired_address_outside_user),
	{}
};

static struct kunit_suite orlix_tcti_tls_repair_test_suite = {
	.name = "orlix-tcti-tls-repair",
	.test_cases = orlix_tcti_tls_repair_test_cases,
};

kunit_test_suite(orlix_tcti_tls_repair_test_suite);

MODULE_LICENSE("GPL");
