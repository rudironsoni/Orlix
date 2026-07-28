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
#include <linux/utsname.h>

#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>

#include "../decode_aarch64.h"
#include "orlix_tcti_native_observation.h"
#include "target_instruction_artifact.h"
#include "target_proof_ingestion.h"
#include "target_proof_registry.h"

#define ORLIX_TCTI_ADVSIMD_TABLE_SVC	0xd4000001U
#define ORLIX_TCTI_ADVSIMD_TABLE_VARIABLE_MASK	0x401f03ffU
#define ORLIX_TCTI_ADVSIMD_TABLE_CONDITION \
	"54434e440107000000310700000017070000000c01000000010101000000010101000000010102000000100000000c464541545f41647653494d44"
#define ORLIX_TCTI_ADVSIMD_TABLE_TYPED_CASE \
	"orlix_tcti_advsimd_table_typed_obligations_resume"
#define ORLIX_TCTI_ADVSIMD_TABLE_REJECTION_CASE \
	"orlix_tcti_advsimd_table_fixed_bit_neighbors_resume"

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
	unsigned long simd[ARRAY_SIZE(current->thread.user_simd)];
	unsigned long simd_valid;
	unsigned long fpcr;
	unsigned long fpsr;
};

struct orlix_tcti_advsimd_table_registers {
	u8 rd;
	u8 rn;
	u8 rm;
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

static const struct orlix_tcti_advsimd_table_registers
orlix_tcti_advsimd_table_overlap_cases[] = {
	{ 4U, 20U, 6U },		/* all distinct */
	{ 20U, 20U, 6U },	/* destination is table base */
	{ 4U, 20U, 4U },		/* destination is index */
	{ 4U, 20U, 20U },	/* index is table base */
	{ 20U, 20U, 20U },	/* all operands alias */
	{ 4U, 31U, 7U },		/* L2-L4 table register wraps V31 to V0 */
};

struct orlix_tcti_advsimd_table_typed_obligation {
	enum orlix_tcti_native_obligation native;
	u32 proof;
};

static const struct orlix_tcti_advsimd_table_typed_obligation
orlix_tcti_advsimd_table_typed_obligations[] = {
	{ ORLIX_TCTI_NATIVE_OBLIGATION_DECODE,
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE },
	{ ORLIX_TCTI_NATIVE_OBLIGATION_LEGAL_ENCODINGS,
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ ORLIX_TCTI_NATIVE_OBLIGATION_REGISTERS,
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS },
	{ ORLIX_TCTI_NATIVE_OBLIGATION_PC,
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ ORLIX_TCTI_NATIVE_OBLIGATION_FLAGS,
	  ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
};

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
		    ((*pattern ^ fixed->fixed_value) &
		     (*mask & fixed->fixed_mask)))
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

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_advsimd_table_leaves);
	     index++) {
		const struct orlix_tcti_advsimd_table_leaf *leaf =
			&orlix_tcti_advsimd_table_leaves[index];

		if ((instruction & leaf->source_mask) == leaf->source_pattern)
			return leaf;
	}
	return NULL;
}

static size_t orlix_tcti_advsimd_table_source_owner_count(
	const struct orlix_tcti_target_instruction_artifact *artifact,
	u32 instruction)
{
	size_t owners = 0;
	u32 ordinal;

	for (ordinal = 0; ordinal < artifact->leaf_count; ordinal++) {
		u32 mask;
		u32 pattern;

		if (orlix_tcti_advsimd_table_effective_encoding(
				artifact, ordinal, &mask, &pattern))
			return SIZE_MAX;
		if ((instruction & mask) == pattern)
			owners++;
	}
	return owners;
}

