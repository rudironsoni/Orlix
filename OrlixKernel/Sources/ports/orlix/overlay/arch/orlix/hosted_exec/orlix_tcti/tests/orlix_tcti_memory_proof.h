/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_MEMORY_PROOF_H
#define ORLIX_TCTI_MEMORY_PROOF_H

#include <kunit/test.h>
#include <linux/err.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <asm/orlix_tcti.h>

#include "../decode_aarch64.h"

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

static inline unsigned long orlix_tcti_memory_proof_map_guarded_span(
	struct kunit *test, int first_prot)
{
	unsigned long mapped;
	int ret;

	mapped = ksys_mmap_pgoff(0, 2 * PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	if (first_prot != (PROT_READ | PROT_WRITE)) {
		ret = sys_mprotect(mapped, PAGE_SIZE, first_prot);
		KUNIT_ASSERT_EQ(test, 0, ret);
	}
	ret = sys_mprotect(mapped + PAGE_SIZE, PAGE_SIZE, PROT_NONE);
	KUNIT_ASSERT_EQ(test, 0, ret);
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

static inline u64 orlix_tcti_memory_proof_simd_loaded(u64 stored, u8 access_size)
{
	if (access_size == sizeof(u8))
		return (u8)stored;
	if (access_size == sizeof(u16))
		return (u16)stored;
	if (access_size == sizeof(u32))
		return (u32)stored;
	return stored;
}

static inline void orlix_tcti_memory_proof_expect_simd_dest(struct kunit *test,
	u8 reg, u8 access_size, u64 stored_low, u64 stored_high, const char *name)
{
	u64 expected_low = orlix_tcti_memory_proof_simd_loaded(stored_low,
							       access_size);
	u64 expected_high = access_size > sizeof(u64) ? stored_high : 0;

	KUNIT_EXPECT_EQ_MSG(test, expected_low,
			    current->thread.user_simd[reg * 2],
			    "%s simd%u", name, reg * 2);
	if (access_size > sizeof(u64))
		KUNIT_EXPECT_EQ_MSG(test, expected_high,
				    current->thread.user_simd[reg * 2 + 1],
				    "%s simd%u", name, reg * 2 + 1);
	else
		KUNIT_EXPECT_EQ_MSG(test, 0ULL,
				    current->thread.user_simd[reg * 2 + 1],
				    "%s simd%u high", name, reg * 2 + 1);
}

static inline bool orlix_tcti_memory_proof_writes_back(
	enum orlix_tcti_memory_index_mode mode)
{
	return mode == ORLIX_TCTI_MEMORY_INDEX_PRE ||
		mode == ORLIX_TCTI_MEMORY_INDEX_POST;
}

static inline void orlix_tcti_memory_proof_expect_writeback(struct kunit *test,
	enum orlix_tcti_memory_index_mode mode, u64 before_rn, s64 offset,
	u64 after_rn, const char *name)
{
	if (!orlix_tcti_memory_proof_writes_back(mode)) {
		KUNIT_EXPECT_EQ_MSG(test, before_rn, after_rn,
				    "%s no writeback", name);
		return;
	}
	KUNIT_EXPECT_EQ_MSG(test, before_rn + offset, after_rn,
			    "%s writeback", name);
}

static inline void orlix_tcti_memory_proof_expect_user_fault(struct kunit *test,
	const struct orlix_tcti_result *result, unsigned long before_pc,
	unsigned long after_pc, const char *name)
{
	KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_USER_FAULT, result->reason,
			    "%s unmapped reason", name);
	KUNIT_EXPECT_TRUE_MSG(test,
			      result->status == -EFAULT ||
			      result->status == -EACCES,
			      "%s unmapped status %ld", name, result->status);
	KUNIT_EXPECT_EQ_MSG(test, before_pc, after_pc, "%s fault pc", name);
}

#endif
