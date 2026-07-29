// SPDX-License-Identifier: GPL-2.0-only
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>
#include <asm/mte.h>
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/completion.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/prctl.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/syscalls.h>
#include <asm/pgtable.h>

#include "../decode_aarch64.h"
#include "../switch_debug.h"
#include "orlix_tcti_test_suites.h"

#define MEMORY_SET_SOURCE_MASK 0x3fe0fc00U

struct orlix_tcti_memory_set_leaf {
	u16 ordinal;
	const char *name;
	u32 pattern;
	enum orlix_tcti_memory_set_phase phase;
	bool tagged;
	bool unprivileged;
	bool nontemporal;
};

/* Exact direct leaves from pinned AARCHMRS 2026-06 source_manifest.def. */
static const struct orlix_tcti_memory_set_leaf memory_set_leaves[] = {
	{ 2752, "SETP_SET_memcms", 0x19c00400U,
	  ORLIX_TCTI_MEMORY_SET_PROLOGUE, false, false, false },
	{ 2753, "SETPT_SET_memcms", 0x19c01400U,
	  ORLIX_TCTI_MEMORY_SET_PROLOGUE, false, true, false },
	{ 2754, "SETPN_SET_memcms", 0x19c02400U,
	  ORLIX_TCTI_MEMORY_SET_PROLOGUE, false, false, true },
	{ 2755, "SETPTN_SET_memcms", 0x19c03400U,
	  ORLIX_TCTI_MEMORY_SET_PROLOGUE, false, true, true },
	{ 2756, "SETM_SET_memcms", 0x19c04400U,
	  ORLIX_TCTI_MEMORY_SET_MAIN, false, false, false },
	{ 2757, "SETMT_SET_memcms", 0x19c05400U,
	  ORLIX_TCTI_MEMORY_SET_MAIN, false, true, false },
	{ 2758, "SETMN_SET_memcms", 0x19c06400U,
	  ORLIX_TCTI_MEMORY_SET_MAIN, false, false, true },
	{ 2759, "SETMTN_SET_memcms", 0x19c07400U,
	  ORLIX_TCTI_MEMORY_SET_MAIN, false, true, true },
	{ 2760, "SETE_SET_memcms", 0x19c08400U,
	  ORLIX_TCTI_MEMORY_SET_EPILOGUE, false, false, false },
	{ 2761, "SETET_SET_memcms", 0x19c09400U,
	  ORLIX_TCTI_MEMORY_SET_EPILOGUE, false, true, false },
	{ 2762, "SETEN_SET_memcms", 0x19c0a400U,
	  ORLIX_TCTI_MEMORY_SET_EPILOGUE, false, false, true },
	{ 2763, "SETETN_SET_memcms", 0x19c0b400U,
	  ORLIX_TCTI_MEMORY_SET_EPILOGUE, false, true, true },
	{ 2812, "SETGP_SET_memcms", 0x1dc00400U,
	  ORLIX_TCTI_MEMORY_SET_PROLOGUE, true, false, false },
	{ 2813, "SETGPT_SET_memcms", 0x1dc01400U,
	  ORLIX_TCTI_MEMORY_SET_PROLOGUE, true, true, false },
	{ 2814, "SETGPN_SET_memcms", 0x1dc02400U,
	  ORLIX_TCTI_MEMORY_SET_PROLOGUE, true, false, true },
	{ 2815, "SETGPTN_SET_memcms", 0x1dc03400U,
	  ORLIX_TCTI_MEMORY_SET_PROLOGUE, true, true, true },
	{ 2816, "SETGM_SET_memcms", 0x1dc04400U,
	  ORLIX_TCTI_MEMORY_SET_MAIN, true, false, false },
	{ 2817, "SETGMT_SET_memcms", 0x1dc05400U,
	  ORLIX_TCTI_MEMORY_SET_MAIN, true, true, false },
	{ 2818, "SETGMN_SET_memcms", 0x1dc06400U,
	  ORLIX_TCTI_MEMORY_SET_MAIN, true, false, true },
	{ 2819, "SETGMTN_SET_memcms", 0x1dc07400U,
	  ORLIX_TCTI_MEMORY_SET_MAIN, true, true, true },
	{ 2820, "SETGE_SET_memcms", 0x1dc08400U,
	  ORLIX_TCTI_MEMORY_SET_EPILOGUE, true, false, false },
	{ 2821, "SETGET_SET_memcms", 0x1dc09400U,
	  ORLIX_TCTI_MEMORY_SET_EPILOGUE, true, true, false },
	{ 2822, "SETGEN_SET_memcms", 0x1dc0a400U,
	  ORLIX_TCTI_MEMORY_SET_EPILOGUE, true, false, true },
	{ 2823, "SETGETN_SET_memcms", 0x1dc0b400U,
	  ORLIX_TCTI_MEMORY_SET_EPILOGUE, true, true, true },
};

static u32 memory_set_instruction(const struct orlix_tcti_memory_set_leaf *leaf,
				  u8 rd, u8 rn, u8 rs)
{
	return leaf->pattern | ((u32)rs << 16) | ((u32)rn << 5) | rd;
}

