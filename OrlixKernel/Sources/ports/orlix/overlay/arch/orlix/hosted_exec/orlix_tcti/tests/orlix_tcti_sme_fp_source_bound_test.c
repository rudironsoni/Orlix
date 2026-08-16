// SPDX-License-Identifier: GPL-2.0-only
/* Production-resume source-bound proof for the #141 SME FP slice. */
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>
#include <kunit/test.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/string.h>
#include <linux/syscalls.h>

#include "../decode_aarch64.h"
#include "../sme_fp_decode.h"
#include "../sve_state.h"

#define SME_FP_SOURCE_SVC 0xd4000001U

struct sme_fp_source_leaf {
	u16 ordinal;
	const char *name;
	const char *operation;
	u32 mask;
	u32 pattern;
};

#define ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, id, mnemonic, operation, \
					   mask, pattern, predicate, offset, length) \
	{ ordinal, id, operation, mask, pattern },
static const struct sme_fp_source_leaf sme_fp_source_manifest[] = {
#include "../isa/source_manifest.def"
};
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE

struct sme_fp_source_bound_context {
	struct mm_struct *mm;
	unsigned long code;
	struct user_sve_state sve;
	struct user_sme_state sme;
	unsigned long fpcr;
	unsigned long fpsr;
};

static bool sme_fp_source_excluded(u16 ordinal)
{
	switch (ordinal) {
	case 2102U: case 2103U: case 2106U: case 2107U: case 2108U: case 2109U:
	case 2110U: case 2111U: case 2112U: case 2113U: case 2130U: case 2131U:
	case 2008U: case 2026U: case 2046U: case 2065U:
		return true;
	default:
		return false;
	}
}

static int sme_fp_source_bound_init(struct kunit *test)
{
	struct sme_fp_source_bound_context *context;

	context = kunit_kzalloc(test, sizeof(*context), GFP_KERNEL);
	if (!context)
		return -ENOMEM;
	context->mm = mm_alloc();
	if (!context->mm)
		return -ENOMEM;
	kthread_use_mm(context->mm);
	context->code = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (IS_ERR_VALUE(context->code)) {
		kthread_unuse_mm(context->mm);
		mmput(context->mm);
		return -ENOMEM;
	}
	context->sve = current->thread.user_sve;
	context->sme = current->thread.user_sme;
	context->fpcr = current->thread.user_fpcr;
	context->fpsr = current->thread.user_fpsr;
	test->priv = context;
	return 0;
}

static void sme_fp_source_bound_exit(struct kunit *test)
{
	struct sme_fp_source_bound_context *context = test->priv;

	orlix_tcti_sme_state_release(&current->thread.user_sme);
	if (context->code)
		vm_munmap(context->code, PAGE_SIZE);
	kthread_unuse_mm(context->mm);
	mmput(context->mm);
	current->thread.user_sve = context->sve;
	current->thread.user_sme = context->sme;
	current->thread.user_fpcr = context->fpcr;
	current->thread.user_fpsr = context->fpsr;
}

static void sme_fp_source_seed_state(struct kunit *test, u32 instruction)
{
	unsigned int i;

	memset(&current->thread.user_sve, 0, sizeof(current->thread.user_sve));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(&current->thread.user_sve,
		current->thread.user_simd, ORLIX_TCTI_SVE_MAX_VL_BYTES));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sme_state_reset(&current->thread.user_sme,
		ORLIX_TCTI_SVE_MAX_VL_BYTES, true, true, true));
	for (i = 0; i < ARRAY_SIZE(current->thread.user_sve.z); i++)
		memset(current->thread.user_sve.z[i], (u8)(instruction + i),
			ORLIX_TCTI_SVE_MAX_VL_BYTES);
	memset(current->thread.user_sme.za, (u8)instruction,
		current->thread.user_sme.za_bytes);
	current->thread.user_fpcr = 0;
	current->thread.user_fpsr = 0;
}

