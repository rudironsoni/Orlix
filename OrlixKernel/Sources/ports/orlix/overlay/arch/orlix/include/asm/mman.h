/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_ORLIX_MMAN_H
#define _ASM_ORLIX_MMAN_H

#include <uapi/asm/mman.h>

#ifndef BUILD_VDSO
#include <linux/mm.h>

/* Keep the generic VM names: core-mm and the kselftests use these names. */
/* asm/mman.h is parsed before linux/mm.h publishes VM_HIGH_ARCH_*; use the
 * identical reserved high-VMA bits here. CONFIG_ARM64_MTE makes generic mm
 * publish VM_MTE and VM_MTE_ALLOWED with these same values later. */
#define ORLIX_VM_MTE (1UL << 36)
#define ORLIX_VM_MTE_ALLOWED (1UL << 37)

static inline unsigned long arch_calc_vm_prot_bits(unsigned long prot,
					   unsigned long pkey)
{
	(void)pkey;
	return prot & PROT_MTE ? ORLIX_VM_MTE : 0;
}
#define arch_calc_vm_prot_bits(prot, pkey) arch_calc_vm_prot_bits(prot, pkey)

static inline unsigned long arch_calc_vm_flag_bits(struct file *file,
					   unsigned long flags)
{
	return !file && (flags & MAP_ANONYMOUS) ? ORLIX_VM_MTE_ALLOWED : 0;
}
#define arch_calc_vm_flag_bits(file, flags) arch_calc_vm_flag_bits(file, flags)

static inline bool arch_validate_prot(unsigned long prot,
				      unsigned long addr __always_unused)
{
	return !(prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC | PROT_SEM | PROT_MTE));
}
#define arch_validate_prot(prot, addr) arch_validate_prot(prot, addr)

static inline bool arch_validate_flags(unsigned long vm_flags)
{
	return !(vm_flags & ORLIX_VM_MTE) ||
		(vm_flags & ORLIX_VM_MTE_ALLOWED);
}
#define arch_validate_flags(vm_flags) arch_validate_flags(vm_flags)
#endif

#endif /* _ASM_ORLIX_MMAN_H */
