// SPDX-License-Identifier: GPL-2.0-only
/*
 * Pinned AARCHMRS 2026-06 source-bound production proof for scalar FP/SIMD
 * register-offset load/store. This is distinct from ordinary integer
 * load/store: Rt names a SIMD register, including V31, rather than XZR.
 */
#include <asm/ptrace.h>
#include <asm/tcti.h>
#include <kunit/test.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/syscalls.h>

#include "../decode_aarch64.h"

#define SFRO_SVC 0xd4000001U

struct sfro_source_leaf {
	u32 ordinal;
	const char *name;
	u32 mask;
	u32 pattern;
};

struct sfro_expectation {
	u32 ordinal;
	bool load;
	u8 size;
};

#define TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, id, mnemonic, operation, mask, \
					     pattern, feature_predicate, offset, length) \
	{ ordinal, id, mask, pattern },
static const struct sfro_source_leaf sfro_source_leaves[] = {
#include "../isa/source_manifest.def"
};
#undef TCTI_A64_SOURCE_MANIFEST_ROW
#undef TCTI_A64_SOURCE_MANIFEST_SOURCE

#define SFRO(ordinal, load, size) { ordinal##U, load, size }
static const struct sfro_expectation sfro_expected[] = {
	SFRO(3305, false, 1), SFRO(3306, false, 1),
	SFRO(3307, true,  1), SFRO(3308, true,  1),
	SFRO(3309, false, 16), SFRO(3310, true,  16),
	SFRO(3315, false, 2), SFRO(3316, true,  2),
	SFRO(3320, false, 4), SFRO(3321, true,  4),
	SFRO(3326, false, 8), SFRO(3327, true,  8),
};
#undef SFRO

static const struct sfro_source_leaf *sfro_leaf(u32 ordinal)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(sfro_source_leaves); index++)
		if (sfro_source_leaves[index].ordinal == ordinal)
			return &sfro_source_leaves[index];
	return NULL;
}

static u32 sfro_instruction(const struct sfro_source_leaf *leaf, u8 rt,
				    u8 rn, u8 rm, u8 option, bool shift)
{
	return leaf->pattern | ((u32)rm << 16) | ((u32)option << 13) |
		(shift ? BIT(12) : 0) | ((u32)rn << 5) | rt;
}

static unsigned long sfro_map_code(struct kunit *test, u32 instruction)
{
	const u32 program[] = { instruction, SFRO_SVC };
	unsigned long mapped;
	int ret;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	ret = tcti_write_user_data(current->mm, mapped, program, sizeof(program));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = sys_mprotect(mapped, PAGE_SIZE, PROT_READ | PROT_EXEC);
	KUNIT_ASSERT_EQ(test, 0, ret);
	return mapped;
}

static unsigned long sfro_map_data(struct kunit *test)
{
	unsigned long mapped;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	return mapped;
}

static void sfro_source_decode_is_exact_and_reserved_is_rejected(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(sfro_expected); index++) {
		const struct sfro_expectation *expected = &sfro_expected[index];
		const struct sfro_source_leaf *leaf = sfro_leaf(expected->ordinal);
		struct tcti_decoded_instruction decoded;
		u32 instruction;

		KUNIT_ASSERT_NOT_NULL(test, leaf);
		instruction = sfro_instruction(leaf, 0, 10, 1, 3, true);
		KUNIT_EXPECT_EQ_MSG(test, leaf->pattern,
			instruction & leaf->mask, "%s ordinal %u", leaf->name,
			expected->ordinal);
		decoded = tcti_decode_aarch64(instruction);
		KUNIT_EXPECT_EQ_MSG(test, TCTI_DECODE_LOAD_STORE_REGISTER_OFFSET,
			decoded.decode_class, "%s ordinal %u", leaf->name,
			expected->ordinal);
		KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
		KUNIT_EXPECT_EQ(test, expected->load, decoded.load);
		KUNIT_EXPECT_EQ(test, expected->size, decoded.access_size);
		KUNIT_EXPECT_EQ(test, expected->size, decoded.result_size);
		KUNIT_EXPECT_EQ(test, 10U, decoded.rn);
		KUNIT_EXPECT_EQ(test, 1U, decoded.rm);
		KUNIT_EXPECT_EQ(test, 3U, decoded.offset_extend);
		KUNIT_EXPECT_TRUE(test, decoded.offset_shift);

	}

	/* FP/SIMD register-offset allows only extend options 010, 011, 110, 111. */
	KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNSUPPORTED,
		tcti_decode_aarch64(0x3c200800U).decode_class);

	/* size=01, opc=10 has no scalar FP/SIMD load/store variant. */
	KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNSUPPORTED,
		tcti_decode_aarch64(0x7ca06800U).decode_class);
}

static void sfro_run(struct kunit *test, u32 instruction, struct pt_regs *regs)
{
	struct tcti_result result;
	unsigned long code;

	code = sfro_map_code(test, instruction);
	regs->pc = code;
	regs->pstate = PSR_MODE_EL0t;
	regs->syscallno = NO_SYSCALL;
	result = tcti_resume_user(current, regs, current->mm);
	KUNIT_EXPECT_EQ(test, TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0L, result.status);
	KUNIT_EXPECT_EQ(test, SFRO_SVC, result.instruction);
	KUNIT_EXPECT_EQ(test, code + 2 * sizeof(u32), result.pc);
	KUNIT_EXPECT_EQ(test, code + 2 * sizeof(u32), regs->pc);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
}

