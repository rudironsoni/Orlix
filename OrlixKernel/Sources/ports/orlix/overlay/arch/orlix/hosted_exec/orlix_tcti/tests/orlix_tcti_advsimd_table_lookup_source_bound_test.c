// SPDX-License-Identifier: GPL-2.0-only
/*
 * Source-bound production proof for the eight classic AdvSIMD TBL/TBX leaves.
 * A family record is finalized only after the complete source-derived matrix
 * has executed. It deliberately does not export caller-selected generic proof
 * kinds; #120 owns promotion of these aggregate records into shared ingestion.
 */
#include <kunit/test.h>
#include <linux/bitmap.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/string.h>
#include <linux/syscalls.h>
#include <linux/vmalloc.h>

#include <asm/orlix_tcti.h>
#include <asm/processor.h>
#include <asm/ptrace.h>

#include "../decode_aarch64.h"
#include "target_instruction_artifact.h"

#define ORLIX_TCTI_ADVSIMD_TABLE_SVC		0xd4000001U
#define ORLIX_TCTI_ADVSIMD_TABLE_VARIABLE_MASK	0x401f03ffU
#define ORLIX_TCTI_ADVSIMD_TABLE_FIXED_BITS	16U
#define ORLIX_TCTI_ADVSIMD_TABLE_FIXED_FORMS	BIT(ORLIX_TCTI_ADVSIMD_TABLE_FIXED_BITS)
#define ORLIX_TCTI_ADVSIMD_TABLE_REGISTER_FORMS	(32U * 32U * 32U)
#define ORLIX_TCTI_ADVSIMD_TABLE_PROGRAM_SLOTS	ORLIX_TCTI_ADVSIMD_TABLE_FIXED_FORMS
#define ORLIX_TCTI_ADVSIMD_TABLE_PROGRAM_BYTES	(ORLIX_TCTI_ADVSIMD_TABLE_PROGRAM_SLOTS * 2U * sizeof(u32))
#define ORLIX_TCTI_ADVSIMD_TABLE_RECORD_MAGIC	0x54424c58U
#define ORLIX_TCTI_ADVSIMD_TABLE_RECORD_VERSION	1U

enum orlix_tcti_advsimd_table_domain_class {
	ORLIX_TCTI_ADVSIMD_TABLE_DOMAIN_COHORT,
	ORLIX_TCTI_ADVSIMD_TABLE_DOMAIN_OTHER_OWNER,
	ORLIX_TCTI_ADVSIMD_TABLE_DOMAIN_RESERVED,
	ORLIX_TCTI_ADVSIMD_TABLE_DOMAIN_CLASSES,
};

struct orlix_tcti_advsimd_table_leaf {
	u16 source_ordinal;
	const char *source_name;
	const char *source_mnemonic;
	const char *source_operation;
	u32 source_mask;
	u32 source_pattern;
	u8 table_count;
	enum orlix_tcti_simd_table_lookup_op operation;
};

struct orlix_tcti_advsimd_table_context {
	struct mm_struct *mm;
	unsigned long instructions;
	u32 *program;
	unsigned long saved_simd[ARRAY_SIZE(current->thread.user_simd)];
	unsigned long before_simd[ARRAY_SIZE(current->thread.user_simd)];
	unsigned long expected_simd[ARRAY_SIZE(current->thread.user_simd)];
	unsigned long saved_simd_valid;
	unsigned long saved_fpcr;
	unsigned long saved_fpsr;
};

struct orlix_tcti_advsimd_table_registers {
	u8 rd;
	u8 rn;
	u8 rm;
};

struct orlix_tcti_advsimd_table_encoding {
	u32 mask;
	u32 pattern;
};

struct orlix_tcti_advsimd_table_matrix_record {
	unsigned long decoded[2][BITS_TO_LONGS(ORLIX_TCTI_ADVSIMD_TABLE_REGISTER_FORMS)];
	unsigned long resumed[2][BITS_TO_LONGS(ORLIX_TCTI_ADVSIMD_TABLE_REGISTER_FORMS)];
	unsigned long indexes[2][BITS_TO_LONGS(256U)];
	u32 decode_count[2];
	u32 resume_count[2];
	u32 out_of_range_count[2];
	u8 rd_table_aliases[2];
	u8 rm_table_aliases[2];
	bool rd_rm_alias[2];
	bool wrapped_table_list[2];
	u64 transcript;
	u64 transcript_inverse;
};

struct orlix_tcti_advsimd_table_domain_record {
	unsigned long classes[ORLIX_TCTI_ADVSIMD_TABLE_DOMAIN_CLASSES]
		[BITS_TO_LONGS(ORLIX_TCTI_ADVSIMD_TABLE_FIXED_FORMS)];
	u32 class_count[ORLIX_TCTI_ADVSIMD_TABLE_DOMAIN_CLASSES];
	u32 rejected_resumes;
	u64 transcript;
	u64 transcript_inverse;
};

struct orlix_tcti_advsimd_table_proof_record {
	u32 magic;
	u16 version;
	u16 leaf_bitmap;
	u32 legal_decodes;
	u32 production_resumes;
	u32 fixed_forms;
	u32 reserved_resumes;
	u64 transcript;
	u64 transcript_inverse;
};