static int orlix_tcti_advsimd_table_test_init(struct kunit *test)
{
	struct orlix_tcti_advsimd_table_context *context;

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

static void orlix_tcti_advsimd_table_test_exit(struct kunit *test)
{
	struct orlix_tcti_advsimd_table_context *context = test->priv;

	memcpy(current->thread.user_simd, context->simd, sizeof(context->simd));
	current->thread.user_simd_valid = context->simd_valid;
	current->thread.user_fpcr = context->fpcr;
	current->thread.user_fpsr = context->fpsr;
	vm_munmap(context->instructions, PAGE_SIZE);
	kthread_unuse_mm(context->mm);
	mmput(context->mm);
}

static u32 orlix_tcti_advsimd_table_instruction(
	const struct orlix_tcti_advsimd_table_leaf *leaf, u8 q, u8 rd, u8 rn, u8 rm)
{
	return leaf->source_pattern | ((u32)q << 30) | ((u32)rm << 16) |
		((u32)rn << 5) | rd;
}

static int orlix_tcti_advsimd_table_load_instruction(struct kunit *test,
						 u32 instruction)
{
	struct orlix_tcti_advsimd_table_context *context = test->priv;
	const u32 program[] = { instruction, ORLIX_TCTI_ADVSIMD_TABLE_SVC };
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

static u8 orlix_tcti_advsimd_table_byte(const unsigned long simd[], u8 reg, u8 lane)
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

static void orlix_tcti_advsimd_table_seed_simd(const struct orlix_tcti_advsimd_table_leaf *leaf,
					  const struct orlix_tcti_advsimd_table_registers *registers)
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
		orlix_tcti_advsimd_table_set_byte(current->thread.user_simd,
			registers->rm, lane, value);
	}
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
			value = orlix_tcti_advsimd_table_byte(before, registers->rd, lane);
		orlix_tcti_advsimd_table_set_byte(expected, registers->rd, lane, value);
	}
}

static const char *orlix_tcti_advsimd_table_proof_id(
	const struct orlix_tcti_advsimd_table_leaf *leaf)
{
	return leaf->operation == ORLIX_TCTI_SIMD_TABLE_LOOKUP_TBX ?
		"kunit:advsimd-table-tbx-source-leaves" :
		"kunit:advsimd-table-tbl-source-leaves";
}

static const struct orlix_tcti_target_proof_registry_entry *
orlix_tcti_advsimd_table_registry_entry(
	const struct orlix_tcti_advsimd_table_leaf *leaf)
{
	const struct orlix_tcti_target_proof_registry_entry *entries;
	const char *proof_id = orlix_tcti_advsimd_table_proof_id(leaf);
	size_t count;
	size_t index;

	entries = orlix_tcti_target_proof_registry_entries(&count);
	for (index = 0; entries && index < count; index++)
		if (!strcmp(entries[index].id, proof_id))
			return &entries[index];
	return NULL;
}

static void orlix_tcti_advsimd_table_capture_fp_simd(
	struct orlix_tcti_native_fp_simd_state *state,
	const unsigned long simd[], unsigned long fpcr, unsigned long fpsr,
	bool valid)
{
	memset(state, 0, sizeof(*state));
	memcpy(state->v, simd, sizeof(state->v));
	state->fpcr = fpcr;
	state->fpsr = fpsr;
	state->valid = valid;
}

static void orlix_tcti_advsimd_table_ingest_record(
	struct kunit *test, const struct orlix_tcti_advsimd_table_leaf *leaf,
	const char *case_name,
	struct orlix_tcti_target_native_result_record *record,
	struct orlix_tcti_target_proof_ingestion_ledger *ledger)
{
	const struct orlix_tcti_target_proof_registry_entry *entry =
		orlix_tcti_advsimd_table_registry_entry(leaf);
	struct orlix_tcti_target_kunit_provenance_identity provenance;
	struct orlix_tcti_target_native_ingestion_selector selector;
	enum orlix_tcti_target_proof_ingestion_error error;
	char build_identity[ORLIX_TCTI_TARGET_PROOF_BUILD_ID_MAX];
	int ret;

	KUNIT_ASSERT_NOT_NULL(test, entry);
	KUNIT_ASSERT_NOT_NULL(test, record);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_target_kunit_provenance_identity(
			entry, case_name, &provenance));
	scnprintf(build_identity, sizeof(build_identity), "%s|%s|%s",
		 init_utsname()->release, init_utsname()->version,
		 init_utsname()->machine);
	selector = (struct orlix_tcti_target_native_ingestion_selector) {
		.proof_id = entry->id,
		.classification_mask = ORLIX_TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0,
		.condition_tcnd_hex = ORLIX_TCTI_ADVSIMD_TABLE_CONDITION,
		.kunit_source = provenance.source,
		.kunit_source_sha256 = provenance.source_sha256,
		.kunit_build_source = provenance.build_source,
		.kunit_build_source_sha256 = provenance.build_source_sha256,
		.kunit_suite = provenance.suite,
		.kunit_case = provenance.case_name,
		.executing_kernel_identity = build_identity,
	};
	ret = orlix_tcti_target_proof_ingest_native(
		ledger, record, &selector, &error);
	KUNIT_EXPECT_EQ_MSG(test, 0, ret, "%s ordinal=%u error=%u",
		case_name, leaf->source_ordinal, error);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_TARGET_PROOF_INGEST_OK, error);
}

