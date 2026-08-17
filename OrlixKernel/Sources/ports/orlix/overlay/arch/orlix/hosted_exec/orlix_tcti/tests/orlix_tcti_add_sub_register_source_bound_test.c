// SPDX-License-Identifier: GPL-2.0-only
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/syscalls.h>

#include "../decode_aarch64.h"
#include "orlix_tcti_test_suites.h"

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
static struct kunit_case asr_cases[] = {
    KUNIT_CASE(asr_source_and_decode),
    KUNIT_CASE(asr_resume_semantics),
    KUNIT_CASE(asr_reserved_structured_exits),
    {}};
static int orlix_tcti_add_sub_register_source_bound_test_init(struct kunit *test)
{
	struct mm_struct *mm = mm_alloc();

	if (!mm)
		return -ENOMEM;
	kthread_use_mm(mm);
	test->priv = mm;
	return 0;
}

static void orlix_tcti_add_sub_register_source_bound_test_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (!mm)
		return;
	kthread_unuse_mm(mm);
	mmput(mm);
}

struct kunit_suite orlix_tcti_add_sub_register_source_bound_test_suite = {
    .name = "orlix-tcti-add-sub-register-source-bound",
    .init = orlix_tcti_add_sub_register_source_bound_test_init,
    .exit = orlix_tcti_add_sub_register_source_bound_test_exit,
    .test_cases = asr_cases,
};
kunit_test_suite(orlix_tcti_add_sub_register_source_bound_test_suite);
