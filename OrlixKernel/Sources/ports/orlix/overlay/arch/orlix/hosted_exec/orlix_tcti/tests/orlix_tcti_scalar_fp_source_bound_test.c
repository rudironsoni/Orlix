// SPDX-License-Identifier: GPL-2.0-only
/*
 * Production-path proof for the 140 always-on EL0 SCALAR_FP leaves.
 * Leftover CORE scalar-FP semantics stays regression and does not receive
 * #120 close credit. The 126 optional FP16, FPRCVT, FRINTTS, JSCVT, and
 * BF16 leaves remain in the EL0 complete target as unclassified blockers.
 * Runtime rejection does not complete them. Those features are not advertised.
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
#include "orlix_tcti_scalar_fp_production_capture.h"
#include "target_native_proof_contract_private.h"
#include "target_proof_ingestion_private.h"

#define FP_SVC 0xd4000001U
#define FP_FAMILY_COUNT 266U
#define FP_EL0_COUNT 140U
#define FP_OPTIONAL_COUNT 126U
#define FP_RD 0U
#define FP_RN 1U
#define FP_RM 2U
#define FP_RA 3U
#define AARCH64_FPSR_IOC BIT(0)
#define AARCH64_FPCR_RMODE_POSINF BIT(22)
#define AARCH64_FPCR_RMODE_NEGINF BIT(23)
#define AARCH64_FPCR_RMODE_ZERO (BIT(22) | BIT(23))
#define AARCH64_FPCR_FZ BIT(24)
#define NZCV (PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT)
#define FP32_POS_INF 0x7f800000U
#define FP32_NEG_INF 0xff800000U
#define FP32_POS_ZERO 0x0U
#define FP32_NEG_ZERO 0x80000000U
#define FP32_MIN_SUBNORMAL 0x1U
#define FP32_ONE 0x3f800000U
#define FP32_TWO 0x40000000U
#define FP32_ONE_POINT_FIVE 0x3fc00000U
#define FP32_NEG_ONE_POINT_FIVE 0xbfc00000U
#define FP32_TWO_POINT_FIVE 0x40200000U
#define FP32_SNAN 0x7f800001U
#define FP32_QNAN 0x7fc00000U
#define FP32_FOUR 0x40800000U
#define FP32_NEG_ONE 0xbf800000U
#define FP32_FMA_A 0x3f800800U
#define FP32_NARROW_H 0x3f801000U
#define FP64_NARROW_S 0x3ff0000010000000ULL
#define FP64_NARROW_H 0x3ff0020000000000ULL
#define FP16_ONE 0x3c00U
#define FP16_ONE_POINT_FIVE 0x3e00U
#define FP16_TWO_POINT_FIVE 0x4100U
#define FP16_POS_INF 0x7c00U
#define FP16_NEG_ONE_POINT_FIVE 0xbe00U
#define FP64_ONE 0x3ff0000000000000ULL
#define FP64_TWO 0x4000000000000000ULL
#define FP64_ONE_POINT_FIVE 0x3ff8000000000000ULL
#define FP64_NEG_ONE_POINT_FIVE 0xbff8000000000000ULL
#define FP64_TWO_POINT_FIVE 0x4004000000000000ULL
#define FP64_POS_INF 0x7ff0000000000000ULL
#define FP64_QNAN 0x7ff8000000000000ULL
#define FP64_FOUR 0x4010000000000000ULL
#define FP64_THREE 0x4008000000000000ULL
#define FP64_MIN_SUBNORMAL 0x1ULL
#define FP32_THREE 0x40400000U
#define FP64_NEG_ONE 0xbff0000000000000ULL
#define FP64_FMA_A 0x3ff0000002000000ULL
#define CONVERT_GPR_NEG8 0xfffffffffffffff8ULL
#define CONVERT_GPR_I32_MIN 0xffffffff80000000ULL
#define CONVERT_GPR_S_INEXACT 0x1000001ULL
#define CONVERT_GPR_D_INEXACT ((1ULL << 53) + 1ULL)

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

static bool orlix_tcti_test_is_scalar_fp(
	const struct orlix_tcti_test_fp_source *source)
{
	return source->ordinal < ARRAY_SIZE(orlix_tcti_test_source_families) &&
		orlix_tcti_test_source_families[source->ordinal] ==
			ORLIX_TCTI_TEST_SOURCE_FAMILY_SCALAR_FP;
}

static bool orlix_tcti_test_is_optional_fp(
	const struct orlix_tcti_test_fp_source *source)
{
	return orlix_tcti_scalar_fp_optional_ordinal(source->ordinal);
}

static bool orlix_tcti_test_is_el0_fp(
	const struct orlix_tcti_test_fp_source *source)
{
	return orlix_tcti_test_is_scalar_fp(source) &&
		!orlix_tcti_test_is_optional_fp(source);
}

static u32 orlix_tcti_scalar_fp_dest_obligation(
	const struct orlix_tcti_test_fp_source *source)
{
	const char *name = source->name;

	if (strstr(name, "floatcmp") || strstr(name, "floatccmp"))
		return ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS;
	if (!strncmp(name, "FMOV_32", 7) || !strncmp(name, "FMOV_64D", 8) ||
	    !strncmp(name, "FMOV_64VX", 9))
		return ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS;
	if (!strncmp(name, "FCVTZS_", 7) || !strncmp(name, "FCVTZU_", 7) ||
	    !strncmp(name, "FCVTNS_", 7) || !strncmp(name, "FCVTNU_", 7) ||
	    !strncmp(name, "FCVTAS_", 7) || !strncmp(name, "FCVTAU_", 7) ||
	    !strncmp(name, "FCVTPS_", 7) || !strncmp(name, "FCVTPU_", 7) ||
	    !strncmp(name, "FCVTMS_", 7) || !strncmp(name, "FCVTMU_", 7)) {
		const char *dest = name + 7;

		if (!strncmp(dest, "32", 2) || !strncmp(dest, "64", 2))
			return ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS;
	}
	return ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD;
}

static bool orlix_tcti_scalar_fp_gpr_rn(
	const struct orlix_tcti_test_fp_source *source)
{
	const char *name;

	if (!source)
		return false;
	if (!strcmp(source->mnemonic, "SCVTF") ||
	    !strcmp(source->mnemonic, "UCVTF"))
		return true;
	if (strcmp(source->mnemonic, "FMOV"))
		return false;
	name = source->name;
	return strstr(name, "S32") || strstr(name, "D64") ||
		strstr(name, "V64I") || strstr(name, "H32") ||
		strstr(name, "H64");
}

static const struct orlix_tcti_test_fp_source *
orlix_tcti_test_fp_name(const char *name)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++)
		if (orlix_tcti_test_is_scalar_fp(&orlix_tcti_test_sources[index]) &&
		    !strcmp(orlix_tcti_test_sources[index].name, name))
			return &orlix_tcti_test_sources[index];
	return NULL;
}

static u32 orlix_tcti_scalar_fp_legal_instruction(
	const struct orlix_tcti_test_fp_source *source)
{
	u32 instruction = source->pattern & source->mask;

	instruction |= FP_RD;
	if (!strstr(source->name, "floatimm"))
		instruction |= FP_RN << 5;
	if (!strstr(source->name, "floatimm") &&
	    (source->mask & (0x1fU << 16)) == 0)
		instruction |= FP_RM << 16;
	if (strstr(source->name, "floatdp3") &&
	    (source->mask & (0x1fU << 10)) == 0)
		instruction |= FP_RA << 10;
	/* 32-bit float2fix requires scale<5>=1. scale=32 gives fbits=32. */
	if (strstr(source->name, "float2fix") &&
	    !(instruction & BIT(31)) &&
	    (source->mask & (0x3fU << 10)) == 0)
		instruction |= BIT(15);
	return instruction;
}

static u8 orlix_tcti_scalar_fp_rd_index(u32 instruction)
{
	return instruction & 0x1fU;
}

static u8 orlix_tcti_scalar_fp_rn_index(u32 instruction)
{
	return (instruction >> 5) & 0x1fU;
}

static u8 orlix_tcti_scalar_fp_rm_index(u32 instruction)
{
	return (instruction >> 16) & 0x1fU;
}

static u8 orlix_tcti_scalar_fp_ra_index(u32 instruction)
{
	return (instruction >> 10) & 0x1fU;
}

static u32 orlix_tcti_scalar_fp_instruction_with_imm8(
	const struct orlix_tcti_test_fp_source *source, u8 imm8)
{
	u32 instruction = orlix_tcti_scalar_fp_legal_instruction(source);

	if (!strstr(source->name, "floatimm"))
		return instruction;
	instruction &= ~(0xffU << 13);
	instruction |= ((u32)imm8) << 13;
	return instruction;
}

static u32 orlix_tcti_scalar_fp_instruction_with_cond(
	const struct orlix_tcti_test_fp_source *source, u8 cond, u8 nzcv)
{
	u32 instruction = orlix_tcti_scalar_fp_legal_instruction(source);

	instruction &= ~(0xfU << 12);
	instruction |= ((u32)cond & 0xfU) << 12;
	if (strstr(source->name, "floatccmp")) {
		instruction &= ~0xfU;
		instruction |= nzcv & 0xfU;
	}
	return instruction;
}

static u32 orlix_tcti_scalar_fp_instruction_with_fbits(
	const struct orlix_tcti_test_fp_source *source, u8 fbits)
{
	u32 instruction = orlix_tcti_scalar_fp_legal_instruction(source);
	u8 scale;

	if (!strstr(source->name, "float2fix") || fbits == 0 || fbits > 64)
		return instruction;
	scale = 64U - fbits;
	instruction &= ~(0x3fU << 10);
	instruction |= ((u32)scale & 0x3fU) << 10;
	return instruction;
}

static bool orlix_tcti_test_fp_decode_is_family(u32 decode_class)
{
	return decode_class == ORLIX_TCTI_DECODE_FP_SCALAR_IMMEDIATE ||
		decode_class == ORLIX_TCTI_DECODE_FP_SCALAR_MOVE ||
		decode_class == ORLIX_TCTI_DECODE_FP_SCALAR_1SOURCE ||
		decode_class == ORLIX_TCTI_DECODE_FP_SCALAR_2SOURCE ||
		decode_class == ORLIX_TCTI_DECODE_FP_SCALAR_3SOURCE ||
		decode_class == ORLIX_TCTI_DECODE_FP_SCALAR_COMPARE ||
		decode_class == ORLIX_TCTI_DECODE_FP_CONDITIONAL_SELECT ||
		decode_class == ORLIX_TCTI_DECODE_FP_INT_CONVERT;
}

static bool orlix_tcti_scalar_fp_instruction_matches_source(
	const struct orlix_tcti_test_fp_source *source, u32 instruction)
{
	struct orlix_tcti_decoded_instruction decoded;

	if ((instruction & source->mask) != source->pattern)
		return false;
	decoded = orlix_tcti_decode_aarch64(instruction);
	return decoded.source_ordinal == source->ordinal &&
		orlix_tcti_test_fp_decode_is_family(decoded.decode_class);
}

static int orlix_tcti_scalar_fp_test_init(struct kunit *test)
{
	struct mm_struct *mm = mm_alloc();

	if (!mm)
		return -ENOMEM;
	kthread_use_mm(mm);
	test->priv = mm;
	return 0;
}

static void orlix_tcti_scalar_fp_test_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (!mm)
		return;
	kthread_unuse_mm(mm);
	mmput(mm);
}

static unsigned long orlix_tcti_scalar_fp_map(struct kunit *test, u32 instruction)
{
	u8 bytes[16] = {};
	u32 program[2] = { instruction, FP_SVC };

	memcpy(bytes, program, sizeof(program));
	return orlix_tcti_memory_proof_map_bytes(test, bytes, sizeof(bytes),
						 PROT_READ | PROT_EXEC);
}

static bool orlix_tcti_scalar_fp_is_double(
	const struct orlix_tcti_test_fp_source *source);

static u64 orlix_tcti_scalar_fp_rn_seed_bits(
	const struct orlix_tcti_test_fp_source *source)
{
	const char *name;

	if (!source)
		return FP32_ONE;
	name = source->name;
	if (!strcmp(source->mnemonic, "FCVT")) {
		if (strstr(name, "_DS") || strstr(name, "_HS"))
			return FP32_ONE;
		if (strstr(name, "_SD") || strstr(name, "_HD"))
			return FP64_ONE;
		if (strstr(name, "_SH") || strstr(name, "_DH"))
			return FP16_ONE;
	}
	return orlix_tcti_scalar_fp_is_double(source) ? FP64_ONE : FP32_ONE;
}

static u64 orlix_tcti_scalar_fp_lane_bits(
	const struct orlix_tcti_test_fp_source *source, u64 f32, u64 f64)
{
	return orlix_tcti_scalar_fp_is_double(source) ? f64 : f32;
}

static void orlix_tcti_scalar_fp_seed_simd(void)
{
	size_t index;

	current->thread.user_simd_valid = 1;
	current->thread.user_fpcr = 0;
	current->thread.user_fpsr = 0;
	for (index = 0; index < ARRAY_SIZE(current->thread.user_simd); index++)
		current->thread.user_simd[index] =
			(index % 2U) ? 0 : (0x55550000ULL | (index / 2U));
}

static void orlix_tcti_scalar_fp_seed(
	const struct orlix_tcti_test_fp_source *source, struct pt_regs *regs,
	unsigned long code, u32 instruction)
{
	bool d = source && orlix_tcti_scalar_fp_is_double(source);
	const char *name = source ? source->name : NULL;
	bool has_rn = name && !strstr(name, "floatimm");
	bool has_rm = has_rn && source && (source->mask & (0x1fU << 16)) == 0;
	bool has_ra = name && strstr(name, "floatdp3") &&
		(source->mask & (0x1fU << 10)) == 0;
	u8 rd = orlix_tcti_scalar_fp_rd_index(instruction);
	u8 rn = orlix_tcti_scalar_fp_rn_index(instruction);
	u8 rm = orlix_tcti_scalar_fp_rm_index(instruction);
	u8 ra = orlix_tcti_scalar_fp_ra_index(instruction);

	memset(regs, 0, sizeof(*regs));
	regs->pc = code;
	regs->pstate = PSR_MODE_EL0t | PSR_Z_BIT;
	regs->syscallno = NO_SYSCALL;
	regs->sp = 0x1111111111111111ULL;
	if (rd != 31U)
		regs->regs[rd] = 0x1111111111111111ULL;
	if (has_rn && rn != 31U)
		regs->regs[rn] = 8;
	if (has_rm && rm != 31U)
		regs->regs[rm] = 3;
	if (has_ra && ra != 31U)
		regs->regs[ra] = 5;
	regs->regs[30] = 0x4444444444444444ULL;
	orlix_tcti_scalar_fp_seed_simd();
	if (has_rn)
		current->thread.user_simd[rn * 2U] =
			orlix_tcti_scalar_fp_rn_seed_bits(source);
	if (has_rm)
		current->thread.user_simd[rm * 2U] = d ? FP64_TWO : FP32_TWO;
	if (has_ra)
		current->thread.user_simd[ra * 2U] = d ? FP64_ONE : FP32_ONE;
	if (rd != rn && (!has_rm || rd != rm) && (!has_ra || rd != ra))
		current->thread.user_simd[rd * 2U] = 0;
}