static int memory_set_map_pages(struct kunit *test, unsigned long pages,
				unsigned long *mapped)
{
	unsigned long address;

	if (!current->mm) {
		KUNIT_EXPECT_NOT_NULL(test, current->mm);
		return -EINVAL;
	}
	address = ksys_mmap_pgoff(0, pages * PAGE_SIZE,
				  PROT_READ | PROT_WRITE | PROT_MTE,
				  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (IS_ERR_VALUE(address)) {
		KUNIT_EXPECT_FALSE(test, IS_ERR_VALUE(address));
		return (long)address;
	}
	*mapped = address;
	return 0;
}

static int memory_set_map(struct kunit *test, unsigned long *mapped)
{
	return memory_set_map_pages(test, 1, mapped);
}

static int memory_set_set_pte_user(struct kunit *test, unsigned long address,
				   bool user, pte_t *saved_pte)
{
	struct mm_struct *mm = current->mm;
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	pte_t saved;
	spinlock_t *ptl;
	bool pmd_missing;
	bool present;

	if (!mm) {
		KUNIT_EXPECT_NOT_NULL(test, mm);
		return -EINVAL;
	}
	mmap_write_lock(mm);
	pgd = pgd_offset(mm, address);
	p4d = p4d_offset(pgd, address);
	pud = pud_offset(p4d, address);
	pmd = pmd_offset(pud, address);
	pmd_missing = pmd_none(*pmd);
	if (pmd_missing) {
		mmap_write_unlock(mm);
		KUNIT_EXPECT_FALSE(test, pmd_missing);
		return -EINVAL;
	}
	pte = pte_offset_map_lock(mm, pmd, address, &ptl);
	saved = READ_ONCE(*pte);
	present = pte_present(saved);
	if (!present) {
		pte_unmap_unlock(pte, ptl);
		mmap_write_unlock(mm);
		KUNIT_EXPECT_TRUE(test, present);
		return -EINVAL;
	}
	if (user)
		set_pte(pte, __pte(pte_val(saved) | _PAGE_USER));
	else
		set_pte(pte, __pte(pte_val(saved) & ~_PAGE_USER));
	pte_unmap_unlock(pte, ptl);
	mmap_write_unlock(mm);
	*saved_pte = saved;
	return 0;
}

static bool memory_set_pte_is_user(struct kunit *test, unsigned long address)
{
	struct mm_struct *mm = current->mm;
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	pte_t entry;
	spinlock_t *ptl;
	bool user = false;

	if (!mm) {
		KUNIT_EXPECT_NOT_NULL(test, mm);
		return false;
	}
	mmap_read_lock(mm);
	pgd = pgd_offset(mm, address);
	p4d = p4d_offset(pgd, address);
	pud = pud_offset(p4d, address);
	pmd = pmd_offset(pud, address);
	if (!pmd_none(*pmd) && !pmd_bad(*pmd)) {
		pte = pte_offset_map_lock(mm, pmd, address, &ptl);
		entry = READ_ONCE(*pte);
		user = pte_present(entry) && (pte_val(entry) & _PAGE_USER);
		pte_unmap_unlock(pte, ptl);
	}
	mmap_read_unlock(mm);
	return user;
}

static bool memory_set_pte_is_present(struct kunit *test, unsigned long address)
{
	struct mm_struct *mm = current->mm;
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	pte_t entry;
	spinlock_t *ptl;
	bool present = false;

	if (!mm) {
		KUNIT_EXPECT_NOT_NULL(test, mm);
		return false;
	}
	mmap_read_lock(mm);
	pgd = pgd_offset(mm, address);
	p4d = p4d_offset(pgd, address);
	pud = pud_offset(p4d, address);
	pmd = pmd_offset(pud, address);
	if (!pmd_none(*pmd) && !pmd_bad(*pmd)) {
		pte = pte_offset_map_lock(mm, pmd, address, &ptl);
		entry = READ_ONCE(*pte);
		present = pte_present(entry);
		pte_unmap_unlock(pte, ptl);
	}
	mmap_read_unlock(mm);
	return present;
}

struct memory_set_ordering_observer {
	struct mm_struct *mm;
	const struct pt_regs *regs;
	unsigned long address;
	u8 value;
	u8 destination_reg;
	unsigned long expected_progress;
	unsigned long observed_progress;
	u8 observed_byte;
	int read_ret;
	bool requested;
	bool timed_out;
	struct completion ready;
	struct completion progress_published;
	struct completion observation_complete;
};

static int memory_set_ordering_observer_worker(void *data)
{
	struct memory_set_ordering_observer *observer = data;

	kthread_use_mm(observer->mm);
	complete(&observer->ready);
	wait_for_completion(&observer->progress_published);
	if (!observer->regs) {
		observer->read_ret = -ECANCELED;
		complete(&observer->observation_complete);
		kthread_unuse_mm(observer->mm);
		return 0;
	}
	/* The observer has no byte-read permission until Xd was published. */
	observer->observed_progress = smp_load_acquire(
		&observer->regs->regs[observer->destination_reg]);
	observer->read_ret = orlix_tcti_read_user_data(observer->mm,
		observer->address, &observer->observed_byte,
		sizeof(observer->observed_byte));
	complete(&observer->observation_complete);
	kthread_unuse_mm(observer->mm);
	return 0;
}

static void memory_set_ordering_request_observation(
	struct memory_set_ordering_observer *observer, const struct pt_regs *regs,
	u8 destination_reg, unsigned long destination)
{
	if (observer->requested)
		return;
	observer->requested = true;
	observer->regs = regs;
	observer->destination_reg = destination_reg;
	observer->expected_progress = destination;
	complete(&observer->progress_published);
	if (!wait_for_completion_timeout(&observer->observation_complete,
					 msecs_to_jiffies(5000)))
		observer->timed_out = true;
}

static void memory_set_ordering_observe_progress(void *data,
					  const struct pt_regs *regs,
					  u8 destination_reg,
					  unsigned long destination)
{
	memory_set_ordering_request_observation(data, regs, destination_reg,
						destination);
}

static int memory_set_execute_production(struct kunit *test, struct pt_regs *regs,
					 u32 instruction,
					 struct pt_regs *before,
					 unsigned long *entry_pc,
					 struct orlix_tcti_result *result)
{
	u32 program[] = { instruction, 0xd4000001U };
	unsigned long text;
	int unmap_ret;
	int ret;

	if (!current->mm) {
		KUNIT_EXPECT_NOT_NULL(test, current->mm);
		return -EINVAL;
	}
	text = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (IS_ERR_VALUE(text)) {
		KUNIT_EXPECT_FALSE(test, IS_ERR_VALUE(text));
		return (long)text;
	}
	ret = orlix_tcti_write_user_data(current->mm, text, program, sizeof(program));
	if (ret)
		goto out;
	ret = sys_mprotect(text, PAGE_SIZE, PROT_READ | PROT_EXEC);
	if (ret)
		goto out;
	regs->pc = text;
	regs->syscallno = NO_SYSCALL;
	*entry_pc = regs->pc;
	*before = *regs;
	*result = orlix_tcti_resume_user(current, regs, current->mm);
out:
	if (ret)
		KUNIT_EXPECT_EQ(test, 0, ret);
	unmap_ret = vm_munmap(text, PAGE_SIZE);
	if (unmap_ret) {
		KUNIT_EXPECT_EQ(test, 0, unmap_ret);
		if (!ret)
			ret = unmap_ret;
	}
	return ret;
}

static void orlix_tcti_memory_set_source_bindings(struct kunit *test)
{
	size_t index;

	KUNIT_ASSERT_EQ(test, 24U, ARRAY_SIZE(memory_set_leaves));
	for (index = 0; index < ARRAY_SIZE(memory_set_leaves); index++) {
		const struct orlix_tcti_memory_set_leaf *leaf = &memory_set_leaves[index];
		struct orlix_tcti_decoded_instruction decoded =
			orlix_tcti_decode_aarch64(memory_set_instruction(leaf, 4, 5, 6));

		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_MEMORY_SET,
				    decoded.decode_class, "%s ordinal %u", leaf->name,
				    leaf->ordinal);
		KUNIT_EXPECT_EQ(test, leaf->phase, decoded.memory_set_phase);
		KUNIT_EXPECT_EQ(test, leaf->tagged, decoded.memory_set_tagged);
		KUNIT_EXPECT_EQ(test, leaf->unprivileged, decoded.memory_set_unprivileged);
		KUNIT_EXPECT_EQ(test, leaf->nontemporal, decoded.memory_set_nontemporal);
		KUNIT_EXPECT_EQ(test, 4, decoded.rd);
		KUNIT_EXPECT_EQ(test, 5, decoded.rn);
		KUNIT_EXPECT_EQ(test, 6, decoded.rs);
	}
}

