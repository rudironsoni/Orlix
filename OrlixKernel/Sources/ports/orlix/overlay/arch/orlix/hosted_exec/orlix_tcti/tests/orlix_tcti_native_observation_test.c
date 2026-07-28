// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched/mm.h>
#include <linux/string.h>
#include <linux/syscalls.h>

#include "orlix_tcti_native_observation.h"

#define NATIVE_NOP_ORDINAL 2238U
#define NATIVE_NOP 0xd503201fU
#define NATIVE_LDR_64_ORDINAL 3352U
#define NATIVE_LDR_X0_X1 0xf9400020U
#define NATIVE_SVC 0xd4000001U

static int native_observation_test_init(struct kunit *test)
{
	struct mm_struct *mm = mm_alloc();

	if (!mm)
		return -ENOMEM;
	kthread_use_mm(mm);
	test->priv = mm;
	return 0;
}

static void native_observation_test_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (!mm)
		return;
	kthread_unuse_mm(mm);
	mmput(mm);
}

static unsigned long native_map_program(struct kunit *test, u32 instruction)
{
	const u32 program[] = { instruction, NATIVE_SVC };
	unsigned long mapped;
	int ret;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	ret = orlix_tcti_write_user_data(current->mm, mapped, program,
					 sizeof(program));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = sys_mprotect(mapped, PAGE_SIZE, PROT_READ | PROT_EXEC);
	KUNIT_ASSERT_EQ(test, 0, ret);
	return mapped;
}

static void native_seed_execution(
		struct orlix_tcti_native_observation_spec *spec,
		struct pt_regs *regs, enum orlix_tcti_native_obligation obligation,
		unsigned long mapped)
{
	struct pt_regs expected;
	u32 index;

	memset(spec, 0, sizeof(*spec));
	memset(regs, 0, sizeof(*regs));
	for (index = 0; index < ARRAY_SIZE(regs->regs); index++)
		regs->regs[index] = 0x100000000ULL + index;
	regs->sp = 0x20000;
	regs->pc = mapped;
	regs->pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT;
	regs->syscallno = NO_SYSCALL;
	expected = *regs;
	expected.pc = mapped + sizeof(u32);
	spec->source_ordinal = NATIVE_NOP_ORDINAL;
	spec->obligation = obligation;
	spec->result.reason = ORLIX_TCTI_EXIT_SYSCALL;
	spec->result.status = 0;
	spec->result.fault_access = ORLIX_TCTI_ACCESS_FETCH;
	spec->result.pc = mapped + sizeof(u32);
	spec->result.instruction = NATIVE_SVC;
	orlix_tcti_native_gpr_capture(&spec->gpr, &expected);
}

static struct orlix_tcti_native_observation *native_create_and_execute(
		struct kunit *test,
		const struct orlix_tcti_native_observation_spec *spec,
		struct pt_regs *regs)
{
	struct orlix_tcti_native_observation *observation =
		orlix_tcti_native_observation_create(spec);

	KUNIT_ASSERT_NOT_NULL(test, observation);
	KUNIT_ASSERT_EQ(test, 0,
			orlix_tcti_native_observation_execute(
				observation, current, regs, current->mm));
	return observation;
}

