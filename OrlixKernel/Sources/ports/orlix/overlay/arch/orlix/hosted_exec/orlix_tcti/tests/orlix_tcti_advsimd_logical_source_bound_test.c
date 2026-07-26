// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/syscalls.h>

#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>

#include "../decode_aarch64.h"
#include "target_instruction_artifact.h"

#define ORLIX_TCTI_ADVSIMD_LOGICAL_SVC	0xd4000001U
#define ORLIX_TCTI_ADVSIMD_LOGICAL_VARIABLE_MASK	0x401f03ffU

struct orlix_tcti_advsimd_logical_leaf {
	u16 source_ordinal;
	const char *source_name;
	const char *source_mnemonic;
	const char *source_operation;
	u32 source_mask;
	u32 source_pattern;
	enum orlix_tcti_logical_op operation;
};

static const struct orlix_tcti_advsimd_logical_leaf orlix_tcti_advsimd_logical_leaves[] = {
	{ 3925U, "AND_asimdsame_only", "AND", "AND_advsimd",
	  0xbfe0fc00U, 0x0e201c00U, ORLIX_TCTI_LOGICAL_AND },
	{ 3927U, "BIC_asimdsame_only", "BIC", "BIC_advsimd_reg",
	  0xbfe0fc00U, 0x0e601c00U, ORLIX_TCTI_LOGICAL_BIC },
	{ 3934U, "ORR_asimdsame_only", "ORR", "ORR_advsimd_reg",
	  0xbfe0fc00U, 0x0ea01c00U, ORLIX_TCTI_LOGICAL_ORR },
	{ 3936U, "ORN_asimdsame_only", "ORN", "ORN_advsimd",
	  0xbfe0fc00U, 0x0ee01c00U, ORLIX_TCTI_LOGICAL_ORN },
	{ 3966U, "EOR_asimdsame_only", "EOR", "EOR_advsimd",
	  0xbfe0fc00U, 0x2e201c00U, ORLIX_TCTI_LOGICAL_EOR },
	{ 3968U, "BSL_asimdsame_only", "BSL", "BSL_advsimd",
	  0xbfe0fc00U, 0x2e601c00U, ORLIX_TCTI_LOGICAL_BSL },
	{ 3976U, "BIT_asimdsame_only", "BIT", "BIT_advsimd",
	  0xbfe0fc00U, 0x2ea01c00U, ORLIX_TCTI_LOGICAL_BIT },
	{ 3978U, "BIF_asimdsame_only", "BIF", "BIF_advsimd",
	  0xbfe0fc00U, 0x2ee01c00U, ORLIX_TCTI_LOGICAL_BIF },
};

struct orlix_tcti_advsimd_logical_context {
	struct mm_struct *mm;
	unsigned long instructions;
	u64 simd[ARRAY_SIZE(current->thread.user_simd)];
	unsigned long simd_valid;
	unsigned long fpcr;
	unsigned long fpsr;
};

struct orlix_tcti_advsimd_logical_registers {
	u8 rd;
	u8 rn;
	u8 rm;
};

static const struct orlix_tcti_advsimd_logical_registers
orlix_tcti_advsimd_logical_overlap_cases[] = {
	{ 3U, 4U, 5U },
	{ 6U, 6U, 7U },
	{ 8U, 9U, 8U },
	{ 10U, 11U, 11U },
	{ 12U, 12U, 12U },
	{ 31U, 0U, 30U },
};

static const char *orlix_tcti_advsimd_logical_artifact_string(
	const struct orlix_tcti_target_instruction_artifact *artifact, u32 offset)
{
	if (offset >= artifact->string_pool_size)
		return NULL;

	return (const char *)artifact->string_pool + offset;
}

