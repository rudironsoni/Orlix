/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_HOST_TEST_LINUX_BITOPS_H
#define ORLIX_TCTI_HOST_TEST_LINUX_BITOPS_H

#include <linux/types.h>

static inline s64 sign_extend64(u64 value, int index)
{
	u8 shift = 63 - index;

	return (s64)(value << shift) >> shift;
}

static inline int fls(unsigned int x)
{
	return x ? 32 - __builtin_clz(x) : 0;
}

#endif /* ORLIX_TCTI_HOST_TEST_LINUX_BITOPS_H */
