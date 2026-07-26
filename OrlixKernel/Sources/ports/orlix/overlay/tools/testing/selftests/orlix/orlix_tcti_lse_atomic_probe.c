// SPDX-License-Identifier: GPL-2.0
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/wait.h>

#include "orlix_kselftest_user.h"

#define PUBLICATION_SPIN_LIMIT 4096U
#define PUBLICATION_PAYLOAD 0x6f726c69782d6c73ULL

struct lse_pair {
	uint64_t low;
	uint64_t high;
} __attribute__((aligned(16)));

struct publication_state {
	uint64_t flag;
	uint64_t payload;
	uint64_t producer_observed;
	uint64_t consumer_observed;
};

static const void *fault_expected_address;
static const volatile unsigned char *fault_memory_start;
static size_t fault_memory_size;

static uint64_t execute_casal(uint64_t *address, uint64_t expected,
			      uint64_t desired)
{
	uint64_t observed;

	asm volatile(
		"mov x0, %[expected]\n"
		"mov x1, %[desired]\n"
		"mov x2, %[address]\n"
		".inst 0xc8e0fc41\n" /* casal x0, x1, [x2] */
		"mov %[observed], x0\n"
		: [observed] "=r" (observed)
		: [address] "r" (address), [expected] "r" (expected),
		  [desired] "r" (desired)
		: "x0", "x1", "x2", "memory");
	return observed;
}

static struct lse_pair execute_caspal(struct lse_pair *address,
				      struct lse_pair expected,
				      struct lse_pair desired)
{
	struct lse_pair observed;

	asm volatile(
		"mov x0, %[expected_low]\n"
		"mov x1, %[expected_high]\n"
		"mov x2, %[desired_low]\n"
		"mov x3, %[desired_high]\n"
		"mov x4, %[address]\n"
		".inst 0x4860fc82\n" /* caspal x0, x1, x2, x3, [x4] */
		"mov %[observed_low], x0\n"
		"mov %[observed_high], x1\n"
		: [observed_low] "=r" (observed.low),
		  [observed_high] "=r" (observed.high)
		: [address] "r" (address),
		  [expected_low] "r" (expected.low),
		  [expected_high] "r" (expected.high),
		  [desired_low] "r" (desired.low),
		  [desired_high] "r" (desired.high)
		: "x0", "x1", "x2", "x3", "x4", "memory");
	return observed;
}

static uint64_t execute_ldaddal(uint64_t *address, uint64_t addend)
{
	uint64_t observed;

	asm volatile(
		"mov x0, %[addend]\n"
		"mov x2, %[address]\n"
		".inst 0xf8e00041\n" /* ldaddal x0, x1, [x2] */
		"mov %[observed], x1\n"
		: [observed] "=r" (observed)
		: [address] "r" (address), [addend] "r" (addend)
		: "x0", "x1", "x2", "memory");
	return observed;
}

static uint64_t execute_swpal(uint64_t *address, uint64_t desired)
{
	uint64_t observed;

	asm volatile(
		"mov x0, %[desired]\n"
		"mov x2, %[address]\n"
		".inst 0xf8e08041\n" /* swpal x0, x1, [x2] */
		"mov %[observed], x1\n"
		: [observed] "=r" (observed)
		: [address] "r" (address), [desired] "r" (desired)
		: "x0", "x1", "x2", "memory");
	return observed;
}

static void store_plain(uint64_t *address, uint64_t value)
{
	asm volatile("str %[value], [%[address]]"
		     :
		     : [address] "r" (address), [value] "r" (value)
		     : "memory");
}

static uint64_t load_plain(const uint64_t *address)
{
	uint64_t value;

	asm volatile("ldr %[value], [%[address]]"
		     : [value] "=r" (value)
		     : [address] "r" (address)
		     : "memory");
	return value;
}