static void orlix_tcti_advsimd_table_observe_legal(
	struct kunit *test, const struct orlix_tcti_advsimd_table_leaf *leaf,
	enum orlix_tcti_native_obligation obligation,
	struct orlix_tcti_target_native_result_record **record)
{
	static const struct orlix_tcti_advsimd_table_registers registers = {
		4U, 31U, 7U,
	};
	struct orlix_tcti_advsimd_table_context *context = test->priv;
	struct orlix_tcti_native_observation_spec spec = {};
	struct orlix_tcti_native_observation *observation;
	struct orlix_tcti_native_fp_simd_state observed_fp_simd;
	unsigned long *before_simd;
	unsigned long *expected_simd;
	struct pt_regs regs;
	struct pt_regs expected_regs;
	u32 instruction = orlix_tcti_advsimd_table_instruction(
		leaf, 1U, registers.rd, registers.rn, registers.rm);
	int ret;

	*record = NULL;
	before_simd = kunit_kcalloc(test,
		ARRAY_SIZE(current->thread.user_simd), sizeof(*before_simd),
		GFP_KERNEL);
	expected_simd = kunit_kcalloc(test,
		ARRAY_SIZE(current->thread.user_simd), sizeof(*expected_simd),
		GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, before_simd);
	KUNIT_ASSERT_NOT_NULL(test, expected_simd);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_advsimd_table_load_instruction(test, instruction));
	orlix_tcti_advsimd_table_seed_simd(leaf, &registers);
	memcpy(before_simd, current->thread.user_simd,
	       sizeof(*before_simd) * ARRAY_SIZE(current->thread.user_simd));
	orlix_tcti_advsimd_table_expected(
		expected_simd, before_simd, leaf, &registers, 16U);
	current->thread.user_simd_valid = 0;
	current->thread.user_fpcr = BIT(22) | BIT(24);
	current->thread.user_fpsr = BIT(27) | BIT(4);
	orlix_tcti_advsimd_table_seed_regs(
		&regs, context->instructions, instruction);
	expected_regs = regs;
	expected_regs.pc += sizeof(u32);
	spec.source_ordinal = leaf->source_ordinal;
	spec.obligation = obligation;
	spec.result = (struct orlix_tcti_result) {
		.reason = ORLIX_TCTI_EXIT_SYSCALL,
		.status = 0,
		.fault_access = ORLIX_TCTI_ACCESS_FETCH,
		.pc = context->instructions + sizeof(u32),
		.instruction = ORLIX_TCTI_ADVSIMD_TABLE_SVC,
	};
	orlix_tcti_native_gpr_capture(&spec.gpr, &expected_regs);
	if (obligation == ORLIX_TCTI_NATIVE_OBLIGATION_REGISTERS ||
	    obligation == ORLIX_TCTI_NATIVE_OBLIGATION_FLAGS)
		orlix_tcti_advsimd_table_capture_fp_simd(
			&spec.expected.fp_simd, expected_simd,
			BIT(22) | BIT(24), BIT(27) | BIT(4), true);
	observation = orlix_tcti_native_observation_create(&spec);
	KUNIT_ASSERT_NOT_NULL(test, observation);
	ret = orlix_tcti_native_observation_execute(
		observation, current, &regs, current->mm);
	KUNIT_ASSERT_EQ(test, 0, ret);
	if (obligation == ORLIX_TCTI_NATIVE_OBLIGATION_REGISTERS ||
	    obligation == ORLIX_TCTI_NATIVE_OBLIGATION_FLAGS) {
		orlix_tcti_advsimd_table_capture_fp_simd(
			&observed_fp_simd, current->thread.user_simd,
			current->thread.user_fpcr, current->thread.user_fpsr,
			current->thread.user_simd_valid);
		KUNIT_ASSERT_EQ(test, 0,
			orlix_tcti_native_observation_add_fp_simd(
				observation, &observed_fp_simd));
	}
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_native_observation_compare(observation));
	KUNIT_ASSERT_EQ(test, ORLIX_TCTI_NATIVE_OBSERVATION_MATCH,
		orlix_tcti_native_observation_state(observation));
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_native_observation_export(observation, record));
	orlix_tcti_native_observation_destroy(observation);
}

