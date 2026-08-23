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
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/string.h>
#include <linux/syscalls.h>

#include "../decode_aarch64.h"
#include "../fixed_fp.h"
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

static int orlix_tcti_advsimd_fp_arith_from_source(
	const struct orlix_tcti_test_fp_source *source)
{
	const char *m = source->mnemonic;
	const char *name = source->name;
	bool zero = strstr(name, "_FZ") != NULL;

	if (!strcmp(m, "FADD"))
		return ORLIX_TCTI_SIMD_ARITH_FADD;
	if (!strcmp(m, "FSUB"))
		return ORLIX_TCTI_SIMD_ARITH_FSUB;
	if (!strcmp(m, "FMUL"))
		return ORLIX_TCTI_SIMD_ARITH_FMUL;
	if (!strcmp(m, "FDIV"))
		return ORLIX_TCTI_SIMD_ARITH_FDIV;
	if (!strcmp(m, "FMLA"))
		return ORLIX_TCTI_SIMD_ARITH_FMLA;
	if (!strcmp(m, "FMLS"))
		return ORLIX_TCTI_SIMD_ARITH_FMLS;
	if (!strcmp(m, "FMAX"))
		return ORLIX_TCTI_SIMD_ARITH_FMAX;
	if (!strcmp(m, "FMIN"))
		return ORLIX_TCTI_SIMD_ARITH_FMIN;
	if (!strcmp(m, "FMAXNM"))
		return ORLIX_TCTI_SIMD_ARITH_FMAXNM;
	if (!strcmp(m, "FMINNM"))
		return ORLIX_TCTI_SIMD_ARITH_FMINNM;
	if (!strcmp(m, "FMULX"))
		return ORLIX_TCTI_SIMD_ARITH_FMULX;
	if (!strcmp(m, "FABD"))
		return ORLIX_TCTI_SIMD_ARITH_FABD;
	if (!strcmp(m, "FACGE"))
		return ORLIX_TCTI_SIMD_ARITH_FACGE;
	if (!strcmp(m, "FACGT"))
		return ORLIX_TCTI_SIMD_ARITH_FACGT;
	if (!strcmp(m, "FRECPS"))
		return ORLIX_TCTI_SIMD_ARITH_FRECPS;
	if (!strcmp(m, "FRSQRTS"))
		return ORLIX_TCTI_SIMD_ARITH_FRSQRTS;
	if (!strcmp(m, "FADDP"))
		return ORLIX_TCTI_SIMD_ARITH_FADDP;
	if (!strcmp(m, "FMAXP"))
		return ORLIX_TCTI_SIMD_ARITH_FMAXP;
	if (!strcmp(m, "FMINP"))
		return ORLIX_TCTI_SIMD_ARITH_FMINP;
	if (!strcmp(m, "FMAXNMP"))
		return ORLIX_TCTI_SIMD_ARITH_FMAXNMP;
	if (!strcmp(m, "FMINNMP"))
		return ORLIX_TCTI_SIMD_ARITH_FMINNMP;
	if (!strcmp(m, "FABS"))
		return ORLIX_TCTI_SIMD_ARITH_FABS;
	if (!strcmp(m, "FNEG"))
		return ORLIX_TCTI_SIMD_ARITH_FNEG;
	if (!strcmp(m, "FSQRT"))
		return ORLIX_TCTI_SIMD_ARITH_FSQRT;
	if (!strcmp(m, "FRECPE"))
		return ORLIX_TCTI_SIMD_ARITH_FRECPE;
	if (!strcmp(m, "FRECPX"))
		return ORLIX_TCTI_SIMD_ARITH_FRECPX;
	if (!strcmp(m, "FRSQRTE"))
		return ORLIX_TCTI_SIMD_ARITH_FRSQRTE;
	if (!strcmp(m, "FRINTN"))
		return ORLIX_TCTI_SIMD_ARITH_FRINTN;
	if (!strcmp(m, "FRINTP"))
		return ORLIX_TCTI_SIMD_ARITH_FRINTP;
	if (!strcmp(m, "FRINTM"))
		return ORLIX_TCTI_SIMD_ARITH_FRINTM;
	if (!strcmp(m, "FRINTZ"))
		return ORLIX_TCTI_SIMD_ARITH_FRINTZ;
	if (!strcmp(m, "FRINTA"))
		return ORLIX_TCTI_SIMD_ARITH_FRINTA;
	if (!strcmp(m, "FRINTX"))
		return ORLIX_TCTI_SIMD_ARITH_FRINTX;
	if (!strcmp(m, "FRINTI"))
		return ORLIX_TCTI_SIMD_ARITH_FRINTI;
	if (!strcmp(m, "FCVTN"))
		return ORLIX_TCTI_SIMD_ARITH_FCVTN;
	if (!strcmp(m, "FCVTL"))
		return ORLIX_TCTI_SIMD_ARITH_FCVTL;
	if (!strcmp(m, "FCVTXN"))
		return ORLIX_TCTI_SIMD_ARITH_FCVTXN;
	if (!strcmp(m, "FCMEQ"))
		return zero ? ORLIX_TCTI_SIMD_ARITH_FCMEQ_ZERO :
			ORLIX_TCTI_SIMD_ARITH_FCMEQ;
	if (!strcmp(m, "FCMGE"))
		return zero ? ORLIX_TCTI_SIMD_ARITH_FCMGE_ZERO :
			ORLIX_TCTI_SIMD_ARITH_FCMGE;
	if (!strcmp(m, "FCMGT"))
		return zero ? ORLIX_TCTI_SIMD_ARITH_FCMGT_ZERO :
			ORLIX_TCTI_SIMD_ARITH_FCMGT;
	if (!strcmp(m, "FCMLE"))
		return ORLIX_TCTI_SIMD_ARITH_FCMLE_ZERO;
	if (!strcmp(m, "FCMLT"))
		return ORLIX_TCTI_SIMD_ARITH_FCMLT_ZERO;
	return -EINVAL;
}

