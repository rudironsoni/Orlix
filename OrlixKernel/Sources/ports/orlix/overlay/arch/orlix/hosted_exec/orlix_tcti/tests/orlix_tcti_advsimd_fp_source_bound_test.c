// SPDX-License-Identifier: GPL-2.0-only
/*
 * Production-path proof for the 113 always-on EL0 ADVSIMD_FP leaves.
 * Leftover CORE FP arithmetic stays regression and does not receive #120
 * close credit. The 155 optional FP16, FHM, BF16, FCMA, FRINTTS, FP8,
 * FAMINMAX, and FSCALE leaves remain in the EL0 complete target as
 * unclassified blockers. Runtime rejection does not complete them.
 * Those features are not advertised.
 */
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/compiler.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/preempt.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/string.h>
#include <linux/syscalls.h>

#include "../decode_aarch64.h"
#include "../switch_debug.h"
#include "orlix_tcti_memory_proof.h"
#include "orlix_tcti_native_observation.h"
#include "orlix_tcti_advsimd_fp_production_capture.h"
#include "target_native_proof_contract_private.h"
#include "target_proof_ingestion_private.h"

#define FP_SVC 0xd4000001U
#define FP_FAMILY_COUNT 268U
#define FP_EL0_COUNT 113U
#define FP_NON_EL0_COUNT 155U
#define FP_RD 0U
#define FP_RN 1U
#define FP_RM 2U
#define AARCH64_FPSR_IOC BIT(0)

enum orlix_tcti_test_source_family {
#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_SOURCE(...)
#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_COUNTS(...)
#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_MEMBER(...)
#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_FAMILY(symbol, ...) \
	ORLIX_TCTI_TEST_SOURCE_FAMILY_##symbol,
#include "../isa/target_execution_slice_map.def"
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_SOURCE
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_COUNTS
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_MEMBER
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_FAMILY
};

static const u8 orlix_tcti_test_source_families[] = {
#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_SOURCE(...)
#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_COUNTS(...)
#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_FAMILY(...)
#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_MEMBER(i, name, condition, family) \
	[i] = ORLIX_TCTI_TEST_SOURCE_FAMILY_##family,
#include "../isa/target_execution_slice_map.def"
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_SOURCE
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_COUNTS
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_FAMILY
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_MEMBER
};

struct orlix_tcti_test_fp_source {
	u32 ordinal;
	const char *name;
	const char *mnemonic;
	const char *operation;
	u32 mask;
	u32 pattern;
	const char *condition_tcnd_hex;
};

#define ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(i, name, mnemonic, operation, mask, \
	pattern, condition, ...) { i, name, mnemonic, operation, mask, pattern, condition },
static const struct orlix_tcti_test_fp_source orlix_tcti_test_sources[] = {
#include "../isa/source_manifest.def"
};
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW

static const bool orlix_tcti_test_has_ddi0602_semantics[] = {
#define ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_SOURCE(...)
#define ORLIX_TCTI_A64_DDI0602_PROVENANCE_ROW(i, ...) [i] = true,
#define ORLIX_TCTI_A64_OFFICIAL_SEMANTICS_NOT_SPECIFIED_ROW(i, ...) [i] = false,
#include "../isa/target_asl_availability.def"
#undef ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_SOURCE
#undef ORLIX_TCTI_A64_DDI0602_PROVENANCE_ROW
#undef ORLIX_TCTI_A64_OFFICIAL_SEMANTICS_NOT_SPECIFIED_ROW
};

static bool orlix_tcti_test_is_advsimd_fp(
	const struct orlix_tcti_test_fp_source *source)
{
	return source->ordinal < ARRAY_SIZE(orlix_tcti_test_source_families) &&
		orlix_tcti_test_source_families[source->ordinal] ==
			ORLIX_TCTI_TEST_SOURCE_FAMILY_ADVSIMD_FP;
}

static bool orlix_tcti_test_is_optional_fp_feature(
	const struct orlix_tcti_test_fp_source *source)
{
	const char *name = source->name;
	const char *op = source->operation;

	if (strstr(name, "fp16") || strstr(name, "FP16") ||
	    strstr(name, "_H") || strstr(name, "only_H") ||
	    strstr(name, "_RH_H") || strstr(name, "_H_h"))
		return true;
	if (!strncmp(op, "FMLAL", 5) || !strncmp(op, "FMLSL", 5))
		return true;
	if (!strncmp(op, "FDOT", 4) || !strncmp(op, "FMMLA", 5) ||
	    !strncmp(op, "BF", 2))
		return true;
	if (strstr(name, "FRINT32") || strstr(name, "FRINT64"))
		return true;
	if (!strncmp(op, "FCMLA", 5) || !strncmp(op, "FCADD", 5))
		return true;
	if (strstr(name, "F1CVTL") || strstr(name, "F2CVTL") ||
	    strstr(name, "FCVTN_asimdsame2"))
		return true;
	if (!strncmp(op, "FAMAX", 5) || !strncmp(op, "FAMIN", 5) ||
	    !strncmp(op, "FSCALE", 6))
		return true;
	return false;
}

static bool orlix_tcti_test_is_el0_fp(
	const struct orlix_tcti_test_fp_source *source)
{
	return orlix_tcti_test_is_advsimd_fp(source) &&
		!orlix_tcti_test_is_optional_fp_feature(source);
}

static const struct orlix_tcti_test_fp_source *
orlix_tcti_test_fp_name(const char *name)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++)
		if (orlix_tcti_test_is_advsimd_fp(&orlix_tcti_test_sources[index]) &&
		    !strcmp(orlix_tcti_test_sources[index].name, name))
			return &orlix_tcti_test_sources[index];
	return NULL;
}

static u32 orlix_tcti_advsimd_fp_legal_instruction(
	const struct orlix_tcti_test_fp_source *source)
{
	u32 instruction = source->pattern & source->mask;

	/* asimdimm bits 9:5 are imm8, not Rn. */
	if (strstr(source->name, "asimdimm"))
		instruction |= FP_RD;
	else
		instruction |= (FP_RN << 5) | FP_RD;
	if ((source->mask & BIT(30)) == 0 && !(source->pattern & BIT(28)))
		instruction |= BIT(30);
	if ((strstr(source->name, "asimdshf") ||
	     strstr(source->name, "asisdshf")) &&
	    ((instruction >> 19) & 0xfU) == 0) {
		if (strstr(source->name, "asisdshf"))
			instruction |= 65U << 16;
		else
			instruction |= BIT(21);
	} else if ((source->mask & (0x1fU << 16)) == 0)
		instruction |= FP_RM << 16;
	if (strstr(source->name, "asimdelem") &&
	    ((instruction >> 21) & 1U) == 0 && (source->mask & BIT(21)) == 0)
		instruction |= BIT(21);
	return instruction;
}

static bool orlix_tcti_test_fp_decode_is_family(u32 decode_class)
{
	return decode_class == ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC ||
		decode_class == ORLIX_TCTI_DECODE_SIMD_VECTOR_COMPARE ||
		decode_class == ORLIX_TCTI_DECODE_SIMD_VECTOR_REDUCTION ||
		decode_class == ORLIX_TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE ||
		decode_class == ORLIX_TCTI_DECODE_FP_INT_CONVERT ||
		decode_class == ORLIX_TCTI_DECODE_FP_SCALAR_1SOURCE ||
		decode_class == ORLIX_TCTI_DECODE_FP_SCALAR_2SOURCE ||
		decode_class == ORLIX_TCTI_DECODE_FP_SCALAR_COMPARE;
}