static int orlix_tcti_advsimd_logical_test_init(struct kunit *test)
{
	struct orlix_tcti_advsimd_logical_context *context;

	context = kunit_kzalloc(test, sizeof(*context), GFP_KERNEL);
	if (!context)
		return -ENOMEM;
	context->mm = mm_alloc();
	if (!context->mm)
		return -ENOMEM;
	kthread_use_mm(context->mm);
	context->instructions = ksys_mmap_pgoff(
		0, PAGE_SIZE, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (IS_ERR_VALUE(context->instructions)) {
		kthread_unuse_mm(context->mm);
		mmput(context->mm);
		return (int)context->instructions;
	}
	memcpy(context->simd, current->thread.user_simd,
	       sizeof(context->simd));
	context->simd_valid = current->thread.user_simd_valid;
	context->fpcr = current->thread.user_fpcr;
	context->fpsr = current->thread.user_fpsr;
	test->priv = context;
	return 0;
}

static void orlix_tcti_advsimd_logical_test_exit(struct kunit *test)
{
	struct orlix_tcti_advsimd_logical_context *context = test->priv;

	memcpy(current->thread.user_simd, context->simd,
	       sizeof(context->simd));
	current->thread.user_simd_valid = context->simd_valid;
	current->thread.user_fpcr = context->fpcr;
	current->thread.user_fpsr = context->fpsr;
	vm_munmap(context->instructions, PAGE_SIZE);
	kthread_unuse_mm(context->mm);
	mmput(context->mm);
}

static u32 orlix_tcti_advsimd_logical_instruction(
	const struct orlix_tcti_advsimd_logical_leaf *leaf, u8 q, u8 rd, u8 rn,
	u8 rm)
{
	return leaf->source_pattern | ((u32)q << 30) | ((u32)rm << 16) |
		((u32)rn << 5) | rd;
}

static int orlix_tcti_advsimd_logical_load_instruction(
	struct kunit *test, u32 instruction)
{
	struct orlix_tcti_advsimd_logical_context *context = test->priv;
	const u32 program[] = { instruction, ORLIX_TCTI_ADVSIMD_LOGICAL_SVC };
	int ret;

	ret = sys_mprotect(context->instructions, PAGE_SIZE,
			   PROT_READ | PROT_WRITE);
	if (ret)
		return ret;
	ret = orlix_tcti_write_user_data(current->mm, context->instructions, program,
				   sizeof(program));
	if (ret)
		return ret;
	return sys_mprotect(context->instructions, PAGE_SIZE,
			    PROT_READ | PROT_EXEC);
}

static void orlix_tcti_advsimd_logical_seed_regs(struct pt_regs *regs,
					   unsigned long pc, u32 instruction)
{
	unsigned int index;

	memset(regs, 0, sizeof(*regs));
	for (index = 0; index < ARRAY_SIZE(regs->regs); index++)
		regs->regs[index] = 0x9e3779b97f4a7c15ULL ^
			((u64)instruction << (index & 15U)) ^ index;
	regs->sp = 0x00000001fffffff0ULL;
	regs->pc = pc;
	regs->pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT |
		       PSR_V_BIT | PSR_D_BIT;
	regs->orig_x0 = 0x13579bdf2468ace0ULL;
	regs->syscallno = NO_SYSCALL;
	regs->unused = 0x76543210U;
}

static u64 orlix_tcti_advsimd_logical_word_expected(enum orlix_tcti_logical_op operation,
					      u64 destination, u64 left,
					      u64 right)
{
	switch (operation) {
	case ORLIX_TCTI_LOGICAL_AND:
		return left & right;
	case ORLIX_TCTI_LOGICAL_BIC:
		return left & ~right;
	case ORLIX_TCTI_LOGICAL_ORR:
		return left | right;
	case ORLIX_TCTI_LOGICAL_ORN:
		return left | ~right;
	case ORLIX_TCTI_LOGICAL_EOR:
		return left ^ right;
	case ORLIX_TCTI_LOGICAL_BSL:
		return (left & destination) | (right & ~destination);
	case ORLIX_TCTI_LOGICAL_BIT:
		return (destination & ~right) | (left & right);
	case ORLIX_TCTI_LOGICAL_BIF:
		return (destination & right) | (left & ~right);
	}

	return 0;
}

static void orlix_tcti_advsimd_logical_source_bindings(struct kunit *test)
{
	const struct orlix_tcti_target_instruction_artifact *artifact =
		orlix_tcti_target_instruction_artifact_canonical();
	struct orlix_tcti_target_instruction_artifact_validation_result validation;
	size_t index;

	KUNIT_ASSERT_NOT_NULL(test, artifact);
	KUNIT_ASSERT_EQ(test, 0,
			orlix_tcti_target_instruction_artifact_validate(artifact,
							  &validation));
	KUNIT_ASSERT_GT(test, artifact->leaf_count, 3978U);
	KUNIT_ASSERT_EQ(test, 8U, ARRAY_SIZE(orlix_tcti_advsimd_logical_leaves));
	for (index = 0; index < ARRAY_SIZE(orlix_tcti_advsimd_logical_leaves);
	     index++) {
		const struct orlix_tcti_advsimd_logical_leaf *leaf =
			&orlix_tcti_advsimd_logical_leaves[index];
		const struct orlix_tcti_target_instruction_artifact_leaf *source =
			&artifact->leaves[leaf->source_ordinal];
		const char *name = orlix_tcti_advsimd_logical_artifact_string(
			artifact, source->name_offset);
		const char *mnemonic = orlix_tcti_advsimd_logical_artifact_string(
			artifact, source->mnemonic_offset);
		const char *operation = orlix_tcti_advsimd_logical_artifact_string(
			artifact, source->operation_offset);

		KUNIT_ASSERT_NOT_NULL(test, name);
		KUNIT_ASSERT_NOT_NULL(test, mnemonic);
		KUNIT_ASSERT_NOT_NULL(test, operation);
		KUNIT_EXPECT_STREQ(test, leaf->source_name, name);
		KUNIT_EXPECT_STREQ(test, leaf->source_mnemonic, mnemonic);
		KUNIT_EXPECT_STREQ(test, leaf->source_operation, operation);
		KUNIT_EXPECT_EQ(test, leaf->source_mask, source->encoding_mask);
		KUNIT_EXPECT_EQ(test, leaf->source_pattern,
				source->encoding_pattern);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ADVSIMD_LOGICAL_VARIABLE_MASK,
				~source->encoding_mask);
	}
}

