// SPDX-License-Identifier: GPL-2.0-only
/*
 * Production-path evidence for the ordinary AArch64 pair load/store cohort.
 * Every row below is an exact pinned AARCHMRS 2026-06 source ordinal.  LSUI
 * and MTE pair leaves deliberately belong to their own extension suites.
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

#include "target_instruction_artifact.h"
#include "../decode_aarch64.h"

#define PLSP_SVC 0xd4000001U

struct plsp_source_leaf {
	u32 ordinal;
	const char *name;
	const char *mnemonic;
	const char *operation;
	u32 mask;
	u32 pattern;
};

struct plsp_expectation {
	u32 ordinal;
	bool load;
	bool simd_fp;
	u8 access_size;
	u8 result_size;
	bool sign_extend_load;
	enum tcti_memory_index_mode index_mode;
};

struct plsp_context {
	u64 simd[ARRAY_SIZE(current->thread.user_simd)];
	unsigned long simd_valid;
	unsigned long fpcr;
	unsigned long fpsr;
};

#define TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, id, mnemonic, operation, mask, \
					     pattern, feature_predicate, offset, length) \
	{ ordinal, id, mnemonic, operation, mask, pattern },
static const struct plsp_source_leaf plsp_source_leaves[] = {
#include "../isa/source_manifest.def"
};
#undef TCTI_A64_SOURCE_MANIFEST_ROW
#undef TCTI_A64_SOURCE_MANIFEST_SOURCE

#define PLSP(ordinal, load, simd, access, result, sign, mode) \
	{ ordinal##U, load, simd, access, result, sign, mode }

static const struct plsp_expectation plsp_expected[] = {
	PLSP(2856, false, false, 4, 4, false, TCTI_MEMORY_INDEX_SIGNED_OFFSET),
	PLSP(2857, true,  false, 4, 4, false, TCTI_MEMORY_INDEX_SIGNED_OFFSET),
	PLSP(2858, false, true,  4, 4, false, TCTI_MEMORY_INDEX_SIGNED_OFFSET),
	PLSP(2859, true,  true,  4, 4, false, TCTI_MEMORY_INDEX_SIGNED_OFFSET),
	PLSP(2860, false, true,  8, 8, false, TCTI_MEMORY_INDEX_SIGNED_OFFSET),
	PLSP(2861, true,  true,  8, 8, false, TCTI_MEMORY_INDEX_SIGNED_OFFSET),
	PLSP(2862, false, false, 8, 8, false, TCTI_MEMORY_INDEX_SIGNED_OFFSET),
	PLSP(2863, true,  false, 8, 8, false, TCTI_MEMORY_INDEX_SIGNED_OFFSET),
	PLSP(2864, false, true, 16,16, false, TCTI_MEMORY_INDEX_SIGNED_OFFSET),
	PLSP(2865, true,  true, 16,16, false, TCTI_MEMORY_INDEX_SIGNED_OFFSET),
	PLSP(2870, false, false, 4, 4, false, TCTI_MEMORY_INDEX_POST),
	PLSP(2871, true,  false, 4, 4, false, TCTI_MEMORY_INDEX_POST),
	PLSP(2872, false, true,  4, 4, false, TCTI_MEMORY_INDEX_POST),
	PLSP(2873, true,  true,  4, 4, false, TCTI_MEMORY_INDEX_POST),
	PLSP(2875, true,  false, 4, 8, true,  TCTI_MEMORY_INDEX_POST),
	PLSP(2876, false, true,  8, 8, false, TCTI_MEMORY_INDEX_POST),
	PLSP(2877, true,  true,  8, 8, false, TCTI_MEMORY_INDEX_POST),
	PLSP(2878, false, false, 8, 8, false, TCTI_MEMORY_INDEX_POST),
	PLSP(2879, true,  false, 8, 8, false, TCTI_MEMORY_INDEX_POST),
	PLSP(2880, false, true, 16,16, false, TCTI_MEMORY_INDEX_POST),
	PLSP(2881, true,  true, 16,16, false, TCTI_MEMORY_INDEX_POST),
	PLSP(2886, false, false, 4, 4, false, TCTI_MEMORY_INDEX_SIGNED_OFFSET),
	PLSP(2887, true,  false, 4, 4, false, TCTI_MEMORY_INDEX_SIGNED_OFFSET),
	PLSP(2888, false, true,  4, 4, false, TCTI_MEMORY_INDEX_SIGNED_OFFSET),
	PLSP(2889, true,  true,  4, 4, false, TCTI_MEMORY_INDEX_SIGNED_OFFSET),
	PLSP(2891, true,  false, 4, 8, true,  TCTI_MEMORY_INDEX_SIGNED_OFFSET),
	PLSP(2892, false, true,  8, 8, false, TCTI_MEMORY_INDEX_SIGNED_OFFSET),
	PLSP(2893, true,  true,  8, 8, false, TCTI_MEMORY_INDEX_SIGNED_OFFSET),
	PLSP(2894, false, false, 8, 8, false, TCTI_MEMORY_INDEX_SIGNED_OFFSET),
	PLSP(2895, true,  false, 8, 8, false, TCTI_MEMORY_INDEX_SIGNED_OFFSET),
	PLSP(2896, false, true, 16,16, false, TCTI_MEMORY_INDEX_SIGNED_OFFSET),
	PLSP(2897, true,  true, 16,16, false, TCTI_MEMORY_INDEX_SIGNED_OFFSET),
	PLSP(2902, false, false, 4, 4, false, TCTI_MEMORY_INDEX_PRE),
	PLSP(2903, true,  false, 4, 4, false, TCTI_MEMORY_INDEX_PRE),
	PLSP(2904, false, true,  4, 4, false, TCTI_MEMORY_INDEX_PRE),
	PLSP(2905, true,  true,  4, 4, false, TCTI_MEMORY_INDEX_PRE),
	PLSP(2907, true,  false, 4, 8, true,  TCTI_MEMORY_INDEX_PRE),
	PLSP(2908, false, true,  8, 8, false, TCTI_MEMORY_INDEX_PRE),
	PLSP(2909, true,  true,  8, 8, false, TCTI_MEMORY_INDEX_PRE),
	PLSP(2910, false, false, 8, 8, false, TCTI_MEMORY_INDEX_PRE),
	PLSP(2911, true,  false, 8, 8, false, TCTI_MEMORY_INDEX_PRE),
	PLSP(2912, false, true, 16,16, false, TCTI_MEMORY_INDEX_PRE),
	PLSP(2913, true,  true, 16,16, false, TCTI_MEMORY_INDEX_PRE),
};
#undef PLSP

static const struct plsp_source_leaf *plsp_source_leaf(u32 ordinal)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(plsp_source_leaves); index++)
		if (plsp_source_leaves[index].ordinal == ordinal)
			return &plsp_source_leaves[index];
	return NULL;
}

static int plsp_test_init(struct kunit *test)
{
	struct plsp_context *context;

	context = kunit_kzalloc(test, sizeof(*context), GFP_KERNEL);
	if (!context)
		return -ENOMEM;
	memcpy(context->simd, current->thread.user_simd, sizeof(context->simd));
	context->simd_valid = current->thread.user_simd_valid;
	context->fpcr = current->thread.user_fpcr;
	context->fpsr = current->thread.user_fpsr;
	test->priv = context;
	return 0;
}

static void plsp_test_exit(struct kunit *test)
{
	struct plsp_context *context = test->priv;

	memcpy(current->thread.user_simd, context->simd, sizeof(context->simd));
	current->thread.user_simd_valid = context->simd_valid;
	current->thread.user_fpcr = context->fpcr;
	current->thread.user_fpsr = context->fpsr;
}

static const char *plsp_artifact_string(
	const struct tcti_target_instruction_artifact *artifact, u32 offset)
{
	if (offset >= artifact->string_pool_size)
		return NULL;
	return (const char *)artifact->string_pool + offset;
}

static void plsp_seed_state(struct pt_regs *regs, u32 salt)
{
	size_t index;

	memset(regs, 0x5a, sizeof(*regs));
	for (index = 0; index < ARRAY_SIZE(regs->regs); index++)
		regs->regs[index] = 0x9e3779b97f4a7c15ULL ^ ((u64)salt << 17) ^
			index * 0x0101010101010101ULL;
	regs->sp = 0xfedcba9876543210ULL ^ salt;
	for (index = 0; index < ARRAY_SIZE(current->thread.user_simd); index++)
		current->thread.user_simd[index] = 0xd1b54a32d192ed03ULL ^
			((u64)salt << 23) ^ index * 0x0102030405060708ULL;
	current->thread.user_simd_valid = 1;
	current->thread.user_fpcr = 0x00300000UL | (salt & 0x1fU);
	current->thread.user_fpsr = BIT(27) | BIT(4) | (salt & 0xfU);
}

static u32 plsp_instruction(const struct plsp_source_leaf *leaf)
{
	/* imm7=1, Rt=x0/v0, Rt2=x2/v2, Rn=x10. */
	return leaf->pattern | BIT(15) | (2U << 10) | (10U << 5);
}

