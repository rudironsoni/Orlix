// SPDX-License-Identifier: GPL-2.0-only
/*
 * Production-path proof for the 240 ADVSIMD_INTEGER leaves owned by
 * GitHub #125. Leftover CORE integer suites remain regression and do not
 * receive #120 close credit. Each captured EL0 leaf compares destination
 * SIMD and QC against a source-mnemonic architectural result. FEAT_RDM,
 * FEAT_DotProd, and FEAT_I8MM stay off this suite. Those features are not
 * advertised.
 */
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/compiler.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/limits.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/preempt.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/string.h>
#include <linux/syscalls.h>

#include "../decode_aarch64.h"
#include "orlix_tcti_memory_proof.h"
#include "orlix_tcti_native_observation.h"
#include "orlix_tcti_advsimd_integer_production_capture.h"
#include "target_native_proof_contract_private.h"
#include "target_proof_ingestion_private.h"

#define INT_SVC 0xd4000001U
#define INT_FAMILY_COUNT 240U
#define INT_EL0_COUNT 222U
#define INT_NON_EL0_COUNT 18U
#define INT_RD 0U
#define INT_RN 1U
#define INT_RM 2U
#define AARCH64_FPSR_IOC BIT(0)
#define AARCH64_FPSR_IDC BIT(7)
#define AARCH64_FPSR_QC BIT(27)
#define AARCH64_FPSR_PRESERVED (AARCH64_FPSR_IOC | AARCH64_FPSR_IDC)
#define AARCH64_FPCR_RMODE_POSINF BIT(22)

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

struct orlix_tcti_test_integer_source {
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
static const struct orlix_tcti_test_integer_source orlix_tcti_test_sources[] = {
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

static bool orlix_tcti_test_is_advsimd_integer(
	const struct orlix_tcti_test_integer_source *source)
{
	return source->ordinal < ARRAY_SIZE(orlix_tcti_test_source_families) &&
		orlix_tcti_test_source_families[source->ordinal] ==
			ORLIX_TCTI_TEST_SOURCE_FAMILY_ADVSIMD_INTEGER;
}

static bool orlix_tcti_test_is_optional_integer_feature(
	const struct orlix_tcti_test_integer_source *source)
{
	const char *op = source->operation;

	return !strncmp(op, "SQRDMLAH_", 9) || !strncmp(op, "SQRDMLSH_", 9) ||
		!strncmp(op, "SDOT_", 5) || !strncmp(op, "UDOT_", 5) ||
		!strncmp(op, "USDOT_", 6) || !strncmp(op, "SUDOT_", 6) ||
		!strncmp(op, "SMMLA_", 6) || !strncmp(op, "UMMLA_", 6) ||
		!strncmp(op, "USMMLA_", 7);
}

static bool orlix_tcti_test_is_el0_integer(
	const struct orlix_tcti_test_integer_source *source)
{
	return orlix_tcti_test_is_advsimd_integer(source) &&
		!orlix_tcti_test_is_optional_integer_feature(source);
}

static const struct orlix_tcti_test_integer_source *
orlix_tcti_test_integer_name(const char *name)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++)
		if (orlix_tcti_test_is_advsimd_integer(
			    &orlix_tcti_test_sources[index]) &&
		    !strcmp(orlix_tcti_test_sources[index].name, name))
			return &orlix_tcti_test_sources[index];
	return NULL;
}

static bool orlix_tcti_advsimd_integer_size00_reserved(
	const struct orlix_tcti_test_integer_source *source)
{
	const char *name = source->name;
	const char *op = source->operation;

	if (strstr(name, "asimdelem") || strstr(name, "asisdelem"))
		return true;
	return strstr(op, "SQDMULH") || strstr(op, "SQRDMULH") ||
		strstr(op, "SQDMLAL") || strstr(op, "SQDMLSL") ||
		strstr(op, "SQDMULL");
}

static u32 orlix_tcti_advsimd_integer_legal_instruction(
	const struct orlix_tcti_test_integer_source *source)
{
	u32 instruction = source->pattern & source->mask;

	instruction |= (INT_RN << 5) | INT_RD;
	if ((source->mask & BIT(30)) == 0 && !(source->pattern & BIT(28)))
		instruction |= BIT(30);
	if (orlix_tcti_advsimd_integer_size00_reserved(source) &&
	    ((source->mask >> 22) & 0x3U) == 0 &&
	    ((instruction >> 22) & 0x3U) == 0)
		instruction |= 1U << 22;
	if (!strncmp(source->operation, "SDOT_", 5) ||
	    !strncmp(source->operation, "UDOT_", 5)) {
		instruction &= ~(3U << 22);
		instruction |= 2U << 22;
	}
	if ((strstr(source->name, "asimdshf") ||
	     strstr(source->name, "asisdshf")) &&
	    ((instruction >> 19) & 0xfU) == 0) {
		if (strstr(source->name, "_N"))
			instruction |= 31U << 16;
		else if (strstr(source->name, "asisdshf"))
			instruction |= 65U << 16;
		else
			instruction |= 9U << 16;
	} else if ((source->mask & (0x1fU << 16)) == 0)
		instruction |= INT_RM << 16;
	if (strstr(source->name, "asimdelem") &&
	    ((instruction >> 21) & 1U) == 0 && (source->mask & BIT(21)) == 0)
		instruction |= BIT(21);
	return instruction;
}

static bool orlix_tcti_test_integer_decode_is_family(u32 decode_class)
{
	return decode_class == ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC ||
		decode_class == ORLIX_TCTI_DECODE_SIMD_VECTOR_LOGICAL ||
		decode_class == ORLIX_TCTI_DECODE_SIMD_VECTOR_COMPARE ||
		decode_class == ORLIX_TCTI_DECODE_SIMD_VECTOR_REDUCTION ||
		decode_class == ORLIX_TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE ||
		decode_class == ORLIX_TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE;
}

static int orlix_tcti_advsimd_integer_test_init(struct kunit *test)
{
	struct mm_struct *mm = mm_alloc();

	if (!mm)
		return -ENOMEM;
	kthread_use_mm(mm);
	test->priv = mm;
	return 0;
}

static void orlix_tcti_advsimd_integer_test_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (!mm)
		return;
	kthread_unuse_mm(mm);
	mmput(mm);
}

static unsigned long orlix_tcti_advsimd_integer_map(struct kunit *test,
						    u32 instruction)
{
	u8 bytes[16] = {};
	u32 program[2] = { instruction, INT_SVC };

	memcpy(bytes, program, sizeof(program));
	return orlix_tcti_memory_proof_map_bytes(test, bytes, sizeof(bytes),
						 PROT_READ | PROT_EXEC);
}

static u64 orlix_tcti_advsimd_integer_lane_mask(u8 lane_bytes)
{
	return lane_bytes == 8 ? ~0ULL : (BIT_ULL(lane_bytes * 8) - 1);
}

static u32 orlix_tcti_advsimd_integer_urecpe_u32(u32 operand)
{
	u32 input;
	u32 a;
	u32 b;
	u32 estimate;

	if ((operand & 0x80000000U) == 0)
		return 0xffffffffU;
	input = (operand >> 23) & 0x1ffU;
	a = input * 2U + 1U;
	b = (1U << 19) / a;
	estimate = (b + 1U) >> 1;
	return estimate << 23;
}

static u32 orlix_tcti_advsimd_integer_ursqrte_u32(u32 operand)
{
	u32 a;
	u32 b;
	u32 estimate;

	if ((operand & 0xc0000000U) == 0)
		return 0xffffffffU;
	a = (operand >> 23) & 0x1ffU;
	if (a < 256U)
		a = a * 2U + 1U;
	else
		a = ((a >> 1) << 1) * 2U + 2U;
	b = 512U;
	while ((u64)a * (b + 1U) * (b + 1U) < (1U << 28))
		b++;
	estimate = (b + 1U) / 2U;
	return estimate << 23;
}

static u64 orlix_tcti_advsimd_integer_get_lane(const u64 words[2], u8 lane,
					       u8 lane_bytes)
{
	u8 byte = lane * lane_bytes;
	u64 mask = orlix_tcti_advsimd_integer_lane_mask(lane_bytes);

	return (words[byte / 8U] >> ((byte % 8U) * 8U)) & mask;
}

static u8 orlix_tcti_advsimd_integer_elem_index(u32 instruction, u8 size)
{
	u8 h = (instruction >> 11) & 1U;
	u8 l = (instruction >> 21) & 1U;
	u8 mbit = (instruction >> 20) & 1U;

	if (size == 2)
		return (u8)((h << 1) | l);
	if (size == 1)
		return (u8)((h << 2) | (l << 1) | mbit);
	if (size == 0)
		return (u8)((h << 3) | (l << 2) | (mbit << 1) |
			    ((instruction >> 16) & 1U));
	return h;
}

static void orlix_tcti_advsimd_integer_set_lane(u64 words[2], u8 lane,
						u8 lane_bytes, u64 value)
{
	u8 byte = lane * lane_bytes;
	u64 mask = orlix_tcti_advsimd_integer_lane_mask(lane_bytes);
	u8 shift = (byte % 8U) * 8U;

	words[byte / 8U] &= ~(mask << shift);
	words[byte / 8U] |= (value & mask) << shift;
}

static u64 orlix_tcti_advsimd_integer_sat_signed(__int128 value, u8 bits,
						 bool *qc)
{
	__int128 max;
	__int128 min;
	u64 lane_mask = orlix_tcti_advsimd_integer_lane_mask(bits / 8);

	if (bits == 64) {
		max = S64_MAX;
		min = S64_MIN;
	} else {
		max = ((__int128)1 << (bits - 1)) - 1;
		min = -((__int128)1 << (bits - 1));
	}
	if (value > max) {
		*qc = true;
		return (u64)max & lane_mask;
	}
	if (value < min) {
		*qc = true;
		return (u64)min & lane_mask;
	}
	return (u64)value & lane_mask;
}

static u64 orlix_tcti_advsimd_integer_sat_unsigned(u64 value, u8 bits, bool *qc,
						   bool overflow)
{
	u64 max = orlix_tcti_advsimd_integer_lane_mask(bits / 8);

	if (overflow) {
		*qc = true;
		return max;
	}
	return value & max;
}

static u64 orlix_tcti_advsimd_integer_sat_shift(u64 a, s64 sh, u8 bits,
					       bool is_unsigned,
					       bool signed_to_unsigned,
					       bool rounding, bool *qc)
{
	u64 mask = orlix_tcti_advsimd_integer_lane_mask(bits / 8);
	s64 sa = sign_extend64(a, bits - 1);
	s64 maximum = bits == 64 ? S64_MAX : (s64)BIT_ULL(bits - 1) - 1;
	s64 minimum = bits == 64 ? S64_MIN : -(s64)BIT_ULL(bits - 1);

	a &= mask;
	if (signed_to_unsigned && sa < 0) {
		*qc = true;
		return 0;
	}
	if (sh > 0) {
		u8 n = (u8)sh;

		if (signed_to_unsigned) {
			if (n >= bits ? sa != 0 : (u64)sa > (mask >> n)) {
				*qc = true;
				return mask;
			}
			return ((u64)sa << n) & mask;
		}
		if (is_unsigned) {
			if (n >= bits ? a != 0 : a > (mask >> n)) {
				*qc = true;
				return mask;
			}
			return (a << n) & mask;
		}
		if (n >= bits) {
			if (!sa)
				return 0;
			*qc = true;
			return sa > 0 ? (u64)maximum & mask :
					(u64)minimum & mask;
		}
		if (sa > (maximum >> n)) {
			*qc = true;
			return (u64)maximum & mask;
		}
		if (sa < (minimum >> n)) {
			*qc = true;
			return (u64)minimum & mask;
		}
		return ((u64)sa << n) & mask;
	}

	{
		u8 r = (u8)(-sh);
		u64 shifted;

		if (r > bits)
			shifted = rounding ? 0 :
				(is_unsigned || signed_to_unsigned ||
				 sa >= 0 ? 0 : mask);
		else if (r == bits)
			shifted = rounding ?
				((is_unsigned || signed_to_unsigned) &&
				 (a & BIT_ULL(bits - 1)) ? 1 : 0) :
				(is_unsigned || signed_to_unsigned ||
				 sa >= 0 ? 0 : mask);
		else {
			if (is_unsigned || signed_to_unsigned)
				shifted = a >> r;
			else
				shifted = ((u64)(sa >> r)) & mask;
			if (rounding && r && (a & BIT_ULL(r - 1)))
				shifted++;
		}
		return shifted & mask;
	}
}