static void orlix_tcti_advsimd_logical_all_legal_fields_decode(struct kunit *test)
{
	size_t leaf_index;
	u8 q;
	u8 reg;

	for (leaf_index = 0;
	     leaf_index < ARRAY_SIZE(orlix_tcti_advsimd_logical_leaves);
	     leaf_index++) {
		const struct orlix_tcti_advsimd_logical_leaf *leaf =
			&orlix_tcti_advsimd_logical_leaves[leaf_index];

		for (q = 0; q < 2; q++) {
			for (reg = 0; reg < 32; reg++) {
				struct orlix_tcti_decoded_instruction decoded =
					orlix_tcti_decode_aarch64(
						orlix_tcti_advsimd_logical_instruction(
							leaf, q, reg,
							(reg + 1U) & 31U,
							(reg + 2U) & 31U));

				KUNIT_EXPECT_EQ(test,
						ORLIX_TCTI_DECODE_SIMD_VECTOR_LOGICAL,
						decoded.decode_class);
				KUNIT_EXPECT_EQ(test, leaf->operation,
						decoded.logical_op);
				KUNIT_EXPECT_EQ(test, reg, decoded.rd);
				KUNIT_EXPECT_EQ(test, (reg + 1U) & 31U,
						decoded.rn);
				KUNIT_EXPECT_EQ(test, (reg + 2U) & 31U,
						decoded.rm);
				KUNIT_EXPECT_EQ(test,
						q ? 2 * sizeof(u64) : sizeof(u64),
						decoded.access_size);
				KUNIT_EXPECT_EQ(test, decoded.access_size,
						decoded.result_size);
			}
		}
	}
}

