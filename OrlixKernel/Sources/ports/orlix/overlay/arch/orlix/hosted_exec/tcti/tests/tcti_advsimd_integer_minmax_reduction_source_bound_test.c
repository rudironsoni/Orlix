// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/string.h>
#include <linux/syscalls.h>

#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/tcti.h>

#include "../decode_aarch64.h"
#include "target_instruction_artifact.h"

#define TCTI_MINMAXV_SVC 0xd4000001U
#define TCTI_MINMAXV_VARIABLE_MASK 0x40c003ffU

struct tcti_minmaxv_leaf {
	u16 source_ordinal;
	const char *source_name;
	const char *source_mnemonic;
	const char *source_operation;
	u32 source_mask;
	u32 source_pattern;
	enum tcti_simd_reduction_op operation;
};

static const struct tcti_minmaxv_leaf tcti_minmaxv_leaves[] = {
	{ 3855U, "SMAXV_asimdall_only", "SMAXV", "SMAXV_advsimd", 0xbf3ffc00U,
	  0x0e30a800U, TCTI_SIMD_REDUCTION_SMAXV },
	{ 3856U, "SMINV_asimdall_only", "SMINV", "SMINV_advsimd", 0xbf3ffc00U,
	  0x0e31a800U, TCTI_SIMD_REDUCTION_SMINV },
	{ 3863U, "UMAXV_asimdall_only", "UMAXV", "UMAXV_advsimd", 0xbf3ffc00U,
	  0x2e30a800U, TCTI_SIMD_REDUCTION_UMAXV },
	{ 3864U, "UMINV_asimdall_only", "UMINV", "UMINV_advsimd", 0xbf3ffc00U,
	  0x2e31a800U, TCTI_SIMD_REDUCTION_UMINV },
};

struct tcti_minmaxv_context {
	struct mm_struct *mm;
	unsigned long instructions;
	u64 simd[ARRAY_SIZE(current->thread.user_simd)];
	unsigned long simd_valid;
	unsigned long fpcr;
	unsigned long fpsr;
};

struct tcti_minmaxv_arrangement {
	u8 q;
	u8 size;
};

static const struct tcti_minmaxv_arrangement tcti_minmaxv_legal[] = {
	{ 0U, 0U }, { 1U, 0U }, { 0U, 1U }, { 1U, 1U }, { 1U, 2U },
};

static const struct tcti_minmaxv_arrangement tcti_minmaxv_reserved[] = {
	{ 0U, 2U },
	{ 0U, 3U },
	{ 1U, 3U },
};

static const char *
tcti_minmaxv_artifact_string(const struct tcti_target_instruction_artifact
			     *artifact, u32 offset)
{
	if (offset >= artifact->string_pool_size)
		return NULL;
	return (const char *)artifact->string_pool + offset;
}

static int tcti_minmaxv_test_init(struct kunit *test)
{
	struct tcti_minmaxv_context *context;

	context = kunit_kzalloc(test, sizeof(*context), GFP_KERNEL);
	if (!context)
		return -ENOMEM;
	context->mm = mm_alloc();
	if (!context->mm)
		return -ENOMEM;
	kthread_use_mm(context->mm);
	context->instructions =
		ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (IS_ERR_VALUE(context->instructions)) {
		kthread_unuse_mm(context->mm);
		mmput(context->mm);
		return (int)context->instructions;
	}
	memcpy(context->simd, current->thread.user_simd, sizeof(context->simd));
	context->simd_valid = current->thread.user_simd_valid;
	context->fpcr = current->thread.user_fpcr;
	context->fpsr = current->thread.user_fpsr;
	test->priv = context;
	return 0;
}

static void tcti_minmaxv_test_exit(struct kunit *test)
{
	struct tcti_minmaxv_context *context = test->priv;

	memcpy(current->thread.user_simd, context->simd, sizeof(context->simd));
	current->thread.user_simd_valid = context->simd_valid;
	current->thread.user_fpcr = context->fpcr;
	current->thread.user_fpsr = context->fpsr;
	vm_munmap(context->instructions, PAGE_SIZE);
	kthread_unuse_mm(context->mm);
	mmput(context->mm);
}

