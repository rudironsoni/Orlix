// SPDX-License-Identifier: GPL-2.0-only
/* Production resume and canonical source-binding tests for native proof. */
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>
#include <kunit/test.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/syscalls.h>

#include "../gadget_program.h"
#include "orlix_tcti_native_observation.h"

#define NATIVE_OBSERVATION_NOP_ORDINAL 2238U
#define NATIVE_OBSERVATION_ADD_ORDINAL 2177U
#define NATIVE_OBSERVATION_HINT_ORDINAL 2270U
#define NATIVE_OBSERVATION_NOP 0xd503201fU
#define NATIVE_OBSERVATION_ADD_X0_X0_1 0x91000400U
#define NATIVE_OBSERVATION_SVC 0xd4000001U

static int native_observation_production_init(struct kunit *test)
{
	struct mm_struct *mm = mm_alloc();

	if (!mm)
		return -ENOMEM;
	test->priv = mm;
	return 0;
}

static void native_observation_production_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (mm)
		mmput(mm);
}

static bool native_observation_attach_mm(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	KUNIT_EXPECT_NOT_NULL(test, mm);
	KUNIT_EXPECT_NULL(test, current->mm);
	if (!mm || current->mm)
		return false;
	kthread_use_mm(mm);
	return true;
}

static void native_observation_detach_mm(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	KUNIT_EXPECT_PTR_EQ(test, current->mm, mm);
	if (current->mm == mm)
		kthread_unuse_mm(mm);
}

static int native_observation_map_image(struct kunit *test, u32 instruction,
					int protection,
					unsigned long *mapped_address)
{
	const u32 program[] = { instruction, NATIVE_OBSERVATION_SVC };
	unsigned long mapped;
	int ret;

	if (!mapped_address)
		return -EINVAL;
	*mapped_address = 0;
	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_EXPECT_FALSE(test, IS_ERR_VALUE(mapped));
	if (IS_ERR_VALUE(mapped))
		return (int)(long)mapped;
	ret = orlix_tcti_write_user_data(current->mm, mapped, program,
					 sizeof(program));
	KUNIT_EXPECT_EQ(test, 0, ret);
	if (ret)
		goto unmap;
	if (protection != (PROT_READ | PROT_WRITE)) {
		ret = sys_mprotect(mapped, PAGE_SIZE, protection);
		KUNIT_EXPECT_EQ(test, 0, ret);
		if (ret)
			goto unmap;
	}
	*mapped_address = mapped;
	return 0;

unmap:
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	return ret;
}

static void native_observation_seed_spec(
		struct orlix_tcti_native_observation_spec *spec,
		struct pt_regs *regs, unsigned long mapped, u32 ordinal)
{
	struct pt_regs expected = {};

	*regs = (struct pt_regs) {
		.sp = 0x20000,
		.pc = mapped,
		.pstate = PSR_MODE_EL0t | PSR_N_BIT,
		.syscallno = NO_SYSCALL,
	};
	regs->regs[0] = 0x1234;
	expected = *regs;
	expected.pc = mapped + sizeof(u32);
	*spec = (struct orlix_tcti_native_observation_spec) {
		.source_ordinal = ordinal,
		.obligation = ORLIX_TCTI_NATIVE_OBLIGATION_GPR,
		.result = {
			.reason = ORLIX_TCTI_EXIT_SYSCALL,
			.status = 0,
			.fault_access = ORLIX_TCTI_ACCESS_FETCH,
			.pc = mapped + sizeof(u32),
			.instruction = NATIVE_OBSERVATION_SVC,
		},
	};
	orlix_tcti_native_gpr_capture(&spec->gpr, &expected);
}

static bool native_observation_expect_rejection(
	struct kunit *test, unsigned long mapped, u32 ordinal, int expected_error)
{
	struct orlix_tcti_native_observation_spec spec;
	struct orlix_tcti_native_observation *observation;
	struct pt_regs regs;

	native_observation_seed_spec(&spec, &regs, mapped, ordinal);
	observation = orlix_tcti_native_observation_create(&spec);
	KUNIT_EXPECT_NOT_NULL(test, observation);
	if (!observation)
		return false;
	KUNIT_EXPECT_EQ(test, expected_error,
			orlix_tcti_native_observation_execute(
				observation, current, &regs, current->mm));
	KUNIT_EXPECT_EQ(test, -EPERM,
			orlix_tcti_native_observation_compare(observation));
	orlix_tcti_native_observation_destroy(observation);
	return true;
}