static int orlix_tcti_advsimd_fp_test_init(struct kunit *test)
{
	struct mm_struct *mm = mm_alloc();

	if (!mm)
		return -ENOMEM;
	kthread_use_mm(mm);
	test->priv = mm;
	return 0;
}

static void orlix_tcti_advsimd_fp_test_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (!mm)
		return;
	kthread_unuse_mm(mm);
	mmput(mm);
}

static unsigned long orlix_tcti_advsimd_fp_map(struct kunit *test, u32 instruction)
{
	u8 bytes[16] = {};
	u32 program[2] = { instruction, FP_SVC };

	memcpy(bytes, program, sizeof(program));
	return orlix_tcti_memory_proof_map_bytes(test, bytes, sizeof(bytes),
						 PROT_READ | PROT_EXEC);
}

static u8 orlix_tcti_advsimd_fp_imm8(u32 instruction)
{
	return (((instruction >> 16) & 0x7U) << 5) | ((instruction >> 5) & 0x1fU);
}

static u32 orlix_tcti_advsimd_fp_expand_fp32_imm(u8 imm8)
{
	u32 sign = (imm8 >> 7) & 1U;
	u32 exponent_bit = (imm8 >> 6) & 1U;
	u32 fraction = imm8 & 0x3fU;

	return (sign << 31) | ((!exponent_bit) << 30) |
		((exponent_bit ? 0x1fU : 0) << 25) | (fraction << 19);
}

static u64 orlix_tcti_advsimd_fp_expand_fp64_imm(u8 imm8)
{
	u64 sign = (imm8 >> 7) & 1U;
	u64 exponent_bit = (imm8 >> 6) & 1U;
	u64 fraction = imm8 & 0x3fU;

	return (sign << 63) | ((u64)!exponent_bit << 62) |
		((exponent_bit ? 0xffULL : 0) << 54) | (fraction << 48);
}

static void orlix_tcti_advsimd_fp_broadcast_lane(u64 dest[2], const u64 src[2],
						 u8 lane_bytes, u8 index,
						 u8 result_bytes)
{
	u64 mask = lane_bytes == 8 ? ~0ULL : (BIT_ULL(lane_bytes * 8) - 1);
	u8 src_byte = index * lane_bytes;
	u64 lane = (src[src_byte / 8U] >> ((src_byte % 8U) * 8U)) & mask;
	u8 lane_count = result_bytes / lane_bytes;
	u8 i;

	dest[0] = 0;
	dest[1] = 0;
	for (i = 0; i < lane_count; i++) {
		u8 byte = i * lane_bytes;

		dest[byte / 8U] |= lane << ((byte % 8U) * 8U);
	}
}

#define ORLIX_TCTI_ADV_FP_HOST_UN(insn) \
	asm volatile("ldr q0, [%[source]]\n" insn "\n str q0, [%[result]]\n" \
		     : : [result] "r" (result), [source] "r" (source) \
		     : "v0", "memory")
#define ORLIX_TCTI_ADV_FP_HOST_BIN(insn) \
	asm volatile("ldr q0, [%[left]]\n ldr q1, [%[right]]\n" insn \
		     "\n str q0, [%[result]]\n" \
		     : : [result] "r" (result), [left] "r" (left), \
			 [right] "r" (right) : "v0", "v1", "memory")
#define ORLIX_TCTI_ADV_FP_HOST_ACC(insn) \
	asm volatile("ldr q0, [%[left]]\n ldr q1, [%[right]]\n" \
		     "ldr q2, [%[acc]]\n" insn "\n str q2, [%[result]]\n" \
		     : : [result] "r" (result), [left] "r" (left), \
			 [right] "r" (right), [acc] "r" (acc) \
		     : "v0", "v1", "v2", "memory")

static int orlix_tcti_advsimd_fp_host_wrap(int (*body)(void *), void *ctx,
					  unsigned long fpcr,
					  unsigned long *fpsr)
{
	unsigned long host_fpcr;
	unsigned long host_fpsr;
	unsigned long guest_fpsr;
	int ret;

	if (!fpsr)
		return -EINVAL;
	preempt_disable();
	asm volatile("mrs %0, fpcr\n mrs %1, fpsr\n"
		     : "=r" (host_fpcr), "=r" (host_fpsr));
	asm volatile("msr fpcr, %0\n msr fpsr, %1\n isb\n"
		     : : "r" (fpcr), "r" (*fpsr) : "memory");
	ret = body(ctx);
	asm volatile("mrs %0, fpsr\n" : "=r" (guest_fpsr));
	asm volatile("msr fpcr, %0\n msr fpsr, %1\n isb\n"
		     : : "r" (host_fpcr), "r" (host_fpsr) : "memory");
	*fpsr = guest_fpsr;
	preempt_enable();
	return ret;
}

struct orlix_tcti_advsimd_fp_host_bin_ctx {
	const char *mnemonic;
	const char *name;
	bool scalar;
	u8 access;
	u8 result;
	u64 *result_reg;
	const u64 *left;
	const u64 *right;
	const u64 *acc;
};

