// SPDX-License-Identifier: GPL-2.0-only
/*
 * Production-path evidence for the ordinary single-register load/store
 * addressing cohort. Source identity is consumed directly from the pinned
 * AARCHMRS 2026-06 source_manifest.def. Aggregate proof-registry binding is
 * intentionally separate, so this suite does not make an ASL-proof claim.
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

#include "../decode_aarch64.h"

#define OSLS_SVC 0xd4000001U

struct osls_source_leaf {
	u32 ordinal;
	const char *name;
	const char *mnemonic;
	const char *operation;
	u32 mask;
	u32 pattern;
};

#define ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, id, mnemonic, operation, mask, \
					     pattern, feature_predicate, offset, length) \
	{ ordinal, id, mnemonic, operation, mask, pattern },
static const struct osls_source_leaf osls_source_leaves[] = {
#include "../isa/source_manifest.def"
};
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE

/* This suite's focused production-path cohort, identified only by source ID. */
static const u32 osls_ordinals[] = {
	2918U, 2919U, 2920U, 2932U, 2933U, 2934U,
	2965U, 2966U, 2967U, 2968U, 2969U, 2970U, 2971U,
	2972U, 2973U, 2974U, 2975U, 2976U, 2977U,
	2978U, 2979U, 2980U, 2992U, 2993U, 2994U,
	3297U, 3299U, 3301U, 3317U, 3318U, 3319U, 3322U, 3323U,
	3332U, 3333U, 3334U, 3346U, 3347U, 3348U, 3351U, 3352U,
};

static const struct osls_source_leaf *osls_source_leaf(u32 ordinal)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(osls_source_leaves); index++)
		if (osls_source_leaves[index].ordinal == ordinal)
			return &osls_source_leaves[index];
	return NULL;
}

static enum orlix_tcti_decode_class osls_decode_class(u32 ordinal)
{
	switch (ordinal) {
	case 2918U ... 2934U:
	case 2965U ... 2977U:
	case 2978U ... 2994U:
		return ORLIX_TCTI_DECODE_LOAD_STORE_SIGNED_IMMEDIATE;
	case 3297U ... 3323U:
		return ORLIX_TCTI_DECODE_LOAD_STORE_REGISTER_OFFSET;
	case 3332U ... 3352U:
		return ORLIX_TCTI_DECODE_LOAD_STORE_UNSIGNED_IMMEDIATE;
	default:
		return ORLIX_TCTI_DECODE_UNSUPPORTED;
	}
}

static u32 osls_test_instruction(const struct osls_source_leaf *leaf)
{
	/* Rn=x1 and Rt=x0 provide valid operand values for each source pattern. */
	u32 instruction = leaf->pattern | 0x20U;

	if (osls_decode_class(leaf->ordinal) ==
	    ORLIX_TCTI_DECODE_LOAD_STORE_REGISTER_OFFSET)
		/* option=UXTW makes the variable register-offset encoding valid. */
		instruction |= 0x4000U;
	return instruction;
}

static unsigned long osls_map_instruction(struct kunit *test, u32 instruction)
{
	const u32 program[] = { instruction, OSLS_SVC };
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

static struct orlix_tcti_result osls_run(struct kunit *test, u32 instruction,
					 struct pt_regs *regs, unsigned long *mapped)
{
	struct orlix_tcti_result result;

	*mapped = osls_map_instruction(test, instruction);
	regs->pc = *mapped;
	regs->pstate = PSR_MODE_EL0t;
	regs->syscallno = NO_SYSCALL;
	result = orlix_tcti_resume_user(current, regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0L, result.status);
	KUNIT_EXPECT_EQ(test, OSLS_SVC, result.instruction);
	KUNIT_EXPECT_EQ(test, *mapped + 2 * sizeof(u32), result.pc);
	KUNIT_EXPECT_EQ(test, *mapped + 2 * sizeof(u32), regs->pc);
	return result;
}

static unsigned long osls_map_data(struct kunit *test)
{
	unsigned long mapped;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	return mapped;
}

static void osls_source_decode(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(osls_ordinals); index++) {
		const struct osls_source_leaf *leaf =
			osls_source_leaf(osls_ordinals[index]);
		struct orlix_tcti_decoded_instruction decoded;

		KUNIT_ASSERT_NOT_NULL(test, leaf);
		decoded = orlix_tcti_decode_aarch64(osls_test_instruction(leaf));
		KUNIT_EXPECT_EQ_MSG(test, leaf->pattern,
			osls_test_instruction(leaf) & leaf->mask,
			"%s ordinal %u", leaf->name, leaf->ordinal);
		KUNIT_EXPECT_EQ_MSG(test, osls_decode_class(leaf->ordinal),
				    decoded.decode_class,
				    "%s ordinal %u", leaf->name, leaf->ordinal);
	}
}