static void native_observation_binds_canonical_ordinal_and_executes_once(
		struct kunit *test)
{
	struct orlix_tcti_native_observation_spec spec;
	struct orlix_tcti_native_observation *observation = NULL;
	struct pt_regs regs;
	unsigned long mapped = 0;
	int ret;

	if (!native_observation_attach_mm(test))
		return;
	ret = native_observation_map_image(
		test, NATIVE_OBSERVATION_NOP, PROT_READ | PROT_EXEC, &mapped);
	if (ret)
		goto cleanup;
	native_observation_seed_spec(&spec, &regs, mapped,
				     NATIVE_OBSERVATION_NOP_ORDINAL);
	observation = orlix_tcti_native_observation_create(&spec);
	KUNIT_EXPECT_NOT_NULL(test, observation);
	if (!observation)
		goto cleanup;
	KUNIT_EXPECT_EQ(test, 0,
			orlix_tcti_native_observation_execute(
				observation, current, &regs, current->mm));
	KUNIT_EXPECT_EQ(test, 0,
			orlix_tcti_native_observation_compare(observation));
	KUNIT_EXPECT_EQ(test, -EALREADY,
			orlix_tcti_native_observation_execute(
				observation, current, &regs, current->mm));
	KUNIT_EXPECT_EQ(test, -EPERM,
			orlix_tcti_native_observation_compare(observation));

cleanup:
	orlix_tcti_native_observation_destroy(observation);
	if (mapped)
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	native_observation_detach_mm(test);
}

static void native_observation_wrong_ordinal_or_encoding_never_binds(
		struct kunit *test)
{
	unsigned long nop_mapped = 0;
	unsigned long add_mapped = 0;
	int ret;

	if (!native_observation_attach_mm(test))
		return;
	ret = native_observation_map_image(
		test, NATIVE_OBSERVATION_NOP, PROT_READ | PROT_EXEC,
		&nop_mapped);
	if (ret)
		goto cleanup;
	ret = native_observation_map_image(
		test, NATIVE_OBSERVATION_ADD_X0_X0_1, PROT_READ | PROT_EXEC,
		&add_mapped);
	if (ret)
		goto cleanup;

	if (!native_observation_expect_rejection(
			test, nop_mapped, NATIVE_OBSERVATION_ADD_ORDINAL,
			-EBADMSG))
		goto cleanup;
	/* NOP also matches broad HINT, but only NOP is uniquely most specific. */
	if (!native_observation_expect_rejection(
			test, nop_mapped, NATIVE_OBSERVATION_HINT_ORDINAL,
			-ENOTUNIQ))
		goto cleanup;
	native_observation_expect_rejection(
		test, add_mapped, NATIVE_OBSERVATION_NOP_ORDINAL, -EBADMSG);

cleanup:
	if (nop_mapped)
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(nop_mapped, PAGE_SIZE));
	if (add_mapped)
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(add_mapped, PAGE_SIZE));
	native_observation_detach_mm(test);
}

static void native_observation_rejects_range_and_execution_context(
		struct kunit *test)
{
	struct orlix_tcti_native_observation_spec spec;
	struct orlix_tcti_native_observation *observation = NULL;
	struct mm_struct *other_mm = NULL;
	struct pt_regs regs;
	unsigned long mapped = 0;
	int ret;

	if (!native_observation_attach_mm(test))
		return;
	ret = native_observation_map_image(
		test, NATIVE_OBSERVATION_NOP, PROT_READ | PROT_EXEC, &mapped);
	if (ret)
		goto cleanup;
	if (!native_observation_expect_rejection(test, mapped, 4350U, -ERANGE))
		goto cleanup;

