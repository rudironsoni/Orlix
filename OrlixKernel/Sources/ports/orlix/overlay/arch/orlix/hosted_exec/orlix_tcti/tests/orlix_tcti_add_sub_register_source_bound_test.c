// SPDX-License-Identifier: GPL-2.0-only
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/syscalls.h>

#include "../decode_aarch64.h"
#include "orlix_tcti_test_suites.h"
#include "target_execution_slice_map.h"
#include "target_proof_registry.h"

#define ASR_NZCV (PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT)
#define ASR_SVC 0xd4000001U
enum asr_family { ASR_SHIFTED, ASR_EXTENDED, ASR_CARRY };
struct asr_leaf {
	u16 ordinal;
	const char *name;
	const char *operation;
	u32 pattern;
	u32 mask;
	enum asr_family family;
	bool wide;
	bool subtract;
	bool flags;
};
#define S(n, name, op, p, w, sub, f)                                           \
	{n, name, op, p, 0xff200000U, ASR_SHIFTED, w, sub, f}
#define E(n, name, op, p, w, sub, f)                                           \
	{n, name, op, p, 0xffe00000U, ASR_EXTENDED, w, sub, f}
#define C(n, name, op, p, w, sub, f)                                           \
	{n, name, op, p, 0xffe0fc00U, ASR_CARRY, w, sub, f}
static const struct asr_leaf leaves[] = {
    S(3450, "ADD_32_addsub_shift", "ADD_addsub_shift", 0x0b000000U, 0, 0, 0),
    S(3451, "ADDS_32_addsub_shift", "ADDS_addsub_shift", 0x2b000000U, 0, 0, 1),
    S(3452, "SUB_32_addsub_shift", "SUB_addsub_shift", 0x4b000000U, 0, 1, 0),
    S(3453, "SUBS_32_addsub_shift", "SUBS_addsub_shift", 0x6b000000U, 0, 1, 1),
    S(3454, "ADD_64_addsub_shift", "ADD_addsub_shift", 0x8b000000U, 1, 0, 0),
    S(3455, "ADDS_64_addsub_shift", "ADDS_addsub_shift", 0xab000000U, 1, 0, 1),
    S(3456, "SUB_64_addsub_shift", "SUB_addsub_shift", 0xcb000000U, 1, 1, 0),
    S(3457, "SUBS_64_addsub_shift", "SUBS_addsub_shift", 0xeb000000U, 1, 1, 1),
    E(3458, "ADD_32_addsub_ext", "ADD_addsub_ext", 0x0b200000U, 0, 0, 0),
    E(3459, "ADDS_32S_addsub_ext", "ADDS_addsub_ext", 0x2b200000U, 0, 0, 1),
    E(3460, "SUB_32_addsub_ext", "SUB_addsub_ext", 0x4b200000U, 0, 1, 0),
    E(3461, "SUBS_32S_addsub_ext", "SUBS_addsub_ext", 0x6b200000U, 0, 1, 1),
    E(3462, "ADD_64_addsub_ext", "ADD_addsub_ext", 0x8b200000U, 1, 0, 0),
    E(3463, "ADDS_64S_addsub_ext", "ADDS_addsub_ext", 0xab200000U, 1, 0, 1),
    E(3464, "SUB_64_addsub_ext", "SUB_addsub_ext", 0xcb200000U, 1, 1, 0),
    E(3465, "SUBS_64S_addsub_ext", "SUBS_addsub_ext", 0xeb200000U, 1, 1, 1),
    C(3466, "ADC_32_addsub_carry", "ADC", 0x1a000000U, 0, 0, 0),
    C(3467, "ADCS_32_addsub_carry", "ADCS", 0x3a000000U, 0, 0, 1),
    C(3468, "SBC_32_addsub_carry", "SBC", 0x5a000000U, 0, 1, 0),
    C(3469, "SBCS_32_addsub_carry", "SBCS", 0x7a000000U, 0, 1, 1),
    C(3470, "ADC_64_addsub_carry", "ADC", 0x9a000000U, 1, 0, 0),
    C(3471, "ADCS_64_addsub_carry", "ADCS", 0xba000000U, 1, 0, 1),
    C(3472, "SBC_64_addsub_carry", "SBC", 0xda000000U, 1, 1, 0),
    C(3473, "SBCS_64_addsub_carry", "SBCS", 0xfa000000U, 1, 1, 1),
};

struct asr_pointer_leaf {
	u16 ordinal;
	const char *name;
	const char *operation;
	u32 pattern;
	bool subtract;
};

static const struct asr_pointer_leaf pointer_leaves[] = {
	{ 3474, "ADDPT_64_addsub_pt", "ADDPT", 0x9a002000U, false },
	{ 3475, "SUBPT_64_addsub_pt", "SUBPT", 0xda002000U, true },
};

struct asr_cohort_binding {
	u16 ordinal;
	const char *name;
	const char *proof_id;
};

static const struct asr_cohort_binding asr_cohort_bindings[] = {
	{ 2173, "ADD_32_addsub_imm", "kunit:add-sub-immediate-add" },
	{ 2174, "ADDS_32S_addsub_imm", "kunit:add-sub-immediate-adds" },
	{ 2175, "SUB_32_addsub_imm", "kunit:add-sub-immediate-sub" },
	{ 2176, "SUBS_32S_addsub_imm", "kunit:add-sub-immediate-subs" },
	{ 2177, "ADD_64_addsub_imm", "kunit:add-sub-immediate-add" },
	{ 2178, "ADDS_64S_addsub_imm", "kunit:add-sub-immediate-adds" },
	{ 2179, "SUB_64_addsub_imm", "kunit:add-sub-immediate-sub" },
	{ 2180, "SUBS_64S_addsub_imm", "kunit:add-sub-immediate-subs" },
	{ 3450, "ADD_32_addsub_shift", "kunit:add-sub-register-add-shift" },
	{ 3451, "ADDS_32_addsub_shift", "kunit:add-sub-register-adds-shift" },
	{ 3452, "SUB_32_addsub_shift", "kunit:add-sub-register-sub-shift" },
	{ 3453, "SUBS_32_addsub_shift", "kunit:add-sub-register-subs-shift" },
	{ 3454, "ADD_64_addsub_shift", "kunit:add-sub-register-add-shift" },
	{ 3455, "ADDS_64_addsub_shift", "kunit:add-sub-register-adds-shift" },
	{ 3456, "SUB_64_addsub_shift", "kunit:add-sub-register-sub-shift" },
	{ 3457, "SUBS_64_addsub_shift", "kunit:add-sub-register-subs-shift" },
	{ 3458, "ADD_32_addsub_ext", "kunit:add-sub-register-add-ext" },
	{ 3459, "ADDS_32S_addsub_ext", "kunit:add-sub-register-adds-ext" },
	{ 3460, "SUB_32_addsub_ext", "kunit:add-sub-register-sub-ext" },
	{ 3461, "SUBS_32S_addsub_ext", "kunit:add-sub-register-subs-ext" },
	{ 3462, "ADD_64_addsub_ext", "kunit:add-sub-register-add-ext" },
	{ 3463, "ADDS_64S_addsub_ext", "kunit:add-sub-register-adds-ext" },
	{ 3464, "SUB_64_addsub_ext", "kunit:add-sub-register-sub-ext" },
	{ 3465, "SUBS_64S_addsub_ext", "kunit:add-sub-register-subs-ext" },
	{ 3466, "ADC_32_addsub_carry", "kunit:add-sub-register-adc" },
	{ 3467, "ADCS_32_addsub_carry", "kunit:add-sub-register-adcs" },
	{ 3468, "SBC_32_addsub_carry", "kunit:add-sub-register-sbc" },
	{ 3469, "SBCS_32_addsub_carry", "kunit:add-sub-register-sbcs" },
	{ 3470, "ADC_64_addsub_carry", "kunit:add-sub-register-adc" },
	{ 3471, "ADCS_64_addsub_carry", "kunit:add-sub-register-adcs" },
	{ 3472, "SBC_64_addsub_carry", "kunit:add-sub-register-sbc" },
	{ 3473, "SBCS_64_addsub_carry", "kunit:add-sub-register-sbcs" },
	{ 3474, "ADDPT_64_addsub_pt", "kunit:add-sub-register-addpt" },
	{ 3475, "SUBPT_64_addsub_pt", "kunit:add-sub-register-subpt" },
};

