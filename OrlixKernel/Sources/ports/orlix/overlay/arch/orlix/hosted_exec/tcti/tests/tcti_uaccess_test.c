// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/string.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <asm/tcti.h>

#define TCTI_UACCESS_CHUNK_SIZE 16
#define TCTI_UACCESS_COPY_SIZE (2 * TCTI_UACCESS_CHUNK_SIZE)

static unsigned long tcti_uaccess_map_two_pages(struct kunit *test)
{
	unsigned long mapped;

	mapped = ksys_mmap_pgoff(0, 2 * PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_EXPECT_FALSE(test, IS_ERR_VALUE(mapped));
	return mapped;
}

static void tcti_uaccess_partial_cross_page_read_returns_residual(
	struct kunit *test)
{
	u8 source[TCTI_UACCESS_CHUNK_SIZE];
	u8 destination[TCTI_UACCESS_COPY_SIZE];
	unsigned long mapped = tcti_uaccess_map_two_pages(test);
	unsigned long user_address;
	unsigned long left;
	size_t index;
	int ret;

	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	user_address = mapped + PAGE_SIZE - sizeof(source);
	for (index = 0; index < sizeof(source); index++)
		source[index] = 0x40U + index;
	memset(destination, 0xa5, sizeof(destination));

	ret = tcti_write_user_data(current->mm, user_address, source,
				   sizeof(source));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_ASSERT_EQ(test, 0,
			vm_munmap(mapped + PAGE_SIZE, PAGE_SIZE));

	left = raw_copy_from_user(destination,
				  (const void __user *)user_address,
				  sizeof(destination));

	KUNIT_EXPECT_EQ(test, (unsigned long)TCTI_UACCESS_CHUNK_SIZE, left);
	KUNIT_EXPECT_MEMEQ(test, source, destination, sizeof(source));
	for (index = sizeof(source); index < sizeof(destination); index++)
		KUNIT_EXPECT_EQ(test, (u8)0xa5, destination[index]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void tcti_uaccess_partial_cross_page_write_returns_residual(
	struct kunit *test)
{
	u8 source[TCTI_UACCESS_COPY_SIZE];
	u8 observed[TCTI_UACCESS_CHUNK_SIZE] = {};
	unsigned long mapped = tcti_uaccess_map_two_pages(test);
	unsigned long user_address;
	unsigned long left;
	size_t index;
	int ret;

	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	user_address = mapped + PAGE_SIZE - sizeof(observed);
	for (index = 0; index < sizeof(source); index++)
		source[index] = 0x80U + index;
	KUNIT_ASSERT_EQ(test, 0,
			vm_munmap(mapped + PAGE_SIZE, PAGE_SIZE));

	left = raw_copy_to_user((void __user *)user_address, source,
				sizeof(source));

	KUNIT_EXPECT_EQ(test, (unsigned long)TCTI_UACCESS_CHUNK_SIZE, left);
	ret = tcti_read_user_data(current->mm, user_address, observed,
				  sizeof(observed));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_MEMEQ(test, source, observed, sizeof(observed));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void tcti_uaccess_partial_cross_page_clear_returns_residual(
	struct kunit *test)
{
	u8 initial[TCTI_UACCESS_CHUNK_SIZE];
	u8 observed[TCTI_UACCESS_CHUNK_SIZE];
	u8 zero[TCTI_UACCESS_CHUNK_SIZE] = {};
	unsigned long mapped = tcti_uaccess_map_two_pages(test);
	unsigned long user_address;
	unsigned long left;
	int ret;

	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	user_address = mapped + PAGE_SIZE - sizeof(initial);
	memset(initial, 0x5a, sizeof(initial));
	ret = tcti_write_user_data(current->mm, user_address, initial,
				   sizeof(initial));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_ASSERT_EQ(test, 0,
			vm_munmap(mapped + PAGE_SIZE, PAGE_SIZE));

	left = clear_user((void __user *)user_address,
			  TCTI_UACCESS_COPY_SIZE);

	KUNIT_EXPECT_EQ(test, (unsigned long)TCTI_UACCESS_CHUNK_SIZE, left);
	ret = tcti_read_user_data(current->mm, user_address, observed,
				  sizeof(observed));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_MEMEQ(test, zero, observed, sizeof(observed));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void tcti_uaccess_pagefault_disabled_returns_full_residual(
	struct kunit *test)
{
	u8 initial[TCTI_UACCESS_CHUNK_SIZE];
	u8 source[TCTI_UACCESS_CHUNK_SIZE];
	u8 destination[TCTI_UACCESS_CHUNK_SIZE];
	u8 observed[TCTI_UACCESS_CHUNK_SIZE];
	unsigned long mapped;
	unsigned long read_left;
	unsigned long write_left;
	unsigned long clear_left;
	size_t index;
	int ret;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	memset(initial, 0x3c, sizeof(initial));
	memset(source, 0xc3, sizeof(source));
	memset(destination, 0xa5, sizeof(destination));
	ret = tcti_write_user_data(current->mm, mapped, initial,
				   sizeof(initial));
	KUNIT_ASSERT_EQ(test, 0, ret);

	pagefault_disable();
	read_left = raw_copy_from_user(destination,
				       (const void __user *)mapped,
				       sizeof(destination));
	write_left = raw_copy_to_user((void __user *)mapped, source,
				      sizeof(source));
	clear_left = __clear_user((void __user *)mapped, sizeof(initial));
	pagefault_enable();

	KUNIT_EXPECT_EQ(test, (unsigned long)sizeof(destination), read_left);
	KUNIT_EXPECT_EQ(test, (unsigned long)sizeof(source), write_left);
	KUNIT_EXPECT_EQ(test, (unsigned long)sizeof(initial), clear_left);
	for (index = 0; index < sizeof(destination); index++)
		KUNIT_EXPECT_EQ(test, (u8)0xa5, destination[index]);
	ret = tcti_read_user_data(current->mm, mapped, observed,
				  sizeof(observed));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_MEMEQ(test, initial, observed, sizeof(observed));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static struct kunit_case tcti_uaccess_test_cases[] = {
	KUNIT_CASE(tcti_uaccess_partial_cross_page_read_returns_residual),
	KUNIT_CASE(tcti_uaccess_partial_cross_page_write_returns_residual),
	KUNIT_CASE(tcti_uaccess_partial_cross_page_clear_returns_residual),
	KUNIT_CASE(tcti_uaccess_pagefault_disabled_returns_full_residual),
	{}
};

static struct kunit_suite tcti_uaccess_test_suite = {
	.name = "orlix-tcti-uaccess",
	.test_cases = tcti_uaccess_test_cases,
};
kunit_test_suite(tcti_uaccess_test_suite);
