// SPDX-License-Identifier: GPL-2.0-only
/*
 * Source-bound inventory and production-decoder evidence for GitHub issue
 * #134.  The checked execution-slice map owns membership; this suite does not
 * maintain a second 209-entry list.
 */
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>
#include <linux/unaligned.h>
#include <kunit/test.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/syscalls.h>

#include "../decode_aarch64.h"
#include "target_execution_slice_map.h"
#include "target_instruction_artifact.h"

#define BLS_ISSUE_ID 134U
#define BLS_LEAF_COUNT 209U
#define BLS_SVC 0xd4000001U

struct bls_context {
	u64 simd[ARRAY_SIZE(current->thread.user_simd)];
	unsigned long simd_valid;
	unsigned long fpcr;
	unsigned long fpsr;
};

struct bls_source_leaf {
	u32 ordinal;
	const char *name;
	const char *mnemonic;
	const char *operation;
	u32 mask;
	u32 pattern;
	const char *feature_predicate;
};

#define ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, id, mnemonic, operation, mask, \
					     pattern, feature_predicate, offset, length) \
	{ ordinal, id, mnemonic, operation, mask, pattern, feature_predicate },
static const struct bls_source_leaf bls_source_leaves[] = {
#include "../isa/source_manifest.def"
};
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE

static const struct bls_source_leaf *bls_source_leaf(u32 ordinal)
{
	if (ordinal >= ARRAY_SIZE(bls_source_leaves) ||
	    bls_source_leaves[ordinal].ordinal != ordinal)
		return NULL;
	return &bls_source_leaves[ordinal];
}

static enum orlix_tcti_decode_class bls_expected_class(u32 ordinal)
{
	if (ordinal >= 2565U && ordinal <= 2566U)
		return ORLIX_TCTI_DECODE_GCS_STORE;
	if (ordinal >= 2697U && ordinal <= 2703U)
		return ORLIX_TCTI_DECODE_LOAD_LITERAL;
	if (ordinal >= 2856U && ordinal <= 2917U)
		return ORLIX_TCTI_DECODE_LOAD_STORE_PAIR;
	if (ordinal >= 2918U && ordinal <= 3000U)
		return ORLIX_TCTI_DECODE_LOAD_STORE_SIGNED_IMMEDIATE;
	if (ordinal >= 3297U && ordinal <= 3327U)
		return ORLIX_TCTI_DECODE_LOAD_STORE_REGISTER_OFFSET;
	if (ordinal >= 3332U && ordinal <= 3355U)
		return ORLIX_TCTI_DECODE_LOAD_STORE_UNSIGNED_IMMEDIATE;
	return ORLIX_TCTI_DECODE_UNSUPPORTED;
}

static u32 bls_legal_instruction(const struct bls_source_leaf *leaf)
{
	u32 instruction = leaf->pattern;

	switch (bls_expected_class(leaf->ordinal)) {
	case ORLIX_TCTI_DECODE_GCS_STORE:
		/* Rt=x0, Rn=x1. */
		return instruction | BIT(5);
	case ORLIX_TCTI_DECODE_LOAD_LITERAL:
		/* imm19=2, Rt=0. */
		return instruction | (2U << 5);
	case ORLIX_TCTI_DECODE_LOAD_STORE_PAIR:
		/* imm7=1, Rt=x0/v0, Rt2=x2/v2, Rn=x10. */
		return instruction | BIT(15) | (2U << 10) | (10U << 5);
	case ORLIX_TCTI_DECODE_LOAD_STORE_REGISTER_OFFSET:
		/* Rm=x2, option=LSL, S=0, Rn=x1, Rt=x0/v0. */
		return instruction | (2U << 16) | (3U << 13) | BIT(5);
	case ORLIX_TCTI_DECODE_LOAD_STORE_SIGNED_IMMEDIATE:
	case ORLIX_TCTI_DECODE_LOAD_STORE_UNSIGNED_IMMEDIATE:
		/* Rn=x1, Rt=x0/v0. */
		return instruction | BIT(5);
	default:
		return instruction;
	}
}