static void orlix_tcti_memory_set_rejects_reserved_and_tagged_forms(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(memory_set_leaves); index++) {
		u32 instruction = memory_set_leaves[index].pattern;

		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
			orlix_tcti_decode_aarch64(instruction ^ BIT(31)).decode_class);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
			orlix_tcti_decode_aarch64(instruction ^ BIT(10)).decode_class);
	}
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
		orlix_tcti_decode_aarch64(0x19c0c400U).decode_class);
}

static void orlix_tcti_memory_set_production_resume_semantics(struct kunit *test)
{
	unsigned long mapped = 0;
	int ret;
	size_t index;

	ret = memory_set_map(test, &mapped);
	if (ret)
		return;

	for (index = 0; index < ARRAY_SIZE(memory_set_leaves); index++) {
		const struct orlix_tcti_memory_set_leaf *leaf = &memory_set_leaves[index];
		struct pt_regs regs = {};
		struct pt_regs before;
		unsigned long entry_pc;
		u8 before_memory[23] = {};
		u8 observed[23] = {};
		struct orlix_tcti_result result;
		u64 expected_done = leaf->phase == ORLIX_TCTI_MEMORY_SET_EPILOGUE ? 23 : 16;

		regs.regs[4] = mapped + 3;
		regs.regs[5] = 23;
		regs.regs[6] = leaf->tagged ?
			(0xaUL << ORLIX_MTE_TAG_SHIFT) | 0xa5 : 0xa5;
		regs.pc = 0x1000;
		regs.pstate = PSR_MODE_EL0t | (leaf->phase == ORLIX_TCTI_MEMORY_SET_PROLOGUE ?
			PSR_N_BIT | PSR_Z_BIT | PSR_V_BIT : PSR_C_BIT);
		if (leaf->tagged) {
			KUNIT_ASSERT_EQ(test, 0, orlix_mte_store_allocation_tags(
				current->mm, mapped, 5, 2));
			KUNIT_ASSERT_EQ(test, 0L, set_tagged_addr_ctrl(current,
				PR_TAGGED_ADDR_ENABLE | PR_MTE_TCF_SYNC));
			regs.regs[4] |= 5UL << ORLIX_MTE_TAG_SHIFT;
		}
		ret = orlix_tcti_read_user_data(current->mm, mapped + 3,
					       before_memory, sizeof(before_memory));
		if (ret) {
			KUNIT_EXPECT_EQ(test, 0, ret);
			goto out;
		}
		ret = memory_set_execute_production(test, &regs,
			memory_set_instruction(leaf, 4, 5, 6), &before, &entry_pc, &result);
		if (ret)
			goto out;
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason,
				    "%s", leaf->name);
		KUNIT_EXPECT_EQ(test, mapped + 3 + expected_done,
				regs.regs[4] & ORLIX_MTE_ADDRESS_MASK);
		KUNIT_EXPECT_EQ(test, 23 - expected_done, regs.regs[5]);
		KUNIT_EXPECT_EQ(test, entry_pc + sizeof(u32), regs.pc);
		if (leaf->tagged)
			KUNIT_EXPECT_EQ(test, 5UL,
				(regs.regs[4] & ORLIX_MTE_TAG_MASK) >>
				ORLIX_MTE_TAG_SHIFT);
		if (leaf->phase == ORLIX_TCTI_MEMORY_SET_PROLOGUE)
			KUNIT_EXPECT_EQ(test, PSR_C_BIT,
				regs.pstate & (PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT));
		ret = orlix_tcti_read_user_data(current->mm, mapped + 3, observed,
					       expected_done);
		if (ret) {
			KUNIT_EXPECT_EQ(test, 0, ret);
			goto out;
		}
		while (expected_done--)
			KUNIT_EXPECT_EQ(test, (u8)0xa5, observed[expected_done]);
		if (leaf->tagged) {
			u8 allocation_tags[2] = {};

			KUNIT_EXPECT_EQ(test, 0, orlix_mte_load_allocation_tags(
				current->mm, mapped, allocation_tags,
				ARRAY_SIZE(allocation_tags)));
			/* SETG uses the logical tag in Xd, never the source data tag. */
			KUNIT_EXPECT_EQ(test, (u8)5, allocation_tags[0]);
			KUNIT_EXPECT_EQ(test, (u8)5, allocation_tags[1]);
			KUNIT_ASSERT_EQ(test, 0L, set_tagged_addr_ctrl(current, 0));
		}
	}