static u32 tcti_minmaxv_instruction(const struct tcti_minmaxv_leaf *leaf, u8 q,
				    u8 size, u8 rd, u8 rn)
{
	return leaf->source_pattern | ((u32)q << 30) | ((u32)size << 22) |
	       ((u32)rn << 5) | rd;
}

static int tcti_minmaxv_load_instruction(struct kunit *test, u32 instruction)
{
	struct tcti_minmaxv_context *context = test->priv;
	const u32 program[] = { instruction, TCTI_MINMAXV_SVC };
	int ret;

	ret = sys_mprotect(context->instructions, PAGE_SIZE,
			   PROT_READ | PROT_WRITE);
	if (ret)
		return ret;
	ret = tcti_write_user_data(current->mm, context->instructions, program,
				   sizeof(program));
	if (ret)
		return ret;
	return sys_mprotect(context->instructions, PAGE_SIZE,
			    PROT_READ | PROT_EXEC);
}

static void tcti_minmaxv_seed_regs(struct pt_regs *regs, unsigned long pc,
				   u32 instruction)
{
	unsigned int index;

	memset(regs, 0, sizeof(*regs));
	for (index = 0; index < ARRAY_SIZE(regs->regs); index++)
		regs->regs[index] = 0xd1b54a32d192ed03ULL ^
				    ((u64)instruction << (index & 15U)) ^ index;
	regs->sp = 0x00000001ffffffe0ULL;
	regs->pc = pc;
	regs->pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT |
		       PSR_V_BIT | PSR_D_BIT;
	regs->orig_x0 = 0x0f1e2d3c4b5a6978ULL;
	regs->syscallno = NO_SYSCALL;
	regs->unused = 0xa55a3cc3U;
}

static u32 minmaxv_oracle(enum tcti_simd_reduction_op operation, u8 q,
			  u8 size, const u64 source[2])
{
	u8 lane_bytes = 1U << size;
	u8 lane_bits = lane_bytes * 8U;
	u8 lane_count = (q ? 2U * sizeof(u64) : sizeof(u64)) / lane_bytes;
	u64 lane_mask = GENMASK_ULL(lane_bits - 1U, 0);
	u64 selected = 0;
	s64 selected_signed = 0;
	bool signed_compare = operation == TCTI_SIMD_REDUCTION_SMAXV ||
			      operation == TCTI_SIMD_REDUCTION_SMINV;
	bool minimum = operation == TCTI_SIMD_REDUCTION_SMINV ||
		       operation == TCTI_SIMD_REDUCTION_UMINV;
	u8 lane;

	for (lane = 0; lane < lane_count; lane++) {
		u8 byte_offset = lane * lane_bytes;
		u8 word = byte_offset / sizeof(u64);
		u8 shift = (byte_offset % sizeof(u64)) * 8U;
		u64 value = (source[word] >> shift) & lane_mask;

		if (signed_compare) {
			s64 signed_value = sign_extend64(value, lane_bits - 1U);

			if (!lane ||
			    (minimum && signed_value < selected_signed) ||
			    (!minimum && signed_value > selected_signed)) {
				selected = value;
				selected_signed = signed_value;
			}
		} else if (!lane || (minimum && value < selected) ||
			   (!minimum && value > selected)) {
			selected = value;
		}
	}

	return (u32)selected;
}

