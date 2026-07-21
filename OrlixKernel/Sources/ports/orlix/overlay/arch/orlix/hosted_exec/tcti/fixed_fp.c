// SPDX-License-Identifier: GPL-2.0-only
#include <linux/errno.h>
#include <linux/preempt.h>
#include <linux/types.h>

#include "fixed_fp.h"

#define TCTI_FIXED_FP_TABLE_32(operation, destination, source) \
	"sub w1, w1, #1\n" \
	"adr x2, 1f\n" \
	"add x2, x2, x1, lsl #3\n" \
	"br x2\n" \
	"1:\n" \
	".irp fbits,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16," \
		"17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32\n" \
	operation " " destination ", " source ", #\\fbits\n" \
	"b 2f\n" \
	".endr\n"

#define TCTI_FIXED_FP_TABLE_64(operation, destination, source) \
	".irp fbits,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48," \
		"49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64\n" \
	operation " " destination ", " source ", #\\fbits\n" \
	"b 2f\n" \
	".endr\n"

#define TCTI_DEFINE_FIXED_FP_32(name, load, operation, destination, source) \
	__attribute__((naked)) u64 name(u64 value, u64 fractional_bits) \
	{ \
		asm(load "\n" \
		    TCTI_FIXED_FP_TABLE_32(operation, destination, source) \
		    "2:\n" \
		    "ret\n"); \
	}

#define TCTI_DEFINE_FIXED_FP_64(name, load, operation, destination, source) \
	__attribute__((naked)) u64 name(u64 value, u64 fractional_bits) \
	{ \
		asm(load "\n" \
		    TCTI_FIXED_FP_TABLE_32(operation, destination, source) \
		    TCTI_FIXED_FP_TABLE_64(operation, destination, source) \
		    "2:\n" \
		    "ret\n"); \
	}

TCTI_DEFINE_FIXED_FP_32(tcti_native_fcvtzs_w_s, "fmov s0, w0", "fcvtzs",
			"w0", "s0")
TCTI_DEFINE_FIXED_FP_32(tcti_native_fcvtzs_w_d, "fmov d0, x0", "fcvtzs",
			"w0", "d0")
TCTI_DEFINE_FIXED_FP_64(tcti_native_fcvtzs_x_s, "fmov s0, w0", "fcvtzs",
			"x0", "s0")
TCTI_DEFINE_FIXED_FP_64(tcti_native_fcvtzs_x_d, "fmov d0, x0", "fcvtzs",
			"x0", "d0")
TCTI_DEFINE_FIXED_FP_32(tcti_native_fcvtzu_w_s, "fmov s0, w0", "fcvtzu",
			"w0", "s0")
TCTI_DEFINE_FIXED_FP_32(tcti_native_fcvtzu_w_d, "fmov d0, x0", "fcvtzu",
			"w0", "d0")
TCTI_DEFINE_FIXED_FP_64(tcti_native_fcvtzu_x_s, "fmov s0, w0", "fcvtzu",
			"x0", "s0")
TCTI_DEFINE_FIXED_FP_64(tcti_native_fcvtzu_x_d, "fmov d0, x0", "fcvtzu",
			"x0", "d0")

#define TCTI_NATIVE_FP_ONE_SOURCE_RUN(instruction) \
	({ \
		asm volatile("ldr q0, [%[source]]\n" \
			     instruction "\n" \
			     "str q0, [%[result]]\n" \
			     : \
			     : [result] "r" (result), [source] "r" (source) \
			     : "v0", "memory"); \
	})

