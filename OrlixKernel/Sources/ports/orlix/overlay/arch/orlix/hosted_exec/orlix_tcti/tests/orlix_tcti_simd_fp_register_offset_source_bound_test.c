// SPDX-License-Identifier: GPL-2.0-only
/*
 * Production-path evidence for the AArch64 SIMD and FP register-offset
 * load/store cohort.  Each leaf is bound to the pinned AARCHMRS 2026-06
 * source manifest and to the canonical checked instruction artifact.  The
 * byte forms are two source-conditioned alternatives: B permits every legal
 * extend other than LSL, while BL is the LSL alternative.
 */
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>
#include <kunit/test.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/syscalls.h>
#include <linux/unaligned.h>

#include "target_instruction_artifact.h"
#include "../decode_aarch64.h"

#define SFRO_SVC 0xd4000001U

struct sfro_source_leaf {
	u32 ordinal;
	const char *name;
	const char *mnemonic;
	const char *operation;
	u32 mask;
	u32 pattern;
};

struct sfro_expectation {
	u32 ordinal;
	bool load;
	u8 access_size;
	bool lsl_only;
	bool non_lsl_only;
};

struct sfro_context {
	unsigned long simd[ARRAY_SIZE(current->thread.user_simd)];
	unsigned long simd_valid;
	unsigned long fpcr;
	unsigned long fpsr;
};

#define ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, id, mnemonic, operation, mask, \
					     pattern, feature_predicate, offset, length) \
	{ ordinal, id, mnemonic, operation, mask, pattern },
static const struct sfro_source_leaf sfro_source_leaves[] = {
#include "../isa/source_manifest.def"
};
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE

#define SFRO(ordinal, load, access, lsl, non_lsl) \
	{ ordinal##U, load, access, lsl, non_lsl }

static const struct sfro_expectation sfro_expected[] = {
	SFRO(3305, false, 1, false, true),
	SFRO(3306, false, 1, true, false),
	SFRO(3307, true,  1, false, true),
	SFRO(3308, true,  1, true, false),
	SFRO(3309, false, 16, false, false),
	SFRO(3310, true,  16, false, false),
	SFRO(3315, false, 2, false, false),
	SFRO(3316, true,  2, false, false),
	SFRO(3320, false, 4, false, false),
	SFRO(3321, true,  4, false, false),
	SFRO(3326, false, 8, false, false),
	SFRO(3327, true,  8, false, false),
};
#undef SFRO

static const struct sfro_source_leaf *sfro_source_leaf(u32 ordinal)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(sfro_source_leaves); index++)
		if (sfro_source_leaves[index].ordinal == ordinal)
			return &sfro_source_leaves[index];
	return NULL;
}