static void tcti_minmaxv_source_bindings(struct kunit *test)
{
	const struct tcti_target_instruction_artifact *artifact =
		tcti_target_instruction_artifact_canonical();
	struct tcti_target_instruction_artifact_validation_result validation;
	size_t index;

	KUNIT_ASSERT_NOT_NULL(test, artifact);
	KUNIT_ASSERT_EQ(test, 0,
			tcti_target_instruction_artifact_validate(artifact,
								  &validation));
	KUNIT_ASSERT_GT(test, artifact->leaf_count, 3864U);
	KUNIT_ASSERT_EQ(test, 4U, ARRAY_SIZE(tcti_minmaxv_leaves));
	for (index = 0; index < ARRAY_SIZE(tcti_minmaxv_leaves); index++) {
		const struct tcti_minmaxv_leaf *leaf =
			&tcti_minmaxv_leaves[index];
		const struct tcti_target_instruction_artifact_leaf *source =
			&artifact->leaves[leaf->source_ordinal];
		const char *name = tcti_minmaxv_artifact_string(artifact,
							 source->name_offset);
		const char *mnemonic = tcti_minmaxv_artifact_string(artifact,
							     source->mnemonic_offset);
		const char *operation = tcti_minmaxv_artifact_string(artifact,
							      source->operation_offset);

		KUNIT_ASSERT_NOT_NULL(test, name);
		KUNIT_ASSERT_NOT_NULL(test, mnemonic);
		KUNIT_ASSERT_NOT_NULL(test, operation);
		KUNIT_EXPECT_STREQ(test, leaf->source_name, name);
		KUNIT_EXPECT_STREQ(test, leaf->source_mnemonic, mnemonic);
		KUNIT_EXPECT_STREQ(test, leaf->source_operation, operation);
		KUNIT_EXPECT_EQ(test, leaf->source_mask, source->encoding_mask);
		KUNIT_EXPECT_EQ(test, leaf->source_pattern,
				source->encoding_pattern);
		KUNIT_EXPECT_EQ(test, TCTI_MINMAXV_VARIABLE_MASK,
				~source->encoding_mask);
	}
}

static int tcti_minmaxv_read_program(struct kunit *test, u32 program[2])
{
	struct tcti_minmaxv_context *context = test->priv;

	return tcti_read_user_data(current->mm, context->instructions, program,
				   2U * sizeof(u32));
}

static void
expect_decode(struct kunit *test, const struct tcti_minmaxv_leaf *leaf,
	      u8 q, u8 size, u8 rd, u8 rn)
{
	u32 instruction = tcti_minmaxv_instruction(leaf, q, size, rd, rn);
	struct tcti_decoded_instruction decoded =
		tcti_decode_aarch64(instruction);
	bool legal = size < 2 || (size == 2 && q);

	if (!legal) {
		KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNSUPPORTED,
				decoded.decode_class);
		return;
	}
	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_REDUCTION,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, leaf->operation, decoded.simd_reduction_op);
	KUNIT_EXPECT_EQ(test, rd, decoded.rd);
	KUNIT_EXPECT_EQ(test, rn, decoded.rn);
	KUNIT_EXPECT_EQ(test, 1U << size, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 1U << size, decoded.result_size);
	KUNIT_EXPECT_EQ(test, !!q, decoded.simd_q);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
}

static void tcti_minmaxv_complete_encoding_domain(struct kunit *test)
{
	size_t leaf_index;
	u8 q;
	u8 size;
	u8 rd;
	u8 rn;

	for (leaf_index = 0; leaf_index < ARRAY_SIZE(tcti_minmaxv_leaves);
	     leaf_index++) {
		const struct tcti_minmaxv_leaf *leaf =
			&tcti_minmaxv_leaves[leaf_index];

		for (q = 0; q < 2; q++) {
			for (size = 0; size < 4; size++) {
				for (rd = 0; rd < 32; rd++)
					for (rn = 0; rn < 32; rn++)
						expect_decode(test, leaf, q, size, rd, rn);
			}
		}
	}
}

static void tcti_minmaxv_seed_simd(u8 rn)
{
	unsigned int index;

	for (index = 0; index < ARRAY_SIZE(current->thread.user_simd); index++)
		current->thread.user_simd[index] =
			0x6a09e667f3bcc909ULL ^
			((u64)(index + 1U) * 0x0102040810204081ULL);
	current->thread.user_simd[rn * 2U] = 0xc040028201fe7e81ULL;
	current->thread.user_simd[rn * 2U + 1U] = 0xaa55e02000ff7f80ULL;
}

