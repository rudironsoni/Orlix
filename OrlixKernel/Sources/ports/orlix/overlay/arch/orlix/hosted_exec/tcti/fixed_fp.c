// SPDX-License-Identifier: GPL-2.0-only
#include <linux/types.h>

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