struct asr_source_bound_projection {
	u16 ordinal;
	const char *proof_id;
};

#define ORLIX_TCTI_A64_SOURCE_BOUND_PROOF(ordinal, proof_id) \
	{ ordinal, proof_id },
static const struct asr_source_bound_projection asr_source_bound_projections[] = {
#include "../isa/source_bound_proof.def"
};
#undef ORLIX_TCTI_A64_SOURCE_BOUND_PROOF

struct asr_classification_projection {
	const char *evidence;
	const char *proof_id;
};

#define ORLIX_TCTI_A64_TARGET_CLASSIFICATION(name, classification, relation, \
					      evidence, proof_id, note) \
	{ proof_id, note },
static const struct asr_classification_projection asr_classifications[] = {
#include "../isa/target_classification.def"
};
#undef ORLIX_TCTI_A64_TARGET_CLASSIFICATION

static u32 asr_pointer_instruction(const struct asr_pointer_leaf *leaf, u8 rm,
				   u8 shift, u8 rn, u8 rd)
{
	return leaf->pattern | ((u32)rm << 16) | ((u32)shift << 10) |
	       ((u32)rn << 5) | rd;
}
static u32 asr_instruction(const struct asr_leaf *l, u8 rm, u8 a, u8 b, u8 rn,
			   u8 rd)
{
	if (l->family == ASR_SHIFTED)
		return l->pattern | ((u32)a << 22) | ((u32)rm << 16) |
		       ((u32)b << 10) | ((u32)rn << 5) | rd;
	if (l->family == ASR_EXTENDED)
		return l->pattern | ((u32)rm << 16) | ((u32)a << 13) |
		       ((u32)b << 10) | ((u32)rn << 5) | rd;
	return l->pattern | ((u32)rm << 16) | ((u32)rn << 5) | rd;
}
static enum orlix_tcti_decode_class asr_class(enum asr_family f)
{
	return f == ASR_SHIFTED	   ? ORLIX_TCTI_DECODE_ADD_SUB_SHIFTED_REGISTER
	       : f == ASR_EXTENDED ? ORLIX_TCTI_DECODE_ADD_SUB_EXTENDED_REGISTER
				   : ORLIX_TCTI_DECODE_ADD_SUB_WITH_CARRY;
}
static bool asr_valid(const struct asr_leaf *l, u8 a, u8 b)
{
	if (l->family == ASR_SHIFTED)
		return a < 3 && (l->wide || b < 32);
	if (l->family == ASR_EXTENDED)
		return b <= 4;
	return true;
}
static unsigned long asr_map(struct kunit *test, u32 insn)
{
	u32 p[] = {insn, ASR_SVC};
	unsigned long m;
	int ret;
	m = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
			    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(m));
	ret = orlix_tcti_write_user_data(current->mm, m, p, sizeof(p));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = sys_mprotect(m, PAGE_SIZE, PROT_READ | PROT_EXEC);
	KUNIT_ASSERT_EQ(test, 0, ret);
	return m;
}
static u64 asr_right(u64 value, bool wide, enum asr_family family, u8 a, u8 b)
{
	u64 mask = wide ? U64_MAX : U32_MAX;
	u8 width = wide ? 64 : 32;

	if (family == ASR_SHIFTED) {
		value &= mask;
		if (a == 0)
			return value << b & mask;
		if (a == 1)
			return value >> b;
		return (u64)(sign_extend64(value, width - 1) >> b) & mask;
	}
	if (family == ASR_EXTENDED) {
		switch (a) {
		case 0:
			value = (u8)value;
			break;
		case 1:
			value = (u16)value;
			break;
		case 2:
			value = (u32)value;
			break;
		case 3:
			break;
		case 4:
			value = (s64)(s8)value;
			break;
		case 5:
			value = (s64)(s16)value;
			break;
		case 6:
			value = (s64)(s32)value;
			break;
		default:
			value = (s64)value;
		}
		return value << b & mask;
	}
	return value & mask;
}
static unsigned long asr_nzcv(bool wide, bool sub, u64 left, u64 right,
			      u64 result, bool carry)
{
	u64 mask = wide ? U64_MAX : U32_MAX,
	    sign = wide ? BIT_ULL(63) : BIT_ULL(31);
	u64 addend = sub ? ~right & mask : right;
	__uint128_t total = (__uint128_t)left + addend + (carry ? 1 : 0);
	unsigned long f = 0;
	bool ln = left & sign, rn = (carry ? addend : right) & sign,
	     zn = result & sign;
	if (zn)
		f |= PSR_N_BIT;
	if (!result)
		f |= PSR_Z_BIT;
	if (carry ? total >> (wide ? 64 : 32)
	    : sub ? left >= right
		  : (__uint128_t)left + right > mask)
		f |= PSR_C_BIT;
	if (carry ? ln == rn && ln != zn
	    : sub
		? !!(left & sign) != !!(right & sign) && !!(left & sign) != zn
		: !!(left & sign) == !!(right & sign) && !!(left & sign) != zn)
		f |= PSR_V_BIT;
	return f;
}

struct asr_ddi_vector {
	const char *name;
	u8 leaf_index;
	u8 rm;
	u8 modifier;
	u8 amount;
	u8 rn;
	u8 rd;
	bool carry_in;
	u64 rn_value;
	u64 rm_value;
	u64 sp;
	u64 expected;
	unsigned long expected_nzcv;
};