static const struct orlix_tcti_advsimd_table_leaf orlix_tcti_advsimd_table_leaves[] = {
	{ 3672U, "TBL_asimdtbl_L1_1", "TBL", "TBL_advsimd",
	  0xbfe0fc00U, 0x0e000000U, 1U, ORLIX_TCTI_SIMD_TABLE_LOOKUP_TBL },
	{ 3673U, "TBX_asimdtbl_L1_1", "TBX", "TBX_advsimd",
	  0xbfe0fc00U, 0x0e001000U, 1U, ORLIX_TCTI_SIMD_TABLE_LOOKUP_TBX },
	{ 3674U, "TBL_asimdtbl_L2_2", "TBL", "TBL_advsimd",
	  0xbfe0fc00U, 0x0e002000U, 2U, ORLIX_TCTI_SIMD_TABLE_LOOKUP_TBL },
	{ 3675U, "TBX_asimdtbl_L2_2", "TBX", "TBX_advsimd",
	  0xbfe0fc00U, 0x0e003000U, 2U, ORLIX_TCTI_SIMD_TABLE_LOOKUP_TBX },
	{ 3676U, "TBL_asimdtbl_L3_3", "TBL", "TBL_advsimd",
	  0xbfe0fc00U, 0x0e004000U, 3U, ORLIX_TCTI_SIMD_TABLE_LOOKUP_TBL },
	{ 3677U, "TBX_asimdtbl_L3_3", "TBX", "TBX_advsimd",
	  0xbfe0fc00U, 0x0e005000U, 3U, ORLIX_TCTI_SIMD_TABLE_LOOKUP_TBX },
	{ 3678U, "TBL_asimdtbl_L4_4", "TBL", "TBL_advsimd",
	  0xbfe0fc00U, 0x0e006000U, 4U, ORLIX_TCTI_SIMD_TABLE_LOOKUP_TBL },
	{ 3679U, "TBX_asimdtbl_L4_4", "TBX", "TBX_advsimd",
	  0xbfe0fc00U, 0x0e007000U, 4U, ORLIX_TCTI_SIMD_TABLE_LOOKUP_TBX },
};

static u64 orlix_tcti_advsimd_table_mix(u64 hash, u64 value)
{
	hash ^= value;
	return hash * 0x100000001b3ULL;
}

static const char *orlix_tcti_advsimd_table_artifact_string(
	const struct orlix_tcti_target_instruction_artifact *artifact, u32 offset)
{
	if (offset >= artifact->string_pool_size)
		return NULL;
	return (const char *)artifact->string_pool + offset;
}

static int orlix_tcti_advsimd_table_effective_encoding(
	const struct orlix_tcti_target_instruction_artifact *artifact, u32 ordinal,
	u32 *mask, u32 *pattern)
{
	const struct orlix_tcti_target_instruction_artifact_leaf *leaf;
	u32 index;

	if (!artifact || ordinal >= artifact->leaf_count || !mask || !pattern)
		return -EINVAL;
	leaf = &artifact->leaves[ordinal];
	if (leaf->fixed_operand_first > artifact->fixed_operand_count ||
	    leaf->fixed_operand_count >
		artifact->fixed_operand_count - leaf->fixed_operand_first)
		return -EBADMSG;
	*mask = leaf->encoding_mask;
	*pattern = leaf->encoding_pattern;
	for (index = leaf->fixed_operand_first;
	     index < leaf->fixed_operand_first + leaf->fixed_operand_count;
	     index++) {
		const struct orlix_tcti_target_instruction_artifact_fixed_operand *fixed =
			&artifact->fixed_operands[index];

		if (fixed->leaf_index != ordinal ||
		    (fixed->fixed_value & ~fixed->fixed_mask) ||
		    ((*pattern ^ fixed->fixed_value) & (*mask & fixed->fixed_mask)))
			return -EBADMSG;
		*pattern = (*pattern & ~fixed->fixed_mask) | fixed->fixed_value;
		*mask |= fixed->fixed_mask;
	}
	return (*pattern & ~*mask) ? -EBADMSG : 0;
}

static const struct orlix_tcti_advsimd_table_leaf *
orlix_tcti_advsimd_table_union_leaf(u32 instruction)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_advsimd_table_leaves); index++) {
		const struct orlix_tcti_advsimd_table_leaf *leaf =
			&orlix_tcti_advsimd_table_leaves[index];

		if ((instruction & leaf->source_mask) == leaf->source_pattern)
			return leaf;
	}
	return NULL;
}