static int orlix_tcti_advsimd_integer_expected_from_source(
	const struct orlix_tcti_test_integer_source *source, u32 instruction,
	const u64 rn[2], const u64 rm[2], const u64 rd[2],
	unsigned long fpsr_in, u64 expected_rd[2], unsigned long *expected_fpsr)
{
	const char *m = source->mnemonic;
	const char *name = source->name;
	bool scalar = strstr(name, "asisd") != NULL;
	bool q = instruction & BIT(30);
	u8 size = (instruction >> 22) & 3U;
	u8 lane_bytes = 1U << size;
	u8 result_bytes = scalar ? sizeof(u64) : (q ? 2 * sizeof(u64) : sizeof(u64));
	u8 lanes;
	u8 lane;
	bool qc = false;
	u64 left[2] = { rn[0], rn[1] };
	u64 right[2] = { rm[0], rm[1] };
	u64 acc[2] = { rd[0], rd[1] };

	if (strstr(name, "asimdshf") || strstr(name, "asisdshf")) {
		u32 immh = (instruction >> 19) & 0xfU;

		if (immh & 8U)
			lane_bytes = 8;
		else if (immh & 4U)
			lane_bytes = 4;
		else if (immh & 2U)
			lane_bytes = 2;
		else
			lane_bytes = 1;
	}
	if (!strcmp(m, "RBIT") || !strcmp(m, "CNT"))
		lane_bytes = 1;
	if (strstr(name, "_FZ") || strstr(name, "misc")) {
		right[0] = 0;
		right[1] = 0;
	}
	if (strstr(name, "asimdimm")) {
		u8 cmode = (instruction >> 12) & 0xfU;
		u8 imm8 = (((instruction >> 16) & 0x7U) << 5) |
			((instruction >> 5) & 0x1fU);
		bool opbit = instruction & BIT(29);
		u64 pattern;
		u32 lane32;

		if (cmode <= 7) {
			lane32 = (u32)imm8 << ((cmode >> 1) * 8);
			pattern = (u64)lane32 | ((u64)lane32 << 32);
		} else if (cmode <= 11) {
			u16 lane16 = (u16)imm8 << (((cmode - 8) >> 1) * 8);

			pattern = 0;
			pattern |= (u64)lane16;
			pattern |= (u64)lane16 << 16;
			pattern |= (u64)lane16 << 32;
			pattern |= (u64)lane16 << 48;
		} else if (cmode == 12 || cmode == 13) {
			u8 shift = cmode == 12 ? 8 : 16;

			lane32 = ((u32)imm8 << shift) | (BIT(shift) - 1U);
			pattern = (u64)lane32 | ((u64)lane32 << 32);
		} else {
			return -EINVAL;
		}
		if (!strcmp(m, "MVNI"))
			pattern = ~pattern;
		expected_rd[0] = !strcmp(m, "ORR") ? (rd[0] | pattern) :
			!strcmp(m, "BIC") ? (rd[0] & ~pattern) : pattern;
		expected_rd[1] = q ?
			(!strcmp(m, "ORR") ? (rd[1] | pattern) :
			 !strcmp(m, "BIC") ? (rd[1] & ~pattern) : pattern) : 0;
		*expected_fpsr = fpsr_in;
		return 0;
	}
	if (!strcmp(m, "SADDL") || !strcmp(m, "UADDL") ||
	    !strcmp(m, "SSUBL") || !strcmp(m, "USUBL") ||
	    !strcmp(m, "SMLAL") || !strcmp(m, "UMLAL") ||
	    !strcmp(m, "SMLSL") || !strcmp(m, "UMLSL") ||
	    !strcmp(m, "SMULL") || !strcmp(m, "UMULL") ||
	    !strcmp(m, "SABDL") || !strcmp(m, "UABDL") ||
	    !strcmp(m, "SABAL") || !strcmp(m, "UABAL") ||
	    !strcmp(m, "SQDMULL") || !strcmp(m, "SQDMLAL") ||
	    !strcmp(m, "SQDMLSL") || !strcmp(m, "SADDW") ||
	    !strcmp(m, "UADDW") || !strcmp(m, "SSUBW") ||
	    !strcmp(m, "USUBW") || !strcmp(m, "SSHLL") ||
	    !strcmp(m, "USHLL") || !strcmp(m, "SHLL") ||
	    !strcmp(m, "ADDHN") || !strcmp(m, "SUBHN") ||
	    !strcmp(m, "RADDHN") || !strcmp(m, "RSUBHN") ||
	    !strcmp(m, "SADDLP") || !strcmp(m, "UADDLP") ||
	    !strcmp(m, "SADALP") || !strcmp(m, "UADALP") ||
	    !strcmp(m, "ADDV") || !strcmp(m, "SADDLV") ||
	    !strcmp(m, "UADDLV") || !strcmp(m, "XTN") ||
	    !strcmp(m, "SQXTN") ||
	    !strcmp(m, "UQXTN") || !strcmp(m, "SQXTUN") ||
	    !strcmp(m, "SHRN") || !strcmp(m, "RSHRN") ||
	    !strcmp(m, "SQSHRN") || !strcmp(m, "UQSHRN") ||
	    !strcmp(m, "SQRSHRN") || !strcmp(m, "UQRSHRN") ||
	    !strcmp(m, "SQSHRUN") || !strcmp(m, "SQRSHRUN") ||
	    !strcmp(m, "URECPE") || !strcmp(m, "URSQRTE")) {
		u8 src_bytes = 1U << size;
		u8 dst_bytes;
		u8 n;
		bool qc = false;

		if (strstr(name, "asimdshf") || strstr(name, "asisdshf")) {
			u32 immh = (instruction >> 19) & 0xfU;

			src_bytes = (immh & 8U) ? 8 : (immh & 4U) ? 4 :
				(immh & 2U) ? 2 : 1;
		}
		expected_rd[0] = 0;
		expected_rd[1] = 0;
		if (!strcmp(m, "SADDW") || !strcmp(m, "UADDW") ||
		    !strcmp(m, "SSUBW") || !strcmp(m, "USUBW")) {
			dst_bytes = src_bytes * 2;
			n = 16 / dst_bytes;
			for (lane = 0; lane < n; lane++) {
				u64 wide = orlix_tcti_advsimd_integer_get_lane(
					rn, lane, dst_bytes);
				u64 narrow = orlix_tcti_advsimd_integer_get_lane(
					rm, (q ? n : 0) + lane, src_bytes);
				u64 out;

				if (m[0] == 'S') {
					s64 sa = sign_extend64(wide, dst_bytes * 8 - 1);
					s64 sb = sign_extend64(narrow, src_bytes * 8 - 1);

					out = (m[3] == 'D') ? (u64)(sa + sb) :
						(u64)(sa - sb);
				} else {
					out = (m[3] == 'D') ? wide + narrow :
						wide - narrow;
				}
				orlix_tcti_advsimd_integer_set_lane(expected_rd, lane,
					dst_bytes, out);
			}
		} else if (!strcmp(m, "ADDHN") || !strcmp(m, "SUBHN") ||
			   !strcmp(m, "RADDHN") || !strcmp(m, "RSUBHN")) {
			u8 wide_bytes = src_bytes * 2;

			if (q) {
				expected_rd[0] = rd[0];
				expected_rd[1] = 0;
			}
			n = 8 / src_bytes;
			for (lane = 0; lane < n; lane++) {
				u64 va = orlix_tcti_advsimd_integer_get_lane(
					rn, lane, wide_bytes);
				u64 vb = orlix_tcti_advsimd_integer_get_lane(
					rm, lane, wide_bytes);
				u64 sum = m[0] == 'S' || strstr(m, "SUB") ?
					va - vb : va + vb;
				u8 round = (m[0] == 'R') ? 1 : 0;
				u64 shifted = (sum + (round ? BIT_ULL(src_bytes * 8 - 1) : 0)) >>
					(src_bytes * 8);
				u8 dest_lane = q ? n + lane : lane;

				orlix_tcti_advsimd_integer_set_lane(expected_rd,
					dest_lane, src_bytes,
					shifted & orlix_tcti_advsimd_integer_lane_mask(src_bytes));
			}
		} else if (!strcmp(m, "ADDV") || !strcmp(m, "SADDLV") ||
			   !strcmp(m, "UADDLV")) {
			u64 acc_sum = 0;
			s64 sacc = 0;
			u8 count = (q ? 16 : 8) / src_bytes;

			for (lane = 0; lane < count; lane++) {
				u64 v = orlix_tcti_advsimd_integer_get_lane(
					rn, lane, src_bytes);

				acc_sum += v;
				sacc += sign_extend64(v, src_bytes * 8 - 1);
			}
			if (!strcmp(m, "ADDV"))
				orlix_tcti_advsimd_integer_set_lane(expected_rd, 0,
					src_bytes, acc_sum);
			else if (!strcmp(m, "SADDLV"))
				orlix_tcti_advsimd_integer_set_lane(expected_rd, 0,
					src_bytes * 2, (u64)sacc);
			else
				orlix_tcti_advsimd_integer_set_lane(expected_rd, 0,
					src_bytes * 2, acc_sum);
		} else if (!strcmp(m, "SADDLP") || !strcmp(m, "UADDLP") ||
			   !strcmp(m, "SADALP") || !strcmp(m, "UADALP")) {
			u8 dst_b = src_bytes * 2;
			u8 pairs = (q ? 16 : 8) / dst_b;

			if (!strcmp(m, "SADALP") || !strcmp(m, "UADALP")) {
				expected_rd[0] = rd[0];
				expected_rd[1] = q ? rd[1] : 0;
			}
			for (lane = 0; lane < pairs; lane++) {
				u64 a = orlix_tcti_advsimd_integer_get_lane(
					rn, lane * 2U, src_bytes);
				u64 b = orlix_tcti_advsimd_integer_get_lane(
					rn, lane * 2U + 1U, src_bytes);
				u64 out;
				u64 accv;

				if (m[0] == 'S')
					out = (u64)(sign_extend64(a, src_bytes * 8 - 1) +
						sign_extend64(b, src_bytes * 8 - 1));
				else
					out = a + b;
				if (!strcmp(m, "SADALP") || !strcmp(m, "UADALP")) {
					accv = orlix_tcti_advsimd_integer_get_lane(
						rd, lane, dst_b);
					out += accv;
				}
				orlix_tcti_advsimd_integer_set_lane(expected_rd, lane,
					dst_b, out);
			}
			if (!q)
				expected_rd[1] = 0;
		} else if (!strcmp(m, "URECPE") || !strcmp(m, "URSQRTE")) {
			u8 lane32;
			u8 n32 = q ? 4U : 2U;

			expected_rd[0] = 0;
			expected_rd[1] = 0;
			for (lane32 = 0; lane32 < n32; lane32++) {
				u32 in = (u32)orlix_tcti_advsimd_integer_get_lane(
					rn, lane32, 4);
				u32 out = !strcmp(m, "URECPE") ?
					orlix_tcti_advsimd_integer_urecpe_u32(in) :
					orlix_tcti_advsimd_integer_ursqrte_u32(in);

				orlix_tcti_advsimd_integer_set_lane(expected_rd,
					lane32, 4, out);
			}
			*expected_fpsr = fpsr_in;
			return 0;
		} else {
			bool narrow_sat = !strcmp(m, "XTN") ||
				!strcmp(m, "SQXTN") || !strcmp(m, "UQXTN") ||
				!strcmp(m, "SQXTUN") || strstr(m, "SHRN") ||
				strstr(m, "SHRUN");
			u32 immh = (instruction >> 19) & 0xfU;
			u32 immb = (instruction >> 16) & 0x7U;
			u32 imm = (immh << 3) | immb;

			if (narrow_sat) {
				u8 wide_bytes = src_bytes * 2;
				u8 n_src = scalar ? 1 : (8 / src_bytes);
				u8 shr = (u8)(wide_bytes * 8 - imm);

				if (scalar) {
					expected_rd[0] = 0;
					expected_rd[1] = 0;
				} else if (q) {
					expected_rd[0] = rd[0];
					expected_rd[1] = 0;
				}
				for (lane = 0; lane < n_src; lane++) {
					u64 src = orlix_tcti_advsimd_integer_get_lane(
						rn, lane, wide_bytes);
					s64 sval = sign_extend64(src, wide_bytes * 8 - 1);
					u64 shifted;
					u8 dest_lane = (scalar || !q) ? lane :
						n_src + lane;
					u64 out;

					if (!strcmp(m, "SHRN") ||
					    !strcmp(m, "RSHRN")) {
						shifted = shr >= wide_bytes * 8 ?
							0 : (src >> shr);
						if (!strcmp(m, "RSHRN") && shr &&
						    shr <= wide_bytes * 8 &&
						    (src & BIT_ULL(shr - 1)))
							shifted++;
						out = shifted & orlix_tcti_advsimd_integer_lane_mask(src_bytes);
					} else if (!strcmp(m, "SQXTN")) {
						out = orlix_tcti_advsimd_integer_sat_signed(
							sval, src_bytes * 8, &qc);
					} else if (!strcmp(m, "SQXTUN")) {
						u64 umax = orlix_tcti_advsimd_integer_lane_mask(src_bytes);

						if (sval < 0) {
							out = 0;
							qc = true;
						} else if ((u64)sval > umax) {
							out = umax;
							qc = true;
						} else {
							out = (u64)sval;
						}
					} else if (!strcmp(m, "UQXTN")) {
						out = src > orlix_tcti_advsimd_integer_lane_mask(src_bytes) ?
							(qc = true,
							 orlix_tcti_advsimd_integer_lane_mask(src_bytes)) :
							src;
					} else if (!strcmp(m, "XTN")) {
						out = src & orlix_tcti_advsimd_integer_lane_mask(src_bytes);
					} else {
						s64 sshifted = shr >= wide_bytes * 8 ?
							(sval < 0 ? -1 : 0) :
							(sval >> shr);
						if (strstr(m, "RSH") && shr &&
						    shr <= wide_bytes * 8 &&
						    (src & BIT_ULL(shr - 1)))
							sshifted++;
						if (strstr(m, "SHRUN")) {
							u64 umax = orlix_tcti_advsimd_integer_lane_mask(src_bytes);

							if (sshifted < 0) {
								out = 0;
								qc = true;
							} else if ((u64)sshifted > umax) {
								out = umax;
								qc = true;
							} else {
								out = (u64)sshifted;
							}
						} else if (m[0] == 'U') {
							u64 umax = orlix_tcti_advsimd_integer_lane_mask(src_bytes);
							u64 ushifted = shr >= wide_bytes * 8 ?
								0 : (src >> shr);

							if (strstr(m, "RSH") && shr &&
							    shr <= wide_bytes * 8 &&
							    (src & BIT_ULL(shr - 1)))
								ushifted++;
							if (ushifted > umax) {
								out = umax;
								qc = true;
							} else {
								out = ushifted;
							}
						} else
							out = orlix_tcti_advsimd_integer_sat_signed(
								sshifted, src_bytes * 8, &qc);
					}
					orlix_tcti_advsimd_integer_set_lane(expected_rd,
						dest_lane, src_bytes, out);
				}
			} else {
				dst_bytes = src_bytes * 2;
				n = scalar ? 1 : (16 / dst_bytes);
				if (scalar) {
					expected_rd[0] = 0;
					expected_rd[1] = 0;
				} else if (!strcmp(m, "SMLAL") || !strcmp(m, "UMLAL") ||
				    !strcmp(m, "SMLSL") || !strcmp(m, "UMLSL") ||
				    !strcmp(m, "SABAL") || !strcmp(m, "UABAL") ||
				    !strcmp(m, "SQDMLAL") || !strcmp(m, "SQDMLSL")) {
					expected_rd[0] = rd[0];
					expected_rd[1] = q ? rd[1] : 0;
				}
				for (lane = 0; lane < n; lane++) {
					u8 src_lane = (!scalar && q) ? (n + lane) : lane;
					u64 va = orlix_tcti_advsimd_integer_get_lane(
						rn, src_lane, src_bytes);
					u64 vb = (strstr(name, "asimdelem") ||
						  strstr(name, "asisdelem")) ?
						orlix_tcti_advsimd_integer_get_lane(
							rm,
							orlix_tcti_advsimd_integer_elem_index(
								instruction, size),
							src_bytes) :
						orlix_tcti_advsimd_integer_get_lane(
							rm, src_lane, src_bytes);
					s64 sa = sign_extend64(va, src_bytes * 8 - 1);
					s64 sb = sign_extend64(vb, src_bytes * 8 - 1);
					u64 accv = orlix_tcti_advsimd_integer_get_lane(
						rd, lane, dst_bytes);
					s64 sacc = sign_extend64(accv, dst_bytes * 8 - 1);
					u64 out;
					u32 immh = (instruction >> 19) & 0xfU;
					u32 immb = (instruction >> 16) & 0x7U;
					u32 imm = (immh << 3) | immb;
					u8 shl = (u8)(imm - src_bytes * 8);

					if (!strcmp(m, "SADDL") || !strcmp(m, "SADDW"))
						out = (u64)(sa + sb);
					else if (!strcmp(m, "UADDL") || !strcmp(m, "UADDW"))
						out = va + vb;
					else if (!strcmp(m, "SSUBL"))
						out = (u64)(sa - sb);
					else if (!strcmp(m, "USUBL"))
						out = va - vb;
					else if (!strcmp(m, "SMULL"))
						out = (u64)(sa * sb);
					else if (!strcmp(m, "UMULL"))
						out = va * vb;
					else if (!strcmp(m, "SMLAL"))
						out = (u64)(sacc + sa * sb);
					else if (!strcmp(m, "UMLAL"))
						out = accv + va * vb;
					else if (!strcmp(m, "SMLSL"))
						out = (u64)(sacc - sa * sb);
					else if (!strcmp(m, "UMLSL"))
						out = accv - va * vb;
					else if (!strcmp(m, "SABDL"))
						out = (u64)(sa >= sb ? sa - sb : sb - sa);
					else if (!strcmp(m, "UABDL"))
						out = va >= vb ? va - vb : vb - va;
					else if (!strcmp(m, "SABAL"))
						out = (u64)(sacc + (sa >= sb ? sa - sb : sb - sa));
					else if (!strcmp(m, "UABAL"))
						out = accv + (va >= vb ? va - vb : vb - va);
					else if (!strcmp(m, "SQDMULL"))
						out = orlix_tcti_advsimd_integer_sat_signed(
							(__int128)sa * (__int128)sb * 2,
							dst_bytes * 8, &qc);
					else if (!strcmp(m, "SQDMLAL") ||
						 !strcmp(m, "SQDMLSL")) {
						u64 product =
							orlix_tcti_advsimd_integer_sat_signed(
								(__int128)sa *
								(__int128)sb * 2,
								dst_bytes * 8, &qc);
						s64 sp = sign_extend64(product,
								       dst_bytes * 8 - 1);

						out = orlix_tcti_advsimd_integer_sat_signed(
							!strcmp(m, "SQDMLSL") ?
							(__int128)sacc - sp :
							(__int128)sacc + sp,
							dst_bytes * 8, &qc);
					}
					else if (!strcmp(m, "SSHLL") || !strcmp(m, "SHLL"))
						out = (u64)sa << (strcmp(m, "SHLL") ? shl : src_bytes * 8);
					else if (!strcmp(m, "USHLL"))
						out = va << shl;
					else
						return -EINVAL;
					orlix_tcti_advsimd_integer_set_lane(expected_rd,
						lane, dst_bytes, out);
				}
			}
		}
		*expected_fpsr = fpsr_in;
		if (qc)
			*expected_fpsr |= AARCH64_FPSR_QC;
		return 0;
	}
	lanes = scalar ? 1 : (result_bytes / lane_bytes);
	if (!lanes)
		return -EINVAL;
	expected_rd[0] = scalar || !q ? 0 : rd[0];
	expected_rd[1] = (!scalar && q) ? rd[1] : 0;
	if (strstr(name, "asimdmisc") || strstr(name, "asisdmisc") ||
	    strstr(name, "asimdsame") || strstr(name, "asisdsame") ||
	    strstr(name, "asimdshf") || strstr(name, "asisdshf") ||
	    strstr(name, "asimdelem") || strstr(name, "asisdelem") ||
	    strstr(name, "asimddiff") || strstr(name, "asisddiff") ||
	    strstr(name, "asimdimm") || strstr(name, "asimdall") ||
	    strstr(name, "asisdpair"))
		expected_rd[0] = 0;
	if (!q || scalar)
		expected_rd[1] = 0;

	if (!strcmp(m, "SMAXP") || !strcmp(m, "SMINP") ||
	    !strcmp(m, "UMAXP") || !strcmp(m, "UMINP") ||
	    (!strcmp(m, "ADDP") && !strstr(name, "pair"))) {
		u8 n = (q ? 16U : 8U) / lane_bytes;
		u8 half = n / 2U;
		u64 mask = orlix_tcti_advsimd_integer_lane_mask(lane_bytes);
		u8 bits = lane_bytes * 8;

		expected_rd[0] = 0;
		expected_rd[1] = 0;
		for (lane = 0; lane < n; lane++) {
			const u64 *src = (lane < half) ? left : right;
			u8 pair = lane % half;
			u64 a = orlix_tcti_advsimd_integer_get_lane(src, pair * 2U,
								    lane_bytes);
			u64 b = orlix_tcti_advsimd_integer_get_lane(src,
								    pair * 2U + 1U,
								    lane_bytes);
			s64 sa = sign_extend64(a, bits - 1);
			s64 sb = sign_extend64(b, bits - 1);
			u64 out;

			if (!strcmp(m, "SMAXP"))
				out = sa >= sb ? a : b;
			else if (!strcmp(m, "SMINP"))
				out = sa <= sb ? a : b;
			else if (!strcmp(m, "UMAXP"))
				out = a >= b ? a : b;
			else if (!strcmp(m, "UMINP"))
				out = a <= b ? a : b;
			else
				out = (a + b) & mask;
			orlix_tcti_advsimd_integer_set_lane(expected_rd, lane,
							    lane_bytes, out);
		}
		*expected_fpsr = fpsr_in;
		return 0;
	}
	if (!strcmp(m, "SMAXV") || !strcmp(m, "SMINV") ||
	    !strcmp(m, "UMAXV") || !strcmp(m, "UMINV") ||
	    !strcmp(m, "ADDV")) {
		u8 n = (q ? 16U : 8U) / lane_bytes;
		u64 acc = orlix_tcti_advsimd_integer_get_lane(left, 0, lane_bytes);
		u64 mask = orlix_tcti_advsimd_integer_lane_mask(lane_bytes);
		u8 bits = lane_bytes * 8;
		s64 sacc = sign_extend64(acc, bits - 1);

		for (lane = 1; lane < n; lane++) {
			u64 v = orlix_tcti_advsimd_integer_get_lane(left, lane,
								    lane_bytes);
			s64 sv = sign_extend64(v, bits - 1);

			if (!strcmp(m, "SMAXV") && sv > sacc) {
				acc = v;
				sacc = sv;
			} else if (!strcmp(m, "SMINV") && sv < sacc) {
				acc = v;
				sacc = sv;
			} else if (!strcmp(m, "UMAXV") && v > acc)
				acc = v;
			else if (!strcmp(m, "UMINV") && v < acc)
				acc = v;
			else if (!strcmp(m, "ADDV"))
				acc = (acc + v) & mask;
		}
		expected_rd[0] = 0;
		expected_rd[1] = 0;
		orlix_tcti_advsimd_integer_set_lane(expected_rd, 0, lane_bytes, acc);
		*expected_fpsr = fpsr_in;
		return 0;
	}
	if (strstr(name, "asimdelem") || strstr(name, "asisdelem")) {
		u8 index = orlix_tcti_advsimd_integer_elem_index(instruction, size);
		u64 elem = orlix_tcti_advsimd_integer_get_lane(rm, index, lane_bytes);

		right[0] = 0;
		right[1] = 0;
		for (lane = 0; lane < lanes; lane++)
			orlix_tcti_advsimd_integer_set_lane(right, lane, lane_bytes,
							    elem);
	}

	for (lane = 0; lane < lanes; lane++) {
		u64 a = orlix_tcti_advsimd_integer_get_lane(left, lane, lane_bytes);
		u64 b = orlix_tcti_advsimd_integer_get_lane(right, lane, lane_bytes);
		u64 d = orlix_tcti_advsimd_integer_get_lane(acc, lane, lane_bytes);
		u64 mask = orlix_tcti_advsimd_integer_lane_mask(lane_bytes);
		u8 bits = lane_bytes * 8;
		s64 sa = sign_extend64(a, bits - 1);
		s64 sb = sign_extend64(b, bits - 1);
		s64 sd = sign_extend64(d, bits - 1);
		u64 out = 0;
		u32 immh = (instruction >> 19) & 0xfU;
		u32 immb = (instruction >> 16) & 0x7U;
		u32 imm = (immh << 3) | immb;
		u8 esize_bits = lane_bytes * 8;
		u8 shl = (u8)(imm - esize_bits);
		u8 shr = (u8)(esize_bits * 2U - imm);

		if (!strcmp(m, "ADDP") && strstr(name, "pair")) {
			u64 p0 = orlix_tcti_advsimd_integer_get_lane(left, 0, lane_bytes);
			u64 p1 = orlix_tcti_advsimd_integer_get_lane(left, 1, lane_bytes);

			out = (p0 + p1) & mask;
			orlix_tcti_advsimd_integer_set_lane(expected_rd, 0, lane_bytes,
							    out);
			break;
		} else if (!strcmp(m, "ADD") || !strcmp(m, "ADDP"))
			out = (a + b) & mask;
		else if (!strcmp(m, "SUB"))
			out = (a - b) & mask;
		else if (!strcmp(m, "AND"))
			out = a & b;
		else if (!strcmp(m, "ORR"))
			out = a | b;
		else if (!strcmp(m, "EOR"))
			out = a ^ b;
		else if (!strcmp(m, "BIC"))
			out = a & ~b;
		else if (!strcmp(m, "ORN"))
			out = a | ~b;
		else if (!strcmp(m, "NOT"))
			out = ~a;
		else if (!strcmp(m, "BSL"))
			out = (a & d) | (b & ~d);
		else if (!strcmp(m, "BIT"))
			out = (d & ~b) | (a & b);
		else if (!strcmp(m, "BIF"))
			out = (d & b) | (a & ~b);
		else if (!strcmp(m, "MUL"))
			out = (a * b) & mask;
		else if (!strcmp(m, "MLA"))
			out = (d + (a * b)) & mask;
		else if (!strcmp(m, "MLS"))
			out = (d - (a * b)) & mask;
		else if (!strcmp(m, "PMUL")) {
			u64 p = 0;
			u64 aa = a;
			u64 bb = b;
			u8 bit;

			for (bit = 0; bit < bits; bit++)
				if (bb & BIT_ULL(bit))
					p ^= aa << bit;
			out = p & mask;
		} else if (!strcmp(m, "CMEQ"))
			out = (strstr(name, "_FZ") ? a == 0 : a == b) ? mask : 0;
		else if (!strcmp(m, "CMGT"))
			out = (strstr(name, "_FZ") ? (sa > 0) : (sa > sb)) ? mask : 0;
		else if (!strcmp(m, "CMGE"))
			out = (strstr(name, "_FZ") ? (sa >= 0) : (sa >= sb)) ? mask : 0;
		else if (!strcmp(m, "CMHI"))
			out = (a > b) ? mask : 0;
		else if (!strcmp(m, "CMHS"))
			out = (a >= b) ? mask : 0;
		else if (!strcmp(m, "CMLT"))
			out = (sa < 0) ? mask : 0;
		else if (!strcmp(m, "CMLE"))
			out = (sa <= 0) ? mask : 0;
		else if (!strcmp(m, "CMTST"))
			out = (a & b) ? mask : 0;
		else if (!strcmp(m, "SMAX"))
			out = sa >= sb ? a : b;
		else if (!strcmp(m, "SMIN"))
			out = sa <= sb ? a : b;
		else if (!strcmp(m, "UMAX"))
			out = a >= b ? a : b;
		else if (!strcmp(m, "UMIN"))
			out = a <= b ? a : b;
		else if (!strcmp(m, "ABS"))
			out = sa < 0 ? (0U - a) & mask : a;
		else if (!strcmp(m, "NEG"))
			out = (0U - a) & mask;
		else if (!strcmp(m, "SQABS"))
			out = orlix_tcti_advsimd_integer_sat_signed(
				sa < 0 ? -(__int128)sa : sa, bits, &qc);
		else if (!strcmp(m, "SQNEG"))
			out = orlix_tcti_advsimd_integer_sat_signed(
				-(__int128)sa, bits, &qc);
		else if (!strcmp(m, "SQADD"))
			out = orlix_tcti_advsimd_integer_sat_signed(
				(__int128)sa + (__int128)sb, bits, &qc);
		else if (!strcmp(m, "SQSUB"))
			out = orlix_tcti_advsimd_integer_sat_signed(
				(__int128)sa - (__int128)sb, bits, &qc);
		else if (!strcmp(m, "UQADD"))
			out = orlix_tcti_advsimd_integer_sat_unsigned(
				a + b, bits, &qc,
				(unsigned __int128)a + (unsigned __int128)b >
					mask);
		else if (!strcmp(m, "UQSUB"))
			out = a >= b ? a - b : (qc = true, 0);
		else if (!strcmp(m, "SUQADD")) {
			out = orlix_tcti_advsimd_integer_sat_signed(
				(__int128)sd + (__int128)a, bits, &qc);
		} else if (!strcmp(m, "USQADD")) {
			__int128 sum = (__int128)d + (__int128)sa;

			if (sum > (__int128)mask) {
				qc = true;
				out = mask;
			} else if (sum < 0) {
				qc = true;
				out = 0;
			} else {
				out = (u64)sum;
			}
		} else if (!strcmp(m, "SHADD"))
			out = ((sa + sb) >> 1) & mask;
		else if (!strcmp(m, "UHADD"))
			out = ((a + b) >> 1) & mask;
		else if (!strcmp(m, "SRHADD"))
			out = ((sa + sb + 1) >> 1) & mask;
		else if (!strcmp(m, "URHADD"))
			out = ((a + b + 1) >> 1) & mask;
		else if (!strcmp(m, "SHSUB"))
			out = ((sa - sb) >> 1) & mask;
		else if (!strcmp(m, "UHSUB"))
			out = ((a - b) >> 1) & mask;
		else if (!strcmp(m, "SABD"))
			out = (sa >= sb ? sa - sb : sb - sa) & mask;
		else if (!strcmp(m, "UABD"))
			out = (a >= b ? a - b : b - a) & mask;
		else if (!strcmp(m, "SABA"))
			out = (d + (sa >= sb ? sa - sb : sb - sa)) & mask;
		else if (!strcmp(m, "UABA"))
			out = (d + (a >= b ? a - b : b - a)) & mask;
		else if (!strcmp(m, "CLS")) {
			u8 count = 0;
			u8 bit;
			u64 sign = (a >> (bits - 1)) & 1U;

			for (bit = bits - 1; bit > 0; bit--) {
				if (((a >> (bit - 1)) & 1U) != sign)
					break;
				count++;
			}
			out = count;
		} else if (!strcmp(m, "CLZ")) {
			u8 count = 0;
			u8 bit;

			for (bit = bits; bit > 0; bit--) {
				if (a & BIT_ULL(bit - 1))
					break;
				count++;
			}
			out = count;
		} else if (!strcmp(m, "CNT")) {
			out = hweight64(a & mask);
		} else if (!strcmp(m, "RBIT")) {
			u64 r = 0;
			u8 bit;

			for (bit = 0; bit < bits; bit++)
				if (a & BIT_ULL(bit))
					r |= BIT_ULL(bits - 1 - bit);
			out = r;
		} else if (!strcmp(m, "SHL"))
			out = shl >= bits ? 0 : (a << shl) & mask;
		else if (!strcmp(m, "SSHL")) {
			s64 sh = sign_extend64(b & 0xff, 7);

			if (sh >= bits)
				out = 0;
			else if (sh > 0)
				out = ((u64)sa << (u8)sh) & mask;
			else if (-sh >= bits)
				out = sa < 0 ? mask : 0;
			else
				out = (sa >> (-sh)) & mask;
		} else if (!strcmp(m, "USHL")) {
			s64 sh = sign_extend64(b & 0xff, 7);

			if (sh >= bits || -sh >= bits)
				out = 0;
			else if (sh > 0)
				out = (a << sh) & mask;
			else
				out = a >> (-sh);
		} else if (!strcmp(m, "SSHR") || !strcmp(m, "SRSHR")) {
			s64 shifted = shr >= bits ? (sa < 0 ? -1 : 0) : (sa >> shr);

			if (!strcmp(m, "SRSHR") && shr && shr <= bits &&
			    (sa & BIT_ULL(shr - 1)))
				shifted++;
			out = shifted & mask;
		} else if (!strcmp(m, "USHR") || !strcmp(m, "URSHR")) {
			u64 shifted = shr >= bits ? 0 : (a >> shr);

			if (!strcmp(m, "URSHR") && shr && shr <= bits &&
			    (a & BIT_ULL(shr - 1)))
				shifted++;
			out = shifted & mask;
		} else if (!strcmp(m, "SSRA") || !strcmp(m, "SRSRA")) {
			s64 shifted = shr >= bits ? (sa < 0 ? -1 : 0) : (sa >> shr);

			if (!strcmp(m, "SRSRA") && shr && shr <= bits &&
			    (sa & BIT_ULL(shr - 1)))
				shifted++;
			out = (sd + shifted) & mask;
		} else if (!strcmp(m, "USRA") || !strcmp(m, "URSRA")) {
			u64 shifted = shr >= bits ? 0 : (a >> shr);

			if (!strcmp(m, "URSRA") && shr && shr <= bits &&
			    (a & BIT_ULL(shr - 1)))
				shifted++;
			out = (d + shifted) & mask;
		} else if (!strcmp(m, "SLI")) {
			u64 insert = shl >= bits ? 0 : (a << shl) & mask;
			u64 keep = shl >= bits ? 0 : (~(mask << shl)) & mask;

			out = (d & keep) | insert;
		}
		else if (!strcmp(m, "SRI")) {
			u64 insert = shr >= bits ? 0 : (a >> shr);
			u64 keep = shr >= bits ? mask :
				((mask << (bits - shr)) & mask);

			out = (d & keep) | insert;
		} else if (!strcmp(m, "SQSHL") || !strcmp(m, "UQSHL") ||
			   !strcmp(m, "SQSHLU") || !strcmp(m, "SQRSHL") ||
			   !strcmp(m, "UQRSHL") || !strcmp(m, "SRSHL") ||
			   !strcmp(m, "URSHL")) {
			s64 sh = strstr(name, "shf") ?
				(s64)shl : sign_extend64(b & 0xff, 7);
			bool saturating = strchr(m, 'Q') != NULL;
			bool rounding = strstr(m, "RSHL") || strstr(m, "QRSHL");
			bool is_unsigned = m[0] == 'U';
			bool signed_to_unsigned = !strcmp(m, "SQSHLU");

			if (saturating)
				out = orlix_tcti_advsimd_integer_sat_shift(
					a, sh, bits, is_unsigned,
					signed_to_unsigned, rounding, &qc);
			else if (sh > 0)
				out = sh >= bits ? 0 : (a << sh) & mask;
			else {
				u8 r = (u8)(-sh);

				if (r > bits)
					out = rounding ? 0 :
						(is_unsigned || sa >= 0 ? 0 :
						 mask);
				else if (r == bits)
					out = rounding ?
						(is_unsigned &&
						 (a & BIT_ULL(bits - 1)) ? 1 :
						  0) :
						(is_unsigned || sa >= 0 ? 0 :
						 mask);
				else {
					out = (is_unsigned ? (a >> r) :
					       ((u64)(sa >> r))) & mask;
					if (rounding && r &&
					    (a & BIT_ULL(r - 1)))
						out = (out + 1) & mask;
				}
			}
		} else if (!strcmp(m, "SQDMULH") || !strcmp(m, "SQRDMULH")) {
			s64 minv = -(s64)BIT_ULL(bits - 1);
			s64 maxv = (s64)BIT_ULL(bits - 1) - 1;
			s64 product;

			if (sa == minv && sb == minv) {
				qc = true;
				out = (u64)maxv & mask;
			} else {
				product = sa * sb;
				if (!strcmp(m, "SQRDMULH"))
					product += (s64)BIT_ULL(bits - 2);
				out = ((u64)(product >> (bits - 1))) & mask;
			}
		} else {
			return -EINVAL;
		}
		orlix_tcti_advsimd_integer_set_lane(expected_rd, lane, lane_bytes,
						    out);
	}
	if (!q || scalar)
		expected_rd[1] = 0;
	*expected_fpsr = fpsr_in;
	if (qc)
		*expected_fpsr |= AARCH64_FPSR_QC;
	return 0;
}