static void native_no_synthetic_or_recoverable_match(struct kunit *test)
{
	struct orlix_tcti_native_observation_spec spec;
	struct orlix_tcti_native_observation *observation;
	struct orlix_tcti_native_memory_state wrong_type = {
		.address = 0,
		.size = 1,
		.bytes = (const u8[]){ 0 },
	};
	struct pt_regs regs;
	unsigned long mapped = native_map_program(test, NATIVE_NOP);

	native_seed_execution(&spec, &regs, ORLIX_TCTI_NATIVE_OBLIGATION_GPR,
			      mapped);
	observation = orlix_tcti_native_observation_create(&spec);
	KUNIT_ASSERT_NOT_NULL(test, observation);
	KUNIT_EXPECT_EQ(test, -EINPROGRESS,
			orlix_tcti_native_observation_compare(observation));
	KUNIT_EXPECT_EQ(test, -EPROTOTYPE,
			orlix_tcti_native_observation_add_memory(observation,
							      &wrong_type));
	KUNIT_EXPECT_EQ(test, -EPERM,
			orlix_tcti_native_observation_execute(
				observation, current, &regs, current->mm));
	KUNIT_EXPECT_EQ(test, -EPERM,
			orlix_tcti_native_observation_compare(observation));
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_NATIVE_OBSERVATION_POISONED,
			orlix_tcti_native_observation_state(observation));
	orlix_tcti_native_observation_destroy(observation);

	native_seed_execution(&spec, &regs, ORLIX_TCTI_NATIVE_OBLIGATION_FP_SIMD,
			      mapped);
	spec.expected.fp_simd.valid = true;
	observation = orlix_tcti_native_observation_create(&spec);
	KUNIT_ASSERT_NOT_NULL(test, observation);
	KUNIT_EXPECT_EQ(test, -EINVAL,
			orlix_tcti_native_observation_add_fp_simd(
				observation,
				&(struct orlix_tcti_native_fp_simd_state){}));
	KUNIT_EXPECT_EQ(test, -EPERM,
			orlix_tcti_native_observation_execute(
				observation, current, &regs, current->mm));
	KUNIT_EXPECT_EQ(test, -EPERM,
			orlix_tcti_native_observation_compare(observation));
	orlix_tcti_native_observation_destroy(observation);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void native_result_and_gpr_are_captured_not_injected(struct kunit *test)
{
	struct orlix_tcti_native_observation_spec spec;
	struct orlix_tcti_native_observation *observation;
	struct pt_regs regs;
	unsigned long mapped = native_map_program(test, NATIVE_NOP);

	native_seed_execution(&spec, &regs, ORLIX_TCTI_NATIVE_OBLIGATION_GPR,
			      mapped);
	observation = native_create_and_execute(test, &spec, &regs);
	KUNIT_EXPECT_EQ(test, 0,
			orlix_tcti_native_observation_compare(observation));
	orlix_tcti_native_observation_destroy(observation);

	native_seed_execution(&spec, &regs, ORLIX_TCTI_NATIVE_OBLIGATION_GPR,
			      mapped);
	spec.gpr.x[30]++;
	observation = native_create_and_execute(test, &spec, &regs);
	KUNIT_EXPECT_EQ(test, -EBADE,
			orlix_tcti_native_observation_compare(observation));
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_NATIVE_OBSERVATION_GPR_MISMATCH,
			orlix_tcti_native_observation_state(observation));
	orlix_tcti_native_observation_destroy(observation);

	native_seed_execution(&spec, &regs, ORLIX_TCTI_NATIVE_OBLIGATION_RESULT,
			      mapped);
	spec.result.status = -EINVAL;
	observation = native_create_and_execute(test, &spec, &regs);
	KUNIT_EXPECT_EQ(test, -EBADE,
			orlix_tcti_native_observation_compare(observation));
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_NATIVE_OBSERVATION_RESULT_MISMATCH,
			orlix_tcti_native_observation_state(observation));
	orlix_tcti_native_observation_destroy(observation);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void native_memory_and_fp_simd_compare_after_production(struct kunit *test)
{
	u8 memory_bytes[] = { 0, 0, 0, 0 };
	u8 changed_memory[] = { 0, 0, 0, 1 };
	struct orlix_tcti_native_observation_spec spec;
	struct orlix_tcti_native_observation *observation;
	struct orlix_tcti_native_memory_state memory;
	struct orlix_tcti_native_fp_simd_state fp_simd;
	struct pt_regs regs;
	unsigned long mapped = native_map_program(test, NATIVE_NOP);

	native_seed_execution(&spec, &regs, ORLIX_TCTI_NATIVE_OBLIGATION_MEMORY,
			      mapped);
	spec.expected.memory.address = 0;
	spec.expected.memory.size = sizeof(memory_bytes);
	spec.expected.memory.bytes = memory_bytes;
	observation = native_create_and_execute(test, &spec, &regs);
	KUNIT_ASSERT_EQ(test, 0,
			orlix_tcti_native_observation_add_memory(
				observation, &spec.expected.memory));
	KUNIT_EXPECT_EQ(test, 0,
			orlix_tcti_native_observation_compare(observation));
	KUNIT_EXPECT_EQ(test, -EALREADY,
			orlix_tcti_native_observation_add_memory(
				observation, &spec.expected.memory));
	KUNIT_EXPECT_EQ(test, -EPERM,
			orlix_tcti_native_observation_compare(observation));
	orlix_tcti_native_observation_destroy(observation);
	native_seed_execution(&spec, &regs, ORLIX_TCTI_NATIVE_OBLIGATION_MEMORY,
			      mapped);
	spec.expected.memory.address = 0;
	spec.expected.memory.size = sizeof(memory_bytes);
	spec.expected.memory.bytes = memory_bytes;
	observation = native_create_and_execute(test, &spec, &regs);
	memory = spec.expected.memory;
	memory.bytes = changed_memory;
	KUNIT_ASSERT_EQ(test, 0,
			orlix_tcti_native_observation_add_memory(observation, &memory));
	KUNIT_EXPECT_EQ(test, -EBADE,
			orlix_tcti_native_observation_compare(observation));
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_NATIVE_OBSERVATION_MEMORY_MISMATCH,
			orlix_tcti_native_observation_state(observation));
	orlix_tcti_native_observation_destroy(observation);

	native_seed_execution(&spec, &regs, ORLIX_TCTI_NATIVE_OBLIGATION_FP_SIMD,
			      mapped);
	spec.expected.fp_simd.valid = true;
	spec.expected.fp_simd.v[31][15] = 0xa5;
	spec.expected.fp_simd.fpcr = BIT(22);
	spec.expected.fp_simd.fpsr = BIT(27);
	observation = native_create_and_execute(test, &spec, &regs);
	KUNIT_ASSERT_EQ(test, 0,
			orlix_tcti_native_observation_add_fp_simd(
				observation, &spec.expected.fp_simd));
	KUNIT_EXPECT_EQ(test, 0,
			orlix_tcti_native_observation_compare(observation));
	orlix_tcti_native_observation_destroy(observation);
	native_seed_execution(&spec, &regs, ORLIX_TCTI_NATIVE_OBLIGATION_FP_SIMD,
			      mapped);
	spec.expected.fp_simd.valid = true;
	spec.expected.fp_simd.v[31][15] = 0xa5;
	spec.expected.fp_simd.fpcr = BIT(22);
	spec.expected.fp_simd.fpsr = BIT(27);
	observation = native_create_and_execute(test, &spec, &regs);
	fp_simd = spec.expected.fp_simd;
	fp_simd.v[0][0] = 1;
	KUNIT_ASSERT_EQ(test, 0,
			orlix_tcti_native_observation_add_fp_simd(observation, &fp_simd));
	KUNIT_EXPECT_EQ(test, -EBADE,
			orlix_tcti_native_observation_compare(observation));
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_NATIVE_OBSERVATION_FP_SIMD_MISMATCH,
			orlix_tcti_native_observation_state(observation));
	orlix_tcti_native_observation_destroy(observation);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void native_sve_uses_heap_owned_complete_state(struct kunit *test)
{
	u8 z[ORLIX_TCTI_SVE_ZREG_COUNT * ORLIX_TCTI_SVE_MIN_VL_BYTES] = {};
	u8 p[ORLIX_TCTI_SVE_PREG_COUNT *
	     (ORLIX_TCTI_SVE_MIN_VL_BYTES / 8)] = {};
	u8 ffr[ORLIX_TCTI_SVE_MIN_VL_BYTES / 8] = {};
	u8 observed_z[sizeof(z)];
	u8 observed_p[sizeof(p)];
	u8 observed_ffr[sizeof(ffr)];
	struct orlix_tcti_native_observation_spec spec;
	struct orlix_tcti_native_observation *observation;
	struct orlix_tcti_native_sve_state observed_sve;
	struct pt_regs regs;
	unsigned long mapped = native_map_program(test, NATIVE_NOP);

	z[sizeof(z) - 1] = 1;
	p[sizeof(p) - 1] = 2;
	ffr[sizeof(ffr) - 1] = 3;
	memcpy(observed_z, z, sizeof(z));
	memcpy(observed_p, p, sizeof(p));
	memcpy(observed_ffr, ffr, sizeof(ffr));
	native_seed_execution(&spec, &regs, ORLIX_TCTI_NATIVE_OBLIGATION_SVE,
			      mapped);
	spec.expected.sve.vl_bytes = ORLIX_TCTI_SVE_MIN_VL_BYTES;
	spec.expected.sve.z = z;
	spec.expected.sve.p = p;
	spec.expected.sve.ffr = ffr;
	spec.expected.sve.valid = true;
	observation = native_create_and_execute(test, &spec, &regs);
	memset(z, 0xff, sizeof(z));
	memset(p, 0xff, sizeof(p));
	memset(ffr, 0xff, sizeof(ffr));
	observed_sve = spec.expected.sve;
	observed_sve.z = observed_z;
	observed_sve.p = observed_p;
	observed_sve.ffr = observed_ffr;
	KUNIT_ASSERT_EQ(test, 0,
			orlix_tcti_native_observation_add_sve(
				observation, &observed_sve));
	memset(observed_z, 0xee, sizeof(observed_z));
	memset(observed_p, 0xee, sizeof(observed_p));
	memset(observed_ffr, 0xee, sizeof(observed_ffr));
	KUNIT_EXPECT_EQ(test, 0,
			orlix_tcti_native_observation_compare(observation));
	orlix_tcti_native_observation_destroy(observation);

	memset(z, 0, sizeof(z));
	memset(p, 0, sizeof(p));
	memset(ffr, 0, sizeof(ffr));
	z[sizeof(z) - 1] = 1;
	p[sizeof(p) - 1] = 2;
	ffr[sizeof(ffr) - 1] = 3;
	native_seed_execution(&spec, &regs, ORLIX_TCTI_NATIVE_OBLIGATION_SVE,
			      mapped);
	spec.expected.sve.vl_bytes = ORLIX_TCTI_SVE_MIN_VL_BYTES;
	spec.expected.sve.z = z;
	spec.expected.sve.p = p;
	spec.expected.sve.ffr = ffr;
	spec.expected.sve.valid = true;
	observation = native_create_and_execute(test, &spec, &regs);
	memcpy(observed_z, z, sizeof(z));
	memcpy(observed_p, p, sizeof(p));
	memcpy(observed_ffr, ffr, sizeof(ffr));
	observed_z[0] = 1;
	observed_sve = spec.expected.sve;
	observed_sve.z = observed_z;
	observed_sve.p = observed_p;
	observed_sve.ffr = observed_ffr;
	KUNIT_ASSERT_EQ(test, 0,
			orlix_tcti_native_observation_add_sve(observation,
							  &observed_sve));
	KUNIT_EXPECT_EQ(test, -EBADE,
			orlix_tcti_native_observation_compare(observation));
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_NATIVE_OBSERVATION_SVE_MISMATCH,
			orlix_tcti_native_observation_state(observation));
	orlix_tcti_native_observation_destroy(observation);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void native_sme_optional_state_is_explicit_and_unavailable(
		struct kunit *test)
{
	u8 za[ORLIX_TCTI_SVE_MIN_VL_BYTES * ORLIX_TCTI_SVE_MIN_VL_BYTES] = {};
	u8 zt0[ORLIX_TCTI_NATIVE_OBSERVATION_ZT0_BYTES] = {};
	u8 observed_za[sizeof(za)];
	u8 observed_zt0[sizeof(zt0)];
	struct orlix_tcti_native_observation_spec spec;
	struct orlix_tcti_native_observation *observation;
	struct orlix_tcti_native_sme_state observed_sme;
	struct pt_regs regs;
	unsigned long mapped = native_map_program(test, NATIVE_NOP);

