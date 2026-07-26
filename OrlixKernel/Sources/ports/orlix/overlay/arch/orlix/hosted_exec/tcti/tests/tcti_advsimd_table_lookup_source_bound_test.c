// SPDX-License-Identifier: GPL-2.0-only
/*
 * Production-path evidence for the eight classic AdvSIMD TBL/TBX leaves.
 * The byte oracle is deliberately independent from the executor: it reads the
 * complete pre-instruction SIMD register image, including every alias case.
 */
#include <kunit/test.h>
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

#define TCTI_ADVSIMD_TABLE_SVC	0xd4000001U
#define TCTI_ADVSIMD_TABLE_VARIABLE_MASK	0x401f03ffU

struct tcti_advsimd_table_leaf {
	u16 source_ordinal;
	const char *source_name;
	const char *source_mnemonic;
	const char *source_operation;
	u32 source_mask;
	u32 source_pattern;
	u8 table_count;
	enum tcti_simd_table_lookup_op operation;
};

struct tcti_advsimd_table_context {
	struct mm_struct *mm;
	unsigned long instructions;
	unsigned long simd[ARRAY_SIZE(current->thread.user_simd)];
	unsigned long simd_valid;
	unsigned long fpcr;
	unsigned long fpsr;
};

struct tcti_advsimd_table_registers {
	u8 rd;
	u8 rn;
	u8 rm;
};

static const struct tcti_advsimd_table_leaf tcti_advsimd_table_leaves[] = {
	{ 3672U, "TBL_asimdtbl_L1_1", "TBL", "TBL_advsimd",
	  0xbfe0fc00U, 0x0e000000U, 1U, TCTI_SIMD_TABLE_LOOKUP_TBL },
	{ 3673U, "TBX_asimdtbl_L1_1", "TBX", "TBX_advsimd",
	  0xbfe0fc00U, 0x0e001000U, 1U, TCTI_SIMD_TABLE_LOOKUP_TBX },
	{ 3674U, "TBL_asimdtbl_L2_2", "TBL", "TBL_advsimd",
	  0xbfe0fc00U, 0x0e002000U, 2U, TCTI_SIMD_TABLE_LOOKUP_TBL },
	{ 3675U, "TBX_asimdtbl_L2_2", "TBX", "TBX_advsimd",
	  0xbfe0fc00U, 0x0e003000U, 2U, TCTI_SIMD_TABLE_LOOKUP_TBX },
	{ 3676U, "TBL_asimdtbl_L3_3", "TBL", "TBL_advsimd",
	  0xbfe0fc00U, 0x0e004000U, 3U, TCTI_SIMD_TABLE_LOOKUP_TBL },
	{ 3677U, "TBX_asimdtbl_L3_3", "TBX", "TBX_advsimd",
	  0xbfe0fc00U, 0x0e005000U, 3U, TCTI_SIMD_TABLE_LOOKUP_TBX },
	{ 3678U, "TBL_asimdtbl_L4_4", "TBL", "TBL_advsimd",
	  0xbfe0fc00U, 0x0e006000U, 4U, TCTI_SIMD_TABLE_LOOKUP_TBL },
	{ 3679U, "TBX_asimdtbl_L4_4", "TBX", "TBX_advsimd",
	  0xbfe0fc00U, 0x0e007000U, 4U, TCTI_SIMD_TABLE_LOOKUP_TBX },
};

static const struct tcti_advsimd_table_registers
tcti_advsimd_table_overlap_cases[] = {
	{ 4U, 20U, 6U },		/* all distinct */
	{ 20U, 20U, 6U },	/* destination is table base */
	{ 4U, 20U, 4U },		/* destination is index */
	{ 4U, 20U, 20U },	/* index is table base */
	{ 20U, 20U, 20U },	/* all operands alias */
	{ 4U, 31U, 7U },		/* L2-L4 table register wraps V31 to V0 */
};

static const char *tcti_advsimd_table_artifact_string(
	const struct tcti_target_instruction_artifact *artifact, u32 offset)
{
	if (offset >= artifact->string_pool_size)
		return NULL;

	return (const char *)artifact->string_pool + offset;
}

static int tcti_advsimd_table_test_init(struct kunit *test)
{
	struct tcti_advsimd_table_context *context;

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
	memcpy(context->simd, current->thread.user_simd, sizeof(context->simd));
	context->simd_valid = current->thread.user_simd_valid;
	context->fpcr = current->thread.user_fpcr;
	context->fpsr = current->thread.user_fpsr;
	test->priv = context;
	return 0;
}