static void orlix_tcti_advsimd_integer_seed_simd(void)
{
	size_t index;

	current->thread.user_simd_valid = 1;
	current->thread.user_fpcr = AARCH64_FPCR_RMODE_POSINF;
	current->thread.user_fpsr = AARCH64_FPSR_PRESERVED | AARCH64_FPSR_QC;
	for (index = 0; index < ARRAY_SIZE(current->thread.user_simd); index++) {
		u64 value = 0x0102030405060708ULL ^ ((u64)index << 32);

		if (index == INT_RN * 2U) {
			value = 0x8006050403807f81ULL;
		} else if (index == INT_RN * 2U + 1U) {
			value = 0;
		} else if (index == INT_RM * 2U) {
			value = 0x8006050401f8ff08ULL;
		} else if (index == INT_RM * 2U + 1U) {
			value = 0x0f0e0d0c0b0a0980ULL;
		}
		current->thread.user_simd[index] = value;
	}
}

static void orlix_tcti_advsimd_integer_seed(struct pt_regs *regs,
					   unsigned long code)
{
	memset(regs, 0, sizeof(*regs));
	regs->pc = code;
	regs->pstate = PSR_MODE_EL0t | PSR_Z_BIT;
	regs->syscallno = NO_SYSCALL;
	regs->regs[0] = 0x1111111111111111ULL;
	regs->regs[1] = 0x2222222222222222ULL;
	regs->regs[2] = 0x3333333333333333ULL;
	regs->regs[30] = 0x4444444444444444ULL;
	orlix_tcti_advsimd_integer_seed_simd();
}