	native_seed_execution(&spec, &regs, ORLIX_TCTI_NATIVE_OBLIGATION_SME,
			      mapped);
	spec.expected.sme.sm_present = true;
	spec.expected.sme.streaming_mode = true;
	spec.expected.sme.za_control_present = false;
	spec.expected.sme.za_enabled = false;
	spec.expected.sme.vl_present = true;
	spec.expected.sme.vl_bytes = ORLIX_TCTI_SVE_MIN_VL_BYTES;
	spec.expected.sme.svl_present = true;
	spec.expected.sme.svl_bytes = ORLIX_TCTI_SVE_MIN_VL_BYTES;
	spec.expected.sme.valid = true;
	spec.expected.sme.production_available = false;
	observation = native_create_and_execute(test, &spec, &regs);
	KUNIT_ASSERT_EQ(test, 0,
			orlix_tcti_native_observation_add_sme(
				observation, &spec.expected.sme));
	KUNIT_EXPECT_EQ(test, -EOPNOTSUPP,
			orlix_tcti_native_observation_compare(observation));
	orlix_tcti_native_observation_destroy(observation);

	/* ZA-disabled is present as control state without a ZA or ZT0 buffer. */
	native_seed_execution(&spec, &regs, ORLIX_TCTI_NATIVE_OBLIGATION_SME,
			      mapped);
	spec.expected.sme.sm_present = true;
	spec.expected.sme.za_control_present = true;
	spec.expected.sme.za_enabled = false;
	spec.expected.sme.vl_present = true;
	spec.expected.sme.vl_bytes = ORLIX_TCTI_SVE_MIN_VL_BYTES;
	spec.expected.sme.svl_present = true;
	spec.expected.sme.svl_bytes = ORLIX_TCTI_SVE_MIN_VL_BYTES;
	spec.expected.sme.valid = true;
	observation = orlix_tcti_native_observation_create(&spec);
	KUNIT_EXPECT_NOT_NULL(test, observation);
	orlix_tcti_native_observation_destroy(observation);

