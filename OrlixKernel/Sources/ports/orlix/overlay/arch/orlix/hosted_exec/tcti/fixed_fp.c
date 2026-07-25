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

#define TCTI_DEFINE_FIXED_GPR_TO_FP_32(name, operation, destination, source, \
				       move) \
	static __attribute__((naked)) u64 name(u64 value, u64 fractional_bits) \
	{ \
		asm(TCTI_FIXED_FP_TABLE_32(operation, destination, source) \
		    "2:\n" \
		    move "\n" \
		    "ret\n"); \
	}

#define TCTI_DEFINE_FIXED_GPR_TO_FP_64(name, operation, destination, source, \
				       move) \
	static __attribute__((naked)) u64 name(u64 value, u64 fractional_bits) \
	{ \
		asm(TCTI_FIXED_FP_TABLE_32(operation, destination, source) \
		    TCTI_FIXED_FP_TABLE_64(operation, destination, source) \
		    "2:\n" \
		    move "\n" \
		    "ret\n"); \
	}

TCTI_DEFINE_FIXED_GPR_TO_FP_32(tcti_native_scvtf_s_w, "scvtf", "s0", "w0",
			       "fmov w0, s0")
TCTI_DEFINE_FIXED_GPR_TO_FP_32(tcti_native_scvtf_d_w, "scvtf", "d0", "w0",
			       "fmov x0, d0")
TCTI_DEFINE_FIXED_GPR_TO_FP_64(tcti_native_scvtf_s_x, "scvtf", "s0", "x0",
			       "fmov w0, s0")
TCTI_DEFINE_FIXED_GPR_TO_FP_64(tcti_native_scvtf_d_x, "scvtf", "d0", "x0",
			       "fmov x0, d0")
TCTI_DEFINE_FIXED_GPR_TO_FP_32(tcti_native_ucvtf_s_w, "ucvtf", "s0", "w0",
			       "fmov w0, s0")
TCTI_DEFINE_FIXED_GPR_TO_FP_32(tcti_native_ucvtf_d_w, "ucvtf", "d0", "w0",
			       "fmov x0, d0")
TCTI_DEFINE_FIXED_GPR_TO_FP_64(tcti_native_ucvtf_s_x, "ucvtf", "s0", "x0",
			       "fmov w0, s0")
TCTI_DEFINE_FIXED_GPR_TO_FP_64(tcti_native_ucvtf_d_x, "ucvtf", "d0", "x0",
			       "fmov x0, d0")

u64 tcti_native_gpr_to_fp_fixed(enum tcti_fp_int_convert_op operation,
	u8 access_size, u8 result_size, u64 source, u8 fractional_bits)
{
	bool unsigned_conversion = operation == TCTI_FP_INT_UCVTF_FIXED;

	if (access_size == sizeof(u32)) {
		if (result_size == sizeof(u32))
			return unsigned_conversion ?
				tcti_native_ucvtf_s_w(source, fractional_bits) :
				tcti_native_scvtf_s_w(source, fractional_bits);
		return unsigned_conversion ?
			tcti_native_ucvtf_d_w(source, fractional_bits) :
			tcti_native_scvtf_d_w(source, fractional_bits);
	}

	if (result_size == sizeof(u32))
		return unsigned_conversion ?
			tcti_native_ucvtf_s_x(source, fractional_bits) :
			tcti_native_scvtf_s_x(source, fractional_bits);
	return unsigned_conversion ?
		tcti_native_ucvtf_d_x(source, fractional_bits) :
		tcti_native_scvtf_d_x(source, fractional_bits);
}

#define TCTI_FIXED_SIMD_TABLE_32(operation, destination, source) \
	"sub w2, w2, #1\n" \
	"adr x3, 1f\n" \
	"add x3, x3, x2, lsl #3\n" \
	"br x3\n" \
	"1:\n" \
	".irp fbits,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16," \
		"17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32\n" \
	operation " " destination ", " source ", #\\fbits\n" \
	"b 2f\n" \
	".endr\n"

#define TCTI_FIXED_SIMD_TABLE_64(operation, destination, source) \
	".irp fbits,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48," \
		"49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64\n" \
	operation " " destination ", " source ", #\\fbits\n" \
	"b 2f\n" \
	".endr\n"

#define TCTI_DEFINE_FIXED_SIMD_32(name, operation, destination, source) \
	static __attribute__((naked)) void name(u64 result[2], \
		const u64 input[2], u64 fractional_bits) \
	{ \
		asm("ldr q0, [x1]\n" \
		    TCTI_FIXED_SIMD_TABLE_32(operation, destination, source) \
		    "2:\n" \
		    "str q0, [x0]\n" \
		    "ret\n"); \
	}

#define TCTI_DEFINE_FIXED_SIMD_64(name, operation, destination, source) \
	static __attribute__((naked)) void name(u64 result[2], \
		const u64 input[2], u64 fractional_bits) \
	{ \
		asm("ldr q0, [x1]\n" \
		    TCTI_FIXED_SIMD_TABLE_32(operation, destination, source) \
		    TCTI_FIXED_SIMD_TABLE_64(operation, destination, source) \
		    "2:\n" \
		    "str q0, [x0]\n" \
		    "ret\n"); \
	}