static int orlix_tcti_advsimd_fp_host_bin_body(void *opaque)
{
	struct orlix_tcti_advsimd_fp_host_bin_ctx *c = opaque;
	const char *m = c->mnemonic;
	u64 *result = c->result_reg;
	const u64 *left = c->left;
	const u64 *right = c->right;
	const u64 *acc = c->acc;
	bool s = c->scalar && c->access == sizeof(u32);
	bool d = c->scalar && c->access == sizeof(u64);
	bool v2s = !c->scalar && c->access == sizeof(u32) &&
		c->result == sizeof(u64);
	bool v4s = !c->scalar && c->access == sizeof(u32) &&
		c->result == 2 * sizeof(u64);
	bool v2d = !c->scalar && c->access == sizeof(u64) &&
		c->result == 2 * sizeof(u64);
	bool accu = !strcmp(m, "FMLA") || !strcmp(m, "FMLS");
	bool zero = strstr(c->name, "_FZ") != NULL;

	if (!s && !d && !v2s && !v4s && !v2d)
		return -EINVAL;
	if (accu) {
		if (!strcmp(m, "FMLA")) {
			if (s)
				ORLIX_TCTI_ADV_FP_HOST_ACC("fmla s2, s0, v1.s[0]");
			else if (d)
				ORLIX_TCTI_ADV_FP_HOST_ACC("fmla d2, d0, v1.d[0]");
			else if (v2s)
				ORLIX_TCTI_ADV_FP_HOST_ACC("fmla v2.2s, v0.2s, v1.2s");
			else if (v4s)
				ORLIX_TCTI_ADV_FP_HOST_ACC("fmla v2.4s, v0.4s, v1.4s");
			else
				ORLIX_TCTI_ADV_FP_HOST_ACC("fmla v2.2d, v0.2d, v1.2d");
			return 0;
		}
		if (s)
			ORLIX_TCTI_ADV_FP_HOST_ACC("fmls s2, s0, v1.s[0]");
		else if (d)
			ORLIX_TCTI_ADV_FP_HOST_ACC("fmls d2, d0, v1.d[0]");
		else if (v2s)
			ORLIX_TCTI_ADV_FP_HOST_ACC("fmls v2.2s, v0.2s, v1.2s");
		else if (v4s)
			ORLIX_TCTI_ADV_FP_HOST_ACC("fmls v2.4s, v0.4s, v1.4s");
		else
			ORLIX_TCTI_ADV_FP_HOST_ACC("fmls v2.2d, v0.2d, v1.2d");
		return 0;
	}
#define ORLIX_TCTI_ADV_FP_HOST_BIN5(op) \
	do { \
		if (s) \
			ORLIX_TCTI_ADV_FP_HOST_BIN(op " s0, s0, s1"); \
		else if (d) \
			ORLIX_TCTI_ADV_FP_HOST_BIN(op " d0, d0, d1"); \
		else if (v2s) \
			ORLIX_TCTI_ADV_FP_HOST_BIN(op " v0.2s, v0.2s, v1.2s"); \
		else if (v4s) \
			ORLIX_TCTI_ADV_FP_HOST_BIN(op " v0.4s, v0.4s, v1.4s"); \
		else \
			ORLIX_TCTI_ADV_FP_HOST_BIN(op " v0.2d, v0.2d, v1.2d"); \
	} while (0)
	if (!strcmp(m, "FADD"))
		ORLIX_TCTI_ADV_FP_HOST_BIN5("fadd");
	else if (!strcmp(m, "FSUB"))
		ORLIX_TCTI_ADV_FP_HOST_BIN5("fsub");
	else if (!strcmp(m, "FMUL"))
		ORLIX_TCTI_ADV_FP_HOST_BIN5("fmul");
	else if (!strcmp(m, "FDIV"))
		ORLIX_TCTI_ADV_FP_HOST_BIN5("fdiv");
	else if (!strcmp(m, "FMAX"))
		ORLIX_TCTI_ADV_FP_HOST_BIN5("fmax");
	else if (!strcmp(m, "FMIN"))
		ORLIX_TCTI_ADV_FP_HOST_BIN5("fmin");
	else if (!strcmp(m, "FMAXNM"))
		ORLIX_TCTI_ADV_FP_HOST_BIN5("fmaxnm");
	else if (!strcmp(m, "FMINNM"))
		ORLIX_TCTI_ADV_FP_HOST_BIN5("fminnm");
	else if (!strcmp(m, "FMULX"))
		ORLIX_TCTI_ADV_FP_HOST_BIN5("fmulx");
	else if (!strcmp(m, "FABD"))
		ORLIX_TCTI_ADV_FP_HOST_BIN5("fabd");
	else if (!strcmp(m, "FACGE"))
		ORLIX_TCTI_ADV_FP_HOST_BIN5("facge");
	else if (!strcmp(m, "FACGT"))
		ORLIX_TCTI_ADV_FP_HOST_BIN5("facgt");
	else if (!strcmp(m, "FRECPS"))
		ORLIX_TCTI_ADV_FP_HOST_BIN5("frecps");
	else if (!strcmp(m, "FRSQRTS"))
		ORLIX_TCTI_ADV_FP_HOST_BIN5("frsqrts");
	else if (!strcmp(m, "FCMEQ") && !zero)
		ORLIX_TCTI_ADV_FP_HOST_BIN5("fcmeq");
	else if (!strcmp(m, "FCMGE") && !zero)
		ORLIX_TCTI_ADV_FP_HOST_BIN5("fcmge");
	else if (!strcmp(m, "FCMGT") && !zero)
		ORLIX_TCTI_ADV_FP_HOST_BIN5("fcmgt");
	else if (!strcmp(m, "FADDP") && !c->scalar) {
		if (v2s)
			ORLIX_TCTI_ADV_FP_HOST_BIN("faddp v0.2s, v0.2s, v1.2s");
		else if (v4s)
			ORLIX_TCTI_ADV_FP_HOST_BIN("faddp v0.4s, v0.4s, v1.4s");
		else if (v2d)
			ORLIX_TCTI_ADV_FP_HOST_BIN("faddp v0.2d, v0.2d, v1.2d");
		else
			return -EINVAL;
	} else if (!strcmp(m, "FMAXP") && !c->scalar) {
		if (v2s)
			ORLIX_TCTI_ADV_FP_HOST_BIN("fmaxp v0.2s, v0.2s, v1.2s");
		else if (v4s)
			ORLIX_TCTI_ADV_FP_HOST_BIN("fmaxp v0.4s, v0.4s, v1.4s");
		else if (v2d)
			ORLIX_TCTI_ADV_FP_HOST_BIN("fmaxp v0.2d, v0.2d, v1.2d");
		else
			return -EINVAL;
	} else if (!strcmp(m, "FMINP") && !c->scalar) {
		if (v2s)
			ORLIX_TCTI_ADV_FP_HOST_BIN("fminp v0.2s, v0.2s, v1.2s");
		else if (v4s)
			ORLIX_TCTI_ADV_FP_HOST_BIN("fminp v0.4s, v0.4s, v1.4s");
		else if (v2d)
			ORLIX_TCTI_ADV_FP_HOST_BIN("fminp v0.2d, v0.2d, v1.2d");
		else
			return -EINVAL;
	} else if (!strcmp(m, "FMAXNMP") && !c->scalar) {
		if (v2s)
			ORLIX_TCTI_ADV_FP_HOST_BIN("fmaxnmp v0.2s, v0.2s, v1.2s");
		else if (v4s)
			ORLIX_TCTI_ADV_FP_HOST_BIN("fmaxnmp v0.4s, v0.4s, v1.4s");
		else if (v2d)
			ORLIX_TCTI_ADV_FP_HOST_BIN("fmaxnmp v0.2d, v0.2d, v1.2d");
		else
			return -EINVAL;
	} else if (!strcmp(m, "FMINNMP") && !c->scalar) {
		if (v2s)
			ORLIX_TCTI_ADV_FP_HOST_BIN("fminnmp v0.2s, v0.2s, v1.2s");
		else if (v4s)
			ORLIX_TCTI_ADV_FP_HOST_BIN("fminnmp v0.4s, v0.4s, v1.4s");
		else if (v2d)
			ORLIX_TCTI_ADV_FP_HOST_BIN("fminnmp v0.2d, v0.2d, v1.2d");
		else
			return -EINVAL;
	} else
		return -EINVAL;
#undef ORLIX_TCTI_ADV_FP_HOST_BIN5
	return 0;
}

struct orlix_tcti_advsimd_fp_host_un_ctx {
	const char *mnemonic;
	const char *name;
	bool scalar;
	bool q;
	u8 access;
	u8 result;
	u8 src_index;
	u8 dst_index;
	u8 fbits;
	u64 *result_reg;
	const u64 *source;
	const u64 *acc;
};

