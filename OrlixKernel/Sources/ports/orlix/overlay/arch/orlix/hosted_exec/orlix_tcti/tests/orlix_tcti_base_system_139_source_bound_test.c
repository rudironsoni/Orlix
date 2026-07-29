// SPDX-License-Identifier: GPL-2.0-only
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>
#include <kunit/test.h>
#include <linux/completion.h>
#include <linux/atomic.h>
#include <linux/err.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/syscalls.h>

#include "../base_system_139.h"
#include "../decode_aarch64.h"
#include "../gadget_program.h"
#include "target_execution_slice_map.h"

#define BASE_SYSTEM_139_SVC 0xd4000001U

static void orlix_tcti_base_system_139_sb_has_direct_feature_on_semantics(
	struct kunit *test);
static void orlix_tcti_base_system_139_dsb_nxs_feature_on_and_off_boundaries(
	struct kunit *test);
static void orlix_tcti_base_system_139_pstate_flag_feature_on_and_off_boundaries(
	struct kunit *test);
static void orlix_tcti_base_system_139_msr_pstate_immediate_partitions_fields(
	struct kunit *test);
static void orlix_tcti_base_system_139_two_context_dmb_ordering_interaction(
	struct kunit *test);
struct base_system_139_feature_state_worker {
	struct completion ready;
	struct completion done;
	bool initially_enabled;
	bool enabled_after_local_set;
};

static int base_system_139_feature_state_worker(void *data)
{
	struct base_system_139_feature_state_worker *worker = data;

	worker->initially_enabled = orlix_tcti_private_feature_state_enabled(current);
	orlix_tcti_set_private_feature_state(current, true);
	worker->enabled_after_local_set =
		orlix_tcti_private_feature_state_enabled(current);
	complete(&worker->ready);
	wait_for_completion(&worker->done);
	orlix_tcti_set_private_feature_state(current, false);
	return 0;
}

static void orlix_tcti_base_system_139_feature_state_lifecycle(struct kunit *test)
{
	struct base_system_139_feature_state_worker worker = { };
	struct task_struct *task;

	orlix_tcti_set_private_feature_state(current, false);
	KUNIT_EXPECT_FALSE(test, orlix_tcti_private_feature_state_enabled(current));
	init_completion(&worker.ready);
	init_completion(&worker.done);
	task = kthread_run(base_system_139_feature_state_worker, &worker,
		"orlix-tcti-feature-state");
	KUNIT_ASSERT_FALSE(test, IS_ERR(task));
	KUNIT_ASSERT_NE(test, 0UL, wait_for_completion_timeout(&worker.ready, HZ));
	KUNIT_EXPECT_FALSE(test, worker.initially_enabled);
	KUNIT_EXPECT_TRUE(test, worker.enabled_after_local_set);
	KUNIT_EXPECT_FALSE(test, orlix_tcti_private_feature_state_enabled(current));
	orlix_tcti_set_private_feature_state(current, true);
	KUNIT_EXPECT_TRUE(test, orlix_tcti_private_feature_state_enabled(current));
	complete(&worker.done);
	orlix_tcti_set_private_feature_state(current, false);
	KUNIT_EXPECT_FALSE(test, orlix_tcti_private_feature_state_enabled(current));
}

static void orlix_tcti_base_system_139_wfxt_feature_on_and_off(
	struct kunit *test);
static void orlix_tcti_base_system_139_feature_state_lifecycle(struct kunit *test);

static unsigned long base_system_139_map(struct kunit *test, u32 instruction)
{
	u32 program[] = { instruction, BASE_SYSTEM_139_SVC };
	unsigned long mapped;
	int ret;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	ret = orlix_tcti_write_user_data(current->mm, mapped, program,
					 sizeof(program));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = sys_mprotect(mapped, PAGE_SIZE, PROT_READ | PROT_EXEC);
	KUNIT_ASSERT_EQ(test, 0, ret);
	return mapped;
}

static void orlix_tcti_base_system_139_exact_cohort_is_source_bound(
	struct kunit *test)
{
	static const u32 ordinals[] = {
		ORLIX_TCTI_BASE_SYSTEM_139_LEAF_ORDINALS,
	};
	const struct orlix_tcti_execution_slice_map *map =
		orlix_tcti_execution_slice_map_canonical();
	size_t index;
	size_t member;
	u32 base_system_family = UINT_MAX;

	KUNIT_ASSERT_EQ(test, ORLIX_TCTI_BASE_SYSTEM_139_LEAF_COUNT,
			ARRAY_SIZE(ordinals));
	KUNIT_ASSERT_NOT_NULL(test, map);
	for (index = 0; index < map->counts.family_count; index++)
		if (map->families[index].issue_id == 139U) {
			KUNIT_ASSERT_EQ(test, UINT_MAX, base_system_family);
			base_system_family = index;
		}
	KUNIT_ASSERT_NE(test, UINT_MAX, base_system_family);
	KUNIT_ASSERT_EQ(test, ORLIX_TCTI_BASE_SYSTEM_139_LEAF_COUNT,
			map->families[base_system_family].declared_member_count);
	for (index = 0; index < ARRAY_SIZE(ordinals); index++) {
		bool found = false;

		for (member = 0; member < map->counts.leaf_count; member++) {
			if (map->members[member].family_index != base_system_family ||
			    map->members[member].ordinal != ordinals[index])
				continue;
			KUNIT_EXPECT_FALSE_MSG(test, found, "ordinal %u duplicated",
				ordinals[index]);
			found = true;
		}
		KUNIT_EXPECT_TRUE_MSG(test, found, "ordinal %u absent", ordinals[index]);
		for (member = 0; member < index; member++)
			KUNIT_EXPECT_NE_MSG(test, ordinals[index], ordinals[member],
				"ordinal %u duplicated in #139 manifest", ordinals[index]);
	}
	for (member = 0; member < map->counts.leaf_count; member++) {
		bool found = false;

		if (map->members[member].family_index != base_system_family)
			continue;
		for (index = 0; index < ARRAY_SIZE(ordinals); index++)
			if (ordinals[index] == map->members[member].ordinal)
				found = true;
		KUNIT_EXPECT_TRUE_MSG(test, found, "unexpected #139 ordinal %u",
			map->members[member].ordinal);
	}
}