/* Fixed AddWithCarry and ExtendReg vectors derived from DDI0602 2026-06. */
static const struct asr_ddi_vector asr_ddi_vectors[] = {
	{ "add32-lsl-rd-rn", 0, 7, 0, 4, 5, 5, false,
	  0x10, 3, 0x1000, 0x40, 0 },
	{ "adds32-lsr-cmn-zc", 1, 7, 1, 31, 5, 31, false,
	  U32_MAX, BIT_ULL(31), 0x1000, 0, PSR_Z_BIT | PSR_C_BIT },
	{ "sub32-asr-rd-rm", 2, 7, 2, 31, 5, 7, false,
	  0, BIT_ULL(31), 0x1000, 1, 0 },
	{ "subs32-negs-nv", 3, 7, 0, 0, 31, 3, false,
	  0, BIT_ULL(31), 0x1000, BIT_ULL(31), PSR_N_BIT | PSR_V_BIT },
	{ "add64-lsl-sign", 4, 7, 0, 63, 5, 3, false,
	  0, 1, 0x1000, BIT_ULL(63), 0 },
	{ "adds64-lsr-nv", 5, 7, 1, 63, 5, 3, false,
	  BIT_ULL(63) - 1, BIT_ULL(63), 0x1000, BIT_ULL(63),
	  PSR_N_BIT | PSR_V_BIT },
	{ "sub64-asr", 6, 7, 2, 63, 5, 3, false,
	  0, BIT_ULL(63), 0x1000, 1, 0 },
	{ "subs64-cmp-cv", 7, 7, 0, 0, 5, 31, false,
	  BIT_ULL(63), 1, 0x1000, BIT_ULL(63) - 1,
	  PSR_C_BIT | PSR_V_BIT },
	{ "add32-uxtb-clear", 8, 7, 0, 0, 5, 3, false,
	  U32_MAX, 0x101, 0x1000, 0, 0 },
	{ "adds32-uxth-nv", 9, 7, 1, 0, 5, 3, false,
	  BIT_ULL(31) - 1, 0x10001, 0x1000, BIT_ULL(31),
	  PSR_N_BIT | PSR_V_BIT },
	{ "sub32-uxtw", 10, 7, 2, 0, 5, 3, false,
	  0, 0x100000001ULL, 0x1000, U32_MAX, 0 },
	{ "subs32-uxtx-cmp-zc", 11, 7, 3, 0, 5, 31, false,
	  1, 1, 0x1000, 0, PSR_Z_BIT | PSR_C_BIT },
	{ "add64-sxtb-sp", 12, 7, 4, 1, 31, 31, false,
	  0, 0xff, 0x1000, 0xffe, 0 },
	{ "adds64-sxth-sp-cmn-nv", 13, 7, 5, 0, 31, 31, false,
	  0, 1, BIT_ULL(63) - 1, BIT_ULL(63),
	  PSR_N_BIT | PSR_V_BIT },
	{ "sub64-sxtw-rd-rn", 14, 7, 6, 0, 5, 5, false,
	  0, U32_MAX, 0x1000, 1, 0 },
	{ "subs64-sxtx-cmp-c", 15, 7, 7, 0, 5, 31, false,
	  5, 3, 0x1000, 2, PSR_C_BIT },
	{ "adc32-c0-wrap", 16, 7, 0, 0, 5, 3, false,
	  U32_MAX, 1, 0x1000, 0, 0 },
	{ "adcs32-c0-zc", 17, 7, 0, 0, 5, 3, false,
	  U32_MAX, 1, 0x1000, 0, PSR_Z_BIT | PSR_C_BIT },
	{ "sbc32-c0-borrow", 18, 7, 0, 0, 5, 3, false,
	  0, 0, 0x1000, U32_MAX, 0 },
	{ "sbcs32-c0-n", 19, 7, 0, 0, 5, 3, false,
	  0, 0, 0x1000, U32_MAX, PSR_N_BIT },
	{ "adc64-c1-sign", 20, 7, 0, 0, 5, 3, true,
	  BIT_ULL(63) - 1, 0, 0x1000, BIT_ULL(63), 0 },
	{ "adcs64-c1-nv", 21, 7, 0, 0, 5, 3, true,
	  BIT_ULL(63) - 1, 0, 0x1000, BIT_ULL(63),
	  PSR_N_BIT | PSR_V_BIT },
	{ "sbc64-c1-zero", 22, 7, 0, 0, 5, 3, true,
	  0, 0, 0x1000, 0, 0 },
	{ "sbcs64-c1-cv", 23, 7, 0, 0, 5, 3, true,
	  BIT_ULL(63), 1, 0x1000, BIT_ULL(63) - 1,
	  PSR_C_BIT | PSR_V_BIT },
	{ "adc32-c1", 16, 7, 0, 0, 5, 3, true,
	  1, 2, 0x1000, 4, 0 },
	{ "adcs32-c1", 17, 7, 0, 0, 5, 3, true,
	  1, 2, 0x1000, 4, 0 },
	{ "sbc32-c1", 18, 7, 0, 0, 5, 3, true,
	  5, 2, 0x1000, 3, 0 },
	{ "sbcs32-c1-c", 19, 7, 0, 0, 5, 3, true,
	  5, 2, 0x1000, 3, PSR_C_BIT },
	{ "adc64-c0-rd-rn", 20, 7, 0, 0, 5, 5, false,
	  1, 2, 0x1000, 3, 0 },
	{ "adcs64-c0", 21, 7, 0, 0, 5, 3, false,
	  1, 2, 0x1000, 3, 0 },
	{ "sbc64-c0", 22, 7, 0, 0, 5, 3, false,
	  5, 2, 0x1000, 2, 0 },
	{ "sbcs64-c0-c", 23, 7, 0, 0, 5, 3, false,
	  5, 2, 0x1000, 2, PSR_C_BIT },
	{ "sub32-neg", 2, 7, 0, 0, 31, 3, false,
	  0, 5, 0x1000, 0xfffffffbU, 0 },
	{ "add32-zr-offset", 0, 31, 0, 0, 5, 3, false,
	  5, 0, 0x1000, 5, 0 },
	{ "adc32-zr-base", 16, 7, 0, 0, 31, 3, true,
	  0, 2, 0x1000, 3, 0 },
	{ "add64-ext-rd-rm", 12, 7, 0, 0, 5, 7, false,
	  4, 5, 0x1000, 9, 0 },
};

static void asr_expect_legal_decode(struct kunit *test,
	const struct asr_leaf *leaf, u8 modifier, u8 amount, u8 rm, u8 rn, u8 rd)
{
	u32 instruction = asr_instruction(leaf, rm, modifier, amount, rn, rd);
	struct orlix_tcti_decoded_instruction decoded =
		orlix_tcti_decode_aarch64(instruction);

	KUNIT_ASSERT_EQ_MSG(test, asr_class(leaf->family), decoded.decode_class,
		"%s instruction %#x", leaf->name, instruction);
	KUNIT_EXPECT_EQ(test, leaf->wide, decoded.is_64bit);
	KUNIT_EXPECT_EQ(test, leaf->subtract, decoded.subtract);
	KUNIT_EXPECT_EQ(test, leaf->flags, decoded.set_flags);
	KUNIT_EXPECT_EQ(test, rn, decoded.rn);
	KUNIT_EXPECT_EQ(test, rm, decoded.rm);
	KUNIT_EXPECT_EQ(test, rd, decoded.rd);
	if (leaf->family == ASR_SHIFTED)
		KUNIT_EXPECT_EQ(test, modifier, decoded.shift);
	else if (leaf->family == ASR_EXTENDED)
		KUNIT_EXPECT_EQ(test, modifier, decoded.offset_extend);
	KUNIT_EXPECT_EQ(test, amount, decoded.shift_amount);
}