out:
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void orlix_tcti_memory_set_faults_and_constrained_forms(struct kunit *test)
{
	const struct orlix_tcti_memory_set_leaf *leaf = &memory_set_leaves[8];
	unsigned long mapped = 0;
	struct pt_regs regs = {};
	struct pt_regs before;
	struct orlix_tcti_result result;
	u8 observed[3] = {};
	u8 before_memory[3] = {};
	unsigned long entry_pc;
	u8 guard;
	int ret;

	ret = memory_set_map_pages(test, 2, &mapped);
	if (ret)
		return;
	ret = sys_mprotect(mapped + PAGE_SIZE, PAGE_SIZE, PROT_NONE);
	if (ret) {
		KUNIT_EXPECT_EQ(test, 0, ret);
		goto out;
	}
	ret = orlix_tcti_read_user_data(current->mm, mapped + PAGE_SIZE, &guard,
					       sizeof(guard));
	if (ret != -EFAULT) {
		KUNIT_EXPECT_EQ(test, -EFAULT, ret);
		goto out;
	}

	regs.regs[4] = mapped + PAGE_SIZE - 3;
	regs.regs[5] = 5;
	regs.regs[6] = 0x5a;
	regs.pstate = PSR_MODE_EL0t | PSR_C_BIT;
	ret = memory_set_execute_production(test, &regs,
		memory_set_instruction(leaf, 4, 5, 6), &before, &entry_pc, &result);
	if (ret)
		goto out;
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_USER_FAULT, result.reason);
	KUNIT_EXPECT_EQ(test, -EFAULT, result.status);
	KUNIT_EXPECT_EQ(test, mapped + PAGE_SIZE, result.fault_address);
	KUNIT_EXPECT_EQ(test, mapped + PAGE_SIZE, regs.regs[4]);
	KUNIT_EXPECT_EQ(test, 2ULL, regs.regs[5]);
	KUNIT_EXPECT_EQ(test, entry_pc, regs.pc);
	KUNIT_EXPECT_FALSE(test, memory_set_pte_is_user(test, mapped + PAGE_SIZE));
	ret = orlix_tcti_read_user_data(current->mm, mapped + PAGE_SIZE - 3,
					       observed, sizeof(observed));
	if (ret) {
		KUNIT_EXPECT_EQ(test, 0, ret);
		goto out;
	}
	KUNIT_EXPECT_EQ(test, (u8)0x5a, observed[0]);
	KUNIT_EXPECT_EQ(test, (u8)0x5a, observed[1]);
	KUNIT_EXPECT_EQ(test, (u8)0x5a, observed[2]);

	ret = orlix_tcti_read_user_data(current->mm, mapped + PAGE_SIZE - 3,
					       before_memory, sizeof(before_memory));
	if (ret) {
		KUNIT_EXPECT_EQ(test, 0, ret);
		goto out;
	}
	ret = memory_set_execute_production(test, &regs,
		memory_set_instruction(leaf, 31, 5, 6), &before, &entry_pc, &result);
	if (ret)
		goto out;
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION, result.reason);
	KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
	KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	KUNIT_EXPECT_EQ(test, entry_pc, regs.pc);
	ret = orlix_tcti_read_user_data(current->mm, mapped + PAGE_SIZE - 3,
					       observed, sizeof(observed));
	if (ret) {
		KUNIT_EXPECT_EQ(test, 0, ret);
		goto out;
	}
	KUNIT_EXPECT_MEMEQ(test, before_memory, observed, sizeof(observed));
	ret = memory_set_execute_production(test, &regs,
		memory_set_instruction(leaf, 4, 31, 6), &before, &entry_pc, &result);
	if (ret)
		goto out;
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION, result.reason);
	KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
	KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	KUNIT_EXPECT_EQ(test, entry_pc, regs.pc);
	ret = orlix_tcti_read_user_data(current->mm, mapped + PAGE_SIZE - 3,
					       observed, sizeof(observed));
	if (ret) {
		KUNIT_EXPECT_EQ(test, 0, ret);
		goto out;
	}
	KUNIT_EXPECT_MEMEQ(test, before_memory, observed, sizeof(observed));

	regs = (struct pt_regs) {};
	regs.regs[4] = mapped;
	regs.regs[5] = 1;
	regs.pstate = PSR_MODE_EL0t | PSR_C_BIT;
	ret = memory_set_execute_production(test, &regs,
		memory_set_instruction(&memory_set_leaves[4], 4, 5, 31), &before,
		&entry_pc, &result);
	if (ret)
		goto out;
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, entry_pc + sizeof(u32), regs.pc);
	ret = orlix_tcti_read_user_data(current->mm, mapped, &observed[0], 1);
	if (ret) {
		KUNIT_EXPECT_EQ(test, 0, ret);
		goto out;
	}
	KUNIT_EXPECT_EQ(test, (u8)0, observed[0]);

