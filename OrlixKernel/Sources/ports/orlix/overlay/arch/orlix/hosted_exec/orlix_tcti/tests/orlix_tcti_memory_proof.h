/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_MEMORY_PROOF_H
#define ORLIX_TCTI_MEMORY_PROOF_H

#include <kunit/test.h>
#include <linux/err.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <asm/orlix_tcti.h>

/*
 * Shared guest-memory fixtures for #134 and later memory families.
 * Callers must not use these helpers to claim another family's ordinals.
 */
static inline unsigned long orlix_tcti_memory_proof_map_bytes(struct kunit *test,
	const void *bytes, size_t length, int prot)
{
	unsigned long mapped;
	int ret;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	if (bytes && length) {
		ret = orlix_tcti_write_user_data(current->mm, mapped, bytes, length);
		KUNIT_ASSERT_EQ(test, 0, ret);
	}
	if (prot != (PROT_READ | PROT_WRITE)) {
		ret = sys_mprotect(mapped, PAGE_SIZE, prot);
		KUNIT_ASSERT_EQ(test, 0, ret);
	}
	return mapped;
}

static inline void orlix_tcti_memory_proof_expect_bytes(struct kunit *test,
	unsigned long address, const void *expected, size_t length,
	const char *name)
{
	u8 actual[64];

	KUNIT_ASSERT_LE(test, length, sizeof(actual));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_read_user_data(current->mm, address,
							   actual, length));
	KUNIT_EXPECT_MEMEQ_MSG(test, expected, actual, length, "%s", name);
}

#endif
