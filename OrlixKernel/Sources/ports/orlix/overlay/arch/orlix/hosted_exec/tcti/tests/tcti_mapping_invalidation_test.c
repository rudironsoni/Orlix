// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <asm/tcti.h>
#include <asm/unistd.h>

#include "../block_cache.h"
#include "../engine.h"
#include "../tlb.h"

static void tcti_mapping_syscall_invalidates_task_tlb(struct kunit *test)
{
	static const unsigned long mapping_syscalls[] = {
		__NR_brk,
		__NR_mmap,
		__NR_mprotect,
		__NR_munmap,
		__NR_mremap,
	};
	struct tcti_tlb *tlb;
	struct tcti_user_page page = {
		.user_page = 0x4000,
		.host_data = (void *)0x100000UL,
	};
	u64 before;
	u64 after;
	u32 index;
	int ret;

	KUNIT_ASSERT_NOT_NULL(test, current->mm);
	tlb = tcti_tlb_get_current();
	KUNIT_ASSERT_NOT_NULL(test, tlb);

	for (index = 0; index < ARRAY_SIZE(mapping_syscalls); index++) {
		void *host_data;

		tcti_tlb_flush(tlb);
		before = tcti_translation_generation(current->mm);
		KUNIT_ASSERT_FALSE(test, before & 1);
		page.translation_generation = before;
		ret = tcti_tlb_fill(tlb, current->mm, page.user_page,
				    TCTI_ACCESS_READ, &page);
		KUNIT_ASSERT_EQ_MSG(test, 0, ret, "syscall=%lu",
				    mapping_syscalls[index]);

		host_data = tcti_tlb_lookup(tlb, current->mm, page.user_page,
					    TCTI_ACCESS_READ, before);
		KUNIT_ASSERT_PTR_EQ_MSG(test, page.host_data, host_data,
					"syscall=%lu", mapping_syscalls[index]);

		tcti_invalidate_changed_user_mappings_for_tests(
			current->mm, mapping_syscalls[index], 0);
		after = tcti_translation_generation(current->mm);
		KUNIT_EXPECT_GT_MSG(test, after, before, "syscall=%lu",
				    mapping_syscalls[index]);
		KUNIT_EXPECT_FALSE_MSG(test, after & 1, "syscall=%lu",
				       mapping_syscalls[index]);

		host_data = tcti_tlb_lookup(tlb, current->mm, page.user_page,
					    TCTI_ACCESS_READ, after);
		KUNIT_EXPECT_PTR_EQ_MSG(test, NULL, host_data, "syscall=%lu",
					mapping_syscalls[index]);
	}

	tcti_tlb_flush(tlb);
}

static void tcti_nonmapping_or_failed_syscall_preserves_authorization(
	struct kunit *test)
{
	struct tcti_tlb *tlb;
	struct tcti_user_page page = {
		.user_page = 0x8000,
		.host_data = (void *)0x200000UL,
	};
	u64 generation;
	void *host_data;
	int ret;

	KUNIT_ASSERT_NOT_NULL(test, current->mm);
	tlb = tcti_tlb_get_current();
	KUNIT_ASSERT_NOT_NULL(test, tlb);
	tcti_tlb_flush(tlb);
	generation = tcti_translation_generation(current->mm);
	KUNIT_ASSERT_FALSE(test, generation & 1);
	page.translation_generation = generation;
	ret = tcti_tlb_fill(tlb, current->mm, page.user_page,
			    TCTI_ACCESS_READ, &page);
	KUNIT_ASSERT_EQ(test, 0, ret);

	tcti_invalidate_changed_user_mappings_for_tests(current->mm, __NR_read,
							0);
	KUNIT_EXPECT_EQ(test, generation,
				tcti_translation_generation(current->mm));
	host_data = tcti_tlb_lookup(tlb, current->mm, page.user_page,
					    TCTI_ACCESS_READ, generation);
	KUNIT_EXPECT_PTR_EQ(test, page.host_data, host_data);

	tcti_invalidate_changed_user_mappings_for_tests(current->mm, __NR_mmap,
							-ENOMEM);
	KUNIT_EXPECT_EQ(test, generation,
				tcti_translation_generation(current->mm));
	host_data = tcti_tlb_lookup(tlb, current->mm, page.user_page,
					    TCTI_ACCESS_READ, generation);
	KUNIT_EXPECT_PTR_EQ(test, page.host_data, host_data);
	tcti_tlb_flush(tlb);
}

static struct kunit_case tcti_mapping_invalidation_cases[] = {
	KUNIT_CASE(tcti_mapping_syscall_invalidates_task_tlb),
	KUNIT_CASE(tcti_nonmapping_or_failed_syscall_preserves_authorization),
	{}
};

static struct kunit_suite tcti_mapping_invalidation_suite = {
	.name = "orlix-tcti-mapping-invalidation",
	.test_cases = tcti_mapping_invalidation_cases,
};

kunit_test_suite(tcti_mapping_invalidation_suite);