static void asr_source_and_decode(struct kunit *test)
{
	size_t i;
	for (i = 0; i < ARRAY_SIZE(leaves); i++) {
		const struct asr_leaf *l = &leaves[i];
		u8 a, b;
		KUNIT_EXPECT_EQ(test, 3450U + i, l->ordinal);
		KUNIT_EXPECT_EQ(test, l->pattern, l->pattern & l->mask);
		for (a = 0; a < (l->family == ASR_SHIFTED    ? 4
				 : l->family == ASR_EXTENDED ? 8
							     : 1);
		     a++)
			for (b = 0; b < (l->family == ASR_CARRY	     ? 1
					 : l->family == ASR_EXTENDED ? 8
								     : 64);
			     b++) {
				u32 insn = asr_instruction(l, 7, a, b, 5, 3);
				struct orlix_tcti_decoded_instruction d =
				    orlix_tcti_decode_aarch64(insn);
				u8 reg;

				if (!asr_valid(l, a, b)) {
					KUNIT_EXPECT_EQ(test,
							ORLIX_TCTI_DECODE_UNSUPPORTED,
							d.decode_class);
					continue;
				}
				for (reg = 0; reg < 32; reg++) {
					asr_expect_legal_decode(test, l, a, b, reg, 5, 3);
					asr_expect_legal_decode(test, l, a, b, 7, reg, 3);
					asr_expect_legal_decode(test, l, a, b, 7, 5, reg);
				}
			}
	}
}
static void asr_resume_semantics(struct kunit *test)
{
	size_t i;
	for (i = 0; i < ARRAY_SIZE(leaves); i++) {
		const struct asr_leaf *l = &leaves[i];
		u8 a = l->family == ASR_EXTENDED ? 6 : 0,
		   b = l->family == ASR_EXTENDED  ? 2
		       : l->family == ASR_SHIFTED ? (l->wide ? 63 : 31)
						  : 0;
		u32 insn = asr_instruction(
		    l, 7, a, b, l->family == ASR_EXTENDED ? 31 : 5,
		    l->family == ASR_EXTENDED && !l->flags ? 31 : 3);
		struct pt_regs r = {}, before;
		struct orlix_tcti_result result;
		unsigned long m = asr_map(test, insn);
		r.regs[5] = 0x80000000ffffffffULL;
		r.regs[7] = 0x80000000ffffffffULL;
		r.sp = 0x1234567880000001ULL;
		r.pc = m;
		r.pstate = PSR_MODE_EL0t | ASR_NZCV | PSR_C_BIT;
		r.syscallno = NO_SYSCALL;
		before = r;
		{
			u64 mask = l->wide ? U64_MAX : U32_MAX;
			u64 left =
			    (l->family == ASR_EXTENDED ? before.sp
						       : before.regs[5]) &
			    mask;
			u64 right =
			    asr_right(before.regs[7], l->wide, l->family, a, b);
			u64 expected =
			    l->family == ASR_CARRY
				? (left +
				   (l->subtract ? ~right & mask : right) + 1) &
				      mask
				: (l->subtract ? left - right : left + right) &
				      mask;
			unsigned long pstate = before.pstate;

			if (l->flags)
				pstate =
				    (before.pstate & ~ASR_NZCV) |
				    asr_nzcv(l->wide, l->subtract, left, right,
					     expected, l->family == ASR_CARRY);
			result = orlix_tcti_resume_user(current, &r, current->mm);
			if (l->family == ASR_EXTENDED && !l->flags)
				KUNIT_EXPECT_EQ(test, expected, r.sp);
			else if (!(l->flags &&
				   (l->family != ASR_EXTENDED ? 3 : 31) == 31))
				KUNIT_EXPECT_EQ(test, expected, r.regs[3]);
			KUNIT_EXPECT_EQ(test, pstate, r.pstate);
		}
		KUNIT_ASSERT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason,
				    "%s", l->name);
		KUNIT_EXPECT_EQ(test, ASR_SVC, result.instruction);
		KUNIT_EXPECT_EQ(test, m + sizeof(u32), r.pc);
		KUNIT_EXPECT_EQ(test, (unsigned long)PSR_MODE_EL0t,
				r.pstate & PSR_MODE_MASK);
		if (!l->wide && l->family != ASR_EXTENDED)
			KUNIT_EXPECT_EQ(test, 0ULL, r.regs[3] & ~U32_MAX);
		if (l->family == ASR_EXTENDED && !l->flags)
			KUNIT_EXPECT_NE(test, before.sp, r.sp);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(m, PAGE_SIZE));
	}
}
static void asr_reserved_structured_exits(struct kunit *test)
{
	size_t i;
	for (i = 0; i < ARRAY_SIZE(leaves); i++) {
		const struct asr_leaf *l = &leaves[i];
		u8 a = l->family == ASR_SHIFTED ? 3 : 0,
		   b = l->family == ASR_EXTENDED  ? 5
		       : l->family == ASR_SHIFTED ? 0
						  : 0;
		u32 insn;
		unsigned long m;
		struct pt_regs r = {}, before;
		struct orlix_tcti_result x;
		if (l->family == ASR_CARRY)
			insn = asr_instruction(l, 7, 0, 0, 5, 3) ^ BIT(10);
		else
			insn = asr_instruction(l, 7, a, b, 5, 3);
		m = asr_map(test, insn);
		r.pc = m;
		r.sp = STACK_TOP - 16;
		r.pstate = PSR_MODE_EL0t;
		r.syscallno = NO_SYSCALL;
		before = r;
		x = orlix_tcti_resume_user(current, &r, current->mm);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				x.reason);
		KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, x.status);
		KUNIT_EXPECT_EQ(test, insn, x.instruction);
		KUNIT_EXPECT_MEMEQ(test, before.regs, r.regs, sizeof(r.regs));
		KUNIT_EXPECT_EQ(test, before.pc, r.pc);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(m, PAGE_SIZE));
	}
}