static void orlix_tcti_base_system_139_feature_hints_have_named_feature_on_semantics(
	struct kunit *test)
{
	static const struct orlix_tcti_base_system_139_feature_leaf leaves[] = {
		ORLIX_TCTI_BASE_SYSTEM_139_FEATURE_HINT_LEAVES,
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(leaves); index++) {
		const struct orlix_tcti_base_system_139_feature_leaf *leaf = &leaves[index];
		struct orlix_tcti_decoded_instruction decoded;
		struct pt_regs regs = { };
		struct pt_regs before;
		struct orlix_tcti_result result;
		unsigned long mapped;

		decoded = orlix_tcti_decode_aarch64(leaf->pattern);
		KUNIT_ASSERT_EQ_MSG(test, ORLIX_TCTI_DECODE_FEATURE_HINT,
				    decoded.decode_class, "source ordinal %u",
				    leaf->source_ordinal);
		KUNIT_EXPECT_EQ(test, leaf->source_ordinal, decoded.source_ordinal);
		KUNIT_EXPECT_EQ(test, leaf->operation, decoded.feature_hint_op);
		mapped = base_system_139_map(test, leaf->pattern);
		regs.pc = mapped;
		regs.sp = STACK_TOP - 16;
		regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT | PSR_V_BIT;
		regs.regs[0] = 0x123456789abcdef0ULL;
		regs.regs[30] = 0x0fedcba987654321ULL;
		before = regs;
		orlix_tcti_set_private_feature_state(current, true);
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		orlix_tcti_set_private_feature_state(current, false);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
		KUNIT_EXPECT_EQ(test, before.pc + 2 * sizeof(u32), regs.pc);
		before.pc = regs.pc;
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));

		/* Flipping one fixed bit cannot select this named leaf. */
		KUNIT_EXPECT_NE(test, ORLIX_TCTI_DECODE_FEATURE_HINT,
			orlix_tcti_decode_aarch64(leaf->pattern ^
				(leaf->mask & -leaf->mask)).decode_class);
	}
	orlix_tcti_base_system_139_sb_has_direct_feature_on_semantics(test);
	orlix_tcti_base_system_139_feature_state_lifecycle(test);
}

struct base_system_139_clrex_worker {
	struct completion ready;
	struct completion release;
};

static int base_system_139_clrex_worker_fn(void *data)
{
	struct base_system_139_clrex_worker *worker = data;

	complete(&worker->ready);
	wait_for_completion(&worker->release);
	return 0;
}

static void orlix_tcti_base_system_139_clrex_is_current_monitor_only(
	struct kunit *test)
{
	struct base_system_139_clrex_worker worker = { };
	struct task_struct *task;
	struct pt_regs regs = { };
	struct pt_regs before;
	struct orlix_tcti_result result;
	unsigned long mapped = base_system_139_map(test, 0xd503305fU);

	init_completion(&worker.ready);
	init_completion(&worker.release);
	task = kthread_run(base_system_139_clrex_worker_fn, &worker,
		"orlix-tcti-clrex");
	KUNIT_ASSERT_FALSE(test, IS_ERR(task));
	KUNIT_ASSERT_NE(test, 0UL, wait_for_completion_timeout(&worker.ready, HZ));
	/* The worker is blocked and cannot be cleared by a task switch after this. */
	task->thread.user_exclusive_address = 0x12340000UL;
	task->thread.user_exclusive_valid = 1;
	current->thread.user_exclusive_address = 0x56780000UL;
	current->thread.user_exclusive_valid = 1;
	regs.pc = mapped;
	regs.sp = STACK_TOP - 16;
	regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT;
	regs.regs[2] = 0x123456789abcdef0ULL;
	before = regs;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, before.pc + 2 * sizeof(u32), regs.pc);
	KUNIT_EXPECT_EQ(test, 0, current->thread.user_exclusive_valid);
	KUNIT_EXPECT_EQ(test, 0x12340000UL,
		task->thread.user_exclusive_address);
	KUNIT_EXPECT_EQ(test, 1, task->thread.user_exclusive_valid);
	KUNIT_EXPECT_EQ(test, before.regs[2], regs.regs[2]);
	KUNIT_EXPECT_EQ(test, before.pstate, regs.pstate);
	complete(&worker.release);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void orlix_tcti_base_system_139_feature_hints_are_rejected_when_unadvertised(
	struct kunit *test)
{
	static const struct orlix_tcti_base_system_139_feature_leaf leaves[] = {
		ORLIX_TCTI_BASE_SYSTEM_139_FEATURE_HINT_LEAVES,
	};
	size_t index;

	KUNIT_ASSERT_EQ(test, 0UL, (unsigned long)ELF_HWCAP);
	KUNIT_ASSERT_EQ(test, 0UL, (unsigned long)ELF_HWCAP2);
	for (index = 0; index < ARRAY_SIZE(leaves); index++) {
		const struct orlix_tcti_base_system_139_feature_leaf *leaf = &leaves[index];
		struct pt_regs regs = { };
		struct pt_regs before;
		struct orlix_tcti_result result;
		unsigned long mapped;

		mapped = base_system_139_map(test, leaf->pattern);
		regs.pc = mapped;
		regs.sp = STACK_TOP - 16;
		regs.pstate = PSR_MODE_EL0t;
		regs.syscallno = NO_SYSCALL;
		regs.regs[0] = 0x123456789abcdef0ULL;
		before = regs;
		orlix_tcti_set_private_feature_state(current, false);
		result = orlix_tcti_resume_user(current, &regs, current->mm);

		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				    result.reason, "source ordinal %u",
				    leaf->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, -EOPNOTSUPP, result.status,
				    "source ordinal %u", leaf->source_ordinal);
		KUNIT_EXPECT_EQ(test, leaf->pattern, result.instruction);
		KUNIT_EXPECT_EQ(test, before.pc, result.pc);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	}
	orlix_tcti_base_system_139_dsb_nxs_feature_on_and_off_boundaries(test);
}

