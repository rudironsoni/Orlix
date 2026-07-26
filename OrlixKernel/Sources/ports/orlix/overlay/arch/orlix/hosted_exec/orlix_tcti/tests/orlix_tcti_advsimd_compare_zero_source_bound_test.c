// SPDX-License-Identifier: GPL-2.0-only
/*
 * Source-bound production-path proof for the five AdvSIMD integer compare
 * against zero leaves in the pinned AARCHMRS 2026-06 source.
 */
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/string.h>
#include <linux/syscalls.h>

#include "../decode_aarch64.h"

#define COMPARE_ZERO_SVC	0xd4000001U
#define COMPARE_ZERO_RD	11U
#define COMPARE_ZERO_RN	19U

struct compare_zero_manifest_leaf {
	u32 ordinal;
	const char *name;
	const char *mnemonic;
	const char *operation;
	u32 mask;
	u32 pattern;
	const char *feature;
	u32 offset;
	u32 length;
};

#define ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, name, mnemonic, operation, \
	mask, pattern, feature, offset, length) \
	{ ordinal, name, mnemonic, operation, mask, pattern, feature, offset, length },
static const struct compare_zero_manifest_leaf compare_zero_manifest[] = {
#include "../isa/source_manifest.def"
};

#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE

struct compare_zero_leaf {
	u16 ordinal;
	const char *name;
	const char *mnemonic;
	const char *operation;
	u32 mask;
	u32 pattern;
	enum orlix_tcti_simd_vector_compare_op compare_op;
};

static const struct compare_zero_leaf compare_zero_leaves[] = {
	{ 3791U, "CMGT_asimdmisc_Z", "CMGT", "CMGT_advsimd_zero",
	  0xbf3ffc00U, 0x0e208800U, ORLIX_TCTI_SIMD_COMPARE_CMGT },
	{ 3792U, "CMEQ_asimdmisc_Z", "CMEQ", "CMEQ_advsimd_zero",
	  0xbf3ffc00U, 0x0e209800U, ORLIX_TCTI_SIMD_COMPARE_CMEQ },
	{ 3793U, "CMLT_asimdmisc_Z", "CMLT", "CMLT_advsimd",
	  0xbf3ffc00U, 0x0e20a800U, ORLIX_TCTI_SIMD_COMPARE_CMLT },
	{ 3824U, "CMGE_asimdmisc_Z", "CMGE", "CMGE_advsimd_zero",
	  0xbf3ffc00U, 0x2e208800U, ORLIX_TCTI_SIMD_COMPARE_CMGE },
	{ 3825U, "CMLE_asimdmisc_Z", "CMLE", "CMLE_advsimd",
	  0xbf3ffc00U, 0x2e209800U, ORLIX_TCTI_SIMD_COMPARE_CMLE },
};

struct compare_zero_context {
	struct mm_struct *mm;
	u64 simd[ARRAY_SIZE(current->thread.user_simd)];
	unsigned long simd_valid;
	unsigned long fpcr;
	unsigned long fpsr;
};

static int compare_zero_test_init(struct kunit *test)
{
	struct compare_zero_context *context;

	context = kunit_kzalloc(test, sizeof(*context), GFP_KERNEL);
	if (!context)
		return -ENOMEM;
	context->mm = mm_alloc();
	if (!context->mm)
		return -ENOMEM;
	kthread_use_mm(context->mm);
	memcpy(context->simd, current->thread.user_simd, sizeof(context->simd));
	context->simd_valid = current->thread.user_simd_valid;
	context->fpcr = current->thread.user_fpcr;
	context->fpsr = current->thread.user_fpsr;
	test->priv = context;
	return 0;
}

static void compare_zero_test_exit(struct kunit *test)
{
	struct compare_zero_context *context = test->priv;

	memcpy(current->thread.user_simd, context->simd, sizeof(context->simd));
	current->thread.user_simd_valid = context->simd_valid;
	current->thread.user_fpcr = context->fpcr;
	current->thread.user_fpsr = context->fpsr;
	kthread_unuse_mm(context->mm);
	mmput(context->mm);
}

static const struct compare_zero_manifest_leaf *
compare_zero_manifest_by_ordinal(u16 ordinal)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(compare_zero_manifest); index++)
		if (compare_zero_manifest[index].ordinal == ordinal)
			return &compare_zero_manifest[index];
	return NULL;
}

static u32 compare_zero_instruction(const struct compare_zero_leaf *leaf,
				    u8 q, u8 size, u8 rd, u8 rn)
{
	return leaf->pattern | ((u32)q << 30) | ((u32)size << 22) |
		((u32)rn << 5) | rd;
}