static int sfro_test_init(struct kunit *test)
{
	struct sfro_context *context;

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

static void sfro_test_exit(struct kunit *test)
{
	struct sfro_context *context = test->priv;

	memcpy(current->thread.user_simd, context->simd, sizeof(context->simd));
	current->thread.user_simd_valid = context->simd_valid;
	current->thread.user_fpcr = context->fpcr;
	current->thread.user_fpsr = context->fpsr;
}

static const char *sfro_artifact_string(
	const struct orlix_tcti_target_instruction_artifact *artifact, u32 offset)
{
	if (offset >= artifact->string_pool_size)
		return NULL;
	return (const char *)artifact->string_pool + offset;
}

static bool sfro_option_allowed(const struct sfro_expectation *expected,
				u8 option)
{
	if (expected->lsl_only)
		return option == 3;
	if (expected->non_lsl_only)
		return option == 2 || option == 6 || option == 7;
	return option == 2 || option == 3 || option == 6 || option == 7;
}

static u32 sfro_instruction(const struct sfro_source_leaf *leaf, u8 option,
				    bool shift, u8 rn, u8 rm, u8 rt)
{
	return leaf->pattern | ((u32)option << 13) | ((u32)shift << 12) |
		((u32)rm << 16) | ((u32)rn << 5) | rt;
}

static unsigned long sfro_map_instruction(struct kunit *test, u32 instruction)
{
	const u32 program[] = { instruction, SFRO_SVC };
	unsigned long mapped;
	int ret;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	ret = orlix_tcti_write_user_data(current->mm, mapped, program, sizeof(program));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = sys_mprotect(mapped, PAGE_SIZE, PROT_READ | PROT_EXEC);
	KUNIT_ASSERT_EQ(test, 0, ret);
	return mapped;
}

static unsigned long sfro_map_data(struct kunit *test, size_t bytes)
{
	unsigned long mapped;

	mapped = ksys_mmap_pgoff(0, bytes, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	return mapped;
}

static void sfro_seed(struct pt_regs *regs, u32 salt)
{
	size_t index;

	memset(regs, 0, sizeof(*regs));
	for (index = 0; index < ARRAY_SIZE(regs->regs); index++)
		regs->regs[index] = 0x9e3779b97f4a7c15ULL ^
			((u64)salt << 19) ^ index * 0x0101010101010101ULL;
	regs->sp = 0xfedcba9876543210ULL ^ salt;
	for (index = 0; index < ARRAY_SIZE(current->thread.user_simd); index++)
		current->thread.user_simd[index] = 0xd1b54a32d192ed03ULL ^
			((u64)salt << 23) ^ index * 0x102030405060708ULL;
	current->thread.user_simd_valid = 1;
	current->thread.user_fpcr = 0x00300000UL | (salt & 0x1fU);
	current->thread.user_fpsr = BIT(27) | BIT(4) | (salt & 0xfU);
}

static s64 sfro_offset(u8 option, bool shift, u8 access_size,
			const struct pt_regs *regs, u8 rm)
{
	u64 value = rm == 31 ? 0 : regs->regs[rm];

	switch (option) {
	case 2:
		value = (u32)value;
		break;
	case 3:
		break;
	case 6:
		value = (s64)(s32)value;
		break;
	case 7:
		value = (s64)value;
		break;
	default:
		return 0;
	}
	return shift ? (s64)(value << ilog2(access_size)) : (s64)value;
}

static void sfro_payload(u8 bytes[16], u32 salt)
{
	static const u8 seed[16] = {
		0x80, 0x01, 0xfe, 0x7f, 0x45, 0x23, 0xab, 0x89,
		0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe,
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(seed); index++)
		bytes[index] = seed[index] ^ (u8)(salt + index * 13U);
}

static void sfro_expect_success(struct kunit *test,
				const struct orlix_tcti_result *result,
				const struct pt_regs *regs, unsigned long text)
{
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result->reason);
	KUNIT_EXPECT_EQ(test, 0L, result->status);
	KUNIT_EXPECT_EQ(test, SFRO_SVC, result->instruction);
	KUNIT_EXPECT_EQ(test, 0UL, result->fault_address);
	KUNIT_EXPECT_EQ(test, text + 2 * sizeof(u32), result->pc);
	KUNIT_EXPECT_EQ(test, text + 2 * sizeof(u32), regs->pc);
}

static void sfro_every_source_leaf_executes_all_legal_address_forms(
	struct kunit *test)
{
	const struct orlix_tcti_target_instruction_artifact *artifact =
		orlix_tcti_target_instruction_artifact_canonical();
	struct orlix_tcti_target_instruction_artifact_validation_result validation;
	unsigned long data = sfro_map_data(test, PAGE_SIZE);
	size_t index;

	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_target_instruction_artifact_validate(artifact, &validation));

	for (index = 0; index < ARRAY_SIZE(sfro_expected); index++) {
		const struct sfro_expectation *expected = &sfro_expected[index];
		const struct sfro_source_leaf *leaf =
			sfro_source_leaf(expected->ordinal);
		const struct orlix_tcti_target_instruction_artifact_leaf *canonical;
		u8 option;

		KUNIT_ASSERT_NOT_NULL(test, leaf);
		KUNIT_ASSERT_LT(test, expected->ordinal, artifact->leaf_count);
		canonical = &artifact->leaves[expected->ordinal];
		KUNIT_ASSERT_NOT_NULL(test, sfro_artifact_string(artifact,
			canonical->name_offset));
		KUNIT_ASSERT_NOT_NULL(test, sfro_artifact_string(artifact,
			canonical->operation_offset));
		KUNIT_EXPECT_STREQ(test, leaf->name, sfro_artifact_string(artifact,
			canonical->name_offset));
		KUNIT_EXPECT_STREQ(test, leaf->operation,
			sfro_artifact_string(artifact, canonical->operation_offset));
		KUNIT_EXPECT_EQ(test, leaf->mask, canonical->encoding_mask);
		KUNIT_EXPECT_EQ(test, leaf->pattern, canonical->encoding_pattern);

		for (option = 2; option <= 7; option++) {
			u8 shift;

			if (!sfro_option_allowed(expected, option))
				continue;
			if (option == 4 || option == 5)
				continue;
			for (shift = 0; shift < 2; shift++) {
				struct orlix_tcti_decoded_instruction decoded;
				struct pt_regs regs;
				struct pt_regs before;
				unsigned long simd_before[ARRAY_SIZE(current->thread.user_simd)];
				unsigned long simd_expected[ARRAY_SIZE(current->thread.user_simd)];
				unsigned long base = data + 512;
				unsigned long address;
				unsigned long text;
				unsigned long simd_valid_before;
				unsigned long fpcr_before;
				unsigned long fpsr_before;
				u8 bytes[16];
				u8 observed[16] = {};
				u64 low;
				u64 high;
				u32 instruction;
				int ret;

				instruction = sfro_instruction(leaf, option, shift, 10, 11,
							     12);
				KUNIT_EXPECT_EQ_MSG(test, leaf->pattern,
					instruction & leaf->mask, "%s ordinal %u",
					leaf->name, leaf->ordinal);
				decoded = orlix_tcti_decode_aarch64(instruction);
				KUNIT_ASSERT_EQ_MSG(test,
					ORLIX_TCTI_DECODE_LOAD_STORE_REGISTER_OFFSET,
					decoded.decode_class, "%s option %u shift %u",
					leaf->name, option, shift);
				KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
				KUNIT_EXPECT_EQ(test, expected->load, decoded.load);
				KUNIT_EXPECT_EQ(test, expected->access_size,
					decoded.access_size);
				KUNIT_EXPECT_EQ(test, option, decoded.offset_extend);
				KUNIT_EXPECT_EQ(test, shift, decoded.offset_shift);

				sfro_seed(&regs, expected->ordinal ^ option ^ shift);
				regs.regs[10] = base;
				regs.regs[11] = option == 2 || option == 3 ? 3U :
					~2ULL;
				address = base + sfro_offset(option, shift,
						expected->access_size, &regs, 11);
				sfro_payload(bytes, expected->ordinal ^ option ^ shift);
				if (expected->load) {
					ret = orlix_tcti_write_user_data(current->mm, address, bytes,
							  expected->access_size);
					KUNIT_ASSERT_EQ(test, 0, ret);
					current->thread.user_simd[24] = ~0UL;
					current->thread.user_simd[25] = ~0UL;
				} else {
					low = get_unaligned_le64(bytes);
					high = get_unaligned_le64(bytes + sizeof(u64));
					current->thread.user_simd[24] = low;
					current->thread.user_simd[25] = high;
				}
				before = regs;
				memcpy(simd_before, current->thread.user_simd,
				       sizeof(simd_before));
				memcpy(simd_expected, simd_before,
				       sizeof(simd_expected));
				simd_valid_before = current->thread.user_simd_valid;
				fpcr_before = current->thread.user_fpcr;
				fpsr_before = current->thread.user_fpsr;
				text = sfro_map_instruction(test, instruction);
				regs.pc = text;
				regs.pstate = PSR_MODE_EL0t;
				regs.syscallno = NO_SYSCALL;
				before = regs;
				{
					struct orlix_tcti_result result =
						orlix_tcti_resume_user(current, &regs, current->mm);

					sfro_expect_success(test, &result, &regs, text);
				}
				before.pc = text + 2 * sizeof(u32);
				if (expected->load) {
					low = expected->access_size == 1 ? bytes[0] :
						expected->access_size == 2 ?
						get_unaligned_le16(bytes) :
						expected->access_size == 4 ?
						get_unaligned_le32(bytes) :
						get_unaligned_le64(bytes);
					high = expected->access_size == 16 ?
						get_unaligned_le64(bytes + sizeof(u64)) : 0;
					simd_expected[24] = low;
					simd_expected[25] = high;
				} else {
					ret = orlix_tcti_read_user_data(current->mm, address, observed,
							  expected->access_size);
					KUNIT_EXPECT_EQ(test, 0, ret);
					KUNIT_EXPECT_MEMEQ(test, bytes, observed,
						  expected->access_size);
				}
				KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
				KUNIT_EXPECT_MEMEQ(test, simd_expected,
					current->thread.user_simd, sizeof(simd_expected));
				KUNIT_EXPECT_EQ(test, simd_valid_before,
					current->thread.user_simd_valid);
				KUNIT_EXPECT_EQ(test, fpcr_before, current->thread.user_fpcr);
				KUNIT_EXPECT_EQ(test, fpsr_before, current->thread.user_fpsr);
				KUNIT_EXPECT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));
			}
		}
	}
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
}