static void orlix_tcti_base_system_139_pauth_precedes_generic_hint(
	struct kunit *test)
{
	struct orlix_tcti_decoded_instruction decoded =
		orlix_tcti_decode_aarch64(0xd503231fU);

	/* PACIAZ is PAuth precedence, never BTI or generic HINT. */
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_FEATURE_UNAVAILABLE,
		decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 2256U, decoded.source_ordinal);
}

static void orlix_tcti_base_system_139_bti_precedes_generic_hint(
	struct kunit *test)
{
	static const u32 encodings[] = {
		0xd503241fU, 0xd503245fU, 0xd503249fU, 0xd50324dfU,
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(encodings); index++)
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
				orlix_tcti_decode_aarch64(encodings[index]).decode_class);
}

static void orlix_tcti_base_system_139_baseline_yield_hints_use_resume_path(
	struct kunit *test)
{
	static const struct {
		u32 ordinal;
		u32 instruction;
		enum orlix_tcti_exit_reason reason;
		long status;
		u32 pc_instructions;
	} leaves[] = {
		{ 2238U, 0xd503201fU, ORLIX_TCTI_EXIT_SYSCALL, 0, 2U },
		{ 2239U, 0xd503203fU, ORLIX_TCTI_EXIT_YIELD, 1, 1U },
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(leaves); index++) {
		const typeof(leaves[0]) *leaf = &leaves[index];
		struct pt_regs regs = { };
		struct pt_regs before;
		struct orlix_tcti_result result;
		unsigned long mapped;

		mapped = base_system_139_map(test, leaf->instruction);
		regs.pc = mapped;
		regs.sp = STACK_TOP - 16;
		regs.pstate = PSR_MODE_EL0t;
		regs.syscallno = NO_SYSCALL;
		regs.regs[0] = 0x123456789abcdef0ULL;
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, leaf->reason, result.reason,
				    "source ordinal %u", leaf->ordinal);
		KUNIT_EXPECT_EQ(test, leaf->status, result.status);
		KUNIT_EXPECT_EQ(test,
				mapped + leaf->pc_instructions * sizeof(u32), result.pc);
		before.pc = result.pc;
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	}
}

static void orlix_tcti_base_system_139_events_use_typed_resume_exits(
	struct kunit *test)
{
	static const struct {
		u32 instruction;
		u32 ordinal;
		enum orlix_tcti_event_op operation;
	} leaves[] = {
		{ 0xd503205fU, 2240U, ORLIX_TCTI_EVENT_WFE },
		{ 0xd503207fU, 2241U, ORLIX_TCTI_EVENT_WFI },
		{ 0xd503209fU, 2242U, ORLIX_TCTI_EVENT_SEV },
		{ 0xd50320bfU, 2243U, ORLIX_TCTI_EVENT_SEVL },
	};
	struct orlix_tcti_decoded_instruction decoded;
	struct pt_regs regs = { };
	struct orlix_tcti_result result;
	unsigned long mapped;
	size_t index;

	for (index = 0; index < ARRAY_SIZE(leaves); index++) {
		decoded = orlix_tcti_decode_aarch64(leaves[index].instruction);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_EVENT, decoded.decode_class);
		KUNIT_EXPECT_EQ(test, leaves[index].ordinal, decoded.source_ordinal);
		KUNIT_EXPECT_EQ(test, leaves[index].operation, decoded.event_op);
	}

	orlix_tcti_event_reset_task(current);
	mapped = base_system_139_map(test, 0xd503205fU);
	regs.pc = mapped;
	regs.sp = STACK_TOP - 16;
	regs.pstate = PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_WAIT, result.reason);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_WAIT_WFE, result.wait_kind);
	KUNIT_EXPECT_EQ(test, mapped + sizeof(u32), result.pc);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	orlix_tcti_base_system_139_two_context_dmb_ordering_interaction(test);

	mapped = base_system_139_map(test, 0xd503207fU);
	regs.pc = mapped;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_WAIT, result.reason);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_WAIT_WFI, result.wait_kind);
	KUNIT_EXPECT_EQ(test, mapped + sizeof(u32), result.pc);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));

	/* SEVL creates one local event; WFE consumes it and reaches SVC. */
	mapped = base_system_139_map(test, 0xd50320bfU);
	regs.pc = mapped;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, mapped + 2 * sizeof(u32), result.pc);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	mapped = base_system_139_map(test, 0xd503205fU);
	regs.pc = mapped;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, mapped + 2 * sizeof(u32), result.pc);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	orlix_tcti_base_system_139_wfxt_feature_on_and_off(test);
}