static unsigned long plsp_map_instruction(struct kunit *test, u32 instruction)
{
	const u32 program[] = { instruction, PLSP_SVC };
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

static unsigned long plsp_map_data(struct kunit *test, size_t bytes)
{
	unsigned long mapped;

	mapped = ksys_mmap_pgoff(0, bytes, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	return mapped;
}

static struct tcti_result plsp_run(struct kunit *test, u32 instruction,
					   struct pt_regs *regs)
{
	struct tcti_result result;
	unsigned long pc = regs->pc;

	result = tcti_resume_user(current, regs, current->mm);
	KUNIT_EXPECT_EQ(test, TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0L, result.status);
	KUNIT_EXPECT_EQ(test, PLSP_SVC, result.instruction);
	KUNIT_EXPECT_EQ(test, pc + sizeof(u32), result.pc);
	KUNIT_EXPECT_EQ(test, pc + sizeof(u32), regs->pc);
	return result;
}

static unsigned long plsp_address(unsigned long base,
				  const struct tcti_decoded_instruction *decoded)
{
	return decoded->memory_index_mode == TCTI_MEMORY_INDEX_POST ? base :
		base + decoded->memory_offset;
}

static void plsp_write_pair(struct kunit *test, unsigned long address,
			    u8 size, const u64 values[4])
{
	int ret;

	ret = tcti_write_user_data(current->mm, address, values, size);
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = tcti_write_user_data(current->mm, address + size,
				  values + 2, size);
	KUNIT_ASSERT_EQ(test, 0, ret);
}

static void plsp_expect_pair(struct kunit *test, unsigned long address,
			     u8 size, const u64 values[4])
{
	u64 observed[2] = {};
	int ret;

	ret = tcti_read_user_data(current->mm, address, observed, size);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, size == sizeof(u32) ? (u64)(u32)values[0] :
			values[0], observed[0]);
	if (size == 2 * sizeof(u64))
		KUNIT_EXPECT_EQ(test, values[1], observed[1]);
	ret = tcti_read_user_data(current->mm, address + size, observed, size);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, size == sizeof(u32) ? (u64)(u32)values[2] :
			values[2], observed[0]);
	if (size == 2 * sizeof(u64))
		KUNIT_EXPECT_EQ(test, values[3], observed[1]);
}

