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
				if (!asr_valid(l, a, b)) {
					KUNIT_EXPECT_EQ(test,
							ORLIX_TCTI_DECODE_UNSUPPORTED,
							d.decode_class);
					continue;
				}
				KUNIT_EXPECT_EQ(test, asr_class(l->family),
						d.decode_class);
				KUNIT_EXPECT_EQ(test, l->wide, d.is_64bit);
				KUNIT_EXPECT_EQ(test, l->subtract, d.subtract);
				KUNIT_EXPECT_EQ(test, l->flags, d.set_flags);
				KUNIT_EXPECT_EQ(test, 5, d.rn);
				KUNIT_EXPECT_EQ(test, 7, d.rm);
				KUNIT_EXPECT_EQ(test, 3, d.rd);
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
	static const u16 expected_ordinals[] = {
		2173, 2174, 2175, 2176, 2177, 2178, 2179, 2180,
		3450, 3451, 3452, 3453, 3454, 3455, 3456, 3457,
		3458, 3459, 3460, 3461, 3462, 3463, 3464, 3465,
		3466, 3467, 3468, 3469, 3470, 3471, 3472, 3473,
		3474, 3475,
	};
	const struct orlix_tcti_execution_slice_map *map =
		orlix_tcti_execution_slice_map_canonical();
	bool found[ARRAY_SIZE(expected_ordinals)] = {};
	size_t member_index;
	size_t issue_count = 0;

	KUNIT_ASSERT_NOT_NULL(test, map);
	KUNIT_EXPECT_EQ(test, 34, (int)ARRAY_SIZE(expected_ordinals));
	for (member_index = 0; member_index < map->counts.leaf_count;
	     member_index++) {
		const struct orlix_tcti_execution_slice_member *member =
			&map->members[member_index];
		const struct orlix_tcti_execution_slice_family *family =
			&map->families[member->family_index];
		size_t expected_index;

		if (family->issue_id != 133U)
			continue;
		issue_count++;
		KUNIT_EXPECT_STREQ(test, "base-a64-add-subtract",
				   family->stable_id);
		for (expected_index = 0;
		     expected_index < ARRAY_SIZE(expected_ordinals);
		     expected_index++) {
			if (expected_ordinals[expected_index] != member->ordinal)
				continue;
			KUNIT_EXPECT_FALSE(test, found[expected_index]);
			found[expected_index] = true;
			break;
		}
		KUNIT_EXPECT_LT(test, expected_index,
				(size_t)ARRAY_SIZE(expected_ordinals));
	}
	KUNIT_EXPECT_EQ(test, (size_t)34, issue_count);
	for (member_index = 0; member_index < ARRAY_SIZE(found); member_index++)
		KUNIT_EXPECT_TRUE(test, found[member_index]);
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

static u64 asr_pointer_poison(u64 result, u64 base)
{
	result &= GENMASK_ULL(53, 0);
	result |= base & GENMASK_ULL(63, 55);
	if (!(base & BIT_ULL(55)))
		result |= BIT_ULL(54);
	return result;
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
	} cases[] = {
		{ 0, 3, 5, 7, 2, 0x1000, 3, { .feat_cpa = true },
		  false, false, false, false },
		{ 1, 31, 31, 7, 7, 0x9000, 0x11,
		  { .feat_cpa = true }, false, false, false, false },
		{ 0, 4, 5, 31, 7, 0x12345678, U64_MAX,
		  { .feat_cpa = true }, false, false, false, false },
		{ 0, 8, 5, 7, 0, 0x0100000000000000ULL,
		  0x0100000000000000ULL,
		  { true, true, true, true, false }, false, true, true, true },
		{ 0, 9, 5, 7, 0, BIT_ULL(55), 1,
		  { true, true, true, true, false }, true, true, true, true },
		{ 1, 10, 5, 7, 0, 0x0100000000000000ULL,
		  0x0200000000000000ULL,
		  { true, false, true, true, false }, false, true, false, false },
	};
	struct orlix_tcti_cpa_control saved_control =
		current->thread.user_cpa_control;
	struct orlix_tcti_pointer_add_observation saved_observation =
		current->thread.user_cpa_add_observation;
	size_t index;

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
		u64 expected = cases[index].poisoned ?
			asr_pointer_poison(arithmetic, cases[index].base) : arithmetic;

		current->thread.user_cpa_control = cases[index].control;
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
		else
			KUNIT_EXPECT_EQ(test, expected, regs.regs[cases[index].rd]);
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
	current->thread.user_cpa_control = saved_control;
	current->thread.user_cpa_add_observation = saved_observation;
}

static void asr_pointer_rejections_are_structured(struct kunit *test)
{
	static const u32 instructions[] = {
		0x1a002000U,
		0xba002000U,
	};
	struct orlix_tcti_cpa_control saved_control =
		current->thread.user_cpa_control;
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

	current->thread.user_cpa_control = (struct orlix_tcti_cpa_control) {};
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
	current->thread.user_cpa_control = saved_control;
}

static struct kunit_case asr_cases[] = {
    KUNIT_CASE(asr_source_and_decode),
    KUNIT_CASE(asr_resume_semantics),
    KUNIT_CASE(asr_reserved_structured_exits),
	KUNIT_CASE(asr_issue_133_exact_cohort),
	KUNIT_CASE(asr_pointer_source_and_decode),
	KUNIT_CASE(asr_pointer_production_semantics),
	KUNIT_CASE(asr_pointer_rejections_are_structured),
    {}};
struct kunit_suite orlix_tcti_add_sub_register_source_bound_test_suite = {
    .name = "orlix-tcti-add-sub-register-source-bound",
    .test_cases = asr_cases,
};
kunit_test_suite(orlix_tcti_add_sub_register_source_bound_test_suite);