#define ORLIX_TCTI_ADV_INT_ASSERT_SUCCESS(test, source, result, regs, code) \
	do { \
		KUNIT_ASSERT_EQ_MSG((test), ORLIX_TCTI_EXIT_SYSCALL, \
				    (result)->reason, "%s ordinal %u reason", \
				    (source)->name, (source)->ordinal); \
		KUNIT_ASSERT_EQ((test), 0L, (result)->status); \
		KUNIT_ASSERT_EQ((test), INT_SVC, (result)->instruction); \
		KUNIT_ASSERT_EQ_MSG((test), (code) + sizeof(u32), (regs)->pc, \
				    "%s ordinal %u pc", (source)->name, \
				    (source)->ordinal); \
	} while (0)

#define ORLIX_TCTI_ADV_INT_ASSERT_PRESERVED_SIMD(test, source, before_simd, rd) \
	do { \
		size_t orlix_tcti_adv_int_index; \
		for (orlix_tcti_adv_int_index = 0; \
		     orlix_tcti_adv_int_index < ARRAY_SIZE(current->thread.user_simd); \
		     orlix_tcti_adv_int_index++) { \
			if (orlix_tcti_adv_int_index / 2U == (rd)) \
				continue; \
			KUNIT_ASSERT_EQ_MSG((test), \
				(before_simd)[orlix_tcti_adv_int_index], \
				current->thread.user_simd[orlix_tcti_adv_int_index], \
				"%s preserved simd[%zu] %s", (source)->name, \
				orlix_tcti_adv_int_index, (source)->mnemonic); \
		} \
	} while (0)