static void tcti_advsimd_table_test_exit(struct kunit *test)
{
	struct tcti_advsimd_table_context *context = test->priv;

	memcpy(current->thread.user_simd, context->simd, sizeof(context->simd));
	current->thread.user_simd_valid = context->simd_valid;
	current->thread.user_fpcr = context->fpcr;
	current->thread.user_fpsr = context->fpsr;
	vm_munmap(context->instructions, PAGE_SIZE);
	kthread_unuse_mm(context->mm);
	mmput(context->mm);
}

static u32 tcti_advsimd_table_instruction(
	const struct tcti_advsimd_table_leaf *leaf, u8 q, u8 rd, u8 rn, u8 rm)
{
	return leaf->source_pattern | ((u32)q << 30) | ((u32)rm << 16) |
		((u32)rn << 5) | rd;
}

static int tcti_advsimd_table_load_instruction(struct kunit *test,
						 u32 instruction)
{
	struct tcti_advsimd_table_context *context = test->priv;
	const u32 program[] = { instruction, TCTI_ADVSIMD_TABLE_SVC };
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

static void tcti_advsimd_table_seed_regs(struct pt_regs *regs,
					  unsigned long pc, u32 instruction)
{
	unsigned int index;

	memset(regs, 0, sizeof(*regs));
	for (index = 0; index < ARRAY_SIZE(regs->regs); index++)
		regs->regs[index] = 0x9e3779b97f4a7c15ULL ^
			((u64)instruction << (index & 15U)) ^ index;
	regs->sp = STACK_TOP - 16;
	regs->pc = pc;
	regs->pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT |
		PSR_V_BIT | PSR_D_BIT;
	regs->orig_x0 = 0x13579bdf2468ace0ULL;
	regs->syscallno = NO_SYSCALL;
	regs->unused = 0x76543210U;
	regs->regs[8] = __NR_getpid;
}

static u8 tcti_advsimd_table_byte(const unsigned long simd[], u8 reg, u8 lane)
{
	return simd[reg * 2U + lane / sizeof(u64)] >>
		((lane % sizeof(u64)) * 8U);
}

static void tcti_advsimd_table_set_byte(unsigned long simd[], u8 reg,
					u8 lane, u8 value)
{
	unsigned long *word = &simd[reg * 2U + lane / sizeof(u64)];
	u8 shift = (lane % sizeof(u64)) * 8U;

	*word = (*word & ~(0xffUL << shift)) | ((unsigned long)value << shift);
}

static void tcti_advsimd_table_seed_simd(const struct tcti_advsimd_table_leaf *leaf,
					  const struct tcti_advsimd_table_registers *registers)
{
	unsigned int index;
	u8 lane;

	for (index = 0; index < ARRAY_SIZE(current->thread.user_simd); index++)
		current->thread.user_simd[index] = 0x9e3779b97f4a7c15ULL ^
			((u64)(index + 1U) * 0x0102040810204081ULL);
	for (lane = 0; lane < leaf->table_count * 16U; lane++)
		tcti_advsimd_table_set_byte(current->thread.user_simd,
			(registers->rn + lane / 16U) & 31U, lane % 16U,
			0x40U + lane);
	for (lane = 0; lane < 16U; lane++) {
		u8 table_bytes = leaf->table_count * 16U;
		u8 value;

		switch (lane & 3U) {
		case 0:
			value = lane % table_bytes;
			break;
		case 1:
			value = table_bytes - 1U - (lane % table_bytes);
			break;
		case 2:
			value = table_bytes;
			break;
		default:
			value = table_bytes + 7U;
			break;
		}
		tcti_advsimd_table_set_byte(current->thread.user_simd,
			registers->rm, lane, value);
	}
}

static void tcti_advsimd_table_expected(unsigned long expected[],
	const unsigned long before[], const struct tcti_advsimd_table_leaf *leaf,
	const struct tcti_advsimd_table_registers *registers, u8 result_size)
{
	u8 lane;

	memcpy(expected, before,
	       sizeof(unsigned long) * ARRAY_SIZE(current->thread.user_simd));
	expected[registers->rd * 2U] = 0;
	expected[registers->rd * 2U + 1U] = 0;
	for (lane = 0; lane < result_size; lane++) {
		u8 index = tcti_advsimd_table_byte(before, registers->rm, lane);
		u8 value = 0;

		if (index < leaf->table_count * 16U)
			value = tcti_advsimd_table_byte(before,
				(registers->rn + index / 16U) & 31U,
				index % 16U);
		else if (leaf->operation == TCTI_SIMD_TABLE_LOOKUP_TBX)
			value = tcti_advsimd_table_byte(before, registers->rd, lane);
		tcti_advsimd_table_set_byte(expected, registers->rd, lane, value);
	}
}

static void tcti_advsimd_table_source_bindings(struct kunit *test)
{
	const struct tcti_target_instruction_artifact *artifact =
		tcti_target_instruction_artifact_canonical();
	struct tcti_target_instruction_artifact_validation_result validation;
	size_t index;

	KUNIT_ASSERT_NOT_NULL(test, artifact);
	KUNIT_ASSERT_EQ(test, 0,
		tcti_target_instruction_artifact_validate(artifact, &validation));
	KUNIT_ASSERT_GT(test, artifact->leaf_count, 3679U);
	KUNIT_ASSERT_EQ(test, 8U, ARRAY_SIZE(tcti_advsimd_table_leaves));
	for (index = 0; index < ARRAY_SIZE(tcti_advsimd_table_leaves); index++) {
		const struct tcti_advsimd_table_leaf *leaf =
			&tcti_advsimd_table_leaves[index];
		const struct tcti_target_instruction_artifact_leaf *source =
			&artifact->leaves[leaf->source_ordinal];
		const char *name = tcti_advsimd_table_artifact_string(
			artifact, source->name_offset);
		const char *mnemonic = tcti_advsimd_table_artifact_string(
			artifact, source->mnemonic_offset);
		const char *operation = tcti_advsimd_table_artifact_string(
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
		KUNIT_EXPECT_EQ(test, TCTI_ADVSIMD_TABLE_VARIABLE_MASK,
				~source->encoding_mask);
	}
}

static void tcti_advsimd_table_legal_fields_decode(struct kunit *test)
{
	size_t leaf_index;
	u8 q;
	u8 reg;

	for (leaf_index = 0; leaf_index < ARRAY_SIZE(tcti_advsimd_table_leaves);
	     leaf_index++) {
		const struct tcti_advsimd_table_leaf *leaf =
			&tcti_advsimd_table_leaves[leaf_index];

		for (q = 0; q < 2; q++) {
			for (reg = 0; reg < 32U; reg++) {
				const struct tcti_advsimd_table_registers fields[] = {
					{ reg, 30U, 29U },
					{ 3U, reg, 29U },
					{ 3U, 30U, reg },
				};
				size_t field_index;

				for (field_index = 0;
				     field_index < ARRAY_SIZE(fields); field_index++) {
					const struct tcti_advsimd_table_registers *fields_case =
						&fields[field_index];
					struct tcti_decoded_instruction decoded =
						tcti_decode_aarch64(
							tcti_advsimd_table_instruction(
								leaf, q, fields_case->rd,
								fields_case->rn,
								fields_case->rm));

					KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_TABLE_LOOKUP,
							decoded.decode_class);
					KUNIT_EXPECT_EQ(test, leaf->operation,
							decoded.simd_table_lookup_op);
					KUNIT_EXPECT_EQ(test, leaf->table_count,
							decoded.simd_table_count);
					KUNIT_EXPECT_EQ(test, q != 0, decoded.simd_q);
					KUNIT_EXPECT_EQ(test, q ? 16U : 8U,
							decoded.result_size);
					KUNIT_EXPECT_EQ(test, fields_case->rd, decoded.rd);
					KUNIT_EXPECT_EQ(test, fields_case->rn, decoded.rn);
					KUNIT_EXPECT_EQ(test, fields_case->rm, decoded.rm);
				}
			}
		}
	}
}

static void tcti_advsimd_table_source_leaves_resume(struct kunit *test)
{
	struct tcti_advsimd_table_context *context = test->priv;
	unsigned long *before_simd;
	unsigned long *expected_simd;
	size_t leaf_index;
	u8 q;

	before_simd = kunit_kcalloc(test,
		ARRAY_SIZE(current->thread.user_simd), sizeof(*before_simd), GFP_KERNEL);
	expected_simd = kunit_kcalloc(test,
		ARRAY_SIZE(current->thread.user_simd), sizeof(*expected_simd), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, before_simd);
	KUNIT_ASSERT_NOT_NULL(test, expected_simd);

	for (leaf_index = 0; leaf_index < ARRAY_SIZE(tcti_advsimd_table_leaves);
	     leaf_index++) {
		const struct tcti_advsimd_table_leaf *leaf =
			&tcti_advsimd_table_leaves[leaf_index];

		for (q = 0; q < 2; q++) {
			size_t case_index;

			for (case_index = 0;
			     case_index < ARRAY_SIZE(tcti_advsimd_table_overlap_cases);
			     case_index++) {
				const struct tcti_advsimd_table_registers *registers =
					&tcti_advsimd_table_overlap_cases[case_index];
				u8 result_size = q ? 16U : 8U;
				u32 instruction = tcti_advsimd_table_instruction(
					leaf, q, registers->rd, registers->rn,
					registers->rm);
				const u32 expected_program[] = {
					instruction, TCTI_ADVSIMD_TABLE_SVC,
				};
				u32 before_program[ARRAY_SIZE(expected_program)];
				u32 after_program[ARRAY_SIZE(expected_program)];
				struct pt_regs regs;
				struct pt_regs expected_regs;
				struct tcti_result result;
				int ret;

				ret = tcti_advsimd_table_load_instruction(test, instruction);
				KUNIT_ASSERT_EQ(test, 0, ret);
				ret = tcti_read_user_data(current->mm,
					context->instructions, before_program,
					sizeof(before_program));
				KUNIT_ASSERT_EQ(test, 0, ret);
				KUNIT_EXPECT_MEMEQ(test, expected_program, before_program,
						   sizeof(expected_program));

				tcti_advsimd_table_seed_simd(leaf, registers);
				memcpy(before_simd, current->thread.user_simd,
				       sizeof(*before_simd) *
				       ARRAY_SIZE(current->thread.user_simd));
				tcti_advsimd_table_expected(expected_simd, before_simd,
					leaf, registers, result_size);
				current->thread.user_simd_valid = 0;
				current->thread.user_fpcr = BIT(22) | BIT(24);
				current->thread.user_fpsr = BIT(27) | BIT(4);
				tcti_advsimd_table_seed_regs(&regs, context->instructions,
					instruction);
				expected_regs = regs;
				expected_regs.pc += sizeof(u32);

				result = tcti_resume_user(current, &regs, current->mm);

				KUNIT_EXPECT_EQ_MSG(test, TCTI_EXIT_SYSCALL, result.reason,
					"%s q=%u case=%zu", leaf->source_name, q,
					case_index);
				KUNIT_EXPECT_EQ(test, 0L, result.status);
				KUNIT_EXPECT_EQ(test, 0UL, result.fault_address);
				KUNIT_EXPECT_EQ(test, TCTI_ACCESS_FETCH,
						result.fault_access);
				KUNIT_EXPECT_EQ(test, context->instructions + sizeof(u32),
						result.pc);
				KUNIT_EXPECT_EQ(test, TCTI_ADVSIMD_TABLE_SVC,
						result.instruction);
				KUNIT_EXPECT_MEMEQ(test, expected_simd,
					current->thread.user_simd,
					sizeof(*expected_simd) *
					ARRAY_SIZE(current->thread.user_simd));
				KUNIT_EXPECT_EQ(test, 1UL,
					current->thread.user_simd_valid);
				KUNIT_EXPECT_EQ(test, BIT(22) | BIT(24),
					current->thread.user_fpcr);
				KUNIT_EXPECT_EQ(test, BIT(27) | BIT(4),
					current->thread.user_fpsr);
				KUNIT_EXPECT_MEMEQ(test, &expected_regs, &regs,
						sizeof(expected_regs));
				ret = tcti_read_user_data(current->mm,
					context->instructions, after_program,
					sizeof(after_program));
				KUNIT_EXPECT_EQ(test, 0, ret);
				KUNIT_EXPECT_MEMEQ(test, expected_program, after_program,
						   sizeof(expected_program));
			}
		}
	}
}

static struct kunit_case tcti_advsimd_table_source_bound_cases[] = {
	KUNIT_CASE(tcti_advsimd_table_source_bindings),
	KUNIT_CASE(tcti_advsimd_table_legal_fields_decode),
	KUNIT_CASE(tcti_advsimd_table_source_leaves_resume),
	{}
};

static struct kunit_suite tcti_advsimd_table_source_bound_suite = {
	.name = "orlix-tcti-advsimd-table-source-bound",
	.init = tcti_advsimd_table_test_init,
	.exit = tcti_advsimd_table_test_exit,
	.test_cases = tcti_advsimd_table_source_bound_cases,
};

kunit_test_suite(tcti_advsimd_table_source_bound_suite);

MODULE_LICENSE("GPL");