static void asr_issue_133_exact_cohort(struct kunit *test)
{
	const struct orlix_tcti_execution_slice_map *map =
		orlix_tcti_execution_slice_map_canonical();
	const struct orlix_tcti_target_proof_registry_entry *registry;
	size_t registry_count;
	size_t cohort_index;
	size_t map_count = 0;
	size_t source_count = 0;
	size_t classification_count = 0;
	size_t binding_count = 0;

	KUNIT_ASSERT_NOT_NULL(test, map);
	registry = orlix_tcti_target_proof_registry_entries(&registry_count);
	KUNIT_ASSERT_NOT_NULL(test, registry);
	KUNIT_ASSERT_EQ(test, 34U, ARRAY_SIZE(asr_cohort_bindings));
	KUNIT_ASSERT_EQ(test, 4350U, ARRAY_SIZE(asr_classifications));
	for (cohort_index = 0; cohort_index < ARRAY_SIZE(asr_cohort_bindings);
	     cohort_index++) {
		const struct asr_cohort_binding *expected =
			&asr_cohort_bindings[cohort_index];
		size_t index;
		size_t map_matches = 0;
		size_t source_matches = 0;
		size_t registry_matches = 0;

		for (index = 0; index < map->counts.leaf_count; index++) {
			const struct orlix_tcti_execution_slice_member *member =
				&map->members[index];
			const struct orlix_tcti_execution_slice_family *family =
				&map->families[member->family_index];

			if (family->issue_id == 133U &&
			    member->ordinal == expected->ordinal &&
			    !strcmp(member->source_name, expected->name)) {
				KUNIT_EXPECT_STREQ(test, "base-a64-add-subtract",
					family->stable_id);
				map_matches++;
			}
		}
		for (index = 0; index < ARRAY_SIZE(asr_source_bound_projections);
		     index++)
			if (asr_source_bound_projections[index].ordinal ==
				    expected->ordinal &&
			    !strcmp(asr_source_bound_projections[index].proof_id,
				    expected->proof_id))
				source_matches++;
		KUNIT_ASSERT_LT(test, (u32)expected->ordinal,
			ARRAY_SIZE(asr_classifications));
		KUNIT_EXPECT_STREQ(test, expected->proof_id,
			asr_classifications[expected->ordinal].proof_id);
		KUNIT_EXPECT_TRUE(test,
			!strcmp(asr_classifications[expected->ordinal].evidence,
				"source-bound-kunit-add-sub-immediate") ||
			!strcmp(asr_classifications[expected->ordinal].evidence,
				"source-bound-kunit-add-sub-register"));
		for (index = 0; index < registry_count; index++) {
			size_t binding;

			if (strcmp(registry[index].id, expected->proof_id))
				continue;
			for (binding = 0; binding < registry[index].binding_count;
			     binding++)
				if (registry[index].bindings[binding].source_ordinal ==
					    expected->ordinal &&
				    !strcmp(registry[index].bindings[binding].leaf_name,
					    expected->name))
					registry_matches++;
		}
		KUNIT_EXPECT_EQ_MSG(test, 1U, map_matches, "map ordinal %u",
			expected->ordinal);
		KUNIT_EXPECT_EQ_MSG(test, 1U, source_matches, "source ordinal %u",
			expected->ordinal);
		KUNIT_EXPECT_EQ_MSG(test, 1U, registry_matches,
			"registry ordinal %u", expected->ordinal);
	}
	for (cohort_index = 0; cohort_index < map->counts.leaf_count;
	     cohort_index++)
		if (map->families[map->members[cohort_index].family_index].issue_id ==
		    133U)
			map_count++;
	for (cohort_index = 0;
	     cohort_index < ARRAY_SIZE(asr_source_bound_projections);
	     cohort_index++)
		if (!strncmp(asr_source_bound_projections[cohort_index].proof_id,
			    "kunit:add-sub-", strlen("kunit:add-sub-")))
			source_count++;
	for (cohort_index = 0; cohort_index < ARRAY_SIZE(asr_classifications);
	     cohort_index++)
		if (!strncmp(asr_classifications[cohort_index].evidence,
			    "source-bound-kunit-add-sub-",
			    strlen("source-bound-kunit-add-sub-")))
			classification_count++;
	for (cohort_index = 0; cohort_index < registry_count; cohort_index++) {
		size_t binding;

		if (strncmp(registry[cohort_index].id, "kunit:add-sub-",
			    strlen("kunit:add-sub-")))
			continue;
		for (binding = 0; binding < registry[cohort_index].binding_count;
		     binding++)
			binding_count++;
	}
	KUNIT_EXPECT_EQ(test, 34U, map_count);
	KUNIT_EXPECT_EQ(test, 34U, source_count);
	KUNIT_EXPECT_EQ(test, 34U, classification_count);
	KUNIT_EXPECT_EQ(test, 34U, binding_count);
}

static void asr_pointer_source_and_decode(struct kunit *test)
{
	size_t leaf_index;

	for (leaf_index = 0; leaf_index < ARRAY_SIZE(pointer_leaves);
	     leaf_index++) {
		const struct asr_pointer_leaf *leaf = &pointer_leaves[leaf_index];
		u8 shift;
		u8 rm;
		u8 rn;
		u8 rd;

		KUNIT_EXPECT_EQ(test, 3474U + leaf_index, leaf->ordinal);
		KUNIT_EXPECT_EQ(test, leaf->pattern,
				leaf->pattern & 0xffe0e000U);
		for (shift = 0; shift < 8; shift++)
			for (rm = 0; rm < 32; rm++)
				for (rn = 0; rn < 32; rn++)
					for (rd = 0; rd < 32; rd++) {
						u32 instruction = asr_pointer_instruction(
							leaf, rm, shift, rn, rd);
						struct orlix_tcti_decoded_instruction decoded =
							orlix_tcti_decode_aarch64(instruction);

						KUNIT_EXPECT_EQ(test,
							ORLIX_TCTI_DECODE_ADD_SUB_POINTER_CHECKED,
							decoded.decode_class);
						KUNIT_EXPECT_EQ(test, rd, decoded.rd);
						KUNIT_EXPECT_EQ(test, rn, decoded.rn);
						KUNIT_EXPECT_EQ(test, rm, decoded.rm);
						KUNIT_EXPECT_EQ(test, shift,
							decoded.shift_amount);
						KUNIT_EXPECT_TRUE(test, decoded.is_64bit);
						KUNIT_EXPECT_EQ(test, leaf->subtract,
							decoded.subtract);
						KUNIT_EXPECT_FALSE(test, decoded.set_flags);
					}
	}
}

static void asr_pointer_production_semantics(struct kunit *test)
{
	static const struct {
		u8 leaf;
		u8 rd;
		u8 rn;
		u8 rm;
		u8 shift;
		u64 base;
		u64 offset;
		struct orlix_tcti_cpa_control control;
		bool previous_detection;
		bool cpta_detected;
		bool effective_cpta;
		bool poisoned;
		u64 expected;
	} cases[] = {
		{ 0, 3, 5, 7, 2, 0x1000, 3, { .feat_cpa = true },
		  false, false, false, false, 0x100c },
		{ 1, 31, 31, 7, 7, 0x9000, 0x11,
		  { .feat_cpa = true }, false, false, false, false, 0x8780 },
		{ 0, 4, 5, 31, 7, 0x12345678, U64_MAX,
		  { .feat_cpa = true }, false, false, false, false, 0x12345678 },
		{ 0, 8, 5, 7, 0, 0x0100000000000000ULL,
		  0x0100000000000000ULL,
		  { true, true, true, true, false }, false, true, true, true,
		  0x0140000000000000ULL },
		{ 0, 9, 5, 7, 0, BIT_ULL(55), 1,
		  { true, true, true, true, false }, true, true, true, true,
		  0x0080000000000001ULL },
		{ 1, 10, 5, 7, 0, 0x0100000000000000ULL,
		  0x0200000000000000ULL,
		  { true, false, true, true, false }, false, true, false, false,
		  0xff00000000000000ULL },
		/* DDI alias vectors: destination overlaps the base and offset. */
		{ 0, 5, 5, 7, 1, 0x100, 3, { .feat_cpa = true },
		  false, false, false, false, 0x106 },
		{ 1, 7, 5, 7, 1, 0x100, 3, { .feat_cpa = true },
		  false, false, false, false, 0xfa },
	};
	struct orlix_tcti_cpa_control saved_control;
	struct orlix_tcti_pointer_add_observation saved_observation =
		current->thread.user_cpa_add_observation;
	size_t index;

	orlix_tcti_cpu_cpa_control_get(&saved_control);
	for (index = 0; index < ARRAY_SIZE(cases); index++) {
		const struct asr_pointer_leaf *leaf = &pointer_leaves[cases[index].leaf];
		struct orlix_tcti_pointer_add_observation *observation;
		struct pt_regs regs = {};
		struct pt_regs before;
		struct orlix_tcti_result result;
		u32 instruction = asr_pointer_instruction(leaf, cases[index].rm,
			cases[index].shift, cases[index].rn, cases[index].rd);
		unsigned long mapped = asr_map(test, instruction);
		u64 shifted_offset = cases[index].rm == 31 ? 0 :
			cases[index].offset << cases[index].shift;
		u64 arithmetic = leaf->subtract ? cases[index].base - shifted_offset :
			cases[index].base + shifted_offset;
		u64 expected = cases[index].expected;

		orlix_tcti_cpu_cpa_control_set(&cases[index].control);
		memset(&current->thread.user_cpa_add_observation, 0xa5,
		       sizeof(current->thread.user_cpa_add_observation));
		if (cases[index].rn != 31)
			regs.regs[cases[index].rn] = cases[index].base;
		if (cases[index].rm != 31)
			regs.regs[cases[index].rm] = cases[index].offset;
		regs.sp = cases[index].base;
		regs.pc = mapped;
		regs.pstate = PSR_MODE_EL0t | ASR_NZCV;
		regs.syscallno = NO_SYSCALL;
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		observation = &current->thread.user_cpa_add_observation;
		KUNIT_ASSERT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason,
				    "%s", leaf->name);
		KUNIT_EXPECT_EQ(test, ASR_SVC, result.instruction);
		if (cases[index].rd == 31)
			KUNIT_EXPECT_EQ(test, expected, regs.sp);
		else {
			before.regs[cases[index].rd] = expected;
			KUNIT_EXPECT_EQ(test, expected, regs.regs[cases[index].rd]);
		}
		KUNIT_EXPECT_MEMEQ(test, before.regs, regs.regs,
				   sizeof(regs.regs));
		KUNIT_EXPECT_EQ(test, mapped + sizeof(u32), regs.pc);
		KUNIT_EXPECT_EQ(test, before.pstate, regs.pstate);
		KUNIT_EXPECT_TRUE(test, observation->valid);
		KUNIT_EXPECT_EQ(test, cases[index].base, observation->base);
		KUNIT_EXPECT_EQ(test, arithmetic, observation->arithmetic_result);
		KUNIT_EXPECT_EQ(test, expected, observation->result);
		KUNIT_EXPECT_EQ(test, cases[index].previous_detection,
				observation->previous_detection);
		KUNIT_EXPECT_EQ(test, cases[index].cpta_detected,
				observation->cpta_detected);
		KUNIT_EXPECT_EQ(test, cases[index].effective_cpta,
				observation->effective_cpta);
		KUNIT_EXPECT_EQ(test, cases[index].poisoned,
				observation->poisoned);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	}
	orlix_tcti_cpu_cpa_control_set(&saved_control);
	current->thread.user_cpa_add_observation = saved_observation;
}

