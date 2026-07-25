// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched/mm.h>
#include <linux/syscalls.h>
#include <asm/ptrace.h>
#include <asm/tcti.h>

#include "../decode_aarch64.h"
#include "target_lse_operation_catalog.h"

/*
 * AARCHMRS 2026-06 source ordinals 3000 through 3299 contain all base-LSE
 * scalar atomic-RMW leaves.  This suite executes the 144 FEAT_LSE leaves in
 * that interval through mapped RX text and tcti_resume_user().  It proves
 * deterministic state transitions only.  Concurrent ordering remains a
 * separate atomicity and ordering obligation.
 */
#define TCTI_LSE_SCALAR_RMW_SVC 0xd4000001U
#define TCTI_LSE_SCALAR_RMW_REGISTER_FIELDS \
	((0x1fU << 16) | (0x1fU << 5) | 0x1fU)

static int tcti_lse_scalar_rmw_resume_test_init(struct kunit *test)
{
	struct mm_struct *mm = mm_alloc();

	if (!mm)
		return -ENOMEM;
	kthread_use_mm(mm);
	test->priv = mm;
	return 0;
}

static void tcti_lse_scalar_rmw_resume_test_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (!mm)
		return;
	kthread_unuse_mm(mm);
	mmput(mm);
}

static bool tcti_lse_scalar_rmw_entry_in_scope(
	const struct tcti_lse_operation_catalog_entry *entry)
{
	return entry->source_ordinal >= 3000U && entry->source_ordinal <= 3299U &&
		entry->cohort == TCTI_LSE_OPERATION_COHORT_BASE_LSE &&
		entry->semantic_class == TCTI_LSE_OPERATION_SEMANTIC_ATOMIC_RMW;
}