static int bls_test_init(struct kunit *test)
{
	struct bls_context *context = kunit_kzalloc(test, sizeof(*context), GFP_KERNEL);

	if (!context)
		return -ENOMEM;
	memcpy(context->simd, current->thread.user_simd, sizeof(context->simd));
	context->simd_valid = current->thread.user_simd_valid;
	context->fpcr = current->thread.user_fpcr;
	context->fpsr = current->thread.user_fpsr;
	test->priv = context;
	return 0;
}

static void bls_test_exit(struct kunit *test)
{
	struct bls_context *context = test->priv;

	memcpy(current->thread.user_simd, context->simd, sizeof(context->simd));
	current->thread.user_simd_valid = context->simd_valid;
	current->thread.user_fpcr = context->fpcr;
	current->thread.user_fpsr = context->fpsr;
}

static unsigned long bls_map(struct kunit *test, size_t size, unsigned long prot)
{
	unsigned long mapped = ksys_mmap_pgoff(0, size, prot,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	return mapped;
}

static unsigned long bls_effective_address(
	const struct orlix_tcti_decoded_instruction *decoded,
	const struct pt_regs *regs, unsigned long pc)
{
	if (decoded->decode_class == ORLIX_TCTI_DECODE_LOAD_LITERAL)
		return pc + decoded->memory_offset;
	if (decoded->decode_class == ORLIX_TCTI_DECODE_GCS_STORE)
		return decoded->rn == 31 ? regs->sp : regs->regs[decoded->rn];
	if (decoded->decode_class == ORLIX_TCTI_DECODE_LOAD_STORE_REGISTER_OFFSET)
		return decoded->rn == 31 ? regs->sp : regs->regs[decoded->rn];
	if (decoded->memory_index_mode == ORLIX_TCTI_MEMORY_INDEX_POST)
		return decoded->rn == 31 ? regs->sp : regs->regs[decoded->rn];
	return (decoded->rn == 31 ? regs->sp : regs->regs[decoded->rn]) +
		decoded->memory_offset;
}

static void bls_fill_bytes(u8 *bytes, size_t size, u32 ordinal)
{
	size_t index;

	for (index = 0; index < size; index++)
		bytes[index] = (u8)(ordinal * 17U + index * 29U + 3U);
}

static unsigned long bls_map_program(struct kunit *test, u32 instruction)
{
	const u32 program[] = { instruction, BLS_SVC };
	unsigned long text = bls_map(test, PAGE_SIZE, PROT_READ | PROT_WRITE);
	int ret;

	ret = orlix_tcti_write_user_data(current->mm, text, program,
					  sizeof(program));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = sys_mprotect(text, PAGE_SIZE, PROT_READ | PROT_EXEC);
	KUNIT_ASSERT_EQ(test, 0, ret);
	return text;
}

static void bls_inventory_is_exact_and_source_bound(struct kunit *test)
{
	const struct orlix_tcti_execution_slice_map *map =
		orlix_tcti_execution_slice_map_canonical();
	const struct orlix_tcti_target_instruction_artifact *artifact =
		orlix_tcti_target_instruction_artifact_canonical();
	struct orlix_tcti_execution_slice_map_validation_result map_validation;
	struct orlix_tcti_target_instruction_artifact_validation_result artifact_validation;
	size_t count = 0;
	size_t index;

	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_execution_slice_map_validate(map, &map_validation));
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_target_instruction_artifact_validate(
			artifact, &artifact_validation));
	for (index = 0; index < map->counts.leaf_count; index++) {
		const struct orlix_tcti_execution_slice_member *member =
			&map->members[index];
		const struct orlix_tcti_execution_slice_family *family =
			&map->families[member->family_index];
		const struct bls_source_leaf *leaf;
		const struct orlix_tcti_target_instruction_artifact_leaf *canonical;

		if (family->issue_id != BLS_ISSUE_ID)
			continue;
		count++;
		leaf = bls_source_leaf(member->ordinal);
		KUNIT_ASSERT_NOT_NULL(test, leaf);
		canonical = &artifact->leaves[member->ordinal];
		KUNIT_EXPECT_STREQ(test, member->source_name, leaf->name);
		KUNIT_EXPECT_EQ(test, leaf->mask, canonical->encoding_mask);
		KUNIT_EXPECT_EQ(test, leaf->pattern, canonical->encoding_pattern);
		KUNIT_EXPECT_NE_MSG(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
			bls_expected_class(member->ordinal), "%u %s outside #134 ranges",
			member->ordinal, member->source_name);
		KUNIT_EXPECT_TRUE_MSG(test, member->condition_tcnd_hex[0] != '\0',
			"%u %s lost applicability", member->ordinal,
			member->source_name);
	}
	KUNIT_EXPECT_EQ(test, (size_t)BLS_LEAF_COUNT, count);
}