static void orlix_tcti_advsimd_logical_source_leaves_resume(struct kunit *test)
{
	struct orlix_tcti_advsimd_logical_context *context = test->priv;
	size_t leaf_index;
	u8 q;
	size_t overlap_index;

	for (leaf_index = 0;
	     leaf_index < ARRAY_SIZE(orlix_tcti_advsimd_logical_leaves);
	     leaf_index++) {
		const struct orlix_tcti_advsimd_logical_leaf *leaf =
			&orlix_tcti_advsimd_logical_leaves[leaf_index];

		for (q = 0; q < 2; q++) {
			for (overlap_index = 0;
			     overlap_index <
				ARRAY_SIZE(orlix_tcti_advsimd_logical_overlap_cases);
			     overlap_index++) {
				const struct orlix_tcti_advsimd_logical_registers *registers =
					&orlix_tcti_advsimd_logical_overlap_cases[
						overlap_index];
				u32 instruction = orlix_tcti_advsimd_logical_instruction(
					leaf, q, registers->rd, registers->rn,
					registers->rm);
				const u32 expected_program[] = {
					instruction, ORLIX_TCTI_ADVSIMD_LOGICAL_SVC,
				};
				u32 before_program[ARRAY_SIZE(expected_program)];
				u32 after_program[ARRAY_SIZE(expected_program)];
				u64 expected_simd[
					ARRAY_SIZE(current->thread.user_simd)];
				struct pt_regs regs;
				struct pt_regs expected_regs;
				struct orlix_tcti_result result;
				unsigned int simd_index;
				unsigned int word;
				int ret;

				ret = orlix_tcti_advsimd_logical_load_instruction(test,
								       instruction);
				KUNIT_ASSERT_EQ(test, 0, ret);
				ret = orlix_tcti_read_user_data(current->mm,
					context->instructions, before_program,
					sizeof(before_program));
				KUNIT_ASSERT_EQ(test, 0, ret);
				KUNIT_EXPECT_MEMEQ(test, expected_program,
						   before_program,
						   sizeof(expected_program));

				for (simd_index = 0;
				     simd_index <
					ARRAY_SIZE(current->thread.user_simd);
				     simd_index++)
					current->thread.user_simd[simd_index] =
						0x6a09e667f3bcc909ULL ^
						((u64)(simd_index + 1U) *
						 0x0102040810204081ULL);
				memcpy(expected_simd, current->thread.user_simd,
				       sizeof(expected_simd));
				for (word = 0; word < (q ? 2U : 1U); word++) {
					u64 destination = expected_simd[
						registers->rd * 2U + word];
					u64 left = expected_simd[
						registers->rn * 2U + word];
					u64 right = expected_simd[
						registers->rm * 2U + word];

					expected_simd[registers->rd * 2U + word] =
						orlix_tcti_advsimd_logical_word_expected(
							leaf->operation, destination,
							left, right);
				}
				if (!q)
					expected_simd[registers->rd * 2U + 1U] = 0;

				current->thread.user_simd_valid = 0;
				current->thread.user_fpcr = BIT(22) | BIT(24);
				current->thread.user_fpsr = BIT(27) | BIT(4);
				orlix_tcti_advsimd_logical_seed_regs(
					&regs, context->instructions, instruction);
				expected_regs = regs;
				expected_regs.pc += sizeof(u32);

				result = orlix_tcti_resume_user(current, &regs,
							  current->mm);

				KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL,
						    result.reason,
						    "%s q=%u overlap=%zu",
						    leaf->source_name, q,
						    overlap_index);
				KUNIT_EXPECT_EQ(test, 0L, result.status);
				KUNIT_EXPECT_EQ(test, 0UL, result.fault_address);
				KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_FETCH,
						result.fault_access);
				KUNIT_EXPECT_EQ(test,
						context->instructions + sizeof(u32),
						result.pc);
				KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ADVSIMD_LOGICAL_SVC,
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

				ret = orlix_tcti_read_user_data(current->mm,
					context->instructions, after_program,
					sizeof(after_program));
				KUNIT_ASSERT_EQ(test, 0, ret);
				KUNIT_EXPECT_MEMEQ(test, expected_program,
						   after_program,
						   sizeof(expected_program));
			}
		}
	}
}

static struct kunit_case orlix_tcti_advsimd_logical_source_bound_cases[] = {
	KUNIT_CASE(orlix_tcti_advsimd_logical_source_bindings),
	KUNIT_CASE(orlix_tcti_advsimd_logical_all_legal_fields_decode),
	KUNIT_CASE(orlix_tcti_advsimd_logical_source_leaves_resume),
	{}
};

static struct kunit_suite orlix_tcti_advsimd_logical_source_bound_suite = {
	.name = "orlix-tcti-advsimd-logical-source-bound",
	.init = orlix_tcti_advsimd_logical_test_init,
	.exit = orlix_tcti_advsimd_logical_test_exit,
	.test_cases = orlix_tcti_advsimd_logical_source_bound_cases,
};

kunit_test_suite(orlix_tcti_advsimd_logical_source_bound_suite);

MODULE_LICENSE("GPL");