int tcti_native_fp_one_source(enum tcti_fp_scalar_1source_op operation,
	u8 access_size, u8 result_size, u64 result[2], const u64 source[2],
	unsigned long fpcr, unsigned long *fpsr)
{
	unsigned long host_fpcr;
	unsigned long host_fpsr;
	unsigned long guest_fpsr;
	int ret = 0;

	if (!result || !source || !fpsr)
		return -EINVAL;
	if (access_size != sizeof(u32) && access_size != sizeof(u64))
		return -EINVAL;
	if (result_size != sizeof(u32) && result_size != sizeof(u64))
		return -EINVAL;

	preempt_disable();
	asm volatile("mrs %0, fpcr\n"
		     "mrs %1, fpsr\n"
		     : "=r" (host_fpcr), "=r" (host_fpsr));
	asm volatile("msr fpcr, %0\n"
		     "msr fpsr, %1\n"
		     "isb\n"
		     :
		     : "r" (fpcr), "r" (*fpsr)
		     : "memory");

	switch (operation) {
	case TCTI_FP1_FCVT:
		if (access_size == sizeof(u32) && result_size == sizeof(u64))
			TCTI_NATIVE_FP_ONE_SOURCE_RUN("fcvt d0, s0");
		else if (access_size == sizeof(u64) &&
			 result_size == sizeof(u32))
			TCTI_NATIVE_FP_ONE_SOURCE_RUN("fcvt s0, d0");
		else
			ret = -EINVAL;
		break;
	case TCTI_FP1_FSQRT:
		if (access_size == sizeof(u32) && result_size == sizeof(u32))
			TCTI_NATIVE_FP_ONE_SOURCE_RUN("fsqrt s0, s0");
		else if (access_size == sizeof(u64) &&
			 result_size == sizeof(u64))
			TCTI_NATIVE_FP_ONE_SOURCE_RUN("fsqrt d0, d0");
		else
			ret = -EINVAL;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	asm volatile("mrs %0, fpsr\n" : "=r" (guest_fpsr));
	asm volatile("msr fpcr, %0\n"
		     "msr fpsr, %1\n"
		     "isb\n"
		     :
		     : "r" (host_fpcr), "r" (host_fpsr)
		     : "memory");
	*fpsr = guest_fpsr;
	preempt_enable();
	return ret;
}

enum tcti_native_simd_fp_shape {
	TCTI_NATIVE_SIMD_FP_SCALAR_S,
	TCTI_NATIVE_SIMD_FP_SCALAR_D,
	TCTI_NATIVE_SIMD_FP_VECTOR_2S,
	TCTI_NATIVE_SIMD_FP_VECTOR_4S,
	TCTI_NATIVE_SIMD_FP_VECTOR_2D,
};

#define TCTI_NATIVE_SIMD_FP_RUN(instruction) \
	({ \
		asm volatile("ldr q0, [%[left]]\n" \
			     "ldr q1, [%[right]]\n" \
			     instruction "\n" \
			     "str q0, [%[result]]\n" \
			     : \
			     : [result] "r" (result), [left] "r" (left), \
			       [right] "r" (right) \
			     : "v0", "v1", "memory"); \
	})

#define TCTI_NATIVE_SIMD_FP_CASE(operation, scalar_s, scalar_d, vector_2s, \
				 vector_4s, vector_2d) \
	case operation: \
		switch (shape) { \
		case TCTI_NATIVE_SIMD_FP_SCALAR_S: \
			TCTI_NATIVE_SIMD_FP_RUN(scalar_s); \
			break; \
		case TCTI_NATIVE_SIMD_FP_SCALAR_D: \
			TCTI_NATIVE_SIMD_FP_RUN(scalar_d); \
			break; \
		case TCTI_NATIVE_SIMD_FP_VECTOR_2S: \
			TCTI_NATIVE_SIMD_FP_RUN(vector_2s); \
			break; \
		case TCTI_NATIVE_SIMD_FP_VECTOR_4S: \
			TCTI_NATIVE_SIMD_FP_RUN(vector_4s); \
			break; \
		case TCTI_NATIVE_SIMD_FP_VECTOR_2D: \
			TCTI_NATIVE_SIMD_FP_RUN(vector_2d); \
			break; \
		} \
		break