	other_mm = mm_alloc();
	KUNIT_EXPECT_NOT_NULL(test, other_mm);
	if (!other_mm)
		goto cleanup;
	native_observation_seed_spec(&spec, &regs, mapped,
				     NATIVE_OBSERVATION_NOP_ORDINAL);
	observation = orlix_tcti_native_observation_create(&spec);
	KUNIT_EXPECT_NOT_NULL(test, observation);
	if (!observation)
		goto cleanup;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			orlix_tcti_native_observation_execute(
				observation, current, &regs, other_mm));
	KUNIT_EXPECT_EQ(test, -EPERM,
			orlix_tcti_native_observation_compare(observation));
	orlix_tcti_native_observation_destroy(observation);
	observation = NULL;

	native_observation_seed_spec(&spec, &regs, mapped,
				     NATIVE_OBSERVATION_NOP_ORDINAL);
	observation = orlix_tcti_native_observation_create(&spec);
	KUNIT_EXPECT_NOT_NULL(test, observation);
	if (!observation)
		goto cleanup;
	KUNIT_EXPECT_FALSE(test, current == current->real_parent);
	if (current != current->real_parent) {
		KUNIT_EXPECT_EQ(test, -EINVAL,
			orlix_tcti_native_observation_execute(
				observation, current->real_parent, &regs,
				current->mm));
		KUNIT_EXPECT_EQ(test, -EPERM,
			orlix_tcti_native_observation_compare(observation));
	}

cleanup:
	orlix_tcti_native_observation_destroy(observation);
	if (other_mm)
		mmput(other_mm);
	if (mapped)
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	native_observation_detach_mm(test);
}

struct native_observation_mutation_context {
	struct mm_struct *mm;
	unsigned long pc;
	u32 replacement;
	int ret;
};

static void native_observation_mutate_before_authorization(void *data)
{
	struct native_observation_mutation_context *context = data;

	context->ret = orlix_tcti_write_user_data(
		context->mm, context->pc, &context->replacement,
		sizeof(context->replacement));
}

static void native_observation_rejects_replaced_entry(
		struct kunit *test, bool prewarm)
{
	struct native_observation_mutation_context context = {
		.replacement = NATIVE_OBSERVATION_ADD_X0_X0_1,
	};
	struct orlix_tcti_native_observation_spec spec;
	struct orlix_tcti_native_observation *observation = NULL;
	struct pt_regs regs;
	unsigned long mapped = 0;
	int ret;

	if (!native_observation_attach_mm(test))
		return;
	context.mm = current->mm;
	ret = native_observation_map_image(
		test, NATIVE_OBSERVATION_NOP,
		PROT_READ | PROT_WRITE | PROT_EXEC, &mapped);
	if (ret)
		goto cleanup;
	context.pc = mapped;

	if (prewarm) {
		struct orlix_tcti_result result;

		native_observation_seed_spec(&spec, &regs, mapped,
					     NATIVE_OBSERVATION_NOP_ORDINAL);
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	}

	native_observation_seed_spec(&spec, &regs, mapped,
				     NATIVE_OBSERVATION_NOP_ORDINAL);
	observation = orlix_tcti_native_observation_create(&spec);
	KUNIT_EXPECT_NOT_NULL(test, observation);
	if (!observation)
		goto cleanup;
	orlix_tcti_gadget_program_set_pre_authorized_test_hook(
		native_observation_mutate_before_authorization, &context);

	ret = orlix_tcti_native_observation_execute(
		observation, current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, 0, context.ret);
	KUNIT_EXPECT_EQ(test, -EBADMSG, ret);
	KUNIT_EXPECT_EQ(test, 0x1235ULL, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, -EPERM,
			orlix_tcti_native_observation_compare(observation));

cleanup:
	orlix_tcti_gadget_program_set_pre_authorized_test_hook(NULL, NULL);
	orlix_tcti_native_observation_destroy(observation);
	if (mapped)
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	native_observation_detach_mm(test);
}

static void native_observation_rejects_replaced_entry_cold(struct kunit *test)
{
	native_observation_rejects_replaced_entry(test, false);
}

static void native_observation_rejects_replaced_entry_prewarmed(
		struct kunit *test)
{
	native_observation_rejects_replaced_entry(test, true);
}

static struct kunit_case native_observation_production_test_cases[] = {
	KUNIT_CASE(native_observation_binds_canonical_ordinal_and_executes_once),
	KUNIT_CASE(native_observation_wrong_ordinal_or_encoding_never_binds),
	KUNIT_CASE(native_observation_rejects_range_and_execution_context),
	KUNIT_CASE(native_observation_rejects_replaced_entry_cold),
	KUNIT_CASE(native_observation_rejects_replaced_entry_prewarmed),
	{}
};

static struct kunit_suite native_observation_production_test_suite = {
	.name = "orlix-tcti-native-observation-production",
	.init = native_observation_production_init,
	.exit = native_observation_production_exit,
	.test_cases = native_observation_production_test_cases,
};

kunit_test_suite(native_observation_production_test_suite);

MODULE_LICENSE("GPL");