TCTI_DEFINE_FIXED_SIMD_32(tcti_fixed_fcvtzs_s, "fcvtzs", "s0", "s0")
TCTI_DEFINE_FIXED_SIMD_64(tcti_fixed_fcvtzs_d, "fcvtzs", "d0", "d0")
TCTI_DEFINE_FIXED_SIMD_32(tcti_fixed_fcvtzs_2s, "fcvtzs", "v0.2s", "v0.2s")
TCTI_DEFINE_FIXED_SIMD_32(tcti_fixed_fcvtzs_4s, "fcvtzs", "v0.4s", "v0.4s")
TCTI_DEFINE_FIXED_SIMD_64(tcti_fixed_fcvtzs_2d, "fcvtzs", "v0.2d", "v0.2d")
TCTI_DEFINE_FIXED_SIMD_32(tcti_fixed_fcvtzu_s, "fcvtzu", "s0", "s0")
TCTI_DEFINE_FIXED_SIMD_64(tcti_fixed_fcvtzu_d, "fcvtzu", "d0", "d0")
TCTI_DEFINE_FIXED_SIMD_32(tcti_fixed_fcvtzu_2s, "fcvtzu", "v0.2s", "v0.2s")
TCTI_DEFINE_FIXED_SIMD_32(tcti_fixed_fcvtzu_4s, "fcvtzu", "v0.4s", "v0.4s")
TCTI_DEFINE_FIXED_SIMD_64(tcti_fixed_fcvtzu_2d, "fcvtzu", "v0.2d", "v0.2d")
TCTI_DEFINE_FIXED_SIMD_32(tcti_fixed_scvtf_s, "scvtf", "s0", "s0")
TCTI_DEFINE_FIXED_SIMD_64(tcti_fixed_scvtf_d, "scvtf", "d0", "d0")
TCTI_DEFINE_FIXED_SIMD_32(tcti_fixed_scvtf_2s, "scvtf", "v0.2s", "v0.2s")
TCTI_DEFINE_FIXED_SIMD_32(tcti_fixed_scvtf_4s, "scvtf", "v0.4s", "v0.4s")
TCTI_DEFINE_FIXED_SIMD_64(tcti_fixed_scvtf_2d, "scvtf", "v0.2d", "v0.2d")
TCTI_DEFINE_FIXED_SIMD_32(tcti_fixed_ucvtf_s, "ucvtf", "s0", "s0")
TCTI_DEFINE_FIXED_SIMD_64(tcti_fixed_ucvtf_d, "ucvtf", "d0", "d0")
TCTI_DEFINE_FIXED_SIMD_32(tcti_fixed_ucvtf_2s, "ucvtf", "v0.2s", "v0.2s")
TCTI_DEFINE_FIXED_SIMD_32(tcti_fixed_ucvtf_4s, "ucvtf", "v0.4s", "v0.4s")
TCTI_DEFINE_FIXED_SIMD_64(tcti_fixed_ucvtf_2d, "ucvtf", "v0.2d", "v0.2d")

typedef void (*tcti_fixed_simd_fn)(u64 result[2], const u64 source[2],
	u64 fractional_bits);