static unsigned long tcti_lse_scalar_rmw_map(struct kunit *test, int prot)
{
	unsigned long address;

	address = ksys_mmap_pgoff(0, PAGE_SIZE, prot,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_EXPECT_FALSE(test, IS_ERR_VALUE(address));
	return address;
}

static int tcti_lse_scalar_rmw_write_program(unsigned long address,
					       u32 instruction)
{
	const u32 program[] = { instruction, TCTI_LSE_SCALAR_RMW_SVC };
	int ret;

	ret = sys_mprotect(address, PAGE_SIZE, PROT_READ | PROT_WRITE);
	if (ret)
		return ret;
	ret = tcti_write_user_data(current->mm, address, program,
				   sizeof(program));
	if (ret)
		return ret;
	return sys_mprotect(address, PAGE_SIZE, PROT_READ | PROT_EXEC);
}

static u32 tcti_lse_scalar_rmw_instruction(
	const struct tcti_lse_operation_catalog_entry *entry)
{
	return (entry->encoding_pattern & ~TCTI_LSE_SCALAR_RMW_REGISTER_FIELDS) |
		(6U << 16) | (10U << 5) | 8U;
}

static void tcti_lse_scalar_rmw_assert_encoding(struct kunit *test,
	const struct tcti_lse_operation_catalog_entry *entry, u32 instruction)
{
	KUNIT_ASSERT_EQ_MSG(test, entry->encoding_pattern,
			    instruction & entry->encoding_mask, "%s source=%u",
			    entry->source_leaf, entry->source_ordinal);
}

static u64 tcti_lse_scalar_rmw_mask(u8 width)
{
	return width == sizeof(u64) ? U64_MAX : (1ULL << (width * 8)) - 1;
}

static u64 tcti_lse_scalar_rmw_expected(enum tcti_lse_atomic_op operation,
					 u64 old, u64 operand, u8 width)
{
	u64 mask = tcti_lse_scalar_rmw_mask(width);
	s64 signed_old = sign_extend64(old, width * 8 - 1);
	s64 signed_operand = sign_extend64(operand, width * 8 - 1);

	switch (operation) {
	case TCTI_LSE_ATOMIC_SWP:
		return operand & mask;
	case TCTI_LSE_ATOMIC_ADD:
		return (old + operand) & mask;
	case TCTI_LSE_ATOMIC_CLR:
		return old & ~operand & mask;
	case TCTI_LSE_ATOMIC_EOR:
		return (old ^ operand) & mask;
	case TCTI_LSE_ATOMIC_SET:
		return (old | operand) & mask;
	case TCTI_LSE_ATOMIC_SMAX:
		return (signed_old > signed_operand ? old : operand) & mask;
	case TCTI_LSE_ATOMIC_SMIN:
		return (signed_old < signed_operand ? old : operand) & mask;
	case TCTI_LSE_ATOMIC_UMAX:
		return (old > operand ? old : operand) & mask;
	case TCTI_LSE_ATOMIC_UMIN:
		return (old < operand ? old : operand) & mask;
	case TCTI_LSE_ATOMIC_CAS:
		break;
	}
	return 0;
}

static void tcti_lse_scalar_rmw_init_regs(struct pt_regs *regs,
					   unsigned long instructions,
					   unsigned long data, u64 operand)
{
	*regs = (struct pt_regs) {};
	regs->regs[6] = operand;
	regs->regs[8] = operand ^ U64_MAX;
	regs->regs[10] = data;
	regs->pc = instructions;
	regs->pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT |
		PSR_C_BIT | PSR_V_BIT;
	regs->syscallno = NO_SYSCALL;
}

static void tcti_lse_scalar_rmw_executes_every_source_leaf(struct kunit *test)
{
	const struct tcti_lse_operation_catalog_entry *entries;
	const u64 original_old = 0x8123456789abcdefULL;
	const u64 original_operand = 0x1020304050607081ULL;
	const u64 expected_pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT |
		PSR_C_BIT | PSR_V_BIT;
	unsigned long instructions = tcti_lse_scalar_rmw_map(test,
							     PROT_READ | PROT_WRITE);
	unsigned long data = tcti_lse_scalar_rmw_map(test,
						      PROT_READ | PROT_WRITE);
	size_t count;
	size_t index;
	size_t exercised = 0;

	if (IS_ERR_VALUE(instructions) || IS_ERR_VALUE(data))
		return;
	entries = tcti_lse_operation_catalog(&count);
	KUNIT_ASSERT_NOT_NULL(test, entries);
	for (index = 0; index < count; index++) {
		const struct tcti_lse_operation_catalog_entry *entry = &entries[index];
		u32 instruction;
		struct tcti_decoded_instruction decoded;
		struct pt_regs regs;
		struct pt_regs expected_regs;
		struct tcti_result result;
		u64 mask;
		u64 old;
		u64 operand;
		u64 observed = 0;
		int ret;

		if (!tcti_lse_scalar_rmw_entry_in_scope(entry))
			continue;
		instruction = tcti_lse_scalar_rmw_instruction(entry);
		tcti_lse_scalar_rmw_assert_encoding(test, entry, instruction);
		decoded = tcti_decode_aarch64(instruction);
		KUNIT_ASSERT_EQ_MSG(test, TCTI_DECODE_LSE_ATOMIC,
				    decoded.decode_class, "%s", entry->source_leaf);
		KUNIT_ASSERT_NE_MSG(test, TCTI_LSE_ATOMIC_CAS,
				    decoded.lse_atomic_op, "%s", entry->source_leaf);
		KUNIT_ASSERT_FALSE_MSG(test, decoded.pair, "%s", entry->source_leaf);
		KUNIT_ASSERT_TRUE_MSG(test, decoded.access_size == sizeof(u8) ||
				      decoded.access_size == sizeof(u16) ||
				      decoded.access_size == sizeof(u32) ||
				      decoded.access_size == sizeof(u64), "%s",
				      entry->source_leaf);
		mask = tcti_lse_scalar_rmw_mask(decoded.access_size);
		old = original_old & mask;
		operand = original_operand & mask;
		ret = tcti_lse_scalar_rmw_write_program(instructions, instruction);
		KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s", entry->source_leaf);
		ret = tcti_write_user_data(current->mm, data, &old,
					   decoded.access_size);
		KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s", entry->source_leaf);
		tcti_lse_scalar_rmw_init_regs(&regs, instructions, data, operand);
		expected_regs = regs;
		expected_regs.regs[8] = old;
		expected_regs.pc = instructions + sizeof(u32);
		result = tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, TCTI_EXIT_SYSCALL, result.reason, "%s",
				    entry->source_leaf);
		KUNIT_EXPECT_EQ_MSG(test, 0L, result.status, "%s",
				    entry->source_leaf);
		KUNIT_EXPECT_EQ_MSG(test, instructions + sizeof(u32), result.pc,
				    "%s", entry->source_leaf);
		KUNIT_EXPECT_EQ_MSG(test, TCTI_LSE_SCALAR_RMW_SVC,
				    result.instruction, "%s", entry->source_leaf);
		KUNIT_EXPECT_EQ_MSG(test, instructions + sizeof(u32), regs.pc,
				    "%s", entry->source_leaf);
		KUNIT_EXPECT_EQ_MSG(test, expected_pstate, regs.pstate, "%s",
				    entry->source_leaf);
		KUNIT_EXPECT_MEMEQ_MSG(test, &expected_regs, &regs, sizeof(regs),
				       "%s source=%u", entry->source_leaf,
				       entry->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, operand, regs.regs[6], "%s",
				    entry->source_leaf);
		KUNIT_EXPECT_EQ_MSG(test, old, regs.regs[8], "%s",
				    entry->source_leaf);
		ret = tcti_read_user_data(current->mm, data, &observed,
					  decoded.access_size);
		KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s", entry->source_leaf);
		KUNIT_EXPECT_EQ_MSG(test,
				tcti_lse_scalar_rmw_expected(decoded.lse_atomic_op, old,
						     operand, decoded.access_size), observed,
				"%s", entry->source_leaf);
		exercised++;
	}
	KUNIT_EXPECT_EQ(test, 144U, exercised);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(instructions, PAGE_SIZE));
}