static bool casal_success_preserves_register_and_memory_results(void)
{
	uint64_t value = 0x1020304050607080ULL;
	uint64_t observed;

	observed = execute_casal(&value, 0x1020304050607080ULL,
				0x8877665544332211ULL);
	return observed == 0x1020304050607080ULL &&
	       value == 0x8877665544332211ULL;
}

static bool casal_failure_preserves_register_and_memory_results(void)
{
	uint64_t value = 0x1020304050607080ULL;
	uint64_t observed;

	observed = execute_casal(&value, 0xffffffffffffffffULL,
				0x8877665544332211ULL);
	return observed == 0x1020304050607080ULL &&
	       value == 0x1020304050607080ULL;
}

static bool caspal_preserves_pair_register_and_memory_results(void)
{
	struct lse_pair value = {
		.low = 0x0123456789abcdefULL,
		.high = 0xfedcba9876543210ULL,
	};
	const struct lse_pair expected = value;
	const struct lse_pair desired = {
		.low = 0x1122334455667788ULL,
		.high = 0x8877665544332211ULL,
	};
	struct lse_pair observed;

	observed = execute_caspal(&value, expected, desired);
	return observed.low == expected.low &&
	       observed.high == expected.high &&
	       value.low == desired.low &&
	       value.high == desired.high;
}

static bool caspal_failure_preserves_pair_register_and_memory_results(void)
{
	struct lse_pair value = {
		.low = 0x0123456789abcdefULL,
		.high = 0xfedcba9876543210ULL,
	};
	const struct lse_pair expected = {
		.low = 0xffffffffffffffffULL,
		.high = 0xeeeeeeeeeeeeeeeeULL,
	};
	const struct lse_pair desired = {
		.low = 0x1122334455667788ULL,
		.high = 0x8877665544332211ULL,
	};
	struct lse_pair observed;

	observed = execute_caspal(&value, expected, desired);
	return observed.low == 0x0123456789abcdefULL &&
	       observed.high == 0xfedcba9876543210ULL &&
	       value.low == 0x0123456789abcdefULL &&
	       value.high == 0xfedcba9876543210ULL;
}

static bool ldaddal_preserves_register_and_memory_results(void)
{
	uint64_t value = 0x1000000000000001ULL;
	uint64_t observed;

	observed = execute_ldaddal(&value, 0x20ULL);
	return observed == 0x1000000000000001ULL &&
	       value == 0x1000000000000021ULL;
}

static bool swpal_preserves_register_and_memory_results(void)
{
	uint64_t value = 0x0123456789abcdefULL;
	uint64_t observed;

	observed = execute_swpal(&value, 0xfedcba9876543210ULL);
	return observed == 0x0123456789abcdefULL &&
	       value == 0xfedcba9876543210ULL;
}

static void *publication_consumer(void *arg)
{
	struct publication_state *state = arg;
	unsigned int spin;

	for (spin = 0; spin < PUBLICATION_SPIN_LIMIT; spin++) {
		if (execute_ldaddal(&state->flag, 0) == 1) {
			state->consumer_observed = load_plain(&state->payload);
			return NULL;
		}
		(void)sched_yield();
	}
	return NULL;
}

static void *publication_producer(void *arg)
{
	struct publication_state *state = arg;

	store_plain(&state->payload, PUBLICATION_PAYLOAD);
	state->producer_observed = execute_swpal(&state->flag, 1);
	return NULL;
}

static bool lse_acquire_release_publishes_between_threads(void)
{
	struct publication_state state = {};
	pthread_t consumer;
	pthread_t producer;
	bool producer_created;

	if (pthread_create(&consumer, NULL, publication_consumer, &state))
		return false;
	producer_created =
		pthread_create(&producer, NULL, publication_producer, &state) == 0;
	if (producer_created && pthread_join(producer, NULL))
		producer_created = false;
	if (pthread_join(consumer, NULL))
		return false;

	return producer_created &&
	       state.producer_observed == 0 &&
	       state.flag == 1 &&
	       state.consumer_observed == PUBLICATION_PAYLOAD;
}