out:
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, 2 * PAGE_SIZE));
}

static void orlix_tcti_memory_set_tagged_mismatch_preserves_progress(struct kunit *test)
{
	const struct orlix_tcti_memory_set_leaf *leaf = &memory_set_leaves[20];
	struct pt_regs regs = {};
	struct pt_regs before;
	struct orlix_tcti_result result;
	unsigned long mapped = 0, entry_pc;
	u8 observed[8];
	int ret;

	ret = memory_set_map(test, &mapped);
	if (ret)
		return;
	KUNIT_ASSERT_EQ(test, 0,
		orlix_mte_store_allocation_tag(current->mm, mapped, 5));
	KUNIT_ASSERT_EQ(test, 0,
		orlix_mte_store_allocation_tag(current->mm, mapped + 16, 6));
	KUNIT_ASSERT_EQ(test, 0L, set_tagged_addr_ctrl(current,
		PR_TAGGED_ADDR_ENABLE | PR_MTE_TCF_SYNC));
	regs.regs[4] = (mapped + 8) | (5UL << ORLIX_MTE_TAG_SHIFT);
	regs.regs[5] = 16;
	regs.regs[6] = 0x5a;
	regs.pstate = PSR_MODE_EL0t | PSR_C_BIT;
	ret = memory_set_execute_production(test, &regs,
		memory_set_instruction(leaf, 4, 5, 6), &before, &entry_pc, &result);
	if (!ret) {
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_MTE_TAG_FAULT, result.reason);
		KUNIT_EXPECT_EQ(test, -EHWPOISON, result.status);
		KUNIT_EXPECT_EQ(test, mapped + 16 | (5UL << ORLIX_MTE_TAG_SHIFT),
				result.fault_address);
		KUNIT_EXPECT_EQ(test, mapped + 16,
				regs.regs[4] & ORLIX_MTE_ADDRESS_MASK);
		KUNIT_EXPECT_EQ(test, 8ULL, regs.regs[5]);
		KUNIT_EXPECT_EQ(test, entry_pc, regs.pc);
	}
	KUNIT_ASSERT_EQ(test, 0L, set_tagged_addr_ctrl(current, 0));
	KUNIT_EXPECT_EQ(test, 0, orlix_tcti_read_user_data(current->mm,
		mapped + 8, observed, sizeof(observed)));
	if (!ret) {
		size_t index;
		u8 allocation_tags[2] = {};

		for (index = 0; index < ARRAY_SIZE(observed); index++)
			KUNIT_EXPECT_EQ(test, (u8)0x5a, observed[index]);
		KUNIT_EXPECT_EQ(test, 0, orlix_mte_load_allocation_tags(current->mm,
			mapped, allocation_tags, ARRAY_SIZE(allocation_tags)));
		KUNIT_EXPECT_EQ(test, (u8)5, allocation_tags[0]);
		KUNIT_EXPECT_EQ(test, (u8)6, allocation_tags[1]);
	}
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void orlix_tcti_memory_set_tagged_cross_page_transaction(struct kunit *test)
{
	const struct orlix_tcti_memory_set_leaf *leaf = &memory_set_leaves[16];
	struct pt_regs regs = {};
	struct pt_regs before;
	struct orlix_tcti_result result;
	unsigned long mapped = 0, entry_pc;
	u8 allocation_tags[2] = {};
	int ret;

	ret = memory_set_map_pages(test, 2, &mapped);
	if (ret)
		return;
	KUNIT_ASSERT_EQ(test, 0, orlix_mte_store_allocation_tag(current->mm,
		mapped + PAGE_SIZE - ORLIX_MTE_GRANULE_SIZE, 5));
	KUNIT_ASSERT_EQ(test, 0, orlix_mte_store_allocation_tag(current->mm,
		mapped + PAGE_SIZE, 5));
	KUNIT_ASSERT_EQ(test, 0L, set_tagged_addr_ctrl(current,
		PR_TAGGED_ADDR_ENABLE | PR_MTE_TCF_SYNC));
	regs.regs[4] = (mapped + PAGE_SIZE - 8) | (5UL << ORLIX_MTE_TAG_SHIFT);
	regs.regs[5] = ORLIX_TCTI_MEMORY_SET_STAGE_BYTES;
	regs.regs[6] = (9UL << ORLIX_MTE_TAG_SHIFT) | 0x3c;
	regs.pstate = PSR_MODE_EL0t | PSR_C_BIT;
	ret = memory_set_execute_production(test, &regs,
		memory_set_instruction(leaf, 4, 5, 6), &before, &entry_pc, &result);
	if (!ret) {
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
		KUNIT_EXPECT_EQ(test, entry_pc + sizeof(u32), regs.pc);
		KUNIT_EXPECT_EQ(test, mapped + PAGE_SIZE + 8,
			regs.regs[4] & ORLIX_MTE_ADDRESS_MASK);
		KUNIT_EXPECT_EQ(test, 0ULL, regs.regs[5]);
		KUNIT_EXPECT_EQ(test, 0, orlix_mte_load_allocation_tags(current->mm,
			mapped + PAGE_SIZE - ORLIX_MTE_GRANULE_SIZE, allocation_tags,
			ARRAY_SIZE(allocation_tags)));
		KUNIT_EXPECT_EQ(test, (u8)5, allocation_tags[0]);
		KUNIT_EXPECT_EQ(test, (u8)5, allocation_tags[1]);
	}
	KUNIT_ASSERT_EQ(test, 0L, set_tagged_addr_ctrl(current, 0));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, 2 * PAGE_SIZE));
}