static unsigned long compare_zero_map_program(struct kunit *test,
					      u32 instruction)
{
	const u32 program[] = { instruction, COMPARE_ZERO_SVC };
	unsigned long protection = PROT_READ | PROT_WRITE;
	unsigned long flags = MAP_PRIVATE | MAP_ANONYMOUS;
	unsigned long address;
	int ret;

	address = ksys_mmap_pgoff(0, PAGE_SIZE, protection, flags, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(address));
	ret = orlix_tcti_write_user_data(current->mm, address, program, sizeof(program));
	if (ret) {
		vm_munmap(address, PAGE_SIZE);
		KUNIT_FAIL(test, "could not write compare-zero program: %d", ret);
		return 0;
	}
	ret = sys_mprotect(address, PAGE_SIZE, PROT_READ | PROT_EXEC);
	if (ret) {
		vm_munmap(address, PAGE_SIZE);
		KUNIT_FAIL(test, "could not protect compare-zero program: %d", ret);
		return 0;
	}
	return address;
}

static void compare_zero_seed_regs(struct pt_regs *regs, unsigned long pc)
{
	unsigned int index;

	memset(regs, 0, sizeof(*regs));
	for (index = 0; index < ARRAY_SIZE(regs->regs); index++)
		regs->regs[index] = 0x9e3779b97f4a7c15ULL ^
			((u64)(index + 1) * 0x0101010101010101ULL);
	regs->sp = 0x00000001fffffff0ULL;
	regs->pc = pc;
	regs->pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT | PSR_V_BIT;
	regs->orig_x0 = 0xd1b54a32d192ed03ULL;
	regs->syscallno = NO_SYSCALL;
	regs->unused = 0x6d5a56c3U;
}

static u64 compare_zero_lane_value(u8 lane_bits, u8 lane, u8 scenario)
{
	u64 mask = GENMASK_ULL(lane_bits - 1, 0);

	switch ((lane + scenario * 2) % 3) {
	case 0:
		return BIT_ULL(lane_bits - 1) | (lane + 1);
	case 1:
		return 0;
	default:
		return (lane + 1) & mask;
	}
}

static bool compare_zero_matches(enum orlix_tcti_simd_vector_compare_op operation,
				 u64 value, u8 lane_bits)
{
	bool negative = value & BIT_ULL(lane_bits - 1);
	bool zero = value == 0;

	switch (operation) {
	case ORLIX_TCTI_SIMD_COMPARE_CMGT:
		return !negative && !zero;
	case ORLIX_TCTI_SIMD_COMPARE_CMGE:
		return !negative;
	case ORLIX_TCTI_SIMD_COMPARE_CMEQ:
		return zero;
	case ORLIX_TCTI_SIMD_COMPARE_CMLE:
		return negative || zero;
	case ORLIX_TCTI_SIMD_COMPARE_CMLT:
		return negative;
	default:
		return false;
	}
}

static void compare_zero_fill_source(u64 words[2], u8 lane_bytes,
				     u8 result_bytes, u8 scenario)
{
	u8 lane;

	words[0] = 0;
	words[1] = 0;
	for (lane = 0; lane < result_bytes / lane_bytes; lane++) {
		u8 byte = lane * lane_bytes;
		u8 word = byte / sizeof(u64);
		u8 shift = (byte % sizeof(u64)) * 8;

		words[word] |= compare_zero_lane_value(lane_bytes * 8, lane,
						       scenario) << shift;
	}
}

static void compare_zero_expected(u64 words[2],
				  enum orlix_tcti_simd_vector_compare_op operation,
				  const u64 source[2], u8 lane_bytes,
				  u8 result_bytes)
{
	u64 mask = GENMASK_ULL(lane_bytes * 8 - 1, 0);
	u8 lane;

	words[0] = 0;
	words[1] = 0;
	for (lane = 0; lane < result_bytes / lane_bytes; lane++) {
		u8 byte = lane * lane_bytes;
		u8 word = byte / sizeof(u64);
		u8 shift = (byte % sizeof(u64)) * 8;
		u64 value = (source[word] >> shift) & mask;

		if (compare_zero_matches(operation, value, lane_bytes * 8))
			words[word] |= mask << shift;
	}
}

static void compare_zero_seed_simd(u32 instruction)
{
	unsigned int index;

	for (index = 0; index < ARRAY_SIZE(current->thread.user_simd); index++)
		current->thread.user_simd[index] =
			0xa5a5a5a55a5a5a5aULL ^ ((u64)instruction << 11) ^ index;
	current->thread.user_simd_valid = 0;
	current->thread.user_fpcr = BIT(22) | BIT(24);
	current->thread.user_fpsr = BIT(27) | BIT(4);
}