static int orlix_tcti_advsimd_fp_host_un_body(void *opaque)
{
	struct orlix_tcti_advsimd_fp_host_un_ctx *c = opaque;
	const char *m = c->mnemonic;
	u64 *result = c->result_reg;
	const u64 *source = c->source;
	const u64 *acc = c->acc;
	bool s = c->scalar && c->access == sizeof(u32);
	bool d = c->scalar && c->access == sizeof(u64);
	bool v2s = !c->scalar && c->access == sizeof(u32) &&
		c->result == sizeof(u64);
	bool v4s = !c->scalar && c->access == sizeof(u32) &&
		c->result == 2 * sizeof(u64);
	bool v2d = !c->scalar && c->access == sizeof(u64) &&
		c->result == 2 * sizeof(u64);
	bool zero = strstr(c->name, "_FZ") != NULL;

	if (!strcmp(m, "FCVTL")) {
		if (c->src_index)
			ORLIX_TCTI_ADV_FP_HOST_UN("fcvtl2 v0.2d, v0.4s");
		else
			ORLIX_TCTI_ADV_FP_HOST_UN("fcvtl v0.2d, v0.2s");
		return 0;
	}
	if (!strcmp(m, "FCVTN")) {
		if (c->dst_index)
			asm volatile("ldr q0, [%[source]]\n ldr q1, [%[acc]]\n"
				     "fcvtn2 v1.4s, v0.2d\n str q1, [%[result]]\n"
				     : : [result] "r" (result), [source] "r" (source),
					 [acc] "r" (acc) : "v0", "v1", "memory");
		else
			ORLIX_TCTI_ADV_FP_HOST_UN("fcvtn v0.2s, v0.2d");
		return 0;
	}
	if (!strcmp(m, "FCVTXN")) {
		if (c->scalar)
			ORLIX_TCTI_ADV_FP_HOST_UN("fcvtxn s0, d0");
		else if (c->dst_index)
			asm volatile("ldr q0, [%[source]]\n ldr q1, [%[acc]]\n"
				     "fcvtxn2 v1.4s, v0.2d\n str q1, [%[result]]\n"
				     : : [result] "r" (result), [source] "r" (source),
					 [acc] "r" (acc) : "v0", "v1", "memory");
		else
			ORLIX_TCTI_ADV_FP_HOST_UN("fcvtxn v0.2s, v0.2d");
		return 0;
	}
	if (!strcmp(m, "FADDP") || !strcmp(m, "FMAXP") || !strcmp(m, "FMINP") ||
	    !strcmp(m, "FMAXNMP") || !strcmp(m, "FMINNMP") ||
	    !strcmp(m, "FMAXV") || !strcmp(m, "FMINV") ||
	    !strcmp(m, "FMAXNMV") || !strcmp(m, "FMINNMV")) {
		if (!strcmp(m, "FADDP")) {
			if (c->access == sizeof(u32))
				ORLIX_TCTI_ADV_FP_HOST_UN("faddp s0, v0.2s");
			else
				ORLIX_TCTI_ADV_FP_HOST_UN("faddp d0, v0.2d");
		} else if (!strcmp(m, "FMAXP")) {
			if (c->access == sizeof(u32))
				ORLIX_TCTI_ADV_FP_HOST_UN("fmaxp s0, v0.2s");
			else
				ORLIX_TCTI_ADV_FP_HOST_UN("fmaxp d0, v0.2d");
		} else if (!strcmp(m, "FMINP")) {
			if (c->access == sizeof(u32))
				ORLIX_TCTI_ADV_FP_HOST_UN("fminp s0, v0.2s");
			else
				ORLIX_TCTI_ADV_FP_HOST_UN("fminp d0, v0.2d");
		} else if (!strcmp(m, "FMAXNMP")) {
			if (c->access == sizeof(u32))
				ORLIX_TCTI_ADV_FP_HOST_UN("fmaxnmp s0, v0.2s");
			else
				ORLIX_TCTI_ADV_FP_HOST_UN("fmaxnmp d0, v0.2d");
		} else if (!strcmp(m, "FMINNMP")) {
			if (c->access == sizeof(u32))
				ORLIX_TCTI_ADV_FP_HOST_UN("fminnmp s0, v0.2s");
			else
				ORLIX_TCTI_ADV_FP_HOST_UN("fminnmp d0, v0.2d");
		} else if (!strcmp(m, "FMAXV"))
			ORLIX_TCTI_ADV_FP_HOST_UN("fmaxv s0, v0.4s");
		else if (!strcmp(m, "FMINV"))
			ORLIX_TCTI_ADV_FP_HOST_UN("fminv s0, v0.4s");
		else if (!strcmp(m, "FMAXNMV"))
			ORLIX_TCTI_ADV_FP_HOST_UN("fmaxnmv s0, v0.4s");
		else
			ORLIX_TCTI_ADV_FP_HOST_UN("fminnmv s0, v0.4s");
		return 0;
	}
#define ORLIX_TCTI_ADV_FP_HOST_UN5(op) \
	do { \
		if (s) \
			ORLIX_TCTI_ADV_FP_HOST_UN(op " s0, s0"); \
		else if (d) \
			ORLIX_TCTI_ADV_FP_HOST_UN(op " d0, d0"); \
		else if (v2s) \
			ORLIX_TCTI_ADV_FP_HOST_UN(op " v0.2s, v0.2s"); \
		else if (v4s) \
			ORLIX_TCTI_ADV_FP_HOST_UN(op " v0.4s, v0.4s"); \
		else if (v2d) \
			ORLIX_TCTI_ADV_FP_HOST_UN(op " v0.2d, v0.2d"); \
		else \
			return -EINVAL; \
	} while (0)
	if (c->fbits) {
		if (c->fbits == 32 && v4s) {
			if (!strcmp(m, "SCVTF"))
				ORLIX_TCTI_ADV_FP_HOST_UN("scvtf v0.4s, v0.4s, #32");
			else if (!strcmp(m, "UCVTF"))
				ORLIX_TCTI_ADV_FP_HOST_UN("ucvtf v0.4s, v0.4s, #32");
			else if (!strcmp(m, "FCVTZS"))
				ORLIX_TCTI_ADV_FP_HOST_UN("fcvtzs v0.4s, v0.4s, #32");
			else if (!strcmp(m, "FCVTZU"))
				ORLIX_TCTI_ADV_FP_HOST_UN("fcvtzu v0.4s, v0.4s, #32");
			else
				return -EINVAL;
		} else if (c->fbits == 32 && v2s) {
			if (!strcmp(m, "SCVTF"))
				ORLIX_TCTI_ADV_FP_HOST_UN("scvtf v0.2s, v0.2s, #32");
			else if (!strcmp(m, "UCVTF"))
				ORLIX_TCTI_ADV_FP_HOST_UN("ucvtf v0.2s, v0.2s, #32");
			else if (!strcmp(m, "FCVTZS"))
				ORLIX_TCTI_ADV_FP_HOST_UN("fcvtzs v0.2s, v0.2s, #32");
			else if (!strcmp(m, "FCVTZU"))
				ORLIX_TCTI_ADV_FP_HOST_UN("fcvtzu v0.2s, v0.2s, #32");
			else
				return -EINVAL;
		} else if (c->fbits == 64 && v2d) {
			if (!strcmp(m, "SCVTF"))
				ORLIX_TCTI_ADV_FP_HOST_UN("scvtf v0.2d, v0.2d, #64");
			else if (!strcmp(m, "UCVTF"))
				ORLIX_TCTI_ADV_FP_HOST_UN("ucvtf v0.2d, v0.2d, #64");
			else if (!strcmp(m, "FCVTZS"))
				ORLIX_TCTI_ADV_FP_HOST_UN("fcvtzs v0.2d, v0.2d, #64");
			else if (!strcmp(m, "FCVTZU"))
				ORLIX_TCTI_ADV_FP_HOST_UN("fcvtzu v0.2d, v0.2d, #64");
			else
				return -EINVAL;
		} else if (c->fbits == 63 && d) {
			if (!strcmp(m, "SCVTF"))
				ORLIX_TCTI_ADV_FP_HOST_UN("scvtf d0, d0, #63");
			else if (!strcmp(m, "UCVTF"))
				ORLIX_TCTI_ADV_FP_HOST_UN("ucvtf d0, d0, #63");
			else if (!strcmp(m, "FCVTZS"))
				ORLIX_TCTI_ADV_FP_HOST_UN("fcvtzs d0, d0, #63");
			else if (!strcmp(m, "FCVTZU"))
				ORLIX_TCTI_ADV_FP_HOST_UN("fcvtzu d0, d0, #63");
			else
				return -EINVAL;
		} else if (c->fbits == 32 && s) {
			if (!strcmp(m, "SCVTF"))
				ORLIX_TCTI_ADV_FP_HOST_UN("scvtf s0, s0, #32");
			else if (!strcmp(m, "UCVTF"))
				ORLIX_TCTI_ADV_FP_HOST_UN("ucvtf s0, s0, #32");
			else if (!strcmp(m, "FCVTZS"))
				ORLIX_TCTI_ADV_FP_HOST_UN("fcvtzs s0, s0, #32");
			else if (!strcmp(m, "FCVTZU"))
				ORLIX_TCTI_ADV_FP_HOST_UN("fcvtzu s0, s0, #32");
			else
				return -EINVAL;
		} else
			return -EINVAL;
		return 0;
	}
	if (!strcmp(m, "FABS"))
		ORLIX_TCTI_ADV_FP_HOST_UN5("fabs");
	else if (!strcmp(m, "FNEG"))
		ORLIX_TCTI_ADV_FP_HOST_UN5("fneg");
	else if (!strcmp(m, "FSQRT"))
		ORLIX_TCTI_ADV_FP_HOST_UN5("fsqrt");
	else if (!strcmp(m, "FRECPE"))
		ORLIX_TCTI_ADV_FP_HOST_UN5("frecpe");
	else if (!strcmp(m, "FRSQRTE"))
		ORLIX_TCTI_ADV_FP_HOST_UN5("frsqrte");
	else if (!strcmp(m, "FRECPX")) {
		if (s)
			ORLIX_TCTI_ADV_FP_HOST_UN("frecpx s0, s0");
		else if (d)
			ORLIX_TCTI_ADV_FP_HOST_UN("frecpx d0, d0");
		else
			return -EINVAL;
	} else if (!strcmp(m, "FRINTN"))
		ORLIX_TCTI_ADV_FP_HOST_UN5("frintn");
	else if (!strcmp(m, "FRINTP"))
		ORLIX_TCTI_ADV_FP_HOST_UN5("frintp");
	else if (!strcmp(m, "FRINTM"))
		ORLIX_TCTI_ADV_FP_HOST_UN5("frintm");
	else if (!strcmp(m, "FRINTZ"))
		ORLIX_TCTI_ADV_FP_HOST_UN5("frintz");
	else if (!strcmp(m, "FRINTA"))
		ORLIX_TCTI_ADV_FP_HOST_UN5("frinta");
	else if (!strcmp(m, "FRINTX"))
		ORLIX_TCTI_ADV_FP_HOST_UN5("frintx");
	else if (!strcmp(m, "FRINTI"))
		ORLIX_TCTI_ADV_FP_HOST_UN5("frinti");
	else if (!strcmp(m, "FCMEQ") && zero) {
		if (s)
			ORLIX_TCTI_ADV_FP_HOST_UN("fcmeq s0, s0, #0.0");
		else if (d)
			ORLIX_TCTI_ADV_FP_HOST_UN("fcmeq d0, d0, #0.0");
		else if (v2s)
			ORLIX_TCTI_ADV_FP_HOST_UN("fcmeq v0.2s, v0.2s, #0.0");
		else if (v4s)
			ORLIX_TCTI_ADV_FP_HOST_UN("fcmeq v0.4s, v0.4s, #0.0");
		else if (v2d)
			ORLIX_TCTI_ADV_FP_HOST_UN("fcmeq v0.2d, v0.2d, #0.0");
		else
			return -EINVAL;
	} else if (!strcmp(m, "FCMGE") && zero) {
		if (s)
			ORLIX_TCTI_ADV_FP_HOST_UN("fcmge s0, s0, #0.0");
		else if (d)
			ORLIX_TCTI_ADV_FP_HOST_UN("fcmge d0, d0, #0.0");
		else if (v2s)
			ORLIX_TCTI_ADV_FP_HOST_UN("fcmge v0.2s, v0.2s, #0.0");
		else if (v4s)
			ORLIX_TCTI_ADV_FP_HOST_UN("fcmge v0.4s, v0.4s, #0.0");
		else if (v2d)
			ORLIX_TCTI_ADV_FP_HOST_UN("fcmge v0.2d, v0.2d, #0.0");
		else
			return -EINVAL;
	} else if (!strcmp(m, "FCMGT") && zero) {
		if (s)
			ORLIX_TCTI_ADV_FP_HOST_UN("fcmgt s0, s0, #0.0");
		else if (d)
			ORLIX_TCTI_ADV_FP_HOST_UN("fcmgt d0, d0, #0.0");
		else if (v2s)
			ORLIX_TCTI_ADV_FP_HOST_UN("fcmgt v0.2s, v0.2s, #0.0");
		else if (v4s)
			ORLIX_TCTI_ADV_FP_HOST_UN("fcmgt v0.4s, v0.4s, #0.0");
		else if (v2d)
			ORLIX_TCTI_ADV_FP_HOST_UN("fcmgt v0.2d, v0.2d, #0.0");
		else
			return -EINVAL;
	} else if (!strcmp(m, "FCMLE")) {
		if (s)
			ORLIX_TCTI_ADV_FP_HOST_UN("fcmle s0, s0, #0.0");
		else if (d)
			ORLIX_TCTI_ADV_FP_HOST_UN("fcmle d0, d0, #0.0");
		else if (v2s)
			ORLIX_TCTI_ADV_FP_HOST_UN("fcmle v0.2s, v0.2s, #0.0");
		else if (v4s)
			ORLIX_TCTI_ADV_FP_HOST_UN("fcmle v0.4s, v0.4s, #0.0");
		else if (v2d)
			ORLIX_TCTI_ADV_FP_HOST_UN("fcmle v0.2d, v0.2d, #0.0");
		else
			return -EINVAL;
	} else if (!strcmp(m, "FCMLT")) {
		if (s)
			ORLIX_TCTI_ADV_FP_HOST_UN("fcmlt s0, s0, #0.0");
		else if (d)
			ORLIX_TCTI_ADV_FP_HOST_UN("fcmlt d0, d0, #0.0");
		else if (v2s)
			ORLIX_TCTI_ADV_FP_HOST_UN("fcmlt v0.2s, v0.2s, #0.0");
		else if (v4s)
			ORLIX_TCTI_ADV_FP_HOST_UN("fcmlt v0.4s, v0.4s, #0.0");
		else if (v2d)
			ORLIX_TCTI_ADV_FP_HOST_UN("fcmlt v0.2d, v0.2d, #0.0");
		else
			return -EINVAL;
	} else if (!strcmp(m, "SCVTF"))
		ORLIX_TCTI_ADV_FP_HOST_UN5("scvtf");
	else if (!strcmp(m, "UCVTF"))
		ORLIX_TCTI_ADV_FP_HOST_UN5("ucvtf");
	else if (!strcmp(m, "FCVTZS"))
		ORLIX_TCTI_ADV_FP_HOST_UN5("fcvtzs");
	else if (!strcmp(m, "FCVTZU"))
		ORLIX_TCTI_ADV_FP_HOST_UN5("fcvtzu");
	else if (!strcmp(m, "FCVTNS"))
		ORLIX_TCTI_ADV_FP_HOST_UN5("fcvtns");
	else if (!strcmp(m, "FCVTNU"))
		ORLIX_TCTI_ADV_FP_HOST_UN5("fcvtnu");
	else if (!strcmp(m, "FCVTPS"))
		ORLIX_TCTI_ADV_FP_HOST_UN5("fcvtps");
	else if (!strcmp(m, "FCVTPU"))
		ORLIX_TCTI_ADV_FP_HOST_UN5("fcvtpu");
	else if (!strcmp(m, "FCVTMS"))
		ORLIX_TCTI_ADV_FP_HOST_UN5("fcvtms");
	else if (!strcmp(m, "FCVTMU"))
		ORLIX_TCTI_ADV_FP_HOST_UN5("fcvtmu");
	else if (!strcmp(m, "FCVTAS"))
		ORLIX_TCTI_ADV_FP_HOST_UN5("fcvtas");
	else if (!strcmp(m, "FCVTAU"))
		ORLIX_TCTI_ADV_FP_HOST_UN5("fcvtau");
	else
		return -EINVAL;
#undef ORLIX_TCTI_ADV_FP_HOST_UN5
	return 0;
}