static void tcti_lse_scalar_rmw_readonly_faults_do_not_mutate(struct kunit *test)
{
	const struct tcti_lse_operation_catalog_entry *entries;
	const u64 initial = 0x8123456789abcdefULL;
	unsigned long instructions = tcti_lse_scalar_rmw_map(test,
							     PROT_READ | PROT_WRITE);
	unsigned long data = tcti_lse_scalar_rmw_map(test,
						      PROT_READ | PROT_WRITE);
	bool width_seen[sizeof(u64) + 1] = {};
	size_t count;
	size_t index;

	if (IS_ERR_VALUE(instructions) || IS_ERR_VALUE(data))
		return;
	entries = tcti_lse_operation_catalog(&count);
	KUNIT_ASSERT_NOT_NULL(test, entries);
	for (index = 0; index < count; index++) {
		const struct tcti_lse_operation_catalog_entry *entry = &entries[index];
		u32 instruction;
		struct tcti_decoded_instruction decoded;
		struct pt_regs regs;
		struct pt_regs before;
		struct tcti_result result;
		u64 observed = 0;
		u64 masked_initial;
		int ret;

		if (!tcti_lse_scalar_rmw_entry_in_scope(entry))
			continue;
		instruction = tcti_lse_scalar_rmw_instruction(entry);
		tcti_lse_scalar_rmw_assert_encoding(test, entry, instruction);
		decoded = tcti_decode_aarch64(instruction);
		if (width_seen[decoded.access_size])
			continue;
		width_seen[decoded.access_size] = true;
		masked_initial = initial & tcti_lse_scalar_rmw_mask(decoded.access_size);
		ret = tcti_lse_scalar_rmw_write_program(instructions, instruction);
		KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s", entry->source_leaf);
		ret = tcti_write_user_data(current->mm, data, &masked_initial,
					   decoded.access_size);
		KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s", entry->source_leaf);
		tcti_lse_scalar_rmw_init_regs(&regs, instructions, data, 1);
		before = regs;
		KUNIT_ASSERT_EQ(test, 0, sys_mprotect(data, PAGE_SIZE, PROT_READ));
		result = tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, TCTI_EXIT_USER_FAULT, result.reason,
				    "%s", entry->source_leaf);
		KUNIT_EXPECT_EQ_MSG(test, -EACCES, result.status, "%s",
				    entry->source_leaf);
		KUNIT_EXPECT_EQ_MSG(test, data, result.fault_address, "%s",
				    entry->source_leaf);
		KUNIT_EXPECT_EQ_MSG(test, TCTI_ACCESS_WRITE, result.fault_access,
				    "%s", entry->source_leaf);
		KUNIT_EXPECT_EQ_MSG(test, instructions, result.pc, "%s",
				    entry->source_leaf);
		KUNIT_EXPECT_EQ_MSG(test, instruction, result.instruction, "%s",
				    entry->source_leaf);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		KUNIT_ASSERT_EQ(test, 0, sys_mprotect(data, PAGE_SIZE,
						       PROT_READ | PROT_WRITE));
		ret = tcti_read_user_data(current->mm, data, &observed,
					  decoded.access_size);
		KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s", entry->source_leaf);
		KUNIT_EXPECT_EQ_MSG(test, masked_initial, observed, "%s",
				    entry->source_leaf);
	}
	KUNIT_EXPECT_TRUE(test, width_seen[sizeof(u8)]);
	KUNIT_EXPECT_TRUE(test, width_seen[sizeof(u16)]);
	KUNIT_EXPECT_TRUE(test, width_seen[sizeof(u32)]);
	KUNIT_EXPECT_TRUE(test, width_seen[sizeof(u64)]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(instructions, PAGE_SIZE));
}

static struct kunit_case tcti_lse_scalar_rmw_resume_test_cases[] = {
	KUNIT_CASE(tcti_lse_scalar_rmw_executes_every_source_leaf),
	KUNIT_CASE(tcti_lse_scalar_rmw_readonly_faults_do_not_mutate),
	{}
};

static struct kunit_suite tcti_lse_scalar_rmw_resume_test_suite = {
	.name = "orlix-tcti-lse-scalar-rmw-resume",
	.init = tcti_lse_scalar_rmw_resume_test_init,
	.exit = tcti_lse_scalar_rmw_resume_test_exit,
	.test_cases = tcti_lse_scalar_rmw_resume_test_cases,
};
kunit_test_suite(tcti_lse_scalar_rmw_resume_test_suite);

MODULE_LICENSE("GPL");