static void orlix_tcti_advsimd_table_observe_rejection(
	struct kunit *test, const struct orlix_tcti_advsimd_table_leaf *leaf,
	u32 instruction,
	struct orlix_tcti_target_native_result_record **record)
{
	struct orlix_tcti_advsimd_table_context *context = test->priv;
	struct orlix_tcti_native_observation_spec spec = {};
	struct orlix_tcti_native_observation *observation;
	struct orlix_tcti_native_fp_simd_state observed_fp_simd;
	unsigned long *before_simd;
	struct pt_regs regs;
	struct pt_regs before_regs;
	int ret;

	*record = NULL;
	before_simd = kunit_kcalloc(test,
		ARRAY_SIZE(current->thread.user_simd), sizeof(*before_simd),
		GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, before_simd);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_advsimd_table_load_instruction(test, instruction));
	orlix_tcti_advsimd_table_seed_simd(
		leaf, &orlix_tcti_advsimd_table_overlap_cases[0]);
	memcpy(before_simd, current->thread.user_simd,
	       sizeof(*before_simd) * ARRAY_SIZE(current->thread.user_simd));
	current->thread.user_simd_valid = 1;
	current->thread.user_fpcr = BIT(22) | BIT(24);
	current->thread.user_fpsr = BIT(27) | BIT(4);
	orlix_tcti_advsimd_table_seed_regs(
		&regs, context->instructions, instruction);
	before_regs = regs;
	spec.source_ordinal = leaf->source_ordinal;
	spec.obligation = ORLIX_TCTI_NATIVE_OBLIGATION_REJECTED_ENCODINGS;
	spec.result = (struct orlix_tcti_result) {
		.reason = ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
		.status = -EOPNOTSUPP,
		.fault_access = ORLIX_TCTI_ACCESS_FETCH,
		.pc = context->instructions,
		.instruction = instruction,
	};
	orlix_tcti_native_gpr_capture(&spec.gpr, &before_regs);
	orlix_tcti_advsimd_table_capture_fp_simd(
		&spec.expected.fp_simd, before_simd, BIT(22) | BIT(24),
		BIT(27) | BIT(4), true);
	observation = orlix_tcti_native_observation_create(&spec);
	KUNIT_ASSERT_NOT_NULL(test, observation);
	ret = orlix_tcti_native_observation_execute(
		observation, current, &regs, current->mm);
	KUNIT_ASSERT_EQ(test, 0, ret);
	orlix_tcti_advsimd_table_capture_fp_simd(
		&observed_fp_simd, current->thread.user_simd,
		current->thread.user_fpcr, current->thread.user_fpsr,
		current->thread.user_simd_valid);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_native_observation_add_fp_simd(
			observation, &observed_fp_simd));
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_native_observation_compare(observation));
	KUNIT_ASSERT_EQ(test, ORLIX_TCTI_NATIVE_OBSERVATION_MATCH,
		orlix_tcti_native_observation_state(observation));
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_native_observation_export(observation, record));
	KUNIT_EXPECT_MEMEQ(test, &before_regs, &regs, sizeof(regs));
	KUNIT_EXPECT_MEMEQ(test, before_simd, current->thread.user_simd,
		sizeof(*before_simd) * ARRAY_SIZE(current->thread.user_simd));
	orlix_tcti_native_observation_destroy(observation);
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
		const char *name = orlix_tcti_advsimd_table_artifact_string(
			artifact, source->name_offset);
		const char *mnemonic = orlix_tcti_advsimd_table_artifact_string(
			artifact, source->mnemonic_offset);
		const char *operation = orlix_tcti_advsimd_table_artifact_string(
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
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ADVSIMD_TABLE_VARIABLE_MASK,
				~source->encoding_mask);
	}
}