static void orlix_tcti_base_system_139_wfxt_feature_on_and_off(
	struct kunit *test)
{
	static const struct {
		u32 instruction;
		u32 ordinal;
		enum orlix_tcti_event_op operation;
		int direct_status;
	} leaves[] = {
		{ 0xd5031005U, 2236U, ORLIX_TCTI_EVENT_WFET, -EWOULDBLOCK },
		{ 0xd5031025U, 2237U, ORLIX_TCTI_EVENT_WFIT, -EINPROGRESS },
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(leaves); index++) {
		struct orlix_tcti_decoded_instruction decoded =
			orlix_tcti_decode_aarch64(leaves[index].instruction);
		struct pt_regs regs = { };
		struct pt_regs before;
		struct orlix_tcti_result result;
		unsigned long mapped;

		KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DECODE_EVENT, decoded.decode_class);
		KUNIT_EXPECT_EQ(test, leaves[index].ordinal, decoded.source_ordinal);
		KUNIT_EXPECT_EQ(test, leaves[index].operation, decoded.event_op);
		KUNIT_EXPECT_TRUE(test, decoded.event_timeout);
		KUNIT_EXPECT_EQ(test, 5U, decoded.rt);
		mapped = base_system_139_map(test, leaves[index].instruction);
		regs.pc = mapped;
		regs.sp = STACK_TOP - 16;
		regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT;
		regs.regs[5] = 37;
		before = regs;
		orlix_tcti_event_reset_task(current);
		orlix_tcti_set_private_feature_state(current, true);
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		orlix_tcti_set_private_feature_state(current, false);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_WAIT, result.reason);
		KUNIT_EXPECT_EQ(test, 37, result.status);
		KUNIT_EXPECT_EQ(test, before.pc + sizeof(u32), regs.pc);
		KUNIT_EXPECT_EQ(test, before.pstate, regs.pstate);
		KUNIT_EXPECT_EQ(test, before.regs[5], regs.regs[5]);

		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
		mapped = base_system_139_map(test, leaves[index].instruction);
		regs = before;
		regs.pc = mapped;
		regs.sp = STACK_TOP - 16;
		regs.syscallno = NO_SYSCALL;
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
			result.reason);
		KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	}
}

static void orlix_tcti_base_system_139_csdb_is_not_generic_hint(struct kunit *test)
{
	struct orlix_tcti_decoded_instruction decoded =
		orlix_tcti_decode_aarch64(0xd503229fU);

	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_SPECULATION_BARRIER,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 2254U, decoded.source_ordinal);
}

struct base_system_139_event_worker {
	struct completion ready;
	struct completion release;
	struct completion done;
	bool initially_consumed;
	bool sev_consumed;
};

static int base_system_139_event_worker(void *data)
{
	struct base_system_139_event_worker *worker = data;

	orlix_tcti_event_reset_task(current);
	worker->initially_consumed = orlix_tcti_event_wfe_consumed(NULL);
	complete(&worker->ready);
	wait_for_completion(&worker->release);
	worker->sev_consumed = orlix_tcti_event_wfe_consumed(NULL);
	complete(&worker->done);
	return 0;
}

static void orlix_tcti_base_system_139_sev_reaches_another_virtual_pe(
	struct kunit *test)
{
	struct base_system_139_event_worker worker;
	struct task_struct *task;

	init_completion(&worker.ready);
	init_completion(&worker.release);
	init_completion(&worker.done);
	task = kthread_run(base_system_139_event_worker, &worker,
		"orlix-tcti-event");
	KUNIT_ASSERT_FALSE(test, IS_ERR(task));
	KUNIT_ASSERT_NE(test, 0UL,
		wait_for_completion_timeout(&worker.ready, HZ));
	KUNIT_EXPECT_FALSE(test, worker.initially_consumed);
	orlix_tcti_event_sev();
	complete(&worker.release);
	KUNIT_EXPECT_NE(test, 0UL,
		wait_for_completion_timeout(&worker.done, HZ));
	KUNIT_EXPECT_TRUE(test, worker.sev_consumed);
}

static void orlix_tcti_base_system_139_feature_forms_are_typed_rejections(
	struct kunit *test)
{
	static const struct orlix_tcti_base_system_139_feature_leaf leaves[] = {
		ORLIX_TCTI_BASE_SYSTEM_139_FEATURE_UNAVAILABLE_LEAVES,
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(leaves); index++) {
		const struct orlix_tcti_base_system_139_feature_leaf *leaf = &leaves[index];
		struct orlix_tcti_decoded_instruction decoded =
			orlix_tcti_decode_aarch64(leaf->pattern);
		struct pt_regs regs = { };
		struct pt_regs before;
		struct orlix_tcti_result result;
		unsigned long mapped;

		KUNIT_ASSERT_EQ_MSG(test, ORLIX_TCTI_DECODE_FEATURE_UNAVAILABLE,
				    decoded.decode_class, "source ordinal %u",
				    leaf->source_ordinal);
		KUNIT_EXPECT_EQ(test, leaf->source_ordinal, decoded.source_ordinal);
		mapped = base_system_139_map(test, leaf->pattern);
		regs.pc = mapped;
		regs.sp = STACK_TOP - 16;
		regs.pstate = PSR_MODE_EL0t;
		regs.syscallno = NO_SYSCALL;
		regs.regs[0] = 0x123456789abcdef0ULL;
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				result.reason);
		KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
		KUNIT_EXPECT_EQ(test, before.pc, result.pc);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	}
	orlix_tcti_base_system_139_pstate_flag_feature_on_and_off_boundaries(test);
	orlix_tcti_base_system_139_msr_pstate_immediate_partitions_fields(test);
}

static void orlix_tcti_base_system_139_sb_has_direct_feature_on_semantics(
	struct kunit *test)
{
	const u32 instruction = 0xd50330ffU;
	struct orlix_tcti_decoded_instruction decoded =
		orlix_tcti_decode_aarch64(instruction);
	struct pt_regs regs = { };
	struct pt_regs before;
	struct orlix_tcti_result result;
	unsigned long mapped;

	KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DECODE_FEATURE_HINT, decoded.decode_class);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_FEATURE_HINT_SB, decoded.feature_hint_op);
	KUNIT_EXPECT_EQ(test, 2275U, decoded.source_ordinal);
	mapped = base_system_139_map(test, instruction);
	regs.pc = mapped;
	regs.sp = STACK_TOP - 16;
	regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT;
	regs.regs[3] = 0x123456789abcdef0ULL;
	before = regs;
	orlix_tcti_set_private_feature_state(current, true);
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	orlix_tcti_set_private_feature_state(current, false);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, before.pc + 2 * sizeof(u32), regs.pc);
	before.pc = regs.pc;
	KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	mapped = base_system_139_map(test, instruction);
	regs.pc = mapped;
	regs.sp = STACK_TOP - 16;
	regs.pstate = PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;
	before = regs;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION, result.reason);
	KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
	KUNIT_EXPECT_EQ(test, before.pc, result.pc);
	KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void orlix_tcti_base_system_139_dsb_nxs_feature_on_and_off_boundaries(
	struct kunit *test)
{
	static const u8 legal_options[] = { 2U, 6U, 10U, 14U };
	size_t index;

	for (index = 0; index < ARRAY_SIZE(legal_options); index++) {
		u32 instruction = 0xd503303fU | ((u32)legal_options[index] << 8);
		struct orlix_tcti_decoded_instruction decoded =
			orlix_tcti_decode_aarch64(instruction);
		struct pt_regs regs = { };
		struct pt_regs before;
		struct orlix_tcti_result result;
		unsigned long mapped;

		KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DECODE_BARRIER, decoded.decode_class);
		KUNIT_EXPECT_TRUE(test, decoded.barrier_nxs);
		KUNIT_EXPECT_EQ(test, 2276U, decoded.source_ordinal);
		KUNIT_EXPECT_EQ(test, legal_options[index], decoded.barrier_option);
		mapped = base_system_139_map(test, instruction);
		regs.pc = mapped;
		regs.sp = STACK_TOP - 16;
		regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT;
		regs.regs[3] = 0x123456789abcdef0ULL;
		before = regs;
		orlix_tcti_set_private_feature_state(current, true);
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		orlix_tcti_set_private_feature_state(current, false);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
		KUNIT_EXPECT_EQ(test, before.pc + 2 * sizeof(u32), regs.pc);
		before.pc = regs.pc;
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));

		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
		mapped = base_system_139_map(test, instruction);
		regs.pc = mapped;
		regs.sp = STACK_TOP - 16;
		regs.pstate = PSR_MODE_EL0t;
		regs.syscallno = NO_SYSCALL;
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				result.reason);
		KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	}
	for (index = 0; index < 16U; index++)
		if (index != 2U && index != 6U && index != 10U && index != 14U)
			KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
				orlix_tcti_decode_aarch64(0xd503303fU |
					((u32)index << 8)).decode_class);
}

static u64 base_system_139_pstate_flag_result(u64 pstate,
					       enum orlix_tcti_pstate_flag_op operation)
{
	bool z = pstate & PSR_Z_BIT;
	bool c = pstate & PSR_C_BIT;
	u64 nzcv = pstate & (PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT);

	switch (operation) {
	case ORLIX_TCTI_PSTATE_FLAG_CFINV:
		return pstate ^ PSR_C_BIT;
	case ORLIX_TCTI_PSTATE_FLAG_XAFLAG:
		nzcv = (!c && !z ? PSR_N_BIT : 0) |
			(z && c ? PSR_Z_BIT : 0) |
			(c || z ? PSR_C_BIT : 0) |
			(!c && z ? PSR_V_BIT : 0);
		break;
	case ORLIX_TCTI_PSTATE_FLAG_AXFLAG:
		nzcv = (z || (pstate & PSR_V_BIT) ? PSR_Z_BIT : 0) |
			(c && !(pstate & PSR_V_BIT) ? PSR_C_BIT : 0);
		break;
	}
	return (pstate & ~(PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT)) | nzcv;
}

static void orlix_tcti_base_system_139_pstate_flag_feature_on_and_off_boundaries(
	struct kunit *test)
{
	static const struct {
		u32 instruction;
		u32 ordinal;
		enum orlix_tcti_pstate_flag_op operation;
	} leaves[] = {
		{ 0xd500401fU, 2278U, ORLIX_TCTI_PSTATE_FLAG_CFINV },
		{ 0xd500403fU, 2279U, ORLIX_TCTI_PSTATE_FLAG_XAFLAG },
		{ 0xd500405fU, 2280U, ORLIX_TCTI_PSTATE_FLAG_AXFLAG },
	};
	size_t index;
	u32 flags;

	for (index = 0; index < ARRAY_SIZE(leaves); index++) {
		for (flags = 0; flags < 16U; flags++) {
			struct orlix_tcti_decoded_instruction decoded =
				orlix_tcti_decode_aarch64(leaves[index].instruction);
			struct pt_regs regs = { };
			struct pt_regs before;
			struct orlix_tcti_result result;
			unsigned long mapped;

			KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DECODE_PSTATE_FLAG,
					decoded.decode_class);
			mapped = base_system_139_map(test, leaves[index].instruction);
			regs.pc = mapped;
			regs.sp = STACK_TOP - 16;
			regs.pstate = PSR_MODE_EL0t | (flags << 28);
			regs.regs[4] = 0x123456789abcdef0ULL;
			before = regs;
			orlix_tcti_set_private_feature_state(current, true);
			result = orlix_tcti_resume_user(current, &regs, current->mm);
			orlix_tcti_set_private_feature_state(current, false);
			KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
			KUNIT_EXPECT_EQ(test, before.pc + 2 * sizeof(u32), regs.pc);
			KUNIT_EXPECT_EQ(test, base_system_139_pstate_flag_result(
				before.pstate, leaves[index].operation), regs.pstate);
			KUNIT_EXPECT_EQ(test, before.regs[4], regs.regs[4]);
			KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
		}
		{
			struct pt_regs regs = { };
			struct pt_regs before;
			struct orlix_tcti_result result;
			unsigned long mapped = base_system_139_map(test,
				leaves[index].instruction);

			regs.pc = mapped;
			regs.sp = STACK_TOP - 16;
			regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT;
			regs.syscallno = NO_SYSCALL;
			before = regs;
			result = orlix_tcti_resume_user(current, &regs, current->mm);
			KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				result.reason);
			KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
			KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
			KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
		}
	}
}

