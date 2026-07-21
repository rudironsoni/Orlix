/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_FIXED_FP_H
#define ORLIX_TCTI_FIXED_FP_H

#include <linux/types.h>

#include "decode_aarch64.h"

int tcti_native_simd_fp_three_same(
	enum tcti_simd_vector_arithmetic_op operation, bool scalar,
	u8 access_size, u8 result_size, u64 result[2], const u64 left[2],
	const u64 right[2], const u64 accumulator[2], unsigned long fpcr,
	unsigned long *fpsr);
int tcti_native_simd_fp_pairwise(
	enum tcti_simd_reduction_op operation, u8 access_size, u64 result[2],
	const u64 source[2], unsigned long fpcr, unsigned long *fpsr);

#endif /* ORLIX_TCTI_FIXED_FP_H */