static int orlix_tcti_advsimd_fp_expected_from_source(
	const struct orlix_tcti_test_fp_source *source, u32 instruction,
	const u64 rn[2], const u64 rm[2], const u64 rd[2],
	unsigned long fpcr, unsigned long fpsr_in, u64 expected_rd[2],
	unsigned long *expected_fpsr)
{
	const char *name = source->name;
	bool scalar = strstr(name, "asisd") != NULL;
	bool q = instruction & BIT(30);
	u8 sz = (instruction >> 22) & 1U;
	u8 access = sz ? sizeof(u64) : sizeof(u32);
	u8 result = scalar ? access : (q ? 2 * sizeof(u64) : sizeof(u64));
	u64 left[2] = { rn[0], rn[1] };
	u64 right[2] = { rm[0], rm[1] };
	u64 acc[2] = { rd[0], rd[1] };
	int ret;

	*expected_fpsr = fpsr_in;
	expected_rd[0] = rd[0];
	expected_rd[1] = rd[1];

	if (strstr(name, "asimdimm")) {
		u8 imm8 = orlix_tcti_advsimd_fp_imm8(instruction);
		u64 pattern;

		if (instruction & BIT(29)) {
			if (!q)
				return -EINVAL;
			pattern = orlix_tcti_advsimd_fp_expand_fp64_imm(imm8);
			expected_rd[0] = pattern;
			expected_rd[1] = pattern;
		} else {
			u32 lane = orlix_tcti_advsimd_fp_expand_fp32_imm(imm8);

			pattern = (u64)lane | ((u64)lane << 32);
			expected_rd[0] = pattern;
			expected_rd[1] = q ? pattern : 0;
		}
		return 0;
	}
	if (strstr(name, "asimdall") || strstr(name, "asisdpair")) {
		struct orlix_tcti_advsimd_fp_host_un_ctx ctx = {
			.mnemonic = source->mnemonic,
			.name = name,
			.scalar = scalar,
			.q = q,
			.access = access,
			.result = result,
			.result_reg = expected_rd,
			.source = left,
			.acc = acc,
		};

		ret = orlix_tcti_advsimd_fp_host_wrap(
			orlix_tcti_advsimd_fp_host_un_body, &ctx, fpcr,
			expected_fpsr);
		if (!ret && !q && !scalar)
			expected_rd[1] = 0;
		return ret;
	}
	if (strstr(name, "asimdelem") || strstr(name, "asisdelem")) {
		struct orlix_tcti_advsimd_fp_host_bin_ctx ctx = {
			.mnemonic = source->mnemonic,
			.name = name,
			.scalar = scalar,
			.access = access,
			.result = result,
			.result_reg = expected_rd,
			.left = left,
			.right = right,
			.acc = acc,
		};
		u8 h = (instruction >> 11) & 1U;
		u8 l = (instruction >> 21) & 1U;
		u8 index = sz ? h : ((h << 1) | l);

		orlix_tcti_advsimd_fp_broadcast_lane(right, rm, access, index,
						     result);
		return orlix_tcti_advsimd_fp_host_wrap(
			orlix_tcti_advsimd_fp_host_bin_body, &ctx, fpcr,
			expected_fpsr);
	}
	if (strstr(name, "asimdshf") || strstr(name, "asisdshf")) {
		struct orlix_tcti_advsimd_fp_host_un_ctx ctx = {
			.mnemonic = source->mnemonic,
			.name = name,
			.scalar = scalar,
			.q = q,
			.result_reg = expected_rd,
			.source = left,
			.acc = acc,
		};
		u32 immh = (instruction >> 19) & 0xfU;
		u32 immb = (instruction >> 16) & 0x7U;
		u32 imm = (immh << 3) | immb;
		u8 esize = (immh & 8U) ? 64 : (immh & 4U) ? 32 : 16;

		access = esize / 8U;
		ctx.access = access;
		ctx.result = scalar ? access : (q ? 2 * sizeof(u64) : sizeof(u64));
		ctx.fbits = (u8)(esize * 2U - imm);
		return orlix_tcti_advsimd_fp_host_wrap(
			orlix_tcti_advsimd_fp_host_un_body, &ctx, fpcr,
			expected_fpsr);
	}
	if (!strcmp(source->mnemonic, "FCVTL") ||
	    !strcmp(source->mnemonic, "FCVTN") ||
	    !strcmp(source->mnemonic, "FCVTXN") ||
	    strstr(name, "asimdmisc") || strstr(name, "asisdmisc") ||
	    !strcmp(source->mnemonic, "FABS") ||
	    !strcmp(source->mnemonic, "FNEG") ||
	    !strcmp(source->mnemonic, "FSQRT") ||
	    !strcmp(source->mnemonic, "FRECPE") ||
	    !strcmp(source->mnemonic, "FRSQRTE") ||
	    !strcmp(source->mnemonic, "FRECPX") ||
	    !strncmp(source->mnemonic, "FRINT", 5) ||
	    !strcmp(source->mnemonic, "SCVTF") ||
	    !strcmp(source->mnemonic, "UCVTF") ||
	    !strncmp(source->mnemonic, "FCVT", 4)) {
		struct orlix_tcti_advsimd_fp_host_un_ctx ctx = {
			.mnemonic = source->mnemonic,
			.name = name,
			.scalar = scalar,
			.q = q,
			.access = access,
			.result = result,
			.result_reg = expected_rd,
			.source = left,
			.acc = acc,
		};

		if (!strcmp(source->mnemonic, "FCVTL")) {
			ctx.access = sizeof(u32);
			ctx.result = 2 * sizeof(u64);
			ctx.src_index = q ? 1U : 0;
		} else if (!strcmp(source->mnemonic, "FCVTN") ||
			   (!strcmp(source->mnemonic, "FCVTXN") && !scalar)) {
			ctx.access = sizeof(u64);
			ctx.result = q ? 2 * sizeof(u64) : sizeof(u64);
			ctx.dst_index = q ? 1U : 0;
		} else if (!strcmp(source->mnemonic, "FCVTXN")) {
			ctx.access = sizeof(u64);
			ctx.result = sizeof(u32);
			ctx.scalar = true;
		}
		return orlix_tcti_advsimd_fp_host_wrap(
			orlix_tcti_advsimd_fp_host_un_body, &ctx, fpcr,
			expected_fpsr);
	}
	{
		struct orlix_tcti_advsimd_fp_host_bin_ctx ctx = {
			.mnemonic = source->mnemonic,
			.name = name,
			.scalar = scalar,
			.access = access,
			.result = result,
			.result_reg = expected_rd,
			.left = left,
			.right = right,
			.acc = acc,
		};

		return orlix_tcti_advsimd_fp_host_wrap(
			orlix_tcti_advsimd_fp_host_bin_body, &ctx, fpcr,
			expected_fpsr);
	}
}