static int orlix_tcti_advsimd_table_test_init(struct kunit *test)
{
	struct orlix_tcti_advsimd_table_context *context;
	int ret;

	context = kunit_kzalloc(test, sizeof(*context), GFP_KERNEL);
	if (!context)
		return -ENOMEM;
	context->program = kvzalloc(ORLIX_TCTI_ADVSIMD_TABLE_PROGRAM_BYTES,
				   GFP_KERNEL);
	if (!context->program)
		return -ENOMEM;
	context->mm = mm_alloc();
	if (!context->mm) {
		kvfree(context->program);
		return -ENOMEM;
	}
	kthread_use_mm(context->mm);
	context->instructions = ksys_mmap_pgoff(
		0, ORLIX_TCTI_ADVSIMD_TABLE_PROGRAM_BYTES,
		PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (IS_ERR_VALUE(context->instructions)) {
		ret = (int)context->instructions;
		kthread_unuse_mm(context->mm);
		mmput(context->mm);
		kvfree(context->program);
		return ret;
	}
	memcpy(context->saved_simd, current->thread.user_simd,
	       sizeof(context->saved_simd));
	context->saved_simd_valid = current->thread.user_simd_valid;
	context->saved_fpcr = current->thread.user_fpcr;
	context->saved_fpsr = current->thread.user_fpsr;
	test->priv = context;
	return 0;
}

static void orlix_tcti_advsimd_table_test_exit(struct kunit *test)
{
	struct orlix_tcti_advsimd_table_context *context = test->priv;

	memcpy(current->thread.user_simd, context->saved_simd,
	       sizeof(context->saved_simd));
	current->thread.user_simd_valid = context->saved_simd_valid;
	current->thread.user_fpcr = context->saved_fpcr;
	current->thread.user_fpsr = context->saved_fpsr;
	vm_munmap(context->instructions, ORLIX_TCTI_ADVSIMD_TABLE_PROGRAM_BYTES);
	kthread_unuse_mm(context->mm);
	mmput(context->mm);
	kvfree(context->program);
}

static u32 orlix_tcti_advsimd_table_instruction(
	const struct orlix_tcti_advsimd_table_leaf *leaf, u8 q, u8 rd, u8 rn, u8 rm)
{
	return leaf->source_pattern | ((u32)q << 30) | ((u32)rm << 16) |
		((u32)rn << 5) | rd;
}

static u32 orlix_tcti_advsimd_table_register_slot(u8 rd, u8 rn, u8 rm)
{
	return ((u32)rd << 10) | ((u32)rn << 5) | rm;
}

static int orlix_tcti_advsimd_table_install_programs(
	struct kunit *test, u32 count)
{
	struct orlix_tcti_advsimd_table_context *context = test->priv;
	size_t bytes = (size_t)count * 2U * sizeof(u32);
	int ret;

	ret = sys_mprotect(context->instructions,
		ORLIX_TCTI_ADVSIMD_TABLE_PROGRAM_BYTES, PROT_READ | PROT_WRITE);
	if (ret)
		return ret;
	ret = orlix_tcti_write_user_data(current->mm, context->instructions,
					 context->program, bytes);
	if (ret)
		return ret;
	return sys_mprotect(context->instructions,
		ORLIX_TCTI_ADVSIMD_TABLE_PROGRAM_BYTES, PROT_READ | PROT_EXEC);
}

static void orlix_tcti_advsimd_table_seed_regs(struct pt_regs *regs,
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

static u8 orlix_tcti_advsimd_table_byte(const unsigned long simd[], u8 reg,
					u8 lane)
{
	return simd[reg * 2U + lane / sizeof(u64)] >>
		((lane % sizeof(u64)) * 8U);
}

static void orlix_tcti_advsimd_table_set_byte(unsigned long simd[], u8 reg,
					      u8 lane, u8 value)
{
	unsigned long *word = &simd[reg * 2U + lane / sizeof(u64)];
	u8 shift = (lane % sizeof(u64)) * 8U;

	*word = (*word & ~(0xffUL << shift)) | ((unsigned long)value << shift);
}

static void orlix_tcti_advsimd_table_seed_simd(
	const struct orlix_tcti_advsimd_table_leaf *leaf,
	const struct orlix_tcti_advsimd_table_registers *registers, u8 index_base)
{
	unsigned int index;
	u8 lane;

	for (index = 0; index < ARRAY_SIZE(current->thread.user_simd); index++)
		current->thread.user_simd[index] = 0x9e3779b97f4a7c15ULL ^
			((u64)(index + 1U) * 0x0102040810204081ULL);
	for (lane = 0; lane < leaf->table_count * 16U; lane++)
		orlix_tcti_advsimd_table_set_byte(current->thread.user_simd,
			(registers->rn + lane / 16U) & 31U, lane % 16U,
			0x40U + lane);
	for (lane = 0; lane < 16U; lane++)
		orlix_tcti_advsimd_table_set_byte(current->thread.user_simd,
			registers->rm, lane, index_base + lane);
}

static void orlix_tcti_advsimd_table_expected(unsigned long expected[],
	const unsigned long before[], const struct orlix_tcti_advsimd_table_leaf *leaf,
	const struct orlix_tcti_advsimd_table_registers *registers, u8 result_size)
{
	u8 lane;

	memcpy(expected, before,
	       sizeof(unsigned long) * ARRAY_SIZE(current->thread.user_simd));
	expected[registers->rd * 2U] = 0;
	expected[registers->rd * 2U + 1U] = 0;
	for (lane = 0; lane < result_size; lane++) {
		u8 index = orlix_tcti_advsimd_table_byte(before, registers->rm, lane);
		u8 value = 0;

		if (index < leaf->table_count * 16U)
			value = orlix_tcti_advsimd_table_byte(before,
				(registers->rn + index / 16U) & 31U,
				index % 16U);
		else if (leaf->operation == ORLIX_TCTI_SIMD_TABLE_LOOKUP_TBX)
			value = orlix_tcti_advsimd_table_byte(before,
				registers->rd, lane);
		orlix_tcti_advsimd_table_set_byte(expected, registers->rd, lane, value);
	}
}

static bool orlix_tcti_advsimd_table_matrix_valid(
	const struct orlix_tcti_advsimd_table_leaf *leaf,
	const struct orlix_tcti_advsimd_table_matrix_record *record)
{
	u8 aliases = BIT(leaf->table_count) - 1U;
	u8 q;

	if (!record || !record->transcript ||
	    record->transcript_inverse != ~record->transcript)
		return false;
	for (q = 0; q < 2; q++) {
		if (record->decode_count[q] !=
			    ORLIX_TCTI_ADVSIMD_TABLE_REGISTER_FORMS ||
		    record->resume_count[q] !=
			    ORLIX_TCTI_ADVSIMD_TABLE_REGISTER_FORMS ||
		    !bitmap_full(record->decoded[q],
				 ORLIX_TCTI_ADVSIMD_TABLE_REGISTER_FORMS) ||
		    !bitmap_full(record->resumed[q],
				 ORLIX_TCTI_ADVSIMD_TABLE_REGISTER_FORMS) ||
		    !bitmap_full(record->indexes[q], 256U) ||
		    record->rd_table_aliases[q] != aliases ||
		    record->rm_table_aliases[q] != aliases ||
		    !record->rd_rm_alias[q] || !record->out_of_range_count[q] ||
		    (leaf->table_count > 1U && !record->wrapped_table_list[q]))
			return false;
	}
	return true;
}

static int orlix_tcti_advsimd_table_run_matrix_entry(
	struct kunit *test, const struct orlix_tcti_advsimd_table_leaf *leaf,
	struct orlix_tcti_advsimd_table_matrix_record *record, u8 q,
	const struct orlix_tcti_advsimd_table_registers *registers)
{
	struct orlix_tcti_advsimd_table_context *context = test->priv;
	struct orlix_tcti_decoded_instruction decoded;
	struct orlix_tcti_result result;
	struct pt_regs regs;
	struct pt_regs expected_regs;
	u32 slot = orlix_tcti_advsimd_table_register_slot(
		registers->rd, registers->rn, registers->rm);
	u32 instruction = context->program[slot * 2U];
	unsigned long pc = context->instructions + slot * 2U * sizeof(u32);
	u8 result_size = q ? 16U : 8U;
	u8 lane;
	u8 table;

	decoded = orlix_tcti_decode_aarch64(instruction);
	if (decoded.decode_class != ORLIX_TCTI_DECODE_SIMD_TABLE_LOOKUP ||
	    decoded.simd_table_lookup_op != leaf->operation ||
	    decoded.simd_table_count != leaf->table_count || decoded.simd_q != !!q ||
	    decoded.result_size != result_size || decoded.rd != registers->rd ||
	    decoded.rn != registers->rn || decoded.rm != registers->rm)
		return -EBADMSG;
	if (test_and_set_bit(slot, record->decoded[q]))
		return -EALREADY;
	record->decode_count[q]++;

	orlix_tcti_advsimd_table_seed_simd(leaf, registers, (u8)slot);
	memcpy(context->before_simd, current->thread.user_simd,
	       sizeof(context->before_simd));
	orlix_tcti_advsimd_table_expected(context->expected_simd,
		context->before_simd, leaf, registers, result_size);
	current->thread.user_simd_valid = 0;
	current->thread.user_fpcr = BIT(22) | BIT(24);
	current->thread.user_fpsr = BIT(27) | BIT(4);
	orlix_tcti_advsimd_table_seed_regs(&regs, pc, instruction);
	expected_regs = regs;
	expected_regs.pc += sizeof(u32);
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	if (!result.entry_valid || result.entry_pc != pc ||
	    result.entry_instruction != instruction ||
	    result.reason != ORLIX_TCTI_EXIT_SYSCALL || result.status ||
	    result.fault_address || result.fault_access != ORLIX_TCTI_ACCESS_FETCH ||
	    result.pc != pc + sizeof(u32) ||
	    result.instruction != ORLIX_TCTI_ADVSIMD_TABLE_SVC ||
	    memcmp(&expected_regs, &regs, sizeof(regs)) ||
	    memcmp(context->expected_simd, current->thread.user_simd,
		   sizeof(context->expected_simd)) ||
	    current->thread.user_simd_valid != 1 ||
	    current->thread.user_fpcr != (BIT(22) | BIT(24)) ||
	    current->thread.user_fpsr != (BIT(27) | BIT(4)))
		return -EUCLEAN;
	if (test_and_set_bit(slot, record->resumed[q]))
		return -EALREADY;
	record->resume_count[q]++;
	record->transcript = orlix_tcti_advsimd_table_mix(
		record->transcript, instruction);
	record->transcript = orlix_tcti_advsimd_table_mix(
		record->transcript, current->thread.user_simd[registers->rd * 2U]);
	for (lane = 0; lane < result_size; lane++) {
		u8 index = orlix_tcti_advsimd_table_byte(
			context->before_simd, registers->rm, lane);

		set_bit(index, record->indexes[q]);
		if (index >= leaf->table_count * 16U)
			record->out_of_range_count[q]++;
	}
	for (table = 0; table < leaf->table_count; table++) {
		u8 table_reg = (registers->rn + table) & 31U;

		if (registers->rd == table_reg)
			record->rd_table_aliases[q] |= BIT(table);
		if (registers->rm == table_reg)
			record->rm_table_aliases[q] |= BIT(table);
		if (registers->rn + table > 31U)
			record->wrapped_table_list[q] = true;
	}
	if (registers->rd == registers->rm)
		record->rd_rm_alias[q] = true;
	return 0;
}

static int orlix_tcti_advsimd_table_run_leaf_matrix(
	struct kunit *test, const struct orlix_tcti_advsimd_table_leaf *leaf,
	struct orlix_tcti_advsimd_table_matrix_record *record)
{
	struct orlix_tcti_advsimd_table_context *context = test->priv;
	u8 q;
	u8 rd;
	u8 rn;
	u8 rm;

	memset(record, 0, sizeof(*record));
	record->transcript = 0xcbf29ce484222325ULL;
	for (q = 0; q < 2; q++) {
		for (rd = 0; rd < 32U; rd++)
			for (rn = 0; rn < 32U; rn++)
				for (rm = 0; rm < 32U; rm++) {
					u32 slot = orlix_tcti_advsimd_table_register_slot(
						rd, rn, rm);

					context->program[slot * 2U] =
						orlix_tcti_advsimd_table_instruction(
							leaf, q, rd, rn, rm);
					context->program[slot * 2U + 1U] =
						ORLIX_TCTI_ADVSIMD_TABLE_SVC;
				}
		if (orlix_tcti_advsimd_table_install_programs(
				test, ORLIX_TCTI_ADVSIMD_TABLE_REGISTER_FORMS))
			return -EFAULT;
		for (rd = 0; rd < 32U; rd++)
			for (rn = 0; rn < 32U; rn++)
				for (rm = 0; rm < 32U; rm++) {
					const struct orlix_tcti_advsimd_table_registers regs = {
						rd, rn, rm,
					};
					int ret = orlix_tcti_advsimd_table_run_matrix_entry(
						test, leaf, record, q, &regs);

					if (ret)
						return ret;
				}
	}
	record->transcript_inverse = ~record->transcript;
	return orlix_tcti_advsimd_table_matrix_valid(leaf, record) ? 0 : -EBADMSG;
}

static u32 orlix_tcti_advsimd_table_deposit_fixed(u32 value, u32 mask)
{
	u32 instruction = 0;
	u8 source_bit = 0;
	u8 target_bit;

	for (target_bit = 0; target_bit < 32U; target_bit++) {
		if (!(mask & BIT(target_bit)))
			continue;
		if (value & BIT(source_bit))
			instruction |= BIT(target_bit);
		source_bit++;
	}
	return instruction;
}

static size_t orlix_tcti_advsimd_table_compatible_owner_count(
	const struct orlix_tcti_advsimd_table_encoding *encodings,
	size_t encoding_count, u32 fixed_instruction, u32 fixed_mask)
{
	size_t owners = 0;
	size_t index;

	for (index = 0; index < encoding_count; index++)
		if (!((encodings[index].pattern ^ fixed_instruction) &
		      encodings[index].mask & fixed_mask))
			owners++;
	return owners;
}

static bool orlix_tcti_advsimd_table_domain_valid(
	const struct orlix_tcti_advsimd_table_domain_record *record)
{
	DECLARE_BITMAP(seen, ORLIX_TCTI_ADVSIMD_TABLE_FIXED_FORMS);
	u32 total = 0;
	u8 class;

	if (!record || !record->transcript ||
	    record->transcript_inverse != ~record->transcript ||
	    record->class_count[ORLIX_TCTI_ADVSIMD_TABLE_DOMAIN_COHORT] !=
		ARRAY_SIZE(orlix_tcti_advsimd_table_leaves) ||
	    record->rejected_resumes !=
		record->class_count[ORLIX_TCTI_ADVSIMD_TABLE_DOMAIN_RESERVED])
		return false;
	bitmap_zero(seen, ORLIX_TCTI_ADVSIMD_TABLE_FIXED_FORMS);
	for (class = 0; class < ORLIX_TCTI_ADVSIMD_TABLE_DOMAIN_CLASSES; class++) {
		if (bitmap_intersects(seen, record->classes[class],
				      ORLIX_TCTI_ADVSIMD_TABLE_FIXED_FORMS))
			return false;
		bitmap_or(seen, seen, record->classes[class],
			  ORLIX_TCTI_ADVSIMD_TABLE_FIXED_FORMS);
		total += record->class_count[class];
		if (bitmap_weight(record->classes[class],
				  ORLIX_TCTI_ADVSIMD_TABLE_FIXED_FORMS) !=
		    record->class_count[class])
			return false;
	}
	return total == ORLIX_TCTI_ADVSIMD_TABLE_FIXED_FORMS &&
		bitmap_full(seen, ORLIX_TCTI_ADVSIMD_TABLE_FIXED_FORMS);
}

static int orlix_tcti_advsimd_table_run_reserved(
	struct kunit *test, struct orlix_tcti_advsimd_table_domain_record *record,
	u32 count)
{
	struct orlix_tcti_advsimd_table_context *context = test->priv;
	u32 slot;

	if (orlix_tcti_advsimd_table_install_programs(test, count))
		return -EFAULT;
	for (slot = 0; slot < count; slot++) {
		struct orlix_tcti_result result;
		struct pt_regs regs;
		struct pt_regs before_regs;
		u32 instruction = context->program[slot * 2U];
		unsigned long pc = context->instructions + slot * 2U * sizeof(u32);
		unsigned int index;

		for (index = 0; index < ARRAY_SIZE(current->thread.user_simd); index++)
			current->thread.user_simd[index] =
				0x6a09e667f3bcc909ULL ^ instruction ^ index;
		memcpy(context->before_simd, current->thread.user_simd,
		       sizeof(context->before_simd));
		current->thread.user_simd_valid = 1;
		current->thread.user_fpcr = BIT(22) | BIT(24);
		current->thread.user_fpsr = BIT(27) | BIT(4);
		orlix_tcti_advsimd_table_seed_regs(&regs, pc, instruction);
		before_regs = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		if (!result.entry_valid || result.entry_pc != pc ||
		    result.entry_instruction != instruction ||
		    result.reason != ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION ||
		    result.status != -EOPNOTSUPP || result.pc != pc ||
		    result.instruction != instruction ||
		    memcmp(&before_regs, &regs, sizeof(regs)) ||
		    memcmp(context->before_simd, current->thread.user_simd,
			   sizeof(context->before_simd)) ||
		    current->thread.user_simd_valid != 1 ||
		    current->thread.user_fpcr != (BIT(22) | BIT(24)) ||
		    current->thread.user_fpsr != (BIT(27) | BIT(4)))
			return -EUCLEAN;
		record->rejected_resumes++;
		record->transcript = orlix_tcti_advsimd_table_mix(
			record->transcript, instruction);
	}
	return 0;
}

static int orlix_tcti_advsimd_table_collect_fixed_domain(
	struct kunit *test, struct orlix_tcti_advsimd_table_domain_record *record)
{
	const struct orlix_tcti_target_instruction_artifact *artifact =
		orlix_tcti_target_instruction_artifact_canonical();
	struct orlix_tcti_advsimd_table_context *context = test->priv;
	struct orlix_tcti_advsimd_table_encoding *encodings;
	u32 fixed_mask = orlix_tcti_advsimd_table_leaves[0].source_mask;
	u32 reserved_slots = 0;
	u32 form;
	u32 ordinal;
	int ret = 0;

	if (!artifact || hweight32(fixed_mask) !=
			 ORLIX_TCTI_ADVSIMD_TABLE_FIXED_BITS)
		return -EBADMSG;
	encodings = kvmalloc_array(artifact->leaf_count, sizeof(*encodings),
				  GFP_KERNEL);
	if (!encodings)
		return -ENOMEM;
	for (ordinal = 0; ordinal < artifact->leaf_count; ordinal++) {
		ret = orlix_tcti_advsimd_table_effective_encoding(
			artifact, ordinal, &encodings[ordinal].mask,
			&encodings[ordinal].pattern);
		if (ret)
			goto out;
	}
	memset(record, 0, sizeof(*record));
	record->transcript = 0xcbf29ce484222325ULL;
	for (form = 0; form < ORLIX_TCTI_ADVSIMD_TABLE_FIXED_FORMS; form++) {
		const struct orlix_tcti_advsimd_table_leaf *cohort_leaf;
		struct orlix_tcti_decoded_instruction decoded;
		enum orlix_tcti_advsimd_table_domain_class class;
		u32 instruction = orlix_tcti_advsimd_table_deposit_fixed(
			form, fixed_mask);
		size_t owners = orlix_tcti_advsimd_table_compatible_owner_count(
			encodings, artifact->leaf_count, instruction, fixed_mask);

		cohort_leaf = orlix_tcti_advsimd_table_union_leaf(instruction);
		decoded = orlix_tcti_decode_aarch64(instruction);
		if (cohort_leaf) {
			if (!owners ||
			    decoded.decode_class != ORLIX_TCTI_DECODE_SIMD_TABLE_LOOKUP ||
			    decoded.simd_table_lookup_op != cohort_leaf->operation ||
			    decoded.simd_table_count != cohort_leaf->table_count)
				goto bad_domain;
			class = ORLIX_TCTI_ADVSIMD_TABLE_DOMAIN_COHORT;
		} else if (owners) {
			if (decoded.decode_class == ORLIX_TCTI_DECODE_SIMD_TABLE_LOOKUP)
				goto bad_domain;
			class = ORLIX_TCTI_ADVSIMD_TABLE_DOMAIN_OTHER_OWNER;
		} else {
			if (decoded.decode_class != ORLIX_TCTI_DECODE_UNSUPPORTED)
				goto bad_domain;
			class = ORLIX_TCTI_ADVSIMD_TABLE_DOMAIN_RESERVED;
			context->program[reserved_slots * 2U] = instruction;
			context->program[reserved_slots * 2U + 1U] =
				ORLIX_TCTI_ADVSIMD_TABLE_SVC;
			reserved_slots++;
		}
		set_bit(form, record->classes[class]);
		record->class_count[class]++;
		record->transcript = orlix_tcti_advsimd_table_mix(
			record->transcript,
			((u64)instruction << 32) | ((u64)owners << 2) | class);
	}
	ret = orlix_tcti_advsimd_table_run_reserved(test, record, reserved_slots);
	if (ret)
		goto out;
	record->transcript_inverse = ~record->transcript;
	ret = orlix_tcti_advsimd_table_domain_valid(record) ? 0 : -EBADMSG;
	goto out;

bad_domain:
	ret = -EBADMSG;
out:
	kvfree(encodings);
	return ret;
}

static bool orlix_tcti_advsimd_table_proof_record_valid(
	const struct orlix_tcti_advsimd_table_proof_record *record)
{
	return record && record->magic == ORLIX_TCTI_ADVSIMD_TABLE_RECORD_MAGIC &&
		record->version == ORLIX_TCTI_ADVSIMD_TABLE_RECORD_VERSION &&
		record->leaf_bitmap == BIT(ARRAY_SIZE(orlix_tcti_advsimd_table_leaves)) - 1U &&
		record->legal_decodes == ARRAY_SIZE(orlix_tcti_advsimd_table_leaves) *
			2U * ORLIX_TCTI_ADVSIMD_TABLE_REGISTER_FORMS &&
		record->production_resumes == record->legal_decodes &&
		record->fixed_forms == ORLIX_TCTI_ADVSIMD_TABLE_FIXED_FORMS &&
		record->reserved_resumes > 0 && record->transcript &&
		record->transcript_inverse == ~record->transcript;
}

static void orlix_tcti_advsimd_table_source_bindings(struct kunit *test)
{
	const struct orlix_tcti_target_instruction_artifact *artifact =
		orlix_tcti_target_instruction_artifact_canonical();
	struct orlix_tcti_target_instruction_artifact_validation_result validation;
	size_t index;

	KUNIT_ASSERT_NOT_NULL(test, artifact);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_target_instruction_artifact_validate(artifact, &validation));
	KUNIT_ASSERT_GT(test, artifact->leaf_count, 3679U);
	KUNIT_ASSERT_EQ(test, 8U, ARRAY_SIZE(orlix_tcti_advsimd_table_leaves));
	for (index = 0; index < ARRAY_SIZE(orlix_tcti_advsimd_table_leaves); index++) {
		const struct orlix_tcti_advsimd_table_leaf *leaf =
			&orlix_tcti_advsimd_table_leaves[index];
		const struct orlix_tcti_target_instruction_artifact_leaf *source =
			&artifact->leaves[leaf->source_ordinal];
		u32 mask;
		u32 pattern;

		KUNIT_ASSERT_EQ(test, 0,
			orlix_tcti_advsimd_table_effective_encoding(
				artifact, leaf->source_ordinal, &mask, &pattern));
		KUNIT_EXPECT_STREQ(test, leaf->source_name,
			orlix_tcti_advsimd_table_artifact_string(
				artifact, source->name_offset));
		KUNIT_EXPECT_STREQ(test, leaf->source_mnemonic,
			orlix_tcti_advsimd_table_artifact_string(
				artifact, source->mnemonic_offset));
		KUNIT_EXPECT_STREQ(test, leaf->source_operation,
			orlix_tcti_advsimd_table_artifact_string(
				artifact, source->operation_offset));
		KUNIT_EXPECT_EQ(test, leaf->source_mask, mask);
		KUNIT_EXPECT_EQ(test, leaf->source_pattern, pattern);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ADVSIMD_TABLE_VARIABLE_MASK, ~mask);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ADVSIMD_TABLE_FIXED_BITS,
			hweight32(mask));
	}
}