static bool orlix_tcti_scalar_fp_is_double(
	const struct orlix_tcti_test_fp_source *source)
{
	const char *name = source->name;

	return strstr(name, "_D") || strstr(name, "D32") ||
		strstr(name, "D64") || strstr(name, "_64D") ||
		strstr(name, "_32D") || strstr(name, "64VX") ||
		strstr(name, "V64I") || strstr(name, "_DZ") ||
		strstr(name, "_HD") || strstr(name, "_DH") ||
		strstr(name, "_DS");
}

static bool orlix_tcti_scalar_fp_is_convert(
	const struct orlix_tcti_test_fp_source *source)
{
	return strstr(source->name, "float2int") ||
		strstr(source->name, "float2fix");
}

static void orlix_tcti_scalar_fp_apply_convert_operands(
	const struct orlix_tcti_test_fp_source *source, struct pt_regs *regs,
	u64 fp_bits, u64 gpr_n, u32 instruction)
{
	u8 rn = orlix_tcti_scalar_fp_rn_index(instruction);

	if (!orlix_tcti_scalar_fp_is_convert(source) || !regs)
		return;
	if (rn != 31U)
		regs->regs[rn] = gpr_n;
	if (strstr(source->name, "64VX"))
		current->thread.user_simd[rn * 2U + 1U] = gpr_n;
	else
		current->thread.user_simd[rn * 2U] = fp_bits;
}

static u64 orlix_tcti_scalar_fp_src_lane_bits(
	const struct orlix_tcti_test_fp_source *source, u64 f32, u64 f64, u64 f16)
{
	const char *name;

	if (!source)
		return f32;
	name = source->name;
	if (!strcmp(source->mnemonic, "FCVT")) {
		if (strstr(name, "_DS") || strstr(name, "_HS"))
			return f32;
		if (strstr(name, "_SD") || strstr(name, "_HD"))
			return f64;
		if (strstr(name, "_SH") || strstr(name, "_DH"))
			return f16;
	}
	return orlix_tcti_scalar_fp_is_double(source) ? f64 : f32;
}

static u64 orlix_tcti_scalar_fp_convert_bits(
	const struct orlix_tcti_test_fp_source *source, u64 f32, u64 f64)
{
	return orlix_tcti_scalar_fp_src_lane_bits(source, f32, f64, FP16_ONE);
}

static u64 orlix_tcti_scalar_fp_read_gpr(const struct pt_regs *regs, u8 index)
{
	return index == 31U ? regs->sp : regs->regs[index];
}

static void orlix_tcti_scalar_fp_seed_convert(
	const struct orlix_tcti_test_fp_source *source, struct pt_regs *regs,
	unsigned long code, u32 instruction)
{
	u64 fp_bits;

	orlix_tcti_scalar_fp_seed(source, regs, code, instruction);
	fp_bits = orlix_tcti_scalar_fp_convert_bits(source,
		FP32_NEG_ONE_POINT_FIVE, FP64_NEG_ONE_POINT_FIVE);
	orlix_tcti_scalar_fp_apply_convert_operands(source, regs, fp_bits,
						    CONVERT_GPR_NEG8, instruction);
}