static void bls_every_source_leaf_reaches_its_production_decoder(
	struct kunit *test)
{
	const struct orlix_tcti_execution_slice_map *map =
		orlix_tcti_execution_slice_map_canonical();
	size_t index;

	for (index = 0; index < map->counts.leaf_count; index++) {
		const struct orlix_tcti_execution_slice_member *member =
			&map->members[index];
		const struct orlix_tcti_execution_slice_family *family =
			&map->families[member->family_index];
		const struct bls_source_leaf *leaf;
		struct orlix_tcti_decoded_instruction decoded;
		u32 instruction;

		if (family->issue_id != BLS_ISSUE_ID)
			continue;
		leaf = bls_source_leaf(member->ordinal);
		KUNIT_ASSERT_NOT_NULL(test, leaf);
		instruction = bls_legal_instruction(leaf);
		KUNIT_EXPECT_EQ_MSG(test, leaf->pattern, instruction & leaf->mask,
			"%u %s source encoding", leaf->ordinal, leaf->name);
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_EXPECT_EQ_MSG(test, bls_expected_class(leaf->ordinal),
			decoded.decode_class, "%u %s", leaf->ordinal, leaf->name);
	}
}

static void bls_every_source_leaf_executes_through_resume(struct kunit *test)
{
	const struct orlix_tcti_execution_slice_map *map =
		orlix_tcti_execution_slice_map_canonical();
	unsigned long data = bls_map(test, PAGE_SIZE, PROT_READ | PROT_WRITE);
	size_t index;
	int ret;

	ret = orlix_tcti_set_gcs_memory(current->mm, data, PAGE_SIZE, true);
	KUNIT_ASSERT_EQ(test, 0, ret);

	for (index = 0; index < map->counts.leaf_count; index++) {
		const struct orlix_tcti_execution_slice_member *member =
			&map->members[index];
		const struct orlix_tcti_execution_slice_family *family =
			&map->families[member->family_index];
		const struct bls_source_leaf *leaf;
		struct orlix_tcti_decoded_instruction decoded;
		struct orlix_tcti_result result;
		struct pt_regs regs = {};
		struct pt_regs before;
		u64 simd_before[ARRAY_SIZE(current->thread.user_simd)];
		u8 expected[2 * sizeof(u64)] = {};
		u8 observed[2 * sizeof(u64)] = {};
		unsigned long text;
		unsigned long address;
		u32 program[6] = {};
		u32 instruction;
		size_t transfer_size;
		int ret;

		if (family->issue_id != BLS_ISSUE_ID)
			continue;
		leaf = bls_source_leaf(member->ordinal);
		KUNIT_ASSERT_NOT_NULL(test, leaf);
		instruction = bls_legal_instruction(leaf);
		if (bls_expected_class(leaf->ordinal) ==
		    ORLIX_TCTI_DECODE_LOAD_LITERAL)
			instruction = leaf->pattern | (4U << 5);
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_ASSERT_EQ_MSG(test, bls_expected_class(leaf->ordinal),
			decoded.decode_class, "%u %s", leaf->ordinal, leaf->name);

		text = bls_map(test, PAGE_SIZE, PROT_READ | PROT_WRITE);
		program[0] = instruction;
		program[1] = BLS_SVC;
		ret = orlix_tcti_write_user_data(current->mm, text, program,
				sizeof(program));
		KUNIT_ASSERT_EQ(test, 0, ret);
		memset(&regs, 0, sizeof(regs));
		regs.pc = text;
		regs.sp = ALIGN(data + 512, 16);
		regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT;
		regs.syscallno = NO_SYSCALL;
		regs.regs[0] = 0x8877665544332211ULL ^ leaf->ordinal;
		regs.regs[1] = data + 256;
		regs.regs[2] = 0;
		regs.regs[10] = data + 256;
		memset(current->thread.user_simd, 0,
		       sizeof(current->thread.user_simd));
		current->thread.user_simd[0] =
			0x0123456789abcdefULL ^ leaf->ordinal;
		current->thread.user_simd[1] =
			0xfedcba9876543210ULL ^ leaf->ordinal;
		current->thread.user_simd[4] =
			0x1122334455667788ULL ^ leaf->ordinal;
		current->thread.user_simd[5] =
			0x8877665544332211ULL ^ leaf->ordinal;
		current->thread.user_simd_valid = 1;
		address = bls_effective_address(&decoded, &regs, text);
		transfer_size = decoded.access_size;
		if (decoded.decode_class == ORLIX_TCTI_DECODE_LOAD_STORE_PAIR)
			transfer_size *= 2;
		if (decoded.prefetch)
			transfer_size = sizeof(expected);
		KUNIT_ASSERT_LE(test, transfer_size, sizeof(expected));
		bls_fill_bytes(expected, transfer_size, leaf->ordinal);
		if (decoded.load && !decoded.prefetch) {
			ret = orlix_tcti_write_user_data(current->mm, address,
				expected, transfer_size);
			KUNIT_ASSERT_EQ(test, 0, ret);
		} else if (decoded.prefetch) {
			ret = orlix_tcti_write_user_data(current->mm, data + 256,
				expected, transfer_size);
			KUNIT_ASSERT_EQ(test, 0, ret);
		} else {
			memset(expected, 0, transfer_size);
			ret = orlix_tcti_write_user_data(current->mm, address,
				expected, transfer_size);
			KUNIT_ASSERT_EQ(test, 0, ret);
		}
		if (decoded.decode_class == ORLIX_TCTI_DECODE_LOAD_LITERAL &&
		    decoded.load && !decoded.prefetch) {
			ret = orlix_tcti_write_user_data(current->mm, address,
				expected, transfer_size);
			KUNIT_ASSERT_EQ(test, 0, ret);
		}
		ret = sys_mprotect(text, PAGE_SIZE, PROT_READ | PROT_EXEC);
		KUNIT_ASSERT_EQ(test, 0, ret);
		before = regs;
		memcpy(simd_before, current->thread.user_simd, sizeof(simd_before));
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason,
			"%u %s", leaf->ordinal, leaf->name);
		KUNIT_EXPECT_EQ_MSG(test, 0L, result.status, "%u %s",
			leaf->ordinal, leaf->name);
		KUNIT_EXPECT_EQ(test, BLS_SVC, result.instruction);
		KUNIT_EXPECT_EQ(test, text + 2 * sizeof(u32), regs.pc);
		KUNIT_EXPECT_EQ(test, before.pstate, regs.pstate);

		if (decoded.prefetch) {
			ret = orlix_tcti_read_user_data(current->mm, data + 256,
				observed, transfer_size);
			KUNIT_ASSERT_EQ(test, 0, ret);
			KUNIT_EXPECT_MEMEQ_MSG(test, expected, observed, transfer_size,
				"%u %s prefetch changed memory", leaf->ordinal,
				leaf->name);
			KUNIT_EXPECT_MEMEQ(test, before.regs, regs.regs,
					   sizeof(regs.regs));
			KUNIT_EXPECT_MEMEQ(test, simd_before,
					   current->thread.user_simd,
					   sizeof(simd_before));
		} else if (!decoded.load) {
			ret = orlix_tcti_read_user_data(current->mm, address,
				observed, transfer_size);
			KUNIT_ASSERT_EQ(test, 0, ret);
			KUNIT_EXPECT_MEMNEQ_MSG(test, expected, observed, transfer_size,
				"%u %s did not store", leaf->ordinal, leaf->name);
		} else if (decoded.simd_fp) {
			KUNIT_EXPECT_EQ_MSG(test, get_unaligned_le64(expected),
				current->thread.user_simd[decoded.rt * 2],
				"%u %s SIMD low", leaf->ordinal, leaf->name);
		} else {
			u64 value = 0;

			memcpy(&value, expected,
			       min_t(size_t, decoded.access_size, sizeof(value)));
			if (decoded.sign_extend_load)
				value = sign_extend64(value,
					decoded.access_size * 8 - 1);
			else if (decoded.result_size == sizeof(u32))
				value = (u32)value;
			KUNIT_EXPECT_EQ_MSG(test, value, regs.regs[decoded.rt],
				"%u %s GPR", leaf->ordinal, leaf->name);
		}
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));
	}
	KUNIT_EXPECT_EQ(test, 0,
		orlix_tcti_set_gcs_memory(current->mm, data, PAGE_SIZE, false));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
}