#define ORLIX_TCTI_ADV_INT_ASSERT_PRESERVED_GPRS(test, source, before, after) \
	do { \
		size_t orlix_tcti_adv_int_reg; \
		for (orlix_tcti_adv_int_reg = 0; \
		     orlix_tcti_adv_int_reg < ARRAY_SIZE((before)->regs); \
		     orlix_tcti_adv_int_reg++) { \
			KUNIT_ASSERT_EQ_MSG((test), \
				(before)->regs[orlix_tcti_adv_int_reg], \
				(after)->regs[orlix_tcti_adv_int_reg], \
				"%s preserved x%zu %s", (source)->name, \
				orlix_tcti_adv_int_reg, (source)->mnemonic); \
		} \
		KUNIT_ASSERT_EQ_MSG((test), (before)->pstate, (after)->pstate, \
			"%s preserved pstate %s", (source)->name, \
			(source)->mnemonic); \
	} while (0)

static u8 orlix_tcti_advsimd_integer_rd_index(u32 instruction)
{
	return instruction & 0x1fU;
}

static u8 orlix_tcti_advsimd_integer_rn_index(u32 instruction)
{
	return (instruction >> 5) & 0x1fU;
}

static u8 orlix_tcti_advsimd_integer_rm_index(
	const struct orlix_tcti_test_integer_source *source, u32 instruction)
{
	u8 size = (instruction >> 22) & 3U;

	if (strstr(source->name, "asimdshf") || strstr(source->name, "asisdshf") ||
	    strstr(source->name, "asimdimm"))
		return INT_RM;
	if ((strstr(source->name, "asimdelem") ||
	     strstr(source->name, "asisdelem")) && size < 2U)
		return INT_RM;
	return (instruction >> 16) & 0x1fU;
}

static bool orlix_tcti_advsimd_integer_variant_is_executable(
	const struct orlix_tcti_test_integer_source *source, u32 instruction)
{
	bool scalar = instruction & BIT(28);
	bool q = instruction & BIT(30);
	u8 size = (instruction >> 22) & 3U;
	const char *m = source->mnemonic;
	const char *name = source->name;

	if ((instruction & source->mask) != source->pattern)
		return false;
	if ((size == 0U || size == 3U) &&
	    orlix_tcti_advsimd_integer_size00_reserved(source))
		return false;
	if (strstr(name, "asimdshf") || strstr(name, "asisdshf")) {
		u32 immh = (instruction >> 19) & 0xfU;

		if (!immh)
			return false;
		if (!scalar && !q && (immh & 8U))
			return false;
		if ((strstr(name, "_N") || strstr(name, "_L")) &&
		    (immh & 8U))
			return false;
	}
	if (!strcmp(m, "CNT") || !strcmp(m, "NOT") || !strcmp(m, "REV16"))
		return size == 0U;
	if (!strcmp(m, "RBIT"))
		return size == 1U;
	if (!strcmp(m, "REV32") && size > 1U)
		return false;
	if ((!strcmp(m, "CLS") || !strcmp(m, "CLZ") || !strcmp(m, "REV64")) &&
	    size == 3U)
		return false;
	if (strstr(name, "asimdall") && size >= 2U && !q)
		return false;
	/*
	 * Vector size=3 Q=0 is reserved 1D. Q=1 2D is legal for many
	 * asimdmisc leaves (ABS/NEG/SQABS/SQNEG). Dedicated decode
	 * rejects SHADD and other three-same 64-bit reserved forms.
	 */
	if (size == 3U && !scalar && !q)
		return false;
	if ((strstr(name, "asimdelem") || strstr(name, "asisdelem")) &&
	    size == 3U && (instruction & BIT(21)))
		return false;
	return true;
}

static bool orlix_tcti_advsimd_integer_instruction_matches_source(
	const struct orlix_tcti_test_integer_source *source, u32 instruction)
{
	struct orlix_tcti_decoded_instruction decoded;

	if ((instruction & source->mask) != source->pattern)
		return false;
	decoded = orlix_tcti_decode_aarch64(instruction);
	/*
	 * A wide source mask can bind a reserved size that the dedicated
	 * decoder already rejected. Promote then fills a SIMD class without
	 * access_size. Skip those shells. They are not legal variants.
	 */
	return decoded.source_ordinal == source->ordinal &&
		orlix_tcti_test_integer_decode_is_family(decoded.decode_class) &&
		decoded.access_size != 0;
}

static void orlix_tcti_advsimd_integer_compare_run(struct kunit *test,
	const struct orlix_tcti_test_integer_source *source, u32 instruction,
	struct pt_regs *regs, unsigned long code);

#define ORLIX_TCTI_ADV_INT_COMPARE_MAPPED(test, source, variant) \
	do { \
		u32 _orlix_tcti_adv_int_variant = (variant); \
		struct pt_regs _orlix_tcti_adv_int_regs = {}; \
		unsigned long _orlix_tcti_adv_int_code; \
		if (!orlix_tcti_advsimd_integer_variant_is_executable( \
			    (source), _orlix_tcti_adv_int_variant)) \
			break; \
		if (!orlix_tcti_advsimd_integer_instruction_matches_source( \
			    (source), _orlix_tcti_adv_int_variant)) \
			break; \
		_orlix_tcti_adv_int_code = orlix_tcti_advsimd_integer_map( \
			(test), _orlix_tcti_adv_int_variant); \
		orlix_tcti_advsimd_integer_seed(&_orlix_tcti_adv_int_regs, \
						_orlix_tcti_adv_int_code); \
		current->thread.user_fpsr = AARCH64_FPSR_PRESERVED; \
		orlix_tcti_advsimd_integer_compare_run((test), (source), \
			_orlix_tcti_adv_int_variant, &_orlix_tcti_adv_int_regs, \
			_orlix_tcti_adv_int_code); \
		KUNIT_EXPECT_EQ((test), 0, \
			vm_munmap(_orlix_tcti_adv_int_code, PAGE_SIZE)); \
		if ((test)->status == KUNIT_FAILURE) \
			return; \
	} while (0)

static void orlix_tcti_advsimd_integer_compare_run(struct kunit *test,
	const struct orlix_tcti_test_integer_source *source, u32 instruction,
	struct pt_regs *regs, unsigned long code)
{
	struct orlix_tcti_result result;
	struct pt_regs before_regs;
	u64 before_simd[ARRAY_SIZE(current->thread.user_simd)];
	u64 expected_rd[2];
	unsigned long expected_fpsr;
	unsigned long before_fpcr;
	u8 rd;
	u8 rn;
	u8 rm;
	int ret;

