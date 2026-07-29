// SPDX-License-Identifier: GPL-2.0-only
#include <linux/highmem.h>
#include <linux/mm.h>

#include <asm/mte.h>

void copy_highpage(struct page *to, struct page *from)
{
	void *to_address = kmap_local_page(to);
	void *from_address = kmap_local_page(from);

	copy_page(to_address, from_address);
	kunmap_local(from_address);
	kunmap_local(to_address);
	/* COW publishes a distinct page with a snapshot of its allocation tags. */
	orlix_mte_copy_page_tags(to, from);
}

void copy_user_highpage(struct page *to, struct page *from,
			unsigned long vaddr, struct vm_area_struct *vma)
{
	(void)vaddr;
	(void)vma;
	copy_highpage(to, from);
}