static void orlix_tcti_base_system_139_msr_pstate_immediate_partitions_fields(
	struct kunit *test)
{
	u32 op1;
	u32 op2;
	u32 imm;

	for (op1 = 0; op1 < 8U; op1++)
		for (op2 = 0; op2 < 16U; op2++)
			for (imm = 0; imm < 8U; imm++) {
				u32 instruction = 0xd500401fU | (op1 << 16) |
					(op2 << 8) | (imm << 5);
				struct orlix_tcti_decoded_instruction decoded =
					orlix_tcti_decode_aarch64(instruction);
				struct pt_regs regs = { };
				struct pt_regs before;
				struct orlix_tcti_gadget_word
					program[ORLIX_TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS];
				size_t word_count;

				if (decoded.decode_class == ORLIX_TCTI_DECODE_PSTATE_FLAG)
					continue;
				KUNIT_EXPECT_EQ_MSG(test,
					ORLIX_TCTI_DECODE_SYSTEM_PSTATE_IMMEDIATE,
					decoded.decode_class, "op1=%u op2=%u imm=%u",
					op1, op2, imm);
				KUNIT_EXPECT_EQ(test, 2277U, decoded.source_ordinal);
				regs.pc = 0x4000;
				regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT;
				regs.regs[0] = 0x123456789abcdef0ULL;
				before = regs;
				KUNIT_ASSERT_EQ(test, 0,
					orlix_tcti_lower_decoded_instruction(&decoded, program,
						ARRAY_SIZE(program), &word_count));
				KUNIT_EXPECT_EQ(test, -EOPNOTSUPP,
					orlix_tcti_execute_gadget_program(NULL, &regs, program,
						word_count, NULL));
				KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
			}
}

struct base_system_139_ordering_worker {
	struct completion ready;
	struct completion done;
	struct mm_struct *mm;
	unsigned long payload_address;
	unsigned long published_address;
	unsigned long producer_barrier_pc;
	unsigned long consumer_barrier_pc;
	u64 observed;
	bool producer_barrier_executed;
	bool consumer_barrier_executed;
	int producer_ret;
	int consumer_ret;
};

static int base_system_139_resume_barrier(struct mm_struct *mm, unsigned long pc)
{
	struct pt_regs regs = { };
	struct orlix_tcti_result result;

	regs.pc = pc;
	regs.sp = STACK_TOP - 16;
	regs.pstate = PSR_MODE_EL0t;
	result = orlix_tcti_resume_user(current, &regs, mm);
	return result.reason == ORLIX_TCTI_EXIT_SYSCALL ? 0 : result.status;
}

static int base_system_139_ordering_producer(void *data)
{
	struct base_system_139_ordering_worker *worker = data;

	u64 payload = 0x123456789abcdef0ULL;
	u64 published = 1;

	worker->producer_ret = orlix_tcti_write_user_data(worker->mm,
		worker->payload_address, &payload, sizeof(payload));
	if (worker->producer_ret)
		goto out;
	worker->producer_ret = base_system_139_resume_barrier(worker->mm,
		worker->producer_barrier_pc); /* DMB ISH guest text */
	if (worker->producer_ret)
		goto out;
	worker->producer_ret = orlix_tcti_write_user_data(worker->mm,
		worker->published_address, &published, sizeof(published));
	worker->producer_barrier_executed = !worker->producer_ret;
out:
	complete(&worker->ready);
	return 0;
}

static int base_system_139_ordering_consumer(void *data)
{
	struct base_system_139_ordering_worker *worker = data;

	u64 published = 0;

	while (!published) {
		worker->consumer_ret = orlix_tcti_read_user_data(worker->mm,
			worker->published_address, &published, sizeof(published));
		if (worker->consumer_ret)
			goto out;
		cond_resched();
	}
	worker->consumer_ret = base_system_139_resume_barrier(worker->mm,
		worker->consumer_barrier_pc); /* DMB ISHLD guest text */
	if (!worker->consumer_ret)
		worker->consumer_ret = orlix_tcti_read_user_data(worker->mm,
			worker->payload_address, &worker->observed,
			sizeof(worker->observed));
	worker->consumer_barrier_executed = !worker->consumer_ret;
out:
	complete(&worker->done);
	return 0;
}

static void orlix_tcti_base_system_139_two_context_dmb_ordering_interaction(
	struct kunit *test)
{
	struct base_system_139_ordering_worker worker = { };
	struct task_struct *producer;
	struct task_struct *consumer;
	unsigned long mapped;
	unsigned long producer_barrier;
	unsigned long consumer_barrier;