	/* A production-available complete state owns both scalable copies. */
	za[0] = 1;
	zt0[0] = 2;
	memcpy(observed_za, za, sizeof(za));
	memcpy(observed_zt0, zt0, sizeof(zt0));
	native_seed_execution(&spec, &regs, ORLIX_TCTI_NATIVE_OBLIGATION_SME,
			      mapped);
	spec.expected.sme.sm_present = true;
	spec.expected.sme.streaming_mode = true;
	spec.expected.sme.za_control_present = true;
	spec.expected.sme.za_enabled = true;
	spec.expected.sme.vl_present = true;
	spec.expected.sme.vl_bytes = ORLIX_TCTI_SVE_MIN_VL_BYTES;
	spec.expected.sme.svl_present = true;
	spec.expected.sme.svl_bytes = ORLIX_TCTI_SVE_MIN_VL_BYTES;
	spec.expected.sme.za_applicable = true;
	spec.expected.sme.za_present = true;
	spec.expected.sme.za = za;
	spec.expected.sme.za_size = sizeof(za);
	spec.expected.sme.zt0_applicable = true;
	spec.expected.sme.zt0_present = true;
	spec.expected.sme.zt0 = zt0;
	spec.expected.sme.zt0_size = sizeof(zt0);
	spec.expected.sme.valid = true;
	spec.expected.sme.production_available = true;
	observation = native_create_and_execute(test, &spec, &regs);
	memset(za, 0xff, sizeof(za));
	memset(zt0, 0xff, sizeof(zt0));
	observed_sme = spec.expected.sme;
	observed_sme.za = observed_za;
	observed_sme.zt0 = observed_zt0;
	KUNIT_ASSERT_EQ(test, 0,
			orlix_tcti_native_observation_add_sme(observation,
							  &observed_sme));
	memset(observed_za, 0xee, sizeof(observed_za));
	memset(observed_zt0, 0xee, sizeof(observed_zt0));
	KUNIT_EXPECT_EQ(test, 0,
			orlix_tcti_native_observation_compare(observation));
	orlix_tcti_native_observation_destroy(observation);