static void orlix_tcti_advsimd_table_complete_typed_proof_resume(
	struct kunit *test)
{
	struct orlix_tcti_advsimd_table_matrix_record *matrix;
	struct orlix_tcti_advsimd_table_domain_record *domain;
	struct orlix_tcti_advsimd_table_proof_record *proof;
	size_t leaf_index;

	matrix = kvzalloc(sizeof(*matrix), GFP_KERNEL);
	domain = kvzalloc(sizeof(*domain), GFP_KERNEL);
	proof = kunit_kzalloc(test, sizeof(*proof), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, matrix);
	KUNIT_ASSERT_NOT_NULL(test, domain);
	KUNIT_ASSERT_NOT_NULL(test, proof);
	proof->magic = ORLIX_TCTI_ADVSIMD_TABLE_RECORD_MAGIC;
	proof->version = ORLIX_TCTI_ADVSIMD_TABLE_RECORD_VERSION;
	proof->transcript = 0xcbf29ce484222325ULL;
	for (leaf_index = 0; leaf_index < ARRAY_SIZE(orlix_tcti_advsimd_table_leaves);
	     leaf_index++) {
		const struct orlix_tcti_advsimd_table_leaf *leaf =
			&orlix_tcti_advsimd_table_leaves[leaf_index];

		KUNIT_ASSERT_EQ_MSG(test, 0,
			orlix_tcti_advsimd_table_run_leaf_matrix(test, leaf, matrix),
			"ordinal=%u", leaf->source_ordinal);
		proof->leaf_bitmap |= BIT(leaf_index);
		proof->legal_decodes += matrix->decode_count[0] +
			matrix->decode_count[1];
		proof->production_resumes += matrix->resume_count[0] +
			matrix->resume_count[1];
		proof->transcript = orlix_tcti_advsimd_table_mix(
			proof->transcript, matrix->transcript);
	}
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_advsimd_table_collect_fixed_domain(test, domain));
	KUNIT_ASSERT_TRUE(test, orlix_tcti_advsimd_table_domain_valid(domain));
	proof->fixed_forms = ORLIX_TCTI_ADVSIMD_TABLE_FIXED_FORMS;
	proof->reserved_resumes = domain->rejected_resumes;
	proof->transcript = orlix_tcti_advsimd_table_mix(
		proof->transcript, domain->transcript);
	proof->transcript_inverse = ~proof->transcript;
	KUNIT_EXPECT_TRUE(test, orlix_tcti_advsimd_table_proof_record_valid(proof));
	kvfree(domain);
	kvfree(matrix);
}