static tcti_fixed_simd_fn tcti_fixed_simd_function(
	enum tcti_fp_int_convert_op operation, bool scalar, bool q,
	u8 access_size)
{
#define TCTI_FIXED_SIMD_SELECT(prefix) \
	(access_size == sizeof(u64) ? \
		(scalar ? prefix##_d : prefix##_2d) : \
		(scalar ? prefix##_s : (q ? prefix##_4s : prefix##_2s)))

	switch (operation) {
	case TCTI_FP_INT_FCVTZS_FIXED_SIMD:
		return TCTI_FIXED_SIMD_SELECT(tcti_fixed_fcvtzs);
	case TCTI_FP_INT_FCVTZU_FIXED_SIMD:
		return TCTI_FIXED_SIMD_SELECT(tcti_fixed_fcvtzu);
	case TCTI_FP_INT_SCVTF_FIXED_SIMD:
		return TCTI_FIXED_SIMD_SELECT(tcti_fixed_scvtf);
	case TCTI_FP_INT_UCVTF_FIXED_SIMD:
		return TCTI_FIXED_SIMD_SELECT(tcti_fixed_ucvtf);
	default:
		return NULL;
	}

#undef TCTI_FIXED_SIMD_SELECT
}

int tcti_native_fixed_simd_fp_convert(
	enum tcti_fp_int_convert_op operation, bool scalar, bool q,
	u8 access_size, u8 fractional_bits, u64 result[2],
	const u64 source[2], unsigned long fpcr, unsigned long *fpsr)
{
	tcti_fixed_simd_fn function;
	unsigned long guest_fpsr;
	unsigned long host_fpcr;
	unsigned long host_fpsr;

	if (!result || !source || !fpsr ||
	    (access_size != sizeof(u32) && access_size != sizeof(u64)) ||
	    !fractional_bits || fractional_bits > access_size * BITS_PER_BYTE)
		return -EINVAL;

	function = tcti_fixed_simd_function(operation, scalar, q, access_size);
	if (!function)
		return -EINVAL;

	preempt_disable();
	asm volatile("mrs %0, fpcr\n"
		     "mrs %1, fpsr\n"
		     "msr fpcr, %2\n"
		     "msr fpsr, %3\n"
		     "isb\n"
		     : "=&r" (host_fpcr), "=&r" (host_fpsr)
		     : "r" (fpcr), "r" (*fpsr)
		     : "memory");
	function(result, source, fractional_bits);
	asm volatile("mrs %0, fpsr\n"
		     "msr fpcr, %1\n"
		     "msr fpsr, %2\n"
		     "isb\n"
		     : "=&r" (guest_fpsr)
		     : "r" (host_fpcr), "r" (host_fpsr)
		     : "memory");
	*fpsr = guest_fpsr;
	preempt_enable();

	return 0;
}

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
	if (access_size != sizeof(u16) && access_size != sizeof(u32) &&
	    access_size != sizeof(u64))
		return -EINVAL;
	if (result_size != sizeof(u16) && result_size != sizeof(u32) &&
	    result_size != sizeof(u64))
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
		else if (access_size == sizeof(u16) &&
			 result_size == sizeof(u64))
			TCTI_NATIVE_FP_ONE_SOURCE_RUN("fcvt d0, h0");
		else if (access_size == sizeof(u64) &&
			 result_size == sizeof(u16))
			TCTI_NATIVE_FP_ONE_SOURCE_RUN("fcvt h0, d0");
		else if (access_size == sizeof(u32) &&
			 result_size == sizeof(u16))
			TCTI_NATIVE_FP_ONE_SOURCE_RUN("fcvt h0, s0");
		else if (access_size == sizeof(u16) &&
			 result_size == sizeof(u32))
			TCTI_NATIVE_FP_ONE_SOURCE_RUN("fcvt s0, h0");
		else
			ret = -EINVAL;
		break;
	case TCTI_FP1_FSQRT:
		if (access_size == sizeof(u16) && result_size == sizeof(u16))
			TCTI_NATIVE_FP_ONE_SOURCE_RUN("fsqrt h0, h0");
		else if (access_size == sizeof(u32) && result_size == sizeof(u32))
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

#define TCTI_NATIVE_FP_TO_GPR_RUN_S_W(instruction) \
	({ \
		u32 result32; \
		asm volatile("fmov s0, %w[source]\n" \
			     instruction " %w[value], s0\n" \
			     : [value] "=r" (result32) \
			     : [source] "r" ((u32)source) \
			     : "v0", "memory"); \
		*result = result32; \
	})

#define TCTI_NATIVE_FP_TO_GPR_RUN_S_X(instruction) \
	({ \
		asm volatile("fmov s0, %w[source]\n" \
			     instruction " %[value], s0\n" \
			     : [value] "=r" (*result) \
			     : [source] "r" ((u32)source) \
			     : "v0", "memory"); \
	})

#define TCTI_NATIVE_FP_TO_GPR_RUN_D_W(instruction) \
	({ \
		u32 result32; \
		asm volatile("fmov d0, %[source]\n" \
			     instruction " %w[value], d0\n" \
			     : [value] "=r" (result32) \
			     : [source] "r" (source) \
			     : "v0", "memory"); \
		*result = result32; \
	})

#define TCTI_NATIVE_FP_TO_GPR_RUN_D_X(instruction) \
	({ \
		asm volatile("fmov d0, %[source]\n" \
			     instruction " %[value], d0\n" \
			     : [value] "=r" (*result) \
			     : [source] "r" (source) \
			     : "v0", "memory"); \
	})

#define TCTI_NATIVE_FP_TO_GPR_CASE(operation, instruction) \
	case operation: \
		if (access_size == sizeof(u32) && result_size == sizeof(u32)) \
			TCTI_NATIVE_FP_TO_GPR_RUN_S_W(instruction); \
		else if (access_size == sizeof(u32) && \
			 result_size == sizeof(u64)) \
			TCTI_NATIVE_FP_TO_GPR_RUN_S_X(instruction); \
		else if (access_size == sizeof(u64) && \
			 result_size == sizeof(u32)) \
			TCTI_NATIVE_FP_TO_GPR_RUN_D_W(instruction); \
		else if (access_size == sizeof(u64) && \
			 result_size == sizeof(u64)) \
			TCTI_NATIVE_FP_TO_GPR_RUN_D_X(instruction); \
		else \
			ret = -EINVAL; \
		break

int tcti_native_fp_to_gpr(enum tcti_fp_int_convert_op operation,
	u8 access_size, u8 result_size, u64 source, u64 *result,
	unsigned long fpcr, unsigned long *fpsr)
{
	unsigned long host_fpcr;
	unsigned long host_fpsr;
	unsigned long guest_fpsr;
	int ret = 0;

	if (!result || !fpsr)
		return -EINVAL;
	*result = 0;
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
	TCTI_NATIVE_FP_TO_GPR_CASE(TCTI_FP_INT_FCVTNS, "fcvtns");
	TCTI_NATIVE_FP_TO_GPR_CASE(TCTI_FP_INT_FCVTNU, "fcvtnu");
	TCTI_NATIVE_FP_TO_GPR_CASE(TCTI_FP_INT_FCVTPS, "fcvtps");
	TCTI_NATIVE_FP_TO_GPR_CASE(TCTI_FP_INT_FCVTPU, "fcvtpu");
	TCTI_NATIVE_FP_TO_GPR_CASE(TCTI_FP_INT_FCVTMS, "fcvtms");
	TCTI_NATIVE_FP_TO_GPR_CASE(TCTI_FP_INT_FCVTMU, "fcvtmu");
	TCTI_NATIVE_FP_TO_GPR_CASE(TCTI_FP_INT_FCVTAS, "fcvtas");
	TCTI_NATIVE_FP_TO_GPR_CASE(TCTI_FP_INT_FCVTAU, "fcvtau");
	TCTI_NATIVE_FP_TO_GPR_CASE(TCTI_FP_INT_FCVTZS, "fcvtzs");
	TCTI_NATIVE_FP_TO_GPR_CASE(TCTI_FP_INT_FCVTZU, "fcvtzu");
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

#define TCTI_NATIVE_SIMD_FP_CONVERT_RUN(instruction) \
	({ \
		asm volatile("ldr q0, [%[source]]\n" \
			     instruction "\n" \
			     "str q0, [%[result]]\n" \
			     : \
			     : [result] "r" (result), [source] "r" (source) \
			     : "v0", "memory"); \
	})

#define TCTI_NATIVE_SIMD_FP_CONVERT_CASE(operation, instruction) \
	case operation: \
		if (scalar && access_size == sizeof(u32)) \
			TCTI_NATIVE_SIMD_FP_CONVERT_RUN(instruction " s0, s0"); \
		else if (scalar && access_size == sizeof(u64)) \
			TCTI_NATIVE_SIMD_FP_CONVERT_RUN(instruction " d0, d0"); \
		else if (!scalar && access_size == sizeof(u32) && !q) \
			TCTI_NATIVE_SIMD_FP_CONVERT_RUN(instruction " v0.2s, v0.2s"); \
		else if (!scalar && access_size == sizeof(u32) && q) \
			TCTI_NATIVE_SIMD_FP_CONVERT_RUN(instruction " v0.4s, v0.4s"); \
		else if (!scalar && access_size == sizeof(u64) && q) \
			TCTI_NATIVE_SIMD_FP_CONVERT_RUN(instruction " v0.2d, v0.2d"); \
		else \
			ret = -EINVAL; \
		break

int tcti_native_simd_fp_convert(enum tcti_fp_int_convert_op operation,
	bool scalar, bool q, u8 access_size, u64 result[2],
	const u64 source[2], unsigned long fpcr, unsigned long *fpsr)
{
	unsigned long host_fpcr;
	unsigned long host_fpsr;
	unsigned long guest_fpsr;
	int ret = 0;

	if (!result || !source || !fpsr)
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
	TCTI_NATIVE_SIMD_FP_CONVERT_CASE(TCTI_FP_INT_FCVTNS_SIMD, "fcvtns");
	TCTI_NATIVE_SIMD_FP_CONVERT_CASE(TCTI_FP_INT_FCVTNU_SIMD, "fcvtnu");
	TCTI_NATIVE_SIMD_FP_CONVERT_CASE(TCTI_FP_INT_FCVTPS_SIMD, "fcvtps");
	TCTI_NATIVE_SIMD_FP_CONVERT_CASE(TCTI_FP_INT_FCVTPU_SIMD, "fcvtpu");
	TCTI_NATIVE_SIMD_FP_CONVERT_CASE(TCTI_FP_INT_FCVTMS_SIMD, "fcvtms");
	TCTI_NATIVE_SIMD_FP_CONVERT_CASE(TCTI_FP_INT_FCVTMU_SIMD, "fcvtmu");
	TCTI_NATIVE_SIMD_FP_CONVERT_CASE(TCTI_FP_INT_FCVTZS_SIMD, "fcvtzs");
	TCTI_NATIVE_SIMD_FP_CONVERT_CASE(TCTI_FP_INT_FCVTZU_SIMD, "fcvtzu");
	TCTI_NATIVE_SIMD_FP_CONVERT_CASE(TCTI_FP_INT_FCVTAS_SIMD, "fcvtas");
	TCTI_NATIVE_SIMD_FP_CONVERT_CASE(TCTI_FP_INT_FCVTAU_SIMD, "fcvtau");
	TCTI_NATIVE_SIMD_FP_CONVERT_CASE(TCTI_FP_INT_SCVTF_SIMD, "scvtf");
	TCTI_NATIVE_SIMD_FP_CONVERT_CASE(TCTI_FP_INT_UCVTF_SIMD, "ucvtf");
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

#undef TCTI_NATIVE_SIMD_FP_CONVERT_CASE
#undef TCTI_NATIVE_SIMD_FP_CONVERT_RUN

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

#define TCTI_NATIVE_SIMD_FP_ACCUMULATE_EXECUTE(asm_instruction) \
	({ \
		asm volatile("ldr q0, [%[left]]\n" \
			     "ldr q1, [%[right]]\n" \
			     "ldr q2, [%[accumulator]]\n" \
			     asm_instruction "\n" \
			     "str q2, [%[result]]\n" \
			     : \
			     : [result] "r" (result), [left] "r" (left), \
			       [right] "r" (right), \
			       [accumulator] "r" (accumulator) \
			     : "v0", "v1", "v2", "memory"); \
	})

#define TCTI_NATIVE_SIMD_FP_ACCUMULATE_CASE(operation, scalar_s, scalar_d, \
					    vector_2s, vector_4s, vector_2d) \
	case operation: \
		switch (shape) { \
		case TCTI_NATIVE_SIMD_FP_SCALAR_S: \
			TCTI_NATIVE_SIMD_FP_ACCUMULATE_EXECUTE(scalar_s); \
			break; \
		case TCTI_NATIVE_SIMD_FP_SCALAR_D: \
			TCTI_NATIVE_SIMD_FP_ACCUMULATE_EXECUTE(scalar_d); \
			break; \
		case TCTI_NATIVE_SIMD_FP_VECTOR_2S: \
			TCTI_NATIVE_SIMD_FP_ACCUMULATE_EXECUTE(vector_2s); \
			break; \
		case TCTI_NATIVE_SIMD_FP_VECTOR_4S: \
			TCTI_NATIVE_SIMD_FP_ACCUMULATE_EXECUTE(vector_4s); \
			break; \
		case TCTI_NATIVE_SIMD_FP_VECTOR_2D: \
			TCTI_NATIVE_SIMD_FP_ACCUMULATE_EXECUTE(vector_2d); \
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
		"fmla s2, s0, v1.s[0]", "fmla d2, d0, v1.d[0]",
		"fmla v2.2s, v0.2s, v1.2s", "fmla v2.4s, v0.4s, v1.4s",
		"fmla v2.2d, v0.2d, v1.2d");
	TCTI_NATIVE_SIMD_FP_ACCUMULATE_CASE(TCTI_SIMD_ARITH_FMLS,
		"fmls s2, s0, v1.s[0]", "fmls d2, d0, v1.d[0]",
		"fmls v2.2s, v0.2s, v1.2s", "fmls v2.4s, v0.4s, v1.4s",
		"fmls v2.2d, v0.2d, v1.2d");
	TCTI_NATIVE_SIMD_FP_CASE(TCTI_SIMD_ARITH_FMUL,
		"fmul s0, s0, s1", "fmul d0, d0, d1",
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

/*
 * FEAT_FP16 AdvSIMD three-same forms use H scalars, 4H vectors, or 8H
 * vectors.  Keep their native execution separate from the S/D helper above:
 * the decoder must validate the FP16 encoding and select this helper, while
 * this layer owns the architectural FPCR/FPSR transaction and register width.
 */
enum tcti_native_simd_fp16_shape {
	TCTI_NATIVE_SIMD_FP16_SCALAR_H,
	TCTI_NATIVE_SIMD_FP16_VECTOR_4H,
	TCTI_NATIVE_SIMD_FP16_VECTOR_8H,
};

#define TCTI_NATIVE_SIMD_FP16_RUN(instruction) \
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

#define TCTI_NATIVE_SIMD_FP16_ACCUMULATE_RUN(instruction) \
	({ \
		asm volatile("ldr q0, [%[left]]\n" \
			     "ldr q1, [%[right]]\n" \
			     "ldr q2, [%[accumulator]]\n" \
			     instruction "\n" \
			     "str q2, [%[result]]\n" \
				     : \
				     : [result] "r" (result), [left] "r" (left), \
				       [right] "r" (right), [accumulator] "r" (accumulator) \
				     : "v0", "v1", "v2", "memory"); \
	})

#define TCTI_NATIVE_SIMD_FP16_CASE(operation_value, scalar_instruction, \
					 vector_4h_instruction, vector_8h_instruction) \
	case operation_value: \
		switch (shape) { \
		case TCTI_NATIVE_SIMD_FP16_SCALAR_H: \
			TCTI_NATIVE_SIMD_FP16_RUN(scalar_instruction); \
			break; \
		case TCTI_NATIVE_SIMD_FP16_VECTOR_4H: \
			TCTI_NATIVE_SIMD_FP16_RUN(vector_4h_instruction); \
			break; \
		case TCTI_NATIVE_SIMD_FP16_VECTOR_8H: \
			TCTI_NATIVE_SIMD_FP16_RUN(vector_8h_instruction); \
			break; \
		} \
		break

#define TCTI_NATIVE_SIMD_FP16_ACCUMULATE_CASE(operation_value, \
						    scalar_instruction, \
						    vector_4h_instruction, \
						    vector_8h_instruction) \
	case operation_value: \
		switch (shape) { \
		case TCTI_NATIVE_SIMD_FP16_SCALAR_H: \
			TCTI_NATIVE_SIMD_FP16_ACCUMULATE_RUN(scalar_instruction); \
			break; \
		case TCTI_NATIVE_SIMD_FP16_VECTOR_4H: \
			TCTI_NATIVE_SIMD_FP16_ACCUMULATE_RUN(vector_4h_instruction); \
			break; \
		case TCTI_NATIVE_SIMD_FP16_VECTOR_8H: \
			TCTI_NATIVE_SIMD_FP16_ACCUMULATE_RUN(vector_8h_instruction); \
			break; \
		} \
		break

#define TCTI_NATIVE_SIMD_FP16_VECTOR_CASE(operation_value, \
					  vector_4h_instruction, vector_8h_instruction) \
	case operation_value: \
		if (shape == TCTI_NATIVE_SIMD_FP16_SCALAR_H) { \
			ret = -EINVAL; \
			break; \
		} \
		if (shape == TCTI_NATIVE_SIMD_FP16_VECTOR_4H) \
			TCTI_NATIVE_SIMD_FP16_RUN(vector_4h_instruction); \
		else \
			TCTI_NATIVE_SIMD_FP16_RUN(vector_8h_instruction); \
		break

int tcti_native_simd_fp16_three_same(
	enum tcti_simd_vector_arithmetic_op operation, bool scalar, bool q,
	u64 result[2], const u64 left[2], const u64 right[2],
	const u64 accumulator[2], unsigned long fpcr, unsigned long *fpsr)
{
	enum tcti_native_simd_fp16_shape shape;
	unsigned long host_fpcr;
	unsigned long host_fpsr;
	unsigned long guest_fpsr;
	int ret = 0;

	if (!result || !left || !right || !accumulator || !fpsr)
		return -EINVAL;
	if (scalar)
		shape = TCTI_NATIVE_SIMD_FP16_SCALAR_H;
	else if (q)
		shape = TCTI_NATIVE_SIMD_FP16_VECTOR_8H;
	else
		shape = TCTI_NATIVE_SIMD_FP16_VECTOR_4H;

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
	TCTI_NATIVE_SIMD_FP16_CASE(TCTI_SIMD_ARITH_FABD,
		"fabd h0, h0, h1", "fabd v0.4h, v0.4h, v1.4h",
		"fabd v0.8h, v0.8h, v1.8h");
	TCTI_NATIVE_SIMD_FP16_CASE(TCTI_SIMD_ARITH_FACGE,
		"facge h0, h0, h1", "facge v0.4h, v0.4h, v1.4h",
		"facge v0.8h, v0.8h, v1.8h");
	TCTI_NATIVE_SIMD_FP16_CASE(TCTI_SIMD_ARITH_FACGT,
		"facgt h0, h0, h1", "facgt v0.4h, v0.4h, v1.4h",
		"facgt v0.8h, v0.8h, v1.8h");
	TCTI_NATIVE_SIMD_FP16_CASE(TCTI_SIMD_ARITH_FCMEQ,
		"fcmeq h0, h0, h1", "fcmeq v0.4h, v0.4h, v1.4h",
		"fcmeq v0.8h, v0.8h, v1.8h");
	TCTI_NATIVE_SIMD_FP16_CASE(TCTI_SIMD_ARITH_FCMGE,
		"fcmge h0, h0, h1", "fcmge v0.4h, v0.4h, v1.4h",
		"fcmge v0.8h, v0.8h, v1.8h");
	TCTI_NATIVE_SIMD_FP16_CASE(TCTI_SIMD_ARITH_FCMGT,
		"fcmgt h0, h0, h1", "fcmgt v0.4h, v0.4h, v1.4h",
		"fcmgt v0.8h, v0.8h, v1.8h");
	TCTI_NATIVE_SIMD_FP16_CASE(TCTI_SIMD_ARITH_FMULX,
		"fmulx h0, h0, h1", "fmulx v0.4h, v0.4h, v1.4h",
		"fmulx v0.8h, v0.8h, v1.8h");
	TCTI_NATIVE_SIMD_FP16_CASE(TCTI_SIMD_ARITH_FRECPS,
		"frecps h0, h0, h1", "frecps v0.4h, v0.4h, v1.4h",
		"frecps v0.8h, v0.8h, v1.8h");
	TCTI_NATIVE_SIMD_FP16_CASE(TCTI_SIMD_ARITH_FRSQRTS,
		"frsqrts h0, h0, h1", "frsqrts v0.4h, v0.4h, v1.4h",
		"frsqrts v0.8h, v0.8h, v1.8h");
	TCTI_NATIVE_SIMD_FP16_VECTOR_CASE(TCTI_SIMD_ARITH_FADDP,
		"faddp v0.4h, v0.4h, v1.4h", "faddp v0.8h, v0.8h, v1.8h");
	TCTI_NATIVE_SIMD_FP16_CASE(TCTI_SIMD_ARITH_FADD,
		"fadd h0, h0, h1", "fadd v0.4h, v0.4h, v1.4h",
		"fadd v0.8h, v0.8h, v1.8h");
	TCTI_NATIVE_SIMD_FP16_CASE(TCTI_SIMD_ARITH_FDIV,
		"fdiv h0, h0, h1", "fdiv v0.4h, v0.4h, v1.4h",
		"fdiv v0.8h, v0.8h, v1.8h");
	TCTI_NATIVE_SIMD_FP16_CASE(TCTI_SIMD_ARITH_FMAXNM,
		"fmaxnm h0, h0, h1", "fmaxnm v0.4h, v0.4h, v1.4h",
		"fmaxnm v0.8h, v0.8h, v1.8h");
	TCTI_NATIVE_SIMD_FP16_VECTOR_CASE(TCTI_SIMD_ARITH_FMAXNMP,
		"fmaxnmp v0.4h, v0.4h, v1.4h",
		"fmaxnmp v0.8h, v0.8h, v1.8h");
	TCTI_NATIVE_SIMD_FP16_CASE(TCTI_SIMD_ARITH_FMAX,
		"fmax h0, h0, h1", "fmax v0.4h, v0.4h, v1.4h",
		"fmax v0.8h, v0.8h, v1.8h");
	TCTI_NATIVE_SIMD_FP16_VECTOR_CASE(TCTI_SIMD_ARITH_FMAXP,
		"fmaxp v0.4h, v0.4h, v1.4h", "fmaxp v0.8h, v0.8h, v1.8h");
	TCTI_NATIVE_SIMD_FP16_CASE(TCTI_SIMD_ARITH_FMINNM,
		"fminnm h0, h0, h1", "fminnm v0.4h, v0.4h, v1.4h",
		"fminnm v0.8h, v0.8h, v1.8h");
	TCTI_NATIVE_SIMD_FP16_VECTOR_CASE(TCTI_SIMD_ARITH_FMINNMP,
		"fminnmp v0.4h, v0.4h, v1.4h",
		"fminnmp v0.8h, v0.8h, v1.8h");
	TCTI_NATIVE_SIMD_FP16_CASE(TCTI_SIMD_ARITH_FMIN,
		"fmin h0, h0, h1", "fmin v0.4h, v0.4h, v1.4h",
		"fmin v0.8h, v0.8h, v1.8h");
	TCTI_NATIVE_SIMD_FP16_VECTOR_CASE(TCTI_SIMD_ARITH_FMINP,
		"fminp v0.4h, v0.4h, v1.4h", "fminp v0.8h, v0.8h, v1.8h");
	TCTI_NATIVE_SIMD_FP16_ACCUMULATE_CASE(TCTI_SIMD_ARITH_FMLA,
		"fmla h2, h0, v1.h[0]", "fmla v2.4h, v0.4h, v1.4h",
		"fmla v2.8h, v0.8h, v1.8h");
	TCTI_NATIVE_SIMD_FP16_ACCUMULATE_CASE(TCTI_SIMD_ARITH_FMLS,
		"fmls h2, h0, v1.h[0]", "fmls v2.4h, v0.4h, v1.4h",
		"fmls v2.8h, v0.8h, v1.8h");
	TCTI_NATIVE_SIMD_FP16_CASE(TCTI_SIMD_ARITH_FMUL,
		"fmul h0, h0, h1", "fmul v0.4h, v0.4h, v1.4h",
		"fmul v0.8h, v0.8h, v1.8h");
	TCTI_NATIVE_SIMD_FP16_CASE(TCTI_SIMD_ARITH_FSUB,
		"fsub h0, h0, h1", "fsub v0.4h, v0.4h, v1.4h",
		"fsub v0.8h, v0.8h, v1.8h");
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

#undef TCTI_NATIVE_SIMD_FP16_ACCUMULATE_CASE
#undef TCTI_NATIVE_SIMD_FP16_VECTOR_CASE
#undef TCTI_NATIVE_SIMD_FP16_CASE
#undef TCTI_NATIVE_SIMD_FP16_ACCUMULATE_RUN
#undef TCTI_NATIVE_SIMD_FP16_RUN

#define TCTI_NATIVE_SIMD_FP_SCALAR_UNARY_RUN(instruction) \
	({ \
		asm volatile("ldr q0, [%[source]]\n" \
			     instruction "\n" \
			     "str q0, [%[result]]\n" \
			     : \
			     : [result] "r" (result), [source] "r" (source) \
			     : "v0", "memory"); \
	})

int tcti_native_simd_fp_scalar_unary(
	enum tcti_simd_vector_arithmetic_op operation, u8 access_size,
	u8 result_size, u64 result[2], const u64 source[2], unsigned long fpcr,
	unsigned long *fpsr)
{
	unsigned long host_fpcr;
	unsigned long host_fpsr;
	unsigned long guest_fpsr;
	int ret = 0;

	if (!result || !source || !fpsr)
		return -EINVAL;
	if (operation == TCTI_SIMD_ARITH_FCVTXN) {
		if (access_size != sizeof(u64) || result_size != sizeof(u32))
			return -EINVAL;
	} else if ((access_size != sizeof(u32) &&
		    access_size != sizeof(u64)) || result_size != access_size) {
		return -EINVAL;
	}

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
	case TCTI_SIMD_ARITH_FRECPE:
		if (access_size == sizeof(u32))
			TCTI_NATIVE_SIMD_FP_SCALAR_UNARY_RUN("frecpe s0, s0");
		else
			TCTI_NATIVE_SIMD_FP_SCALAR_UNARY_RUN("frecpe d0, d0");
		break;
	case TCTI_SIMD_ARITH_FRECPX:
		if (access_size == sizeof(u32))
			TCTI_NATIVE_SIMD_FP_SCALAR_UNARY_RUN("frecpx s0, s0");
		else
			TCTI_NATIVE_SIMD_FP_SCALAR_UNARY_RUN("frecpx d0, d0");
		break;
	case TCTI_SIMD_ARITH_FRSQRTE:
		if (access_size == sizeof(u32))
			TCTI_NATIVE_SIMD_FP_SCALAR_UNARY_RUN("frsqrte s0, s0");
		else
			TCTI_NATIVE_SIMD_FP_SCALAR_UNARY_RUN("frsqrte d0, d0");
		break;
	case TCTI_SIMD_ARITH_FCMEQ_ZERO:
		if (access_size == sizeof(u32))
			TCTI_NATIVE_SIMD_FP_SCALAR_UNARY_RUN("fcmeq s0, s0, #0.0");
		else
			TCTI_NATIVE_SIMD_FP_SCALAR_UNARY_RUN("fcmeq d0, d0, #0.0");
		break;
	case TCTI_SIMD_ARITH_FCMGE_ZERO:
		if (access_size == sizeof(u32))
			TCTI_NATIVE_SIMD_FP_SCALAR_UNARY_RUN("fcmge s0, s0, #0.0");
		else
			TCTI_NATIVE_SIMD_FP_SCALAR_UNARY_RUN("fcmge d0, d0, #0.0");
		break;
	case TCTI_SIMD_ARITH_FCMGT_ZERO:
		if (access_size == sizeof(u32))
			TCTI_NATIVE_SIMD_FP_SCALAR_UNARY_RUN("fcmgt s0, s0, #0.0");
		else
			TCTI_NATIVE_SIMD_FP_SCALAR_UNARY_RUN("fcmgt d0, d0, #0.0");
		break;
	case TCTI_SIMD_ARITH_FCMLE_ZERO:
		if (access_size == sizeof(u32))
			TCTI_NATIVE_SIMD_FP_SCALAR_UNARY_RUN("fcmle s0, s0, #0.0");
		else
			TCTI_NATIVE_SIMD_FP_SCALAR_UNARY_RUN("fcmle d0, d0, #0.0");
		break;
	case TCTI_SIMD_ARITH_FCMLT_ZERO:
		if (access_size == sizeof(u32))
			TCTI_NATIVE_SIMD_FP_SCALAR_UNARY_RUN("fcmlt s0, s0, #0.0");
		else
			TCTI_NATIVE_SIMD_FP_SCALAR_UNARY_RUN("fcmlt d0, d0, #0.0");
		break;
	case TCTI_SIMD_ARITH_FCVTXN:
		TCTI_NATIVE_SIMD_FP_SCALAR_UNARY_RUN("fcvtxn s0, d0");
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

#undef TCTI_NATIVE_SIMD_FP_SCALAR_UNARY_RUN

#define TCTI_NATIVE_SIMD_FP_TWO_REGISTER_RUN0(instruction) \
	({ \
		asm volatile("ldr q0, [%[source]]\n" \
			 instruction "\n" \
			 "str q0, [%[result]]\n" \
			 : \
			 : [result] "r" (result), [source] "r" (source) \
			 : "v0", "memory"); \
	})

#define TCTI_NATIVE_SIMD_FP_TWO_REGISTER_RUN1(instruction) \
	({ \
		asm volatile("ldr q0, [%[source]]\n" \
			 "ldr q1, [%[accumulator]]\n" \
			 instruction "\n" \
			 "str q1, [%[result]]\n" \
			 : \
			 : [result] "r" (result), [source] "r" (source), \
			   [accumulator] "r" (accumulator) \
			 : "v0", "v1", "memory"); \
	})

#define TCTI_NATIVE_SIMD_FP_TWO_REGISTER_CASE(operation_value, s2, s4, d2) \
	case operation_value: \
		if (access_size == sizeof(u32) && result_size == sizeof(u64)) \
			TCTI_NATIVE_SIMD_FP_TWO_REGISTER_RUN0(s2); \
		else if (access_size == sizeof(u32)) \
			TCTI_NATIVE_SIMD_FP_TWO_REGISTER_RUN0(s4); \
		else \
			TCTI_NATIVE_SIMD_FP_TWO_REGISTER_RUN0(d2); \
		break

int tcti_native_simd_fp_two_register(
	enum tcti_simd_vector_arithmetic_op operation, u8 access_size,
	u8 result_size, u8 source_index, u8 destination_index, u64 result[2],
	const u64 source[2], const u64 accumulator[2], unsigned long fpcr,
	unsigned long *fpsr)
{
	unsigned long host_fpcr;
	unsigned long host_fpsr;
	unsigned long guest_fpsr;
	bool convert = operation == TCTI_SIMD_ARITH_FCVTN ||
		operation == TCTI_SIMD_ARITH_FCVTL ||
		operation == TCTI_SIMD_ARITH_FCVTXN;
	bool uint_estimate = operation == TCTI_SIMD_ARITH_URECPE ||
		operation == TCTI_SIMD_ARITH_URSQRTE;
	int ret = 0;

	if (!result || !source || !accumulator || !fpsr)
		return -EINVAL;
	if (uint_estimate && access_size != sizeof(u32))
		return -EINVAL;
	if (!convert && ((access_size == sizeof(u32) &&
		(result_size == sizeof(u64) || result_size == 2 * sizeof(u64))) ||
		(access_size == sizeof(u64) && result_size == 2 * sizeof(u64)))) {
		if (source_index || destination_index)
			return -EINVAL;
	} else if (operation == TCTI_SIMD_ARITH_FCVTN ||
		   operation == TCTI_SIMD_ARITH_FCVTXN) {
		if (access_size != sizeof(u64) || source_index ||
		    (destination_index ? result_size != 2 * sizeof(u64) :
		     result_size != sizeof(u64)))
			return -EINVAL;
	} else if (operation == TCTI_SIMD_ARITH_FCVTL) {
		if (access_size != sizeof(u32) || result_size != 2 * sizeof(u64) ||
		    destination_index || source_index > 1)
			return -EINVAL;
	} else {
		return -EINVAL;
	}

	preempt_disable();
	asm volatile("mrs %0, fpcr\n"
		     "mrs %1, fpsr\n"
		     : "=r" (host_fpcr), "=r" (host_fpsr));
	asm volatile("msr fpcr, %0\n"
		     "msr fpsr, %1\n"
		     "isb\n"
		     : : "r" (fpcr), "r" (*fpsr) : "memory");

	switch (operation) {
	TCTI_NATIVE_SIMD_FP_TWO_REGISTER_CASE(TCTI_SIMD_ARITH_FABS,
		"fabs v0.2s, v0.2s", "fabs v0.4s, v0.4s",
		"fabs v0.2d, v0.2d");
	TCTI_NATIVE_SIMD_FP_TWO_REGISTER_CASE(TCTI_SIMD_ARITH_FNEG,
		"fneg v0.2s, v0.2s", "fneg v0.4s, v0.4s",
		"fneg v0.2d, v0.2d");
	TCTI_NATIVE_SIMD_FP_TWO_REGISTER_CASE(TCTI_SIMD_ARITH_FSQRT,
		"fsqrt v0.2s, v0.2s", "fsqrt v0.4s, v0.4s",
		"fsqrt v0.2d, v0.2d");
	TCTI_NATIVE_SIMD_FP_TWO_REGISTER_CASE(TCTI_SIMD_ARITH_FRECPE,
		"frecpe v0.2s, v0.2s", "frecpe v0.4s, v0.4s",
		"frecpe v0.2d, v0.2d");
	TCTI_NATIVE_SIMD_FP_TWO_REGISTER_CASE(TCTI_SIMD_ARITH_FRSQRTE,
		"frsqrte v0.2s, v0.2s", "frsqrte v0.4s, v0.4s",
		"frsqrte v0.2d, v0.2d");
	TCTI_NATIVE_SIMD_FP_TWO_REGISTER_CASE(TCTI_SIMD_ARITH_FRINTN,
		"frintn v0.2s, v0.2s", "frintn v0.4s, v0.4s",
		"frintn v0.2d, v0.2d");
	TCTI_NATIVE_SIMD_FP_TWO_REGISTER_CASE(TCTI_SIMD_ARITH_FRINTP,
		"frintp v0.2s, v0.2s", "frintp v0.4s, v0.4s",
		"frintp v0.2d, v0.2d");
	TCTI_NATIVE_SIMD_FP_TWO_REGISTER_CASE(TCTI_SIMD_ARITH_FRINTM,
		"frintm v0.2s, v0.2s", "frintm v0.4s, v0.4s",
		"frintm v0.2d, v0.2d");
	TCTI_NATIVE_SIMD_FP_TWO_REGISTER_CASE(TCTI_SIMD_ARITH_FRINTZ,
		"frintz v0.2s, v0.2s", "frintz v0.4s, v0.4s",
		"frintz v0.2d, v0.2d");
	TCTI_NATIVE_SIMD_FP_TWO_REGISTER_CASE(TCTI_SIMD_ARITH_FRINTA,
		"frinta v0.2s, v0.2s", "frinta v0.4s, v0.4s",
		"frinta v0.2d, v0.2d");
	TCTI_NATIVE_SIMD_FP_TWO_REGISTER_CASE(TCTI_SIMD_ARITH_FRINTX,
		"frintx v0.2s, v0.2s", "frintx v0.4s, v0.4s",
		"frintx v0.2d, v0.2d");
	TCTI_NATIVE_SIMD_FP_TWO_REGISTER_CASE(TCTI_SIMD_ARITH_FRINTI,
		"frinti v0.2s, v0.2s", "frinti v0.4s, v0.4s",
		"frinti v0.2d, v0.2d");
	TCTI_NATIVE_SIMD_FP_TWO_REGISTER_CASE(TCTI_SIMD_ARITH_FCMEQ_ZERO,
		"fcmeq v0.2s, v0.2s, #0.0", "fcmeq v0.4s, v0.4s, #0.0",
		"fcmeq v0.2d, v0.2d, #0.0");
	TCTI_NATIVE_SIMD_FP_TWO_REGISTER_CASE(TCTI_SIMD_ARITH_FCMGE_ZERO,
		"fcmge v0.2s, v0.2s, #0.0", "fcmge v0.4s, v0.4s, #0.0",
		"fcmge v0.2d, v0.2d, #0.0");
	TCTI_NATIVE_SIMD_FP_TWO_REGISTER_CASE(TCTI_SIMD_ARITH_FCMGT_ZERO,
		"fcmgt v0.2s, v0.2s, #0.0", "fcmgt v0.4s, v0.4s, #0.0",
		"fcmgt v0.2d, v0.2d, #0.0");
	TCTI_NATIVE_SIMD_FP_TWO_REGISTER_CASE(TCTI_SIMD_ARITH_FCMLE_ZERO,
		"fcmle v0.2s, v0.2s, #0.0", "fcmle v0.4s, v0.4s, #0.0",
		"fcmle v0.2d, v0.2d, #0.0");
	TCTI_NATIVE_SIMD_FP_TWO_REGISTER_CASE(TCTI_SIMD_ARITH_FCMLT_ZERO,
		"fcmlt v0.2s, v0.2s, #0.0", "fcmlt v0.4s, v0.4s, #0.0",
		"fcmlt v0.2d, v0.2d, #0.0");
	TCTI_NATIVE_SIMD_FP_TWO_REGISTER_CASE(TCTI_SIMD_ARITH_URECPE,
		"urecpe v0.2s, v0.2s", "urecpe v0.4s, v0.4s", "");
	TCTI_NATIVE_SIMD_FP_TWO_REGISTER_CASE(TCTI_SIMD_ARITH_URSQRTE,
		"ursqrte v0.2s, v0.2s", "ursqrte v0.4s, v0.4s", "");
	case TCTI_SIMD_ARITH_FCVTN:
		if (destination_index)
			TCTI_NATIVE_SIMD_FP_TWO_REGISTER_RUN1(
				"fcvtn2 v1.4s, v0.2d");
		else
			TCTI_NATIVE_SIMD_FP_TWO_REGISTER_RUN0(
				"fcvtn v0.2s, v0.2d");
		break;
	case TCTI_SIMD_ARITH_FCVTXN:
		if (destination_index)
			TCTI_NATIVE_SIMD_FP_TWO_REGISTER_RUN1(
				"fcvtxn2 v1.4s, v0.2d");
		else
			TCTI_NATIVE_SIMD_FP_TWO_REGISTER_RUN0(
				"fcvtxn v0.2s, v0.2d");
		break;
	case TCTI_SIMD_ARITH_FCVTL:
		if (source_index)
			TCTI_NATIVE_SIMD_FP_TWO_REGISTER_RUN0(
				"fcvtl2 v0.2d, v0.4s");
		else
			TCTI_NATIVE_SIMD_FP_TWO_REGISTER_RUN0(
				"fcvtl v0.2d, v0.2s");
		break;
	default:
		ret = -EINVAL;
		break;
	}

	asm volatile("mrs %0, fpsr\n" : "=r" (guest_fpsr));
	asm volatile("msr fpcr, %0\n"
		     "msr fpsr, %1\n"
		     "isb\n"
		     : : "r" (host_fpcr), "r" (host_fpsr) : "memory");
	*fpsr = guest_fpsr;
	preempt_enable();
	return ret;
}

#undef TCTI_NATIVE_SIMD_FP_TWO_REGISTER_CASE
#undef TCTI_NATIVE_SIMD_FP_TWO_REGISTER_RUN1
#undef TCTI_NATIVE_SIMD_FP_TWO_REGISTER_RUN0

#define TCTI_NATIVE_SIMD_FP_REDUCTION_RUN(instruction) \
	({ \
		asm volatile("ldr q0, [%[source]]\n" \
			     instruction "\n" \
			     "str q0, [%[result]]\n" \
			     : \
			     : [result] "r" (result), [source] "r" (source) \
			     : "v0", "memory"); \
	})

int tcti_native_simd_fp_reduction(
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
	if (operation >= TCTI_SIMD_REDUCTION_FMAXNMV &&
	    access_size != sizeof(u32))
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
			TCTI_NATIVE_SIMD_FP_REDUCTION_RUN("faddp s0, v0.2s");
		else
			TCTI_NATIVE_SIMD_FP_REDUCTION_RUN("faddp d0, v0.2d");
		break;
	case TCTI_SIMD_REDUCTION_FMAXNMP:
		if (access_size == sizeof(u32))
			TCTI_NATIVE_SIMD_FP_REDUCTION_RUN("fmaxnmp s0, v0.2s");
		else
			TCTI_NATIVE_SIMD_FP_REDUCTION_RUN("fmaxnmp d0, v0.2d");
		break;
	case TCTI_SIMD_REDUCTION_FMAXP:
		if (access_size == sizeof(u32))
			TCTI_NATIVE_SIMD_FP_REDUCTION_RUN("fmaxp s0, v0.2s");
		else
			TCTI_NATIVE_SIMD_FP_REDUCTION_RUN("fmaxp d0, v0.2d");
		break;
	case TCTI_SIMD_REDUCTION_FMINNMP:
		if (access_size == sizeof(u32))
			TCTI_NATIVE_SIMD_FP_REDUCTION_RUN("fminnmp s0, v0.2s");
		else
			TCTI_NATIVE_SIMD_FP_REDUCTION_RUN("fminnmp d0, v0.2d");
		break;
	case TCTI_SIMD_REDUCTION_FMINP:
		if (access_size == sizeof(u32))
			TCTI_NATIVE_SIMD_FP_REDUCTION_RUN("fminp s0, v0.2s");
		else
			TCTI_NATIVE_SIMD_FP_REDUCTION_RUN("fminp d0, v0.2d");
		break;
	case TCTI_SIMD_REDUCTION_FMAXNMV:
		TCTI_NATIVE_SIMD_FP_REDUCTION_RUN("fmaxnmv s0, v0.4s");
		break;
	case TCTI_SIMD_REDUCTION_FMAXV:
		TCTI_NATIVE_SIMD_FP_REDUCTION_RUN("fmaxv s0, v0.4s");
		break;
	case TCTI_SIMD_REDUCTION_FMINNMV:
		TCTI_NATIVE_SIMD_FP_REDUCTION_RUN("fminnmv s0, v0.4s");
		break;
	case TCTI_SIMD_REDUCTION_FMINV:
		TCTI_NATIVE_SIMD_FP_REDUCTION_RUN("fminv s0, v0.4s");
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

#undef TCTI_NATIVE_SIMD_FP_REDUCTION_RUN