static void compare_zero_expect_code_unchanged(struct kunit *test,
					       unsigned long address,
						const u32 expected[2])
{
	u32 observed[2] = {};
	int ret;

	ret = orlix_tcti_read_user_data(current->mm, address, observed, sizeof(observed));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_MEMEQ(test, expected, observed, sizeof(observed));
}

static void compare_zero_leaves_bind_canonical_artifacts(struct kunit *test)
{
	size_t index;
	u8 q;
	u8 size;

	KUNIT_ASSERT_EQ(test, 5U, ARRAY_SIZE(compare_zero_leaves));
	for (index = 0; index < ARRAY_SIZE(compare_zero_leaves); index++) {
		const struct compare_zero_leaf *leaf = &compare_zero_leaves[index];
		const struct compare_zero_manifest_leaf *source =
			compare_zero_manifest_by_ordinal(leaf->ordinal);

		KUNIT_ASSERT_NOT_NULL(test, source);
		KUNIT_EXPECT_STREQ(test, leaf->name, source->name);
		KUNIT_EXPECT_STREQ(test, leaf->mnemonic, source->mnemonic);
		KUNIT_EXPECT_STREQ(test, leaf->operation, source->operation);
		KUNIT_EXPECT_EQ(test, leaf->mask, source->mask);
		KUNIT_EXPECT_EQ(test, leaf->pattern, source->pattern);

		for (q = 0; q < 2; q++) {
			for (size = 0; size < 4; size++) {
				u32 instruction = compare_zero_instruction(leaf, q, size,
					COMPARE_ZERO_RD, COMPARE_ZERO_RN);
				struct orlix_tcti_decoded_instruction decoded =
					orlix_tcti_decode_aarch64(instruction);

				if (!q && size == 3) {
					KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
							    decoded.decode_class,
							    "%s q=%u size=%u",
							    leaf->name, q, size);
					continue;
				}
				KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_SIMD_VECTOR_COMPARE,
						    decoded.decode_class,
						    "%s q=%u size=%u",
						    leaf->name, q, size);
				KUNIT_EXPECT_EQ(test, COMPARE_ZERO_RD, decoded.rd);
				KUNIT_EXPECT_EQ(test, COMPARE_ZERO_RN, decoded.rn);
				KUNIT_EXPECT_EQ(test, BIT(size), decoded.access_size);
				KUNIT_EXPECT_EQ(test, q ? 16U : 8U,
						decoded.result_size);
				KUNIT_EXPECT_EQ(test, leaf->compare_op,
						decoded.simd_compare_op);
				KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
				KUNIT_EXPECT_TRUE(test, decoded.immediate);
				KUNIT_EXPECT_FALSE(test, decoded.simd_scalar);
			}
		}
	}
}