static void sfro_sp_v31_and_xzr_execute_without_aliasing(struct kunit *test)
{
	const struct sfro_source_leaf *ldr_q = sfro_source_leaf(3310U);
	const struct sfro_source_leaf *str_q = sfro_source_leaf(3309U);
	struct pt_regs regs;
	u8 bytes[16];
	u8 observed[16] = {};
	unsigned long data = sfro_map_data(test, PAGE_SIZE);
	unsigned long text;
	u32 instruction;
	int ret;

	KUNIT_ASSERT_NOT_NULL(test, ldr_q);
	KUNIT_ASSERT_NOT_NULL(test, str_q);
	sfro_payload(bytes, 0x31U);
	ret = orlix_tcti_write_user_data(current->mm, data + 128, bytes, sizeof(bytes));
	KUNIT_ASSERT_EQ(test, 0, ret);
	sfro_seed(&regs, 0x310U);
	regs.sp = data + 128;
	instruction = sfro_instruction(ldr_q, 2, false, 31, 31, 31);
	text = sfro_map_instruction(test, instruction);
	regs.pc = text;
	regs.pstate = PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;
	{
		struct orlix_tcti_result result = orlix_tcti_resume_user(current, &regs,
							     current->mm);

		sfro_expect_success(test, &result, &regs, text);
	}
	KUNIT_EXPECT_EQ(test, get_unaligned_le64(bytes),
		current->thread.user_simd[62]);
	KUNIT_EXPECT_EQ(test, get_unaligned_le64(bytes + sizeof(u64)),
		current->thread.user_simd[63]);
	KUNIT_EXPECT_EQ(test, data + 128, regs.sp);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));

	sfro_payload(bytes, 0x32U);
	current->thread.user_simd[62] = get_unaligned_le64(bytes);
	current->thread.user_simd[63] = get_unaligned_le64(bytes + sizeof(u64));
	memset(&regs, 0, sizeof(regs));
	regs.sp = data + 160;
	instruction = sfro_instruction(str_q, 7, false, 31, 31, 31);
	text = sfro_map_instruction(test, instruction);
	regs.pc = text;
	regs.pstate = PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;
	{
		struct orlix_tcti_result result = orlix_tcti_resume_user(current, &regs,
							     current->mm);

		sfro_expect_success(test, &result, &regs, text);
	}
	ret = orlix_tcti_read_user_data(current->mm, data + 160, observed,
				  sizeof(observed));
	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_MEMEQ(test, bytes, observed, sizeof(bytes));
	KUNIT_EXPECT_EQ(test, data + 160, regs.sp);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
}