	rd = orlix_tcti_advsimd_integer_rd_index(instruction);
	rn = orlix_tcti_advsimd_integer_rn_index(instruction);
	rm = orlix_tcti_advsimd_integer_rm_index(source, instruction);
	memcpy(before_simd, current->thread.user_simd, sizeof(before_simd));
	before_regs = *regs;
	before_fpcr = current->thread.user_fpcr;
	ret = orlix_tcti_advsimd_integer_expected_from_source(source, instruction,
		&before_simd[rn * 2U], &before_simd[rm * 2U],
		&before_simd[rd * 2U], current->thread.user_fpsr,
		expected_rd, &expected_fpsr);
	KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s expected-from-source %s insn %#x",
			    source->name, source->mnemonic, instruction);
	result = orlix_tcti_resume_user(current, regs, current->mm);
	ORLIX_TCTI_ADV_INT_ASSERT_SUCCESS(test, source, &result, regs, code);
	ORLIX_TCTI_ADV_INT_ASSERT_PRESERVED_GPRS(test, source, &before_regs, regs);
	KUNIT_ASSERT_EQ_MSG(test, expected_rd[0],
			    current->thread.user_simd[rd * 2U],
			    "%s dest lo %s insn %#x", source->name, source->mnemonic,
			    instruction);
	KUNIT_ASSERT_EQ_MSG(test, expected_rd[1],
			    current->thread.user_simd[rd * 2U + 1U],
			    "%s dest hi %s insn %#x", source->name, source->mnemonic,
			    instruction);
	KUNIT_ASSERT_EQ_MSG(test, expected_fpsr, current->thread.user_fpsr,
			    "%s fpsr %s insn %#x", source->name, source->mnemonic,
			    instruction);
	KUNIT_ASSERT_EQ_MSG(test, before_fpcr, current->thread.user_fpcr,
			    "%s fpcr %s insn %#x", source->name, source->mnemonic,
			    instruction);
	ORLIX_TCTI_ADV_INT_ASSERT_PRESERVED_SIMD(test, source, before_simd, rd);
}

static void orlix_tcti_advsimd_integer_capture_run(struct kunit *test,
	const struct orlix_tcti_test_integer_source *source, u32 instruction,
	u32 obligation, struct pt_regs *regs, unsigned long code)
{
	const void *token;
	struct orlix_tcti_native_capture_session *capture = NULL;
	struct orlix_tcti_native_wire_record wire = {};
	struct orlix_tcti_target_proof_ingestion_ledger *ledger;
	struct orlix_tcti_result result;
	struct pt_regs before_regs;
	u64 before_simd[ARRAY_SIZE(current->thread.user_simd)];
	u64 expected_rd[2];
	unsigned long expected_fpsr;
	unsigned long before_fpcr;
	u8 rd;
	u8 rn;
	u8 rm;
	int ret;

	rd = orlix_tcti_advsimd_integer_rd_index(instruction);
	rn = orlix_tcti_advsimd_integer_rn_index(instruction);
	rm = orlix_tcti_advsimd_integer_rm_index(source, instruction);
	memcpy(before_simd, current->thread.user_simd, sizeof(before_simd));
	before_regs = *regs;
	before_fpcr = current->thread.user_fpcr;
	ret = orlix_tcti_advsimd_integer_expected_from_source(source, instruction,
		&before_simd[rn * 2U], &before_simd[rm * 2U],
		&before_simd[rd * 2U], current->thread.user_fpsr,
		expected_rd, &expected_fpsr);
	KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s expected-from-source %s insn %#x",
			    source->name, source->mnemonic, instruction);
	token = orlix_tcti_advsimd_integer_production_capture_token(
		source->ordinal, obligation);
	KUNIT_EXPECT_TRUE_MSG(test, token != NULL, "%s token %u", source->name,
			      obligation);
	if (!token)
		return;
	ret = orlix_tcti_native_capture_begin(token, source->ordinal, obligation,
					      &capture);
	KUNIT_EXPECT_EQ_MSG(test, 0, ret, "%s begin %u", source->name, obligation);
	result = orlix_tcti_resume_user(current, regs, current->mm);
	ORLIX_TCTI_ADV_INT_ASSERT_SUCCESS(test, source, &result, regs, code);
	ORLIX_TCTI_ADV_INT_ASSERT_PRESERVED_GPRS(test, source, &before_regs, regs);
	KUNIT_ASSERT_EQ_MSG(test, expected_rd[0],
			    current->thread.user_simd[rd * 2U],
			    "%s dest lo %s", source->name, source->mnemonic);
	KUNIT_ASSERT_EQ_MSG(test, expected_rd[1],
			    current->thread.user_simd[rd * 2U + 1U],
			    "%s dest hi %s", source->name, source->mnemonic);
	KUNIT_ASSERT_EQ_MSG(test, expected_fpsr, current->thread.user_fpsr,
			    "%s fpsr %s", source->name, source->mnemonic);
	KUNIT_ASSERT_EQ_MSG(test, before_fpcr, current->thread.user_fpcr,
			    "%s fpcr %s", source->name, source->mnemonic);
	ORLIX_TCTI_ADV_INT_ASSERT_PRESERVED_SIMD(test, source, before_simd, rd);
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

static void orlix_tcti_advsimd_integer_decodes_exact_source_cohort(
	struct kunit *test)
{
	size_t index;
	size_t count = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_integer_source *source =
			&orlix_tcti_test_sources[index];
		struct orlix_tcti_decoded_instruction decoded;
		u32 instruction;

		if (!orlix_tcti_test_is_advsimd_integer(source))
			continue;
		instruction = orlix_tcti_advsimd_integer_legal_instruction(source);
		KUNIT_ASSERT_EQ_MSG(test, source->pattern,
				instruction & source->mask, "%s", source->name);
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_EXPECT_TRUE_MSG(test,
			orlix_tcti_test_integer_decode_is_family(decoded.decode_class),
			"%s class %u insn %#x", source->name, decoded.decode_class,
			instruction);
		KUNIT_EXPECT_EQ_MSG(test, source->ordinal, decoded.source_ordinal,
				"%s %#x", source->name, instruction);
		count++;
	}
	KUNIT_EXPECT_EQ(test, INT_FAMILY_COUNT, count);
}

static void orlix_tcti_advsimd_integer_binds_pinned_ddi0602_semantics(
	struct kunit *test)
{
	size_t index;
	size_t ddi = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_integer_source *source =
			&orlix_tcti_test_sources[index];

		if (!orlix_tcti_test_is_advsimd_integer(source))
			continue;
		KUNIT_ASSERT_LT_MSG(test, source->ordinal,
			ARRAY_SIZE(orlix_tcti_test_has_ddi0602_semantics), "%s",
			source->name);
		KUNIT_EXPECT_TRUE_MSG(test,
			orlix_tcti_test_has_ddi0602_semantics[source->ordinal], "%s",
			source->name);
		ddi++;
	}
	KUNIT_EXPECT_EQ(test, INT_FAMILY_COUNT, ddi);
}

static void orlix_tcti_advsimd_integer_production_resume(struct kunit *test)
{
	size_t index;
	size_t seen = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_integer_source *source =
			&orlix_tcti_test_sources[index];
		u32 instruction;
		u32 q_bit;
		u8 default_size;
		u8 size_lo;
		u8 size_hi;
		struct orlix_tcti_decoded_instruction decoded;
		struct pt_regs regs = {};
		unsigned long code;
		bool q_free;
		bool size_free;
		bool shf;
		bool elem;
		bool imm;

		if (!orlix_tcti_test_is_el0_integer(source))
			continue;
		seen++;
		instruction = orlix_tcti_advsimd_integer_legal_instruction(source);
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_ASSERT_TRUE_MSG(test,
			orlix_tcti_test_integer_decode_is_family(decoded.decode_class),
			"%s class %u", source->name, decoded.decode_class);
		KUNIT_ASSERT_EQ_MSG(test, source->ordinal, decoded.source_ordinal,
				    "%s ordinal", source->name);
		KUNIT_EXPECT_NE_MSG(test, 0U, decoded.access_size,
				    "%s access_size op %u scalar %u imm %d class %u insn %#x",
				    source->name, decoded.simd_arithmetic_op,
				    decoded.simd_scalar, decoded.immediate,
				    decoded.decode_class, instruction);
		q_free = (source->mask & BIT(30)) == 0 &&
			!(source->pattern & BIT(28));
		shf = strstr(source->name, "asimdshf") ||
			strstr(source->name, "asisdshf");
		elem = strstr(source->name, "asimdelem") ||
			strstr(source->name, "asisdelem");
		imm = strstr(source->name, "asimdimm") != NULL;
		size_free = (source->mask & (3U << 22)) == 0 && !shf;
		default_size = (instruction >> 22) & 3U;
		size_lo = size_free ? 0U : default_size;
		size_hi = size_free ? 3U : default_size;
		for (q_bit = 0; q_bit < 2U; q_bit++) {
			u8 size;

			if (!q_free && !!q_bit != !!(instruction & BIT(30)))
				continue;
			for (size = size_lo; size <= size_hi; size++) {
				u32 variant = instruction;

				variant &= ~BIT(30);
				if (q_bit)
					variant |= BIT(30);
				if (size_free) {
					variant &= ~(3U << 22);
					variant |= ((u32)size << 22);
				}
				if (shf) {
					u8 immh;
					u8 immb;

					for (immh = 1U; immh < 16U; immh++) {
						for (immb = 0; immb < 8U; immb++) {
							u32 extra = variant;

							extra &= ~(0x7fU << 16);
							extra |= ((((u32)immh << 3) |
								   immb) << 16);
							ORLIX_TCTI_ADV_INT_COMPARE_MAPPED(
								test, source, extra);
						}
					}
				} else if (elem) {
					u8 h;
					u8 l;
					u8 mbit;

					for (h = 0; h < 2U; h++) {
						for (l = 0; l < 2U; l++) {
							for (mbit = 0; mbit < 2U; mbit++) {
								u32 extra = variant;

								if (size == 3U && l)
									continue;
								extra &= ~(BIT(11) | BIT(21) |
									   BIT(20));
								if (h)
									extra |= BIT(11);
								if (l)
									extra |= BIT(21);
								if (mbit)
									extra |= BIT(20);
								ORLIX_TCTI_ADV_INT_COMPARE_MAPPED(
									test, source, extra);
								{
									u8 rm = size >= 2U ?
										((extra >> 16) &
										 0x1fU) :
										INT_RM;
									u32 aliased =
										(extra & ~0x1fU) |
										rm;

									if (aliased != extra)
										ORLIX_TCTI_ADV_INT_COMPARE_MAPPED(
											test,
											source,
											aliased);
								}
							}
						}
					}
				} else if (imm) {
					static const u8 imm8s[] = { 0x00U, 0x5AU, 0xffU };
					size_t imm8_index;
					u8 cmode;
					u8 cmode_mask = (source->mask >> 12) & 0xfU;
					u8 cmode_pattern = (source->pattern >> 12) & 0xfU;

					for (cmode = 0; cmode < 16U; cmode++) {
						if ((cmode & cmode_mask) !=
						    (cmode_pattern & cmode_mask))
							continue;
						for (imm8_index = 0;
						     imm8_index < ARRAY_SIZE(imm8s);
						     imm8_index++) {
							u8 imm8 = imm8s[imm8_index];
							u32 extra = variant;

							extra &= ~((0x7U << 16) |
								   (0x1fU << 5) |
								   (0xfU << 12));
							extra |= ((u32)(imm8 >> 5) & 0x7U) << 16;
							extra |= ((u32)imm8 & 0x1fU) << 5;
							extra |= ((u32)cmode << 12);
							ORLIX_TCTI_ADV_INT_COMPARE_MAPPED(
								test, source, extra);
						}
					}
				} else {
					ORLIX_TCTI_ADV_INT_COMPARE_MAPPED(
						test, source, variant);
				}
			}
		}
		{
			u32 high = instruction;

			if ((source->mask & 0x1fU) == 0)
				high = (high & ~0x1fU) | 16U;
			if (!imm && (source->mask & (0x1fU << 5)) == 0)
				high = (high & ~(0x1fU << 5)) | (17U << 5);
			if (!shf && !elem && !imm &&
			    (source->mask & (0x1fU << 16)) == 0)
				high = (high & ~(0x1fU << 16)) | (18U << 16);
			if (high != instruction)
				ORLIX_TCTI_ADV_INT_COMPARE_MAPPED(test, source,
								 high);
		}
		{
			u32 alias_rn = instruction;
			u32 alias_rm = instruction;
			bool rn_free = !imm && (source->mask & (0x1fU << 5)) == 0;
			bool rm_free = !shf && !elem && !imm &&
				(source->mask & (0x1fU << 16)) == 0;
			bool rd_free = (source->mask & 0x1fU) == 0;

			if (rd_free) {
				alias_rn = (alias_rn & ~0x1fU) | 16U;
				alias_rm = (alias_rm & ~0x1fU) | 16U;
			}
			if (rn_free)
				alias_rn = (alias_rn & ~(0x1fU << 5)) | (16U << 5);
			if (rm_free)
				alias_rn = (alias_rn & ~(0x1fU << 16)) | (18U << 16);
			if (rn_free)
				alias_rm = (alias_rm & ~(0x1fU << 5)) | (17U << 5);
			if (rm_free)
				alias_rm = (alias_rm & ~(0x1fU << 16)) | (16U << 16);
			if (rd_free && rn_free && alias_rn != instruction)
				ORLIX_TCTI_ADV_INT_COMPARE_MAPPED(test, source,
								 alias_rn);
			if (rd_free && rm_free && alias_rm != instruction &&
			    alias_rm != alias_rn)
				ORLIX_TCTI_ADV_INT_COMPARE_MAPPED(test, source,
								 alias_rm);
		}
		code = orlix_tcti_advsimd_integer_map(test, instruction);
		orlix_tcti_advsimd_integer_seed(&regs, code);
		current->thread.user_fpsr = AARCH64_FPSR_PRESERVED;
		orlix_tcti_advsimd_integer_capture_run(test, source, instruction,
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD, &regs, code);
		if (test->status == KUNIT_FAILURE)
			return;
		orlix_tcti_advsimd_integer_seed(&regs, code);
		current->thread.user_fpsr = AARCH64_FPSR_PRESERVED |
			AARCH64_FPSR_QC;
		orlix_tcti_advsimd_integer_capture_run(test, source, instruction,
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC, &regs, code);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
	}
	KUNIT_EXPECT_EQ(test, INT_EL0_COUNT, seen);
}