static void orlix_tcti_memory_set_n_form_orders_completed_bytes(struct kunit *test)
{
	const struct orlix_tcti_memory_set_leaf *leaf = &memory_set_leaves[10];
	const unsigned int byte_count = 16;
	unsigned long mapped = 0;
	struct pt_regs regs = {};
	struct pt_regs before;
	struct orlix_tcti_result result;
	struct memory_set_ordering_observer observer = {
		.mm = current->mm,
		.address = 0,
		.value = 0x3c,
	};
	struct task_struct *observer_task = NULL;
	unsigned long entry_pc;
	int ret;

	ret = memory_set_map(test, &mapped);
	if (ret)
		return;
	observer.address = mapped;
	init_completion(&observer.ready);
	init_completion(&observer.progress_published);
	init_completion(&observer.observation_complete);
	observer_task = kthread_run(memory_set_ordering_observer_worker, &observer,
		"orlix-tcti-memory-set-observer");
	if (IS_ERR(observer_task)) {
		KUNIT_FAIL(test, "could not create N-form ordering observer");
		observer_task = NULL;
		goto out;
	}
	if (!wait_for_completion_timeout(&observer.ready, msecs_to_jiffies(5000))) {
		KUNIT_FAIL(test, "N-form ordering observer did not become ready");
		goto out;
	}
	regs.regs[4] = mapped;
	regs.regs[5] = byte_count;
	regs.regs[6] = 0x3c;
	regs.pstate = PSR_MODE_EL0t | PSR_C_BIT;
	orlix_tcti_memory_set_set_ordering_test_hook(
		memory_set_ordering_observe_progress, &observer);
	ret = memory_set_execute_production(test, &regs,
		memory_set_instruction(leaf, 4, 5, 6), &before, &entry_pc, &result);
	orlix_tcti_memory_set_set_ordering_test_hook(NULL, NULL);
	if (ret)
		goto out;
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, entry_pc + sizeof(u32), regs.pc);
	KUNIT_EXPECT_EQ(test, mapped + byte_count, regs.regs[4]);
	KUNIT_EXPECT_EQ(test, 0ULL, regs.regs[5]);
	KUNIT_EXPECT_TRUE(test, observer.requested);
	KUNIT_EXPECT_FALSE(test, observer.timed_out);
	KUNIT_EXPECT_EQ(test, observer.expected_progress, observer.observed_progress);
	KUNIT_EXPECT_EQ(test, 0, observer.read_ret);
	KUNIT_EXPECT_EQ(test, observer.value, observer.observed_byte);
	/* The same handoff must reject an intentionally inverted publication. */
	{
		struct memory_set_ordering_observer inverted = {
			.mm = current->mm,
			.address = mapped,
			.value = 0x3c,
		};
		struct pt_regs inverted_regs = {};
		struct task_struct *inverted_task;
		u8 before = 0;

		ret = orlix_tcti_write_user_data(current->mm, mapped, &before,
						       sizeof(before));
		if (ret) {
			KUNIT_EXPECT_EQ(test, 0, ret);
			goto out;
		}
		init_completion(&inverted.ready);
		init_completion(&inverted.progress_published);
		init_completion(&inverted.observation_complete);
		inverted_task = kthread_run(memory_set_ordering_observer_worker,
			&inverted, "orlix-tcti-memory-set-inverted");
		if (IS_ERR(inverted_task)) {
			KUNIT_FAIL(test, "could not create inverted ordering observer");
			goto out;
		}
		if (!wait_for_completion_timeout(&inverted.ready,
						 msecs_to_jiffies(5000))) {
			KUNIT_FAIL(test, "inverted ordering observer did not become ready");
			complete(&inverted.progress_published);
			kthread_stop(inverted_task);
			goto out;
		}
		inverted_regs.regs[4] = mapped;
		smp_store_release(&inverted_regs.regs[4], mapped + 1);
		memory_set_ordering_request_observation(&inverted, &inverted_regs, 4,
							mapped + 1);
		KUNIT_EXPECT_FALSE(test, inverted.timed_out);
		KUNIT_EXPECT_EQ(test, mapped + 1, inverted.observed_progress);
		KUNIT_EXPECT_EQ(test, 0, inverted.read_ret);
		KUNIT_EXPECT_NE(test, inverted.value, inverted.observed_byte);
		KUNIT_EXPECT_EQ(test, 0, kthread_stop(inverted_task));
	}