static int orlix_tcti_advsimd_fp_convert_from_source(
	const struct orlix_tcti_test_fp_source *source, bool fixed)
{
	const char *m = source->mnemonic;

	if (!strcmp(m, "SCVTF"))
		return fixed ? ORLIX_TCTI_FP_INT_SCVTF_FIXED_SIMD :
			ORLIX_TCTI_FP_INT_SCVTF_SIMD;
	if (!strcmp(m, "UCVTF"))
		return fixed ? ORLIX_TCTI_FP_INT_UCVTF_FIXED_SIMD :
			ORLIX_TCTI_FP_INT_UCVTF_SIMD;
	if (!strcmp(m, "FCVTZS"))
		return fixed ? ORLIX_TCTI_FP_INT_FCVTZS_FIXED_SIMD :
			ORLIX_TCTI_FP_INT_FCVTZS_SIMD;
	if (!strcmp(m, "FCVTZU"))
		return fixed ? ORLIX_TCTI_FP_INT_FCVTZU_FIXED_SIMD :
			ORLIX_TCTI_FP_INT_FCVTZU_SIMD;
	if (!strcmp(m, "FCVTNS"))
		return ORLIX_TCTI_FP_INT_FCVTNS_SIMD;
	if (!strcmp(m, "FCVTNU"))
		return ORLIX_TCTI_FP_INT_FCVTNU_SIMD;
	if (!strcmp(m, "FCVTPS"))
		return ORLIX_TCTI_FP_INT_FCVTPS_SIMD;
	if (!strcmp(m, "FCVTPU"))
		return ORLIX_TCTI_FP_INT_FCVTPU_SIMD;
	if (!strcmp(m, "FCVTMS"))
		return ORLIX_TCTI_FP_INT_FCVTMS_SIMD;
	if (!strcmp(m, "FCVTMU"))
		return ORLIX_TCTI_FP_INT_FCVTMU_SIMD;
	if (!strcmp(m, "FCVTAS"))
		return ORLIX_TCTI_FP_INT_FCVTAS_SIMD;
	if (!strcmp(m, "FCVTAU"))
		return ORLIX_TCTI_FP_INT_FCVTAU_SIMD;
	return -EINVAL;
}