#define TCTI_NATIVE_SIMD_FP_VECTOR_CASE(operation, vector_2s, vector_4s, \
					vector_2d) \
	case operation: \
		switch (shape) { \
		case TCTI_NATIVE_SIMD_FP_VECTOR_2S: \
			TCTI_NATIVE_SIMD_FP_RUN(vector_2s); \
			break; \
		case TCTI_NATIVE_SIMD_FP_VECTOR_4S: \
			TCTI_NATIVE_SIMD_FP_RUN(vector_4s); \
			break; \
		case TCTI_NATIVE_SIMD_FP_VECTOR_2D: \
			TCTI_NATIVE_SIMD_FP_RUN(vector_2d); \
			break; \
		default: \
			ret = -EINVAL; \
			break; \
		} \
		break

#define TCTI_NATIVE_SIMD_FP_ACCUMULATE_RUN(instruction) \
	({ \
		asm volatile("ldr q0, [%[left]]\n" \
			     "ldr q1, [%[right]]\n" \
			     "ldr q2, [%[accumulator]]\n" \
			     instruction "\n" \
			     "str q2, [%[result]]\n" \
			     : \
			     : [result] "r" (result), [left] "r" (left), \
			       [right] "r" (right), \
			       [accumulator] "r" (accumulator) \
			     : "v0", "v1", "v2", "memory"); \
	})

#define TCTI_NATIVE_SIMD_FP_ACCUMULATE_CASE(operation, vector_2s, vector_4s, \
					    vector_2d) \
	case operation: \
		switch (shape) { \
		case TCTI_NATIVE_SIMD_FP_VECTOR_2S: \
			TCTI_NATIVE_SIMD_FP_ACCUMULATE_RUN(vector_2s); \
			break; \
		case TCTI_NATIVE_SIMD_FP_VECTOR_4S: \
			TCTI_NATIVE_SIMD_FP_ACCUMULATE_RUN(vector_4s); \
			break; \
		case TCTI_NATIVE_SIMD_FP_VECTOR_2D: \
			TCTI_NATIVE_SIMD_FP_ACCUMULATE_RUN(vector_2d); \
			break; \
		default: \
			ret = -EINVAL; \
			break; \
		} \
		break