static void sfro_production_uses_uxtw_scaled_address_and_preserves_base(
	struct kunit *test)
{
	const struct sfro_source_leaf *store = sfro_leaf(3309U);
	const struct sfro_source_leaf *load = sfro_leaf(3310U);
	struct pt_regs regs = {};
	unsigned long data = sfro_map_data(test);
	u64 observed[2] = {};
	int ret;

	KUNIT_ASSERT_NOT_NULL(test, store);
	KUNIT_ASSERT_NOT_NULL(test, load);
	current->thread.user_simd[0] = 0x0123456789abcdefULL;
	current->thread.user_simd[1] = 0xfedcba9876543210ULL;
	current->thread.user_simd_valid = 1;
	regs.regs[10] = data;
	regs.regs[1] = 0xffffffff00000003ULL;
	sfro_run(test, sfro_instruction(store, 0, 10, 1, 3, true), &regs);
	ret = tcti_read_user_data(current->mm, data + 3 * 16, observed,
				  sizeof(observed));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, current->thread.user_simd[0], observed[0]);
	KUNIT_EXPECT_EQ(test, current->thread.user_simd[1], observed[1]);
	KUNIT_EXPECT_EQ(test, data, regs.regs[10]);

	memset(&regs, 0, sizeof(regs));
	current->thread.user_simd[0] = 0;
	current->thread.user_simd[1] = 0;
	regs.regs[10] = data;
	regs.regs[1] = 0xffffffff00000003ULL;
	sfro_run(test, sfro_instruction(load, 0, 10, 1, 3, true), &regs);
	KUNIT_EXPECT_EQ(test, observed[0], current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, observed[1], current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, data, regs.regs[10]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
}

static void sfro_sp_base_v31_and_fault_leave_complete_state_unchanged(
	struct kunit *test)
{
	const struct sfro_source_leaf *store = sfro_leaf(3315U);
	const struct sfro_source_leaf *fault = sfro_leaf(3310U);
	struct pt_regs regs = {};
	struct pt_regs before;
	u64 simd_before[ARRAY_SIZE(current->thread.user_simd)];
	unsigned long simd_valid_before;
	unsigned long fpcr_before;
	unsigned long fpsr_before;
	u32 code_before[2];
	struct tcti_result result;
	unsigned long data = sfro_map_data(test);
	unsigned long code;
	u16 observed = 0;
	int ret;

	KUNIT_ASSERT_NOT_NULL(test, store);
	KUNIT_ASSERT_NOT_NULL(test, fault);
	current->thread.user_simd[31 * 2] = 0x0123456789abcd55ULL;
	current->thread.user_simd[31 * 2 + 1] = 0xfeedfacecafebeefULL;
	regs.sp = data - 2;
	regs.regs[1] = 2;
	sfro_run(test, sfro_instruction(store, 31, 31, 1, 3, false), &regs);
	ret = tcti_read_user_data(current->mm, data, &observed, sizeof(observed));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, (u16)0xcd55, observed);
	KUNIT_EXPECT_EQ(test, data - 2, regs.sp);

	memset(&regs, 0xa5, sizeof(regs));
	regs.pc = code = sfro_map_code(test, sfro_instruction(fault, 0, 10, 1, 3,
							 false));
	regs.pstate = PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;
	regs.regs[10] = 0;
	regs.regs[1] = 0;
	memcpy(simd_before, current->thread.user_simd, sizeof(simd_before));
	simd_valid_before = current->thread.user_simd_valid;
	fpcr_before = current->thread.user_fpcr;
	fpsr_before = current->thread.user_fpsr;
	before = regs;
	ret = tcti_read_user_data(current->mm, code, code_before, sizeof(code_before));
	KUNIT_ASSERT_EQ(test, 0, ret);
	result = tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, TCTI_EXIT_USER_FAULT, result.reason);
	KUNIT_EXPECT_EQ(test, -EFAULT, result.status);
	KUNIT_EXPECT_EQ(test, 0UL, result.fault_address);
	KUNIT_EXPECT_EQ(test, TCTI_ACCESS_READ, result.fault_access);
	KUNIT_EXPECT_EQ(test, code, result.pc);
	KUNIT_EXPECT_EQ(test, code_before[0], result.instruction);
	KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	KUNIT_EXPECT_MEMEQ(test, simd_before, current->thread.user_simd,
			   sizeof(simd_before));
	KUNIT_EXPECT_EQ(test, simd_valid_before, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, fpcr_before, current->thread.user_fpcr);
	KUNIT_EXPECT_EQ(test, fpsr_before, current->thread.user_fpsr);
	ret = tcti_read_user_data(current->mm, code, code_before, sizeof(code_before));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, sfro_instruction(fault, 0, 10, 1, 3, false),
			code_before[0]);
	KUNIT_EXPECT_EQ(test, SFRO_SVC, code_before[1]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
}

static struct kunit_case sfro_cases[] = {
	KUNIT_CASE(sfro_source_decode_is_exact_and_reserved_is_rejected),
	KUNIT_CASE(sfro_production_uses_uxtw_scaled_address_and_preserves_base),
	KUNIT_CASE(sfro_sp_base_v31_and_fault_leave_complete_state_unchanged),
	{}
};

static struct kunit_suite sfro_suite = {
	.name = "orlix-tcti-simd-fp-register-offset-load-store",
	.test_cases = sfro_cases,
};

kunit_test_suite(sfro_suite);

MODULE_LICENSE("GPL");