static void compare_zero_leaves_execute_all_arrangements(struct kunit *test)
{
	size_t index;
	u8 q;
	u8 size;
	u8 scenario;

	for (index = 0; index < ARRAY_SIZE(compare_zero_leaves); index++) {
		const struct compare_zero_leaf *leaf = &compare_zero_leaves[index];

		for (q = 0; q < 2; q++) {
			for (size = 0; size < 4; size++) {
				if (!q && size == 3)
					continue;
				for (scenario = 0; scenario < 2; scenario++) {
					u8 rd = scenario ? COMPARE_ZERO_RN : COMPARE_ZERO_RD;
					u8 lane_bytes = BIT(size);
					u8 result_bytes = q ? 16 : 8;
					u32 instruction = compare_zero_instruction(leaf, q,
						size, rd, COMPARE_ZERO_RN);
					const u32 program[] = { instruction, COMPARE_ZERO_SVC };
					unsigned long address =
						compare_zero_map_program(test, instruction);
					struct pt_regs regs;
					struct pt_regs expected_regs;
					struct orlix_tcti_result result;
					u64 source[2];
					u64 expected_result[2];
					u64 expected_simd[ARRAY_SIZE(current->thread.user_simd)];
					unsigned long *source_register =
						&current->thread.user_simd[COMPARE_ZERO_RN * 2];

					KUNIT_ASSERT_NE(test, 0UL, address);
					compare_zero_expect_code_unchanged(test, address, program);
					compare_zero_seed_simd(instruction);
					compare_zero_fill_source(source, lane_bytes, result_bytes,
								 scenario);
					source_register[0] = source[0];
					source_register[1] = source[1];
					memcpy(expected_simd, current->thread.user_simd,
					       sizeof(expected_simd));
					compare_zero_expected(expected_result, leaf->compare_op,
							      source, lane_bytes, result_bytes);
					expected_simd[rd * 2] = expected_result[0];
					expected_simd[rd * 2 + 1] = q ? expected_result[1] : 0;
					compare_zero_seed_regs(&regs, address);
					expected_regs = regs;
					expected_regs.pc += sizeof(u32);

					result = orlix_tcti_resume_user(current, &regs, current->mm);

					KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason,
							    "%s q=%u size=%u overlap=%u",
							    leaf->name, q, size, scenario);
					KUNIT_EXPECT_EQ(test, 0L, result.status);
					KUNIT_EXPECT_EQ(test, 0UL, result.fault_address);
					KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_FETCH,
							result.fault_access);
					KUNIT_EXPECT_EQ(test, address + sizeof(u32), result.pc);
					KUNIT_EXPECT_EQ(test, COMPARE_ZERO_SVC, result.instruction);
					KUNIT_EXPECT_MEMEQ(test, &expected_regs, &regs,
							   sizeof(regs));
					KUNIT_EXPECT_MEMEQ(test, expected_simd,
							   current->thread.user_simd,
							   sizeof(expected_simd));
					KUNIT_EXPECT_EQ(test, 1UL,
							current->thread.user_simd_valid);
					KUNIT_EXPECT_EQ(test, BIT(22) | BIT(24),
							current->thread.user_fpcr);
					KUNIT_EXPECT_EQ(test, BIT(27) | BIT(4),
							current->thread.user_fpsr);
					compare_zero_expect_code_unchanged(test, address, program);
					KUNIT_EXPECT_EQ(test, 0,
							vm_munmap(address, PAGE_SIZE));
				}
			}
		}
	}
}

static void compare_zero_reserved_arrangements_reject(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(compare_zero_leaves); index++) {
		const struct compare_zero_leaf *leaf = &compare_zero_leaves[index];
		u32 instruction = compare_zero_instruction(leaf, 0, 3,
							   COMPARE_ZERO_RD,
							   COMPARE_ZERO_RN);
		const u32 program[] = { instruction, COMPARE_ZERO_SVC };
		unsigned long address = compare_zero_map_program(test, instruction);
		struct pt_regs regs;
		struct pt_regs before_regs;
		struct orlix_tcti_result result;
		u64 before_simd[ARRAY_SIZE(current->thread.user_simd)];

		KUNIT_ASSERT_NE(test, 0UL, address);
		KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
				orlix_tcti_decode_aarch64(instruction).decode_class);
		compare_zero_seed_simd(instruction);
		memcpy(before_simd, current->thread.user_simd, sizeof(before_simd));
		compare_zero_seed_regs(&regs, address);
		before_regs = regs;
		compare_zero_expect_code_unchanged(test, address, program);

		result = orlix_tcti_resume_user(current, &regs, current->mm);

		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				    result.reason, "%s", leaf->name);
		KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
		KUNIT_EXPECT_EQ(test, 0UL, result.fault_address);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_FETCH, result.fault_access);
		KUNIT_EXPECT_EQ(test, address, result.pc);
		KUNIT_EXPECT_EQ(test, instruction, result.instruction);
		KUNIT_EXPECT_MEMEQ(test, &before_regs, &regs, sizeof(regs));
		KUNIT_EXPECT_MEMEQ(test, before_simd, current->thread.user_simd,
				   sizeof(before_simd));
		KUNIT_EXPECT_EQ(test, 0UL, current->thread.user_simd_valid);
		KUNIT_EXPECT_EQ(test, BIT(22) | BIT(24), current->thread.user_fpcr);
		KUNIT_EXPECT_EQ(test, BIT(27) | BIT(4), current->thread.user_fpsr);
		compare_zero_expect_code_unchanged(test, address, program);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(address, PAGE_SIZE));
	}
}

static struct kunit_case compare_zero_source_bound_cases[] = {
	KUNIT_CASE(compare_zero_leaves_bind_canonical_artifacts),
	KUNIT_CASE(compare_zero_leaves_execute_all_arrangements),
	KUNIT_CASE(compare_zero_reserved_arrangements_reject),
	{}
};

static struct kunit_suite compare_zero_source_bound_suite = {
	.name = "orlix-tcti-advsimd-compare-zero-source-bound",
	.init = compare_zero_test_init,
	.exit = compare_zero_test_exit,
	.test_cases = compare_zero_source_bound_cases,
};

kunit_test_suite(compare_zero_source_bound_suite);

MODULE_LICENSE("GPL");