int tcti_native_simd_fp_three_same(
	enum tcti_simd_vector_arithmetic_op operation, bool scalar,
	u8 access_size, u8 result_size, u64 result[2], const u64 left[2],
	const u64 right[2], const u64 accumulator[2], unsigned long fpcr,
	unsigned long *fpsr)
{
	enum tcti_native_simd_fp_shape shape;
	unsigned long host_fpcr;
	unsigned long host_fpsr;
	unsigned long guest_fpsr;
	int ret = 0;

	if (!result || !left || !right || !accumulator || !fpsr)
		return -EINVAL;
	if (scalar && access_size == sizeof(u32) && result_size == sizeof(u32))
		shape = TCTI_NATIVE_SIMD_FP_SCALAR_S;
	else if (scalar && access_size == sizeof(u64) &&
		 result_size == sizeof(u64))
		shape = TCTI_NATIVE_SIMD_FP_SCALAR_D;
	else if (!scalar && access_size == sizeof(u32) &&
		 result_size == sizeof(u64))
		shape = TCTI_NATIVE_SIMD_FP_VECTOR_2S;
	else if (!scalar && access_size == sizeof(u32) &&
		 result_size == 2 * sizeof(u64))
		shape = TCTI_NATIVE_SIMD_FP_VECTOR_4S;
	else if (!scalar && access_size == sizeof(u64) &&
		 result_size == 2 * sizeof(u64))
		shape = TCTI_NATIVE_SIMD_FP_VECTOR_2D;
	else
		return -EINVAL;

	preempt_disable();
	asm volatile("mrs %0, fpcr\n"
		     "mrs %1, fpsr\n"
		     : "=r" (host_fpcr), "=r" (host_fpsr));
	asm volatile("msr fpcr, %0\n"
		     "msr fpsr, %1\n"
		     "isb\n"
		     :
		     : "r" (fpcr), "r" (*fpsr)
		     : "memory");

	switch (operation) {
	TCTI_NATIVE_SIMD_FP_CASE(TCTI_SIMD_ARITH_FABD,
		"fabd s0, s0, s1", "fabd d0, d0, d1",
		"fabd v0.2s, v0.2s, v1.2s", "fabd v0.4s, v0.4s, v1.4s",
		"fabd v0.2d, v0.2d, v1.2d");
	TCTI_NATIVE_SIMD_FP_CASE(TCTI_SIMD_ARITH_FACGE,
		"facge s0, s0, s1", "facge d0, d0, d1",
		"facge v0.2s, v0.2s, v1.2s", "facge v0.4s, v0.4s, v1.4s",
		"facge v0.2d, v0.2d, v1.2d");
	TCTI_NATIVE_SIMD_FP_CASE(TCTI_SIMD_ARITH_FACGT,
		"facgt s0, s0, s1", "facgt d0, d0, d1",
		"facgt v0.2s, v0.2s, v1.2s", "facgt v0.4s, v0.4s, v1.4s",
		"facgt v0.2d, v0.2d, v1.2d");
	TCTI_NATIVE_SIMD_FP_CASE(TCTI_SIMD_ARITH_FCMEQ,
		"fcmeq s0, s0, s1", "fcmeq d0, d0, d1",
		"fcmeq v0.2s, v0.2s, v1.2s", "fcmeq v0.4s, v0.4s, v1.4s",
		"fcmeq v0.2d, v0.2d, v1.2d");
	TCTI_NATIVE_SIMD_FP_CASE(TCTI_SIMD_ARITH_FCMGE,
		"fcmge s0, s0, s1", "fcmge d0, d0, d1",
		"fcmge v0.2s, v0.2s, v1.2s", "fcmge v0.4s, v0.4s, v1.4s",
		"fcmge v0.2d, v0.2d, v1.2d");
	TCTI_NATIVE_SIMD_FP_CASE(TCTI_SIMD_ARITH_FCMGT,
		"fcmgt s0, s0, s1", "fcmgt d0, d0, d1",
		"fcmgt v0.2s, v0.2s, v1.2s", "fcmgt v0.4s, v0.4s, v1.4s",
		"fcmgt v0.2d, v0.2d, v1.2d");
	TCTI_NATIVE_SIMD_FP_CASE(TCTI_SIMD_ARITH_FMULX,
		"fmulx s0, s0, s1", "fmulx d0, d0, d1",
		"fmulx v0.2s, v0.2s, v1.2s", "fmulx v0.4s, v0.4s, v1.4s",
		"fmulx v0.2d, v0.2d, v1.2d");
	TCTI_NATIVE_SIMD_FP_CASE(TCTI_SIMD_ARITH_FRECPS,
		"frecps s0, s0, s1", "frecps d0, d0, d1",
		"frecps v0.2s, v0.2s, v1.2s", "frecps v0.4s, v0.4s, v1.4s",
		"frecps v0.2d, v0.2d, v1.2d");
	TCTI_NATIVE_SIMD_FP_CASE(TCTI_SIMD_ARITH_FRSQRTS,
		"frsqrts s0, s0, s1", "frsqrts d0, d0, d1",
		"frsqrts v0.2s, v0.2s, v1.2s",
		"frsqrts v0.4s, v0.4s, v1.4s",
		"frsqrts v0.2d, v0.2d, v1.2d");
	TCTI_NATIVE_SIMD_FP_VECTOR_CASE(TCTI_SIMD_ARITH_FADDP,
		"faddp v0.2s, v0.2s, v1.2s", "faddp v0.4s, v0.4s, v1.4s",
		"faddp v0.2d, v0.2d, v1.2d");
	TCTI_NATIVE_SIMD_FP_VECTOR_CASE(TCTI_SIMD_ARITH_FADD,
		"fadd v0.2s, v0.2s, v1.2s", "fadd v0.4s, v0.4s, v1.4s",
		"fadd v0.2d, v0.2d, v1.2d");
	TCTI_NATIVE_SIMD_FP_VECTOR_CASE(TCTI_SIMD_ARITH_FDIV,
		"fdiv v0.2s, v0.2s, v1.2s", "fdiv v0.4s, v0.4s, v1.4s",
		"fdiv v0.2d, v0.2d, v1.2d");
	TCTI_NATIVE_SIMD_FP_VECTOR_CASE(TCTI_SIMD_ARITH_FMAXNMP,
		"fmaxnmp v0.2s, v0.2s, v1.2s",
		"fmaxnmp v0.4s, v0.4s, v1.4s",
		"fmaxnmp v0.2d, v0.2d, v1.2d");
	TCTI_NATIVE_SIMD_FP_VECTOR_CASE(TCTI_SIMD_ARITH_FMAXNM,
		"fmaxnm v0.2s, v0.2s, v1.2s",
		"fmaxnm v0.4s, v0.4s, v1.4s",
		"fmaxnm v0.2d, v0.2d, v1.2d");
	TCTI_NATIVE_SIMD_FP_VECTOR_CASE(TCTI_SIMD_ARITH_FMAXP,
		"fmaxp v0.2s, v0.2s, v1.2s", "fmaxp v0.4s, v0.4s, v1.4s",
		"fmaxp v0.2d, v0.2d, v1.2d");
	TCTI_NATIVE_SIMD_FP_VECTOR_CASE(TCTI_SIMD_ARITH_FMAX,
		"fmax v0.2s, v0.2s, v1.2s", "fmax v0.4s, v0.4s, v1.4s",
		"fmax v0.2d, v0.2d, v1.2d");
	TCTI_NATIVE_SIMD_FP_VECTOR_CASE(TCTI_SIMD_ARITH_FMINNMP,
		"fminnmp v0.2s, v0.2s, v1.2s",
		"fminnmp v0.4s, v0.4s, v1.4s",
		"fminnmp v0.2d, v0.2d, v1.2d");
	TCTI_NATIVE_SIMD_FP_VECTOR_CASE(TCTI_SIMD_ARITH_FMINNM,
		"fminnm v0.2s, v0.2s, v1.2s",
		"fminnm v0.4s, v0.4s, v1.4s",
		"fminnm v0.2d, v0.2d, v1.2d");
	TCTI_NATIVE_SIMD_FP_VECTOR_CASE(TCTI_SIMD_ARITH_FMINP,
		"fminp v0.2s, v0.2s, v1.2s", "fminp v0.4s, v0.4s, v1.4s",
		"fminp v0.2d, v0.2d, v1.2d");
	TCTI_NATIVE_SIMD_FP_VECTOR_CASE(TCTI_SIMD_ARITH_FMIN,
		"fmin v0.2s, v0.2s, v1.2s", "fmin v0.4s, v0.4s, v1.4s",
		"fmin v0.2d, v0.2d, v1.2d");
	TCTI_NATIVE_SIMD_FP_ACCUMULATE_CASE(TCTI_SIMD_ARITH_FMLA,
		"fmla v2.2s, v0.2s, v1.2s", "fmla v2.4s, v0.4s, v1.4s",
		"fmla v2.2d, v0.2d, v1.2d");
	TCTI_NATIVE_SIMD_FP_ACCUMULATE_CASE(TCTI_SIMD_ARITH_FMLS,
		"fmls v2.2s, v0.2s, v1.2s", "fmls v2.4s, v0.4s, v1.4s",
		"fmls v2.2d, v0.2d, v1.2d");
	TCTI_NATIVE_SIMD_FP_VECTOR_CASE(TCTI_SIMD_ARITH_FMUL,
		"fmul v0.2s, v0.2s, v1.2s", "fmul v0.4s, v0.4s, v1.4s",
		"fmul v0.2d, v0.2d, v1.2d");
	TCTI_NATIVE_SIMD_FP_VECTOR_CASE(TCTI_SIMD_ARITH_FSUB,
		"fsub v0.2s, v0.2s, v1.2s", "fsub v0.4s, v0.4s, v1.4s",
		"fsub v0.2d, v0.2d, v1.2d");
	default:
		ret = -EINVAL;
		break;
	}

	asm volatile("mrs %0, fpsr\n" : "=r" (guest_fpsr));
	asm volatile("msr fpcr, %0\n"
		     "msr fpsr, %1\n"
		     "isb\n"
		     :
		     : "r" (host_fpcr), "r" (host_fpsr)
		     : "memory");
	*fpsr = guest_fpsr;
	preempt_enable();
	return ret;
}