static void asr_pointer_all_cpa_controls(struct kunit *test)
{
	struct orlix_tcti_cpa_control saved_control;
	struct orlix_tcti_pointer_add_observation saved_observation =
		current->thread.user_cpa_add_observation;
	u32 combination;
	size_t leaf_index;

	orlix_tcti_cpu_cpa_control_get(&saved_control);
	for (combination = 0; combination < 32; combination++) {
		const struct orlix_tcti_cpa_control control = {
			.feat_cpa = combination & BIT(0),
			.feat_cpa2 = combination & BIT(1),
			.sctlr2_el1_enabled = combination & BIT(2),
			.sctlr2_el1_cpta0 = combination & BIT(3),
			.sctlr2_el1_cptm0 = combination & BIT(4),
		};

		for (leaf_index = 0; leaf_index < ARRAY_SIZE(pointer_leaves);
		     leaf_index++) {
			struct pt_regs regs = {};
			struct pt_regs before;
			struct orlix_tcti_result result;
			u32 instruction = asr_pointer_instruction(
				&pointer_leaves[leaf_index], 7, 0, 5, 3);
			unsigned long mapped = asr_map(test, instruction);
			bool effective = control.feat_cpa2 &&
				control.sctlr2_el1_enabled && control.sctlr2_el1_cpta0;
			u64 raw = leaf_index ? 0xff00000000000000ULL :
				0x0200000000000000ULL;
			u64 expected = effective ? 0x0140000000000000ULL : raw;

			orlix_tcti_cpu_cpa_control_set(&control);
			memset(&current->thread.user_cpa_add_observation, 0,
			       sizeof(current->thread.user_cpa_add_observation));
			regs.regs[3] = 0xa5a5a5a5a5a5a5a5ULL;
			regs.regs[5] = 0x0100000000000000ULL;
			regs.regs[7] = leaf_index ? 0x0200000000000000ULL :
				0x0100000000000000ULL;
			regs.pc = mapped;
			regs.pstate = PSR_MODE_EL0t | ASR_NZCV;
			regs.syscallno = NO_SYSCALL;
			before = regs;
			result = orlix_tcti_resume_user(current, &regs, current->mm);
			if (!control.feat_cpa) {
				KUNIT_EXPECT_EQ(test,
					ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
					result.reason);
				KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
				KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
				KUNIT_EXPECT_FALSE(test,
					current->thread.user_cpa_add_observation.valid);
			} else {
				KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL,
					result.reason);
				KUNIT_EXPECT_EQ(test, expected, regs.regs[3]);
				KUNIT_EXPECT_EQ(test, effective,
					current->thread.user_cpa_add_observation.poisoned);
			}
			KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
		}
	}
	orlix_tcti_cpu_cpa_control_set(&saved_control);
	current->thread.user_cpa_add_observation = saved_observation;
}

static void asr_pointer_source_mask_reserved_neighbors(struct kunit *test)
{
	const u32 source_mask = 0xffe0e000U;
	size_t leaf_index;

	for (leaf_index = 0; leaf_index < ARRAY_SIZE(pointer_leaves);
	     leaf_index++) {
		u32 bit;
		u32 rejected = 0;

		for (bit = 0; bit < 32; bit++) {
			u32 instruction;

			if (!(source_mask & BIT(bit)))
				continue;
			instruction = pointer_leaves[leaf_index].pattern ^ BIT(bit);
			if (orlix_tcti_decode_aarch64(instruction).decode_class !=
			    ORLIX_TCTI_DECODE_UNSUPPORTED)
				continue;
			rejected++;
		}
		KUNIT_EXPECT_GT(test, rejected, 0U);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
			orlix_tcti_decode_aarch64(
				pointer_leaves[leaf_index].pattern ^ BIT(31)).decode_class);
	}
}