static void osls_unsigned_unscaled_and_sign_extend_production_path(
	struct kunit *test)
{
	struct pt_regs regs = {};
	unsigned long data = osls_map_data(test);
	unsigned long mapped;
	u8 byte = 0x80;
	u8 observed = 0;
	int ret;

	regs.regs[0] = 0xa5;
	regs.regs[1] = data;
	osls_run(test, 0x39000420U, &regs, &mapped); /* STRB w0, [x1, #1] */
	ret = orlix_tcti_read_user_data(current->mm, data + 1, &observed,
				  sizeof(observed));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, (u8)0xa5, observed);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));

	ret = orlix_tcti_write_user_data(current->mm, data + 1, &byte, sizeof(byte));
	KUNIT_ASSERT_EQ(test, 0, ret);
	memset(&regs, 0, sizeof(regs));
	regs.regs[1] = data;
	osls_run(test, 0x39800420U, &regs, &mapped); /* LDRSB x0, [x1, #1] */
	KUNIT_EXPECT_EQ(test, 0xffffffffffffff80ULL, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, data, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));

	/* LDURSB x0, [x1, #-1] proves signed unscaled offset formation. */
	ret = orlix_tcti_write_user_data(current->mm, data, &byte, sizeof(byte));
	KUNIT_ASSERT_EQ(test, 0, ret);
	memset(&regs, 0, sizeof(regs));
	regs.regs[1] = data + 1;
	osls_run(test, 0x389ff020U, &regs, &mapped);
	KUNIT_EXPECT_EQ(test, 0xffffffffffffff80ULL, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, data + 1, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
}

static void osls_pre_post_and_register_offset_production_path(struct kunit *test)
{
	struct pt_regs regs = {};
	unsigned long data = osls_map_data(test);
	unsigned long mapped;
	u32 observed;
	u32 word = 0x80000001U;
	int ret;

	regs.regs[0] = 0x12345678;
	regs.regs[1] = data;
	osls_run(test, 0xb8400c20U, &regs, &mapped); /* LDR w0, [x1, #0]! */
	KUNIT_EXPECT_EQ(test, data, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));

	/* Pre-indexed store commits base and memory only after a successful write. */
	memset(&regs, 0, sizeof(regs));
	regs.regs[0] = 0x89abcdef;
	regs.regs[1] = data;
	osls_run(test, 0xb8004c20U, &regs, &mapped); /* STR w0, [x1, #4]! */
	ret = orlix_tcti_read_user_data(current->mm, data + 4, &observed,
				  sizeof(observed));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x89abcdefU, observed);
	KUNIT_EXPECT_EQ(test, data + 4, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));

	ret = orlix_tcti_write_user_data(current->mm, data + 4, &word, sizeof(word));
	KUNIT_ASSERT_EQ(test, 0, ret);
	memset(&regs, 0, sizeof(regs));
	regs.regs[1] = data;
	osls_run(test, 0xb8404420U, &regs, &mapped); /* LDR w0, [x1], #4 */
	KUNIT_EXPECT_EQ(test, word, (u32)regs.regs[0]);
	KUNIT_EXPECT_EQ(test, data + 4, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));

	memset(&regs, 0, sizeof(regs));
	regs.regs[1] = data;
	regs.regs[2] = 1;
	osls_run(test, 0xb8a2d820U, &regs, &mapped);
	/* LDRSW x0, [x1, w2, sxtw #2] */
	KUNIT_EXPECT_EQ(test, 0xffffffff80000001ULL, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, data, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));

	/* UXTW register offset zero-extends Wm before addressing. */
	{
		u8 byte = 0x5a;

		ret = orlix_tcti_write_user_data(current->mm, data + 3, &byte,
					  sizeof(byte));
		KUNIT_ASSERT_EQ(test, 0, ret);
		memset(&regs, 0, sizeof(regs));
		regs.regs[1] = data;
		regs.regs[2] = 3;
		regs.regs[0] = ~0ULL;
		osls_run(test, 0x38624820U, &regs, &mapped);
		/* LDRB w0, [x1, w2, uxtw] */
		KUNIT_EXPECT_EQ(test, 0x5aULL, regs.regs[0]);
		KUNIT_EXPECT_EQ(test, data, regs.regs[1]);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	}
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
}

