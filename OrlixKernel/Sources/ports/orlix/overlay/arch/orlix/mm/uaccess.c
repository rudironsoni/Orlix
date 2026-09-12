// SPDX-License-Identifier: GPL-2.0-only

#include <linux/mm.h>
#include <linux/uaccess.h>
#include <asm/page.h>
#include <asm/orlix_tcti.h>

static unsigned long orlix_uaccess_copy_from_user(void *to,
					  unsigned long from,
					  unsigned long n)
{
	struct mm_struct *mm = current->mm;
	unsigned long remaining = n;
	unsigned char *dst = to;

	/*
	 * raw_copy_from_user() also backs __copy_from_user_inatomic().
	 * generic_perform_write() copies with pagefaults disabled after
	 * fault_in_iov_iter_readable(). A full residual here makes that loop
	 * retry forever. Copy present pages without sleeping; if a page is
	 * missing or a lock would sleep, return the residual.
	 */
	if (!mm)
		return n;

	while (remaining) {
		unsigned long chunk = min(remaining,
					  PAGE_SIZE - offset_in_page(from));
		int ret;

		if (faulthandler_disabled())
			ret = orlix_tcti_copy_user_data_nofault(
				mm, from, dst, chunk, ORLIX_TCTI_ACCESS_READ);
		else
			ret = orlix_tcti_read_user_data(mm, from, dst, chunk);
		if (ret)
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
	if (!mm)
		return n;

	while (remaining) {
		unsigned long chunk = min(remaining,
					  PAGE_SIZE - offset_in_page(to));
		int ret;

		if (faulthandler_disabled())
			ret = orlix_tcti_copy_user_data_nofault(
				mm, to, (void *)src, chunk,
				ORLIX_TCTI_ACCESS_WRITE);
		else
			ret = orlix_tcti_write_user_data(mm, to, src, chunk);
		if (ret)
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