static void asr_pointer_rejections_are_structured(struct kunit *test)
{
	static const u32 instructions[] = {
		0x1a002000U,
		0xba002000U,
	};
	struct orlix_tcti_cpa_control saved_control;
	size_t index;

	for (index = 0; index < ARRAY_SIZE(instructions); index++) {
		struct pt_regs regs = {};
		struct pt_regs before;
		struct orlix_tcti_result result;
		unsigned long mapped = asr_map(test, instructions[index]);

		regs.pc = mapped;
		regs.sp = STACK_TOP - 16;
		regs.pstate = PSR_MODE_EL0t | ASR_NZCV;
		regs.syscallno = NO_SYSCALL;
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				result.reason);
		KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
		KUNIT_EXPECT_EQ(test, instructions[index], result.instruction);
		KUNIT_EXPECT_MEMEQ(test, before.regs, regs.regs,
				   sizeof(regs.regs));
		KUNIT_EXPECT_EQ(test, before.sp, regs.sp);
		KUNIT_EXPECT_EQ(test, before.pc, regs.pc);
		KUNIT_EXPECT_EQ(test, before.pstate, regs.pstate);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	}

	orlix_tcti_cpu_cpa_control_get(&saved_control);
	{
		const struct orlix_tcti_cpa_control disabled = {};

		orlix_tcti_cpu_cpa_control_set(&disabled);
	}
	{
		struct pt_regs regs = {};
		struct pt_regs before;
		struct orlix_tcti_result result;
		u32 instruction = asr_pointer_instruction(&pointer_leaves[0],
							 7, 0, 5, 3);
		unsigned long mapped = asr_map(test, instruction);

		regs.regs[5] = 4;
		regs.regs[7] = 2;
		regs.pc = mapped;
		regs.sp = STACK_TOP - 16;
		regs.pstate = PSR_MODE_EL0t | ASR_NZCV;
		regs.syscallno = NO_SYSCALL;
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				result.reason);
		KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		KUNIT_EXPECT_FALSE(test,
			current->thread.user_cpa_add_observation.valid);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	}
	orlix_tcti_cpu_cpa_control_set(&saved_control);
}
static void asr_seed_extended_state(void)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(current->thread.user_simd); index++)
		current->thread.user_simd[index] =
			0x1100000000000000ULL + index;
	current->thread.user_fpcr = 0x00400000;
	current->thread.user_fpsr = 0x15;
	current->thread.user_fpmr = 0x26;
	current->thread.user_simd_valid = 1;
	orlix_tcti_sve_state_reset(&current->thread.user_sve,
		current->thread.user_simd, ORLIX_TCTI_SVE_MIN_VL_BYTES);
	for (index = 0; index < sizeof(current->thread.user_sve.z); index++)
		((u8 *)current->thread.user_sve.z)[index] = (u8)(index * 17U + 3U);
	for (index = 0; index < sizeof(current->thread.user_sve.p); index++)
		((u8 *)current->thread.user_sve.p)[index] = (u8)(index * 5U + 1U);
	for (index = 0; index < sizeof(current->thread.user_sve.ffr); index++)
		current->thread.user_sve.ffr[index] = (u8)(index * 7U + 2U);
}
static void asr_run_ddi_vector(struct kunit *test,
	const struct asr_ddi_vector *vector)
{
	const struct asr_leaf *leaf = &leaves[vector->leaf_index];
	typeof(current->thread.user_sve) *sve_before =
		kunit_kzalloc(test, sizeof(*sve_before), GFP_KERNEL);
	typeof(current->thread.user_sme) sme_before;
	u64 simd_before[ARRAY_SIZE(current->thread.user_simd)];
	u8 memory_before[sizeof(u32) * 2];
	u8 memory_after[sizeof(memory_before)];
	struct pt_regs regs = {};
	struct pt_regs expected;
	struct orlix_tcti_result result;
	u64 expected_result = leaf->wide ? vector->expected :
		(u32)vector->expected;
	u32 instruction = asr_instruction(leaf, vector->rm, vector->modifier,
		vector->amount, vector->rn, vector->rd);
	unsigned long mapped = asr_map(test, instruction);
	unsigned long fpcr_before;
	unsigned long fpsr_before;
	unsigned long fpmr_before;
	unsigned long simd_valid_before;
	unsigned long expected_pstate;
	unsigned int reg;

	KUNIT_ASSERT_NOT_NULL(test, sve_before);
	for (reg = 0; reg < 31; reg++)
		regs.regs[reg] = 0x8400000000000000ULL + reg;
	if (vector->rn < 31)
		regs.regs[vector->rn] = vector->rn_value;
	if (vector->rm < 31) {
		if (vector->rm == vector->rn)
			KUNIT_ASSERT_EQ(test, vector->rn_value, vector->rm_value);
		regs.regs[vector->rm] = vector->rm_value;
	}
	regs.sp = vector->sp;
	regs.pc = mapped;
	regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT | PSR_V_BIT |
		(vector->carry_in ? PSR_C_BIT : 0);
	regs.syscallno = NO_SYSCALL;
	expected = regs;
	if (vector->rd < 31)
		expected.regs[vector->rd] = expected_result;
	else if (leaf->family == ASR_EXTENDED && !leaf->flags)
		expected.sp = expected_result;
	expected.pc += sizeof(u32);
	expected_pstate = expected.pstate;
	if (leaf->flags)
		expected_pstate = (expected.pstate & ~ASR_NZCV) |
			vector->expected_nzcv;
	expected.pstate = expected_pstate;

	asr_seed_extended_state();
	memcpy(simd_before, current->thread.user_simd, sizeof(simd_before));
	memcpy(sve_before, &current->thread.user_sve, sizeof(*sve_before));
	memcpy(&sme_before, &current->thread.user_sme, sizeof(sme_before));
	fpcr_before = current->thread.user_fpcr;
	fpsr_before = current->thread.user_fpsr;
	fpmr_before = current->thread.user_fpmr;
	simd_valid_before = current->thread.user_simd_valid;
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_read_user_data(current->mm, mapped,
		memory_before, sizeof(memory_before)));

	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_ASSERT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason,
		"%s %s", leaf->name, vector->name);
	KUNIT_EXPECT_EQ(test, 0L, result.status);
	KUNIT_EXPECT_EQ(test, ASR_SVC, result.instruction);
	KUNIT_EXPECT_EQ(test, expected.pc, result.pc);
	KUNIT_EXPECT_MEMEQ_MSG(test, &expected, &regs, sizeof(regs),
		"%s %s complete GPR state", leaf->name, vector->name);
	if (!leaf->wide && vector->rd < 31)
		KUNIT_EXPECT_EQ_MSG(test, 0ULL,
			regs.regs[vector->rd] & ~((u64)U32_MAX),
			"%s %s W write upper clearing", leaf->name, vector->name);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_read_user_data(current->mm, mapped,
		memory_after, sizeof(memory_after)));
	KUNIT_EXPECT_MEMEQ_MSG(test, memory_before, memory_after,
		sizeof(memory_before), "%s %s memory", leaf->name, vector->name);
	KUNIT_EXPECT_MEMEQ_MSG(test, simd_before, current->thread.user_simd,
		sizeof(simd_before), "%s %s FP/SIMD", leaf->name, vector->name);
	KUNIT_EXPECT_EQ(test, fpcr_before, current->thread.user_fpcr);
	KUNIT_EXPECT_EQ(test, fpsr_before, current->thread.user_fpsr);
	KUNIT_EXPECT_EQ(test, fpmr_before, current->thread.user_fpmr);
	KUNIT_EXPECT_EQ(test, simd_valid_before,
		current->thread.user_simd_valid);
	KUNIT_EXPECT_MEMEQ_MSG(test, sve_before, &current->thread.user_sve,
		sizeof(*sve_before), "%s %s SVE", leaf->name, vector->name);
	KUNIT_EXPECT_MEMEQ_MSG(test, &sme_before, &current->thread.user_sme,
		sizeof(sme_before), "%s %s SME", leaf->name, vector->name);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void asr_ddi_arithmetic_alias_and_preservation_vectors(
	struct kunit *test)
{
	u8 leaf_coverage[ARRAY_SIZE(leaves)] = {};
	u8 carry_coverage[8] = {};
	bool nzcv_zero = false;
	bool nzcv_n = false;
	bool nzcv_zc = false;
	bool nzcv_c = false;
	bool nzcv_nv = false;
	bool nzcv_cv = false;
	size_t index;