static void plsp_every_source_leaf_executes_through_resume(struct kunit *test)
{
	const u64 values[] = {
		0x8877665580000001ULL, 0x0f1e2d3c4b5a6978ULL,
		0x112233447ffffffeULL, 0x8796a5b4c3d2e1f0ULL,
	};
	const struct tcti_target_instruction_artifact *artifact =
		tcti_target_instruction_artifact_canonical();
	struct tcti_target_instruction_artifact_validation_result validation;
	unsigned long data = plsp_map_data(test, PAGE_SIZE);
	size_t index;

	KUNIT_ASSERT_EQ(test, 0,
		tcti_target_instruction_artifact_validate(artifact, &validation));

	for (index = 0; index < ARRAY_SIZE(plsp_expected); index++) {
		const struct plsp_expectation *expected = &plsp_expected[index];
		const struct plsp_source_leaf *leaf =
			plsp_source_leaf(expected->ordinal);
		const struct tcti_target_instruction_artifact_leaf *canonical;
		struct tcti_decoded_instruction decoded;
		struct pt_regs regs = {};
		struct pt_regs regs_before;
		struct pt_regs regs_expected;
		u64 simd_before[ARRAY_SIZE(current->thread.user_simd)];
		u64 simd_expected[ARRAY_SIZE(current->thread.user_simd)];
		unsigned long simd_valid_before;
		unsigned long fpcr_before;
		unsigned long fpsr_before;
		unsigned long text;
		unsigned long base = data + 256;
		unsigned long address;
		u32 instruction;

		KUNIT_ASSERT_NOT_NULL(test, leaf);
		KUNIT_ASSERT_LT(test, expected->ordinal, artifact->leaf_count);
		canonical = &artifact->leaves[expected->ordinal];
		KUNIT_ASSERT_NOT_NULL(test, plsp_artifact_string(artifact,
				canonical->name_offset));
		KUNIT_ASSERT_NOT_NULL(test, plsp_artifact_string(artifact,
				canonical->operation_offset));
		KUNIT_EXPECT_STREQ(test, leaf->name, plsp_artifact_string(artifact,
				canonical->name_offset));
		KUNIT_EXPECT_STREQ(test, leaf->operation, plsp_artifact_string(artifact,
				canonical->operation_offset));
		KUNIT_EXPECT_EQ(test, leaf->mask, canonical->encoding_mask);
		KUNIT_EXPECT_EQ(test, leaf->pattern, canonical->encoding_pattern);
		instruction = plsp_instruction(leaf);
		KUNIT_EXPECT_EQ_MSG(test, leaf->pattern,
					instruction & leaf->mask, "%s ordinal %u",
					leaf->name, leaf->ordinal);
		decoded = tcti_decode_aarch64(instruction);
		KUNIT_ASSERT_EQ_MSG(test, TCTI_DECODE_LOAD_STORE_PAIR,
					decoded.decode_class, "%s ordinal %u",
					leaf->name, leaf->ordinal);
		KUNIT_EXPECT_EQ(test, expected->load, decoded.load);
		KUNIT_EXPECT_EQ(test, expected->simd_fp, decoded.simd_fp);
		KUNIT_EXPECT_EQ(test, expected->access_size, decoded.access_size);
		KUNIT_EXPECT_EQ(test, expected->result_size, decoded.result_size);
		KUNIT_EXPECT_EQ(test, expected->sign_extend_load,
				decoded.sign_extend_load);
		KUNIT_EXPECT_EQ(test, expected->index_mode,
				decoded.memory_index_mode);
		KUNIT_EXPECT_EQ(test, (s64)expected->access_size,
				decoded.memory_offset);

		address = plsp_address(base, &decoded);
		plsp_seed_state(&regs, expected->ordinal);
		regs.regs[10] = base;
		if (expected->load) {
			plsp_write_pair(test, address, expected->access_size, values);
			regs.regs[0] = ~0ULL;
			regs.regs[2] = ~0ULL;
			current->thread.user_simd[0] = ~0ULL;
			current->thread.user_simd[1] = ~0ULL;
			current->thread.user_simd[4] = ~0ULL;
			current->thread.user_simd[5] = ~0ULL;
		} else if (expected->simd_fp) {
			current->thread.user_simd[0] = values[0];
			current->thread.user_simd[1] = values[1];
			current->thread.user_simd[4] = values[2];
			current->thread.user_simd[5] = values[3];
		} else {
			regs.regs[0] = values[0];
			regs.regs[2] = values[2];
		}

		memcpy(simd_before, current->thread.user_simd, sizeof(simd_before));
		memcpy(simd_expected, simd_before, sizeof(simd_expected));
		simd_valid_before = current->thread.user_simd_valid;
		fpcr_before = current->thread.user_fpcr;
		fpsr_before = current->thread.user_fpsr;
		text = plsp_map_instruction(test, instruction);
		regs.pc = text;
		regs.pstate = PSR_MODE_EL0t;
		regs.syscallno = NO_SYSCALL;
		regs_before = regs;
		plsp_run(test, instruction, &regs);
		regs_expected = regs_before;
		regs_expected.pc = text + sizeof(u32);
		if (expected->index_mode != TCTI_MEMORY_INDEX_SIGNED_OFFSET)
			regs_expected.regs[10] = base + expected->access_size;
		if (expected->load && expected->simd_fp) {
			KUNIT_EXPECT_EQ(test, expected->access_size == sizeof(u32) ?
				(u64)(u32)values[0] : values[0],
				current->thread.user_simd[0]);
			KUNIT_EXPECT_EQ(test, expected->access_size == 2 * sizeof(u64) ?
				values[1] : 0ULL, current->thread.user_simd[1]);
			KUNIT_EXPECT_EQ(test, expected->access_size == sizeof(u32) ?
				(u64)(u32)values[2] : values[2],
				current->thread.user_simd[4]);
			KUNIT_EXPECT_EQ(test, expected->access_size == 2 * sizeof(u64) ?
				values[3] : 0ULL, current->thread.user_simd[5]);
			simd_expected[0] = expected->access_size == sizeof(u32) ?
				(u64)(u32)values[0] : values[0];
			simd_expected[1] = expected->access_size == 2 * sizeof(u64) ?
				values[1] : 0ULL;
			simd_expected[4] = expected->access_size == sizeof(u32) ?
				(u64)(u32)values[2] : values[2];
			simd_expected[5] = expected->access_size == 2 * sizeof(u64) ?
				values[3] : 0ULL;
		} else if (expected->load && expected->sign_extend_load) {
			KUNIT_EXPECT_EQ(test, 0xffffffff80000001ULL, regs.regs[0]);
			KUNIT_EXPECT_EQ(test, 0x000000007ffffffeULL, regs.regs[2]);
			regs_expected.regs[0] = 0xffffffff80000001ULL;
			regs_expected.regs[2] = 0x000000007ffffffeULL;
		} else if (expected->load) {
			KUNIT_EXPECT_EQ(test, expected->access_size == sizeof(u32) ?
				(u64)(u32)values[0] : values[0], regs.regs[0]);
			KUNIT_EXPECT_EQ(test, expected->access_size == sizeof(u32) ?
				(u64)(u32)values[2] : values[2], regs.regs[2]);
			regs_expected.regs[0] = expected->access_size == sizeof(u32) ?
				(u64)(u32)values[0] : values[0];
			regs_expected.regs[2] = expected->access_size == sizeof(u32) ?
				(u64)(u32)values[2] : values[2];
		} else {
			plsp_expect_pair(test, address, expected->access_size, values);
		}
		KUNIT_EXPECT_MEMEQ(test, &regs_expected, &regs, sizeof(regs));
		KUNIT_EXPECT_MEMEQ(test, simd_expected, current->thread.user_simd,
				  sizeof(simd_expected));
		KUNIT_EXPECT_EQ(test, simd_valid_before, current->thread.user_simd_valid);
		KUNIT_EXPECT_EQ(test, fpcr_before, current->thread.user_fpcr);
		KUNIT_EXPECT_EQ(test, fpsr_before, current->thread.user_fpsr);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));
	}
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
}