static void sfro_q_cross_page_faults_preserve_architectural_state(
	struct kunit *test)
{
	const struct sfro_source_leaf *ldr_q = sfro_source_leaf(3310U);
	const struct sfro_source_leaf *str_q = sfro_source_leaf(3309U);
	unsigned long data = sfro_map_data(test, 2 * PAGE_SIZE);
	unsigned long address = data + PAGE_SIZE - sizeof(u64);
	unsigned long text;
	struct pt_regs regs;
	struct pt_regs before;
	unsigned long simd_before[ARRAY_SIZE(current->thread.user_simd)];
	unsigned long simd_valid_before;
	unsigned long fpcr_before;
	unsigned long fpsr_before;
	u64 sentinel = 0x0123456789abcdefULL;
	int ret;

	KUNIT_ASSERT_NOT_NULL(test, ldr_q);
	KUNIT_ASSERT_NOT_NULL(test, str_q);
	ret = orlix_tcti_write_user_data(current->mm, address, &sentinel,
				  sizeof(sentinel));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = sys_mprotect(data + PAGE_SIZE, PAGE_SIZE, PROT_NONE);
	KUNIT_ASSERT_EQ(test, 0, ret);

	sfro_seed(&regs, 0x410U);
	regs.regs[10] = address;
	regs.regs[11] = 0;
	text = sfro_map_instruction(test, sfro_instruction(ldr_q, 2, false,
								    10, 11, 12));
	regs.pc = text;
	regs.pstate = PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;
	before = regs;
	memcpy(simd_before, current->thread.user_simd, sizeof(simd_before));
	simd_valid_before = current->thread.user_simd_valid;
	fpcr_before = current->thread.user_fpcr;
	fpsr_before = current->thread.user_fpsr;
	{
		struct orlix_tcti_result result = orlix_tcti_resume_user(current, &regs,
							     current->mm);

		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_USER_FAULT, result.reason);
		KUNIT_EXPECT_EQ(test, -EFAULT, result.status);
		KUNIT_EXPECT_EQ(test, address, result.fault_address);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_READ, result.fault_access);
		KUNIT_EXPECT_EQ(test, text, result.pc);
		KUNIT_EXPECT_EQ(test, sfro_instruction(ldr_q, 2, false, 10, 11, 12),
			result.instruction);
	}
	KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	KUNIT_EXPECT_MEMEQ(test, simd_before, current->thread.user_simd,
			  sizeof(simd_before));
	KUNIT_EXPECT_EQ(test, simd_valid_before, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, fpcr_before, current->thread.user_fpcr);
	KUNIT_EXPECT_EQ(test, fpsr_before, current->thread.user_fpsr);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));

	sfro_seed(&regs, 0x411U);
	regs.regs[10] = address;
	regs.regs[11] = 0;
	text = sfro_map_instruction(test, sfro_instruction(str_q, 2, false,
								    10, 11, 12));
	regs.pc = text;
	regs.pstate = PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;
	before = regs;
	memcpy(simd_before, current->thread.user_simd, sizeof(simd_before));
	simd_valid_before = current->thread.user_simd_valid;
	fpcr_before = current->thread.user_fpcr;
	fpsr_before = current->thread.user_fpsr;
	{
		struct orlix_tcti_result result = orlix_tcti_resume_user(current, &regs,
							     current->mm);

		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_USER_FAULT, result.reason);
		KUNIT_EXPECT_EQ(test, -EFAULT, result.status);
		KUNIT_EXPECT_EQ(test, address, result.fault_address);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_WRITE, result.fault_access);
		KUNIT_EXPECT_EQ(test, text, result.pc);
		KUNIT_EXPECT_EQ(test, sfro_instruction(str_q, 2, false, 10, 11, 12),
			result.instruction);
	}
	KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	KUNIT_EXPECT_MEMEQ(test, simd_before, current->thread.user_simd,
			  sizeof(simd_before));
	KUNIT_EXPECT_EQ(test, simd_valid_before, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, fpcr_before, current->thread.user_fpcr);
	KUNIT_EXPECT_EQ(test, fpsr_before, current->thread.user_fpsr);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, 2 * PAGE_SIZE));
}

static struct kunit_case sfro_cases[] = {
	KUNIT_CASE(sfro_every_source_leaf_executes_all_legal_address_forms),
	KUNIT_CASE(sfro_sp_v31_and_xzr_execute_without_aliasing),
	KUNIT_CASE(sfro_q_cross_page_faults_preserve_architectural_state),
	{}
};

static struct kunit_suite orlix_tcti_simd_fp_register_offset_source_bound_suite = {
	.name = "orlix-tcti-simd-fp-register-offset-source-bound",
	.init = sfro_test_init,
	.exit = sfro_test_exit,
	.test_cases = sfro_cases,
};

kunit_test_suite(orlix_tcti_simd_fp_register_offset_source_bound_suite);