static void orlix_tcti_advsimd_table_legal_fields_decode(struct kunit *test)
{
	size_t leaf_index;
	u8 q;
	u8 reg;

	for (leaf_index = 0; leaf_index < ARRAY_SIZE(orlix_tcti_advsimd_table_leaves);
	     leaf_index++) {
		const struct orlix_tcti_advsimd_table_leaf *leaf =
			&orlix_tcti_advsimd_table_leaves[leaf_index];

		for (q = 0; q < 2; q++) {
			for (reg = 0; reg < 32U; reg++) {
				const struct orlix_tcti_advsimd_table_registers fields[] = {
					{ reg, 30U, 29U },
					{ 3U, reg, 29U },
					{ 3U, 30U, reg },
				};
				size_t field_index;

				for (field_index = 0;
				     field_index < ARRAY_SIZE(fields); field_index++) {
					const struct orlix_tcti_advsimd_table_registers *fields_case =
						&fields[field_index];
					struct orlix_tcti_decoded_instruction decoded =
						orlix_tcti_decode_aarch64(
							orlix_tcti_advsimd_table_instruction(
								leaf, q, fields_case->rd,
								fields_case->rn,
								fields_case->rm));

					KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_SIMD_TABLE_LOOKUP,
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

static void orlix_tcti_advsimd_table_source_leaves_resume(struct kunit *test)
{
	struct orlix_tcti_advsimd_table_context *context = test->priv;
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

	for (leaf_index = 0; leaf_index < ARRAY_SIZE(orlix_tcti_advsimd_table_leaves);
	     leaf_index++) {
		const struct orlix_tcti_advsimd_table_leaf *leaf =
			&orlix_tcti_advsimd_table_leaves[leaf_index];

		for (q = 0; q < 2; q++) {
			size_t case_index;

			for (case_index = 0;
			     case_index < ARRAY_SIZE(orlix_tcti_advsimd_table_overlap_cases);
			     case_index++) {
				const struct orlix_tcti_advsimd_table_registers *registers =
					&orlix_tcti_advsimd_table_overlap_cases[case_index];
				u8 result_size = q ? 16U : 8U;
				u32 instruction = orlix_tcti_advsimd_table_instruction(
					leaf, q, registers->rd, registers->rn,
					registers->rm);
				const u32 expected_program[] = {
					instruction, ORLIX_TCTI_ADVSIMD_TABLE_SVC,
				};
				u32 before_program[ARRAY_SIZE(expected_program)];
				u32 after_program[ARRAY_SIZE(expected_program)];
				struct pt_regs regs;
				struct pt_regs expected_regs;
				struct orlix_tcti_result result;
				int ret;

				ret = orlix_tcti_advsimd_table_load_instruction(test, instruction);
				KUNIT_ASSERT_EQ(test, 0, ret);
				ret = orlix_tcti_read_user_data(current->mm,
					context->instructions, before_program,
					sizeof(before_program));
				KUNIT_ASSERT_EQ(test, 0, ret);
				KUNIT_EXPECT_MEMEQ(test, expected_program, before_program,
						   sizeof(expected_program));

				orlix_tcti_advsimd_table_seed_simd(leaf, registers);
				memcpy(before_simd, current->thread.user_simd,
				       sizeof(*before_simd) *
				       ARRAY_SIZE(current->thread.user_simd));
				orlix_tcti_advsimd_table_expected(expected_simd, before_simd,
					leaf, registers, result_size);
				current->thread.user_simd_valid = 0;
				current->thread.user_fpcr = BIT(22) | BIT(24);
				current->thread.user_fpsr = BIT(27) | BIT(4);
				orlix_tcti_advsimd_table_seed_regs(&regs, context->instructions,
					instruction);
				expected_regs = regs;
				expected_regs.pc += sizeof(u32);

				result = orlix_tcti_resume_user(current, &regs, current->mm);

				KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason,
					"%s q=%u case=%zu", leaf->source_name, q,
					case_index);
				KUNIT_EXPECT_EQ(test, 0L, result.status);
				KUNIT_EXPECT_EQ(test, 0UL, result.fault_address);
				KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_FETCH,
						result.fault_access);
				KUNIT_EXPECT_EQ(test, context->instructions + sizeof(u32),
						result.pc);
				KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ADVSIMD_TABLE_SVC,
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
				ret = orlix_tcti_read_user_data(current->mm,
					context->instructions, after_program,
					sizeof(after_program));
				KUNIT_EXPECT_EQ(test, 0, ret);
				KUNIT_EXPECT_MEMEQ(test, expected_program, after_program,
						   sizeof(expected_program));
			}
		}
	}
}

static void orlix_tcti_advsimd_table_typed_obligations_resume(
	struct kunit *test)
{
	struct orlix_tcti_target_native_result_record *records[
		ARRAY_SIZE(orlix_tcti_advsimd_table_leaves) *
		ARRAY_SIZE(orlix_tcti_advsimd_table_typed_obligations)] = {};
	struct orlix_tcti_target_proof_ingestion_ledger *ledger;
	struct orlix_tcti_target_proof_ingestion_summary summary;
	u32 covered = 0;
	size_t record_count = 0;
	size_t leaf_index;

	ledger = orlix_tcti_target_proof_ingestion_ledger_create(
		ARRAY_SIZE(records));
	KUNIT_ASSERT_NOT_NULL(test, ledger);
	for (leaf_index = 0;
	     leaf_index < ARRAY_SIZE(orlix_tcti_advsimd_table_leaves);
	     leaf_index++) {
		const struct orlix_tcti_advsimd_table_leaf *leaf =
			&orlix_tcti_advsimd_table_leaves[leaf_index];
		size_t obligation_index;

		for (obligation_index = 0;
		     obligation_index <
			ARRAY_SIZE(orlix_tcti_advsimd_table_typed_obligations);
		     obligation_index++) {
			const struct orlix_tcti_advsimd_table_typed_obligation *typed =
				&orlix_tcti_advsimd_table_typed_obligations[
					obligation_index];

			orlix_tcti_advsimd_table_observe_legal(
				test, leaf, typed->native, &records[record_count]);
			KUNIT_ASSERT_NOT_NULL(test, records[record_count]);
			orlix_tcti_advsimd_table_ingest_record(
				test, leaf, ORLIX_TCTI_ADVSIMD_TABLE_TYPED_CASE,
				records[record_count], ledger);
			covered |= typed->proof;
			record_count++;
		}
	}
	KUNIT_EXPECT_EQ(test,
		ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
		ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC |
		ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS,
		covered);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_target_proof_ingestion_summary(ledger, &summary));
	KUNIT_EXPECT_EQ(test, ARRAY_SIZE(records), summary.accepted_records);
	KUNIT_EXPECT_EQ(test, ARRAY_SIZE(records), summary.native_passed);
	KUNIT_EXPECT_EQ(test, 0U, summary.kselftest_passed);
	KUNIT_EXPECT_EQ(test, 0U, summary.rejected);
	while (record_count)
		orlix_tcti_target_native_result_record_destroy(
			records[--record_count]);
	orlix_tcti_target_proof_ingestion_ledger_destroy(ledger);
}

static void orlix_tcti_advsimd_table_fixed_bit_neighbors_resume(
	struct kunit *test)
{
	const struct orlix_tcti_target_instruction_artifact *artifact =
		orlix_tcti_target_instruction_artifact_canonical();
	struct orlix_tcti_target_instruction_artifact_validation_result validation;
	struct orlix_tcti_target_native_result_record *records[
		ARRAY_SIZE(orlix_tcti_advsimd_table_leaves)] = {};
	struct orlix_tcti_target_proof_ingestion_ledger *ledger;
	struct orlix_tcti_target_proof_ingestion_summary summary;
	size_t record_count = 0;
	size_t leaf_index;

	KUNIT_ASSERT_NOT_NULL(test, artifact);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_target_instruction_artifact_validate(
			artifact, &validation));
	ledger = orlix_tcti_target_proof_ingestion_ledger_create(
		ARRAY_SIZE(records));
	KUNIT_ASSERT_NOT_NULL(test, ledger);
	for (leaf_index = 0;
	     leaf_index < ARRAY_SIZE(orlix_tcti_advsimd_table_leaves);
	     leaf_index++) {
		const struct orlix_tcti_advsimd_table_leaf *leaf =
			&orlix_tcti_advsimd_table_leaves[leaf_index];
		const struct orlix_tcti_advsimd_table_registers registers = {
			4U, 20U, 6U,
		};
		u32 canonical = orlix_tcti_advsimd_table_instruction(
			leaf, 1U, registers.rd, registers.rn, registers.rm);
		u32 fixed_mutations = 0;
		u32 union_neighbors = 0;
		u32 owned_neighbors = 0;
		u32 rejected_neighbors = 0;
		bool exported = false;
		u8 bit;

		for (bit = 0; bit < 32U; bit++) {
			const struct orlix_tcti_advsimd_table_leaf *union_leaf;
			struct orlix_tcti_target_native_result_record *record = NULL;
			struct orlix_tcti_decoded_instruction decoded;
			size_t owners;
			u32 instruction;

			if (!(leaf->source_mask & BIT(bit)))
				continue;
			fixed_mutations++;
			instruction = canonical ^ BIT(bit);
			union_leaf = orlix_tcti_advsimd_table_union_leaf(instruction);
			owners = orlix_tcti_advsimd_table_source_owner_count(
				artifact, instruction);
			KUNIT_ASSERT_NE(test, SIZE_MAX, owners);
			decoded = orlix_tcti_decode_aarch64(instruction);
			if (union_leaf) {
				union_neighbors++;
				KUNIT_EXPECT_GT(test, owners, 0U);
				KUNIT_EXPECT_EQ(test,
					ORLIX_TCTI_DECODE_SIMD_TABLE_LOOKUP,
					decoded.decode_class);
				KUNIT_EXPECT_EQ(test, union_leaf->operation,
					decoded.simd_table_lookup_op);
				KUNIT_EXPECT_EQ(test, union_leaf->table_count,
					decoded.simd_table_count);
				continue;
			}
			if (owners) {
				owned_neighbors++;
				KUNIT_EXPECT_NE(test,
					ORLIX_TCTI_DECODE_SIMD_TABLE_LOOKUP,
					decoded.decode_class);
				continue;
			}
			rejected_neighbors++;
			KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
				decoded.decode_class);
			orlix_tcti_advsimd_table_observe_rejection(
				test, leaf, instruction, &record);
			KUNIT_ASSERT_NOT_NULL(test, record);
			if (!exported) {
				records[record_count] = record;
				orlix_tcti_advsimd_table_ingest_record(
					test, leaf,
					ORLIX_TCTI_ADVSIMD_TABLE_REJECTION_CASE,
					records[record_count], ledger);
				record_count++;
				exported = true;
			} else {
				orlix_tcti_target_native_result_record_destroy(record);
			}
		}
		KUNIT_EXPECT_EQ(test, 16U, fixed_mutations);
		KUNIT_EXPECT_EQ(test, 3U, union_neighbors);
		KUNIT_EXPECT_GT(test, owned_neighbors, 0U);
		KUNIT_EXPECT_GT(test, rejected_neighbors, 0U);
		KUNIT_EXPECT_EQ(test, fixed_mutations,
			union_neighbors + owned_neighbors + rejected_neighbors);
		KUNIT_EXPECT_TRUE(test, exported);
	}
	KUNIT_EXPECT_EQ(test, ARRAY_SIZE(records), record_count);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_target_proof_ingestion_summary(ledger, &summary));
	KUNIT_EXPECT_EQ(test, ARRAY_SIZE(records), summary.accepted_records);
	KUNIT_EXPECT_EQ(test, ARRAY_SIZE(records), summary.native_passed);
	KUNIT_EXPECT_EQ(test, 0U, summary.kselftest_passed);
	KUNIT_EXPECT_EQ(test, 0U, summary.rejected);
	while (record_count)
		orlix_tcti_target_native_result_record_destroy(
			records[--record_count]);
	orlix_tcti_target_proof_ingestion_ledger_destroy(ledger);
}

static struct kunit_case orlix_tcti_advsimd_table_source_bound_cases[] = {
	KUNIT_CASE(orlix_tcti_advsimd_table_source_bindings),
	KUNIT_CASE(orlix_tcti_advsimd_table_legal_fields_decode),
	KUNIT_CASE(orlix_tcti_advsimd_table_source_leaves_resume),
	KUNIT_CASE(orlix_tcti_advsimd_table_typed_obligations_resume),
	KUNIT_CASE(orlix_tcti_advsimd_table_fixed_bit_neighbors_resume),
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