static void plsp_constrained_overlap_is_rejected_by_resume(struct kunit *test)
{
	/* LDP x3, x3, [x4] has a constrained-unpredictable destination overlap. */
	const u32 instruction = 0xa9400c83U;
	struct pt_regs regs = {};
	struct pt_regs before;
	struct tcti_result result;
	u64 simd_before[ARRAY_SIZE(current->thread.user_simd)];
	unsigned long simd_valid_before;
	unsigned long fpcr_before;
	unsigned long fpsr_before;
	unsigned long text;

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNSUPPORTED,
			tcti_decode_aarch64(instruction).decode_class);
	text = plsp_map_instruction(test, instruction);
	plsp_seed_state(&regs, instruction);
	regs.pc = text;
	regs.pstate = PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;
	regs.regs[3] = 0x0123456789abcdefULL;
	regs.regs[4] = 0;
	before = regs;
	memcpy(simd_before, current->thread.user_simd, sizeof(simd_before));
	simd_valid_before = current->thread.user_simd_valid;
	fpcr_before = current->thread.user_fpcr;
	fpsr_before = current->thread.user_fpsr;
	result = tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, TCTI_EXIT_UNSUPPORTED_INSTRUCTION, result.reason);
	KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
	KUNIT_EXPECT_EQ(test, instruction, result.instruction);
	KUNIT_EXPECT_EQ(test, text, result.pc);
	KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	KUNIT_EXPECT_MEMEQ(test, simd_before, current->thread.user_simd,
				  sizeof(simd_before));
	KUNIT_EXPECT_EQ(test, simd_valid_before, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, fpcr_before, current->thread.user_fpcr);
	KUNIT_EXPECT_EQ(test, fpsr_before, current->thread.user_fpsr);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));
}