out:
	orlix_tcti_memory_set_set_ordering_test_hook(NULL, NULL);
	if (observer_task && !observer.requested)
		complete(&observer.progress_published);
	if (observer_task)
		KUNIT_EXPECT_EQ(test, 0, kthread_stop(observer_task));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void orlix_tcti_memory_set_unprivileged_access(struct kunit *test)
{
	const struct orlix_tcti_memory_set_leaf *ordinary = &memory_set_leaves[8];
	const struct orlix_tcti_memory_set_leaf *unprivileged = &memory_set_leaves[9];
	unsigned long mapped = 0;
	struct pt_regs regs = {};
	struct orlix_tcti_result result;
	struct pt_regs before;
	pte_t saved;
	unsigned long entry_pc;
	int ret;
	bool pte_changed = false;

	ret = memory_set_map_pages(test, 2, &mapped);
	if (ret)
		return;
	if (memory_set_pte_is_present(test, mapped)) {
		KUNIT_EXPECT_FALSE(test, memory_set_pte_is_present(test, mapped));
		goto out;
	}
	/* A forced-EL0 store faults in an eligible absent PTE and retries once. */
	regs.regs[4] = mapped;
	regs.regs[5] = 1;
	regs.regs[6] = 0x4d;
	regs.pstate = PSR_MODE_EL1h | PSR_C_BIT;
	ret = memory_set_execute_production(test, &regs,
		memory_set_instruction(unprivileged, 4, 5, 6), &before, &entry_pc,
		&result);
	if (ret)
		goto out;
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, mapped + 1, regs.regs[4]);
	KUNIT_EXPECT_EQ(test, 0ULL, regs.regs[5]);
	KUNIT_EXPECT_EQ(test, entry_pc + sizeof(u32), regs.pc);
	KUNIT_EXPECT_TRUE(test, memory_set_pte_is_user(test, mapped));
	ret = orlix_tcti_write_user_data(current->mm, mapped + PAGE_SIZE, "x", 1);
	if (ret) {
		KUNIT_EXPECT_EQ(test, 0, ret);
		goto out;
	}
	ret = memory_set_set_pte_user(test, mapped + PAGE_SIZE, false, &saved);
	if (ret)
		goto out;
	pte_changed = true;

	regs.regs[4] = mapped + PAGE_SIZE - 3;
	regs.regs[5] = 5;
	regs.regs[6] = 0x7e;
	regs.pstate = PSR_MODE_EL1h | PSR_C_BIT;
	ret = memory_set_execute_production(test, &regs,
		memory_set_instruction(ordinary, 4, 5, 6), &before, &entry_pc, &result);
	if (ret)
		goto out;
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, mapped + PAGE_SIZE + 2, regs.regs[4]);
	KUNIT_EXPECT_EQ(test, 0ULL, regs.regs[5]);
	KUNIT_EXPECT_EQ(test, entry_pc + sizeof(u32), regs.pc);

	regs = (struct pt_regs) {};
	regs.regs[4] = mapped + PAGE_SIZE - 3;
	regs.regs[5] = 5;
	regs.regs[6] = 0x6d;
	regs.pstate = PSR_MODE_EL1h | PSR_C_BIT;
	ret = memory_set_execute_production(test, &regs,
		memory_set_instruction(unprivileged, 4, 5, 6), &before, &entry_pc,
		&result);
	if (ret)
		goto out;
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_USER_FAULT, result.reason);
	KUNIT_EXPECT_EQ(test, -EFAULT, result.status);
	KUNIT_EXPECT_EQ(test, mapped + PAGE_SIZE, result.fault_address);
	KUNIT_EXPECT_EQ(test, mapped + PAGE_SIZE, regs.regs[4]);
	KUNIT_EXPECT_EQ(test, 2ULL, regs.regs[5]);
	KUNIT_EXPECT_EQ(test, entry_pc, regs.pc);

out:
	if (pte_changed) {
		ret = memory_set_set_pte_user(test, mapped + PAGE_SIZE,
					      !!(pte_val(saved) & _PAGE_USER), &saved);
		KUNIT_EXPECT_EQ(test, 0, ret);
	}
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, 2 * PAGE_SIZE));
}

static void orlix_tcti_memory_set_rejects_operand_overlaps(struct kunit *test)
{
	const struct orlix_tcti_memory_set_leaf *leaf = &memory_set_leaves[8];
	const u8 overlaps[][3] = { { 4, 4, 6 }, { 4, 5, 4 }, { 4, 5, 5 } };
	unsigned long mapped = 0;
	int ret;
	size_t index;

	ret = memory_set_map(test, &mapped);
	if (ret)
		return;

	for (index = 0; index < ARRAY_SIZE(overlaps); index++) {
		struct pt_regs regs = {};
		struct pt_regs before;
		struct orlix_tcti_result result;
		u8 before_memory[4] = {};
		u8 observed[4] = {};
		unsigned long entry_pc;

		regs.regs[4] = mapped;
		regs.regs[5] = 4;
		regs.regs[6] = 0x5a;
		regs.pstate = PSR_MODE_EL0t | PSR_C_BIT;
		ret = orlix_tcti_read_user_data(current->mm, mapped, before_memory,
					       sizeof(before_memory));
		if (ret) {
			KUNIT_EXPECT_EQ(test, 0, ret);
			goto out;
		}
		ret = memory_set_execute_production(test, &regs,
			memory_set_instruction(leaf, overlaps[index][0], overlaps[index][1],
					       overlaps[index][2]), &before, &entry_pc, &result);
		if (ret)
			goto out;
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				result.reason);
		KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		KUNIT_EXPECT_EQ(test, entry_pc, regs.pc);
		ret = orlix_tcti_read_user_data(current->mm, mapped, observed,
					       sizeof(observed));
		if (ret) {
			KUNIT_EXPECT_EQ(test, 0, ret);
			goto out;
		}
		KUNIT_EXPECT_MEMEQ(test, before_memory, observed, sizeof(observed));
	}