static void orlix_tcti_advsimd_integer_non_el0_rejected(struct kunit *test)
{
	size_t index;
	size_t seen = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_integer_source *source =
			&orlix_tcti_test_sources[index];
		u32 instruction;
		struct orlix_tcti_decoded_instruction decoded;
		struct pt_regs regs = {};
		struct pt_regs before;
		struct orlix_tcti_result result;
		unsigned long code;

		if (!orlix_tcti_test_is_advsimd_integer(source) ||
		    !orlix_tcti_test_is_optional_integer_feature(source))
			continue;
		seen++;
		instruction = orlix_tcti_advsimd_integer_legal_instruction(source);
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_EXPECT_TRUE_MSG(test,
			orlix_tcti_test_integer_decode_is_family(decoded.decode_class),
			"%s class", source->name);
		KUNIT_EXPECT_EQ_MSG(test, source->ordinal, decoded.source_ordinal,
				    "%s ordinal", source->name);
		code = orlix_tcti_advsimd_integer_map(test, instruction);
		orlix_tcti_advsimd_integer_seed(&regs, code);
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
	KUNIT_EXPECT_EQ(test, INT_NON_EL0_COUNT, seen);
}

static void orlix_tcti_advsimd_integer_saturation_qc(struct kunit *test)
{
	const struct orlix_tcti_test_integer_source *sqadd =
		orlix_tcti_test_integer_name("SQADD_asimdsame_only");
	const struct orlix_tcti_test_integer_source *uqadd =
		orlix_tcti_test_integer_name("UQADD_asimdsame_only");
	const struct orlix_tcti_test_integer_source *uqsub =
		orlix_tcti_test_integer_name("UQSUB_asimdsame_only");
	const struct orlix_tcti_test_integer_source *sqshlu =
		orlix_tcti_test_integer_name("SQSHLU_asimdshf_R");
	const struct orlix_tcti_test_integer_source *sqabs =
		orlix_tcti_test_integer_name("SQABS_asimdmisc_R");
	const struct orlix_tcti_test_integer_source *sqabs_s =
		orlix_tcti_test_integer_name("SQABS_asisdmisc_R");
	const struct orlix_tcti_test_integer_source *sqneg =
		orlix_tcti_test_integer_name("SQNEG_asimdmisc_R");
	const struct orlix_tcti_test_integer_source *sqneg_s =
		orlix_tcti_test_integer_name("SQNEG_asisdmisc_R");
	const struct orlix_tcti_test_integer_source *cmeq_z_s =
		orlix_tcti_test_integer_name("CMEQ_asisdmisc_Z");
	const struct orlix_tcti_test_integer_source *cmlt_s =
		orlix_tcti_test_integer_name("CMLT_asisdmisc_Z");
	const struct orlix_tcti_test_integer_source *sqsub =
		orlix_tcti_test_integer_name("SQSUB_asimdsame_only");
	u32 insn;
	unsigned long code;
	struct pt_regs regs = {};
	struct orlix_tcti_result result;

	KUNIT_ASSERT_NOT_NULL(test, sqadd);
	KUNIT_ASSERT_NOT_NULL(test, uqadd);
	KUNIT_ASSERT_NOT_NULL(test, uqsub);
	KUNIT_ASSERT_NOT_NULL(test, sqshlu);
	KUNIT_ASSERT_NOT_NULL(test, sqabs);
	KUNIT_ASSERT_NOT_NULL(test, sqabs_s);
	KUNIT_ASSERT_NOT_NULL(test, sqneg);
	KUNIT_ASSERT_NOT_NULL(test, sqneg_s);
	KUNIT_ASSERT_NOT_NULL(test, cmeq_z_s);
	KUNIT_ASSERT_NOT_NULL(test, cmlt_s);
	KUNIT_ASSERT_NOT_NULL(test, sqsub);

	insn = (sqadd->pattern & sqadd->mask) | (INT_RN << 5) |
		(INT_RM << 16) | INT_RD | BIT(30);
	code = orlix_tcti_advsimd_integer_map(test, insn);
	orlix_tcti_advsimd_integer_seed(&regs, code);
	current->thread.user_simd[INT_RN * 2U] = 0x7f;
	current->thread.user_simd[INT_RM * 2U] = 0x01;
	current->thread.user_fpsr = 0;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0x7fULL, current->thread.user_simd[INT_RD * 2U] & 0xffULL);
	KUNIT_EXPECT_NE(test, 0UL, current->thread.user_fpsr & AARCH64_FPSR_QC);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	insn = (uqadd->pattern & uqadd->mask) | (INT_RN << 5) |
		(INT_RM << 16) | INT_RD | BIT(30);
	code = orlix_tcti_advsimd_integer_map(test, insn);
	orlix_tcti_advsimd_integer_seed(&regs, code);
	current->thread.user_simd[INT_RN * 2U] = 0xff;
	current->thread.user_simd[INT_RM * 2U] = 0x01;
	current->thread.user_fpsr = 0;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0xffULL, current->thread.user_simd[INT_RD * 2U] & 0xffULL);
	KUNIT_EXPECT_NE(test, 0UL, current->thread.user_fpsr & AARCH64_FPSR_QC);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	insn = (uqsub->pattern & uqsub->mask) | (INT_RN << 5) |
		(INT_RM << 16) | INT_RD | BIT(30);
	code = orlix_tcti_advsimd_integer_map(test, insn);
	orlix_tcti_advsimd_integer_seed(&regs, code);
	current->thread.user_simd[INT_RN * 2U] = 0x00;
	current->thread.user_simd[INT_RM * 2U] = 0x01;
	current->thread.user_fpsr = 0;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[INT_RD * 2U] & 0xffULL);
	KUNIT_EXPECT_NE(test, 0UL, current->thread.user_fpsr & AARCH64_FPSR_QC);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	/* 8-bit SQSHLU #0 saturates a negative source to 0 and sets QC. */
	insn = (sqshlu->pattern & sqshlu->mask) | (INT_RN << 5) | INT_RD |
		BIT(30) | (8U << 16);
	code = orlix_tcti_advsimd_integer_map(test, insn);
	orlix_tcti_advsimd_integer_seed(&regs, code);
	current->thread.user_simd[INT_RN * 2U] = 0x80;
	current->thread.user_fpsr = 0;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[INT_RD * 2U] & 0xffULL);
	KUNIT_EXPECT_NE(test, 0UL, current->thread.user_fpsr & AARCH64_FPSR_QC);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	insn = (sqabs->pattern & sqabs->mask) | (INT_RN << 5) | INT_RD | BIT(30);
	code = orlix_tcti_advsimd_integer_map(test, insn);
	orlix_tcti_advsimd_integer_seed(&regs, code);
	current->thread.user_simd[INT_RN * 2U] = 0x80;
	current->thread.user_fpsr = 0;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0x7fULL, current->thread.user_simd[INT_RD * 2U] & 0xffULL);
	KUNIT_EXPECT_NE(test, 0UL, current->thread.user_fpsr & AARCH64_FPSR_QC);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	insn = (sqneg->pattern & sqneg->mask) | (INT_RN << 5) | INT_RD | BIT(30);
	code = orlix_tcti_advsimd_integer_map(test, insn);
	orlix_tcti_advsimd_integer_seed(&regs, code);
	current->thread.user_simd[INT_RN * 2U] = 0x80;
	current->thread.user_fpsr = 0;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0x7fULL, current->thread.user_simd[INT_RD * 2U] & 0xffULL);
	KUNIT_EXPECT_NE(test, 0UL, current->thread.user_fpsr & AARCH64_FPSR_QC);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	insn = orlix_tcti_advsimd_integer_legal_instruction(sqabs_s) |
		(3U << 22);
	code = orlix_tcti_advsimd_integer_map(test, insn);
	orlix_tcti_advsimd_integer_seed(&regs, code);
	current->thread.user_simd[INT_RN * 2U] = 0x8000000000000000ULL;
	current->thread.user_simd[INT_RN * 2U + 1U] = 0;
	current->thread.user_fpsr = 0;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0x7fffffffffffffffULL,
			current->thread.user_simd[INT_RD * 2U]);
	KUNIT_EXPECT_NE(test, 0UL, current->thread.user_fpsr & AARCH64_FPSR_QC);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	insn = orlix_tcti_advsimd_integer_legal_instruction(sqneg_s) |
		(3U << 22);
	code = orlix_tcti_advsimd_integer_map(test, insn);
	orlix_tcti_advsimd_integer_seed(&regs, code);
	current->thread.user_simd[INT_RN * 2U] = 0x8000000000000000ULL;
	current->thread.user_simd[INT_RN * 2U + 1U] = 0;
	current->thread.user_fpsr = 0;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0x7fffffffffffffffULL,
			current->thread.user_simd[INT_RD * 2U]);
	KUNIT_EXPECT_NE(test, 0UL, current->thread.user_fpsr & AARCH64_FPSR_QC);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	insn = orlix_tcti_advsimd_integer_legal_instruction(cmeq_z_s);
	code = orlix_tcti_advsimd_integer_map(test, insn);
	orlix_tcti_advsimd_integer_seed(&regs, code);
	current->thread.user_simd[INT_RN * 2U] = 0;
	current->thread.user_simd[INT_RN * 2U + 1U] = 0;
	current->thread.user_fpsr = 0;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, ~0ULL, current->thread.user_simd[INT_RD * 2U]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	insn = orlix_tcti_advsimd_integer_legal_instruction(cmlt_s);
	code = orlix_tcti_advsimd_integer_map(test, insn);
	orlix_tcti_advsimd_integer_seed(&regs, code);
	current->thread.user_simd[INT_RN * 2U] = 0x8000000000000000ULL;
	current->thread.user_simd[INT_RN * 2U + 1U] = 0;
	current->thread.user_fpsr = 0;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, ~0ULL, current->thread.user_simd[INT_RD * 2U]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	insn = (sqsub->pattern & sqsub->mask) | (INT_RN << 5) |
		(INT_RM << 16) | INT_RD | BIT(30);
	code = orlix_tcti_advsimd_integer_map(test, insn);
	orlix_tcti_advsimd_integer_seed(&regs, code);
	current->thread.user_simd[INT_RN * 2U] = 0x80;
	current->thread.user_simd[INT_RM * 2U] = 0x01;
	current->thread.user_fpsr = 0;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0x80ULL, current->thread.user_simd[INT_RD * 2U] & 0xffULL);
	KUNIT_EXPECT_NE(test, 0UL, current->thread.user_fpsr & AARCH64_FPSR_QC);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	{
		static const char *const sqd_names[] = {
			"SQDMULH_asimdsame_only",
			"SQDMULH_asisdsame_only",
			"SQDMULH_asimdelem_R",
			"SQDMULH_asisdelem_R",
			"SQRDMULH_asimdsame_only",
			"SQRDMULH_asisdsame_only",
			"SQRDMULH_asimdelem_R",
			"SQRDMULH_asisdelem_R",
			"SQDMULL_asimddiff_L",
			"SQDMULL_asisddiff_only",
			"SQDMULL_asimdelem_L",
			"SQDMULL_asisdelem_L",
			"SQDMLAL_asimddiff_L",
			"SQDMLAL_asisddiff_only",
			"SQDMLAL_asimdelem_L",
			"SQDMLAL_asisdelem_L",
			"SQDMLSL_asimddiff_L",
			"SQDMLSL_asisddiff_only",
			"SQDMLSL_asimdelem_L",
			"SQDMLSL_asisdelem_L",
		};
		size_t sqd;

		for (sqd = 0; sqd < ARRAY_SIZE(sqd_names); sqd++) {
			const struct orlix_tcti_test_integer_source *source =
				orlix_tcti_test_integer_name(sqd_names[sqd]);
			u64 before_simd[ARRAY_SIZE(current->thread.user_simd)];
			u64 expected_rd[2];
			unsigned long expected_fpsr;
			int ret;

			KUNIT_ASSERT_NOT_NULL_MSG(test, source, "%s",
						  sqd_names[sqd]);
			insn = orlix_tcti_advsimd_integer_legal_instruction(source);
			code = orlix_tcti_advsimd_integer_map(test, insn);
			orlix_tcti_advsimd_integer_seed(&regs, code);
			current->thread.user_simd[INT_RN * 2U] =
				0x8000800080008000ULL;
			current->thread.user_simd[INT_RN * 2U + 1U] =
				0x8000800080008000ULL;
			current->thread.user_simd[INT_RM * 2U] =
				0x8000800080008000ULL;
			current->thread.user_simd[INT_RM * 2U + 1U] =
				0x8000800080008000ULL;
			if (strstr(source->name, "SQDMLAL") ||
			    strstr(source->name, "SQDMLSL")) {
				current->thread.user_simd[INT_RD * 2U] = ~0ULL;
				current->thread.user_simd[INT_RD * 2U + 1U] = ~0ULL;
			} else {
				current->thread.user_simd[INT_RD * 2U] = 0;
				current->thread.user_simd[INT_RD * 2U + 1U] = 0;
			}
			current->thread.user_fpsr = 0;
			memcpy(before_simd, current->thread.user_simd,
			       sizeof(before_simd));
			ret = orlix_tcti_advsimd_integer_expected_from_source(
				source, insn, &before_simd[INT_RN * 2U],
				&before_simd[INT_RM * 2U],
				&before_simd[INT_RD * 2U],
				current->thread.user_fpsr, expected_rd,
				&expected_fpsr);
			KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s expected",
					    source->name);
			result = orlix_tcti_resume_user(current, &regs,
							current->mm);
			KUNIT_ASSERT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL,
					    result.reason, "%s", source->name);
			KUNIT_ASSERT_EQ_MSG(test, expected_rd[0],
				current->thread.user_simd[INT_RD * 2U],
				"%s dest lo", source->name);
			KUNIT_ASSERT_EQ_MSG(test, expected_rd[1],
				current->thread.user_simd[INT_RD * 2U + 1U],
				"%s dest hi", source->name);
			KUNIT_ASSERT_EQ_MSG(test, expected_fpsr,
					    current->thread.user_fpsr,
					    "%s fpsr", source->name);
			KUNIT_ASSERT_NE_MSG(test, 0UL,
				current->thread.user_fpsr & AARCH64_FPSR_QC,
				"%s qc", source->name);
			KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
		}
	}
}