static int orlix_tcti_scalar_fp_host_wrap(int (*body)(void *), void *ctx,
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

struct orlix_tcti_scalar_fp_host_ctx {
	const struct orlix_tcti_test_fp_source *source;
	u32 instruction;
	const struct pt_regs *regs;
	const u64 *simd;
	u64 *out_simd;
	u64 *out_gpr;
	unsigned long *out_nzcv;
};

#define HOST_FIX_I2F(dreg, sreg, op, n, dstp, srcp, c1, c2) \
	asm volatile("ldr " sreg ", [%[src]]\n " op " " dreg ", " sreg ", #" #n \
		     "\n str " dreg ", [%[dst]]\n" \
		     : : [dst] "r" (dstp), [src] "r" (srcp) \
		     : c1, c2, "memory")
#define HOST_FIX_F2I(dreg, sreg, op, n, dstp, srcp, c1, c2) \
	asm volatile("ldr " sreg ", [%[src]]\n " op " " dreg ", " sreg ", #" #n \
		     "\n str " dreg ", [%[dst]]\n" \
		     : : [dst] "r" (dstp), [src] "r" (srcp) \
		     : c1, c2, "memory")

static int orlix_tcti_scalar_fp_host_fixed(
	const char *m, const char *name, u8 fbits, u64 *result, u64 *gpr_out,
	u64 left, u64 gpr_n)
{
	if (!strcmp(m, "SCVTF") && strstr(name, "S32_float2fix")) {
		switch (fbits) {
		case 1: HOST_FIX_I2F("s0", "w0", "scvtf", 1, result, &gpr_n, "w0", "v0"); break;
		case 16: HOST_FIX_I2F("s0", "w0", "scvtf", 16, result, &gpr_n, "w0", "v0"); break;
		case 32: HOST_FIX_I2F("s0", "w0", "scvtf", 32, result, &gpr_n, "w0", "v0"); break;
		default: return -EINVAL;
		}
	} else if (!strcmp(m, "UCVTF") && strstr(name, "S32_float2fix")) {
		switch (fbits) {
		case 1: HOST_FIX_I2F("s0", "w0", "ucvtf", 1, result, &gpr_n, "w0", "v0"); break;
		case 16: HOST_FIX_I2F("s0", "w0", "ucvtf", 16, result, &gpr_n, "w0", "v0"); break;
		case 32: HOST_FIX_I2F("s0", "w0", "ucvtf", 32, result, &gpr_n, "w0", "v0"); break;
		default: return -EINVAL;
		}
	} else if (!strcmp(m, "SCVTF") && strstr(name, "D32_float2fix")) {
		switch (fbits) {
		case 1: HOST_FIX_I2F("d0", "w0", "scvtf", 1, result, &gpr_n, "w0", "v0"); break;
		case 16: HOST_FIX_I2F("d0", "w0", "scvtf", 16, result, &gpr_n, "w0", "v0"); break;
		case 32: HOST_FIX_I2F("d0", "w0", "scvtf", 32, result, &gpr_n, "w0", "v0"); break;
		default: return -EINVAL;
		}
	} else if (!strcmp(m, "UCVTF") && strstr(name, "D32_float2fix")) {
		switch (fbits) {
		case 1: HOST_FIX_I2F("d0", "w0", "ucvtf", 1, result, &gpr_n, "w0", "v0"); break;
		case 16: HOST_FIX_I2F("d0", "w0", "ucvtf", 16, result, &gpr_n, "w0", "v0"); break;
		case 32: HOST_FIX_I2F("d0", "w0", "ucvtf", 32, result, &gpr_n, "w0", "v0"); break;
		default: return -EINVAL;
		}
	} else if (!strcmp(m, "SCVTF") && strstr(name, "S64_float2fix")) {
		switch (fbits) {
		case 1: HOST_FIX_I2F("s0", "x0", "scvtf", 1, result, &gpr_n, "x0", "v0"); break;
		case 32: HOST_FIX_I2F("s0", "x0", "scvtf", 32, result, &gpr_n, "x0", "v0"); break;
		case 64: HOST_FIX_I2F("s0", "x0", "scvtf", 64, result, &gpr_n, "x0", "v0"); break;
		default: return -EINVAL;
		}
	} else if (!strcmp(m, "UCVTF") && strstr(name, "S64_float2fix")) {
		switch (fbits) {
		case 1: HOST_FIX_I2F("s0", "x0", "ucvtf", 1, result, &gpr_n, "x0", "v0"); break;
		case 32: HOST_FIX_I2F("s0", "x0", "ucvtf", 32, result, &gpr_n, "x0", "v0"); break;
		case 64: HOST_FIX_I2F("s0", "x0", "ucvtf", 64, result, &gpr_n, "x0", "v0"); break;
		default: return -EINVAL;
		}
	} else if (!strcmp(m, "SCVTF") && strstr(name, "D64_float2fix")) {
		switch (fbits) {
		case 1: HOST_FIX_I2F("d0", "x0", "scvtf", 1, result, &gpr_n, "x0", "v0"); break;
		case 32: HOST_FIX_I2F("d0", "x0", "scvtf", 32, result, &gpr_n, "x0", "v0"); break;
		case 64: HOST_FIX_I2F("d0", "x0", "scvtf", 64, result, &gpr_n, "x0", "v0"); break;
		default: return -EINVAL;
		}
	} else if (!strcmp(m, "UCVTF") && strstr(name, "D64_float2fix")) {
		switch (fbits) {
		case 1: HOST_FIX_I2F("d0", "x0", "ucvtf", 1, result, &gpr_n, "x0", "v0"); break;
		case 32: HOST_FIX_I2F("d0", "x0", "ucvtf", 32, result, &gpr_n, "x0", "v0"); break;
		case 64: HOST_FIX_I2F("d0", "x0", "ucvtf", 64, result, &gpr_n, "x0", "v0"); break;
		default: return -EINVAL;
		}
	} else if (!strcmp(m, "FCVTZS") && strstr(name, "32S_float2fix")) {
		switch (fbits) {
		case 1: HOST_FIX_F2I("w0", "s0", "fcvtzs", 1, gpr_out, &left, "w0", "v0"); break;
		case 16: HOST_FIX_F2I("w0", "s0", "fcvtzs", 16, gpr_out, &left, "w0", "v0"); break;
		case 32: HOST_FIX_F2I("w0", "s0", "fcvtzs", 32, gpr_out, &left, "w0", "v0"); break;
		default: return -EINVAL;
		}
	} else if (!strcmp(m, "FCVTZU") && strstr(name, "32S_float2fix")) {
		switch (fbits) {
		case 1: HOST_FIX_F2I("w0", "s0", "fcvtzu", 1, gpr_out, &left, "w0", "v0"); break;
		case 16: HOST_FIX_F2I("w0", "s0", "fcvtzu", 16, gpr_out, &left, "w0", "v0"); break;
		case 32: HOST_FIX_F2I("w0", "s0", "fcvtzu", 32, gpr_out, &left, "w0", "v0"); break;
		default: return -EINVAL;
		}
	} else if (!strcmp(m, "FCVTZS") && strstr(name, "32D_float2fix")) {
		switch (fbits) {
		case 1: HOST_FIX_F2I("w0", "d0", "fcvtzs", 1, gpr_out, &left, "w0", "v0"); break;
		case 16: HOST_FIX_F2I("w0", "d0", "fcvtzs", 16, gpr_out, &left, "w0", "v0"); break;
		case 32: HOST_FIX_F2I("w0", "d0", "fcvtzs", 32, gpr_out, &left, "w0", "v0"); break;
		default: return -EINVAL;
		}
	} else if (!strcmp(m, "FCVTZU") && strstr(name, "32D_float2fix")) {
		switch (fbits) {
		case 1: HOST_FIX_F2I("w0", "d0", "fcvtzu", 1, gpr_out, &left, "w0", "v0"); break;
		case 16: HOST_FIX_F2I("w0", "d0", "fcvtzu", 16, gpr_out, &left, "w0", "v0"); break;
		case 32: HOST_FIX_F2I("w0", "d0", "fcvtzu", 32, gpr_out, &left, "w0", "v0"); break;
		default: return -EINVAL;
		}
	} else if (!strcmp(m, "FCVTZS") && strstr(name, "64S_float2fix")) {
		switch (fbits) {
		case 1: HOST_FIX_F2I("x0", "s0", "fcvtzs", 1, gpr_out, &left, "x0", "v0"); break;
		case 32: HOST_FIX_F2I("x0", "s0", "fcvtzs", 32, gpr_out, &left, "x0", "v0"); break;
		case 64: HOST_FIX_F2I("x0", "s0", "fcvtzs", 64, gpr_out, &left, "x0", "v0"); break;
		default: return -EINVAL;
		}
	} else if (!strcmp(m, "FCVTZU") && strstr(name, "64S_float2fix")) {
		switch (fbits) {
		case 1: HOST_FIX_F2I("x0", "s0", "fcvtzu", 1, gpr_out, &left, "x0", "v0"); break;
		case 32: HOST_FIX_F2I("x0", "s0", "fcvtzu", 32, gpr_out, &left, "x0", "v0"); break;
		case 64: HOST_FIX_F2I("x0", "s0", "fcvtzu", 64, gpr_out, &left, "x0", "v0"); break;
		default: return -EINVAL;
		}
	} else if (!strcmp(m, "FCVTZS") && strstr(name, "64D_float2fix")) {
		switch (fbits) {
		case 1: HOST_FIX_F2I("x0", "d0", "fcvtzs", 1, gpr_out, &left, "x0", "v0"); break;
		case 32: HOST_FIX_F2I("x0", "d0", "fcvtzs", 32, gpr_out, &left, "x0", "v0"); break;
		case 64: HOST_FIX_F2I("x0", "d0", "fcvtzs", 64, gpr_out, &left, "x0", "v0"); break;
		default: return -EINVAL;
		}
	} else if (!strcmp(m, "FCVTZU") && strstr(name, "64D_float2fix")) {
		switch (fbits) {
		case 1: HOST_FIX_F2I("x0", "d0", "fcvtzu", 1, gpr_out, &left, "x0", "v0"); break;
		case 32: HOST_FIX_F2I("x0", "d0", "fcvtzu", 32, gpr_out, &left, "x0", "v0"); break;
		case 64: HOST_FIX_F2I("x0", "d0", "fcvtzu", 64, gpr_out, &left, "x0", "v0"); break;
		default: return -EINVAL;
		}
	} else
		return -EINVAL;
	return 0;
}

static int orlix_tcti_scalar_fp_host_body(void *opaque)
{
	struct orlix_tcti_scalar_fp_host_ctx *c = opaque;
	const char *m = c->source->mnemonic;
	const char *name = c->source->name;
	bool d = orlix_tcti_scalar_fp_is_double(c->source);
	u8 rd = orlix_tcti_scalar_fp_rd_index(c->instruction);
	u8 rn = orlix_tcti_scalar_fp_rn_index(c->instruction);
	u8 rm = orlix_tcti_scalar_fp_rm_index(c->instruction);
	u8 ra = orlix_tcti_scalar_fp_ra_index(c->instruction);
	u64 left = c->simd[rn * 2U];
	u64 right = c->simd[rm * 2U];
	u64 acc = c->simd[ra * 2U];
	u64 result = 0;
	u64 gpr_n = (rn == 31U) ? 0 : c->regs->regs[rn];
	u64 gpr_out = 0;
	unsigned long nzcv = 0;
	u8 fbits;
	u8 scale = (c->instruction >> 10) & 0x3fU;

	if (strstr(name, "float2fix"))
		fbits = 64U - scale;
	else
		fbits = 0;

#define HOST_UN_S(insn) \
	asm volatile("ldr s0, [%[src]]\n" insn "\n str s0, [%[dst]]\n" \
		     : : [dst] "r" (&result), [src] "r" (&left) \
		     : "v0", "memory")
#define HOST_UN_D(insn) \
	asm volatile("ldr d0, [%[src]]\n" insn "\n str d0, [%[dst]]\n" \
		     : : [dst] "r" (&result), [src] "r" (&left) \
		     : "v0", "memory")
#define HOST_BIN_S(insn) \
	asm volatile("ldr s0, [%[l]]\n ldr s1, [%[r]]\n" insn \
		     "\n str s0, [%[dst]]\n" \
		     : : [dst] "r" (&result), [l] "r" (&left), [r] "r" (&right) \
		     : "v0", "v1", "memory")
#define HOST_BIN_D(insn) \
	asm volatile("ldr d0, [%[l]]\n ldr d1, [%[r]]\n" insn \
		     "\n str d0, [%[dst]]\n" \
		     : : [dst] "r" (&result), [l] "r" (&left), [r] "r" (&right) \
		     : "v0", "v1", "memory")
#define HOST_ACC_S(insn) \
	asm volatile("ldr s0, [%[l]]\n ldr s1, [%[r]]\n ldr s2, [%[a]]\n" \
		     insn "\n str s2, [%[dst]]\n" \
		     : : [dst] "r" (&result), [l] "r" (&left), \
			 [r] "r" (&right), [a] "r" (&acc) \
		     : "v0", "v1", "v2", "memory")
#define HOST_ACC_D(insn) \
	asm volatile("ldr d0, [%[l]]\n ldr d1, [%[r]]\n ldr d2, [%[a]]\n" \
		     insn "\n str d2, [%[dst]]\n" \
		     : : [dst] "r" (&result), [l] "r" (&left), \
			 [r] "r" (&right), [a] "r" (&acc) \
		     : "v0", "v1", "v2", "memory")

	if (!strcmp(m, "FADD")) {
		if (d) HOST_BIN_D("fadd d0, d0, d1");
		else HOST_BIN_S("fadd s0, s0, s1");
	} else if (!strcmp(m, "FSUB")) {
		if (d) HOST_BIN_D("fsub d0, d0, d1");
		else HOST_BIN_S("fsub s0, s0, s1");
	} else if (!strcmp(m, "FMUL")) {
		if (d) HOST_BIN_D("fmul d0, d0, d1");
		else HOST_BIN_S("fmul s0, s0, s1");
	} else if (!strcmp(m, "FDIV")) {
		if (d) HOST_BIN_D("fdiv d0, d0, d1");
		else HOST_BIN_S("fdiv s0, s0, s1");
	} else if (!strcmp(m, "FMAX")) {
		if (d) HOST_BIN_D("fmax d0, d0, d1");
		else HOST_BIN_S("fmax s0, s0, s1");
	} else if (!strcmp(m, "FMIN")) {
		if (d) HOST_BIN_D("fmin d0, d0, d1");
		else HOST_BIN_S("fmin s0, s0, s1");
	} else if (!strcmp(m, "FMAXNM")) {
		if (d) HOST_BIN_D("fmaxnm d0, d0, d1");
		else HOST_BIN_S("fmaxnm s0, s0, s1");
	} else if (!strcmp(m, "FMINNM")) {
		if (d) HOST_BIN_D("fminnm d0, d0, d1");
		else HOST_BIN_S("fminnm s0, s0, s1");
	} else if (!strcmp(m, "FNMUL")) {
		if (d) HOST_BIN_D("fnmul d0, d0, d1");
		else HOST_BIN_S("fnmul s0, s0, s1");
	} else if (!strcmp(m, "FMADD")) {
		if (d) HOST_ACC_D("fmadd d2, d0, d1, d2");
		else HOST_ACC_S("fmadd s2, s0, s1, s2");
	} else if (!strcmp(m, "FMSUB")) {
		if (d) HOST_ACC_D("fmsub d2, d0, d1, d2");
		else HOST_ACC_S("fmsub s2, s0, s1, s2");
	} else if (!strcmp(m, "FNMADD")) {
		if (d) HOST_ACC_D("fnmadd d2, d0, d1, d2");
		else HOST_ACC_S("fnmadd s2, s0, s1, s2");
	} else if (!strcmp(m, "FNMSUB")) {
		if (d) HOST_ACC_D("fnmsub d2, d0, d1, d2");
		else HOST_ACC_S("fnmsub s2, s0, s1, s2");
	} else if (!strcmp(m, "FABS")) {
		if (d) HOST_UN_D("fabs d0, d0");
		else HOST_UN_S("fabs s0, s0");
	} else if (!strcmp(m, "FNEG")) {
		if (d) HOST_UN_D("fneg d0, d0");
		else HOST_UN_S("fneg s0, s0");
	} else if (!strcmp(m, "FSQRT")) {
		if (d) HOST_UN_D("fsqrt d0, d0");
		else HOST_UN_S("fsqrt s0, s0");
	} else if (!strcmp(m, "FRINTN")) {
		if (d) HOST_UN_D("frintn d0, d0");
		else HOST_UN_S("frintn s0, s0");
	} else if (!strcmp(m, "FRINTP")) {
		if (d) HOST_UN_D("frintp d0, d0");
		else HOST_UN_S("frintp s0, s0");
	} else if (!strcmp(m, "FRINTM")) {
		if (d) HOST_UN_D("frintm d0, d0");
		else HOST_UN_S("frintm s0, s0");
	} else if (!strcmp(m, "FRINTZ")) {
		if (d) HOST_UN_D("frintz d0, d0");
		else HOST_UN_S("frintz s0, s0");
	} else if (!strcmp(m, "FRINTA")) {
		if (d) HOST_UN_D("frinta d0, d0");
		else HOST_UN_S("frinta s0, s0");
	} else if (!strcmp(m, "FRINTX")) {
		if (d) HOST_UN_D("frintx d0, d0");
		else HOST_UN_S("frintx s0, s0");
	} else if (!strcmp(m, "FRINTI")) {
		if (d) HOST_UN_D("frinti d0, d0");
		else HOST_UN_S("frinti s0, s0");
	} else if (!strcmp(m, "FCVT")) {
		if (strstr(name, "_DS"))
			asm volatile("ldr s0, [%[src]]\n fcvt d0, s0\n str d0, [%[dst]]\n"
				     : : [dst] "r" (&result), [src] "r" (&left)
				     : "v0", "memory");
		else if (strstr(name, "_SD"))
			asm volatile("ldr d0, [%[src]]\n fcvt s0, d0\n str s0, [%[dst]]\n"
				     : : [dst] "r" (&result), [src] "r" (&left)
				     : "v0", "memory");
		else if (strstr(name, "_HS"))
			asm volatile("ldr s0, [%[src]]\n fcvt h0, s0\n str h0, [%[dst]]\n"
				     : : [dst] "r" (&result), [src] "r" (&left)
				     : "v0", "memory");
		else if (strstr(name, "_HD"))
			asm volatile("ldr d0, [%[src]]\n fcvt h0, d0\n str h0, [%[dst]]\n"
				     : : [dst] "r" (&result), [src] "r" (&left)
				     : "v0", "memory");
		else if (strstr(name, "_SH"))
			asm volatile("ldr h0, [%[src]]\n fcvt s0, h0\n str s0, [%[dst]]\n"
				     : : [dst] "r" (&result), [src] "r" (&left)
				     : "v0", "memory");
		else if (strstr(name, "_DH"))
			asm volatile("ldr h0, [%[src]]\n fcvt d0, h0\n str d0, [%[dst]]\n"
				     : : [dst] "r" (&result), [src] "r" (&left)
				     : "v0", "memory");
		else
			return -EINVAL;
	} else if (!strcmp(m, "FMOV") && strstr(name, "floatdp1")) {
		if (d) HOST_UN_D("fmov d0, d0");
		else HOST_UN_S("fmov s0, s0");
	} else if (!strcmp(m, "FMOV") && strstr(name, "32S_float2int")) {
		asm volatile("ldr s0, [%[src]]\n fmov w0, s0\n str w0, [%[dst]]\n"
			     : : [dst] "r" (&gpr_out), [src] "r" (&left)
			     : "w0", "v0", "memory");
	} else if (!strcmp(m, "FMOV") && strstr(name, "S32")) {
		asm volatile("ldr w0, [%[src]]\n fmov s0, w0\n str s0, [%[dst]]\n"
			     : : [dst] "r" (&result), [src] "r" (&gpr_n)
			     : "w0", "v0", "memory");
	} else if (!strcmp(m, "FMOV") && strstr(name, "64D") &&
		   strstr(name, "float2int") && name[5] == '6') {
		asm volatile("ldr d0, [%[src]]\n fmov x0, d0\n str x0, [%[dst]]\n"
			     : : [dst] "r" (&gpr_out), [src] "r" (&left)
			     : "x0", "v0", "memory");
	} else if (!strcmp(m, "FMOV") && strstr(name, "D64")) {
		asm volatile("ldr x0, [%[src]]\n fmov d0, x0\n str d0, [%[dst]]\n"
			     : : [dst] "r" (&result), [src] "r" (&gpr_n)
			     : "x0", "v0", "memory");
	} else if (!strcmp(m, "FMOV") && strstr(name, "64VX")) {
		asm volatile("ldr q0, [%[src]]\n fmov x0, v0.d[1]\n str x0, [%[dst]]\n"
			     : : [dst] "r" (&gpr_out), [src] "r" (&c->simd[rn * 2U])
			     : "x0", "v0", "memory");
	} else if (!strcmp(m, "FMOV") && strstr(name, "V64I")) {
		u64 dest_q[2] = {
			c->simd[rd * 2U],
			c->simd[rd * 2U + 1U],
		};

		asm volatile("ldr q0, [%[dst]]\n ldr x0, [%[src]]\n"
			     "fmov v0.d[1], x0\n str q0, [%[dst]]\n"
			     : : [dst] "r" (dest_q), [src] "r" (&gpr_n)
			     : "x0", "v0", "memory");
		c->out_simd[0] = dest_q[0];
		c->out_simd[1] = dest_q[1];
		c->out_gpr[0] = orlix_tcti_scalar_fp_read_gpr(c->regs, rd);
		return 0;
	} else if (!strcmp(m, "FCSEL")) {
		unsigned long host_nzcv;
		u8 cond = (c->instruction >> 12) & 0xfU;

		asm volatile("mrs %0, nzcv\n" : "=r" (host_nzcv));
		if (cond == 0) {
			if (d)
				asm volatile("msr nzcv, %[nz]\n ldr d0, [%[l]]\n"
					     "ldr d1, [%[r]]\n fcsel d0, d0, d1, eq\n"
					     "str d0, [%[dst]]\n"
					     : : [nz] "r" (c->regs->pstate & NZCV),
						 [dst] "r" (&result), [l] "r" (&left),
						 [r] "r" (&right)
					     : "v0", "v1", "cc", "memory");
			else
				asm volatile("msr nzcv, %[nz]\n ldr s0, [%[l]]\n"
					     "ldr s1, [%[r]]\n fcsel s0, s0, s1, eq\n"
					     "str s0, [%[dst]]\n"
					     : : [nz] "r" (c->regs->pstate & NZCV),
						 [dst] "r" (&result), [l] "r" (&left),
						 [r] "r" (&right)
					     : "v0", "v1", "cc", "memory");
		} else if (cond == 1) {
			if (d)
				asm volatile("msr nzcv, %[nz]\n ldr d0, [%[l]]\n"
					     "ldr d1, [%[r]]\n fcsel d0, d0, d1, ne\n"
					     "str d0, [%[dst]]\n"
					     : : [nz] "r" (c->regs->pstate & NZCV),
						 [dst] "r" (&result), [l] "r" (&left),
						 [r] "r" (&right)
					     : "v0", "v1", "cc", "memory");
			else
				asm volatile("msr nzcv, %[nz]\n ldr s0, [%[l]]\n"
					     "ldr s1, [%[r]]\n fcsel s0, s0, s1, ne\n"
					     "str s0, [%[dst]]\n"
					     : : [nz] "r" (c->regs->pstate & NZCV),
						 [dst] "r" (&result), [l] "r" (&left),
						 [r] "r" (&right)
					     : "v0", "v1", "cc", "memory");
		} else if (cond == 2) {
			if (d)
				asm volatile("msr nzcv, %[nz]\n ldr d0, [%[l]]\n"
					     "ldr d1, [%[r]]\n fcsel d0, d0, d1, cs\n"
					     "str d0, [%[dst]]\n"
					     : : [nz] "r" (c->regs->pstate & NZCV),
						 [dst] "r" (&result), [l] "r" (&left),
						 [r] "r" (&right)
					     : "v0", "v1", "cc", "memory");
			else
				asm volatile("msr nzcv, %[nz]\n ldr s0, [%[l]]\n"
					     "ldr s1, [%[r]]\n fcsel s0, s0, s1, cs\n"
					     "str s0, [%[dst]]\n"
					     : : [nz] "r" (c->regs->pstate & NZCV),
						 [dst] "r" (&result), [l] "r" (&left),
						 [r] "r" (&right)
					     : "v0", "v1", "cc", "memory");
		} else if (cond == 4) {
			if (d)
				asm volatile("msr nzcv, %[nz]\n ldr d0, [%[l]]\n"
					     "ldr d1, [%[r]]\n fcsel d0, d0, d1, mi\n"
					     "str d0, [%[dst]]\n"
					     : : [nz] "r" (c->regs->pstate & NZCV),
						 [dst] "r" (&result), [l] "r" (&left),
						 [r] "r" (&right)
					     : "v0", "v1", "cc", "memory");
			else
				asm volatile("msr nzcv, %[nz]\n ldr s0, [%[l]]\n"
					     "ldr s1, [%[r]]\n fcsel s0, s0, s1, mi\n"
					     "str s0, [%[dst]]\n"
					     : : [nz] "r" (c->regs->pstate & NZCV),
						 [dst] "r" (&result), [l] "r" (&left),
						 [r] "r" (&right)
					     : "v0", "v1", "cc", "memory");
		} else if (cond == 6) {
			if (d)
				asm volatile("msr nzcv, %[nz]\n ldr d0, [%[l]]\n"
					     "ldr d1, [%[r]]\n fcsel d0, d0, d1, vs\n"
					     "str d0, [%[dst]]\n"
					     : : [nz] "r" (c->regs->pstate & NZCV),
						 [dst] "r" (&result), [l] "r" (&left),
						 [r] "r" (&right)
					     : "v0", "v1", "cc", "memory");
			else
				asm volatile("msr nzcv, %[nz]\n ldr s0, [%[l]]\n"
					     "ldr s1, [%[r]]\n fcsel s0, s0, s1, vs\n"
					     "str s0, [%[dst]]\n"
					     : : [nz] "r" (c->regs->pstate & NZCV),
						 [dst] "r" (&result), [l] "r" (&left),
						 [r] "r" (&right)
					     : "v0", "v1", "cc", "memory");
		} else
			return -EINVAL;
		asm volatile("msr nzcv, %0\n" : : "r" (host_nzcv));
	} else if (!strcmp(m, "FCMP") || !strcmp(m, "FCMPE") ||
		   !strcmp(m, "FCCMP") || !strcmp(m, "FCCMPE")) {
		unsigned long host_nzcv;
		bool zero = strstr(name, "SZ") || strstr(name, "DZ");

		asm volatile("mrs %0, nzcv\n" : "=r" (host_nzcv));
		if (!strcmp(m, "FCCMP") || !strcmp(m, "FCCMPE"))
			asm volatile("msr nzcv, %0\n"
				     : : "r" (c->regs->pstate & NZCV));
		if (!strcmp(m, "FCMP") && zero) {
			if (d)
				asm volatile("ldr d0, [%[l]]\n fcmp d0, #0.0\n mrs %[out], nzcv\n"
					     : [out] "=r" (nzcv)
					     : [l] "r" (&left) : "v0", "cc");
			else
				asm volatile("ldr s0, [%[l]]\n fcmp s0, #0.0\n mrs %[out], nzcv\n"
					     : [out] "=r" (nzcv)
					     : [l] "r" (&left) : "v0", "cc");
		} else if (!strcmp(m, "FCMPE") && zero) {
			if (d)
				asm volatile("ldr d0, [%[l]]\n fcmpe d0, #0.0\n mrs %[out], nzcv\n"
					     : [out] "=r" (nzcv)
					     : [l] "r" (&left) : "v0", "cc");
			else
				asm volatile("ldr s0, [%[l]]\n fcmpe s0, #0.0\n mrs %[out], nzcv\n"
					     : [out] "=r" (nzcv)
					     : [l] "r" (&left) : "v0", "cc");
		} else if (!strcmp(m, "FCMP")) {
			if (d)
				asm volatile("ldr d0, [%[l]]\n ldr d1, [%[r]]\n fcmp d0, d1\n mrs %[out], nzcv\n"
					     : [out] "=r" (nzcv)
					     : [l] "r" (&left), [r] "r" (&right)
					     : "v0", "v1", "cc");
			else
				asm volatile("ldr s0, [%[l]]\n ldr s1, [%[r]]\n fcmp s0, s1\n mrs %[out], nzcv\n"
					     : [out] "=r" (nzcv)
					     : [l] "r" (&left), [r] "r" (&right)
					     : "v0", "v1", "cc");
		} else if (!strcmp(m, "FCMPE")) {
			if (d)
				asm volatile("ldr d0, [%[l]]\n ldr d1, [%[r]]\n fcmpe d0, d1\n mrs %[out], nzcv\n"
					     : [out] "=r" (nzcv)
					     : [l] "r" (&left), [r] "r" (&right)
					     : "v0", "v1", "cc");
			else
				asm volatile("ldr s0, [%[l]]\n ldr s1, [%[r]]\n fcmpe s0, s1\n mrs %[out], nzcv\n"
					     : [out] "=r" (nzcv)
					     : [l] "r" (&left), [r] "r" (&right)
					     : "v0", "v1", "cc");
		} else if (!strcmp(m, "FCCMP") || !strcmp(m, "FCCMPE")) {
			unsigned long guest_nzcv = c->regs->pstate & NZCV;
			u8 cond = (c->instruction >> 12) & 0xfU;
			u8 nzcv_imm = c->instruction & 0xfU;
			bool signal = !strcmp(m, "FCCMPE");

			if ((cond > 1 && cond != 2 && cond != 4 && cond != 6) ||
			    (nzcv_imm != 0 && nzcv_imm != 1 && nzcv_imm != 2 &&
			     nzcv_imm != 4 && nzcv_imm != 8))
				return -EINVAL;
			if (d && !signal && cond == 0 && nzcv_imm == 0)
				asm volatile("msr nzcv, %[guest]\n ldr d0, [%[l]]\n"
					     "ldr d1, [%[r]]\n fccmp d0, d1, #0, eq\n"
					     "mrs %[out], nzcv\n"
					     : [out] "=r" (nzcv)
					     : [guest] "r" (guest_nzcv),
					       [l] "r" (&left), [r] "r" (&right)
					     : "v0", "v1", "cc", "memory");
			else if (d && !signal && cond == 0 && nzcv_imm == 8)
				asm volatile("msr nzcv, %[guest]\n ldr d0, [%[l]]\n"
					     "ldr d1, [%[r]]\n fccmp d0, d1, #8, eq\n"
					     "mrs %[out], nzcv\n"
					     : [out] "=r" (nzcv)
					     : [guest] "r" (guest_nzcv),
					       [l] "r" (&left), [r] "r" (&right)
					     : "v0", "v1", "cc", "memory");
			else if (d && !signal && cond == 1 && nzcv_imm == 0)
				asm volatile("msr nzcv, %[guest]\n ldr d0, [%[l]]\n"
					     "ldr d1, [%[r]]\n fccmp d0, d1, #0, ne\n"
					     "mrs %[out], nzcv\n"
					     : [out] "=r" (nzcv)
					     : [guest] "r" (guest_nzcv),
					       [l] "r" (&left), [r] "r" (&right)
					     : "v0", "v1", "cc", "memory");
			else if (d && !signal && cond == 1 && nzcv_imm == 8)
				asm volatile("msr nzcv, %[guest]\n ldr d0, [%[l]]\n"
					     "ldr d1, [%[r]]\n fccmp d0, d1, #8, ne\n"
					     "mrs %[out], nzcv\n"
					     : [out] "=r" (nzcv)
					     : [guest] "r" (guest_nzcv),
					       [l] "r" (&left), [r] "r" (&right)
					     : "v0", "v1", "cc", "memory");
			else if (d && signal && cond == 0 && nzcv_imm == 0)
				asm volatile("msr nzcv, %[guest]\n ldr d0, [%[l]]\n"
					     "ldr d1, [%[r]]\n fccmpe d0, d1, #0, eq\n"
					     "mrs %[out], nzcv\n"
					     : [out] "=r" (nzcv)
					     : [guest] "r" (guest_nzcv),
					       [l] "r" (&left), [r] "r" (&right)
					     : "v0", "v1", "cc", "memory");
			else if (d && signal && cond == 0 && nzcv_imm == 8)
				asm volatile("msr nzcv, %[guest]\n ldr d0, [%[l]]\n"
					     "ldr d1, [%[r]]\n fccmpe d0, d1, #8, eq\n"
					     "mrs %[out], nzcv\n"
					     : [out] "=r" (nzcv)
					     : [guest] "r" (guest_nzcv),
					       [l] "r" (&left), [r] "r" (&right)
					     : "v0", "v1", "cc", "memory");
			else if (d && signal && cond == 1 && nzcv_imm == 0)
				asm volatile("msr nzcv, %[guest]\n ldr d0, [%[l]]\n"
					     "ldr d1, [%[r]]\n fccmpe d0, d1, #0, ne\n"
					     "mrs %[out], nzcv\n"
					     : [out] "=r" (nzcv)
					     : [guest] "r" (guest_nzcv),
					       [l] "r" (&left), [r] "r" (&right)
					     : "v0", "v1", "cc", "memory");
			else if (d && signal && cond == 1 && nzcv_imm == 8)
				asm volatile("msr nzcv, %[guest]\n ldr d0, [%[l]]\n"
					     "ldr d1, [%[r]]\n fccmpe d0, d1, #8, ne\n"
					     "mrs %[out], nzcv\n"
					     : [out] "=r" (nzcv)
					     : [guest] "r" (guest_nzcv),
					       [l] "r" (&left), [r] "r" (&right)
					     : "v0", "v1", "cc", "memory");
			else if (!d && !signal && cond == 0 && nzcv_imm == 0)
				asm volatile("msr nzcv, %[guest]\n ldr s0, [%[l]]\n"
					     "ldr s1, [%[r]]\n fccmp s0, s1, #0, eq\n"
					     "mrs %[out], nzcv\n"
					     : [out] "=r" (nzcv)
					     : [guest] "r" (guest_nzcv),
					       [l] "r" (&left), [r] "r" (&right)
					     : "v0", "v1", "cc", "memory");
			else if (!d && !signal && cond == 0 && nzcv_imm == 8)
				asm volatile("msr nzcv, %[guest]\n ldr s0, [%[l]]\n"
					     "ldr s1, [%[r]]\n fccmp s0, s1, #8, eq\n"
					     "mrs %[out], nzcv\n"
					     : [out] "=r" (nzcv)
					     : [guest] "r" (guest_nzcv),
					       [l] "r" (&left), [r] "r" (&right)
					     : "v0", "v1", "cc", "memory");
			else if (!d && !signal && cond == 1 && nzcv_imm == 0)
				asm volatile("msr nzcv, %[guest]\n ldr s0, [%[l]]\n"
					     "ldr s1, [%[r]]\n fccmp s0, s1, #0, ne\n"
					     "mrs %[out], nzcv\n"
					     : [out] "=r" (nzcv)
					     : [guest] "r" (guest_nzcv),
					       [l] "r" (&left), [r] "r" (&right)
					     : "v0", "v1", "cc", "memory");
			else if (!d && !signal && cond == 1 && nzcv_imm == 8)
				asm volatile("msr nzcv, %[guest]\n ldr s0, [%[l]]\n"
					     "ldr s1, [%[r]]\n fccmp s0, s1, #8, ne\n"
					     "mrs %[out], nzcv\n"
					     : [out] "=r" (nzcv)
					     : [guest] "r" (guest_nzcv),
					       [l] "r" (&left), [r] "r" (&right)
					     : "v0", "v1", "cc", "memory");
			else if (!d && signal && cond == 0 && nzcv_imm == 0)
				asm volatile("msr nzcv, %[guest]\n ldr s0, [%[l]]\n"
					     "ldr s1, [%[r]]\n fccmpe s0, s1, #0, eq\n"
					     "mrs %[out], nzcv\n"
					     : [out] "=r" (nzcv)
					     : [guest] "r" (guest_nzcv),
					       [l] "r" (&left), [r] "r" (&right)
					     : "v0", "v1", "cc", "memory");
			else if (!d && signal && cond == 0 && nzcv_imm == 8)
				asm volatile("msr nzcv, %[guest]\n ldr s0, [%[l]]\n"
					     "ldr s1, [%[r]]\n fccmpe s0, s1, #8, eq\n"
					     "mrs %[out], nzcv\n"
					     : [out] "=r" (nzcv)
					     : [guest] "r" (guest_nzcv),
					       [l] "r" (&left), [r] "r" (&right)
					     : "v0", "v1", "cc", "memory");
			else if (!d && signal && cond == 1 && nzcv_imm == 0)
				asm volatile("msr nzcv, %[guest]\n ldr s0, [%[l]]\n"
					     "ldr s1, [%[r]]\n fccmpe s0, s1, #0, ne\n"
					     "mrs %[out], nzcv\n"
					     : [out] "=r" (nzcv)
					     : [guest] "r" (guest_nzcv),
					       [l] "r" (&left), [r] "r" (&right)
					     : "v0", "v1", "cc", "memory");
			else if (!d && signal && cond == 1 && nzcv_imm == 8)
				asm volatile("msr nzcv, %[guest]\n ldr s0, [%[l]]\n"
					     "ldr s1, [%[r]]\n fccmpe s0, s1, #8, ne\n"
					     "mrs %[out], nzcv\n"
					     : [out] "=r" (nzcv)
					     : [guest] "r" (guest_nzcv),
					       [l] "r" (&left), [r] "r" (&right)
					     : "v0", "v1", "cc", "memory");
#define HOST_FCCMP_ONE(op, dreg, imm, sfx) \
	asm volatile("msr nzcv, %[guest]\n ldr " dreg "0, [%[l]]\n" \
		     "ldr " dreg "1, [%[r]]\n " op " " dreg "0, " dreg "1, #" #imm \
		     ", " sfx "\n mrs %[out], nzcv\n" \
		     : [out] "=r" (nzcv) \
		     : [guest] "r" (guest_nzcv), \
		       [l] "r" (&left), [r] "r" (&right) \
		     : "v0", "v1", "cc", "memory")
			else if (d && !signal && cond == 2 && nzcv_imm == 0)
				HOST_FCCMP_ONE("fccmp", "d", 0, "cs");
			else if (d && !signal && cond == 4 && nzcv_imm == 0)
				HOST_FCCMP_ONE("fccmp", "d", 0, "mi");
			else if (d && !signal && cond == 6 && nzcv_imm == 0)
				HOST_FCCMP_ONE("fccmp", "d", 0, "vs");
			else if (d && !signal && cond == 1 && nzcv_imm == 4)
				HOST_FCCMP_ONE("fccmp", "d", 4, "ne");
			else if (d && !signal && cond == 1 && nzcv_imm == 2)
				HOST_FCCMP_ONE("fccmp", "d", 2, "ne");
			else if (d && !signal && cond == 1 && nzcv_imm == 1)
				HOST_FCCMP_ONE("fccmp", "d", 1, "ne");
			else if (d && signal && cond == 2 && nzcv_imm == 0)
				HOST_FCCMP_ONE("fccmpe", "d", 0, "cs");
			else if (d && signal && cond == 4 && nzcv_imm == 0)
				HOST_FCCMP_ONE("fccmpe", "d", 0, "mi");
			else if (d && signal && cond == 6 && nzcv_imm == 0)
				HOST_FCCMP_ONE("fccmpe", "d", 0, "vs");
			else if (d && signal && cond == 1 && nzcv_imm == 4)
				HOST_FCCMP_ONE("fccmpe", "d", 4, "ne");
			else if (d && signal && cond == 1 && nzcv_imm == 2)
				HOST_FCCMP_ONE("fccmpe", "d", 2, "ne");
			else if (d && signal && cond == 1 && nzcv_imm == 1)
				HOST_FCCMP_ONE("fccmpe", "d", 1, "ne");
			else if (!d && !signal && cond == 2 && nzcv_imm == 0)
				HOST_FCCMP_ONE("fccmp", "s", 0, "cs");
			else if (!d && !signal && cond == 4 && nzcv_imm == 0)
				HOST_FCCMP_ONE("fccmp", "s", 0, "mi");
			else if (!d && !signal && cond == 6 && nzcv_imm == 0)
				HOST_FCCMP_ONE("fccmp", "s", 0, "vs");
			else if (!d && !signal && cond == 1 && nzcv_imm == 4)
				HOST_FCCMP_ONE("fccmp", "s", 4, "ne");
			else if (!d && !signal && cond == 1 && nzcv_imm == 2)
				HOST_FCCMP_ONE("fccmp", "s", 2, "ne");
			else if (!d && !signal && cond == 1 && nzcv_imm == 1)
				HOST_FCCMP_ONE("fccmp", "s", 1, "ne");
			else if (!d && signal && cond == 2 && nzcv_imm == 0)
				HOST_FCCMP_ONE("fccmpe", "s", 0, "cs");
			else if (!d && signal && cond == 4 && nzcv_imm == 0)
				HOST_FCCMP_ONE("fccmpe", "s", 0, "mi");
			else if (!d && signal && cond == 6 && nzcv_imm == 0)
				HOST_FCCMP_ONE("fccmpe", "s", 0, "vs");
			else if (!d && signal && cond == 1 && nzcv_imm == 4)
				HOST_FCCMP_ONE("fccmpe", "s", 4, "ne");
			else if (!d && signal && cond == 1 && nzcv_imm == 2)
				HOST_FCCMP_ONE("fccmpe", "s", 2, "ne");
			else if (!d && signal && cond == 1 && nzcv_imm == 1)
				HOST_FCCMP_ONE("fccmpe", "s", 1, "ne");
#undef HOST_FCCMP_ONE
			else
				return -EINVAL;
		}
		asm volatile("msr nzcv, %0\n" : : "r" (host_nzcv));
		*c->out_nzcv = nzcv & NZCV;
		c->out_simd[0] = c->simd[rd * 2U];
		c->out_simd[1] = c->simd[rd * 2U + 1U];
		c->out_gpr[0] = orlix_tcti_scalar_fp_read_gpr(c->regs, rd);
		return 0;
	} else if (!strcmp(m, "SCVTF") && strstr(name, "float2int")) {
		if (strstr(name, "S32"))
			asm volatile("ldr w0, [%[src]]\n scvtf s0, w0\n str s0, [%[dst]]\n"
				     : : [dst] "r" (&result), [src] "r" (&gpr_n)
				     : "w0", "v0", "memory");
		else if (strstr(name, "D32"))
			asm volatile("ldr w0, [%[src]]\n scvtf d0, w0\n str d0, [%[dst]]\n"
				     : : [dst] "r" (&result), [src] "r" (&gpr_n)
				     : "w0", "v0", "memory");
		else if (strstr(name, "S64"))
			asm volatile("ldr x0, [%[src]]\n scvtf s0, x0\n str s0, [%[dst]]\n"
				     : : [dst] "r" (&result), [src] "r" (&gpr_n)
				     : "x0", "v0", "memory");
		else
			asm volatile("ldr x0, [%[src]]\n scvtf d0, x0\n str d0, [%[dst]]\n"
				     : : [dst] "r" (&result), [src] "r" (&gpr_n)
				     : "x0", "v0", "memory");
	} else if (!strcmp(m, "UCVTF") && strstr(name, "float2int")) {
		if (strstr(name, "S32"))
			asm volatile("ldr w0, [%[src]]\n ucvtf s0, w0\n str s0, [%[dst]]\n"
				     : : [dst] "r" (&result), [src] "r" (&gpr_n)
				     : "w0", "v0", "memory");
		else if (strstr(name, "D32"))
			asm volatile("ldr w0, [%[src]]\n ucvtf d0, w0\n str d0, [%[dst]]\n"
				     : : [dst] "r" (&result), [src] "r" (&gpr_n)
				     : "w0", "v0", "memory");
		else if (strstr(name, "S64"))
			asm volatile("ldr x0, [%[src]]\n ucvtf s0, x0\n str s0, [%[dst]]\n"
				     : : [dst] "r" (&result), [src] "r" (&gpr_n)
				     : "x0", "v0", "memory");
		else
			asm volatile("ldr x0, [%[src]]\n ucvtf d0, x0\n str d0, [%[dst]]\n"
				     : : [dst] "r" (&result), [src] "r" (&gpr_n)
				     : "x0", "v0", "memory");
	} else if (strstr(name, "float2int") || strstr(name, "float2fix")) {
		/* Integer from FP: dest is GPR. Use the matching host convert. */
		if (!strcmp(m, "FCVTZS") && strstr(name, "32S_float2int"))
			asm volatile("ldr s0, [%[src]]\n fcvtzs w0, s0\n str w0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "w0", "v0", "memory");
		else if (!strcmp(m, "FCVTZU") && strstr(name, "32S_float2int"))
			asm volatile("ldr s0, [%[src]]\n fcvtzu w0, s0\n str w0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "w0", "v0", "memory");
		else if (!strcmp(m, "FCVTNS") && strstr(name, "32S_float2int"))
			asm volatile("ldr s0, [%[src]]\n fcvtns w0, s0\n str w0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "w0", "v0", "memory");
		else if (!strcmp(m, "FCVTNU") && strstr(name, "32S_float2int"))
			asm volatile("ldr s0, [%[src]]\n fcvtnu w0, s0\n str w0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "w0", "v0", "memory");
		else if (!strcmp(m, "FCVTAS") && strstr(name, "32S_float2int"))
			asm volatile("ldr s0, [%[src]]\n fcvtas w0, s0\n str w0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "w0", "v0", "memory");
		else if (!strcmp(m, "FCVTAU") && strstr(name, "32S_float2int"))
			asm volatile("ldr s0, [%[src]]\n fcvtau w0, s0\n str w0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "w0", "v0", "memory");
		else if (!strcmp(m, "FCVTPS") && strstr(name, "32S_float2int"))
			asm volatile("ldr s0, [%[src]]\n fcvtps w0, s0\n str w0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "w0", "v0", "memory");
		else if (!strcmp(m, "FCVTPU") && strstr(name, "32S_float2int"))
			asm volatile("ldr s0, [%[src]]\n fcvtpu w0, s0\n str w0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "w0", "v0", "memory");
		else if (!strcmp(m, "FCVTMS") && strstr(name, "32S_float2int"))
			asm volatile("ldr s0, [%[src]]\n fcvtms w0, s0\n str w0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "w0", "v0", "memory");
		else if (!strcmp(m, "FCVTMU") && strstr(name, "32S_float2int"))
			asm volatile("ldr s0, [%[src]]\n fcvtmu w0, s0\n str w0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "w0", "v0", "memory");
		else if (!strcmp(m, "FCVTZS") && strstr(name, "32D_float2int"))
			asm volatile("ldr d0, [%[src]]\n fcvtzs w0, d0\n str w0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "w0", "v0", "memory");
		else if (!strcmp(m, "FCVTZU") && strstr(name, "32D_float2int"))
			asm volatile("ldr d0, [%[src]]\n fcvtzu w0, d0\n str w0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "w0", "v0", "memory");
		else if (!strcmp(m, "FCVTNS") && strstr(name, "32D_float2int"))
			asm volatile("ldr d0, [%[src]]\n fcvtns w0, d0\n str w0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "w0", "v0", "memory");
		else if (!strcmp(m, "FCVTNU") && strstr(name, "32D_float2int"))
			asm volatile("ldr d0, [%[src]]\n fcvtnu w0, d0\n str w0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "w0", "v0", "memory");
		else if (!strcmp(m, "FCVTAS") && strstr(name, "32D_float2int"))
			asm volatile("ldr d0, [%[src]]\n fcvtas w0, d0\n str w0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "w0", "v0", "memory");
		else if (!strcmp(m, "FCVTAU") && strstr(name, "32D_float2int"))
			asm volatile("ldr d0, [%[src]]\n fcvtau w0, d0\n str w0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "w0", "v0", "memory");
		else if (!strcmp(m, "FCVTPS") && strstr(name, "32D_float2int"))
			asm volatile("ldr d0, [%[src]]\n fcvtps w0, d0\n str w0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "w0", "v0", "memory");
		else if (!strcmp(m, "FCVTPU") && strstr(name, "32D_float2int"))
			asm volatile("ldr d0, [%[src]]\n fcvtpu w0, d0\n str w0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "w0", "v0", "memory");
		else if (!strcmp(m, "FCVTMS") && strstr(name, "32D_float2int"))
			asm volatile("ldr d0, [%[src]]\n fcvtms w0, d0\n str w0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "w0", "v0", "memory");
		else if (!strcmp(m, "FCVTMU") && strstr(name, "32D_float2int"))
			asm volatile("ldr d0, [%[src]]\n fcvtmu w0, d0\n str w0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "w0", "v0", "memory");
		else if (!strcmp(m, "FCVTZS") && strstr(name, "64S_float2int"))
			asm volatile("ldr s0, [%[src]]\n fcvtzs x0, s0\n str x0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "x0", "v0", "memory");
		else if (!strcmp(m, "FCVTZU") && strstr(name, "64S_float2int"))
			asm volatile("ldr s0, [%[src]]\n fcvtzu x0, s0\n str x0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "x0", "v0", "memory");
		else if (!strcmp(m, "FCVTNS") && strstr(name, "64S_float2int"))
			asm volatile("ldr s0, [%[src]]\n fcvtns x0, s0\n str x0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "x0", "v0", "memory");
		else if (!strcmp(m, "FCVTNU") && strstr(name, "64S_float2int"))
			asm volatile("ldr s0, [%[src]]\n fcvtnu x0, s0\n str x0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "x0", "v0", "memory");
		else if (!strcmp(m, "FCVTAS") && strstr(name, "64S_float2int"))
			asm volatile("ldr s0, [%[src]]\n fcvtas x0, s0\n str x0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "x0", "v0", "memory");
		else if (!strcmp(m, "FCVTAU") && strstr(name, "64S_float2int"))
			asm volatile("ldr s0, [%[src]]\n fcvtau x0, s0\n str x0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "x0", "v0", "memory");
		else if (!strcmp(m, "FCVTPS") && strstr(name, "64S_float2int"))
			asm volatile("ldr s0, [%[src]]\n fcvtps x0, s0\n str x0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "x0", "v0", "memory");
		else if (!strcmp(m, "FCVTPU") && strstr(name, "64S_float2int"))
			asm volatile("ldr s0, [%[src]]\n fcvtpu x0, s0\n str x0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "x0", "v0", "memory");
		else if (!strcmp(m, "FCVTMS") && strstr(name, "64S_float2int"))
			asm volatile("ldr s0, [%[src]]\n fcvtms x0, s0\n str x0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "x0", "v0", "memory");
		else if (!strcmp(m, "FCVTMU") && strstr(name, "64S_float2int"))
			asm volatile("ldr s0, [%[src]]\n fcvtmu x0, s0\n str x0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "x0", "v0", "memory");
		else if (!strcmp(m, "FCVTZS") && strstr(name, "64D_float2int"))
			asm volatile("ldr d0, [%[src]]\n fcvtzs x0, d0\n str x0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "x0", "v0", "memory");
		else if (!strcmp(m, "FCVTZU") && strstr(name, "64D_float2int"))
			asm volatile("ldr d0, [%[src]]\n fcvtzu x0, d0\n str x0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "x0", "v0", "memory");
		else if (!strcmp(m, "FCVTNS") && strstr(name, "64D_float2int"))
			asm volatile("ldr d0, [%[src]]\n fcvtns x0, d0\n str x0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "x0", "v0", "memory");
		else if (!strcmp(m, "FCVTNU") && strstr(name, "64D_float2int"))
			asm volatile("ldr d0, [%[src]]\n fcvtnu x0, d0\n str x0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "x0", "v0", "memory");
		else if (!strcmp(m, "FCVTAS") && strstr(name, "64D_float2int"))
			asm volatile("ldr d0, [%[src]]\n fcvtas x0, d0\n str x0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "x0", "v0", "memory");
		else if (!strcmp(m, "FCVTAU") && strstr(name, "64D_float2int"))
			asm volatile("ldr d0, [%[src]]\n fcvtau x0, d0\n str x0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "x0", "v0", "memory");
		else if (!strcmp(m, "FCVTPS") && strstr(name, "64D_float2int"))
			asm volatile("ldr d0, [%[src]]\n fcvtps x0, d0\n str x0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "x0", "v0", "memory");
		else if (!strcmp(m, "FCVTPU") && strstr(name, "64D_float2int"))
			asm volatile("ldr d0, [%[src]]\n fcvtpu x0, d0\n str x0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "x0", "v0", "memory");
		else if (!strcmp(m, "FCVTMS") && strstr(name, "64D_float2int"))
			asm volatile("ldr d0, [%[src]]\n fcvtms x0, d0\n str x0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "x0", "v0", "memory");
		else if (!strcmp(m, "FCVTMU") && strstr(name, "64D_float2int"))
			asm volatile("ldr d0, [%[src]]\n fcvtmu x0, d0\n str x0, [%[dst]]\n"
				     : : [dst] "r" (&gpr_out), [src] "r" (&left)
				     : "x0", "v0", "memory");
		else if (orlix_tcti_scalar_fp_host_fixed(m, name, fbits, &result,
							 &gpr_out, left, gpr_n))
			return -EINVAL;
	} else
		return -EINVAL;

	c->out_simd[0] = result;
	c->out_simd[1] = 0;
	c->out_gpr[0] = (rd == 31U) ? c->regs->sp : gpr_out;
	*c->out_nzcv = nzcv;
	return 0;
}

static int orlix_tcti_scalar_fp_expected_from_source(
	const struct orlix_tcti_test_fp_source *source, u32 instruction,
	const struct pt_regs *regs, const u64 *simd, unsigned long fpcr,
	unsigned long fpsr_in, u64 expected_simd[2], u64 *expected_gpr,
	unsigned long *expected_nzcv, unsigned long *expected_fpsr)
{
	struct orlix_tcti_scalar_fp_host_ctx ctx = {
		.source = source,
		.instruction = instruction,
		.regs = regs,
		.simd = simd,
		.out_simd = expected_simd,
		.out_gpr = expected_gpr,
		.out_nzcv = expected_nzcv,
	};

	expected_simd[0] = simd[orlix_tcti_scalar_fp_rd_index(instruction) * 2U];
	expected_simd[1] = simd[orlix_tcti_scalar_fp_rd_index(instruction) * 2U + 1U];
	*expected_gpr = orlix_tcti_scalar_fp_read_gpr(regs,
		orlix_tcti_scalar_fp_rd_index(instruction));
	*expected_nzcv = regs->pstate & NZCV;
	*expected_fpsr = fpsr_in;
	if (strstr(source->name, "floatimm")) {
		u8 imm8 = (instruction >> 13) & 0xffU;
		u32 sign = (imm8 >> 7) & 1U;
		u32 exponent_bit = (imm8 >> 6) & 1U;
		u32 fraction = imm8 & 0x3fU;

		if (orlix_tcti_scalar_fp_is_double(source)) {
			expected_simd[0] = ((u64)sign << 63) |
				((u64)!exponent_bit << 62) |
				((exponent_bit ? 0xffULL : 0) << 54) |
				((u64)fraction << 48);
			expected_simd[1] = 0;
		} else {
			expected_simd[0] = (sign << 31) | ((!exponent_bit) << 30) |
				((exponent_bit ? 0x1fU : 0) << 25) |
				(fraction << 19);
			expected_simd[1] = 0;
		}
		return 0;
	}
	return orlix_tcti_scalar_fp_host_wrap(orlix_tcti_scalar_fp_host_body,
					      &ctx, fpcr, expected_fpsr);
}

static void orlix_tcti_scalar_fp_assert_success(struct kunit *test,
	const struct orlix_tcti_test_fp_source *source,
	const struct orlix_tcti_result *result, const struct pt_regs *regs,
	unsigned long code)
{
	KUNIT_ASSERT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result->reason,
			    "%s ordinal %u reason", source->name, source->ordinal);
	KUNIT_ASSERT_EQ(test, 0L, result->status);
	KUNIT_ASSERT_EQ(test, FP_SVC, result->instruction);
	KUNIT_ASSERT_EQ_MSG(test, code + sizeof(u32), regs->pc,
			    "%s ordinal %u pc", source->name, source->ordinal);
}

static void orlix_tcti_scalar_fp_compare_run(struct kunit *test,
	const struct orlix_tcti_test_fp_source *source, u32 instruction,
	struct pt_regs *regs, unsigned long code)
{
	struct orlix_tcti_result result;
	u64 before_simd[ARRAY_SIZE(current->thread.user_simd)];
	u64 expected_simd[2];
	u64 expected_gpr;
	unsigned long expected_nzcv;
	unsigned long expected_fpsr;
	u32 dest = orlix_tcti_scalar_fp_dest_obligation(source);
	int ret;

	memcpy(before_simd, current->thread.user_simd, sizeof(before_simd));
	ret = orlix_tcti_scalar_fp_expected_from_source(source, instruction, regs,
		before_simd, current->thread.user_fpcr, current->thread.user_fpsr,
		expected_simd, &expected_gpr, &expected_nzcv, &expected_fpsr);
	KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s expected-from-source %s insn %#x",
			    source->name, source->mnemonic, instruction);
	result = orlix_tcti_resume_user(current, regs, current->mm);
	orlix_tcti_scalar_fp_assert_success(test, source, &result, regs, code);
	if (dest == ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD) {
		KUNIT_ASSERT_EQ_MSG(test, expected_simd[0],
				    current->thread.user_simd[
					orlix_tcti_scalar_fp_rd_index(instruction) * 2U],
				    "%s dest lo %s insn %#x", source->name,
				    source->mnemonic, instruction);
		KUNIT_ASSERT_EQ_MSG(test, expected_simd[1],
				    current->thread.user_simd[
					orlix_tcti_scalar_fp_rd_index(instruction) * 2U + 1U],
				    "%s dest hi %s insn %#x", source->name,
				    source->mnemonic, instruction);
	} else if (dest == ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS) {
		KUNIT_ASSERT_EQ_MSG(test, expected_gpr,
				    orlix_tcti_scalar_fp_read_gpr(regs,
					orlix_tcti_scalar_fp_rd_index(instruction)),
				    "%s gpr %s insn %#x", source->name,
				    source->mnemonic, instruction);
	} else {
		KUNIT_ASSERT_EQ_MSG(test, expected_nzcv, regs->pstate & NZCV,
				    "%s nzcv %s insn %#x", source->name,
				    source->mnemonic, instruction);
	}
	KUNIT_ASSERT_EQ_MSG(test, expected_fpsr, current->thread.user_fpsr,
			    "%s fpsr %s insn %#x", source->name, source->mnemonic,
			    instruction);
}

static void orlix_tcti_scalar_fp_capture_run(struct kunit *test,
	const struct orlix_tcti_test_fp_source *source, u32 instruction,
	u32 obligation, struct pt_regs *regs, unsigned long code)
{
	const void *token;
	struct orlix_tcti_native_capture_session *capture = NULL;
	struct orlix_tcti_native_wire_record wire = {};
	struct orlix_tcti_target_proof_ingestion_ledger *ledger;
	int ret;

	orlix_tcti_scalar_fp_compare_run(test, source, instruction, regs, code);
	if (test->status == KUNIT_FAILURE)
		return;
	token = orlix_tcti_scalar_fp_production_capture_token(source->ordinal,
							      obligation);
	KUNIT_ASSERT_TRUE_MSG(test, token != NULL, "%s token %u", source->name,
			      obligation);
	ret = orlix_tcti_native_capture_begin(token, source->ordinal, obligation,
					      &capture);
	KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s begin %u", source->name, obligation);
	if (orlix_tcti_scalar_fp_is_convert(source))
		orlix_tcti_scalar_fp_seed_convert(source, regs, code, instruction);
	else
		orlix_tcti_scalar_fp_seed(source, regs, code, instruction);
	orlix_tcti_scalar_fp_compare_run(test, source, instruction, regs, code);
	if (test->status == KUNIT_FAILURE) {
		orlix_tcti_native_capture_destroy(capture);
		return;
	}
	ret = orlix_tcti_native_capture_take_wire(capture, &wire);
	KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s wire %u", source->name, obligation);
	ledger = orlix_tcti_target_proof_ingestion_ledger_create(1U);
	KUNIT_ASSERT_NOT_NULL(test, ledger);
	KUNIT_ASSERT_EQ_MSG(test, 0,
		orlix_tcti_target_proof_ingest_native(ledger, &wire, NULL),
		"%s ingest %u", source->name, obligation);
	KUNIT_ASSERT_EQ(test, 1U, ledger->native_passed);
	KUNIT_EXPECT_LT_MSG(test,
		orlix_tcti_target_proof_ingest_native(ledger, &wire, NULL),
		0, "%s replay %u", source->name, obligation);
	orlix_tcti_target_proof_ingestion_ledger_destroy(ledger);
	orlix_tcti_native_wire_record_destroy(&wire);
	orlix_tcti_native_capture_destroy(capture);
}

static void orlix_tcti_scalar_fp_decodes_exact_source_cohort(struct kunit *test)
{
	size_t index;
	size_t count = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_fp_source *source =
			&orlix_tcti_test_sources[index];
		struct orlix_tcti_decoded_instruction decoded;
		u32 instruction;

		if (!orlix_tcti_test_is_scalar_fp(source))
			continue;
		instruction = orlix_tcti_scalar_fp_legal_instruction(source);
		KUNIT_ASSERT_EQ_MSG(test, source->pattern,
				instruction & source->mask, "%s", source->name);
		decoded = orlix_tcti_decode_aarch64(instruction);
		if (orlix_tcti_test_is_optional_fp(source))
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

static void orlix_tcti_scalar_fp_binds_pinned_ddi0602_semantics(
	struct kunit *test)
{
	size_t index;
	size_t ddi = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_fp_source *source =
			&orlix_tcti_test_sources[index];

		if (!orlix_tcti_test_is_scalar_fp(source))
			continue;
		KUNIT_ASSERT_LT(test, source->ordinal,
			ARRAY_SIZE(orlix_tcti_test_has_ddi0602_semantics));
		KUNIT_EXPECT_TRUE_MSG(test,
			orlix_tcti_test_has_ddi0602_semantics[source->ordinal],
			"%s", source->name);
		ddi++;
	}
	KUNIT_EXPECT_EQ(test, FP_FAMILY_COUNT, ddi);
}

static void orlix_tcti_scalar_fp_compare_encoding(struct kunit *test,
	const struct orlix_tcti_test_fp_source *source, u32 instruction,
	bool z_set)
{
	struct pt_regs regs = {};
	unsigned long extra_code;

	KUNIT_ASSERT_TRUE_MSG(test,
		orlix_tcti_scalar_fp_instruction_matches_source(source, instruction),
		"%s insn %#x", source->name, instruction);
	extra_code = orlix_tcti_scalar_fp_map(test, instruction);
	if (orlix_tcti_scalar_fp_is_convert(source))
		orlix_tcti_scalar_fp_seed_convert(source, &regs, extra_code,
						  instruction);
	else
		orlix_tcti_scalar_fp_seed(source, &regs, extra_code, instruction);
	if (!z_set)
		regs.pstate &= ~PSR_Z_BIT;
	orlix_tcti_scalar_fp_compare_run(test, source, instruction, &regs,
					 extra_code);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(extra_code, PAGE_SIZE));
}

static void orlix_tcti_scalar_fp_compare_encoding_flags(struct kunit *test,
	const struct orlix_tcti_test_fp_source *source, u32 instruction,
	unsigned long flags)
{
	struct pt_regs regs = {};
	unsigned long extra_code;

	KUNIT_ASSERT_TRUE_MSG(test,
		orlix_tcti_scalar_fp_instruction_matches_source(source, instruction),
		"%s insn %#x", source->name, instruction);
	extra_code = orlix_tcti_scalar_fp_map(test, instruction);
	if (orlix_tcti_scalar_fp_is_convert(source))
		orlix_tcti_scalar_fp_seed_convert(source, &regs, extra_code,
						  instruction);
	else
		orlix_tcti_scalar_fp_seed(source, &regs, extra_code, instruction);
	regs.pstate = (regs.pstate & ~NZCV) | (flags & NZCV);
	orlix_tcti_scalar_fp_compare_run(test, source, instruction, &regs,
					 extra_code);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(extra_code, PAGE_SIZE));
}

static void orlix_tcti_scalar_fp_run_quiet_nan_compare(struct kunit *test,
	const struct orlix_tcti_test_fp_source *source, u32 instruction,
	struct pt_regs *regs, unsigned long code)
{
	orlix_tcti_scalar_fp_seed(source, regs, code, instruction);
	current->thread.user_simd[orlix_tcti_scalar_fp_rn_index(instruction) * 2U] =
		orlix_tcti_scalar_fp_lane_bits(source, FP32_QNAN, FP64_QNAN);
	if (!strstr(source->name, "_SZ") && !strstr(source->name, "_DZ"))
		current->thread.user_simd[orlix_tcti_scalar_fp_rm_index(instruction) * 2U] =
			orlix_tcti_scalar_fp_lane_bits(source, FP32_ONE, FP64_ONE);
	orlix_tcti_scalar_fp_compare_run(test, source, instruction, regs, code);
}

static void orlix_tcti_scalar_fp_run_fz_subnormal_compare(struct kunit *test,
	const struct orlix_tcti_test_fp_source *source, u32 instruction,
	struct pt_regs *regs, unsigned long code)
{
	u8 rn = orlix_tcti_scalar_fp_rn_index(instruction);
	bool zero = strstr(source->name, "_SZ") || strstr(source->name, "_DZ");

	orlix_tcti_scalar_fp_seed(source, regs, code, instruction);
	current->thread.user_fpcr = AARCH64_FPCR_FZ;
	current->thread.user_simd[rn * 2U] =
		orlix_tcti_scalar_fp_lane_bits(source, FP32_MIN_SUBNORMAL,
					       FP64_MIN_SUBNORMAL);
	if (!zero)
		current->thread.user_simd[
			orlix_tcti_scalar_fp_rm_index(instruction) * 2U] =
			orlix_tcti_scalar_fp_lane_bits(source, FP32_POS_ZERO,
						       0);
	orlix_tcti_scalar_fp_compare_run(test, source, instruction, regs, code);
}

static void orlix_tcti_scalar_fp_run_extra_vectors(struct kunit *test,
	const struct orlix_tcti_test_fp_source *source, u32 instruction,
	struct pt_regs *regs, unsigned long code)
{
	const char *mnemonic = source->mnemonic;

	if (strstr(source->name, "FRINT")) {
		static const u64 fp32[] = {
			FP32_ONE_POINT_FIVE, FP32_NEG_ONE_POINT_FIVE, FP32_TWO_POINT_FIVE,
		};
		static const u64 fp64[] = {
			FP64_ONE_POINT_FIVE, FP64_NEG_ONE_POINT_FIVE, FP64_TWO_POINT_FIVE,
		};
		static const unsigned long modes[] = {
			0, AARCH64_FPCR_RMODE_POSINF, AARCH64_FPCR_RMODE_NEGINF,
			AARCH64_FPCR_RMODE_ZERO,
		};
		size_t i;

		for (i = 0; i < ARRAY_SIZE(fp32); i++) {
			orlix_tcti_scalar_fp_seed(source, regs, code, instruction);
			current->thread.user_simd[orlix_tcti_scalar_fp_rn_index(instruction) * 2U] =
				orlix_tcti_scalar_fp_is_double(source) ? fp64[i] : fp32[i];
			orlix_tcti_scalar_fp_compare_run(test, source, instruction,
							 regs, code);
			if (test->status == KUNIT_FAILURE)
				return;
		}
		if (!strcmp(source->mnemonic, "FRINTI")) {
			for (i = 0; i < ARRAY_SIZE(modes); i++) {
				orlix_tcti_scalar_fp_seed(source, regs, code, instruction);
				current->thread.user_fpcr = modes[i];
				current->thread.user_simd[orlix_tcti_scalar_fp_rn_index(instruction) * 2U] =
					orlix_tcti_scalar_fp_is_double(source) ?
					FP64_ONE_POINT_FIVE : FP32_ONE_POINT_FIVE;
				orlix_tcti_scalar_fp_compare_run(test, source,
					instruction, regs, code);
				if (test->status == KUNIT_FAILURE)
					return;
			}
		}
		return;
	}
	if (strstr(source->name, "floatsel") || strstr(source->name, "floatccmp")) {
		u32 extra;
		bool ccmp = strstr(source->name, "floatccmp");

		orlix_tcti_scalar_fp_seed(source, regs, code, instruction);
		regs->pstate &= ~PSR_Z_BIT;
		orlix_tcti_scalar_fp_compare_run(test, source, instruction, regs,
						 code);
		if (test->status == KUNIT_FAILURE)
			return;
		extra = orlix_tcti_scalar_fp_instruction_with_cond(source, 1,
			ccmp ? 8 : 0);
		orlix_tcti_scalar_fp_compare_encoding(test, source, extra, true);
		if (test->status == KUNIT_FAILURE)
			return;
		orlix_tcti_scalar_fp_compare_encoding(test, source, extra, false);
		if (test->status == KUNIT_FAILURE)
			return;
		{
			static const u8 conds[] = { 2, 4, 6 };
			static const unsigned long flags[] = {
				PSR_C_BIT, PSR_N_BIT, PSR_V_BIT,
			};
			size_t i;

			for (i = 0; i < ARRAY_SIZE(conds); i++) {
				extra = orlix_tcti_scalar_fp_instruction_with_cond(
					source, conds[i], 0);
				orlix_tcti_scalar_fp_compare_encoding_flags(test,
					source, extra, flags[i]);
				if (test->status == KUNIT_FAILURE)
					return;
				orlix_tcti_scalar_fp_compare_encoding_flags(test,
					source, extra, 0);
				if (test->status == KUNIT_FAILURE)
					return;
			}
			if (ccmp) {
				static const u8 fb[] = { 4, 2, 1 };

				for (i = 0; i < ARRAY_SIZE(fb); i++) {
					extra = orlix_tcti_scalar_fp_instruction_with_cond(
						source, 1, fb[i]);
					orlix_tcti_scalar_fp_compare_encoding_flags(test,
						source, extra, PSR_Z_BIT);
					if (test->status == KUNIT_FAILURE)
						return;
				}
			}
		}
		if (ccmp) {
			orlix_tcti_scalar_fp_run_quiet_nan_compare(test, source,
				instruction, regs, code);
			if (test->status == KUNIT_FAILURE)
				return;
			orlix_tcti_scalar_fp_run_fz_subnormal_compare(test, source,
				instruction, regs, code);
		}
		return;
	}
	if (strstr(source->name, "floatcmp")) {
		orlix_tcti_scalar_fp_run_quiet_nan_compare(test, source, instruction,
							   regs, code);
		if (test->status == KUNIT_FAILURE)
			return;
		orlix_tcti_scalar_fp_run_fz_subnormal_compare(test, source,
							     instruction, regs,
							     code);
		return;
	}
	if (!strcmp(mnemonic, "FABS")) {
		orlix_tcti_scalar_fp_seed(source, regs, code, instruction);
		current->thread.user_simd[orlix_tcti_scalar_fp_rn_index(instruction) * 2U] =
			orlix_tcti_scalar_fp_lane_bits(source,
				FP32_NEG_ONE_POINT_FIVE, FP64_NEG_ONE_POINT_FIVE);
		orlix_tcti_scalar_fp_compare_run(test, source, instruction, regs,
						 code);
		return;
	}
	if (!strcmp(mnemonic, "FSQRT")) {
		static const unsigned long modes[] = {
			0, AARCH64_FPCR_RMODE_POSINF, AARCH64_FPCR_RMODE_NEGINF,
			AARCH64_FPCR_RMODE_ZERO,
		};
		u8 rn = orlix_tcti_scalar_fp_rn_index(instruction);
		size_t i;

		orlix_tcti_scalar_fp_seed(source, regs, code, instruction);
		current->thread.user_simd[rn * 2U] =
			orlix_tcti_scalar_fp_lane_bits(source, FP32_FOUR, FP64_FOUR);
		orlix_tcti_scalar_fp_compare_run(test, source, instruction, regs,
						 code);
		if (test->status == KUNIT_FAILURE)
			return;
		for (i = 0; i < ARRAY_SIZE(modes); i++) {
			orlix_tcti_scalar_fp_seed(source, regs, code, instruction);
			current->thread.user_fpcr = modes[i];
			current->thread.user_simd[rn * 2U] =
				orlix_tcti_scalar_fp_lane_bits(source, FP32_TWO,
							       FP64_TWO);
			orlix_tcti_scalar_fp_compare_run(test, source, instruction,
							 regs, code);
			if (test->status == KUNIT_FAILURE)
				return;
		}
		return;
	}
	if (!strcmp(mnemonic, "FMAX") || !strcmp(mnemonic, "FMIN") ||
	    !strcmp(mnemonic, "FMAXNM") || !strcmp(mnemonic, "FMINNM")) {
		orlix_tcti_scalar_fp_seed(source, regs, code, instruction);
		current->thread.user_simd[orlix_tcti_scalar_fp_rn_index(instruction) * 2U] =
			orlix_tcti_scalar_fp_lane_bits(source, FP32_QNAN, FP64_QNAN);
		orlix_tcti_scalar_fp_compare_run(test, source, instruction, regs,
						 code);
		return;
	}
	if (!strcmp(mnemonic, "FMADD") || !strcmp(mnemonic, "FMSUB") ||
	    !strcmp(mnemonic, "FNMADD") || !strcmp(mnemonic, "FNMSUB")) {
		static const unsigned long modes[] = {
			0, AARCH64_FPCR_RMODE_POSINF, AARCH64_FPCR_RMODE_NEGINF,
			AARCH64_FPCR_RMODE_ZERO,
		};
		u8 rn = orlix_tcti_scalar_fp_rn_index(instruction);
		u8 rm = orlix_tcti_scalar_fp_rm_index(instruction);
		u8 ra = orlix_tcti_scalar_fp_ra_index(instruction);
		u64 a = orlix_tcti_scalar_fp_lane_bits(source, FP32_FMA_A,
						       FP64_FMA_A);
		bool sub = !strcmp(mnemonic, "FMSUB") ||
			!strcmp(mnemonic, "FNMSUB");
		size_t i;

		for (i = 0; i < ARRAY_SIZE(modes); i++) {
			orlix_tcti_scalar_fp_seed(source, regs, code, instruction);
			current->thread.user_fpcr = modes[i];
			current->thread.user_simd[rn * 2U] = a;
			current->thread.user_simd[rm * 2U] = a;
			current->thread.user_simd[ra * 2U] = sub ?
				orlix_tcti_scalar_fp_lane_bits(source, FP32_ONE,
							       FP64_ONE) :
				orlix_tcti_scalar_fp_lane_bits(source, FP32_NEG_ONE,
							       FP64_NEG_ONE);
			orlix_tcti_scalar_fp_compare_run(test, source, instruction,
							 regs, code);
			if (test->status == KUNIT_FAILURE)
				return;
		}
		return;
	}
	if (!strcmp(mnemonic, "FADD") || !strcmp(mnemonic, "FSUB") ||
	    !strcmp(mnemonic, "FMUL") || !strcmp(mnemonic, "FDIV") ||
	    !strcmp(mnemonic, "FNMUL")) {
		static const unsigned long modes[] = {
			0, AARCH64_FPCR_RMODE_POSINF, AARCH64_FPCR_RMODE_NEGINF,
			AARCH64_FPCR_RMODE_ZERO,
		};
		u8 rn = orlix_tcti_scalar_fp_rn_index(instruction);
		u8 rm = orlix_tcti_scalar_fp_rm_index(instruction);
		size_t i;

		for (i = 0; i < ARRAY_SIZE(modes); i++) {
			orlix_tcti_scalar_fp_seed(source, regs, code, instruction);
			current->thread.user_fpcr = modes[i];
			if (!strcmp(mnemonic, "FDIV")) {
				current->thread.user_simd[rn * 2U] =
					orlix_tcti_scalar_fp_lane_bits(source,
						FP32_ONE, FP64_ONE);
				current->thread.user_simd[rm * 2U] =
					orlix_tcti_scalar_fp_lane_bits(source,
						FP32_THREE, FP64_THREE);
			} else if (!strcmp(mnemonic, "FMUL") ||
				   !strcmp(mnemonic, "FNMUL")) {
				current->thread.user_simd[rn * 2U] =
					orlix_tcti_scalar_fp_lane_bits(source,
						FP32_FMA_A, FP64_FMA_A);
				current->thread.user_simd[rm * 2U] =
					orlix_tcti_scalar_fp_lane_bits(source,
						FP32_FMA_A, FP64_FMA_A);
			} else {
				current->thread.user_simd[rn * 2U] =
					orlix_tcti_scalar_fp_lane_bits(source,
						FP32_ONE, FP64_ONE);
				current->thread.user_simd[rm * 2U] =
					orlix_tcti_scalar_fp_lane_bits(source,
						FP32_MIN_SUBNORMAL,
						FP64_MIN_SUBNORMAL);
			}
			orlix_tcti_scalar_fp_compare_run(test, source, instruction,
							 regs, code);
			if (test->status == KUNIT_FAILURE)
				return;
		}
		return;
	}
	if (!strcmp(mnemonic, "FCVT")) {
		static const u64 fp32[] = {
			FP32_ONE_POINT_FIVE, FP32_TWO_POINT_FIVE, FP32_POS_INF,
		};
		static const u64 fp64[] = {
			FP64_ONE_POINT_FIVE, FP64_TWO_POINT_FIVE, FP64_POS_INF,
		};
		static const u64 fp16[] = {
			FP16_ONE_POINT_FIVE, FP16_TWO_POINT_FIVE, FP16_POS_INF,
		};
		size_t i;

		for (i = 0; i < ARRAY_SIZE(fp32); i++) {
			orlix_tcti_scalar_fp_seed(source, regs, code, instruction);
			current->thread.user_simd[orlix_tcti_scalar_fp_rn_index(instruction) * 2U] =
				orlix_tcti_scalar_fp_src_lane_bits(source, fp32[i],
					fp64[i], fp16[i]);
			orlix_tcti_scalar_fp_compare_run(test, source, instruction,
							 regs, code);
			if (test->status == KUNIT_FAILURE)
				return;
		}
		if (strstr(source->name, "_SD") || strstr(source->name, "_HS") ||
		    strstr(source->name, "_HD")) {
			static const unsigned long modes[] = {
				0, AARCH64_FPCR_RMODE_POSINF,
				AARCH64_FPCR_RMODE_NEGINF, AARCH64_FPCR_RMODE_ZERO,
			};
			u64 inexact;
			u8 rn = orlix_tcti_scalar_fp_rn_index(instruction);

			if (strstr(source->name, "_HS"))
				inexact = FP32_NARROW_H;
			else if (strstr(source->name, "_SD"))
				inexact = FP64_NARROW_S;
			else
				inexact = FP64_NARROW_H;
			for (i = 0; i < ARRAY_SIZE(modes); i++) {
				orlix_tcti_scalar_fp_seed(source, regs, code,
							  instruction);
				current->thread.user_fpcr = modes[i];
				current->thread.user_simd[rn * 2U] = inexact;
				orlix_tcti_scalar_fp_compare_run(test, source,
					instruction, regs, code);
				if (test->status == KUNIT_FAILURE)
					return;
			}
		}
		return;
	}
	if (strstr(source->name, "floatimm")) {
		static const u8 imm8s[] = { 0x80U, 0x40U, 0x01U, 0xffU };
		size_t i;

		for (i = 0; i < ARRAY_SIZE(imm8s); i++) {
			u32 extra = orlix_tcti_scalar_fp_instruction_with_imm8(
				source, imm8s[i]);

			orlix_tcti_scalar_fp_compare_encoding(test, source, extra,
							      true);
			if (test->status == KUNIT_FAILURE)
				return;
		}
	}
}

static void orlix_tcti_scalar_fp_run_register_vectors(struct kunit *test,
	const struct orlix_tcti_test_fp_source *source, u32 instruction)
{
	const char *name = source->name;
	u32 high = instruction;
	u32 alias_rn = instruction;
	bool rd_free = (source->mask & 0x1fU) == 0;
	bool rn_free = !strstr(name, "floatimm") &&
		(source->mask & (0x1fU << 5)) == 0;
	bool rm_free = !strstr(name, "floatimm") &&
		(source->mask & (0x1fU << 16)) == 0;
	bool ra_free = strstr(name, "floatdp3") &&
		(source->mask & (0x1fU << 10)) == 0;

	if (rd_free)
		high = (high & ~0x1fU) | 16U;
	if (rn_free)
		high = (high & ~(0x1fU << 5)) | (17U << 5);
	if (rm_free)
		high = (high & ~(0x1fU << 16)) | (18U << 16);
	if (ra_free)
		high = (high & ~(0x1fU << 10)) | (19U << 10);
	if (high != instruction) {
		orlix_tcti_scalar_fp_compare_encoding(test, source, high, true);
		if (test->status == KUNIT_FAILURE)
			return;
	}
	if (rd_free && rn_free) {
		alias_rn = (alias_rn & ~0x1fU) | 16U;
		alias_rn = (alias_rn & ~(0x1fU << 5)) | (16U << 5);
		if (rm_free)
			alias_rn = (alias_rn & ~(0x1fU << 16)) | (18U << 16);
		if (ra_free)
			alias_rn = (alias_rn & ~(0x1fU << 10)) | (19U << 10);
		if (alias_rn != instruction && alias_rn != high) {
			orlix_tcti_scalar_fp_compare_encoding(test, source, alias_rn,
							      true);
			if (test->status == KUNIT_FAILURE)
				return;
		}
	}
	if (rd_free && rm_free &&
	    orlix_tcti_scalar_fp_dest_obligation(source) !=
		ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS) {
		u32 alias_rm = instruction;

		alias_rm = (alias_rm & ~0x1fU) | 16U;
		alias_rm = (alias_rm & ~(0x1fU << 16)) | (16U << 16);
		if (alias_rm != instruction && alias_rm != high &&
		    alias_rm != alias_rn) {
			orlix_tcti_scalar_fp_compare_encoding(test, source, alias_rm,
							      true);
			if (test->status == KUNIT_FAILURE)
				return;
		}
	}
	if (rd_free && ra_free) {
		u32 alias_ra = instruction;

		alias_ra = (alias_ra & ~0x1fU) | 16U;
		alias_ra = (alias_ra & ~(0x1fU << 10)) | (16U << 10);
		if (alias_ra != instruction && alias_ra != high &&
		    alias_ra != alias_rn) {
			orlix_tcti_scalar_fp_compare_encoding(test, source, alias_ra,
							      true);
			if (test->status == KUNIT_FAILURE)
				return;
		}
	}
	if (orlix_tcti_scalar_fp_gpr_rn(source) && rn_free) {
		u32 zrn = instruction;

		zrn = (zrn & ~(0x1fU << 5)) | (31U << 5);
		if (zrn != instruction && zrn != high && zrn != alias_rn) {
			orlix_tcti_scalar_fp_compare_encoding(test, source, zrn,
							      true);
			if (test->status == KUNIT_FAILURE)
				return;
		}
	}
	if (orlix_tcti_scalar_fp_dest_obligation(source) ==
	    ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS && rd_free) {
		u32 zrd = instruction;

		zrd = (zrd & ~0x1fU) | 31U;
		if (zrd != instruction && zrd != high)
			orlix_tcti_scalar_fp_compare_encoding(test, source, zrd,
							      true);
	}
}

static void orlix_tcti_scalar_fp_production_resume(struct kunit *test)
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
		u32 dest;

		if (!orlix_tcti_test_is_el0_fp(source))
			continue;
		seen++;
		instruction = orlix_tcti_scalar_fp_legal_instruction(source);
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_ASSERT_TRUE_MSG(test,
			orlix_tcti_test_fp_decode_is_family(decoded.decode_class),
			"%s class %u insn %#x", source->name, decoded.decode_class,
			instruction);
		KUNIT_ASSERT_EQ_MSG(test, source->ordinal, decoded.source_ordinal,
				    "%s ordinal", source->name);
		KUNIT_ASSERT_TRUE_MSG(test,
			orlix_tcti_scalar_fp_instruction_matches_source(source,
								       instruction),
			"%s legal decode", source->name);
		dest = orlix_tcti_scalar_fp_dest_obligation(source);
		code = orlix_tcti_scalar_fp_map(test, instruction);
		if (orlix_tcti_scalar_fp_is_convert(source)) {
			static const u64 fp32_vecs[] = {
				FP32_ONE_POINT_FIVE, FP32_TWO_POINT_FIVE, FP32_POS_INF,
			};
			static const u64 fp64_vecs[] = {
				FP64_ONE_POINT_FIVE, FP64_TWO_POINT_FIVE, FP64_POS_INF,
			};
			static const u64 fp16_vecs[] = {
				FP16_ONE_POINT_FIVE, FP16_TWO_POINT_FIVE, FP16_POS_INF,
			};
			size_t vec;
			u8 extra_fbits[2];
			u8 extra_count = 0;

			orlix_tcti_scalar_fp_seed_convert(source, &regs, code, instruction);
			orlix_tcti_scalar_fp_compare_run(test, source, instruction,
							 &regs, code);
			if (test->status == KUNIT_FAILURE)
				return;
			for (vec = 0; vec < ARRAY_SIZE(fp32_vecs); vec++) {
				orlix_tcti_scalar_fp_seed(source, &regs, code, instruction);
				orlix_tcti_scalar_fp_apply_convert_operands(source,
					&regs, orlix_tcti_scalar_fp_src_lane_bits(source,
						fp32_vecs[vec], fp64_vecs[vec],
						fp16_vecs[vec]),
					CONVERT_GPR_NEG8, instruction);
				orlix_tcti_scalar_fp_compare_run(test, source,
					instruction, &regs, code);
				if (test->status == KUNIT_FAILURE)
					return;
			}
			orlix_tcti_scalar_fp_seed(source, &regs, code, instruction);
			orlix_tcti_scalar_fp_apply_convert_operands(source, &regs,
				orlix_tcti_scalar_fp_convert_bits(source,
					FP32_NEG_ONE_POINT_FIVE,
					FP64_NEG_ONE_POINT_FIVE),
				CONVERT_GPR_I32_MIN, instruction);
			orlix_tcti_scalar_fp_compare_run(test, source, instruction,
							 &regs, code);
			if (test->status == KUNIT_FAILURE)
				return;
			if ((!strcmp(source->mnemonic, "SCVTF") ||
			     !strcmp(source->mnemonic, "UCVTF")) &&
			    (strstr(source->name, "float2int") ||
			     strstr(source->name, "float2fix")) &&
			    !strstr(source->name, "D32")) {
				static const unsigned long modes[] = {
					0, AARCH64_FPCR_RMODE_POSINF,
					AARCH64_FPCR_RMODE_NEGINF,
					AARCH64_FPCR_RMODE_ZERO,
				};
				u64 inexact = strstr(source->name, "D64") ?
					CONVERT_GPR_D_INEXACT :
					CONVERT_GPR_S_INEXACT;
				size_t mode;

				for (mode = 0; mode < ARRAY_SIZE(modes); mode++) {
					orlix_tcti_scalar_fp_seed_convert(source,
						&regs, code, instruction);
					current->thread.user_fpcr = modes[mode];
					orlix_tcti_scalar_fp_apply_convert_operands(
						source, &regs, 0, inexact,
						instruction);
					orlix_tcti_scalar_fp_compare_run(test,
						source, instruction, &regs, code);
					if (test->status == KUNIT_FAILURE)
						return;
					if (!strcmp(source->mnemonic, "SCVTF")) {
						orlix_tcti_scalar_fp_seed_convert(
							source, &regs, code,
							instruction);
						current->thread.user_fpcr =
							modes[mode];
						orlix_tcti_scalar_fp_apply_convert_operands(
							source, &regs, 0,
							(u64)(-(s64)inexact),
							instruction);
						orlix_tcti_scalar_fp_compare_run(
							test, source, instruction,
							&regs, code);
						if (test->status == KUNIT_FAILURE)
							return;
					}
				}
			}
			if (strstr(source->name, "float2fix")) {
				if (instruction & BIT(31)) {
					extra_fbits[0] = 1;
					extra_fbits[1] = 32;
					extra_count = 2;
				} else {
					extra_fbits[0] = 1;
					extra_fbits[1] = 16;
					extra_count = 2;
				}
			}
			for (vec = 0; vec < extra_count; vec++) {
				u32 scaled;
				unsigned long scaled_code;

				scaled = orlix_tcti_scalar_fp_instruction_with_fbits(
					source, extra_fbits[vec]);
				KUNIT_ASSERT_TRUE_MSG(test,
					orlix_tcti_scalar_fp_instruction_matches_source(
						source, scaled),
					"%s fbits %u insn %#x", source->name,
					extra_fbits[vec], scaled);
				scaled_code = orlix_tcti_scalar_fp_map(test, scaled);
				orlix_tcti_scalar_fp_seed_convert(source, &regs,
								  scaled_code, scaled);
				orlix_tcti_scalar_fp_compare_run(test, source, scaled,
								 &regs, scaled_code);
				KUNIT_EXPECT_EQ(test, 0,
						vm_munmap(scaled_code, PAGE_SIZE));
				if (test->status == KUNIT_FAILURE)
					return;
			}
			orlix_tcti_scalar_fp_seed_convert(source, &regs, code, instruction);
		} else {
			orlix_tcti_scalar_fp_seed(source, &regs, code, instruction);
			orlix_tcti_scalar_fp_compare_run(test, source, instruction,
							 &regs, code);
			if (test->status == KUNIT_FAILURE)
				return;
			orlix_tcti_scalar_fp_run_extra_vectors(test, source,
				instruction, &regs, code);
			if (test->status == KUNIT_FAILURE)
				return;
			orlix_tcti_scalar_fp_seed(source, &regs, code, instruction);
		}
		orlix_tcti_scalar_fp_run_register_vectors(test, source, instruction);
		if (test->status == KUNIT_FAILURE)
			return;
		if (orlix_tcti_scalar_fp_is_convert(source))
			orlix_tcti_scalar_fp_seed_convert(source, &regs, code, instruction);
		else
			orlix_tcti_scalar_fp_seed(source, &regs, code, instruction);
		orlix_tcti_scalar_fp_capture_run(test, source, instruction, dest,
						 &regs, code);
		if (test->status == KUNIT_FAILURE)
			return;
		if (orlix_tcti_scalar_fp_is_convert(source))
			orlix_tcti_scalar_fp_seed_convert(source, &regs, code, instruction);
		else
			orlix_tcti_scalar_fp_seed(source, &regs, code, instruction);
		orlix_tcti_scalar_fp_capture_run(test, source, instruction,
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC, &regs, code);
		if (test->status == KUNIT_FAILURE)
			return;
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
	}
	KUNIT_EXPECT_EQ(test, FP_EL0_COUNT, seen);
}

static void orlix_tcti_scalar_fp_optional_rejected(struct kunit *test)
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

		if (!orlix_tcti_test_is_scalar_fp(source) ||
		    !orlix_tcti_test_is_optional_fp(source))
			continue;
		seen++;
		instruction = orlix_tcti_scalar_fp_legal_instruction(source);
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_EXPECT_EQ_MSG(test, source->ordinal, decoded.source_ordinal,
				    "%s ordinal insn %#x class %u", source->name,
				    instruction, decoded.decode_class);
		code = orlix_tcti_scalar_fp_map(test, instruction);
		orlix_tcti_scalar_fp_seed(source, &regs, code, instruction);
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
	KUNIT_EXPECT_EQ(test, FP_OPTIONAL_COUNT, seen);
}

static void orlix_tcti_scalar_fp_edge_vectors(struct kunit *test)
{
	const struct orlix_tcti_test_fp_source *fadd =
		orlix_tcti_test_fp_name("FADD_S_floatdp2");
	u32 insn;
	unsigned long code;
	struct pt_regs regs = {};

	KUNIT_ASSERT_NOT_NULL(test, fadd);
	insn = orlix_tcti_scalar_fp_legal_instruction(fadd);
	code = orlix_tcti_scalar_fp_map(test, insn);
	orlix_tcti_scalar_fp_seed(fadd, &regs, code, insn);
	current->thread.user_simd[orlix_tcti_scalar_fp_rn_index(insn) * 2U] = FP32_POS_INF;
	current->thread.user_simd[orlix_tcti_scalar_fp_rm_index(insn) * 2U] = FP32_NEG_INF;
	orlix_tcti_scalar_fp_compare_run(test, fadd, insn, &regs, code);
	orlix_tcti_scalar_fp_seed(fadd, &regs, code, insn);
	current->thread.user_simd[orlix_tcti_scalar_fp_rn_index(insn) * 2U] = FP32_POS_ZERO;
	current->thread.user_simd[orlix_tcti_scalar_fp_rm_index(insn) * 2U] = FP32_NEG_ZERO;
	orlix_tcti_scalar_fp_compare_run(test, fadd, insn, &regs, code);
	orlix_tcti_scalar_fp_seed(fadd, &regs, code, insn);
	current->thread.user_simd[orlix_tcti_scalar_fp_rn_index(insn) * 2U] = FP32_SNAN;
	current->thread.user_simd[orlix_tcti_scalar_fp_rm_index(insn) * 2U] = FP32_ONE;
	orlix_tcti_scalar_fp_compare_run(test, fadd, insn, &regs, code);
	orlix_tcti_scalar_fp_seed(fadd, &regs, code, insn);
	current->thread.user_fpcr = AARCH64_FPCR_RMODE_ZERO;
	current->thread.user_simd[orlix_tcti_scalar_fp_rn_index(insn) * 2U] = FP32_ONE;
	current->thread.user_simd[orlix_tcti_scalar_fp_rm_index(insn) * 2U] = FP32_MIN_SUBNORMAL;
	orlix_tcti_scalar_fp_compare_run(test, fadd, insn, &regs, code);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
}

static void orlix_tcti_scalar_fp_reserved_encodings(struct kunit *test)
{
	struct orlix_tcti_decoded_instruction decoded;
	u32 insn;
	unsigned long code;
	struct pt_regs regs = {};
	struct pt_regs before;
	struct orlix_tcti_result result;

	/* ftype 10 is reserved for scalar FADD. */
	insn = 0x1ea02800U;
	decoded = orlix_tcti_decode_aarch64(insn);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED, decoded.decode_class);
	code = orlix_tcti_scalar_fp_map(test, insn);
	orlix_tcti_scalar_fp_seed(NULL, &regs, code, insn);
	before = regs;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION, result.reason);
	KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	/* 32-bit float2fix with scale<5>=0 is unallocated. */
	insn = 0x1e020020U;
	decoded = orlix_tcti_decode_aarch64(insn);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED, decoded.decode_class);
	code = orlix_tcti_scalar_fp_map(test, insn);
	orlix_tcti_scalar_fp_seed(NULL, &regs, code, insn);
	before = regs;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION, result.reason);
	KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
}

static struct kunit_case orlix_tcti_scalar_fp_source_bound_cases[] = {
	KUNIT_CASE(orlix_tcti_scalar_fp_decodes_exact_source_cohort),
	KUNIT_CASE(orlix_tcti_scalar_fp_binds_pinned_ddi0602_semantics),
	KUNIT_CASE(orlix_tcti_scalar_fp_production_resume),
	KUNIT_CASE(orlix_tcti_scalar_fp_optional_rejected),
	KUNIT_CASE(orlix_tcti_scalar_fp_edge_vectors),
	KUNIT_CASE(orlix_tcti_scalar_fp_reserved_encodings),
	{}
};

static struct kunit_suite orlix_tcti_scalar_fp_source_bound_suite = {
	.name = "orlix-tcti-scalar-fp-source-bound",
	.init = orlix_tcti_scalar_fp_test_init,
	.exit = orlix_tcti_scalar_fp_test_exit,
	.test_cases = orlix_tcti_scalar_fp_source_bound_cases,
};

kunit_test_suite(orlix_tcti_scalar_fp_source_bound_suite);