static void plsp_second_transfer_faults_preserve_register_state(struct kunit *test)
{
	const u32 ldp_pre = 0xa9c08940U; /* ldp x0, x2, [x10, #8]! */
	const u32 stp_pre = 0xa9808940U; /* stp x0, x2, [x10, #8]! */
	unsigned long data = plsp_map_data(test, 2 * PAGE_SIZE);
	unsigned long base = data + PAGE_SIZE - 2 * sizeof(u64);
	unsigned long text;
	struct pt_regs regs = {};
	struct pt_regs before;
	struct tcti_result result;
	u64 simd_before[ARRAY_SIZE(current->thread.user_simd)];
	unsigned long simd_valid_before;
	unsigned long fpcr_before;
	unsigned long fpsr_before;
	u64 initial_first = 0x1122334455667788ULL;
	u64 stored_first = 0x8877665544332211ULL;
	u64 stored_second = 0x0123456789abcdefULL;
	u64 observed = 0;
	int ret;

	ret = sys_mprotect(data + PAGE_SIZE, PAGE_SIZE, PROT_NONE);
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = tcti_write_user_data(current->mm, data + PAGE_SIZE - sizeof(u64),
				  &initial_first, sizeof(initial_first));
	KUNIT_ASSERT_EQ(test, 0, ret);
	text = plsp_map_instruction(test, ldp_pre);
	plsp_seed_state(&regs, ldp_pre);
	regs.pc = text;
	regs.pstate = PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;
	regs.regs[0] = 0xaaaaaaaaaaaaaaaaULL;
	regs.regs[10] = base;
	before = regs;
	memcpy(simd_before, current->thread.user_simd, sizeof(simd_before));
	simd_valid_before = current->thread.user_simd_valid;
	fpcr_before = current->thread.user_fpcr;
	fpsr_before = current->thread.user_fpsr;
	result = tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, TCTI_EXIT_USER_FAULT, result.reason);
	KUNIT_EXPECT_EQ(test, -EFAULT, result.status);
	KUNIT_EXPECT_EQ(test, data + PAGE_SIZE, result.fault_address);
	KUNIT_EXPECT_EQ(test, TCTI_ACCESS_READ, result.fault_access);
	KUNIT_EXPECT_EQ(test, text, result.pc);
	KUNIT_EXPECT_EQ(test, ldp_pre, result.instruction);
	KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	KUNIT_EXPECT_MEMEQ(test, simd_before, current->thread.user_simd,
				  sizeof(simd_before));
	KUNIT_EXPECT_EQ(test, simd_valid_before, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, fpcr_before, current->thread.user_fpcr);
	KUNIT_EXPECT_EQ(test, fpsr_before, current->thread.user_fpsr);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));

	text = plsp_map_instruction(test, stp_pre);
	plsp_seed_state(&regs, stp_pre);
	regs.pc = text;
	regs.pstate = PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;
	regs.regs[0] = stored_first;
	regs.regs[2] = stored_second;
	regs.regs[10] = base;
	before = regs;
	memcpy(simd_before, current->thread.user_simd, sizeof(simd_before));
	simd_valid_before = current->thread.user_simd_valid;
	fpcr_before = current->thread.user_fpcr;
	fpsr_before = current->thread.user_fpsr;
	result = tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, TCTI_EXIT_USER_FAULT, result.reason);
	KUNIT_EXPECT_EQ(test, -EFAULT, result.status);
	KUNIT_EXPECT_EQ(test, data + PAGE_SIZE, result.fault_address);
	KUNIT_EXPECT_EQ(test, TCTI_ACCESS_WRITE, result.fault_access);
	KUNIT_EXPECT_EQ(test, text, result.pc);
	KUNIT_EXPECT_EQ(test, stp_pre, result.instruction);
	KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	KUNIT_EXPECT_MEMEQ(test, simd_before, current->thread.user_simd,
				  sizeof(simd_before));
	KUNIT_EXPECT_EQ(test, simd_valid_before, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, fpcr_before, current->thread.user_fpcr);
	KUNIT_EXPECT_EQ(test, fpsr_before, current->thread.user_fpsr);
	ret = tcti_read_user_data(current->mm, data + PAGE_SIZE - sizeof(u64),
				  &observed, sizeof(observed));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, stored_first, observed);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, 2 * PAGE_SIZE));
}

static struct kunit_case plsp_cases[] = {
	KUNIT_CASE(plsp_every_source_leaf_executes_through_resume),
	KUNIT_CASE(plsp_constrained_overlap_is_rejected_by_resume),
	KUNIT_CASE(plsp_second_transfer_faults_preserve_register_state),
	{}
};

static struct kunit_suite tcti_pair_load_store_source_bound_suite = {
	.name = "orlix-tcti-pair-load-store-source-bound",
	.init = plsp_test_init,
	.exit = plsp_test_exit,
	.test_cases = plsp_cases,
};

kunit_test_suite(tcti_pair_load_store_source_bound_suite);