	/* Each applicable payload participates independently in comparison. */
	memset(za, 0, sizeof(za));
	memset(zt0, 0, sizeof(zt0));
	za[0] = 1;
	zt0[0] = 2;
	memcpy(observed_za, za, sizeof(za));
	memcpy(observed_zt0, zt0, sizeof(zt0));
	native_seed_execution(&spec, &regs, ORLIX_TCTI_NATIVE_OBLIGATION_SME,
			      mapped);
	spec.expected.sme = (struct orlix_tcti_native_sme_state) {
		.sm_present = true,
		.streaming_mode = true,
		.za_control_present = true,
		.za_enabled = true,
		.vl_present = true,
		.vl_bytes = ORLIX_TCTI_SVE_MIN_VL_BYTES,
		.svl_present = true,
		.svl_bytes = ORLIX_TCTI_SVE_MIN_VL_BYTES,
		.za_applicable = true,
		.za_present = true,
		.za = za,
		.za_size = sizeof(za),
		.zt0_applicable = true,
		.zt0_present = true,
		.zt0 = zt0,
		.zt0_size = sizeof(zt0),
		.valid = true,
		.production_available = true,
	};
	observation = native_create_and_execute(test, &spec, &regs);
	observed_sme = spec.expected.sme;
	observed_za[0] ^= 1;
	observed_sme.za = observed_za;
	observed_sme.zt0 = observed_zt0;
	KUNIT_ASSERT_EQ(test, 0,
			orlix_tcti_native_observation_add_sme(observation,
							  &observed_sme));
	KUNIT_EXPECT_EQ(test, -EBADE,
			orlix_tcti_native_observation_compare(observation));
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_NATIVE_OBSERVATION_SME_MISMATCH,
			orlix_tcti_native_observation_state(observation));
	orlix_tcti_native_observation_destroy(observation);

	memcpy(observed_za, za, sizeof(za));
	memcpy(observed_zt0, zt0, sizeof(zt0));
	native_seed_execution(&spec, &regs, ORLIX_TCTI_NATIVE_OBLIGATION_SME,
			      mapped);
	spec.expected.sme = (struct orlix_tcti_native_sme_state) {
		.sm_present = true, .za_control_present = true, .za_enabled = true,
		.vl_present = true, .vl_bytes = ORLIX_TCTI_SVE_MIN_VL_BYTES,
		.svl_present = true, .svl_bytes = ORLIX_TCTI_SVE_MIN_VL_BYTES,
		.za_applicable = true, .za_present = true, .za = za,
		.za_size = sizeof(za), .zt0_applicable = true, .zt0_present = true,
		.zt0 = zt0, .zt0_size = sizeof(zt0), .valid = true,
		.production_available = true,
	};
	observation = native_create_and_execute(test, &spec, &regs);
	observed_sme = spec.expected.sme;
	observed_zt0[0] ^= 1;
	observed_sme.za = observed_za;
	observed_sme.zt0 = observed_zt0;
	KUNIT_ASSERT_EQ(test, 0,
			orlix_tcti_native_observation_add_sme(observation,
							  &observed_sme));
	KUNIT_EXPECT_EQ(test, -EBADE,
			orlix_tcti_native_observation_compare(observation));
	orlix_tcti_native_observation_destroy(observation);

	/* A malformed SME add poisons the object; a valid retry cannot recover. */
	memcpy(observed_zt0, zt0, sizeof(zt0));
	native_seed_execution(&spec, &regs, ORLIX_TCTI_NATIVE_OBLIGATION_SME,
			      mapped);
	spec.expected.sme = (struct orlix_tcti_native_sme_state) {
		.sm_present = true, .za_control_present = true, .za_enabled = true,
		.vl_present = true, .vl_bytes = ORLIX_TCTI_SVE_MIN_VL_BYTES,
		.svl_present = true, .svl_bytes = ORLIX_TCTI_SVE_MIN_VL_BYTES,
		.za_applicable = true, .za_present = true, .za = za,
		.za_size = sizeof(za), .zt0_applicable = true, .zt0_present = true,
		.zt0 = zt0, .zt0_size = sizeof(zt0), .valid = true,
		.production_available = true,
	};
	observation = native_create_and_execute(test, &spec, &regs);
	observed_sme = spec.expected.sme;
	observed_sme.za_present = false;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			orlix_tcti_native_observation_add_sme(observation,
							  &observed_sme));
	KUNIT_EXPECT_EQ(test, -EPERM,
			orlix_tcti_native_observation_add_sme(
				observation, &spec.expected.sme));
	KUNIT_EXPECT_EQ(test, -EPERM,
			orlix_tcti_native_observation_compare(observation));
	orlix_tcti_native_observation_destroy(observation);

	/* Enabled ZA is malformed unless the applicable ZA payload is present. */
	spec.expected.sme.za_enabled = true;
	spec.expected.sme.za_applicable = true;
	spec.expected.sme.za_present = false;
	KUNIT_EXPECT_PTR_EQ(test, NULL,
			orlix_tcti_native_observation_create(&spec));

	/* Applicable ZT0 likewise requires an explicit 512-bit payload. */
	spec.expected.sme.za_enabled = false;
	spec.expected.sme.za_applicable = false;
	spec.expected.sme.za = NULL;
	spec.expected.sme.za_size = 0;
	spec.expected.sme.zt0_applicable = true;
	spec.expected.sme.zt0_present = false;
	spec.expected.sme.zt0 = NULL;
	spec.expected.sme.zt0_size = 0;
	KUNIT_EXPECT_PTR_EQ(test, NULL,
			orlix_tcti_native_observation_create(&spec));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void native_negative_witnesses_are_typed_mismatches_not_malformed(
		struct kunit *test)
{
	struct orlix_tcti_native_observation_spec spec;
	struct orlix_tcti_native_observation *observation;
	struct orlix_tcti_native_fault_witness fault;
	struct orlix_tcti_native_atomicity_witness atomicity;
	struct orlix_tcti_native_ordering_witness ordering;
	struct pt_regs regs;
	unsigned long mapped = native_map_program(test, NATIVE_NOP);
	unsigned long fault_mapped = native_map_program(test, NATIVE_LDR_X0_X1);