static void bls_reserved_and_invalid_encodings_fail_closed(struct kunit *test)
{
	static const u32 invalid[] = {
		0xdc000000U, /* SIMD literal opc=11 is reserved. */
		0xf8800420U, /* PRFUM class with post-index is reserved. */
		0xf8c00420U, /* signed-immediate size=11 opc=11 is reserved. */
		0x38200820U, /* register offset option=000 is reserved. */
		0x38201820U, /* register offset option=000 with S=1 is reserved. */
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(invalid); index++)
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
			orlix_tcti_decode_aarch64(invalid[index]).decode_class,
			"reserved encoding 0x%08x", invalid[index]);
}

static void bls_gcs_permissions_and_stgp_tag_are_architectural_state(
	struct kunit *test)
{
	unsigned long data = bls_map(test, PAGE_SIZE, PROT_READ | PROT_WRITE);
	unsigned long text = bls_map_program(test, 0xd91f0c20U);
	struct pt_regs regs = {};
	struct orlix_tcti_result result;
	u64 observed[2] = {};
	u8 tag = 0;
	int ret;

	regs.pc = text;
	regs.pstate = PSR_MODE_EL0t | PSR_N_BIT;
	regs.syscallno = NO_SYSCALL;
	regs.regs[0] = 0x0123456789abcdefULL;
	regs.regs[1] = data;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_USER_FAULT, result.reason);
	KUNIT_EXPECT_EQ(test, -EACCES, result.status);
	KUNIT_EXPECT_EQ(test, data, result.fault_address);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_WRITE, result.fault_access);
	KUNIT_EXPECT_EQ(test, text, regs.pc);

	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_set_gcs_memory(current->mm, data, 16, true));
	regs.pc = text;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0L, result.status);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_read_user_data(current->mm, data,
							 observed, sizeof(u64)));
	KUNIT_EXPECT_EQ(test, 0x0123456789abcdefULL, observed[0]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));

	text = bls_map_program(test, 0x69000820U); /* STGP x0, x2, [x1] */
	memset(&regs, 0, sizeof(regs));
	regs.pc = text;
	regs.pstate = PSR_MODE_EL0t | PSR_C_BIT;
	regs.syscallno = NO_SYSCALL;
	regs.regs[0] = 0x8877665544332211ULL;
	regs.regs[1] = data | (0xaULL << 56);
	regs.regs[2] = 0x1020304050607080ULL;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0L, result.status);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_read_user_data(current->mm, data,
							 observed, sizeof(observed)));
	KUNIT_EXPECT_EQ(test, 0x8877665544332211ULL, observed[0]);
	KUNIT_EXPECT_EQ(test, 0x1020304050607080ULL, observed[1]);
	ret = orlix_tcti_load_allocation_tag(current->mm, data, &tag);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, (u8)0xa, tag);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0,
		orlix_tcti_set_gcs_memory(current->mm, data, 16, false));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
}