static void osls_unprivileged_load_store_production_path(struct kunit *test)
{
	struct pt_regs regs = {};
	unsigned long data = osls_map_data(test);
	unsigned long mapped;
	u64 zero = 0;
	u64 word = 0x0123456789abcdefULL;
	u64 observed64;
	u32 observed32;
	u16 half = 0x8001U;
	u16 observed16;
	u8 byte = 0x80U;
	u8 observed8;
	int ret;

	/* STTRB w0, [x1, #1] writes only the low byte and does not write back. */
	regs.regs[0] = 0xfeedfaceULL;
	regs.regs[1] = data;
	osls_run(test, 0x38001820U, &regs, &mapped);
	ret = orlix_tcti_read_user_data(current->mm, data + 1, &observed8,
				  sizeof(observed8));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, (u8)0xce, observed8);
	KUNIT_EXPECT_EQ(test, data, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));

	/* LDTRB w0, [x1, #1] zero-extends its byte destination. */
	byte = 0x81U;
	ret = orlix_tcti_write_user_data(current->mm, data + 1, &byte, sizeof(byte));
	KUNIT_ASSERT_EQ(test, 0, ret);
	memset(&regs, 0, sizeof(regs));
	regs.regs[0] = ~0ULL;
	regs.regs[1] = data;
	osls_run(test, 0x38401820U, &regs, &mapped);
	KUNIT_EXPECT_EQ(test, 0x81ULL, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, data, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));

	/* LDTRSB x0, [x1, #2] has the ordinary signed byte result at EL0. */
	byte = 0x80U;
	ret = orlix_tcti_write_user_data(current->mm, data + 2, &byte, sizeof(byte));
	KUNIT_ASSERT_EQ(test, 0, ret);
	memset(&regs, 0, sizeof(regs));
	regs.regs[1] = data;
	osls_run(test, 0x38802820U, &regs, &mapped);
	KUNIT_EXPECT_EQ(test, 0xffffffffffffff80ULL, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, data, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));

	/* LDTRSB w0, [x1, #3] sign-extends to W and clears X's high half. */
	ret = orlix_tcti_write_user_data(current->mm, data + 3, &byte, sizeof(byte));
	KUNIT_ASSERT_EQ(test, 0, ret);
	memset(&regs, 0, sizeof(regs));
	regs.regs[0] = ~0ULL;
	regs.regs[1] = data;
	osls_run(test, 0x38c03820U, &regs, &mapped);
	KUNIT_EXPECT_EQ(test, 0x00000000ffffff80ULL, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, data, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));

	/* STTRH w0, [x1, #4] writes only the low halfword. */
	memset(&regs, 0, sizeof(regs));
	regs.regs[0] = 0xdeadbeefULL;
	regs.regs[1] = data;
	osls_run(test, 0x78004820U, &regs, &mapped);
	ret = orlix_tcti_read_user_data(current->mm, data + 4, &observed16,
				  sizeof(observed16));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, (u16)0xbeef, observed16);
	KUNIT_EXPECT_EQ(test, data, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));

	/* LDTRH w0, [x1, #5] zero-extends the halfword. */
	half = 0x8001U;
	ret = orlix_tcti_write_user_data(current->mm, data + 5, &half, sizeof(half));
	KUNIT_ASSERT_EQ(test, 0, ret);
	memset(&regs, 0, sizeof(regs));
	regs.regs[0] = ~0ULL;
	regs.regs[1] = data;
	osls_run(test, 0x78405820U, &regs, &mapped);
	KUNIT_EXPECT_EQ(test, 0x8001ULL, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, data, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));

	/* LDTRSH x0, [x1, #6] sign-extends directly to X. */
	ret = orlix_tcti_write_user_data(current->mm, data + 6, &half, sizeof(half));
	KUNIT_ASSERT_EQ(test, 0, ret);
	memset(&regs, 0, sizeof(regs));
	regs.regs[1] = data;
	osls_run(test, 0x78806820U, &regs, &mapped);
	KUNIT_EXPECT_EQ(test, 0xffffffffffff8001ULL, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, data, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));

	/* LDTRSH w0, [x1, #3] sign-extends to W then clears X's high half. */
	ret = orlix_tcti_write_user_data(current->mm, data + 7, &half, sizeof(half));
	KUNIT_ASSERT_EQ(test, 0, ret);
	memset(&regs, 0, sizeof(regs));
	regs.regs[1] = data;
	osls_run(test, 0x78c07820U, &regs, &mapped);
	KUNIT_EXPECT_EQ(test, 0x00000000ffff8001ULL, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, data, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));

	/* STTR w0, [x1, #8] transfers only the low word. */
	memset(&regs, 0, sizeof(regs));
	regs.regs[0] = 0x89abcdef01234567ULL;
	regs.regs[1] = data;
	osls_run(test, 0xb8008820U, &regs, &mapped);
	ret = orlix_tcti_read_user_data(current->mm, data + 8, &observed32,
				  sizeof(observed32));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x01234567U, observed32);
	KUNIT_EXPECT_EQ(test, data, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));

	/* LDTR w0, [x1, #9] clears X's high half. */
	observed32 = 0x80000001U;
	ret = orlix_tcti_write_user_data(current->mm, data + 9, &observed32,
				   sizeof(observed32));
	KUNIT_ASSERT_EQ(test, 0, ret);
	memset(&regs, 0, sizeof(regs));
	regs.regs[0] = ~0ULL;
	regs.regs[1] = data;
	osls_run(test, 0xb8409820U, &regs, &mapped);
	KUNIT_EXPECT_EQ(test, 0x0000000080000001ULL, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, data, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));

	/* LDTRSW x0, [x1, #10] sign-extends the loaded word. */
	ret = orlix_tcti_write_user_data(current->mm, data + 10, &observed32,
				   sizeof(observed32));
	KUNIT_ASSERT_EQ(test, 0, ret);
	memset(&regs, 0, sizeof(regs));
	regs.regs[1] = data;
	osls_run(test, 0xb880a820U, &regs, &mapped);
	KUNIT_EXPECT_EQ(test, 0xffffffff80000001ULL, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, data, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));

	/* STTR x0, [x1, #11] transfers all eight bytes. */
	memset(&regs, 0, sizeof(regs));
	regs.regs[0] = word;
	regs.regs[1] = data;
	osls_run(test, 0xf800b820U, &regs, &mapped);
	ret = orlix_tcti_read_user_data(current->mm, data + 11, &observed64,
				  sizeof(observed64));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, word, observed64);
	KUNIT_EXPECT_EQ(test, data, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));

	/* LDTR x0, [x1, #12] transfers all eight bytes. */
	ret = orlix_tcti_write_user_data(current->mm, data + 12, &word, sizeof(word));
	KUNIT_ASSERT_EQ(test, 0, ret);
	memset(&regs, 0, sizeof(regs));
	regs.regs[1] = data;
	osls_run(test, 0xf840c820U, &regs, &mapped);
	KUNIT_EXPECT_EQ(test, word, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, data, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));

	/* STTR and LDTR use SP as the base and XZR as a zero/discard register. */
	memset(&regs, 0, sizeof(regs));
	regs.sp = data + 16;
	osls_run(test, 0xf8000bffU, &regs, &mapped); /* STTR xzr, [sp] */
	ret = orlix_tcti_read_user_data(current->mm, data + 16, &zero, sizeof(zero));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0ULL, zero);
	KUNIT_EXPECT_EQ(test, data + 16, regs.sp);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));

	ret = orlix_tcti_write_user_data(current->mm, data + 16, &word, sizeof(word));
	KUNIT_ASSERT_EQ(test, 0, ret);
	memset(&regs, 0, sizeof(regs));
	regs.sp = data + 16;
	regs.regs[0] = 0xa5a5a5a5a5a5a5a5ULL;
	osls_run(test, 0xf8400bffU, &regs, &mapped); /* LDTR xzr, [sp] */
	KUNIT_EXPECT_EQ(test, 0xa5a5a5a5a5a5a5ULL, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, data + 16, regs.sp);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
}