	for (index = 0; index < ARRAY_SIZE(asr_ddi_vectors); index++) {
		const struct asr_ddi_vector *vector = &asr_ddi_vectors[index];
		const struct asr_leaf *leaf = &leaves[vector->leaf_index];

		KUNIT_ASSERT_LT(test, (u32)vector->leaf_index,
			(u32)ARRAY_SIZE(leaves));
		KUNIT_ASSERT_TRUE(test,
			asr_valid(leaf, vector->modifier, vector->amount));
		leaf_coverage[vector->leaf_index]++;
		if (leaf->family == ASR_CARRY)
			carry_coverage[vector->leaf_index - 16] |=
				vector->carry_in ? BIT(1) : BIT(0);
		if (leaf->flags) {
			nzcv_zero |= vector->expected_nzcv == 0;
			nzcv_n |= vector->expected_nzcv == PSR_N_BIT;
			nzcv_zc |= vector->expected_nzcv ==
				(PSR_Z_BIT | PSR_C_BIT);
			nzcv_c |= vector->expected_nzcv == PSR_C_BIT;
			nzcv_nv |= vector->expected_nzcv ==
				(PSR_N_BIT | PSR_V_BIT);
			nzcv_cv |= vector->expected_nzcv ==
				(PSR_C_BIT | PSR_V_BIT);
		}
		asr_run_ddi_vector(test, vector);
	}
	for (index = 0; index < ARRAY_SIZE(leaf_coverage); index++)
		KUNIT_EXPECT_GT_MSG(test, leaf_coverage[index], (u8)0,
			"%s DDI vectors", leaves[index].name);
	for (index = 0; index < ARRAY_SIZE(carry_coverage); index++)
		KUNIT_EXPECT_EQ_MSG(test, (u8)(BIT(0) | BIT(1)),
			carry_coverage[index], "%s C=0/1", leaves[index + 16].name);
	KUNIT_EXPECT_TRUE(test, nzcv_zero);
	KUNIT_EXPECT_TRUE(test, nzcv_n);
	KUNIT_EXPECT_TRUE(test, nzcv_zc);
	KUNIT_EXPECT_TRUE(test, nzcv_c);
	KUNIT_EXPECT_TRUE(test, nzcv_nv);
	KUNIT_EXPECT_TRUE(test, nzcv_cv);
}
static void asr_extended_fixed_bits_reject_and_preserve_state(
	struct kunit *test)
{
	size_t leaf_index;

	for (leaf_index = 8; leaf_index < 16; leaf_index++) {
		const struct asr_leaf *leaf = &leaves[leaf_index];
		u32 fixed_bit;

		KUNIT_ASSERT_EQ(test, ASR_EXTENDED, leaf->family);
		for (fixed_bit = 22; fixed_bit <= 23; fixed_bit++) {
			typeof(current->thread.user_sve) *sve_before =
				kunit_kzalloc(test, sizeof(*sve_before), GFP_KERNEL);
			typeof(current->thread.user_sme) sme_before;
			u64 simd_before[ARRAY_SIZE(current->thread.user_simd)];
			u8 memory_before[sizeof(u32) * 2];
			u8 memory_after[sizeof(memory_before)];
			struct pt_regs regs = {};
			struct pt_regs regs_before;
			struct orlix_tcti_result result;
			u64 fpcr_before;
			u64 fpsr_before;
			u8 simd_valid_before;
			unsigned long fpmr_before;
			u32 instruction =
				asr_instruction(leaf, 7, 0, 0, 5, 3) | BIT(fixed_bit);
			unsigned long mapped;
			unsigned int reg;

			KUNIT_ASSERT_NOT_NULL(test, sve_before);
			KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
				orlix_tcti_decode_aarch64(instruction).decode_class,
				"%s fixed bit %u instruction %#x", leaf->name,
				fixed_bit, instruction);
			mapped = asr_map(test, instruction);
			for (reg = 0; reg < 31; reg++)
				regs.regs[reg] = 0x8500000000000000ULL + reg;
			regs.sp = 0x00000001ffffffe0ULL;
			regs.pc = mapped;
			regs.pstate = PSR_MODE_EL0t | ASR_NZCV;
			regs.syscallno = NO_SYSCALL;
			regs_before = regs;

			asr_seed_extended_state();
			memcpy(simd_before, current->thread.user_simd,
			       sizeof(simd_before));
			fpcr_before = current->thread.user_fpcr;
			fpsr_before = current->thread.user_fpsr;
			simd_valid_before = current->thread.user_simd_valid;
			fpmr_before = current->thread.user_fpmr;
			memcpy(sve_before, &current->thread.user_sve,
			       sizeof(*sve_before));
			memcpy(&sme_before, &current->thread.user_sme, sizeof(sme_before));
			KUNIT_ASSERT_EQ(test, 0,
				orlix_tcti_read_user_data(current->mm, mapped,
					memory_before, sizeof(memory_before)));

			result = orlix_tcti_resume_user(current, &regs, current->mm);
			KUNIT_EXPECT_EQ_MSG(test,
				ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION, result.reason,
				"%s fixed bit %u structured reason", leaf->name,
				fixed_bit);
			KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
			KUNIT_EXPECT_EQ(test, instruction, result.instruction);
			KUNIT_EXPECT_EQ(test, mapped, result.pc);
			KUNIT_EXPECT_MEMEQ_MSG(test, &regs_before, &regs, sizeof(regs),
				"%s fixed bit %u complete GPR, flags, and PC state",
				leaf->name, fixed_bit);
			KUNIT_ASSERT_EQ(test, 0,
				orlix_tcti_read_user_data(current->mm, mapped,
					memory_after, sizeof(memory_after)));
			KUNIT_EXPECT_MEMEQ_MSG(test, memory_before, memory_after,
				sizeof(memory_before), "%s fixed bit %u memory",
				leaf->name, fixed_bit);
			KUNIT_EXPECT_MEMEQ_MSG(test, simd_before,
				current->thread.user_simd, sizeof(simd_before),
				"%s fixed bit %u FP/SIMD", leaf->name, fixed_bit);
			KUNIT_EXPECT_EQ(test, fpcr_before, current->thread.user_fpcr);
			KUNIT_EXPECT_EQ(test, fpsr_before, current->thread.user_fpsr);
			KUNIT_EXPECT_EQ(test, simd_valid_before,
				current->thread.user_simd_valid);
			KUNIT_EXPECT_EQ(test, fpmr_before, current->thread.user_fpmr);
			KUNIT_EXPECT_MEMEQ_MSG(test, sve_before,
				&current->thread.user_sve, sizeof(*sve_before),
				"%s fixed bit %u SVE", leaf->name, fixed_bit);
			KUNIT_EXPECT_MEMEQ_MSG(test, &sme_before, &current->thread.user_sme,
				sizeof(sme_before), "%s fixed bit %u SME typed state",
				leaf->name, fixed_bit);
			KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
		}
	}
}

static struct kunit_case asr_cases[] = {
	KUNIT_CASE(asr_source_and_decode),
	KUNIT_CASE(asr_resume_semantics),
	KUNIT_CASE(asr_ddi_arithmetic_alias_and_preservation_vectors),
	KUNIT_CASE(asr_reserved_structured_exits),
	KUNIT_CASE(asr_issue_133_exact_cohort),
	KUNIT_CASE(asr_pointer_source_and_decode),
	KUNIT_CASE(asr_pointer_production_semantics),
	KUNIT_CASE(asr_pointer_all_cpa_controls),
	KUNIT_CASE(asr_pointer_source_mask_reserved_neighbors),
	KUNIT_CASE(asr_pointer_rejections_are_structured),
	KUNIT_CASE(asr_extended_fixed_bits_reject_and_preserve_state),
	{}
};

struct kunit_suite orlix_tcti_add_sub_register_source_bound_test_suite = {
	.name = "orlix-tcti-add-sub-register-source-bound",
	.test_cases = asr_cases,
};
kunit_test_suite(orlix_tcti_add_sub_register_source_bound_test_suite);