out:
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void orlix_tcti_memory_set_prologue_fault_commit_ordering(struct kunit *test)
{
	const struct orlix_tcti_memory_set_leaf *leaves[] = {
		&memory_set_leaves[0], &memory_set_leaves[1],
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(leaves); index++) {
		const struct orlix_tcti_memory_set_leaf *leaf = leaves[index];
		struct pt_regs regs = {};
		struct pt_regs before;
		struct orlix_tcti_result result;
		unsigned long mapped = 0;
		unsigned long entry_pc;
		u8 before_memory[3] = { 0x11, 0x22, 0x33 };
		u8 observed[3] = {};
		int ret;

		/* A completed first byte commits SETP's staged C flag and progress. */
		ret = memory_set_map(test, &mapped);
		if (ret)
			return;
		regs.regs[4] = mapped;
		regs.regs[5] = 1;
		regs.regs[6] = 0xa5;
		regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT | PSR_V_BIT;
		ret = memory_set_execute_production(test, &regs,
			memory_set_instruction(leaf, 4, 5, 6), &before, &entry_pc, &result);
		if (ret)
			goto out_success;
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
		KUNIT_EXPECT_EQ(test, entry_pc + sizeof(u32), regs.pc);
		KUNIT_EXPECT_EQ(test, mapped + 1, regs.regs[4]);
		KUNIT_EXPECT_EQ(test, 0ULL, regs.regs[5]);
		KUNIT_EXPECT_EQ(test, PSR_C_BIT,
			regs.pstate & (PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT));
	out_success:
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
		if (ret)
			return;

		/* A fault before the first store leaves every architectural field intact. */
		ret = memory_set_map(test, &mapped);
		if (ret)
			return;
		ret = orlix_tcti_write_user_data(current->mm, mapped, before_memory,
						 sizeof(before_memory));
		if (ret)
			goto out_first_fault;
		ret = sys_mprotect(mapped, PAGE_SIZE, PROT_NONE);
		if (ret)
			goto out_first_fault;
		regs = (struct pt_regs) {};
		regs.regs[4] = mapped;
		regs.regs[5] = 3;
		regs.regs[6] = 0x5a;
		regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT | PSR_V_BIT;
		ret = memory_set_execute_production(test, &regs,
			memory_set_instruction(leaf, 4, 5, 6), &before, &entry_pc, &result);
		if (ret)
			goto out_first_fault;
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_USER_FAULT, result.reason);
		KUNIT_EXPECT_EQ(test, -EFAULT, result.status);
		KUNIT_EXPECT_EQ(test, mapped, result.fault_address);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		KUNIT_EXPECT_EQ(test, entry_pc, regs.pc);
		ret = sys_mprotect(mapped, PAGE_SIZE, PROT_READ | PROT_WRITE);
		if (ret)
			goto out_first_fault;
		ret = orlix_tcti_read_user_data(current->mm, mapped, observed,
					       sizeof(observed));
		if (ret)
			goto out_first_fault;
		KUNIT_EXPECT_MEMEQ(test, before_memory, observed, sizeof(observed));
	out_first_fault:
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
		if (ret)
			return;

		/* A later fault retains the completed-byte progress and staged flags. */
		ret = memory_set_map_pages(test, 2, &mapped);
		if (ret)
			return;
		ret = sys_mprotect(mapped + PAGE_SIZE, PAGE_SIZE, PROT_NONE);
		if (ret)
			goto out_partial_fault;
		regs = (struct pt_regs) {};
		regs.regs[4] = mapped + PAGE_SIZE - sizeof(observed);
		regs.regs[5] = 5;
		regs.regs[6] = 0x3c;
		regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT | PSR_V_BIT;
		ret = memory_set_execute_production(test, &regs,
			memory_set_instruction(leaf, 4, 5, 6), &before, &entry_pc, &result);
		if (ret)
			goto out_partial_fault;
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_USER_FAULT, result.reason);
		KUNIT_EXPECT_EQ(test, -EFAULT, result.status);
		KUNIT_EXPECT_EQ(test, mapped + PAGE_SIZE, result.fault_address);
		KUNIT_EXPECT_EQ(test, mapped + PAGE_SIZE, regs.regs[4]);
		KUNIT_EXPECT_EQ(test, 2ULL, regs.regs[5]);
		KUNIT_EXPECT_EQ(test, entry_pc, regs.pc);
		KUNIT_EXPECT_EQ(test, PSR_C_BIT,
			regs.pstate & (PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT));
		ret = orlix_tcti_read_user_data(current->mm,
			mapped + PAGE_SIZE - sizeof(observed), observed, sizeof(observed));
		if (ret)
			goto out_partial_fault;
		KUNIT_EXPECT_EQ(test, (u8)0x3c, observed[0]);
		KUNIT_EXPECT_EQ(test, (u8)0x3c, observed[1]);
		KUNIT_EXPECT_EQ(test, (u8)0x3c, observed[2]);
	out_partial_fault:
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, 2 * PAGE_SIZE));
		if (ret)
			return;
	}
}

static struct kunit_case orlix_tcti_memory_set_source_bound_cases[] = {
	KUNIT_CASE(orlix_tcti_memory_set_source_bindings),
	KUNIT_CASE(orlix_tcti_memory_set_rejects_reserved_and_tagged_forms),
	KUNIT_CASE(orlix_tcti_memory_set_production_resume_semantics),
	KUNIT_CASE(orlix_tcti_memory_set_faults_and_constrained_forms),
	KUNIT_CASE(orlix_tcti_memory_set_tagged_mismatch_preserves_progress),
	KUNIT_CASE(orlix_tcti_memory_set_tagged_cross_page_transaction),
	KUNIT_CASE(orlix_tcti_memory_set_unprivileged_access),
	KUNIT_CASE(orlix_tcti_memory_set_n_form_orders_completed_bytes),
	KUNIT_CASE(orlix_tcti_memory_set_rejects_operand_overlaps),
	KUNIT_CASE(orlix_tcti_memory_set_prologue_fault_commit_ordering),
	{}
};

static struct kunit_suite orlix_tcti_memory_set_source_bound_test_suite = {
	.name = "orlix-tcti-memory-set-source-bound",
	.test_cases = orlix_tcti_memory_set_source_bound_cases,
};

kunit_test_suite(orlix_tcti_memory_set_source_bound_test_suite);

MODULE_LICENSE("GPL");