	init_completion(&worker.ready);
	init_completion(&worker.done);
	worker.mm = current->mm;
	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	worker.payload_address = mapped;
	worker.published_address = mapped + sizeof(u64);
	producer_barrier = base_system_139_map(test, 0xd50330bfU | (11U << 8));
	consumer_barrier = base_system_139_map(test, 0xd50330bfU | (9U << 8));
	worker.producer_barrier_pc = producer_barrier;
	worker.consumer_barrier_pc = consumer_barrier;
	consumer = kthread_run(base_system_139_ordering_consumer, &worker,
		"orlix-tcti-dmb-consumer");
	KUNIT_ASSERT_FALSE(test, IS_ERR(consumer));
	producer = kthread_run(base_system_139_ordering_producer, &worker,
		"orlix-tcti-dmb-producer");
	KUNIT_ASSERT_FALSE(test, IS_ERR(producer));
	KUNIT_ASSERT_NE(test, 0UL,
		wait_for_completion_timeout(&worker.ready, HZ));
	KUNIT_ASSERT_NE(test, 0UL,
		wait_for_completion_timeout(&worker.done, HZ));
	KUNIT_EXPECT_EQ(test, 0, worker.producer_ret);
	KUNIT_EXPECT_EQ(test, 0, worker.consumer_ret);
	KUNIT_EXPECT_EQ(test, 0x123456789abcdef0ULL, worker.observed);
	KUNIT_EXPECT_TRUE(test, worker.producer_barrier_executed);
	KUNIT_EXPECT_TRUE(test, worker.consumer_barrier_executed);
	KUNIT_ASSERT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	KUNIT_ASSERT_EQ(test, 0, vm_munmap(producer_barrier, PAGE_SIZE));
	KUNIT_ASSERT_EQ(test, 0, vm_munmap(consumer_barrier, PAGE_SIZE));

}

static void orlix_tcti_base_system_139_barrier_partition_uses_resume_user(
	struct kunit *test)
{
	static const u8 options[] = { 1, 2, 3, 5, 6, 7, 9, 10, 11, 13, 14, 15 };
	size_t index;

	for (index = 0; index < ARRAY_SIZE(options) * 2; index++) {
		bool dmb = index >= ARRAY_SIZE(options);
		u8 option = options[index % ARRAY_SIZE(options)];
		u32 instruction = (dmb ? 0xd50330bfU : 0xd503309fU) |
			((u32)option << 8);
		struct pt_regs regs = { };
		struct pt_regs before;
		struct orlix_tcti_result result;
		unsigned long mapped = base_system_139_map(test, instruction);

		regs.pc = mapped;
		regs.sp = STACK_TOP - 16;
		regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT;
		regs.regs[4] = 0x123456789abcdef0ULL;
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
		KUNIT_EXPECT_EQ(test, before.pc + 2 * sizeof(u32), regs.pc);
		KUNIT_EXPECT_EQ(test, before.regs[4], regs.regs[4]);
		KUNIT_EXPECT_EQ(test, before.pstate, regs.pstate);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	}
	for (index = 0; index < 16; index += 4) {
		u32 bases[] = { 0xd503309fU, 0xd50330bfU };
		size_t base;

		for (base = 0; base < ARRAY_SIZE(bases); base++) {
			struct pt_regs regs = { };
			struct pt_regs before;
			struct orlix_tcti_result result;
			unsigned long mapped = base_system_139_map(test,
				bases[base] | ((u32)index << 8));

			regs.pc = mapped;
			regs.sp = STACK_TOP - 16;
			regs.pstate = PSR_MODE_EL0t | PSR_N_BIT;
			regs.regs[1] = 0x1234;
			before = regs;
			result = orlix_tcti_resume_user(current, &regs, current->mm);
			KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				result.reason);
			KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
			KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
			KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
		}
	}
	for (index = 0; index < 16; index++) {
		struct pt_regs regs = { };
		struct pt_regs before;
		struct orlix_tcti_result result;
		unsigned long mapped = base_system_139_map(test,
			0xd50330dfU | ((u32)index << 8));

		regs.pc = mapped;
		regs.sp = STACK_TOP - 16;
		regs.pstate = PSR_MODE_EL0t;
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
			result.reason);
		KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	}
	for (index = 0; index < 16; index++) {
		struct pt_regs regs = { };
		struct pt_regs before;
		struct orlix_tcti_result result;
		unsigned long mapped = base_system_139_map(test,
			0xd503305fU | ((u32)index << 8));

		regs.pc = mapped;
		regs.sp = STACK_TOP - 16;
		regs.pstate = PSR_MODE_EL0t;
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		if (!index)
			KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
		else {
			KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				result.reason);
			KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
			KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		}
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	}
}

static void orlix_tcti_base_system_139_csdb_is_typed_data_value_sync(
	struct kunit *test)
{
	struct pt_regs regs = { };
	struct orlix_tcti_result result;
	unsigned long mapped = base_system_139_map(test, 0xd503229fU);

	regs.pc = mapped;
	regs.sp = STACK_TOP - 16;
	regs.pstate = PSR_MODE_EL0t;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void orlix_tcti_base_system_139_isb_advances_instruction_context(
	struct kunit *test)
{
	struct pt_regs regs = { };
	struct orlix_tcti_result result;
	u64 value = 1;
	unsigned long data;
	unsigned long mapped = base_system_139_map(test, 0xd5033fdfU);

	data = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(data));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_write_user_data(current->mm, data,
		&value, sizeof(value)));
	regs.pc = mapped;
	regs.sp = STACK_TOP - 16;
	regs.pstate = PSR_MODE_EL0t;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static u32 base_system_139_system_register(bool write, u16 selector, u8 rt)
{
	return (write ? 0xd5100000U : 0xd5300000U) |
		((u32)selector << 5) | rt;
}

static void orlix_tcti_base_system_139_system_register_selectors_use_resume_path(
	struct kunit *test)
{
	const u64 nzcv = PSR_N_BIT | PSR_C_BIT;
	struct pt_regs regs = { };
	struct orlix_tcti_result result;
	struct orlix_tcti_decoded_instruction decoded;
	unsigned long mapped;