static void osls_unprivileged_read_fault_preserves_state(struct kunit *test)
{
	const u32 instruction = 0xf8400820U; /* LDTR x0, [x1] */
	struct pt_regs regs = {};
	struct pt_regs before;
	struct orlix_tcti_result result;
	unsigned long mapped;

	mapped = osls_map_instruction(test, instruction);
	regs.pc = mapped;
	regs.pstate = PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;
	regs.regs[0] = 0x0123456789abcdefULL;
	regs.regs[1] = 0;
	before = regs;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_USER_FAULT, result.reason);
	KUNIT_EXPECT_EQ(test, -EFAULT, result.status);
	KUNIT_EXPECT_EQ(test, 0UL, result.fault_address);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_READ, result.fault_access);
	KUNIT_EXPECT_EQ(test, mapped, result.pc);
	KUNIT_EXPECT_EQ(test, instruction, result.instruction);
	KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void osls_fault_preserves_pc_and_writeback(struct kunit *test)
{
	struct pt_regs regs = {};
	struct orlix_tcti_result result;
	unsigned long mapped;

	mapped = osls_map_instruction(test, 0xb8004c20U);
	regs.pc = mapped;
	regs.pstate = PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;
	regs.regs[0] = 0xfeedface;
	regs.regs[1] = 0;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_USER_FAULT, result.reason);
	KUNIT_EXPECT_EQ(test, -EFAULT, result.status);
	KUNIT_EXPECT_EQ(test, 4UL, result.fault_address);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_WRITE, result.fault_access);
	KUNIT_EXPECT_EQ(test, mapped, result.pc);
	KUNIT_EXPECT_EQ(test, mapped, regs.pc);
	KUNIT_EXPECT_EQ(test, 0UL, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void osls_writeback_overlap_is_rejected_before_state_mutation(
	struct kunit *test)
{
	static const u32 overlap_instructions[] = {
		0xb8004c21U, /* STR w1, [x1, #4]! */
		0xb8404c21U, /* LDR w1, [x1, #4]! */
		0xb8004421U, /* STR w1, [x1], #4 */
		0xb8404421U, /* LDR w1, [x1], #4 */
	};
	const u32 sentinel = 0xdecafbadU;
	unsigned long data = osls_map_data(test);
	size_t index;
	int ret;

	/* SP base and ZR transfer do not form a writeback overlap. */
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_LOAD_STORE_SIGNED_IMMEDIATE,
			orlix_tcti_decode_aarch64(0xb8004fffU).decode_class);

	for (index = 0; index < ARRAY_SIZE(overlap_instructions); index++) {
		struct pt_regs regs = {};
		struct pt_regs before;
		struct orlix_tcti_result result;
		unsigned long mapped;
		u32 observed = 0;

		/*
		 * Fail-closed containment only. The constrained-unpredictable
		 * overlap remains unproved until the authoritative ASL entry is
		 * available, so it must never execute or mutate guest state.
		 */
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
			orlix_tcti_decode_aarch64(overlap_instructions[index]).decode_class,
			"instruction=%#x", overlap_instructions[index]);
		ret = orlix_tcti_write_user_data(current->mm, data + 4, &sentinel,
					  sizeof(sentinel));
		KUNIT_ASSERT_EQ(test, 0, ret);
		mapped = osls_map_instruction(test, overlap_instructions[index]);
		regs.pc = mapped;
		regs.pstate = PSR_MODE_EL0t;
		regs.syscallno = NO_SYSCALL;
		regs.regs[1] = data;
		before = regs;

		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				    result.reason, "instruction=%#x",
				    overlap_instructions[index]);
		KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
		KUNIT_EXPECT_EQ(test, mapped, result.pc);
		KUNIT_EXPECT_EQ(test, overlap_instructions[index], result.instruction);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		ret = orlix_tcti_read_user_data(current->mm, data + 4, &observed,
					 sizeof(observed));
		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_EXPECT_EQ(test, sentinel, observed);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	}
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
}

static struct kunit_case osls_cases[] = {
	KUNIT_CASE(osls_source_decode),
	KUNIT_CASE(osls_unsigned_unscaled_and_sign_extend_production_path),
	KUNIT_CASE(osls_pre_post_and_register_offset_production_path),
	KUNIT_CASE(osls_unprivileged_load_store_production_path),
	KUNIT_CASE(osls_unprivileged_read_fault_preserves_state),
	KUNIT_CASE(osls_fault_preserves_pc_and_writeback),
	KUNIT_CASE(osls_writeback_overlap_is_rejected_before_state_mutation),
	{}
};

static struct kunit_suite orlix_tcti_ordinary_single_load_store_source_bound_suite = {
	.name = "orlix-tcti-ordinary-single-load-store-source-bound",
	.test_cases = osls_cases,
};

kunit_test_suite(orlix_tcti_ordinary_single_load_store_source_bound_suite);