static void tcti_minmaxv_legal_arrangements_resume(struct kunit *test)
{
	static const struct {
		u8 rd;
		u8 rn;
	} registers[] = {
		{ 3U, 4U },
		{ 9U, 9U },
		{ 31U, 0U },
		{ 0U, 31U },
		{ 31U, 31U },
	};
	struct tcti_minmaxv_context *context = test->priv;
	size_t leaf_index;
	size_t arrangement_index;
	size_t register_index;

	for (leaf_index = 0; leaf_index < ARRAY_SIZE(tcti_minmaxv_leaves);
	     leaf_index++) {
		const struct tcti_minmaxv_leaf *leaf =
			&tcti_minmaxv_leaves[leaf_index];

		for (arrangement_index = 0;
		     arrangement_index < ARRAY_SIZE(tcti_minmaxv_legal);
		     arrangement_index++) {
			const struct tcti_minmaxv_arrangement *arrangement =
				&tcti_minmaxv_legal[arrangement_index];

			for (register_index = 0;
			     register_index < ARRAY_SIZE(registers);
			     register_index++) {
				u8 rd = registers[register_index].rd;
				u8 rn = registers[register_index].rn;
				u8 q = arrangement->q;
				u8 size = arrangement->size;
				enum tcti_simd_reduction_op operation = leaf->operation;
				u32 instruction = tcti_minmaxv_instruction(leaf,
					q, size, rd, rn);
				const u32 program[] = {
					instruction,
					TCTI_MINMAXV_SVC,
				};
				u32 before_program[ARRAY_SIZE(program)];
				u32 after_program[ARRAY_SIZE(program)];
				u64 expected_simd[ARRAY_SIZE(current->thread.user_simd)];
				struct pt_regs regs;
				struct pt_regs expected_regs;
				struct tcti_result result;
				u64 source[2];
				u32 expected;
				int ret;

				ret = tcti_minmaxv_load_instruction(test, instruction);
				KUNIT_ASSERT_EQ(test, 0, ret);
				ret = tcti_minmaxv_read_program(test, before_program);
				KUNIT_ASSERT_EQ(test, 0, ret);
				KUNIT_EXPECT_MEMEQ(test, program,
						   before_program,
						   sizeof(program));

				tcti_minmaxv_seed_simd(rn);
				source[0] = current->thread.user_simd[rn * 2U];
				source[1] =
					current->thread.user_simd[rn * 2U + 1U];
				memcpy(expected_simd, current->thread.user_simd,
				       sizeof(expected_simd));
				expected = minmaxv_oracle(operation, q, size, source);
				expected_simd[rd * 2U] = expected;
				expected_simd[rd * 2U + 1U] = 0;
				current->thread.user_simd_valid = 0;
				current->thread.user_fpcr = BIT(22) | BIT(24);
				current->thread.user_fpsr = BIT(27) | BIT(4);
				tcti_minmaxv_seed_regs(&regs,
						       context->instructions,
						       instruction);
				expected_regs = regs;
				expected_regs.pc += sizeof(u32);

				result = tcti_resume_user(current, &regs,
							  current->mm);

				KUNIT_EXPECT_EQ(test, TCTI_EXIT_SYSCALL,
						result.reason);
				KUNIT_EXPECT_EQ(test, 0L, result.status);
				KUNIT_EXPECT_EQ(test, 0UL,
						result.fault_address);
				KUNIT_EXPECT_EQ(test, TCTI_ACCESS_FETCH,
						result.fault_access);
				KUNIT_EXPECT_EQ(test,
						context->instructions +
							sizeof(u32),
						result.pc);
				KUNIT_EXPECT_EQ(test, TCTI_MINMAXV_SVC,
						result.instruction);
				KUNIT_EXPECT_MEMEQ(test, expected_simd,
						   current->thread.user_simd,
						   sizeof(expected_simd));
				KUNIT_EXPECT_EQ(test, 1UL,
						current->thread.user_simd_valid);
				KUNIT_EXPECT_EQ(test, BIT(22) | BIT(24),
						current->thread.user_fpcr);
				KUNIT_EXPECT_EQ(test, BIT(27) | BIT(4),
						current->thread.user_fpsr);
				KUNIT_EXPECT_MEMEQ(test, &expected_regs, &regs,
						   sizeof(expected_regs));

				ret = tcti_minmaxv_read_program(test, after_program);
				KUNIT_ASSERT_EQ(test, 0, ret);
				KUNIT_EXPECT_MEMEQ(test, program, after_program,
						   sizeof(program));
			}
		}
	}
}