	native_seed_execution(&spec, &regs, ORLIX_TCTI_NATIVE_OBLIGATION_FAULT,
			      fault_mapped);
	regs.regs[1] = 0;
	spec.source_ordinal = NATIVE_LDR_64_ORDINAL;
	spec.result.reason = ORLIX_TCTI_EXIT_USER_FAULT;
	spec.result.status = -EFAULT;
	spec.result.fault_address = 0;
	spec.result.fault_access = ORLIX_TCTI_ACCESS_READ;
	spec.result.pc = fault_mapped;
	spec.result.instruction = NATIVE_LDR_X0_X1;
	orlix_tcti_native_gpr_capture(&spec.gpr, &regs);
	spec.expected.fault.address = 0;
	spec.expected.fault.access = ORLIX_TCTI_ACCESS_READ;
	spec.expected.fault.valid = true;
	spec.expected.fault.occurred = true;
	spec.expected.fault.precise = true;
	observation = native_create_and_execute(test, &spec, &regs);
	fault = spec.expected.fault;
	fault.side_effects_committed = true;
	KUNIT_ASSERT_EQ(test, 0,
			orlix_tcti_native_observation_add_fault(observation, &fault));
	KUNIT_EXPECT_EQ(test, -EBADE,
			orlix_tcti_native_observation_compare(observation));
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_NATIVE_OBSERVATION_FAULT_MISMATCH,
			orlix_tcti_native_observation_state(observation));
	orlix_tcti_native_observation_destroy(observation);

	native_seed_execution(&spec, &regs, ORLIX_TCTI_NATIVE_OBLIGATION_ATOMICITY,
			      mapped);
	spec.expected.atomicity.address = 0;
	spec.expected.atomicity.width = 8;
	spec.expected.atomicity.valid = true;
	spec.expected.atomicity.completed = true;
	spec.expected.atomicity.linearization_count = 1;
	observation = native_create_and_execute(test, &spec, &regs);
	atomicity = spec.expected.atomicity;
	atomicity.torn = true;
	KUNIT_ASSERT_EQ(test, 0,
			orlix_tcti_native_observation_add_atomicity(observation,
								&atomicity));
	KUNIT_EXPECT_EQ(test, -EBADE,
			orlix_tcti_native_observation_compare(observation));
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_NATIVE_OBSERVATION_ATOMICITY_MISMATCH,
			orlix_tcti_native_observation_state(observation));
	orlix_tcti_native_observation_destroy(observation);

	native_seed_execution(&spec, &regs, ORLIX_TCTI_NATIVE_OBLIGATION_ORDERING,
			      mapped);
	spec.expected.ordering.address = 0;
	spec.expected.ordering.order = ORLIX_TCTI_ATOMIC_MEMORY_ACQUIRE;
	spec.expected.ordering.predecessor_epoch = 0;
	spec.expected.ordering.successor_epoch = 1;
	spec.expected.ordering.observed_value = 0;
	spec.expected.ordering.valid = true;
	spec.expected.ordering.edge_observed = true;
	observation = native_create_and_execute(test, &spec, &regs);
	ordering = spec.expected.ordering;
	ordering.forbidden_outcome = true;
	KUNIT_ASSERT_EQ(test, 0,
			orlix_tcti_native_observation_add_ordering(observation,
							       &ordering));
	KUNIT_EXPECT_EQ(test, -EBADE,
			orlix_tcti_native_observation_compare(observation));
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_NATIVE_OBSERVATION_ORDERING_MISMATCH,
			orlix_tcti_native_observation_state(observation));
	orlix_tcti_native_observation_destroy(observation);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(fault_mapped, PAGE_SIZE));
}