static void orlix_tcti_advsimd_fp_seed_simd(void)
{
	size_t index;

	current->thread.user_simd_valid = 1;
	current->thread.user_fpcr = 0;
	current->thread.user_fpsr = 0;
	for (index = 0; index < ARRAY_SIZE(current->thread.user_simd); index++) {
		u32 a = 0x3f800000U + ((u32)index << 13);
		u32 b = 0x40000000U + ((u32)index << 13);

		current->thread.user_simd[index] = (u64)a | ((u64)b << 32);
	}
}

static void orlix_tcti_advsimd_fp_seed(struct pt_regs *regs, unsigned long code)
{
	memset(regs, 0, sizeof(*regs));
	regs->pc = code;
	regs->pstate = PSR_MODE_EL0t | PSR_Z_BIT;
	regs->syscallno = NO_SYSCALL;
	regs->regs[0] = 0x1111111111111111ULL;
	regs->regs[1] = 0x2222222222222222ULL;
	regs->regs[2] = 0x3333333333333333ULL;
	regs->regs[30] = 0x4444444444444444ULL;
	orlix_tcti_advsimd_fp_seed_simd();
}

static void orlix_tcti_advsimd_fp_expect_success(struct kunit *test,
	const struct orlix_tcti_test_fp_source *source,
	const struct orlix_tcti_result *result, const struct pt_regs *regs,
	unsigned long code)
{
	KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result->reason,
			    "%s ordinal %u reason", source->name, source->ordinal);
	KUNIT_EXPECT_EQ(test, 0L, result->status);
	KUNIT_EXPECT_EQ(test, FP_SVC, result->instruction);
	KUNIT_EXPECT_EQ_MSG(test, code + sizeof(u32), regs->pc,
			    "%s ordinal %u pc", source->name, source->ordinal);
}