static void tcti_minmaxv_reserved_arrangements_resume(struct kunit *test)
{
	struct tcti_minmaxv_context *context = test->priv;
	size_t leaf_index;
	size_t arrangement_index;

	for (leaf_index = 0; leaf_index < ARRAY_SIZE(tcti_minmaxv_leaves);
	     leaf_index++) {
		const struct tcti_minmaxv_leaf *leaf =
			&tcti_minmaxv_leaves[leaf_index];

		for (arrangement_index = 0;
		     arrangement_index < ARRAY_SIZE(tcti_minmaxv_reserved);
		     arrangement_index++) {
			const struct tcti_minmaxv_arrangement *arrangement =
				&tcti_minmaxv_reserved[arrangement_index];
			u32 instruction = tcti_minmaxv_instruction(leaf,
				arrangement->q, arrangement->size, 9U,
				9U);
			const u32 program[] = {
				instruction,
				TCTI_MINMAXV_SVC,
			};
			u32 before_program[ARRAY_SIZE(program)];
			u32 after_program[ARRAY_SIZE(program)];
			u64 before_simd[ARRAY_SIZE(current->thread.user_simd)];
			struct pt_regs regs;
			struct pt_regs before_regs;
			struct tcti_result result;
			unsigned long before_valid;
			unsigned long before_fpcr;
			unsigned long before_fpsr;
			int ret;

			ret = tcti_minmaxv_load_instruction(test, instruction);
			KUNIT_ASSERT_EQ(test, 0, ret);
			ret = tcti_minmaxv_read_program(test, before_program);
			KUNIT_ASSERT_EQ(test, 0, ret);
			KUNIT_EXPECT_MEMEQ(test, program, before_program,
					   sizeof(program));

			tcti_minmaxv_seed_simd(9U);
			memcpy(before_simd, current->thread.user_simd,
			       sizeof(before_simd));
			current->thread.user_simd_valid = 1;
			current->thread.user_fpcr = BIT(22) | BIT(24);
			current->thread.user_fpsr = BIT(27) | BIT(4);
			before_valid = current->thread.user_simd_valid;
			before_fpcr = current->thread.user_fpcr;
			before_fpsr = current->thread.user_fpsr;
			tcti_minmaxv_seed_regs(&regs, context->instructions,
					       instruction);
			before_regs = regs;

			result = tcti_resume_user(current, &regs, current->mm);

			KUNIT_EXPECT_EQ_MSG(test,
					    TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
					    result.reason, "%s q=%u size=%u",
					    leaf->source_name, arrangement->q,
					    arrangement->size);
			KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
			KUNIT_EXPECT_EQ(test, 0UL, result.fault_address);
			KUNIT_EXPECT_EQ(test, TCTI_ACCESS_FETCH,
					result.fault_access);
			KUNIT_EXPECT_EQ(test, context->instructions, result.pc);
			KUNIT_EXPECT_EQ(test, instruction, result.instruction);
			KUNIT_EXPECT_MEMEQ(test, before_simd,
					   current->thread.user_simd,
					   sizeof(before_simd));
			KUNIT_EXPECT_EQ(test, before_valid,
					current->thread.user_simd_valid);
			KUNIT_EXPECT_EQ(test, before_fpcr,
					current->thread.user_fpcr);
			KUNIT_EXPECT_EQ(test, before_fpsr,
					current->thread.user_fpsr);
			KUNIT_EXPECT_MEMEQ(test, &before_regs, &regs,
					   sizeof(before_regs));

			ret = tcti_minmaxv_read_program(test, after_program);
			KUNIT_ASSERT_EQ(test, 0, ret);
			KUNIT_EXPECT_MEMEQ(test, program, after_program,
					   sizeof(program));
		}
	}
}

static struct kunit_case tcti_minmaxv_cases[] = {
	KUNIT_CASE(tcti_minmaxv_source_bindings),
	KUNIT_CASE(tcti_minmaxv_complete_encoding_domain),
	KUNIT_CASE(tcti_minmaxv_legal_arrangements_resume),
	KUNIT_CASE(tcti_minmaxv_reserved_arrangements_resume),
	{}
};

static struct kunit_suite tcti_minmaxv_suite = {
	.name = "orlix-tcti-advsimd-integer-minmax-reduction-source-bound",
	.init = tcti_minmaxv_test_init,
	.exit = tcti_minmaxv_test_exit,
	.test_cases = tcti_minmaxv_cases,
};

kunit_test_suite(tcti_minmaxv_suite);

MODULE_LICENSE("GPL");