	/* Ordinal 2283: a generated writable EL0 selector executes. */
	decoded = orlix_tcti_decode_aarch64(
		base_system_139_system_register(true, 0xda10U, 4U));
	KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DECODE_SYSTEM_REGISTER,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 2283U, decoded.source_ordinal);
	KUNIT_EXPECT_EQ(test, 0xda10U, decoded.system_accessor_selector);
	KUNIT_EXPECT_NE(test, 0U, decoded.system_accessor_id);
	KUNIT_EXPECT_TRUE(test, decoded.system_register_write);
	mapped = base_system_139_map(test, decoded.instruction);
	regs.pc = mapped;
	regs.sp = STACK_TOP - 16;
	regs.pstate = PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;
	regs.regs[4] = nzcv;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, mapped + 2 * sizeof(u32), result.pc);
	KUNIT_EXPECT_EQ(test, nzcv, regs.pstate &
			(PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));

	/* Ordinal 2284: a generated readable EL0 selector writes Rt after read. */
	decoded = orlix_tcti_decode_aarch64(
		base_system_139_system_register(false, 0xda10U, 5U));
	KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DECODE_SYSTEM_REGISTER,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 2284U, decoded.source_ordinal);
	KUNIT_EXPECT_EQ(test, 0xda10U, decoded.system_accessor_selector);
	KUNIT_EXPECT_NE(test, 0U, decoded.system_accessor_id);
	KUNIT_EXPECT_FALSE(test, decoded.system_register_write);
	mapped = base_system_139_map(test, decoded.instruction);
	memset(&regs, 0, sizeof(regs));
	regs.pc = mapped;
	regs.sp = STACK_TOP - 16;
	regs.pstate = PSR_MODE_EL0t | nzcv;
	regs.syscallno = NO_SYSCALL;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, mapped + 2 * sizeof(u32), result.pc);
	KUNIT_EXPECT_EQ(test, nzcv, regs.regs[5]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void orlix_tcti_base_system_139_selector_rejections_preserve_state(
	struct kunit *test)
{
	static const struct {
		u32 instruction;
		u32 ordinal;
		enum orlix_tcti_decode_class decode_class;
	} leaves[] = {
		{ 0xd50340dfU, 2277U, ORLIX_TCTI_DECODE_SYSTEM_PSTATE_IMMEDIATE },
		{ 0xd5080300U, 2281U, ORLIX_TCTI_DECODE_SYSTEM_INSTRUCTION },
		{ 0xd5280300U, 2282U, ORLIX_TCTI_DECODE_SYSTEM_INSTRUCTION },
		{ 0xd51b9d00U, 2283U, ORLIX_TCTI_DECODE_SYSTEM_REGISTER },
		{ 0xd53b9d00U, 2284U, ORLIX_TCTI_DECODE_SYSTEM_REGISTER },
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(leaves); index++) {
		const typeof(leaves[0]) *leaf = &leaves[index];
		struct orlix_tcti_decoded_instruction decoded =
			orlix_tcti_decode_aarch64(leaf->instruction);
		struct pt_regs regs = { };
		struct pt_regs before;
		struct orlix_tcti_result result;
		unsigned long mapped;

		KUNIT_ASSERT_EQ(test, leaf->decode_class, decoded.decode_class);
		KUNIT_EXPECT_EQ(test, leaf->ordinal, decoded.source_ordinal);
		mapped = base_system_139_map(test, leaf->instruction);
		regs.pc = mapped;
		regs.sp = STACK_TOP - 16;
		regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT;
		regs.syscallno = NO_SYSCALL;
		regs.regs[0] = 0x123456789abcdef0ULL;
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				result.reason);
		KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
		KUNIT_EXPECT_EQ(test, leaf->instruction, result.instruction);
		KUNIT_EXPECT_EQ(test, before.pc, result.pc);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	}
}

static struct kunit_case orlix_tcti_base_system_139_source_bound_cases[] = {
	KUNIT_CASE(orlix_tcti_base_system_139_exact_cohort_is_source_bound),
	KUNIT_CASE(orlix_tcti_base_system_139_feature_hints_have_named_feature_on_semantics),
	KUNIT_CASE(orlix_tcti_base_system_139_feature_hints_are_rejected_when_unadvertised),
	KUNIT_CASE(orlix_tcti_base_system_139_pauth_precedes_generic_hint),
	KUNIT_CASE(orlix_tcti_base_system_139_bti_precedes_generic_hint),
	KUNIT_CASE(orlix_tcti_base_system_139_baseline_yield_hints_use_resume_path),
	KUNIT_CASE(orlix_tcti_base_system_139_events_use_typed_resume_exits),
	KUNIT_CASE(orlix_tcti_base_system_139_csdb_is_not_generic_hint),
	KUNIT_CASE(orlix_tcti_base_system_139_csdb_is_typed_data_value_sync),
	KUNIT_CASE(orlix_tcti_base_system_139_clrex_is_current_monitor_only),
	KUNIT_CASE(orlix_tcti_base_system_139_barrier_partition_uses_resume_user),
	KUNIT_CASE(orlix_tcti_base_system_139_two_context_dmb_ordering_interaction),
	KUNIT_CASE(orlix_tcti_base_system_139_isb_advances_instruction_context),
	KUNIT_CASE(orlix_tcti_base_system_139_dsb_nxs_feature_on_and_off_boundaries),
	KUNIT_CASE(orlix_tcti_base_system_139_sev_reaches_another_virtual_pe),
	KUNIT_CASE(orlix_tcti_base_system_139_feature_forms_are_typed_rejections),
	KUNIT_CASE(orlix_tcti_base_system_139_system_register_selectors_use_resume_path),
	KUNIT_CASE(orlix_tcti_base_system_139_selector_rejections_preserve_state),
	{}
};

static struct kunit_suite orlix_tcti_base_system_139_source_bound_suite = {
	.name = "orlix-tcti-base-system-139-source-bound",
	.test_cases = orlix_tcti_base_system_139_source_bound_cases,
};

kunit_test_suite(orlix_tcti_base_system_139_source_bound_suite);

MODULE_LICENSE("GPL");
