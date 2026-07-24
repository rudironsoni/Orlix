// SPDX-License-Identifier: GPL-2.0-only

#include <linux/mm.h>
#include <linux/uaccess.h>
#include <asm/page.h>
#include <asm/tcti.h>

static unsigned long orlix_uaccess_copy_from_user(void *to,
					  unsigned long from,
					  unsigned long n)
{
	struct mm_struct *mm = current->mm;
	unsigned long remaining = n;
	unsigned char *dst = to;

	/*
	 * raw_copy_from_user() also backs __copy_from_user_inatomic().  The
	 * TCTI transport may fault or acquire sleeping locks, so do not enter it
	 * when fault handling is disabled.
	 */
	if (!mm || faulthandler_disabled())
		return n;

	while (remaining) {
		unsigned long chunk = min(remaining,
					  PAGE_SIZE - offset_in_page(from));

		if (tcti_read_user_data(mm, from, dst, chunk))
			return remaining;

		dst += chunk;
		from += chunk;
		remaining -= chunk;
	}

	return 0;
}

static unsigned long orlix_uaccess_copy_to_user(unsigned long to,
					const void *from, unsigned long n)
{
	struct mm_struct *mm = current->mm;
	unsigned long remaining = n;
	const unsigned char *src = from;

	/* See the corresponding from-user path above. */
	if (!mm || faulthandler_disabled())
		return n;

	while (remaining) {
		unsigned long chunk = min(remaining,
					  PAGE_SIZE - offset_in_page(to));

		if (tcti_write_user_data(mm, to, src, chunk))
			return remaining;

		src += chunk;
		to += chunk;
		remaining -= chunk;
	}

	return 0;
}

unsigned long raw_copy_from_user(void *to, const void __user *from,
				 unsigned long n)
{
	return orlix_uaccess_copy_from_user(to, (unsigned long)from, n);
}

unsigned long raw_copy_to_user(void __user *to, const void *from,
			       unsigned long n)
{
	return orlix_uaccess_copy_to_user((unsigned long)to, from, n);
}

unsigned long __clear_user(void __user *to, unsigned long n)
{
	static const unsigned long zero_page[PAGE_SIZE / sizeof(unsigned long)];
	unsigned long remaining = n;
	unsigned long address = (unsigned long)to;

	while (remaining) {
		unsigned long chunk = min(remaining,
					  PAGE_SIZE - offset_in_page(address));
		unsigned long left;

		left = orlix_uaccess_copy_to_user(address, zero_page, chunk);
		if (left)
			return remaining - (chunk - left);

		address += chunk;
		remaining -= chunk;
	}

	return 0;
}