static void orlix_tcti_advsimd_fp_capture_run(struct kunit *test,
	const struct orlix_tcti_test_fp_source *source, u32 instruction,
	u32 obligation, struct pt_regs *regs, unsigned long code)
{
	const void *token;
	struct orlix_tcti_native_capture_session *capture = NULL;
	struct orlix_tcti_native_wire_record wire = {};
	struct orlix_tcti_target_proof_ingestion_ledger *ledger;
	struct orlix_tcti_result result;
	u64 before_simd[ARRAY_SIZE(current->thread.user_simd)];
	u64 expected_rd[2];
	unsigned long expected_fpsr;
	int ret;

	memcpy(before_simd, current->thread.user_simd, sizeof(before_simd));
	ret = orlix_tcti_advsimd_fp_expected_from_source(source, instruction,
		&before_simd[FP_RN * 2U], &before_simd[FP_RM * 2U],
		&before_simd[FP_RD * 2U], current->thread.user_fpcr,
		current->thread.user_fpsr, expected_rd, &expected_fpsr);
	KUNIT_EXPECT_EQ_MSG(test, 0, ret, "%s expected-from-source %s",
			    source->name, source->mnemonic);
	if (ret)
		return;
	token = orlix_tcti_advsimd_fp_production_capture_token(source->ordinal,
							      obligation);
	KUNIT_EXPECT_TRUE_MSG(test, token != NULL, "%s token %u", source->name,
			      obligation);
	if (!token)
		return;
	ret = orlix_tcti_native_capture_begin(token, source->ordinal, obligation,
					      &capture);
	KUNIT_EXPECT_EQ_MSG(test, 0, ret, "%s begin %u", source->name, obligation);
	result = orlix_tcti_resume_user(current, regs, current->mm);
	orlix_tcti_advsimd_fp_expect_success(test, source, &result, regs, code);
	KUNIT_EXPECT_EQ_MSG(test, expected_rd[0],
			    current->thread.user_simd[FP_RD * 2U],
			    "%s dest lo %s", source->name, source->mnemonic);
	KUNIT_EXPECT_EQ_MSG(test, expected_rd[1],
			    current->thread.user_simd[FP_RD * 2U + 1U],
			    "%s dest hi %s", source->name, source->mnemonic);
	KUNIT_EXPECT_EQ_MSG(test, expected_fpsr, current->thread.user_fpsr,
			    "%s fpsr %s", source->name, source->mnemonic);
	ret = orlix_tcti_native_capture_take_wire(capture, &wire);
	KUNIT_EXPECT_EQ_MSG(test, 0, ret, "%s wire %u", source->name, obligation);
	if (!ret) {
		ledger = orlix_tcti_target_proof_ingestion_ledger_create(1U);
		KUNIT_ASSERT_NOT_NULL(test, ledger);
		KUNIT_EXPECT_EQ_MSG(test, 0,
			orlix_tcti_target_proof_ingest_native(ledger, &wire, NULL),
			"%s ingest %u", source->name, obligation);
		KUNIT_EXPECT_EQ(test, 1U, ledger->native_passed);
		KUNIT_EXPECT_LT_MSG(test,
			orlix_tcti_target_proof_ingest_native(ledger, &wire, NULL),
			0, "%s replay %u", source->name, obligation);
		KUNIT_EXPECT_EQ(test, 1U, ledger->native_passed);
		orlix_tcti_target_proof_ingestion_ledger_destroy(ledger);
	}
	orlix_tcti_native_wire_record_destroy(&wire);
	orlix_tcti_native_capture_destroy(capture);
}

static void orlix_tcti_advsimd_fp_decodes_exact_source_cohort(struct kunit *test)
{
	size_t index;
	size_t count = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_fp_source *source =
			&orlix_tcti_test_sources[index];
		struct orlix_tcti_decoded_instruction decoded;
		u32 instruction;

		if (!orlix_tcti_test_is_advsimd_fp(source))
			continue;
		instruction = orlix_tcti_advsimd_fp_legal_instruction(source);
		KUNIT_ASSERT_EQ_MSG(test, source->pattern,
				instruction & source->mask, "%s", source->name);
		decoded = orlix_tcti_decode_aarch64(instruction);
		if (orlix_tcti_test_is_optional_fp_feature(source))
			KUNIT_EXPECT_TRUE_MSG(test,
				orlix_tcti_test_fp_decode_is_family(
					decoded.decode_class) ||
				decoded.decode_class ==
					ORLIX_TCTI_DECODE_UNSUPPORTED,
				"%s class %u insn %#x", source->name,
				decoded.decode_class, instruction);
		else
			KUNIT_EXPECT_TRUE_MSG(test,
				orlix_tcti_test_fp_decode_is_family(
					decoded.decode_class),
				"%s class %u insn %#x", source->name,
				decoded.decode_class, instruction);
		KUNIT_EXPECT_EQ_MSG(test, source->ordinal, decoded.source_ordinal,
				"%s %#x", source->name, instruction);
		count++;
	}
	KUNIT_EXPECT_EQ(test, FP_FAMILY_COUNT, count);
}

static void orlix_tcti_advsimd_fp_binds_pinned_ddi0602_semantics(
	struct kunit *test)
{
	size_t index;
	size_t ddi = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_fp_source *source =
			&orlix_tcti_test_sources[index];

		if (!orlix_tcti_test_is_advsimd_fp(source))
			continue;
		KUNIT_ASSERT_LT_MSG(test, source->ordinal,
			ARRAY_SIZE(orlix_tcti_test_has_ddi0602_semantics), "%s",
			source->name);
		KUNIT_EXPECT_TRUE_MSG(test,
			orlix_tcti_test_has_ddi0602_semantics[source->ordinal], "%s",
			source->name);
		ddi++;
	}
	KUNIT_EXPECT_EQ(test, FP_FAMILY_COUNT, ddi);
}