static void orlix_tcti_advsimd_table_proof_record_mutations_rejected(
	struct kunit *test)
{
	struct orlix_tcti_advsimd_table_proof_record valid = {
		.magic = ORLIX_TCTI_ADVSIMD_TABLE_RECORD_MAGIC,
		.version = ORLIX_TCTI_ADVSIMD_TABLE_RECORD_VERSION,
		.leaf_bitmap = BIT(ARRAY_SIZE(orlix_tcti_advsimd_table_leaves)) - 1U,
		.legal_decodes = ARRAY_SIZE(orlix_tcti_advsimd_table_leaves) * 2U *
			ORLIX_TCTI_ADVSIMD_TABLE_REGISTER_FORMS,
		.production_resumes = ARRAY_SIZE(orlix_tcti_advsimd_table_leaves) * 2U *
			ORLIX_TCTI_ADVSIMD_TABLE_REGISTER_FORMS,
		.fixed_forms = ORLIX_TCTI_ADVSIMD_TABLE_FIXED_FORMS,
		.reserved_resumes = 1U,
		.transcript = 0x123456789abcdef0ULL,
		.transcript_inverse = ~0x123456789abcdef0ULL,
	};
	struct orlix_tcti_advsimd_table_proof_record mutation;

	KUNIT_ASSERT_TRUE(test, orlix_tcti_advsimd_table_proof_record_valid(&valid));
	mutation = valid;
	mutation.leaf_bitmap &= ~BIT(3);
	KUNIT_EXPECT_FALSE(test,
		orlix_tcti_advsimd_table_proof_record_valid(&mutation));
	mutation = valid;
	mutation.legal_decodes--;
	KUNIT_EXPECT_FALSE(test,
		orlix_tcti_advsimd_table_proof_record_valid(&mutation));
	mutation = valid;
	mutation.production_resumes--;
	KUNIT_EXPECT_FALSE(test,
		orlix_tcti_advsimd_table_proof_record_valid(&mutation));
	mutation = valid;
	mutation.fixed_forms--;
	KUNIT_EXPECT_FALSE(test,
		orlix_tcti_advsimd_table_proof_record_valid(&mutation));
	mutation = valid;
	mutation.reserved_resumes = 0;
	KUNIT_EXPECT_FALSE(test,
		orlix_tcti_advsimd_table_proof_record_valid(&mutation));
	mutation = valid;
	mutation.transcript ^= BIT_ULL(17);
	KUNIT_EXPECT_FALSE(test,
		orlix_tcti_advsimd_table_proof_record_valid(&mutation));
}

static struct kunit_case orlix_tcti_advsimd_table_source_bound_cases[] = {
	KUNIT_CASE(orlix_tcti_advsimd_table_source_bindings),
	KUNIT_CASE(orlix_tcti_advsimd_table_complete_typed_proof_resume),
	KUNIT_CASE(orlix_tcti_advsimd_table_proof_record_mutations_rejected),
	{}
};

static struct kunit_suite orlix_tcti_advsimd_table_source_bound_suite = {
	.name = "orlix-tcti-advsimd-table-source-bound",
	.init = orlix_tcti_advsimd_table_test_init,
	.exit = orlix_tcti_advsimd_table_test_exit,
	.test_cases = orlix_tcti_advsimd_table_source_bound_cases,
};

kunit_test_suite(orlix_tcti_advsimd_table_source_bound_suite);

MODULE_LICENSE("GPL");