static void bls_prefetch_never_reads_the_target(struct kunit *test)
{
	static const u32 instructions[] = {
		0xf9800020U, /* PRFM pldl1keep, [x1] */
		0xf8a24838U, /* RPRFM with Rm=x2 and Rn=x1 */
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(instructions); index++) {
		unsigned long text = bls_map_program(test, instructions[index]);
		struct pt_regs regs = {};
		struct pt_regs before;
		struct orlix_tcti_result result;

		regs.pc = text;
		regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_V_BIT;
		regs.syscallno = NO_SYSCALL;
		regs.regs[1] = TASK_SIZE + PAGE_SIZE;
		regs.regs[2] = U64_MAX;
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
		KUNIT_EXPECT_EQ(test, 0L, result.status);
		KUNIT_EXPECT_MEMEQ(test, before.regs, regs.regs,
				   sizeof(regs.regs));
		KUNIT_EXPECT_EQ(test, before.pstate, regs.pstate);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));
	}
}

static void bls_non_el0_and_sp_alignment_fail_closed(struct kunit *test)
{
	static const u32 class_representatives[] = {
		0xd91f0c20U, 0x58000040U, 0xa9000940U,
		0xf9000020U, 0xf8400020U, 0xf8626820U,
	};
	static const u32 sp_instructions[] = {
		0xd91f0fe0U, 0xa90007e0U, 0xf90003e0U, 0xf98003e0U,
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(class_representatives); index++) {
		unsigned long text = bls_map_program(test,
			class_representatives[index]);
		struct pt_regs regs = {};
		struct pt_regs before;
		struct orlix_tcti_result result;

		regs.pc = text;
		regs.sp = 0x1000;
		regs.pstate = PSR_MODE_EL1h | PSR_N_BIT;
		regs.regs[0] = 0x123456789abcdef0ULL;
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				result.reason);
		KUNIT_EXPECT_EQ(test, -EPERM, result.status);
		KUNIT_EXPECT_EQ(test, text, result.pc);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));
	}

	for (index = 0; index < ARRAY_SIZE(sp_instructions); index++) {
		unsigned long text = bls_map_program(test, sp_instructions[index]);
		struct pt_regs regs = {};
		struct orlix_tcti_result result;

		regs.pc = text;
		regs.sp = 0x1001;
		regs.pstate = PSR_MODE_EL0t;
		regs.syscallno = NO_SYSCALL;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_ALIGNMENT_FAULT,
				result.reason);
		KUNIT_EXPECT_EQ(test, -EFAULT, result.status);
		KUNIT_EXPECT_EQ(test, 0x1001UL, result.fault_address);
		KUNIT_EXPECT_EQ(test, text, regs.pc);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));
	}
}

static struct kunit_case bls_cases[] = {
	KUNIT_CASE(bls_inventory_is_exact_and_source_bound),
	KUNIT_CASE(bls_every_source_leaf_reaches_its_production_decoder),
	KUNIT_CASE(bls_every_source_leaf_executes_through_resume),
	KUNIT_CASE(bls_reserved_and_invalid_encodings_fail_closed),
	KUNIT_CASE(bls_gcs_permissions_and_stgp_tag_are_architectural_state),
	KUNIT_CASE(bls_prefetch_never_reads_the_target),
	KUNIT_CASE(bls_non_el0_and_sp_alignment_fail_closed),
	{}
};

static struct kunit_suite bls_suite = {
	.name = "orlix-tcti-base-load-store-source-bound",
	.init = bls_test_init,
	.exit = bls_test_exit,
	.test_cases = bls_cases,
};

kunit_test_suite(bls_suite);

MODULE_LICENSE("GPL");