static int orlix_tcti_advsimd_fp_reduction_from_source(
	const struct orlix_tcti_test_fp_source *source)
{
	const char *m = source->mnemonic;

	if (!strcmp(m, "FADDP"))
		return ORLIX_TCTI_SIMD_REDUCTION_FADDP;
	if (!strcmp(m, "FMAXP"))
		return ORLIX_TCTI_SIMD_REDUCTION_FMAXP;
	if (!strcmp(m, "FMINP"))
		return ORLIX_TCTI_SIMD_REDUCTION_FMINP;
	if (!strcmp(m, "FMAXNMP"))
		return ORLIX_TCTI_SIMD_REDUCTION_FMAXNMP;
	if (!strcmp(m, "FMINNMP"))
		return ORLIX_TCTI_SIMD_REDUCTION_FMINNMP;
	if (!strcmp(m, "FMAXNMV"))
		return ORLIX_TCTI_SIMD_REDUCTION_FMAXNMV;
	if (!strcmp(m, "FMAXV"))
		return ORLIX_TCTI_SIMD_REDUCTION_FMAXV;
	if (!strcmp(m, "FMINNMV"))
		return ORLIX_TCTI_SIMD_REDUCTION_FMINNMV;
	if (!strcmp(m, "FMINV"))
		return ORLIX_TCTI_SIMD_REDUCTION_FMINV;
	return -EINVAL;
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
	int op;
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
		op = orlix_tcti_advsimd_fp_reduction_from_source(source);
		if (op < 0)
			return op;
		ret = orlix_tcti_native_simd_fp_reduction(op, access, expected_rd,
							  left, fpcr, expected_fpsr);
		if (!ret && !q && !scalar)
			expected_rd[1] = 0;
		return ret;
	}
	if (strstr(name, "asimdelem") || strstr(name, "asisdelem")) {
		u8 h = (instruction >> 11) & 1U;
		u8 l = (instruction >> 21) & 1U;
		u8 index = sz ? h : ((h << 1) | l);

		orlix_tcti_advsimd_fp_broadcast_lane(right, rm, access, index,
						     result);
		op = orlix_tcti_advsimd_fp_arith_from_source(source);
		if (op < 0)
			return op;
		return orlix_tcti_native_simd_fp_three_same(op, scalar, access,
			result, expected_rd, left, right, acc, fpcr, expected_fpsr);
	}
	if (strstr(name, "asimdshf") || strstr(name, "asisdshf")) {
		u32 immh = (instruction >> 19) & 0xfU;
		u32 immb = (instruction >> 16) & 0x7U;
		u32 imm = (immh << 3) | immb;
		u8 esize = (immh & 8U) ? 64 : (immh & 4U) ? 32 : 16;
		u8 fbits = (u8)(esize * 2U - imm);

		access = esize / 8U;
		op = orlix_tcti_advsimd_fp_convert_from_source(source, true);
		if (op < 0)
			return op;
		return orlix_tcti_native_fixed_simd_fp_convert(op, scalar, q,
			access, fbits, expected_rd, left, fpcr, expected_fpsr);
	}
	if (!strcmp(source->mnemonic, "FCVTL"))
		return orlix_tcti_native_simd_fp_two_register(
			ORLIX_TCTI_SIMD_ARITH_FCVTL, sizeof(u32), 2 * sizeof(u64),
			q ? 1U : 0, 0, expected_rd, left, acc, fpcr,
			expected_fpsr);
	if (!strcmp(source->mnemonic, "FCVTN") ||
	    (!strcmp(source->mnemonic, "FCVTXN") && !scalar))
		return orlix_tcti_native_simd_fp_two_register(
			!strcmp(source->mnemonic, "FCVTN") ?
				ORLIX_TCTI_SIMD_ARITH_FCVTN :
				ORLIX_TCTI_SIMD_ARITH_FCVTXN,
			sizeof(u64), q ? 2 * sizeof(u64) : sizeof(u64), 0,
			q ? 1U : 0, expected_rd, left, acc, fpcr, expected_fpsr);
	if (!strcmp(source->mnemonic, "FCVTXN"))
		return orlix_tcti_native_simd_fp_scalar_unary(
			ORLIX_TCTI_SIMD_ARITH_FCVTXN, sizeof(u64), sizeof(u32),
			expected_rd, left, fpcr, expected_fpsr);
	op = orlix_tcti_advsimd_fp_convert_from_source(source, false);
	if (op >= 0)
		return orlix_tcti_native_simd_fp_convert(op, scalar, q, access,
			expected_rd, left, fpcr, expected_fpsr);
	op = orlix_tcti_advsimd_fp_arith_from_source(source);
	if (op < 0)
		return op;
	if (op == ORLIX_TCTI_SIMD_ARITH_FRECPX || strstr(name, "asisdmisc"))
		return orlix_tcti_native_simd_fp_scalar_unary(op, access, result,
			expected_rd, left, fpcr, expected_fpsr);
	if (strstr(name, "asimdmisc") ||
	    op == ORLIX_TCTI_SIMD_ARITH_FABS ||
	    op == ORLIX_TCTI_SIMD_ARITH_FNEG ||
	    op == ORLIX_TCTI_SIMD_ARITH_FSQRT ||
	    op == ORLIX_TCTI_SIMD_ARITH_FRECPE ||
	    op == ORLIX_TCTI_SIMD_ARITH_FRSQRTE ||
	    op == ORLIX_TCTI_SIMD_ARITH_FRINTN ||
	    op == ORLIX_TCTI_SIMD_ARITH_FRINTP ||
	    op == ORLIX_TCTI_SIMD_ARITH_FRINTM ||
	    op == ORLIX_TCTI_SIMD_ARITH_FRINTZ ||
	    op == ORLIX_TCTI_SIMD_ARITH_FRINTA ||
	    op == ORLIX_TCTI_SIMD_ARITH_FRINTX ||
	    op == ORLIX_TCTI_SIMD_ARITH_FRINTI ||
	    op == ORLIX_TCTI_SIMD_ARITH_FCMEQ_ZERO ||
	    op == ORLIX_TCTI_SIMD_ARITH_FCMGE_ZERO ||
	    op == ORLIX_TCTI_SIMD_ARITH_FCMGT_ZERO ||
	    op == ORLIX_TCTI_SIMD_ARITH_FCMLE_ZERO ||
	    op == ORLIX_TCTI_SIMD_ARITH_FCMLT_ZERO)
		return orlix_tcti_native_simd_fp_two_register(op, access, result,
			0, 0, expected_rd, left, acc, fpcr, expected_fpsr);
	return orlix_tcti_native_simd_fp_three_same(op, scalar, access, result,
		expected_rd, left, right, acc, fpcr, expected_fpsr);
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