static void sme_fp_source_resume_all_canonical_rows(struct kunit *test)
{
	struct sme_fp_source_bound_context *context = test->priv;
	size_t index, rows = 0;

	for (index = 0; index < ARRAY_SIZE(sme_fp_source_manifest); index++) {
		const struct sme_fp_source_leaf *leaf = &sme_fp_source_manifest[index];
		struct orlix_tcti_decoded_instruction decoded;
		struct pt_regs regs = {};
		struct orlix_tcti_result result;
		u32 program[] = { leaf->pattern, SME_FP_SOURCE_SVC };
		int ret;

		if (!orlix_tcti_sme_fp_source_ordinal(leaf->ordinal) ||
		    sme_fp_source_excluded(leaf->ordinal))
			continue;
		rows++;
		decoded = orlix_tcti_decode_aarch64(leaf->pattern);
		KUNIT_ASSERT_EQ_MSG(test, ORLIX_TCTI_DECODE_SME_FP,
			decoded.decode_class, "%u %s", leaf->ordinal, leaf->name);
		KUNIT_ASSERT_EQ_MSG(test, leaf->ordinal, decoded.sme_fp_source_ordinal,
			"%u %s", leaf->ordinal, leaf->operation);
		KUNIT_ASSERT_STREQ(test, leaf->operation, sme_fp_source_manifest[index].operation);
		ret = sys_mprotect(context->code, PAGE_SIZE, PROT_READ | PROT_WRITE);
		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_write_user_data(current->mm,
			context->code, program, sizeof(program)));
		KUNIT_ASSERT_EQ(test, 0, sys_mprotect(context->code, PAGE_SIZE,
			PROT_READ | PROT_EXEC));
		sme_fp_source_seed_state(test, leaf->pattern);
		regs.pc = context->code;
		regs.sp = 0x00000001fffffff0ULL;
		regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT | PSR_V_BIT;
		regs.syscallno = NO_SYSCALL;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason,
			"%u %s", leaf->ordinal, leaf->operation);
		KUNIT_EXPECT_EQ_MSG(test, SME_FP_SOURCE_SVC, result.instruction,
			"%u %s", leaf->ordinal, leaf->operation);
		KUNIT_EXPECT_EQ_MSG(test, context->code + sizeof(u32), regs.pc,
			"%u %s", leaf->ordinal, leaf->operation);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(context->code, PAGE_SIZE));
		context->code = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(context->code));
		orlix_tcti_sme_state_release(&current->thread.user_sme);
	}
	KUNIT_EXPECT_EQ(test, 78UL, rows);
}

static void sme_fp_source_rejects_reserved_and_non_family_neighbours(struct kunit *test)
{
	struct orlix_tcti_decoded_instruction decoded;

	decoded = orlix_tcti_decode_aarch64(0xc120a000U); /* SMAX, not #141 FP. */
	KUNIT_EXPECT_NE(test, ORLIX_TCTI_DECODE_SME_FP, decoded.decode_class);
	decoded = orlix_tcti_decode_aarch64(0xc120d000U); /* ZIP, not #141 FP. */
	KUNIT_EXPECT_NE(test, ORLIX_TCTI_DECODE_SME_FP, decoded.decode_class);
	decoded = orlix_tcti_decode_aarch64(0xc1a01c04U); /* reserved FADD ZA bit. */
	KUNIT_EXPECT_NE(test, ORLIX_TCTI_DECODE_SME_FP, decoded.decode_class);
}

static struct kunit_case sme_fp_source_bound_cases[] = {
	KUNIT_CASE(sme_fp_source_resume_all_canonical_rows),
	KUNIT_CASE(sme_fp_source_rejects_reserved_and_non_family_neighbours),
	{}
};

static struct kunit_suite sme_fp_source_bound_suite = {
	.name = "orlix-tcti-sme-fp-source-bound",
	.init = sme_fp_source_bound_init,
	.exit = sme_fp_source_bound_exit,
	.test_cases = sme_fp_source_bound_cases,
};

kunit_test_suite(sme_fp_source_bound_suite);