static void orlix_tcti_advsimd_integer_narrow_shift_overlap(struct kunit *test)
{
	const struct orlix_tcti_test_integer_source *xtn =
		orlix_tcti_test_integer_name("XTN_asimdmisc_N");
	const struct orlix_tcti_test_integer_source *sshl =
		orlix_tcti_test_integer_name("SSHL_asimdsame_only");
	const struct orlix_tcti_test_integer_source *add =
		orlix_tcti_test_integer_name("ADD_asimdsame_only");
	u32 insn;
	unsigned long code;
	struct pt_regs regs = {};
	struct orlix_tcti_result result;
	u64 before_high;

	KUNIT_ASSERT_NOT_NULL(test, xtn);
	KUNIT_ASSERT_NOT_NULL(test, sshl);
	KUNIT_ASSERT_NOT_NULL(test, add);

	insn = (xtn->pattern & xtn->mask) | (INT_RN << 5) | INT_RD;
	code = orlix_tcti_advsimd_integer_map(test, insn);
	orlix_tcti_advsimd_integer_seed(&regs, code);
	current->thread.user_simd[INT_RD * 2U + 1U] = 0xa5a5a5a5a5a5a5a5ULL;
	before_high = current->thread.user_simd[INT_RD * 2U + 1U];
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[INT_RD * 2U + 1U]);
	KUNIT_EXPECT_NE(test, before_high,
			current->thread.user_simd[INT_RD * 2U + 1U]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	insn = (xtn->pattern & xtn->mask) | (INT_RN << 5) | INT_RD | BIT(30);
	code = orlix_tcti_advsimd_integer_map(test, insn);
	orlix_tcti_advsimd_integer_seed(&regs, code);
	current->thread.user_simd[INT_RD * 2U] = 0x1111111111111111ULL;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0x1111111111111111ULL,
			current->thread.user_simd[INT_RD * 2U]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	insn = (sshl->pattern & sshl->mask) | (INT_RN << 5) |
		(INT_RM << 16) | INT_RD | BIT(30);
	code = orlix_tcti_advsimd_integer_map(test, insn);
	orlix_tcti_advsimd_integer_seed(&regs, code);
	current->thread.user_simd[INT_RN * 2U] = 0x01;
	current->thread.user_simd[INT_RM * 2U] = 0x08;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[INT_RD * 2U] & 0xffULL);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	insn = (add->pattern & add->mask) | INT_RD | (INT_RD << 5) |
		(INT_RM << 16) | BIT(30);
	code = orlix_tcti_advsimd_integer_map(test, insn);
	orlix_tcti_advsimd_integer_seed(&regs, code);
	current->thread.user_fpsr = AARCH64_FPSR_PRESERVED;
	{
		u64 before_simd[ARRAY_SIZE(current->thread.user_simd)];
		u64 expected_rd[2];
		unsigned long expected_fpsr;
		unsigned long before_fpcr;
		int ret;

		memcpy(before_simd, current->thread.user_simd, sizeof(before_simd));
		before_fpcr = current->thread.user_fpcr;
		ret = orlix_tcti_advsimd_integer_expected_from_source(add, insn,
			&before_simd[INT_RD * 2U], &before_simd[INT_RM * 2U],
			&before_simd[INT_RD * 2U], current->thread.user_fpsr,
			expected_rd, &expected_fpsr);
		KUNIT_ASSERT_EQ(test, 0, ret);
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_ASSERT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
		KUNIT_ASSERT_EQ(test, expected_rd[0],
				current->thread.user_simd[INT_RD * 2U]);
		KUNIT_ASSERT_EQ(test, expected_rd[1],
				current->thread.user_simd[INT_RD * 2U + 1U]);
		KUNIT_ASSERT_EQ(test, expected_fpsr, current->thread.user_fpsr);
		KUNIT_ASSERT_EQ(test, before_fpcr, current->thread.user_fpcr);
	}
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	insn = (add->pattern & add->mask) | INT_RD | (INT_RN << 5) |
		(INT_RD << 16) | BIT(30);
	code = orlix_tcti_advsimd_integer_map(test, insn);
	orlix_tcti_advsimd_integer_seed(&regs, code);
	current->thread.user_fpsr = AARCH64_FPSR_PRESERVED;
	{
		u64 before_simd[ARRAY_SIZE(current->thread.user_simd)];
		u64 expected_rd[2];
		unsigned long expected_fpsr;
		unsigned long before_fpcr;
		int ret;

		memcpy(before_simd, current->thread.user_simd, sizeof(before_simd));
		before_fpcr = current->thread.user_fpcr;
		ret = orlix_tcti_advsimd_integer_expected_from_source(add, insn,
			&before_simd[INT_RN * 2U], &before_simd[INT_RD * 2U],
			&before_simd[INT_RD * 2U], current->thread.user_fpsr,
			expected_rd, &expected_fpsr);
		KUNIT_ASSERT_EQ(test, 0, ret);
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_ASSERT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
		KUNIT_ASSERT_EQ(test, expected_rd[0],
				current->thread.user_simd[INT_RD * 2U]);
		KUNIT_ASSERT_EQ(test, expected_rd[1],
				current->thread.user_simd[INT_RD * 2U + 1U]);
		KUNIT_ASSERT_EQ(test, expected_fpsr, current->thread.user_fpsr);
		KUNIT_ASSERT_EQ(test, before_fpcr, current->thread.user_fpcr);
	}
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
}

static void orlix_tcti_advsimd_integer_reserved_encodings(struct kunit *test)
{
	static const u32 reserved[] = {
		0x5f000400U, /* asisdshf SSHR immh == 0000 */
		0x0ee08400U, /* ADD Q=0 size=3 */
		0x2f008c00U, /* SQRSHRUN Q=0 immh == 0000 */
		0x2f009c00U, /* UQRSHRN Q=0 immh == 0000 */
		0x6f008c00U, /* SQRSHRUN Q=1 immh == 0000 */
		0x6f009c00U, /* UQRSHRN Q=1 immh == 0000 */
	};
	struct orlix_tcti_decoded_instruction decoded;
	u32 insn;
	size_t index;
	unsigned long code;
	struct pt_regs regs = {};
	struct pt_regs before;
	struct orlix_tcti_result result;

	/* Vector 0x0f000400 is MOVI. Vector 0x2f000400 is MVNI. */
	decoded = orlix_tcti_decode_aarch64(0x0f000400U);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE,
			decoded.decode_class);
	decoded = orlix_tcti_decode_aarch64(0x2f000400U);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE,
			decoded.decode_class);

	for (index = 0; index < ARRAY_SIZE(reserved); index++) {
		insn = reserved[index];
		decoded = orlix_tcti_decode_aarch64(insn);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
				decoded.decode_class);
		code = orlix_tcti_advsimd_integer_map(test, insn);
		orlix_tcti_advsimd_integer_seed(&regs, code);
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				result.reason);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
	}
}

static struct kunit_case orlix_tcti_advsimd_integer_source_bound_cases[] = {
	KUNIT_CASE(orlix_tcti_advsimd_integer_decodes_exact_source_cohort),
	KUNIT_CASE(orlix_tcti_advsimd_integer_binds_pinned_ddi0602_semantics),
	KUNIT_CASE(orlix_tcti_advsimd_integer_production_resume),
	KUNIT_CASE(orlix_tcti_advsimd_integer_non_el0_rejected),
	KUNIT_CASE(orlix_tcti_advsimd_integer_saturation_qc),
	KUNIT_CASE(orlix_tcti_advsimd_integer_narrow_shift_overlap),
	KUNIT_CASE(orlix_tcti_advsimd_integer_reserved_encodings),
	{}
};

static struct kunit_suite orlix_tcti_advsimd_integer_source_bound_suite = {
	.name = "orlix-tcti-advsimd-integer-source-bound",
	.init = orlix_tcti_advsimd_integer_test_init,
	.exit = orlix_tcti_advsimd_integer_test_exit,
	.test_cases = orlix_tcti_advsimd_integer_source_bound_cases,
};

kunit_test_suite(orlix_tcti_advsimd_integer_source_bound_suite);