#define TCTI_NATIVE_SIMD_FP_PAIRWISE_RUN(instruction) \
	({ \
		asm volatile("ldr q0, [%[source]]\n" \
			     instruction "\n" \
			     "str q0, [%[result]]\n" \
			     : \
			     : [result] "r" (result), [source] "r" (source) \
			     : "v0", "memory"); \
	})

int tcti_native_simd_fp_pairwise(
	enum tcti_simd_reduction_op operation, u8 access_size, u64 result[2],
	const u64 source[2], unsigned long fpcr, unsigned long *fpsr)
{
	unsigned long host_fpcr;
	unsigned long host_fpsr;
	unsigned long guest_fpsr;
	int ret = 0;

	if (!result || !source || !fpsr ||
	    (access_size != sizeof(u32) && access_size != sizeof(u64)))
		return -EINVAL;

	preempt_disable();
	asm volatile("mrs %0, fpcr\n"
		     "mrs %1, fpsr\n"
		     : "=r" (host_fpcr), "=r" (host_fpsr));
	asm volatile("msr fpcr, %0\n"
		     "msr fpsr, %1\n"
		     "isb\n"
		     :
		     : "r" (fpcr), "r" (*fpsr)
		     : "memory");

	switch (operation) {
	case TCTI_SIMD_REDUCTION_FADDP:
		if (access_size == sizeof(u32))
			TCTI_NATIVE_SIMD_FP_PAIRWISE_RUN("faddp s0, v0.2s");
		else
			TCTI_NATIVE_SIMD_FP_PAIRWISE_RUN("faddp d0, v0.2d");
		break;
	case TCTI_SIMD_REDUCTION_FMAXNMP:
		if (access_size == sizeof(u32))
			TCTI_NATIVE_SIMD_FP_PAIRWISE_RUN("fmaxnmp s0, v0.2s");
		else
			TCTI_NATIVE_SIMD_FP_PAIRWISE_RUN("fmaxnmp d0, v0.2d");
		break;
	case TCTI_SIMD_REDUCTION_FMAXP:
		if (access_size == sizeof(u32))
			TCTI_NATIVE_SIMD_FP_PAIRWISE_RUN("fmaxp s0, v0.2s");
		else
			TCTI_NATIVE_SIMD_FP_PAIRWISE_RUN("fmaxp d0, v0.2d");
		break;
	case TCTI_SIMD_REDUCTION_FMINNMP:
		if (access_size == sizeof(u32))
			TCTI_NATIVE_SIMD_FP_PAIRWISE_RUN("fminnmp s0, v0.2s");
		else
			TCTI_NATIVE_SIMD_FP_PAIRWISE_RUN("fminnmp d0, v0.2d");
		break;
	case TCTI_SIMD_REDUCTION_FMINP:
		if (access_size == sizeof(u32))
			TCTI_NATIVE_SIMD_FP_PAIRWISE_RUN("fminp s0, v0.2s");
		else
			TCTI_NATIVE_SIMD_FP_PAIRWISE_RUN("fminp d0, v0.2d");
		break;
	default:
		ret = -EINVAL;
		break;
	}

	asm volatile("mrs %0, fpsr\n" : "=r" (guest_fpsr));
	asm volatile("msr fpcr, %0\n"
		     "msr fpsr, %1\n"
		     "isb\n"
		     :
		     : "r" (host_fpcr), "r" (host_fpsr)
		     : "memory");
	*fpsr = guest_fpsr;
	preempt_enable();
	return ret;
}
