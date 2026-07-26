// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>

#include <asm/hosted_tls_repair.h>

#define ORLIX_TEST_USER_BASE	0x100000000UL
#define ORLIX_TEST_USER_LIMIT	0x200000000UL
#define ORLIX_TEST_ACTIVE_TLS	0x1002bff80UL

static struct orlix_host_user_tls_repair_request
orlix_hosted_tls_repair_request(void)
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

static void orlix_hosted_tls_repair_finds_recent_tpidr_base(struct kunit *test)
{
	struct orlix_host_user_tls_repair_request request =
		orlix_hosted_tls_repair_request();
	struct orlix_host_user_tls_repair_decision decision = {};

	request.instruction_history[13] = 0xd53bd056U;
	request.instruction_history_count = 14;

	KUNIT_ASSERT_EQ(test, 1,
		orlix_hosted_decide_user_tls_repair(&request, &decision));
	KUNIT_EXPECT_EQ(test, 22U, decision.register_index);
	KUNIT_EXPECT_EQ(test, ORLIX_TEST_ACTIVE_TLS, decision.register_value);
	KUNIT_EXPECT_EQ(test, ORLIX_TEST_ACTIVE_TLS, decision.user_tls);
}

static void orlix_hosted_tls_repair_rebases_from_live_host_tls(struct kunit *test)
{
	struct orlix_host_user_tls_repair_request request =
		orlix_hosted_tls_repair_request();
	struct orlix_host_user_tls_repair_decision decision = {};

	request.regs[22] = request.live_host_tls - 0x78UL;

	KUNIT_ASSERT_EQ(test, 1,
		orlix_hosted_decide_user_tls_repair(&request, &decision));
	KUNIT_EXPECT_EQ(test, 22U, decision.register_index);
	KUNIT_EXPECT_EQ(test, ORLIX_TEST_ACTIVE_TLS - 0x78UL,
			decision.register_value);
	KUNIT_EXPECT_EQ(test, ORLIX_TEST_ACTIVE_TLS, decision.user_tls);
}

static void orlix_hosted_tls_repair_stops_at_intervening_write(struct kunit *test)
{
	struct orlix_host_user_tls_repair_request request =
		orlix_hosted_tls_repair_request();
	struct orlix_host_user_tls_repair_decision decision = {};

	request.faulting_instruction = 0xf9400d09U;
	request.instruction_history[0] = 0xd101e108U;
	request.instruction_history[1] = 0xd53bd048U;
	request.instruction_history_count = 2;
	request.regs[8] = 0;

	KUNIT_EXPECT_EQ(test, 0,
		orlix_hosted_decide_user_tls_repair(&request, &decision));
}

static void orlix_hosted_tls_repair_rejects_invalid_user_tls(struct kunit *test)
{
	struct orlix_host_user_tls_repair_request request =
		orlix_hosted_tls_repair_request();
	struct orlix_host_user_tls_repair_decision decision = {};

	request.active_user_tls = ORLIX_TEST_ACTIVE_TLS - 2UL;
	KUNIT_EXPECT_EQ(test, 0,
		orlix_hosted_decide_user_tls_repair(&request, &decision));

	request.active_user_tls = 0;
	KUNIT_EXPECT_EQ(test, 0,
		orlix_hosted_decide_user_tls_repair(&request, &decision));
}

static struct kunit_case orlix_hosted_tls_repair_test_cases[] = {
	KUNIT_CASE(orlix_hosted_tls_repair_finds_recent_tpidr_base),
	KUNIT_CASE(orlix_hosted_tls_repair_rebases_from_live_host_tls),
	KUNIT_CASE(orlix_hosted_tls_repair_stops_at_intervening_write),
	KUNIT_CASE(orlix_hosted_tls_repair_rejects_invalid_user_tls),
	{}
};

static struct kunit_suite orlix_hosted_tls_repair_test_suite = {
	.name = "orlix-hosted-tls-repair",
	.test_cases = orlix_hosted_tls_repair_test_cases,
};

kunit_test_suite(orlix_hosted_tls_repair_test_suite);