static void orlix_tcti_advsimd_fp_production_resume(struct kunit *test)
{
	size_t index;
	size_t seen = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_fp_source *source =
			&orlix_tcti_test_sources[index];
		u32 instruction;
		struct orlix_tcti_decoded_instruction decoded;
		struct pt_regs regs = {};
		unsigned long code;

		if (!orlix_tcti_test_is_el0_fp(source))
			continue;
		seen++;
		instruction = orlix_tcti_advsimd_fp_legal_instruction(source);
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_ASSERT_TRUE_MSG(test,
			orlix_tcti_test_fp_decode_is_family(decoded.decode_class),
			"%s class %u", source->name, decoded.decode_class);
		KUNIT_ASSERT_EQ_MSG(test, source->ordinal, decoded.source_ordinal,
				    "%s ordinal", source->name);
		KUNIT_EXPECT_NE_MSG(test, 0U, decoded.access_size,
				    "%s access_size op %u scalar %u imm %d class %u insn %#x",
				    source->name, decoded.simd_arithmetic_op,
				    decoded.simd_scalar, decoded.immediate,
				    decoded.decode_class, instruction);
		code = orlix_tcti_advsimd_fp_map(test, instruction);
		orlix_tcti_advsimd_fp_seed(&regs, code);
		orlix_tcti_advsimd_fp_capture_run(test, source, instruction,
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD, &regs, code);
		orlix_tcti_advsimd_fp_seed(&regs, code);
		orlix_tcti_advsimd_fp_capture_run(test, source, instruction,
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC, &regs, code);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
	}
	KUNIT_EXPECT_EQ(test, FP_EL0_COUNT, seen);
}

static void orlix_tcti_advsimd_fp_non_el0_rejected(struct kunit *test)
{
	size_t index;
	size_t seen = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_fp_source *source =
			&orlix_tcti_test_sources[index];
		u32 instruction;
		struct orlix_tcti_decoded_instruction decoded;
		struct pt_regs regs = {};
		struct pt_regs before;
		struct orlix_tcti_result result;
		unsigned long code;

		if (!orlix_tcti_test_is_advsimd_fp(source) ||
		    !orlix_tcti_test_is_optional_fp_feature(source))
			continue;
		seen++;
		instruction = orlix_tcti_advsimd_fp_legal_instruction(source);
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_EXPECT_TRUE_MSG(test,
			orlix_tcti_test_fp_decode_is_family(decoded.decode_class) ||
			decoded.decode_class == ORLIX_TCTI_DECODE_UNSUPPORTED,
			"%s class %u", source->name, decoded.decode_class);
		KUNIT_EXPECT_EQ_MSG(test, source->ordinal, decoded.source_ordinal,
				    "%s ordinal", source->name);
		code = orlix_tcti_advsimd_fp_map(test, instruction);
		orlix_tcti_advsimd_fp_seed(&regs, code);
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				    result.reason, "%s", source->name);
		KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
		KUNIT_EXPECT_EQ(test, instruction, result.instruction);
		KUNIT_EXPECT_EQ(test, before.pc, result.pc);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
	}
	KUNIT_EXPECT_EQ(test, FP_NON_EL0_COUNT, seen);
}

static void orlix_tcti_advsimd_fp_nan_rounding_overlap(struct kunit *test)
{
	const struct orlix_tcti_test_fp_source *fadd =
		orlix_tcti_test_fp_name("FADD_asimdsame_only");
	u32 insn;
	unsigned long code;
	struct pt_regs regs = {};
	struct orlix_tcti_result result;
	u64 before_high;
	u64 overlap_before;

	KUNIT_ASSERT_NOT_NULL(test, fadd);

	insn = (fadd->pattern & fadd->mask) | (FP_RN << 5) | (FP_RM << 16) |
		FP_RD | BIT(30);
	code = orlix_tcti_advsimd_fp_map(test, insn);
	orlix_tcti_advsimd_fp_seed(&regs, code);
	current->thread.user_simd[FP_RN * 2U] = 0x7f800001ULL;
	current->thread.user_simd[FP_RM * 2U] = 0x3f800000ULL;
	current->thread.user_fpsr = 0;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_NE(test, 0UL, current->thread.user_fpsr & AARCH64_FPSR_IOC);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	insn = (fadd->pattern & fadd->mask) | (FP_RN << 5) | (FP_RM << 16) |
		FP_RD;
	code = orlix_tcti_advsimd_fp_map(test, insn);
	orlix_tcti_advsimd_fp_seed(&regs, code);
	current->thread.user_simd[FP_RD * 2U + 1U] = 0xa5a5a5a5a5a5a5a5ULL;
	before_high = current->thread.user_simd[FP_RD * 2U + 1U];
	current->thread.user_simd[FP_RN * 2U] = 0x3f800000ULL;
	current->thread.user_simd[FP_RM * 2U] = 0x3f800000ULL;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_NE(test, before_high, 0ULL);
	/* Q=0 AdvSIMD writes 64 bits and clears the upper 64 bits. */
	KUNIT_EXPECT_EQ(test, 0ULL,
			current->thread.user_simd[FP_RD * 2U + 1U]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	insn = (fadd->pattern & fadd->mask) | FP_RD | (FP_RD << 5) |
		(FP_RM << 16) | BIT(30);
	code = orlix_tcti_advsimd_fp_map(test, insn);
	orlix_tcti_advsimd_fp_seed(&regs, code);
	current->thread.user_simd[FP_RD * 2U] = 0x3f800000ULL;
	current->thread.user_simd[FP_RM * 2U] = 0x3f800000ULL;
	overlap_before = current->thread.user_simd[FP_RD * 2U];
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_NE(test, overlap_before,
			current->thread.user_simd[FP_RD * 2U]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
}

static void orlix_tcti_advsimd_fp_reserved_encodings(struct kunit *test)
{
	struct orlix_tcti_decoded_instruction decoded;
	u32 insn;
	unsigned long code;
	struct pt_regs regs = {};
	struct pt_regs before;
	struct orlix_tcti_result result;

	/* Q=0 64-bit AdvSIMD FADD is reserved. */
	insn = 0x0e60d400U;
	decoded = orlix_tcti_decode_aarch64(insn);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED, decoded.decode_class);
	code = orlix_tcti_advsimd_fp_map(test, insn);
	orlix_tcti_advsimd_fp_seed(&regs, code);
	before = regs;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION, result.reason);
	KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	/* Reserved asisdshf immh == 0000. */
	insn = 0x5f00e400U;
	decoded = orlix_tcti_decode_aarch64(insn);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED, decoded.decode_class);
	code = orlix_tcti_advsimd_fp_map(test, insn);
	orlix_tcti_advsimd_fp_seed(&regs, code);
	before = regs;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION, result.reason);
	KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
}

static struct kunit_case orlix_tcti_advsimd_fp_source_bound_cases[] = {
	KUNIT_CASE(orlix_tcti_advsimd_fp_decodes_exact_source_cohort),
	KUNIT_CASE(orlix_tcti_advsimd_fp_binds_pinned_ddi0602_semantics),
	KUNIT_CASE(orlix_tcti_advsimd_fp_production_resume),
	KUNIT_CASE(orlix_tcti_advsimd_fp_non_el0_rejected),
	KUNIT_CASE(orlix_tcti_advsimd_fp_nan_rounding_overlap),
	KUNIT_CASE(orlix_tcti_advsimd_fp_reserved_encodings),
	{}
};

static struct kunit_suite orlix_tcti_advsimd_fp_source_bound_suite = {
	.name = "orlix-tcti-advsimd-fp-source-bound",
	.init = orlix_tcti_advsimd_fp_test_init,
	.exit = orlix_tcti_advsimd_fp_test_exit,
	.test_cases = orlix_tcti_advsimd_fp_source_bound_cases,
};

kunit_test_suite(orlix_tcti_advsimd_fp_source_bound_suite);