static void alignment_fault_handler(int signal, siginfo_t *info, void *context)
{
	size_t i;

	(void)context;
	if (signal != SIGBUS)
		_exit(1);
	if (!info)
		_exit(2);
	if (info->si_code != BUS_ADRALN)
		_exit(3);
	if (info->si_addr != fault_expected_address)
		_exit(4);
	for (i = 0; i < fault_memory_size; i++) {
		if (fault_memory_start[i] != 0)
			_exit(5);
	}
	_exit(0);
}

static bool install_alignment_fault_handler(void)
{
	struct sigaction action = {};

	action.sa_sigaction = alignment_fault_handler;
	action.sa_flags = SA_SIGINFO;
	if (sigemptyset(&action.sa_mask))
		return false;
	return sigaction(SIGBUS, &action, NULL) == 0;
}

static bool child_reported_exact_alignment_fault(pid_t child)
{
	int status;

	if (child < 0 || waitpid(child, &status, 0) != child)
		return false;
	return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static bool misaligned_scalar_lse_reports_exact_bus_adraln(void)
{
	pid_t child = fork();

	if (child == 0) {
		unsigned char storage[32] __attribute__((aligned(16))) = {};
		uint64_t *misaligned = (uint64_t *)(storage + 1);

		fault_expected_address = misaligned;
		fault_memory_start = storage;
		fault_memory_size = sizeof(storage);
		if (!install_alignment_fault_handler())
			_exit(6);
		(void)execute_ldaddal(misaligned, 1);
		_exit(7);
	}
	return child_reported_exact_alignment_fault(child);
}

static bool misaligned_casp_reports_exact_bus_adraln(void)
{
	pid_t child = fork();

	if (child == 0) {
		unsigned char storage[32] __attribute__((aligned(16))) = {};
		struct lse_pair *misaligned =
			(struct lse_pair *)(storage + sizeof(uint64_t));
		const struct lse_pair expected = {
			.low = 0x0123456789abcdefULL,
			.high = 0xfedcba9876543210ULL,
		};
		const struct lse_pair desired = {
			.low = 0x1122334455667788ULL,
			.high = 0x8877665544332211ULL,
		};

		fault_expected_address = misaligned;
		fault_memory_start = storage;
		fault_memory_size = sizeof(storage);
		if (!install_alignment_fault_handler())
			_exit(6);
		(void)execute_caspal(misaligned, expected, desired);
		_exit(7);
	}
	return child_reported_exact_alignment_fault(child);
}

int main(void)
{
	orlix_test_plan(9);
	orlix_test_result(
		casal_success_preserves_register_and_memory_results(),
		"CASAL success returns old value and stores replacement");
	orlix_test_result(
		casal_failure_preserves_register_and_memory_results(),
		"CASAL failure returns old value without storing");
	orlix_test_result(
		caspal_preserves_pair_register_and_memory_results(),
		"CASPAL returns old pair and stores replacement pair");
	orlix_test_result(
		caspal_failure_preserves_pair_register_and_memory_results(),
		"CASPAL failure returns old pair without storing");
	orlix_test_result(
		ldaddal_preserves_register_and_memory_results(),
		"LDADDAL returns old value and adds to memory");
	orlix_test_result(
		swpal_preserves_register_and_memory_results(),
		"SWPAL returns old value and swaps memory");
	orlix_test_result(
		lse_acquire_release_publishes_between_threads(),
		"LSE acquire release publishes payload between threads");
	orlix_test_result(
		misaligned_scalar_lse_reports_exact_bus_adraln(),
		"misaligned scalar LSE reports exact SIGBUS address without memory effects");
	orlix_test_result(
		misaligned_casp_reports_exact_bus_adraln(),
		"misaligned CASP reports exact SIGBUS address without memory effects");
	orlix_test_exit();
}