static void native_export_is_opaque_single_use_and_production_only(
		struct kunit *test)
{
	struct orlix_tcti_target_native_result_record *record = NULL;
	struct orlix_tcti_target_native_result_record *replay = NULL;
	struct orlix_tcti_native_observation_spec spec;
	struct orlix_tcti_native_observation *observation;
	struct pt_regs regs;
	unsigned long mapped = native_map_program(test, NATIVE_NOP);

	native_seed_execution(&spec, &regs, ORLIX_TCTI_NATIVE_OBLIGATION_GPR,
			      mapped);
	observation = native_create_and_execute(test, &spec, &regs);
	KUNIT_ASSERT_EQ(test, 0,
			orlix_tcti_native_observation_compare(observation));
	KUNIT_ASSERT_EQ(test, 0,
			orlix_tcti_native_observation_export(observation, &record));
	KUNIT_ASSERT_NOT_NULL(test, record);
	KUNIT_EXPECT_EQ(test, -EALREADY,
			orlix_tcti_native_observation_export(observation, &replay));
	KUNIT_EXPECT_PTR_EQ(test, NULL, replay);
	orlix_tcti_target_native_result_record_destroy(record);
	orlix_tcti_native_observation_destroy(observation);

	native_seed_execution(&spec, &regs, ORLIX_TCTI_NATIVE_OBLIGATION_MEMORY,
			      mapped);
	spec.expected.memory.address = mapped;
	spec.expected.memory.size = 1;
	spec.expected.memory.bytes = (const u8[]){ NATIVE_NOP & 0xffU };
	observation = native_create_and_execute(test, &spec, &regs);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_native_observation_add_memory(
		observation, &spec.expected.memory));
	KUNIT_ASSERT_EQ(test, 0,
			orlix_tcti_native_observation_compare(observation));
	KUNIT_EXPECT_EQ(test, 0,
			orlix_tcti_native_observation_export(observation, &record));
	KUNIT_EXPECT_NOT_NULL(test, record);
	orlix_tcti_target_native_result_record_destroy(record);
	orlix_tcti_native_observation_destroy(observation);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static struct kunit_case orlix_tcti_native_observation_test_cases[] = {
	KUNIT_CASE(native_no_synthetic_or_recoverable_match),
	KUNIT_CASE(native_result_and_gpr_are_captured_not_injected),
	KUNIT_CASE(native_memory_and_fp_simd_compare_after_production),
	KUNIT_CASE(native_sve_uses_heap_owned_complete_state),
	KUNIT_CASE(native_sme_optional_state_is_explicit_and_unavailable),
	KUNIT_CASE(native_negative_witnesses_are_typed_mismatches_not_malformed),
	KUNIT_CASE(native_export_is_opaque_single_use_and_production_only),
	{}
};

static struct kunit_suite orlix_tcti_native_observation_test_suite = {
	.name = "orlix-tcti-native-observation",
	.init = native_observation_test_init,
	.exit = native_observation_test_exit,
	.test_cases = orlix_tcti_native_observation_test_cases,
};

kunit_test_suite(orlix_tcti_native_observation_test_suite);

MODULE_LICENSE("GPL");
