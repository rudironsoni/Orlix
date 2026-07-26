/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_FIXED_FP_H
#define ORLIX_TCTI_FIXED_FP_H

#include <linux/types.h>

#include "decode_aarch64.h"

int orlix_tcti_native_fp_one_source(enum orlix_tcti_fp_scalar_1source_op operation,
	u8 access_size, u8 result_size, u64 result[2], const u64 source[2],
	unsigned long fpcr, unsigned long *fpsr);
int orlix_tcti_native_fp_to_gpr(enum orlix_tcti_fp_int_convert_op operation,
	u8 access_size, u8 result_size, u64 source, u64 *result,
	unsigned long fpcr, unsigned long *fpsr);
u64 orlix_tcti_native_gpr_to_fp_fixed(enum orlix_tcti_fp_int_convert_op operation,
	u8 access_size, u8 result_size, u64 source, u8 fractional_bits);
int orlix_tcti_native_fixed_simd_fp_convert(
	enum orlix_tcti_fp_int_convert_op operation, bool scalar, bool q,
	u8 access_size, u8 fractional_bits, u64 result[2],
	const u64 source[2], unsigned long fpcr, unsigned long *fpsr);
int orlix_tcti_native_simd_fp_convert(enum orlix_tcti_fp_int_convert_op operation,
	bool scalar, bool q, u8 access_size, u64 result[2],
	const u64 source[2], unsigned long fpcr, unsigned long *fpsr);
int orlix_tcti_native_simd_fp_three_same(
	enum orlix_tcti_simd_vector_arithmetic_op operation, bool scalar,
	u8 access_size, u8 result_size, u64 result[2], const u64 left[2],
	const u64 right[2], const u64 accumulator[2], unsigned long fpcr,
	unsigned long *fpsr);
int orlix_tcti_native_simd_fp16_three_same(
	enum orlix_tcti_simd_vector_arithmetic_op operation, bool scalar, bool q,
	u64 result[2], const u64 left[2], const u64 right[2],
	const u64 accumulator[2], unsigned long fpcr, unsigned long *fpsr);
int orlix_tcti_native_simd_fp_scalar_unary(
	enum orlix_tcti_simd_vector_arithmetic_op operation, u8 access_size,
	u8 result_size, u64 result[2], const u64 source[2], unsigned long fpcr,
	unsigned long *fpsr);
int orlix_tcti_native_simd_fp_two_register(
	enum orlix_tcti_simd_vector_arithmetic_op operation, u8 access_size,
	u8 result_size, u8 source_index, u8 destination_index, u64 result[2],
	const u64 source[2], const u64 accumulator[2], unsigned long fpcr,
	unsigned long *fpsr);
int orlix_tcti_native_simd_fp_reduction(
	enum orlix_tcti_simd_reduction_op operation, u8 access_size, u64 result[2],
	const u64 source[2], unsigned long fpcr, unsigned long *fpsr);

#endif /* ORLIX_TCTI_FIXED_FP_H */
